# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the sync stages."""

from __future__ import print_function

import collections
import ConfigParser
import contextlib
import datetime
import itertools
import os
import re
import sys
import time
from xml.etree import ElementTree
from xml.dom import minidom

from chromite.cbuildbot import chroot_lib
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import lkgm_manager
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import repository
from chromite.cbuildbot import tree_status
from chromite.cbuildbot import triage_lib
from chromite.cbuildbot import trybot_patch_pool
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import build_stages
from chromite.lib import clactions
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import graphite
from chromite.lib import osutils
from chromite.lib import patch as cros_patch
from chromite.lib import timeout_util
from chromite.scripts import cros_mark_chrome_as_stable


PRE_CQ = validation_pool.PRE_CQ

PRECQ_LAUNCH_TIMEOUT_MSG = (
    'We were not able to launch a %s trybot for your change within '
    '%s minutes.\n\n'
    'This problem can happen if the trybot waterfall is very '
    'busy, or if there is an infrastructure issue. Please '
    'notify the sheriff and mark your change as ready again. If '
    'this problem occurs multiple times in a row, please file a '
    'bug.')
PRECQ_INFLIGHT_TIMEOUT_MSG = (
    'The %s trybot for your change timed out after %s minutes.'
    '\n\n'
    'This problem can happen if your change causes the builder '
    'to hang, or if there is some infrastructure issue. If your '
    'change is not at fault you may mark your change as ready '
    'again. If this problem occurs multiple times please notify '
    'the sheriff and file a bug.')
PRECQ_EXPIRY_MSG = (
    'The pre-cq verification for this change expired after %s minutes. No '
    'action is required on your part.'
    '\n\n'
    'In order to protect the CQ from picking up stale changes, the pre-cq '
    'status for changes are cleared after a generous timeout. This change '
    'will be re-tested by the pre-cq before the CQ picks it up.')

class PatchChangesStage(generic_stages.BuilderStage):
  """Stage that patches a set of Gerrit changes to the buildroot source tree."""

  def __init__(self, builder_run, patch_pool, **kwargs):
    """Construct a PatchChangesStage.

    Args:
      builder_run: BuilderRun object.
      patch_pool: A TrybotPatchPool object containing the different types of
                  patches to apply.
    """
    super(PatchChangesStage, self).__init__(builder_run, **kwargs)
    self.patch_pool = patch_pool

  @staticmethod
  def _CheckForDuplicatePatches(_series, changes):
    conflicts = {}
    duplicates = []
    for change in changes:
      if change.id is None:
        logging.warning(
            "Change %s lacks a usable ChangeId; duplicate checking cannot "
            "be done for this change.  If cherry-picking fails, this is a "
            "potential cause.", change)
        continue
      conflicts.setdefault(change.id, []).append(change)

    duplicates = [x for x in conflicts.itervalues() if len(x) > 1]
    if not duplicates:
      return changes

    for conflict in duplicates:
      logging.error(
          "Changes %s conflict with each other- they have same id %s., "
          .join(map(str, conflict)), conflict[0].id)

    cros_build_lib.Die("Duplicate patches were encountered: %s", duplicates)

  def _PatchSeriesFilter(self, series, changes):
    return self._CheckForDuplicatePatches(series, changes)

  def _ApplyPatchSeries(self, series, patch_pool, **kwargs):
    """Applies a patch pool using a patch series."""
    kwargs.setdefault('frozen', False)
    # Honor the given ordering, so that if a gerrit/remote patch
    # conflicts w/ a local patch, the gerrit/remote patch are
    # blamed rather than local (patch ordering is typically
    # local, gerrit, then remote).
    kwargs.setdefault('honor_ordering', True)
    kwargs['changes_filter'] = self._PatchSeriesFilter

    _applied, failed_tot, failed_inflight = series.Apply(
        list(patch_pool), **kwargs)

    failures = failed_tot + failed_inflight
    if failures:
      self.HandleApplyFailures(failures)

  def HandleApplyFailures(self, failures):
    cros_build_lib.Die("Failed applying patches: %s",
                       "\n".join(map(str, failures)))

  def PerformStage(self):
    class NoisyPatchSeries(validation_pool.PatchSeries):
      """Custom PatchSeries that adds links to buildbot logs for remote trys."""

      def ApplyChange(self, change):
        if isinstance(change, cros_patch.GerritPatch):
          cros_build_lib.PrintBuildbotLink(str(change), change.url)
        elif isinstance(change, cros_patch.UploadedLocalPatch):
          cros_build_lib.PrintBuildbotStepText(str(change))

        return validation_pool.PatchSeries.ApplyChange(self, change)

    # If we're an external builder, ignore internal patches.
    helper_pool = validation_pool.HelperPool.SimpleCreate(
        cros_internal=self._run.config.internal, cros=True)

    # Limit our resolution to non-manifest patches.
    patch_series = NoisyPatchSeries(
        self._build_root,
        helper_pool=helper_pool,
        deps_filter_fn=lambda p: not trybot_patch_pool.ManifestFilter(p))

    self._ApplyPatchSeries(patch_series, self.patch_pool)


class BootstrapStage(PatchChangesStage):
  """Stage that patches a chromite repo and re-executes inside it.

  Attributes:
    returncode - the returncode of the cbuildbot re-execution.  Valid after
                 calling stage.Run().
  """
  option_name = 'bootstrap'

  def __init__(self, builder_run, chromite_patch_pool,
               manifest_patch_pool=None, **kwargs):
    super(BootstrapStage, self).__init__(
        builder_run, trybot_patch_pool.TrybotPatchPool(), **kwargs)
    self.chromite_patch_pool = chromite_patch_pool
    self.manifest_patch_pool = manifest_patch_pool
    self.returncode = None
    self.tempdir = None

  def _ApplyManifestPatches(self, patch_pool):
    """Apply a pool of manifest patches to a temp manifest checkout.

    Args:
      patch_pool: The pool to apply.

    Returns:
      The path to the patched manifest checkout.

    Raises:
      Exception, if the new patched manifest cannot be parsed.
    """
    checkout_dir = os.path.join(self.tempdir, 'manfest-checkout')
    repository.CloneGitRepo(checkout_dir,
                            self._run.config.manifest_repo_url)

    patch_series = validation_pool.PatchSeries.WorkOnSingleRepo(
        checkout_dir, tracking_branch=self._run.manifest_branch)

    self._ApplyPatchSeries(patch_series, patch_pool)
    # Create the branch that 'repo init -b <target_branch> -u <patched_repo>'
    # will look for.
    cmd = ['branch', '-f', self._run.manifest_branch,
           constants.PATCH_BRANCH]
    git.RunGit(checkout_dir, cmd)

    # Verify that the patched manifest loads properly. Propagate any errors as
    # exceptions.
    manifest = os.path.join(checkout_dir, self._run.config.manifest)
    git.Manifest.Cached(manifest, manifest_include_dir=checkout_dir)
    return checkout_dir

  @staticmethod
  def _FilterArgsForApi(parsed_args, api_minor):
    """Remove arguments that are introduced after an api version."""
    def filter_fn(passed_arg):
      return passed_arg.opt_inst.api_version <= api_minor

    accepted, removed = commandline.FilteringParser.FilterArgs(
        parsed_args, filter_fn)

    if removed:
      logging.warning("The following arguments were removed due to api: '%s'"
                      % ' '.join(removed))
    return accepted

  @classmethod
  def FilterArgsForTargetCbuildbot(cls, buildroot, cbuildbot_path, options):
    _, minor = cros_build_lib.GetTargetChromiteApiVersion(buildroot)
    args = [cbuildbot_path]
    args.extend(options.build_targets)
    args.extend(cls._FilterArgsForApi(options.parsed_args, minor))

    # Only pass down --cache-dir if it was specified. By default, we want
    # the cache dir to live in the root of each checkout, so this means that
    # each instance of cbuildbot needs to calculate the default separately.
    if minor >= 2 and options.cache_dir_specified:
      args += ['--cache-dir', options.cache_dir]

    return args

  def HandleApplyFailures(self, failures):
    """Handle the case where patches fail to apply."""
    if self._run.options.pre_cq or self._run.config.pre_cq:
      # Let the PreCQSync stage handle this failure. The PreCQSync stage will
      # comment on CLs with the appropriate message when they fail to apply.
      #
      # WARNING: For manifest patches, the Pre-CQ attempts to apply external
      # patches to the internal manifest, and this means we may flag a conflict
      # here even if the patch applies cleanly. TODO(davidjames): Fix this.
      cros_build_lib.PrintBuildbotStepWarnings()
      logging.error('Failed applying patches: %s\n'.join(map(str, failures)))
    else:
      PatchChangesStage.HandleApplyFailures(self, failures)

  def _PerformStageInTempDir(self):
    # The plan for the builders is to use master branch to bootstrap other
    # branches. Now, if we wanted to test patches for both the bootstrap code
    # (on master) and the branched chromite (say, R20), we need to filter the
    # patches by branch.
    filter_branch = self._run.manifest_branch
    if self._run.options.test_bootstrap:
      filter_branch = 'master'

    chromite_dir = os.path.join(self.tempdir, 'chromite')
    reference_repo = os.path.join(constants.SOURCE_ROOT, 'chromite', '.git')
    repository.CloneGitRepo(chromite_dir, constants.CHROMITE_URL,
                            reference=reference_repo)
    git.RunGit(chromite_dir, ['checkout', filter_branch])

    def BranchAndChromiteFilter(patch):
      return (trybot_patch_pool.BranchFilter(filter_branch, patch) and
              trybot_patch_pool.ChromiteFilter(patch))

    patch_series = validation_pool.PatchSeries.WorkOnSingleRepo(
        chromite_dir, filter_branch,
        deps_filter_fn=BranchAndChromiteFilter)

    filtered_pool = self.chromite_patch_pool.FilterBranch(filter_branch)
    if filtered_pool:
      self._ApplyPatchSeries(patch_series, filtered_pool)

    cbuildbot_path = constants.PATH_TO_CBUILDBOT
    if not os.path.exists(os.path.join(self.tempdir, cbuildbot_path)):
      cbuildbot_path = 'chromite/cbuildbot/cbuildbot'
    cmd = self.FilterArgsForTargetCbuildbot(self.tempdir, cbuildbot_path,
                                            self._run.options)

    extra_params = ['--sourceroot=%s' % self._run.options.sourceroot]
    extra_params.extend(self._run.options.bootstrap_args)
    if self._run.options.test_bootstrap:
      # We don't want re-executed instance to see this.
      cmd = [a for a in cmd if a != '--test-bootstrap']
    else:
      # If we've already done the desired number of bootstraps, disable
      # bootstrapping for the next execution.  Also pass in the patched manifest
      # repository.
      extra_params.append('--nobootstrap')
      if self.manifest_patch_pool:
        manifest_dir = self._ApplyManifestPatches(self.manifest_patch_pool)
        extra_params.extend(['--manifest-repo-url', manifest_dir])

    cmd += extra_params
    result_obj = cros_build_lib.RunCommand(
        cmd, cwd=self.tempdir, kill_timeout=30, error_code_ok=True)
    self.returncode = result_obj.returncode

  def PerformStage(self):
    with osutils.TempDir(base_dir=self._run.options.bootstrap_dir) as tempdir:
      self.tempdir = tempdir
      self._PerformStageInTempDir()
    self.tempdir = None


class SyncStage(generic_stages.BuilderStage):
  """Stage that performs syncing for the builder."""

  option_name = 'sync'
  output_manifest_sha1 = True

  def __init__(self, builder_run, **kwargs):
    super(SyncStage, self).__init__(builder_run, **kwargs)
    self.repo = None
    self.skip_sync = False

    # TODO(mtennant): Why keep a duplicate copy of this config value
    # at self.internal when it can always be retrieved from config?
    self.internal = self._run.config.internal

  def _GetManifestVersionsRepoUrl(self, internal=None, test=False):
    if internal is None:
      internal = self._run.config.internal

    if internal:
      if test:
        return constants.MANIFEST_VERSIONS_INT_GOB_URL_TEST
      else:
        return constants.MANIFEST_VERSIONS_INT_GOB_URL
    else:
      if test:
        return constants.MANIFEST_VERSIONS_GOB_URL_TEST
      else:
        return constants.MANIFEST_VERSIONS_GOB_URL

  def Initialize(self):
    self._InitializeRepo()

  def _InitializeRepo(self):
    """Set up the RepoRepository object."""
    self.repo = self.GetRepoRepository()

  def GetNextManifest(self):
    """Returns the manifest to use."""
    return self._run.config.manifest

  def ManifestCheckout(self, next_manifest):
    """Checks out the repository to the given manifest."""
    self._Print('\n'.join(['BUILDROOT: %s' % self.repo.directory,
                           'TRACKING BRANCH: %s' % self.repo.branch,
                           'NEXT MANIFEST: %s' % next_manifest]))

    if not self.skip_sync:
      self.repo.Sync(next_manifest)

    print(self.repo.ExportManifest(mark_revision=self.output_manifest_sha1),
          file=sys.stderr)

  def RunPrePatchBuild(self):
    """Run through a pre-patch build to prepare for incremental build.

    This function runs though the InitSDKStage, SetupBoardStage, and
    BuildPackagesStage. It is intended to be called before applying
    any patches under test, to prepare the chroot and sysroot in a state
    corresponding to ToT prior to an incremental build.

    Returns:
      True if all stages were successful, False if any of them failed.
    """
    suffix = ' (pre-Patch)'
    try:
      build_stages.InitSDKStage(
          self._run, chroot_replace=True, suffix=suffix).Run()
      for builder_run in self._run.GetUngroupedBuilderRuns():
        for board in builder_run.config.boards:
          build_stages.SetupBoardStage(
              builder_run, board=board, suffix=suffix).Run()
          build_stages.BuildPackagesStage(
              builder_run, board=board, suffix=suffix).Run()
    except failures_lib.StepFailure:
      return False

    return True

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    self.Initialize()
    with osutils.TempDir() as tempdir:
      # Save off the last manifest.
      fresh_sync = True
      if os.path.exists(self.repo.directory) and not self._run.options.clobber:
        old_filename = os.path.join(tempdir, 'old.xml')
        try:
          old_contents = self.repo.ExportManifest()
        except cros_build_lib.RunCommandError as e:
          logging.warning(str(e))
        else:
          osutils.WriteFile(old_filename, old_contents)
          fresh_sync = False

      # Sync.
      self.ManifestCheckout(self.GetNextManifest())

      # Print the blamelist.
      if fresh_sync:
        cros_build_lib.PrintBuildbotStepText('(From scratch)')
      elif self._run.options.buildbot:
        lkgm_manager.GenerateBlameList(self.repo, old_filename)

      # Incremental builds request an additional build before patching changes.
      if self._run.config.build_before_patching:
        pre_build_passed = self.RunPrePatchBuild()
        if not pre_build_passed:
          cros_build_lib.PrintBuildbotStepText('Pre-patch build failed.')


class LKGMSyncStage(SyncStage):
  """Stage that syncs to the last known good manifest blessed by builders."""

  output_manifest_sha1 = False

  def GetNextManifest(self):
    """Override: Gets the LKGM."""
    # TODO(sosa):  Should really use an initialized manager here.
    if self.internal:
      mv_dir = constants.INTERNAL_MANIFEST_VERSIONS_PATH
    else:
      mv_dir = constants.EXTERNAL_MANIFEST_VERSIONS_PATH

    manifest_path = os.path.join(self._build_root, mv_dir)
    manifest_repo = self._GetManifestVersionsRepoUrl()
    manifest_version.RefreshManifestCheckout(manifest_path, manifest_repo)
    return os.path.join(manifest_path, self._run.config.lkgm_manifest)


class ManifestVersionedSyncStage(SyncStage):
  """Stage that generates a unique manifest file, and sync's to it."""

  # TODO(mtennant): Make this into a builder run value.
  output_manifest_sha1 = False

  def __init__(self, builder_run, **kwargs):
    # Perform the sync at the end of the stage to the given manifest.
    super(ManifestVersionedSyncStage, self).__init__(builder_run, **kwargs)
    self.repo = None
    self.manifest_manager = None

    # If a builder pushes changes (even with dryrun mode), we need a writable
    # repository. Otherwise, the push will be rejected by the server.
    self.manifest_repo = self._GetManifestVersionsRepoUrl()

    # 1. If we're uprevving Chrome, Chrome might have changed even if the
    #    manifest has not, so we should force a build to double check. This
    #    means that we'll create a new manifest, even if there are no changes.
    # 2. If we're running with --debug, we should always run through to
    #    completion, so as to ensure a complete test.
    self._force = self._chrome_rev or self._run.options.debug

  def HandleSkip(self):
    """Initializes a manifest manager to the specified version if skipped."""
    super(ManifestVersionedSyncStage, self).HandleSkip()
    if self._run.options.force_version:
      self.Initialize()
      self.ForceVersion(self._run.options.force_version)

  def ForceVersion(self, version):
    """Creates a manifest manager from given version and returns manifest."""
    cros_build_lib.PrintBuildbotStepText(version)
    return self.manifest_manager.BootstrapFromVersion(version)

  def VersionIncrementType(self):
    """Return which part of the version number should be incremented."""
    if self._run.manifest_branch == 'master':
      return 'build'

    return 'branch'

  def RegisterManifestManager(self, manifest_manager):
    """Save the given manifest manager for later use in this run.

    Args:
      manifest_manager: Expected to be a BuildSpecsManager.
    """
    self._run.attrs.manifest_manager = self.manifest_manager = manifest_manager

  def Initialize(self):
    """Initializes a manager that manages manifests for associated stages."""

    dry_run = self._run.options.debug

    self._InitializeRepo()

    # If chrome_rev is somehow set, fail.
    assert not self._chrome_rev, \
        'chrome_rev is unsupported on release builders.'

    self.RegisterManifestManager(manifest_version.BuildSpecsManager(
        source_repo=self.repo,
        manifest_repo=self.manifest_repo,
        manifest=self._run.config.manifest,
        build_names=self._run.GetBuilderIds(),
        incr_type=self.VersionIncrementType(),
        force=self._force,
        branch=self._run.manifest_branch,
        dry_run=dry_run,
        master=self._run.config.master))

  def _SetChromeVersionIfApplicable(self, manifest):
    """If 'chrome' is in |manifest|, write the version to the BuilderRun object.

    Args:
      manifest: Path to the manifest.
    """
    manifest_dom = minidom.parse(manifest)
    elements = manifest_dom.getElementsByTagName(lkgm_manager.CHROME_ELEMENT)

    if elements:
      chrome_version = elements[0].getAttribute(
          lkgm_manager.CHROME_VERSION_ATTR)
      logging.info(
          'Chrome version was found in the manifest: %s', chrome_version)
      # Update the metadata dictionary. This is necessary because the
      # metadata dictionary is preserved through re-executions, so
      # SyncChromeStage can read the version from the dictionary
      # later. This is easier than parsing the manifest again after
      # the re-execution.
      self._run.attrs.metadata.UpdateKeyDictWithDict(
          'version', {'chrome': chrome_version})

  def GetNextManifest(self):
    """Uses the initialized manifest manager to get the next manifest."""
    assert self.manifest_manager, \
        'Must run GetStageManager before checkout out build.'

    build_id = self._run.attrs.metadata.GetDict().get('build_id')

    to_return = self.manifest_manager.GetNextBuildSpec(build_id=build_id)
    previous_version = self.manifest_manager.GetLatestPassingSpec()
    target_version = self.manifest_manager.current_version

    # Print the Blamelist here.
    url_prefix = 'http://chromeos-images.corp.google.com/diff/report?'
    url = url_prefix + 'from=%s&to=%s' % (previous_version, target_version)
    cros_build_lib.PrintBuildbotLink('Blamelist', url)
    # The testManifestVersionedSyncOnePartBranch interacts badly with this
    # function.  It doesn't fully initialize self.manifest_manager which
    # causes target_version to be None.  Since there isn't a clean fix in
    # either direction, just throw this through str().  In the normal case,
    # it's already a string anyways.
    cros_build_lib.PrintBuildbotStepText(str(target_version))

    return to_return

  @contextlib.contextmanager
  def LocalizeManifest(self, manifest, filter_cros=False):
    """Remove restricted checkouts from the manifest if needed.

    Args:
      manifest: The manifest to localize.
      filter_cros: If set, then only checkouts with a remote of 'cros' or
        'cros-internal' are kept, and the rest are filtered out.
    """
    if filter_cros:
      with osutils.TempDir() as tempdir:
        filtered_manifest = os.path.join(tempdir, 'filtered.xml')
        doc = ElementTree.parse(manifest)
        root = doc.getroot()
        for node in root.findall('project'):
          remote = node.attrib.get('remote')
          if remote and remote not in constants.GIT_REMOTES:
            root.remove(node)
        doc.write(filtered_manifest)
        yield filtered_manifest
    else:
      yield manifest

  def _GetMasterVersion(self, master_id, timeout=5 * 60):
    """Get the platform version associated with the master_build_id.

    Args:
      master_id: Our master build id.
      timeout: How long to wait for the platform version to show up
        in the database. This is needed because the slave builders are
        triggered slightly before the platform version is written. Default
        is 5 minutes.
    """
    # TODO(davidjames): Remove the wait loop here once we've updated slave
    # builders to only get triggered after the platform version is written.
    def _PrintRemainingTime(remaining):
      logging.info('%s until timeout...', remaining)

    def _GetPlatformVersion():
      return db.GetBuildStatus(master_id)['platform_version']

    # Retry until non-None version is returned.
    def _ShouldRetry(x):
      return not x

    _, db = self._run.GetCIDBHandle()
    return timeout_util.WaitForSuccess(_ShouldRetry,
                                       _GetPlatformVersion,
                                       timeout,
                                       period=constants.SLEEP_TIMEOUT,
                                       side_effect_func=_PrintRemainingTime)

  def _VerifyMasterId(self, master_id):
    """Verify that our master id is current and valid.

    Args:
      master_id: Our master build id.
    """
    _, db = self._run.GetCIDBHandle()
    if db and master_id:
      assert not self._run.options.force_version
      master_build_status = db.GetBuildStatus(master_id)
      latest = db.GetBuildHistory(master_build_status['build_config'], 1)
      if latest and latest[0]['id'] != master_id:
        raise failures_lib.MasterSlaveVersionMismatchFailure(
            'This slave\'s master (id=%s) has been supplanted by a newer '
            'master (id=%s). Aborting.' % (master_id, latest[0]['id']))

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    self.Initialize()

    self._VerifyMasterId(self._run.options.master_build_id)
    version = self._run.options.force_version
    if self._run.options.master_build_id:
      version = self._GetMasterVersion(self._run.options.master_build_id)

    next_manifest = None
    if version:
      next_manifest = self.ForceVersion(version)
    else:
      self.skip_sync = True
      try:
        next_manifest = self.GetNextManifest()
      except validation_pool.TreeIsClosedException as e:
        logging.warning(str(e))

    if not next_manifest:
      logging.info('Found no work to do.')
      if self._run.attrs.manifest_manager.DidLastBuildFail():
        raise failures_lib.StepFailure('The previous build failed.')
      else:
        sys.exit(0)

    # Log this early on for the release team to grep out before we finish.
    if self.manifest_manager:
      self._Print('\nRELEASETAG: %s\n' % (
          self.manifest_manager.current_version))

    self._SetChromeVersionIfApplicable(next_manifest)
    # To keep local trybots working, remove restricted checkouts from the
    # official manifest we get from manifest-versions.
    with self.LocalizeManifest(
        next_manifest, filter_cros=self._run.options.local) as new_manifest:
      self.ManifestCheckout(new_manifest)

    # Set the status inflight at the end of the ManifestVersionedSync
    # stage. This guarantees that all syncing has completed.
    if self.manifest_manager:
      self.manifest_manager.SetInFlight(
          self.manifest_manager.current_version,
          dashboard_url=self.ConstructDashboardURL())


class MasterSlaveLKGMSyncStage(ManifestVersionedSyncStage):
  """Stage that generates a unique manifest file candidate, and sync's to it.

  This stage uses an LKGM manifest manager that handles LKGM
  candidates and their states.
  """

  # TODO(mtennant): Turn this into self._run.attrs.sub_manager or similar.
  # An instance of lkgm_manager.LKGMManager for slave builds.
  sub_manager = None
  MAX_BUILD_HISTORY_LENGTH = 10
  MilestoneVersion = collections.namedtuple(
      'MilestoneVersion', ['milestone', 'platform'])

  def __init__(self, builder_run, **kwargs):
    super(MasterSlaveLKGMSyncStage, self).__init__(builder_run, **kwargs)
    # lkgm_manager deals with making sure we're synced to whatever manifest
    # we get back in GetNextManifest so syncing again is redundant.
    self._chrome_version = None

  def _GetInitializedManager(self, internal):
    """Returns an initialized lkgm manager.

    Args:
      internal: Boolean.  True if this is using an internal manifest.

    Returns:
      lkgm_manager.LKGMManager.
    """
    increment = self.VersionIncrementType()
    return lkgm_manager.LKGMManager(
        source_repo=self.repo,
        manifest_repo=self._GetManifestVersionsRepoUrl(internal=internal),
        manifest=self._run.config.manifest,
        build_names=self._run.GetBuilderIds(),
        build_type=self._run.config.build_type,
        incr_type=increment,
        force=self._force,
        branch=self._run.manifest_branch,
        dry_run=self._run.options.debug,
        master=self._run.config.master)

  def Initialize(self):
    """Override: Creates an LKGMManager rather than a ManifestManager."""
    self._InitializeRepo()
    self.RegisterManifestManager(self._GetInitializedManager(self.internal))
    if self._run.config.master and self._GetSlaveConfigs():
      assert self.internal, 'Unified masters must use an internal checkout.'
      MasterSlaveLKGMSyncStage.sub_manager = self._GetInitializedManager(False)

  def ForceVersion(self, version):
    manifest = super(MasterSlaveLKGMSyncStage, self).ForceVersion(version)
    if MasterSlaveLKGMSyncStage.sub_manager:
      MasterSlaveLKGMSyncStage.sub_manager.BootstrapFromVersion(version)

    return manifest

  def _VerifyMasterId(self, master_id):
    """Verify that our master id is current and valid."""
    super(MasterSlaveLKGMSyncStage, self)._VerifyMasterId(master_id)
    if not self._run.config.master and not master_id:
      raise failures_lib.StepFailure(
          'Cannot start build without a master_build_id. Did you hit force '
          'build on a slave? Please hit force build on the master instead.')

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'
    assert self._run.config.master

    build_id = self._run.attrs.metadata.GetDict().get('build_id')
    manifest = self.manifest_manager.CreateNewCandidate(
        chrome_version=self._chrome_version,
        build_id=build_id)
    if MasterSlaveLKGMSyncStage.sub_manager:
      MasterSlaveLKGMSyncStage.sub_manager.CreateFromManifest(
          manifest, build_id=build_id)

    return manifest

  def GetLatestChromeVersion(self):
    """Returns the version of Chrome to uprev."""
    return cros_mark_chrome_as_stable.GetLatestRelease(
        constants.CHROMIUM_GOB_URL)

  def GetLastChromeOSVersion(self):
    """Fetching ChromeOS version from the last run.

    Fetching the chromeos version from the last run that published a manifest
    by querying CIDB. Master builds that failed before publishing a manifest
    will be ignored.

    Returns:
      A namedtuple MilestoneVersion,
      e.g. MilestoneVersion(milestone='44', platform='7072.0.0-rc4')
      or None if failed to retrieve milestone and platform versions.
    """
    build_id, db = self._run.GetCIDBHandle()

    if db is None:
      return None

    builds = db.GetBuildHistory(
        build_config=self._run.config.name,
        num_results=self.MAX_BUILD_HISTORY_LENGTH,
        ignore_build_id=build_id)
    full_versions = [b.get('full_version') for b in builds]
    old_version = next(itertools.ifilter(bool, full_versions), None)
    if old_version:
      pattern = r'^R(\d+)-(\d+.\d+.\d+(-rc\d+)*)'
      m = re.match(pattern, old_version)
      if m:
        milestone = m.group(1)
        platform = m.group(2)
      return self.MilestoneVersion(
          milestone=milestone, platform=platform)
    return None

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    """Performs the stage."""
    if (self._chrome_rev == constants.CHROME_REV_LATEST and
        self._run.config.master):
      # PFQ master needs to determine what version of Chrome to build
      # for all slaves.
      self._chrome_version = self.GetLatestChromeVersion()

    ManifestVersionedSyncStage.PerformStage(self)

    # Generate blamelist
    cros_version = self.GetLastChromeOSVersion()
    if cros_version:
      old_filename = self.manifest_manager.GetBuildSpecFilePath(
          cros_version.milestone, cros_version.platform)
      if not os.path.exists(old_filename):
        logging.error('Could not generate blamelist, '
                      'manifest file does not exist: %s', old_filename)
      else:
        logging.debug('Generate blamelist against: %s', old_filename)
        lkgm_manager.GenerateBlameList(self.repo, old_filename)

class CommitQueueSyncStage(MasterSlaveLKGMSyncStage):
  """Commit Queue Sync stage that handles syncing and applying patches.

  Similar to the MasterSlaveLKGMsync Stage, this stage handles syncing
  to a manifest, passing around that manifest to other builders.

  What makes this stage different is that the CQ master finds the
  patches on Gerrit which are ready to be committed, apply them, and
  includes the pathces in the new manifest. The slaves sync to the
  manifest, and apply the paches written in the manifest.
  """

  # The amount of time we wait before assuming that the Pre-CQ is down and
  # that we should start testing changes that haven't been tested by the Pre-CQ.
  PRE_CQ_TIMEOUT = 2 * 60 * 60

  def __init__(self, builder_run, **kwargs):
    super(CommitQueueSyncStage, self).__init__(builder_run, **kwargs)

    # The pool of patches to be picked up by the commit queue.
    # - For the master commit queue, it's initialized in GetNextManifest.
    # - For slave commit queues, it's initialized in _SetPoolFromManifest.
    #
    # In all cases, the pool is saved to disk.
    self.pool = None

  def HandleSkip(self):
    """Handles skip and initializes validation pool from manifest."""
    super(CommitQueueSyncStage, self).HandleSkip()
    filename = self._run.options.validation_pool
    if filename:
      self.pool = validation_pool.ValidationPool.Load(
          filename, builder_run=self._run)
    else:
      self._SetPoolFromManifest(self.manifest_manager.GetLocalManifest())

  def _ChangeFilter(self, _pool, changes, non_manifest_changes):
    # First, look for changes that were tested by the Pre-CQ.
    changes_to_test = []

    _, db = self._run.GetCIDBHandle()
    actions_for_changes = db.GetActionsForChanges(changes)
    for change in changes:
      status = clactions.GetCLPreCQStatus(change, actions_for_changes)
      if status == constants.CL_STATUS_PASSED:
        changes_to_test.append(change)

    # Allow Commit-Ready=+2 changes to bypass the Pre-CQ, if there are no other
    # changes.
    if not changes_to_test:
      changes_to_test = [x for x in changes if x.HasApproval('COMR', '2')]

    # If we only see changes that weren't verified by Pre-CQ, and some of them
    # are really old changes, try all of the changes. This ensures that the CQ
    # continues to work (albeit slowly) even if the Pre-CQ is down.
    if changes and not changes_to_test:
      oldest = min(x.approval_timestamp for x in changes)
      if time.time() > oldest + self.PRE_CQ_TIMEOUT:
        # It's safest to try all changes here because some of the old changes
        # might depend on newer changes (e.g. via CQ-DEPEND).
        changes_to_test = changes

    return changes_to_test, non_manifest_changes

  def _SetPoolFromManifest(self, manifest):
    """Sets validation pool based on manifest path passed in."""
    # Note that this function is only called after the repo is already
    # sync'd, so AcquirePoolFromManifest does not need to sync.
    self.pool = validation_pool.ValidationPool.AcquirePoolFromManifest(
        manifest, self._run.config.overlays, self.repo,
        self._run.buildnumber, self._run.GetBuilderName(),
        self._run.config.master, self._run.options.debug,
        builder_run=self._run)

  def _GetLGKMVersionFromManifest(self, manifest):
    manifest_dom = minidom.parse(manifest)
    elements = manifest_dom.getElementsByTagName(lkgm_manager.LKGM_ELEMENT)
    if elements:
      lkgm_version = elements[0].getAttribute(lkgm_manager.LKGM_VERSION_ATTR)
      logging.info(
          'LKGM version was found in the manifest: %s', lkgm_version)
      return lkgm_version

  def GetNextManifest(self):
    """Gets the next manifest using LKGM logic."""
    assert self.manifest_manager, \
        'Must run Initialize before we can get a manifest.'
    assert isinstance(self.manifest_manager, lkgm_manager.LKGMManager), \
        'Manifest manager instantiated with wrong class.'
    assert self._run.config.master

    build_id = self._run.attrs.metadata.GetDict().get('build_id')

    try:
      # In order to acquire a pool, we need an initialized buildroot.
      if not git.FindRepoDir(self.repo.directory):
        self.repo.Initialize()

      query = constants.CQ_READY_QUERY
      if self._run.options.cq_gerrit_override:
        query = (self._run.options.cq_gerrit_override, None)

      self.pool = pool = validation_pool.ValidationPool.AcquirePool(
          self._run.config.overlays, self.repo,
          self._run.buildnumber, self._run.GetBuilderName(),
          query,
          dryrun=self._run.options.debug,
          check_tree_open=(not self._run.options.debug or
                           self._run.options.mock_tree_status),
          change_filter=self._ChangeFilter, builder_run=self._run)
    except validation_pool.TreeIsClosedException as e:
      logging.warning(str(e))
      return None

    # We must extend the builder deadline before publishing a new manifest to
    # ensure that slaves have enough time to complete the builds about to
    # start.
    build_id, db = self._run.GetCIDBHandle()
    if db:
      timeout = constants.MASTER_BUILD_TIMEOUT_SECONDS.get(
          self._run.config.build_type,
          constants.MASTER_BUILD_TIMEOUT_DEFAULT_SECONDS)
      db.ExtendDeadline(build_id, timeout)

    manifest = self.manifest_manager.CreateNewCandidate(validation_pool=pool,
                                                        build_id=build_id)
    if MasterSlaveLKGMSyncStage.sub_manager:
      MasterSlaveLKGMSyncStage.sub_manager.CreateFromManifest(
          manifest, build_id=build_id)

    return manifest

  def ManifestCheckout(self, next_manifest):
    """Checks out the repository to the given manifest."""
    if self._run.config.build_before_patching:
      assert not self._run.config.master
      pre_build_passed = self.RunPrePatchBuild()
      cros_build_lib.PrintBuildbotStepName(
          'CommitQueueSync : Apply Patches')
      if not pre_build_passed:
        cros_build_lib.PrintBuildbotStepText('Pre-patch build failed.')

    # Make sure the chroot version is valid.
    lkgm_version = self._GetLGKMVersionFromManifest(next_manifest)
    chroot_manager = chroot_lib.ChrootManager(self._build_root)
    chroot_manager.EnsureChrootAtVersion(lkgm_version)

    # Clear the chroot version as we are in the middle of building it.
    chroot_manager.ClearChrootVersion()

    # Syncing to a pinned manifest ensures that we have the specified
    # revisions, but, unfortunately, repo won't bother to update branches.
    # Sync with an unpinned manifest first to ensure that branches are updated
    # (e.g. in case somebody adds a new branch to a repo.) See crbug.com/482077
    if not self.skip_sync:
      self.repo.Sync(self._run.config.manifest, network_only=True)

    # Sync to the provided manifest on slaves. On the master, we're
    # already synced to this manifest, so self.skip_sync is set and
    # this is a no-op.
    super(CommitQueueSyncStage, self).ManifestCheckout(next_manifest)

    # On slaves, initialize our pool and apply patches. On the master,
    # we've already done that in GetNextManifest, so this is a no-op.
    if not self._run.config.master:
      # Print the list of CHUMP changes since the LKGM, then apply changes and
      # print the list of applied changes.
      self.manifest_manager.GenerateBlameListSinceLKGM()
      self._SetPoolFromManifest(next_manifest)
      self.pool.ApplyPoolIntoRepo()

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    """Performs normal stage and prints blamelist at end."""
    if self._run.options.force_version:
      self.HandleSkip()
    else:
      ManifestVersionedSyncStage.PerformStage(self)


class PreCQSyncStage(SyncStage):
  """Sync and apply patches to test if they compile."""

  def __init__(self, builder_run, patches, **kwargs):
    super(PreCQSyncStage, self).__init__(builder_run, **kwargs)

    # As a workaround for crbug.com/432706, we scan patches to see if they
    # are already being merged. If they are, we don't test them in the PreCQ.
    self.patches = [p for p in patches if not p.IsBeingMerged()]

    if patches and not self.patches:
      cros_build_lib.Die('No patches that still need testing.')

    # The ValidationPool of patches to test. Initialized in PerformStage, and
    # refreshed after bootstrapping by HandleSkip.
    self.pool = None

  def HandleSkip(self):
    """Handles skip and loads validation pool from disk."""
    super(PreCQSyncStage, self).HandleSkip()
    filename = self._run.options.validation_pool
    if filename:
      self.pool = validation_pool.ValidationPool.Load(
          filename, builder_run=self._run)

  def PerformStage(self):
    super(PreCQSyncStage, self).PerformStage()
    self.pool = validation_pool.ValidationPool.AcquirePreCQPool(
        self._run.config.overlays, self._build_root,
        self._run.buildnumber, self._run.config.name,
        dryrun=self._run.options.debug_forced, changes=self.patches,
        builder_run=self._run)
    self.pool.ApplyPoolIntoRepo()

    if len(self.pool.changes) == 0 and self.patches:
      cros_build_lib.Die('No changes have been applied.')


class PreCQLauncherStage(SyncStage):
  """Scans for CLs and automatically launches Pre-CQ jobs to test them."""

  # The number of minutes we wait before launching Pre-CQ jobs. This measures
  # the idle time of a given patch series, so, for example, if a user takes
  # 20 minutes to mark a series of 20 patches as ready, we won't launch a
  # tryjob on any of the patches until the user has been idle for 2 minutes.
  LAUNCH_DELAY = 2

  # The number of minutes we allow before considering a launch attempt failed.
  LAUNCH_TIMEOUT = 30

  # The number of minutes we allow before considering an in-flight job failed.
  INFLIGHT_TIMEOUT = 240

  # The number of minutes we allow before expiring a pre-cq PASSED or
  # FULLY_VERIFIED status. After this timeout is hit, a CL's status will be
  # reset to None. This prevents very stale CLs from entering the CQ.
  STATUS_EXPIRY_TIMEOUT = 60 * 24 * 7

  # The maximum number of patches we will allow in a given trybot run. This is
  # needed because our trybot infrastructure can only handle so many patches at
  # once.
  MAX_PATCHES_PER_TRYBOT_RUN = 50

  # The maximum derivative of the number of tryjobs we will launch in a given
  # cycle of ProcessChanges. Used to rate-limit the launcher when reopening the
  # tree after building up a large backlog.
  MAX_LAUNCHES_PER_CYCLE_DERIVATIVE = 20

  def __init__(self, builder_run, **kwargs):
    super(PreCQLauncherStage, self).__init__(builder_run, **kwargs)
    self.skip_sync = True
    self.last_cycle_launch_count = 0


  def _HasTimedOut(self, start, now, timeout_minutes):
    """Check whether |timeout_minutes| has elapsed between |start| and |now|.

    Args:
      start: datetime.datetime start time.
      now: datetime.datetime current time.
      timeout_minutes: integer number of minutes for timeout.

    Returns:
      True if (now-start) > timeout_minutes.
    """
    diff = datetime.timedelta(minutes=timeout_minutes)
    return (now - start) > diff


  @staticmethod
  def _PrintPatchStatus(patch, status):
    """Print a link to |patch| with |status| info."""
    items = (
        status,
        os.path.basename(patch.project),
        str(patch),
    )
    cros_build_lib.PrintBuildbotLink(' | '.join(items), patch.url)

  def _ConfiguredVerificationsForChange(self, change):
    """Determine which configs to test |change| with.

    This method returns only the configs that are asked for by the config
    file. It does not include special-case logic for adding additional bots
    based on the type of the repository (see VerificationsForChange for that).

    Args:
      change: GerritPatch instance to get configs-to-test for.

    Returns:
      A set of configs to test.
    """
    configs_to_test = None
    # If a pre-cq config is specified in the commit message, use that.
    # Otherwise, look in appropriate COMMIT-QUEUE.ini. Otherwise, default to
    # constants.PRE_CQ_DEFAULT_CONFIGS
    lines = cros_patch.GetOptionLinesFromCommitMessage(
        change.commit_message, constants.PRE_CQ_CONFIGS_OPTION_REGEX)
    if lines is not None:
      configs_to_test = self._ParsePreCQOption(' '.join(lines))
    configs_to_test = configs_to_test or self._ParsePreCQOption(
        triage_lib.GetOptionForChange(self._build_root, change, 'GENERAL',
                                      constants.PRE_CQ_CONFIGS_OPTION))

    return set(configs_to_test or constants.PRE_CQ_DEFAULT_CONFIGS)

  def VerificationsForChange(self, change):
    """Determine which configs to test |change| with.

    Args:
      change: GerritPatch instance to get configs-to-test for.

    Returns:
      A set of configs to test.
    """
    configs_to_test = set(self._ConfiguredVerificationsForChange(change))

    # Add the BINHOST_PRE_CQ to any changes that affect an overlay.
    if '/overlays/' in change.project:
      configs_to_test.add(constants.BINHOST_PRE_CQ)

    return configs_to_test

  def _ParsePreCQOption(self, pre_cq_option):
    """Gets a valid config list, or None, from |pre_cq_option|."""
    if pre_cq_option and pre_cq_option.split():
      configs_to_test = set(pre_cq_option.split())

      # Replace 'default' with the default configs.
      if 'default' in configs_to_test:
        configs_to_test.discard('default')
        configs_to_test.update(constants.PRE_CQ_DEFAULT_CONFIGS)

      # Verify that all of the configs are valid.
      if all(c in self._run.site_config for c in configs_to_test):
        return configs_to_test

    return None

  def ScreenChangeForPreCQ(self, change):
    """Record which pre-cq tryjobs to test |change| with.

    This method determines which configs to test a given |change| with, and
    writes those as pending tryjobs to the cidb.

    Args:
      change: GerritPatch instance to screen. This change should not yet have
              been screened.
    """
    actions = []
    configs_to_test = self.VerificationsForChange(change)
    for c in configs_to_test:
      actions.append(clactions.CLAction.FromGerritPatchAndAction(
          change, constants.CL_ACTION_VALIDATION_PENDING_PRE_CQ,
          reason=c))
    actions.append(clactions.CLAction.FromGerritPatchAndAction(
        change, constants.CL_ACTION_SCREENED_FOR_PRE_CQ))

    build_id, db = self._run.GetCIDBHandle()
    db.InsertCLActions(build_id, actions)

  def CanSubmitChangeInPreCQ(self, change):
    """Look up whether |change| is configured to be submitted in the pre-CQ.

    This looks up the "submit-in-pre-cq" setting inside the project in
    COMMIT-QUEUE.ini and checks whether it is set to "yes".

    [GENERAL]
      submit-in-pre-cq: yes

    Args:
      change: Change to examine.

    Returns:
      Boolean indicating if this change is configured to be submitted
      in the pre-CQ.
    """
    result = None
    try:
      result = triage_lib.GetOptionForChange(
          self._build_root, change, 'GENERAL', 'submit-in-pre-cq')
    except ConfigParser.Error:
      logging.error('%s has malformed config file', change, exc_info=True)
    return bool(result and result.lower() == 'yes')

  def LaunchTrybot(self, plan, configs):
    """Launch a Pre-CQ run with the provided list of CLs.

    Args:
      pool: ValidationPool corresponding to |plan|.
      plan: The list of patches to test in the pre-cq tryjob.
      configs: A list of pre-cq config names to launch.
    """
    cmd = ['cbuildbot', '--remote',
           '--timeout', str(self.INFLIGHT_TIMEOUT * 60)] + configs
    for patch in plan:
      cmd += ['-g', cros_patch.AddPrefix(patch, patch.gerrit_number)]
      self._PrintPatchStatus(patch, 'testing')
    if self._run.options.debug:
      logging.debug('Would have launched tryjob with %s', cmd)
    else:
      cros_build_lib.RunCommand(cmd, cwd=self._build_root)

    build_id, db = self._run.GetCIDBHandle()
    actions = [
        clactions.CLAction.FromGerritPatchAndAction(
            patch, constants.CL_ACTION_TRYBOT_LAUNCHING, config)
        for patch, config in itertools.product(plan, configs)]
    db.InsertCLActions(build_id, actions)

  def GetDisjointTransactionsToTest(self, pool, progress_map):
    """Get the list of disjoint transactions to test.

    Side effect: reject or retry changes that have timed out.

    Args:
      pool: The validation pool.
      progress_map: See return type of clactions.GetPreCQProgressMap.

    Returns:
      A list of (transaction, config) tuples corresponding to different trybots
      that should be launched.
    """
    # Get the set of busy and passed CLs.
    busy, _, verified = clactions.GetPreCQCategories(progress_map)

    screened_changes = set(progress_map)

    # Create a list of disjoint transactions to test.
    manifest = git.ManifestCheckout.Cached(self._build_root)
    plans = pool.CreateDisjointTransactions(
        manifest, screened_changes,
        max_txn_length=self.MAX_PATCHES_PER_TRYBOT_RUN)
    for plan in plans:
      # If any of the CLs in the plan is not yet screened, wait for them to
      # be screened.
      #
      # If any of the CLs in the plan are currently "busy" being tested,
      # wait until they're done before starting to test this plan.
      #
      # Similarly, if all of the CLs in the plan have already been validated,
      # there's no need to launch a trybot run.
      plan = set(plan)
      if not plan.issubset(screened_changes):
        logging.info('CLs waiting to be screened: %s',
                     cros_patch.GetChangesAsString(
                         plan.difference(screened_changes)))
      elif plan.issubset(verified):
        logging.info('CLs already verified: %s',
                     cros_patch.GetChangesAsString(plan))
      elif plan.intersection(busy):
        logging.info('CLs currently being verified: %s',
                     cros_patch.GetChangesAsString(plan.intersection(busy)))
        if plan.difference(busy):
          logging.info('CLs waiting on verification of dependencies: %r',
                       cros_patch.GetChangesAsString(plan.difference(busy)))
      # TODO(akeshet): Consider using a database time rather than gerrit
      # approval time and local clock for launch delay.
      elif any(x.approval_timestamp + self.LAUNCH_DELAY * 60 > time.time()
               for x in plan):
        logging.info('CLs waiting on launch delay: %s',
                     cros_patch.GetChangesAsString(plan))
      else:
        pending_configs = clactions.GetPreCQConfigsToTest(plan, progress_map)
        for config in pending_configs:
          yield (plan, config)

  def _ProcessRequeuedAndSpeculative(self, change, action_history):
    """Detect if |change| was requeued by developer, and mark in cidb.

    Args:
      change: GerritPatch instance to check.
      action_history: List of CLActions.
    """
    action_string = clactions.GetRequeuedOrSpeculative(
        change, action_history, not change.IsMergeable())
    if action_string:
      build_id, db = self._run.GetCIDBHandle()
      action = clactions.CLAction.FromGerritPatchAndAction(
          change, action_string)
      db.InsertCLActions(build_id, [action])

  def _ProcessExpiry(self, change, status, timestamp, pool, current_time):
    """Enforce expiry of a PASSED or FULLY_VERIFIED status.

    Args:
      change: GerritPatch instance to process.
      status: |change|'s pre-cq status.
      timestamp: datetime.datetime for when |status| was achieved.
      pool: The current validation pool.
      current_time: datetime.datetime for current database time.
    """
    if not timestamp:
      return
    timed_out = self._HasTimedOut(timestamp, current_time,
                                  self.STATUS_EXPIRY_TIMEOUT)
    verified = status in (constants.CL_STATUS_PASSED,
                          constants.CL_STATUS_FULLY_VERIFIED)
    if timed_out and verified:
      msg = PRECQ_EXPIRY_MSG % self.STATUS_EXPIRY_TIMEOUT
      build_id, db = self._run.GetCIDBHandle()
      if db:
        pool.SendNotification(change, '%(details)s', details=msg)
        action = clactions.CLAction.FromGerritPatchAndAction(
            change, constants.CL_ACTION_PRE_CQ_RESET)
        db.InsertCLActions(build_id, [action])

  def _ProcessTimeouts(self, change, progress_map, pool, current_time):
    """Enforce per-config launch and inflight timeouts.

    Args:
      change: GerritPatch instance to process.
      progress_map: As returned by clactions.GetCLPreCQProgress a dict mapping
                    each change in |changes| to a dict mapping config names
                    to (status, timestamp) tuples for the configs under test.
      pool: The current validation pool.
      current_time: datetime.datetime timestamp giving current database time.
    """
    # TODO(akeshet) restore trybot launch retries here (there was
    # no straightforward existing mechanism to include them in the
    # transition to parallel pre-cq).
    timeout_statuses = (constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED,
                        constants.CL_PRECQ_CONFIG_STATUS_INFLIGHT)
    config_progress = progress_map[change]
    for config, (config_status, timestamp, _) in config_progress.iteritems():
      if not config_status in timeout_statuses:
        continue
      launched = config_status == constants.CL_PRECQ_CONFIG_STATUS_LAUNCHED
      timeout = self.LAUNCH_TIMEOUT if launched else self.INFLIGHT_TIMEOUT
      msg = (PRECQ_LAUNCH_TIMEOUT_MSG if launched
             else PRECQ_INFLIGHT_TIMEOUT_MSG) % (config, timeout)

      if self._HasTimedOut(timestamp, current_time, timeout):
        pool.SendNotification(change, '%(details)s', details=msg)
        pool.RemoveReady(change, reason=config)
        pool.UpdateCLPreCQStatus(change, constants.CL_STATUS_FAILED)

  def _ProcessVerified(self, change, can_submit, will_submit):
    """Process a change that is fully pre-cq verified.

    Args:
      change: GerritPatch instance to process.
      can_submit: set of changes that can be submitted by the pre-cq.
      will_submit: set of changes that will be submitted by the pre-cq.

    Returns:
      A tuple of (set of changes that should be submitted by pre-cq,
                  set of changes that should be passed by pre-cq)
    """
    # If this change and all its dependencies are pre-cq submittable,
    # and none of them have yet been marked as pre-cq passed, then
    # mark them for submission. Otherwise, mark this change as passed.
    if change in will_submit:
      return set(), set()

    if change in can_submit:
      logging.info('Attempting to determine if %s can be submitted.', change)
      patch_series = validation_pool.PatchSeries(self._build_root)
      try:
        plan = patch_series.CreateTransaction(change, limit_to=can_submit)
        return plan, set()
      except cros_patch.DependencyError:
        pass

    # Changes that cannot be submitted are marked as passed.
    return set(), set([change])

  def UpdateChangeStatuses(self, changes, status):
    """Update |changes| to |status|.

    Args:
      changes: A set of GerritPatch instances.
      status: One of constants.CL_STATUS_* statuses.
    """
    if changes:
      build_id, db = self._run.GetCIDBHandle()
      a = clactions.TranslatePreCQStatusToAction(status)
      actions = [clactions.CLAction.FromGerritPatchAndAction(c, a)
                 for c in changes]
      db.InsertCLActions(build_id, actions)

  def ProcessChanges(self, pool, changes, _non_manifest_changes):
    """Process a list of changes that were marked as Ready.

    From our list of changes that were marked as Ready, we create a
    list of disjoint transactions and send each one to a separate Pre-CQ
    trybot.

    Non-manifest changes are just submitted here because they don't need to be
    verified by either the Pre-CQ or CQ.
    """
    _, db = self._run.GetCIDBHandle()
    action_history = db.GetActionsForChanges(changes)
    for change in changes:
      self._ProcessRequeuedAndSpeculative(change, action_history)

    status_and_timestamp_map = {
        c: clactions.GetCLPreCQStatusAndTime(c, action_history)
        for c in changes}
    status_map = {c: v[0] for c, v in status_and_timestamp_map.items()}

    # Filter out failed speculative changes.
    changes = [c for c in changes if status_map[c] != constants.CL_STATUS_FAILED
               or c.HasReadyFlag()]

    progress_map = clactions.GetPreCQProgressMap(changes, action_history)
    _, inflight, verified = clactions.GetPreCQCategories(progress_map)
    current_db_time = db.GetTime()

    to_process = set(c for c in changes
                     if status_map[c] != constants.CL_STATUS_PASSED)

    # Mark verified changes verified.
    to_mark_verified = [c for c in verified.intersection(to_process) if
                        status_map[c] != constants.CL_STATUS_FULLY_VERIFIED]
    self.UpdateChangeStatuses(to_mark_verified,
                              constants.CL_STATUS_FULLY_VERIFIED)
    # Send notifications to the fully verified changes.
    if to_mark_verified:
      pool.HandlePreCQSuccess(to_mark_verified)

    # Changes that can be submitted, if their dependencies can be too. Only
    # include changes that have not already been marked as passed.
    can_submit = set(c for c in (verified.intersection(to_process)) if
                     c.IsMergeable() and self.CanSubmitChangeInPreCQ(c))

    # Changes that will be submitted.
    will_submit = set()
    # Changes that will be passed.
    will_pass = set()

    # Separately count and log the number of mergable and speculative changes in
    # each of the possible pre-cq statuses (or in status None).
    POSSIBLE_STATUSES = clactions.PRE_CQ_CL_STATUSES | {None}
    status_counts = {}
    for count_bin in itertools.product((True, False), POSSIBLE_STATUSES):
      status_counts[count_bin] = 0
    for c, status in status_map.iteritems():
      count_bin = (c.IsMergeable(), status)
      status_counts[count_bin] = status_counts[count_bin] + 1
    for count_bin, count in sorted(status_counts.items()):
      subtype = 'mergeable' if count_bin[0] else 'speculative'
      status = count_bin[1]
      name = '.'.join(['pre-cq-status', status if status else 'None'])
      logging.info('Sending stat (name, subtype, count): (%s, %s, %s)',
                   name, subtype, count)
      graphite.StatsFactory.GetInstance().Gauge(name).send(subtype, count)

    for change in inflight:
      if status_map[change] != constants.CL_STATUS_INFLIGHT:
        build_ids = [x for _, _, x in progress_map[change].values()]
        # Change the status to inflight.
        self.UpdateChangeStatuses([change], constants.CL_STATUS_INFLIGHT)
        build_dicts = db.GetBuildStatuses(build_ids)
        urls = []
        for b in build_dicts:
          urls.append(tree_status.ConstructDashboardURL(
              b['waterfall'], b['builder_name'], b['build_number']))

        # Send notifications.
        pool.HandleApplySuccess(change, build_log='\n'.join(urls))

    for change in to_process:
      # Detect if change is ready to be marked as passed, or ready to submit.
      if change in verified and change.IsMergeable():
        to_submit, to_pass = self._ProcessVerified(change, can_submit,
                                                   will_submit)
        will_submit.update(to_submit)
        will_pass.update(to_pass)
        continue

      # Screen unscreened changes to determine which trybots to test them with.
      if not clactions.IsChangeScreened(change, action_history):
        self.ScreenChangeForPreCQ(change)
        continue

      self._ProcessTimeouts(change, progress_map, pool, current_db_time)

    # Filter out changes that have already failed, and aren't marked trybot
    # ready or commit ready, before launching.
    launchable_progress_map = {
        k: v for k, v in progress_map.iteritems()
        if k.HasReadyFlag() or status_map[k] != constants.CL_STATUS_FAILED}

    is_tree_open = tree_status.IsTreeOpen(throttled_ok=True)
    launch_count = 0
    cl_launch_count = 0
    launch_count_limit = (self.last_cycle_launch_count +
                          self.MAX_LAUNCHES_PER_CYCLE_DERIVATIVE)
    launches = {}
    for plan, config in self.GetDisjointTransactionsToTest(
        pool, launchable_progress_map):
      launches.setdefault(frozenset(plan), []).append(config)

    for plan, configs in launches.iteritems():
      if not is_tree_open:
        logging.info('Tree is closed, not launching configs %r for plan %s.',
                     configs, cros_patch.GetChangesAsString(plan))
      elif launch_count >= launch_count_limit:
        logging.info('Hit or exceeded maximum launch count of %s this cycle, '
                     'not launching configs %r for plan %s.',
                     launch_count_limit, configs,
                     cros_patch.GetChangesAsString(plan))
      else:
        self.LaunchTrybot(plan, configs)
        launch_count += len(configs)
        cl_launch_count += len(configs) * len(plan)

    graphite.StatsFactory.GetInstance().Counter('pre-cq').increment(
        'launch_count', launch_count)
    graphite.StatsFactory.GetInstance().Counter('pre-cq').increment(
        'cl_launch_count', cl_launch_count)
    graphite.StatsFactory.GetInstance().Counter('pre-cq').increment(
        'tick_count')

    self.last_cycle_launch_count = launch_count

    # Mark passed changes as passed
    self.UpdateChangeStatuses(will_pass, constants.CL_STATUS_PASSED)

    # Expire any very stale passed or fully verified changes.
    for c, v in status_and_timestamp_map.items():
      self._ProcessExpiry(c, v[0], v[1], pool, current_db_time)

    # Submit changes that are ready to submit, if we can.
    if tree_status.IsTreeOpen(throttled_ok=True):
      pool.SubmitNonManifestChanges(check_tree_open=False,
                                    reason=constants.STRATEGY_NONMANIFEST)
      pool.SubmitChanges(will_submit, check_tree_open=False,
                         reason=constants.STRATEGY_PRECQ_SUBMIT)

    # Tell ValidationPool to keep waiting for more changes until we hit
    # its internal timeout.
    return [], []

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    # Setup and initialize the repo.
    super(PreCQLauncherStage, self).PerformStage()

    query = constants.PRECQ_READY_QUERY
    if self._run.options.cq_gerrit_override:
      query = (self._run.options.cq_gerrit_override, None)

    # Loop through all of the changes until we hit a timeout.
    validation_pool.ValidationPool.AcquirePool(
        self._run.config.overlays, self.repo,
        self._run.buildnumber,
        constants.PRE_CQ_LAUNCHER_NAME,
        query,
        dryrun=self._run.options.debug,
        check_tree_open=False, change_filter=self.ProcessChanges,
        builder_run=self._run)
