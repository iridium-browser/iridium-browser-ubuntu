# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main builder code for Chromium OS.

Used by Chromium OS buildbot configuration for all Chromium OS builds including
full and pre-flight-queue builds.
"""

from __future__ import print_function

import collections
import datetime
import distutils.version
import glob
import json
import logging
import multiprocessing
import optparse
import os
import pickle
import sys
import tempfile

from chromite.cbuildbot import afdo
from chromite.cbuildbot import cbuildbot_config
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import remote_try
from chromite.cbuildbot import repository
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import tee
from chromite.cbuildbot import tree_status
from chromite.cbuildbot import trybot_patch_pool
from chromite.cbuildbot.stages import afdo_stages
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import branch_stages
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import chrome_stages
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import release_stages
from chromite.cbuildbot.stages import report_stages
from chromite.cbuildbot.stages import sdk_stages
from chromite.cbuildbot.stages import sync_stages
from chromite.cbuildbot.stages import test_stages


from chromite.lib import cidb
from chromite.lib import cgroups
from chromite.lib import cleanup
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gob_util
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import retry_stats
from chromite.lib import sudo
from chromite.lib import timeout_util

import mock


_DEFAULT_LOG_DIR = 'cbuildbot_logs'
_BUILDBOT_LOG_FILE = 'cbuildbot.log'
_DEFAULT_EXT_BUILDROOT = 'trybot'
_DEFAULT_INT_BUILDROOT = 'trybot-internal'
_BUILDBOT_REQUIRED_BINARIES = ('pbzip2',)
_API_VERSION_ATTR = 'api_version'


def _PrintValidConfigs(display_all=False):
  """Print a list of valid buildbot configs.

  Args:
    display_all: Print all configs.  Otherwise, prints only configs with
                 trybot_list=True.
  """
  def _GetSortKey(config_name):
    config_dict = cbuildbot_config.config[config_name]
    return (not config_dict['trybot_list'], config_dict['description'],
            config_name)

  COLUMN_WIDTH = 45
  print()
  print('config'.ljust(COLUMN_WIDTH), 'description')
  print('------'.ljust(COLUMN_WIDTH), '-----------')
  config_names = cbuildbot_config.config.keys()
  config_names.sort(key=_GetSortKey)
  for name in config_names:
    if display_all or cbuildbot_config.config[name]['trybot_list']:
      desc = cbuildbot_config.config[name].get('description')
      desc = desc if desc else ''
      print(name.ljust(COLUMN_WIDTH), desc)

  print()


def _GetConfig(config_name):
  """Gets the configuration for the build if it exists, None otherwise."""
  if cbuildbot_config.config.has_key(config_name):
    return cbuildbot_config.config[config_name]


class Builder(object):
  """Parent class for all builder types.

  This class functions as an abstract parent class for various build types.
  Its intended use is builder_instance.Run().

  Attributes:
    _run: The BuilderRun object for this run.
    archive_stages: Dict of BuildConfig keys to ArchiveStage values.
    patch_pool: TrybotPatchPool.
  """

  def __init__(self, builder_run):
    """Initializes instance variables. Must be called by all subclasses."""
    self._run = builder_run

    if self._run.config.chromeos_official:
      os.environ['CHROMEOS_OFFICIAL'] = '1'

    self.archive_stages = {}
    self.patch_pool = trybot_patch_pool.TrybotPatchPool()
    self._build_image_lock = multiprocessing.Lock()

  def Initialize(self):
    """Runs through the initialization steps of an actual build."""
    if self._run.options.resume:
      results_lib.LoadCheckpoint(self._run.buildroot)

    self._RunStage(report_stages.BuildStartStage)

    self._RunStage(build_stages.CleanUpStage)

  def _GetStageInstance(self, stage, *args, **kwargs):
    """Helper function to get a stage instance given the args.

    Useful as almost all stages just take in builder_run.
    """
    # Normally the default BuilderRun (self._run) is used, but it can
    # be overridden with "builder_run" kwargs (e.g. for child configs).
    builder_run = kwargs.pop('builder_run', self._run)
    return stage(builder_run, *args, **kwargs)

  def _SetReleaseTag(self):
    """Sets run.attrs.release_tag from the manifest manager used in sync.

    Must be run after sync stage as syncing enables us to have a release tag,
    and must be run before any usage of attrs.release_tag.

    TODO(mtennant): Find a bottleneck place in syncing that can set this
    directly.  Be careful, as there are several kinds of syncing stages, and
    sync stages have been known to abort with sys.exit calls.
    """
    manifest_manager = getattr(self._run.attrs, 'manifest_manager', None)
    if manifest_manager:
      self._run.attrs.release_tag = manifest_manager.current_version
    else:
      self._run.attrs.release_tag = None

    cros_build_lib.Debug('Saved release_tag value for run: %r',
                         self._run.attrs.release_tag)

  def _RunStage(self, stage, *args, **kwargs):
    """Wrapper to run a stage.

    Args:
      stage: A BuilderStage class.
      args: args to pass to stage constructor.
      kwargs: kwargs to pass to stage constructor.

    Returns:
      Whatever the stage's Run method returns.
    """
    stage_instance = self._GetStageInstance(stage, *args, **kwargs)
    return stage_instance.Run()

  @staticmethod
  def _RunParallelStages(stage_objs):
    """Run the specified stages in parallel.

    Args:
      stage_objs: BuilderStage objects.
    """
    steps = [stage.Run for stage in stage_objs]
    try:
      parallel.RunParallelSteps(steps)

    except BaseException as ex:
      # If a stage threw an exception, it might not have correctly reported
      # results (e.g. because it was killed before it could report the
      # results.) In this case, attribute the exception to any stages that
      # didn't report back correctly (if any).
      for stage in stage_objs:
        for name in stage.GetStageNames():
          if not results_lib.Results.StageHasResults(name):
            results_lib.Results.Record(name, ex, str(ex))

      if cidb.CIDBConnectionFactory.IsCIDBSetup():
        db = cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()
        if db:
          for stage in stage_objs:
            for build_stage_id in stage.GetBuildStageIDs():
              if not db.HasBuildStageFailed(build_stage_id):
                failures_lib.ReportStageFailureToCIDB(db,
                                                      build_stage_id,
                                                      ex)

      raise

  def _RunSyncStage(self, sync_instance):
    """Run given |sync_instance| stage and be sure attrs.release_tag set."""
    try:
      sync_instance.Run()
    finally:
      self._SetReleaseTag()

  def GetSyncInstance(self):
    """Returns an instance of a SyncStage that should be run.

    Subclasses must override this method.
    """
    raise NotImplementedError()

  def GetCompletionInstance(self):
    """Returns the MasterSlaveSyncCompletionStage for this build.

    Subclasses may override this method.

    Returns:
      None
    """
    return None

  def RunStages(self):
    """Subclasses must override this method.  Runs the appropriate code."""
    raise NotImplementedError()

  def _ReExecuteInBuildroot(self, sync_instance):
    """Reexecutes self in buildroot and returns True if build succeeds.

    This allows the buildbot code to test itself when changes are patched for
    buildbot-related code.  This is a no-op if the buildroot == buildroot
    of the running chromite checkout.

    Args:
      sync_instance: Instance of the sync stage that was run to sync.

    Returns:
      True if the Build succeeded.
    """
    if not self._run.options.resume:
      results_lib.WriteCheckpoint(self._run.options.buildroot)

    args = sync_stages.BootstrapStage.FilterArgsForTargetCbuildbot(
        self._run.options.buildroot, constants.PATH_TO_CBUILDBOT,
        self._run.options)

    # Specify a buildroot explicitly (just in case, for local trybot).
    # Suppress any timeout options given from the commandline in the
    # invoked cbuildbot; our timeout will enforce it instead.
    args += ['--resume', '--timeout', '0', '--notee', '--nocgroups',
             '--buildroot', os.path.abspath(self._run.options.buildroot)]

    if hasattr(self._run.attrs, 'manifest_manager'):
      # TODO(mtennant): Is this the same as self._run.attrs.release_tag?
      ver = self._run.attrs.manifest_manager.current_version
      args += ['--version', ver]

    pool = getattr(sync_instance, 'pool', None)
    if pool:
      filename = os.path.join(self._run.options.buildroot,
                              'validation_pool.dump')
      pool.Save(filename)
      args += ['--validation_pool', filename]

    # Reset the cache dir so that the child will calculate it automatically.
    if not self._run.options.cache_dir_specified:
      commandline.BaseParser.ConfigureCacheDir(None)

    with tempfile.NamedTemporaryFile(prefix='metadata') as metadata_file:
      metadata_file.write(self._run.attrs.metadata.GetJSON())
      metadata_file.flush()
      args += ['--metadata_dump', metadata_file.name]

      # Re-run the command in the buildroot.
      # Finally, be generous and give the invoked cbuildbot 30s to shutdown
      # when something occurs.  It should exit quicker, but the sigterm may
      # hit while the system is particularly busy.
      return_obj = cros_build_lib.RunCommand(
          args, cwd=self._run.options.buildroot, error_code_ok=True,
          kill_timeout=30)
      return return_obj.returncode == 0

  def _InitializeTrybotPatchPool(self):
    """Generate patch pool from patches specified on the command line.

    Do this only if we need to patch changes later on.
    """
    changes_stage = sync_stages.PatchChangesStage.StageNamePrefix()
    check_func = results_lib.Results.PreviouslyCompletedRecord
    if not check_func(changes_stage) or self._run.options.bootstrap:
      options = self._run.options
      self.patch_pool = trybot_patch_pool.TrybotPatchPool.FromOptions(
          gerrit_patches=options.gerrit_patches,
          local_patches=options.local_patches,
          sourceroot=options.sourceroot,
          remote_patches=options.remote_patches)

  def _GetBootstrapStage(self):
    """Constructs and returns the BootStrapStage object.

    We return None when there are no chromite patches to test, and
    --test-bootstrap wasn't passed in.
    """
    stage = None
    chromite_pool = self.patch_pool.Filter(project=constants.CHROMITE_PROJECT)
    if self._run.config.internal:
      manifest_pool = self.patch_pool.FilterIntManifest()
    else:
      manifest_pool = self.patch_pool.FilterExtManifest()
    chromite_branch = git.GetChromiteTrackingBranch()
    if (chromite_pool or manifest_pool or
        self._run.options.test_bootstrap or
        chromite_branch != self._run.options.branch):
      stage = sync_stages.BootstrapStage(self._run, chromite_pool,
                                         manifest_pool)
    return stage

  def Run(self):
    """Main runner for this builder class.  Runs build and prints summary.

    Returns:
      Whether the build succeeded.
    """
    self._InitializeTrybotPatchPool()

    if self._run.options.bootstrap:
      bootstrap_stage = self._GetBootstrapStage()
      if bootstrap_stage:
        # BootstrapStage blocks on re-execution of cbuildbot.
        bootstrap_stage.Run()
        return bootstrap_stage.returncode == 0

    print_report = True
    exception_thrown = False
    success = True
    sync_instance = None
    try:
      self.Initialize()
      sync_instance = self.GetSyncInstance()
      self._RunSyncStage(sync_instance)

      if self._run.ShouldPatchAfterSync():
        # Filter out patches to manifest, since PatchChangesStage can't handle
        # them.  Manifest patches are patched in the BootstrapStage.
        non_manifest_patches = self.patch_pool.FilterManifest(negate=True)
        if non_manifest_patches:
          self._RunStage(sync_stages.PatchChangesStage, non_manifest_patches)

      if self._run.ShouldReexecAfterSync():
        print_report = False
        success = self._ReExecuteInBuildroot(sync_instance)
      else:
        self._RunStage(report_stages.BuildReexecutionFinishedStage)
        self.RunStages()

    except Exception as ex:
      exception_thrown = True
      if results_lib.Results.BuildSucceededSoFar():
        # If the build is marked as successful, but threw exceptions, that's a
        # problem. Print the traceback for debugging.
        if isinstance(ex, failures_lib.CompoundFailure):
          print(str(ex))

        cros_build_lib.PrintDetailedTraceback(file=sys.stdout)
        raise

      if not (print_report and isinstance(ex, failures_lib.StepFailure)):
        # If the failed build threw a non-StepFailure exception, we
        # should raise it.
        raise

    finally:
      if print_report:
        results_lib.WriteCheckpoint(self._run.options.buildroot)
        completion_instance = self.GetCompletionInstance()
        self._RunStage(report_stages.ReportStage, sync_instance,
                       completion_instance)
        success = results_lib.Results.BuildSucceededSoFar()
        if exception_thrown and success:
          success = False
          cros_build_lib.PrintBuildbotStepWarnings()
          print("""\
Exception thrown, but all stages marked successful. This is an internal error,
because the stage that threw the exception should be marked as failing.""")

    return success


BoardConfig = collections.namedtuple('BoardConfig', ['board', 'name'])


class SimpleBuilder(Builder):
  """Builder that performs basic vetting operations."""

  def GetSyncInstance(self):
    """Sync to lkgm or TOT as necessary.

    Returns:
      The instance of the sync stage to run.
    """
    if self._run.options.force_version:
      sync_stage = self._GetStageInstance(
          sync_stages.ManifestVersionedSyncStage)
    elif self._run.config.use_lkgm:
      sync_stage = self._GetStageInstance(sync_stages.LKGMSyncStage)
    elif self._run.config.use_chrome_lkgm:
      sync_stage = self._GetStageInstance(chrome_stages.ChromeLKGMSyncStage)
    else:
      sync_stage = self._GetStageInstance(sync_stages.SyncStage)

    return sync_stage

  def _RunHWTests(self, builder_run, board):
    """Run hwtest-related stages for the specified board.

    Args:
      builder_run: BuilderRun object for these background stages.
      board: Board name.
    """
    parallel_stages = []

    # We can not run hw tests without archiving the payloads.
    if builder_run.options.archive:
      for suite_config in builder_run.config.hw_tests:
        stage_class = None
        if suite_config.async:
          stage_class = test_stages.ASyncHWTestStage
        elif suite_config.suite == constants.HWTEST_AU_SUITE:
          stage_class = test_stages.AUTestStage
        else:
          stage_class = test_stages.HWTestStage
        if suite_config.blocking:
          self._RunStage(stage_class, board, suite_config,
                         builder_run=builder_run)
        else:
          new_stage = self._GetStageInstance(stage_class, board,
                                             suite_config,
                                             builder_run=builder_run)
          parallel_stages.append(new_stage)

    self._RunParallelStages(parallel_stages)

  def _RunBackgroundStagesForBoardAndMarkAsSuccessful(self, builder_run, board):
    """Run background board-specific stages for the specified board.

    After finishing the build, mark it as successful.

    Args:
      builder_run: BuilderRun object for these background stages.
      board: Board name.
    """
    self._RunBackgroundStagesForBoard(builder_run, board)
    board_runattrs = builder_run.GetBoardRunAttrs(board)
    board_runattrs.SetParallel('success', True)

  def _RunBackgroundStagesForBoard(self, builder_run, board):
    """Run background board-specific stages for the specified board.

    Used by _RunBackgroundStagesForBoardAndMarkAsSuccessful. Callers should use
    that method instead.

    Args:
      builder_run: BuilderRun object for these background stages.
      board: Board name.
    """
    config = builder_run.config

    # TODO(mtennant): This is the last usage of self.archive_stages.  We can
    # kill it once we migrate its uses to BuilderRun so that none of the
    # stages below need it as an argument.
    archive_stage = self.archive_stages[BoardConfig(board, config.name)]
    if config.afdo_generate_min:
      self._RunParallelStages([archive_stage])
      return

    # paygen can't complete without push_image.
    assert not config.paygen or config.push_image

    if config.build_packages_in_background:
      self._RunStage(build_stages.BuildPackagesStage, board,
                     update_metadata=True, builder_run=builder_run,
                     afdo_use=config.afdo_use)

    if builder_run.config.compilecheck or builder_run.options.compilecheck:
      self._RunStage(test_stages.UnitTestStage, board,
                     builder_run=builder_run)
      return

    # Build the image first before doing anything else.
    # TODO(davidjames): Remove this lock once http://crbug.com/352994 is fixed.
    with self._build_image_lock:
      self._RunStage(build_stages.BuildImageStage, board,
                     builder_run=builder_run, afdo_use=config.afdo_use)

    # While this stage list is run in parallel, the order here dictates the
    # order that things will be shown in the log.  So group things together
    # that make sense when read in order.  Also keep in mind that, since we
    # gather output manually, early slow stages will prevent any output from
    # later stages showing up until it finishes.
    stage_list = [[chrome_stages.ChromeSDKStage, board]]

    if config.vm_test_runs > 1:
      # Run the VMTests multiple times to see if they fail.
      stage_list += [
          [generic_stages.RepeatStage, config.vm_test_runs,
           test_stages.VMTestStage, board]]
    else:
      # Give the VMTests one retry attempt in case failures are flaky.
      stage_list += [[generic_stages.RetryStage, 1, test_stages.VMTestStage,
                      board]]

    if config.afdo_generate:
      stage_list += [[afdo_stages.AFDODataGenerateStage, board]]

    stage_list += [
        [release_stages.SignerTestStage, board, archive_stage],
        [release_stages.PaygenStage, board, archive_stage],
        [test_stages.ImageTestStage, board],
        [test_stages.UnitTestStage, board],
        [artifact_stages.UploadPrebuiltsStage, board],
        [artifact_stages.DevInstallerPrebuiltsStage, board],
        [artifact_stages.DebugSymbolsStage, board],
        [artifact_stages.CPEExportStage, board],
        [artifact_stages.UploadTestArtifactsStage, board],
    ]

    stage_objs = [self._GetStageInstance(*x, builder_run=builder_run)
                  for x in stage_list]

    parallel.RunParallelSteps([
        lambda: self._RunParallelStages(stage_objs + [archive_stage]),
        lambda: self._RunHWTests(builder_run, board),
    ])

  def _RunSetupBoard(self):
    """Run the SetupBoard stage for all child configs and boards."""
    for builder_run in self._run.GetUngroupedBuilderRuns():
      for board in builder_run.config.boards:
        self._RunStage(build_stages.SetupBoardStage, board,
                       builder_run=builder_run)

  def _RunChrootBuilderTypeBuild(self):
    """Runs through stages of a CHROOT_BUILDER_TYPE build."""
    # Unlike normal CrOS builds, the SDK has no concept of pinned CrOS manifest
    # or specific Chrome version.  Use a datestamp instead.
    version = datetime.datetime.now().strftime('%Y.%m.%d.%H%M%S')
    self._RunStage(build_stages.UprevStage, boards=[], enter_chroot=False)
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(build_stages.SetupBoardStage, constants.CHROOT_BUILDER_BOARD)
    self._RunStage(chrome_stages.SyncChromeStage)
    self._RunStage(chrome_stages.PatchChromeStage)
    self._RunStage(sdk_stages.SDKBuildToolchainsStage)
    self._RunStage(sdk_stages.SDKPackageStage, version=version)
    self._RunStage(sdk_stages.SDKTestStage)
    self._RunStage(artifact_stages.UploadPrebuiltsStage,
                   constants.CHROOT_BUILDER_BOARD, version=version)

  def _RunRefreshPackagesTypeBuild(self):
    """Runs through the stages of a REFRESH_PACKAGES_TYPE build."""
    self._RunStage(build_stages.InitSDKStage)
    self._RunSetupBoard()
    self._RunStage(report_stages.RefreshPackageStatusStage)

  def _RunMasterPaladinOrChromePFQBuild(self):
    """Runs through the stages of the paladin or chrome PFQ master build."""
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(build_stages.UprevStage)
    # The CQ/Chrome PFQ master will not actually run the SyncChrome stage, but
    # we want the logic that gets triggered when SyncChrome stage is skipped.
    self._RunStage(chrome_stages.SyncChromeStage)
    self._RunStage(artifact_stages.MasterUploadPrebuiltsStage)

  def _RunPayloadsBuild(self):
    """Run the PaygenStage once for each board."""
    def _RunStageWrapper(board):
      self._RunStage(release_stages.PaygenStage, board=board,
                     channels=self._run.options.channels, archive_stage=None)

    with parallel.BackgroundTaskRunner(_RunStageWrapper) as queue:
      for board in self._run.config.boards:
        queue.put([board])

  def _RunDefaultTypeBuild(self):
    """Runs through the stages of a non-special-type build."""
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(build_stages.UprevStage)
    self._RunSetupBoard()
    self._RunStage(chrome_stages.SyncChromeStage)
    self._RunStage(chrome_stages.PatchChromeStage)

    # Prepare stages to run in background.  If child_configs exist then
    # run each of those here, otherwise use default config.
    builder_runs = self._run.GetUngroupedBuilderRuns()

    tasks = []
    for builder_run in builder_runs:
      # Prepare a local archive directory for each "run".
      builder_run.GetArchive().SetupArchivePath()

      for board in builder_run.config.boards:
        archive_stage = self._GetStageInstance(
            artifact_stages.ArchiveStage, board, builder_run=builder_run,
            chrome_version=self._run.attrs.chrome_version)
        board_config = BoardConfig(board, builder_run.config.name)
        self.archive_stages[board_config] = archive_stage
        tasks.append((builder_run, board))

    # Set up a process pool to run test/archive stages in the background.
    # This process runs task(board) for each board added to the queue.
    task_runner = self._RunBackgroundStagesForBoardAndMarkAsSuccessful
    with parallel.BackgroundTaskRunner(task_runner) as queue:
      for builder_run, board in tasks:
        if not builder_run.config.build_packages_in_background:
          # Run BuildPackages in the foreground, generating or using AFDO data
          # if requested.
          kwargs = {'builder_run': builder_run}
          if builder_run.config.afdo_generate_min:
            kwargs['afdo_generate_min'] = True
          elif builder_run.config.afdo_use:
            kwargs['afdo_use'] = True

          self._RunStage(build_stages.BuildPackagesStage, board,
                         update_metadata=True, **kwargs)

          if (builder_run.config.afdo_generate_min and
              afdo.CanGenerateAFDOData(board)):
            # Generate the AFDO data before allowing any other tasks to run.
            self._RunStage(build_stages.BuildImageStage, board, **kwargs)
            self._RunStage(artifact_stages.UploadTestArtifactsStage, board,
                           builder_run=builder_run,
                           suffix='[afdo_generate_min]')
            suite = cbuildbot_config.AFDORecordTest()
            self._RunStage(test_stages.HWTestStage, board, suite,
                           builder_run=builder_run)
            self._RunStage(afdo_stages.AFDODataGenerateStage, board,
                           builder_run=builder_run)

          if (builder_run.config.afdo_generate_min and
              builder_run.config.afdo_update_ebuild):
            self._RunStage(afdo_stages.AFDOUpdateEbuildStage,
                           builder_run=builder_run)

        # Kick off our background stages.
        queue.put([builder_run, board])

  def RunStages(self):
    """Runs through build process."""
    # TODO(sosa): Split these out into classes.
    if self._run.config.build_type == constants.PRE_CQ_LAUNCHER_TYPE:
      self._RunStage(sync_stages.PreCQLauncherStage)
    elif self._run.config.build_type == constants.CREATE_BRANCH_TYPE:
      self._RunStage(branch_stages.BranchUtilStage)
    elif self._run.config.build_type == constants.CHROOT_BUILDER_TYPE:
      self._RunChrootBuilderTypeBuild()
    elif self._run.config.build_type == constants.REFRESH_PACKAGES_TYPE:
      self._RunRefreshPackagesTypeBuild()
    elif ((self._run.config.build_type == constants.PALADIN_TYPE or
           self._run.config.build_type == constants.CHROME_PFQ_TYPE) and
          self._run.config.master):
      self._RunMasterPaladinOrChromePFQBuild()
    elif self._run.config.build_type == constants.PAYLOADS_TYPE:
      self._RunPayloadsBuild()
    else:
      self._RunDefaultTypeBuild()


class DistributedBuilder(SimpleBuilder):
  """Build class that has special logic to handle distributed builds.

  These builds sync using git/manifest logic in manifest_versions.  In general
  they use a non-distributed builder code for the bulk of the work.
  """

  def __init__(self, *args, **kwargs):
    """Initializes a buildbot builder.

    Extra variables:
      completion_stage_class:  Stage used to complete a build.  Set in the Sync
        stage.
    """
    super(DistributedBuilder, self).__init__(*args, **kwargs)
    self.completion_stage_class = None
    self.sync_stage = None
    self._completion_stage = None

  def GetSyncInstance(self):
    """Syncs the tree using one of the distributed sync logic paths.

    Returns:
      The instance of the sync stage to run.
    """
    # Determine sync class to use.  CQ overrides PFQ bits so should check it
    # first.
    if self._run.config.pre_cq or self._run.options.pre_cq:
      sync_stage = self._GetStageInstance(sync_stages.PreCQSyncStage,
                                          self.patch_pool.gerrit_patches)
      self.completion_stage_class = completion_stages.PreCQCompletionStage
      self.patch_pool.gerrit_patches = []
    elif cbuildbot_config.IsCQType(self._run.config.build_type):
      if self._run.config.do_not_apply_cq_patches:
        sync_stage = self._GetStageInstance(
            sync_stages.MasterSlaveLKGMSyncStage)
      else:
        sync_stage = self._GetStageInstance(sync_stages.CommitQueueSyncStage)
      self.completion_stage_class = completion_stages.CommitQueueCompletionStage
    elif cbuildbot_config.IsPFQType(self._run.config.build_type):
      sync_stage = self._GetStageInstance(sync_stages.MasterSlaveLKGMSyncStage)
      self.completion_stage_class = (
          completion_stages.MasterSlaveSyncCompletionStage)
    elif cbuildbot_config.IsCanaryType(self._run.config.build_type):
      sync_stage = self._GetStageInstance(
          sync_stages.ManifestVersionedSyncStage)
      self.completion_stage_class = (
          completion_stages.CanaryCompletionStage)
    else:
      sync_stage = self._GetStageInstance(
          sync_stages.ManifestVersionedSyncStage)
      self.completion_stage_class = (
          completion_stages.ManifestVersionedSyncCompletionStage)

    self.sync_stage = sync_stage
    return self.sync_stage

  def GetCompletionInstance(self):
    """Returns the completion_stage_class instance that was used for this build.

    Returns:
      None if the completion_stage instance was not yet created (this
      occurs during Publish).
    """
    return self._completion_stage

  def Publish(self, was_build_successful, build_finished):
    """Completes build by publishing any required information.

    Args:
      was_build_successful: Whether the build succeeded.
      build_finished: Whether the build completed. A build can be successful
        without completing if it exits early with sys.exit(0).
    """
    completion_stage = self._GetStageInstance(self.completion_stage_class,
                                              self.sync_stage,
                                              was_build_successful)
    self._completion_stage = completion_stage
    completion_successful = False
    try:
      completion_stage.Run()
      completion_successful = True
      if (self._run.config.afdo_update_ebuild and
          not self._run.config.afdo_generate_min):
        self._RunStage(afdo_stages.AFDOUpdateEbuildStage)
    finally:
      if self._run.config.push_overlays:
        publish = (was_build_successful and completion_successful and
                   build_finished)
        self._RunStage(completion_stages.PublishUprevChangesStage, publish)

  def RunStages(self):
    """Runs simple builder logic and publishes information to overlays."""
    was_build_successful = False
    build_finished = False
    try:
      super(DistributedBuilder, self).RunStages()
      was_build_successful = results_lib.Results.BuildSucceededSoFar()
      build_finished = True
    except SystemExit as ex:
      # If a stage calls sys.exit(0), it's exiting with success, so that means
      # we should mark ourselves as successful.
      cros_build_lib.Info('Detected sys.exit(%s)', ex.code)
      if ex.code == 0:
        was_build_successful = True
      raise
    finally:
      self.Publish(was_build_successful, build_finished)


def _ConfirmBuildRoot(buildroot):
  """Confirm with user the inferred buildroot, and mark it as confirmed."""
  cros_build_lib.Warning('Using default directory %s as buildroot', buildroot)
  if not cros_build_lib.BooleanPrompt(default=False):
    print('Please specify a different buildroot via the --buildroot option.')
    sys.exit(0)

  if not os.path.exists(buildroot):
    os.mkdir(buildroot)

  repository.CreateTrybotMarker(buildroot)


def _ConfirmRemoteBuildbotRun():
  """Confirm user wants to run with --buildbot --remote."""
  cros_build_lib.Warning(
      'You are about to launch a PRODUCTION job!  This is *NOT* a '
      'trybot run! Are you sure?')
  if not cros_build_lib.BooleanPrompt(default=False):
    print('Please specify --pass-through="--debug".')
    sys.exit(0)


def _DetermineDefaultBuildRoot(sourceroot, internal_build):
  """Default buildroot to be under the directory that contains current checkout.

  Args:
    internal_build: Whether the build is an internal build
    sourceroot: Use specified sourceroot.
  """
  if not repository.IsARepoRoot(sourceroot):
    cros_build_lib.Die(
        'Could not find root of local checkout at %s.  Please specify '
        'using the --sourceroot option.' % sourceroot)

  # Place trybot buildroot under the directory containing current checkout.
  top_level = os.path.dirname(os.path.realpath(sourceroot))
  if internal_build:
    buildroot = os.path.join(top_level, _DEFAULT_INT_BUILDROOT)
  else:
    buildroot = os.path.join(top_level, _DEFAULT_EXT_BUILDROOT)

  return buildroot


def _BackupPreviousLog(log_file, backup_limit=25):
  """Rename previous log.

  Args:
    log_file: The absolute path to the previous log.
    backup_limit: Maximum number of old logs to keep.
  """
  if os.path.exists(log_file):
    old_logs = sorted(glob.glob(log_file + '.*'),
                      key=distutils.version.LooseVersion)

    if len(old_logs) >= backup_limit:
      os.remove(old_logs[0])

    last = 0
    if old_logs:
      last = int(old_logs.pop().rpartition('.')[2])

    os.rename(log_file, log_file + '.' + str(last + 1))


def _IsDistributedBuilder(options, chrome_rev, build_config):
  """Determines whether the builder should be a DistributedBuilder.

  Args:
    options: options passed on the commandline.
    chrome_rev: Chrome revision to build.
    build_config: Builder configuration dictionary.

  Returns:
    True if the builder should be a distributed_builder
  """
  if build_config['pre_cq'] or options.pre_cq:
    return True
  elif not options.buildbot:
    return False
  elif chrome_rev in (constants.CHROME_REV_TOT,
                      constants.CHROME_REV_LOCAL,
                      constants.CHROME_REV_SPEC):
    # We don't do distributed logic to TOT Chrome PFQ's, nor local
    # chrome roots (e.g. chrome try bots)
    # TODO(davidjames): Update any builders that rely on this logic to use
    # manifest_version=False instead.
    return False
  elif build_config['manifest_version']:
    return True

  return False


def _RunBuildStagesWrapper(options, build_config):
  """Helper function that wraps RunBuildStages()."""
  cros_build_lib.Info('cbuildbot was executed with args %s' %
                      cros_build_lib.CmdToStr(sys.argv))

  chrome_rev = build_config['chrome_rev']
  if options.chrome_rev:
    chrome_rev = options.chrome_rev
  if chrome_rev == constants.CHROME_REV_TOT:
    options.chrome_version = gob_util.GetTipOfTrunkRevision(
        constants.CHROMIUM_GOB_URL)
    options.chrome_rev = constants.CHROME_REV_SPEC

  # If it's likely we'll need to build Chrome, fetch the source.
  if build_config['sync_chrome'] is None:
    options.managed_chrome = (
        chrome_rev != constants.CHROME_REV_LOCAL and
        (not build_config['usepkg_build_packages'] or chrome_rev or
         build_config['profile'] or options.rietveld_patches))
  else:
    options.managed_chrome = build_config['sync_chrome']

  if options.managed_chrome:
    # Tell Chrome to fetch the source locally.
    internal = constants.USE_CHROME_INTERNAL in build_config['useflags']
    chrome_src = 'chrome-src-internal' if internal else 'chrome-src'
    options.chrome_root = os.path.join(options.cache_dir, 'distfiles', 'target',
                                       chrome_src)
  elif options.rietveld_patches:
    cros_build_lib.Die('This builder does not support Rietveld patches.')

  metadata_dump_dict = {}
  if options.metadata_dump:
    with open(options.metadata_dump, 'r') as metadata_file:
      metadata_dump_dict = json.loads(metadata_file.read())

  # We are done munging options values, so freeze options object now to avoid
  # further abuse of it.
  # TODO(mtennant): one by one identify each options value override and see if
  # it can be handled another way.  Try to push this freeze closer and closer
  # to the start of the script (e.g. in or after _PostParseCheck).
  options.Freeze()

  with parallel.Manager() as manager:
    builder_run = cbuildbot_run.BuilderRun(options, build_config, manager)
    if metadata_dump_dict:
      builder_run.attrs.metadata.UpdateWithDict(metadata_dump_dict)
    if _IsDistributedBuilder(options, chrome_rev, build_config):
      builder_cls = DistributedBuilder
    else:
      builder_cls = SimpleBuilder
    builder = builder_cls(builder_run)
    if not builder.Run():
      sys.exit(1)


# Parser related functions
def _CheckLocalPatches(sourceroot, local_patches):
  """Do an early quick check of the passed-in patches.

  If the branch of a project is not specified we append the current branch the
  project is on.

  TODO(davidjames): The project:branch format isn't unique, so this means that
  we can't differentiate what directory the user intended to apply patches to.
  We should references by directory instead.

  Args:
    sourceroot: The checkout where patches are coming from.
    local_patches: List of patches to check in project:branch format.

  Returns:
    A list of patches that have been verified, in project:branch format.
  """
  verified_patches = []
  manifest = git.ManifestCheckout.Cached(sourceroot)
  for patch in local_patches:
    project, _, branch = patch.partition(':')

    checkouts = manifest.FindCheckouts(project, only_patchable=True)
    if not checkouts:
      cros_build_lib.Die('Project %s does not exist.' % (project,))
    if len(checkouts) > 1:
      cros_build_lib.Die(
          'We do not yet support local patching for projects that are checked '
          'out to multiple directories. Try uploading your patch to gerrit '
          'and referencing it via the -g option instead.'
      )

    ok = False
    for checkout in checkouts:
      project_dir = checkout.GetPath(absolute=True)

      # If no branch was specified, we use the project's current branch.
      if not branch:
        local_branch = git.GetCurrentBranch(project_dir)
      else:
        local_branch = branch

      if local_branch and git.DoesCommitExistInRepo(project_dir, local_branch):
        verified_patches.append('%s:%s' % (project, local_branch))
        ok = True

    if not ok:
      if branch:
        cros_build_lib.Die('Project %s does not have branch %s'
                           % (project, branch))
      else:
        cros_build_lib.Die('Project %s is not on a branch!' % (project,))

  return verified_patches


def _CheckChromeVersionOption(_option, _opt_str, value, parser):
  """Upgrade other options based on chrome_version being passed."""
  value = value.strip()

  if parser.values.chrome_rev is None and value:
    parser.values.chrome_rev = constants.CHROME_REV_SPEC

  parser.values.chrome_version = value


def _CheckChromeRootOption(_option, _opt_str, value, parser):
  """Validate and convert chrome_root to full-path form."""
  if parser.values.chrome_rev is None:
    parser.values.chrome_rev = constants.CHROME_REV_LOCAL

  parser.values.chrome_root = value


def _CheckChromeRevOption(_option, _opt_str, value, parser):
  """Validate the chrome_rev option."""
  value = value.strip()
  if value not in constants.VALID_CHROME_REVISIONS:
    raise optparse.OptionValueError('Invalid chrome rev specified')

  parser.values.chrome_rev = value


def FindCacheDir(_parser, _options):
  return None


class CustomGroup(optparse.OptionGroup):
  """Custom option group which supports arguments passed-through to trybot."""

  def add_remote_option(self, *args, **kwargs):
    """For arguments that are passed-through to remote trybot."""
    return optparse.OptionGroup.add_option(self, *args,
                                           remote_pass_through=True,
                                           **kwargs)


class CustomOption(commandline.FilteringOption):
  """Subclass FilteringOption class to implement pass-through and api."""

  ACTIONS = commandline.FilteringOption.ACTIONS + ('extend',)
  STORE_ACTIONS = commandline.FilteringOption.STORE_ACTIONS + ('extend',)
  TYPED_ACTIONS = commandline.FilteringOption.TYPED_ACTIONS + ('extend',)
  ALWAYS_TYPED_ACTIONS = (commandline.FilteringOption.ALWAYS_TYPED_ACTIONS +
                          ('extend',))

  def __init__(self, *args, **kwargs):
    # The remote_pass_through argument specifies whether we should directly
    # pass the argument (with its value) onto the remote trybot.
    self.pass_through = kwargs.pop('remote_pass_through', False)
    self.api_version = int(kwargs.pop('api', '0'))
    commandline.FilteringOption.__init__(self, *args, **kwargs)

  def take_action(self, action, dest, opt, value, values, parser):
    if action == 'extend':
      # If there is extra spaces between each argument, we get '' which later
      # code barfs on, so skip those.  e.g. We see this with the forms:
      #  cbuildbot -p 'proj:branch ' ...
      #  cbuildbot -p ' proj:branch' ...
      #  cbuildbot -p 'proj:branch  proj2:branch' ...
      lvalue = value.split()
      values.ensure_value(dest, []).extend(lvalue)

    commandline.FilteringOption.take_action(
        self, action, dest, opt, value, values, parser)


class CustomParser(commandline.FilteringParser):
  """Custom option parser which supports arguments passed-trhough to trybot"""

  DEFAULT_OPTION_CLASS = CustomOption

  def add_remote_option(self, *args, **kwargs):
    """For arguments that are passed-through to remote trybot."""
    return self.add_option(*args, remote_pass_through=True, **kwargs)


def _CreateParser():
  """Generate and return the parser with all the options."""
  # Parse options
  usage = 'usage: %prog [options] buildbot_config [buildbot_config ...]'
  parser = CustomParser(usage=usage, caching=FindCacheDir)

  # Main options
  parser.add_option('-l', '--list', action='store_true', dest='list',
                    default=False,
                    help='List the suggested trybot configs to use (see --all)')
  parser.add_option('-a', '--all', action='store_true', dest='print_all',
                    default=False,
                    help='List all of the buildbot configs available w/--list')

  parser.add_option('--local', default=False, action='store_true',
                    help=('Specifies that this tryjob should be run locally. '
                          'Implies --debug.'))
  parser.add_option('--remote', default=False, action='store_true',
                    help='Specifies that this tryjob should be run remotely.')

  parser.add_remote_option('-b', '--branch',
                           help=('The manifest branch to test.  The branch to '
                                 'check the buildroot out to.'))
  parser.add_option('-r', '--buildroot', dest='buildroot', type='path',
                    help=('Root directory where source is checked out to, and '
                          'where the build occurs. For external build configs, '
                          "defaults to 'trybot' directory at top level of your "
                          'repo-managed checkout.'))
  parser.add_remote_option('--chrome_rev', default=None, type='string',
                           action='callback', dest='chrome_rev',
                           callback=_CheckChromeRevOption,
                           help=('Revision of Chrome to use, of type [%s]'
                                 % '|'.join(constants.VALID_CHROME_REVISIONS)))
  parser.add_remote_option('--profile', default=None, type='string',
                           action='store', dest='profile',
                           help='Name of profile to sub-specify board variant.')

  #
  # Patch selection options.
  #

  group = CustomGroup(
      parser,
      'Patch Options')

  group.add_remote_option('-g', '--gerrit-patches', action='extend',
                          default=[], type='string',
                          metavar="'Id1 *int_Id2...IdN'",
                          help=('Space-separated list of short-form Gerrit '
                                "Change-Id's or change numbers to patch. "
                                "Please prepend '*' to internal Change-Id's"))
  group.add_remote_option('-G', '--rietveld-patches', action='extend',
                          default=[], type='string',
                          metavar="'id1[:subdir1]...idN[:subdirN]'",
                          help=('Space-separated list of short-form Rietveld '
                                'issue numbers to patch. If no subdir is '
                                'specified, the src directory is used.'))
  group.add_option('-p', '--local-patches', action='extend', default=[],
                   metavar="'<project1>[:<branch1>]...<projectN>[:<branchN>]'",
                   help=('Space-separated list of project branches with '
                         'patches to apply.  Projects are specified by name. '
                         'If no branch is specified the current branch of the '
                         'project will be used.'))

  parser.add_option_group(group)

  #
  # Remote trybot options.
  #

  group = CustomGroup(
      parser,
      'Remote Trybot Options (--remote)')

  group.add_remote_option('--hwtest', dest='hwtest', action='store_true',
                          default=False,
                          help='Run the HWTest stage (tests on real hardware)')
  group.add_option('--remote-description', default=None,
                   help=('Attach an optional description to a --remote run '
                         'to make it easier to identify the results when it '
                         'finishes'))
  group.add_option('--slaves', action='extend', default=[],
                   help=('Specify specific remote tryslaves to run on (e.g. '
                         'build149-m2); if the bot is busy, it will be queued'))
  group.add_remote_option('--channel', dest='channels', action='extend',
                          default=[],
                          help=('Specify a channel for a payloads trybot. Can '
                                'be specified multiple times. No valid for '
                                'non-payloads configs.'))
  group.add_option('--test-tryjob', action='store_true',
                   default=False,
                   help=('Submit a tryjob to the test repository.  Will not '
                         'show up on the production trybot waterfall.'))

  parser.add_option_group(group)

  #
  # Branch creation options.
  #

  group = CustomGroup(
      parser,
      'Branch Creation Options (used with branch-util)')

  group.add_remote_option('--branch-name',
                          help='The branch to create or delete.')
  group.add_remote_option('--delete-branch', default=False, action='store_true',
                          help='Delete the branch specified in --branch-name.')
  group.add_remote_option('--rename-to', type='string',
                          help='Rename a branch to the specified name.')
  group.add_remote_option('--force-create', default=False, action='store_true',
                          help='Overwrites an existing branch.')

  parser.add_option_group(group)

  #
  # Advanced options.
  #

  group = CustomGroup(
      parser,
      'Advanced Options',
      'Caution: use these options at your own risk.')

  group.add_remote_option('--bootstrap-args', action='append', default=[],
                          help=('Args passed directly to the bootstrap re-exec '
                                'to skip verification by the bootstrap code'))
  group.add_remote_option('--buildbot', dest='buildbot', action='store_true',
                          default=False, help='This is running on a buildbot')
  group.add_remote_option('--buildnumber', help='build number', type='int',
                          default=0)
  group.add_option('--chrome_root', default=None, type='path',
                   action='callback', callback=_CheckChromeRootOption,
                   dest='chrome_root', help='Local checkout of Chrome to use.')
  group.add_remote_option('--chrome_version', default=None, type='string',
                          action='callback', dest='chrome_version',
                          callback=_CheckChromeVersionOption,
                          help=('Used with SPEC logic to force a particular '
                                'git revision of chrome rather than the '
                                'latest.'))
  group.add_remote_option('--clobber', action='store_true', dest='clobber',
                          default=False,
                          help='Clears an old checkout before syncing')
  group.add_remote_option('--latest-toolchain', action='store_true',
                          default=False,
                          help='Use the latest toolchain.')
  parser.add_option('--log_dir', dest='log_dir', type='path',
                    help=('Directory where logs are stored.'))
  group.add_remote_option('--maxarchives', dest='max_archive_builds',
                          default=3, type='int',
                          help='Change the local saved build count limit.')
  parser.add_remote_option('--manifest-repo-url',
                           help=('Overrides the default manifest repo url.'))
  group.add_remote_option('--compilecheck', action='store_true', default=False,
                          help='Only verify compilation and unit tests.')
  group.add_remote_option('--noarchive', action='store_false', dest='archive',
                          default=True, help="Don't run archive stage.")
  group.add_remote_option('--nobootstrap', action='store_false',
                          dest='bootstrap', default=True,
                          help=("Don't checkout and run from a standalone "
                                'chromite repo.'))
  group.add_remote_option('--nobuild', action='store_false', dest='build',
                          default=True,
                          help="Don't actually build (for cbuildbot dev)")
  group.add_remote_option('--noclean', action='store_false', dest='clean',
                          default=True, help="Don't clean the buildroot")
  group.add_remote_option('--nocgroups', action='store_false', dest='cgroups',
                          default=True,
                          help='Disable cbuildbots usage of cgroups.')
  group.add_remote_option('--nochromesdk', action='store_false',
                          dest='chrome_sdk', default=True,
                          help=("Don't run the ChromeSDK stage which builds "
                                'Chrome outside of the chroot.'))
  group.add_remote_option('--noprebuilts', action='store_false',
                          dest='prebuilts', default=True,
                          help="Don't upload prebuilts.")
  group.add_remote_option('--nopatch', action='store_false',
                          dest='postsync_patch', default=True,
                          help=("Don't run PatchChanges stage.  This does not "
                                'disable patching in of chromite patches '
                                'during BootstrapStage.'))
  group.add_remote_option('--nopaygen', action='store_false',
                          dest='paygen', default=True,
                          help="Don't generate payloads.")
  group.add_remote_option('--noreexec', action='store_false',
                          dest='postsync_reexec', default=True,
                          help="Don't reexec into the buildroot after syncing.")
  group.add_remote_option('--nosdk', action='store_true',
                          default=False,
                          help='Re-create the SDK from scratch.')
  group.add_remote_option('--nosync', action='store_false', dest='sync',
                          default=True, help="Don't sync before building.")
  group.add_remote_option('--notests', action='store_false', dest='tests',
                          default=True,
                          help=('Override values from buildconfig and run no '
                                'tests.'))
  group.add_remote_option('--noimagetests', action='store_false',
                          dest='image_test', default=True,
                          help=('Override values from buildconfig and run no '
                                'image tests.'))
  group.add_remote_option('--nouprev', action='store_false', dest='uprev',
                          default=True,
                          help=('Override values from buildconfig and never '
                                'uprev.'))
  group.add_option('--reference-repo', action='store', default=None,
                   dest='reference_repo',
                   help=('Reuse git data stored in an existing repo '
                         'checkout. This can drastically reduce the network '
                         'time spent setting up the trybot checkout.  By '
                         "default, if this option isn't given but cbuildbot "
                         'is invoked from a repo checkout, cbuildbot will '
                         'use the repo root.'))
  group.add_option('--resume', action='store_true', default=False,
                   help='Skip stages already successfully completed.')
  group.add_remote_option('--timeout', action='store', type='int', default=0,
                          help=('Specify the maximum amount of time this job '
                                'can run for, at which point the build will be '
                                'aborted.  If set to zero, then there is no '
                                'timeout.'))
  group.add_remote_option('--version', dest='force_version', default=None,
                          help=('Used with manifest logic.  Forces use of this '
                                'version rather than create or get latest. '
                                'Examples: 4815.0.0-rc1, 4815.1.2'))

  parser.add_option_group(group)

  #
  # Internal options.
  #

  group = CustomGroup(
      parser,
      'Internal Chromium OS Build Team Options',
      'Caution: these are for meant for the Chromium OS build team only')

  group.add_remote_option('--archive-base', type='gs_path',
                          help=('Base GS URL (gs://<bucket_name>/<path>) to '
                                'upload archive artifacts to'))
  group.add_remote_option(
      '--cq-gerrit-query', dest='cq_gerrit_override', default=None,
      help=('If given, this gerrit query will be used to find what patches to '
            "test, rather than the normal 'CommitQueue>=1 AND Verified=1 AND "
            "CodeReview=2' query it defaults to.  Use with care- note "
            'additionally this setting only has an effect if the buildbot '
            "target is a cq target, and we're in buildbot mode."))
  group.add_option('--pass-through', dest='pass_through_args', action='append',
                   type='string', default=[])
  group.add_remote_option('--pre-cq', action='store_true', default=False,
                          help='Mark CLs as tested by the PreCQ on success.')
  group.add_option('--reexec-api-version', dest='output_api_version',
                   action='store_true', default=False,
                   help=('Used for handling forwards/backwards compatibility '
                         'with --resume and --bootstrap'))
  group.add_option('--remote-trybot', dest='remote_trybot',
                   action='store_true', default=False,
                   help='Indicates this is running on a remote trybot machine')
  group.add_remote_option('--remote-patches', action='extend', default=[],
                          help=('Patches uploaded by the trybot client when '
                                'run using the -p option'))
  # Note the default here needs to be hardcoded to 3; that is the last version
  # that lacked this functionality.
  group.add_option('--remote-version', default=3, type=int, action='store',
                   help=('Used for compatibility checks w/tryjobs running in '
                         'older chromite instances'))
  group.add_option('--sourceroot', type='path', default=constants.SOURCE_ROOT)
  group.add_remote_option('--test-bootstrap', action='store_true',
                          default=False,
                          help=('Causes cbuildbot to bootstrap itself twice, '
                                'in the sequence A->B->C: A(unpatched) patches '
                                'and bootstraps B; B patches and bootstraps C'))
  group.add_remote_option('--validation_pool', default=None,
                          help=('Path to a pickled validation pool. Intended '
                                'for use only with the commit queue.'))
  group.add_remote_option('--metadata_dump', default=None,
                          help=('Path to a json dumped metadata file. This '
                                'will be used as the initial metadata.'))
  group.add_remote_option('--master-build-id', default=None, type=int,
                          api=constants.REEXEC_API_MASTER_BUILD_ID,
                          help=('cidb build id of the master build to this '
                                'slave build.'))
  group.add_remote_option('--mock-tree-status', dest='mock_tree_status',
                          default=None, action='store',
                          help=('Override the tree status value that would be '
                                'returned from the the actual tree. Example '
                                'values: open, closed, throttled. When used '
                                'in conjunction with --debug, the tree status '
                                'will not be ignored as it usually is in a '
                                '--debug run.'))
  group.add_remote_option(
      '--mock-slave-status', dest='mock_slave_status', default=None,
      action='store', metavar='MOCK_SLAVE_STATUS_PICKLE_FILE',
      help=('Override the result of the _FetchSlaveStatuses method of '
            'MasterSlaveSyncCompletionStage, by specifying a file with a '
            'pickle of the result to be returned.'))

  parser.add_option_group(group)

  #
  # Debug options
  #
  # Temporary hack; in place till --dry-run replaces --debug.
  # pylint: disable=W0212
  group = parser.debug_group
  debug = [x for x in group.option_list if x._long_opts == ['--debug']][0]
  debug.help += '  Currently functions as --dry-run in addition.'
  debug.pass_through = True
  group.add_option('--notee', action='store_false', dest='tee', default=True,
                   help=('Disable logging and internal tee process.  Primarily '
                         'used for debugging cbuildbot itself.'))
  return parser


def _FinishParsing(options, args):
  """Perform some parsing tasks that need to take place after optparse.

  This function needs to be easily testable!  Keep it free of
  environment-dependent code.  Put more detailed usage validation in
  _PostParseCheck().

  Args:
    options: The options object returned by optparse
    args: The args object returned by optparse
  """
  # Populate options.pass_through_args.
  accepted, _ = commandline.FilteringParser.FilterArgs(
      options.parsed_args, lambda x: x.opt_inst.pass_through)
  options.pass_through_args.extend(accepted)

  if options.chrome_root:
    if options.chrome_rev != constants.CHROME_REV_LOCAL:
      cros_build_lib.Die('Chrome rev must be %s if chrome_root is set.' %
                         constants.CHROME_REV_LOCAL)
  elif options.chrome_rev == constants.CHROME_REV_LOCAL:
    cros_build_lib.Die('Chrome root must be set if chrome_rev is %s.' %
                       constants.CHROME_REV_LOCAL)

  if options.chrome_version:
    if options.chrome_rev != constants.CHROME_REV_SPEC:
      cros_build_lib.Die('Chrome rev must be %s if chrome_version is set.' %
                         constants.CHROME_REV_SPEC)
  elif options.chrome_rev == constants.CHROME_REV_SPEC:
    cros_build_lib.Die(
        'Chrome rev must not be %s if chrome_version is not set.'
        % constants.CHROME_REV_SPEC)

  patches = bool(options.gerrit_patches or options.local_patches or
                 options.rietveld_patches)
  if options.remote:
    if options.local:
      cros_build_lib.Die('Cannot specify both --remote and --local')

    # options.channels is a convenient way to detect payloads builds.
    if not options.buildbot and not options.channels and not patches:
      prompt = ('No patches were provided; are you sure you want to just '
                'run a remote build of %s?' % (
                    options.branch if options.branch else 'ToT'))
      if not cros_build_lib.BooleanPrompt(prompt=prompt, default=False):
        cros_build_lib.Die('Must provide patches when running with --remote.')

    # --debug needs to be explicitly passed through for remote invocations.
    release_mode_with_patches = (options.buildbot and patches and
                                 '--debug' not in options.pass_through_args)
  else:
    if len(args) > 1:
      cros_build_lib.Die('Multiple configs not supported if not running with '
                         '--remote.  Got %r', args)

    if options.slaves:
      cros_build_lib.Die('Cannot use --slaves if not running with --remote.')

    release_mode_with_patches = (options.buildbot and patches and
                                 not options.debug)

  # When running in release mode, make sure we are running with checked-in code.
  # We want checked-in cbuildbot/scripts to prevent errors, and we want to build
  # a release image with checked-in code for CrOS packages.
  if release_mode_with_patches:
    cros_build_lib.Die(
        'Cannot provide patches when running with --buildbot!')

  if options.buildbot and options.remote_trybot:
    cros_build_lib.Die(
        '--buildbot and --remote-trybot cannot be used together.')

  # Record whether --debug was set explicitly vs. it was inferred.
  options.debug_forced = False
  if options.debug:
    options.debug_forced = True
  if not options.debug:
    # We don't set debug by default for
    # 1. --buildbot invocations.
    # 2. --remote invocations, because it needs to push changes to the tryjob
    #    repo.
    options.debug = not options.buildbot and not options.remote

  # Record the configs targeted.
  options.build_targets = args[:]

  if constants.BRANCH_UTIL_CONFIG in options.build_targets:
    if options.remote:
      cros_build_lib.Die(
          'Running %s as a remote tryjob is not yet supported.',
          constants.BRANCH_UTIL_CONFIG)
    if len(options.build_targets) > 1:
      cros_build_lib.Die(
          'Cannot run %s with any other configs.',
          constants.BRANCH_UTIL_CONFIG)
    if not options.branch_name:
      cros_build_lib.Die(
          'Must specify --branch-name with the %s config.',
          constants.BRANCH_UTIL_CONFIG)
    if options.branch and options.branch != options.branch_name:
      cros_build_lib.Die(
          'If --branch is specified with the %s config, it must'
          ' have the same value as --branch-name.',
          constants.BRANCH_UTIL_CONFIG)

    exclusive_opts = {'--version': options.force_version,
                      '--delete-branch': options.delete_branch,
                      '--rename-to': options.rename_to}
    if 1 != sum(1 for x in exclusive_opts.values() if x):
      cros_build_lib.Die('When using the %s config, you must'
                         ' specifiy one and only one of the following'
                         ' options: %s.', constants.BRANCH_UTIL_CONFIG,
                         ', '.join(exclusive_opts.keys()))

    # When deleting or renaming a branch, the --branch and --nobootstrap
    # options are implied.
    if options.delete_branch or options.rename_to:
      if not options.branch:
        cros_build_lib.Info('Automatically enabling sync to branch %s'
                            ' for this %s flow.', options.branch_name,
                            constants.BRANCH_UTIL_CONFIG)
        options.branch = options.branch_name
      if options.bootstrap:
        cros_build_lib.Info('Automatically disabling bootstrap step for'
                            ' this %s flow.', constants.BRANCH_UTIL_CONFIG)
        options.bootstrap = False

  elif any([options.delete_branch, options.rename_to, options.branch_name]):
    cros_build_lib.Die(
        'Cannot specify --delete-branch, --rename-to or --branch-name when not '
        'running the %s config', constants.BRANCH_UTIL_CONFIG)


# pylint: disable=W0613
def _PostParseCheck(parser, options, args):
  """Perform some usage validation after we've parsed the arguments

  Args:
    parser: Option parser that was used to parse arguments.
    options: The options returned by optparse.
    args: The args returned by optparse.
  """
  if not options.branch:
    options.branch = git.GetChromiteTrackingBranch()

  if not repository.IsARepoRoot(options.sourceroot):
    if options.local_patches:
      raise Exception('Could not find repo checkout at %s!'
                      % options.sourceroot)

  # Because the default cache dir depends on other options, FindCacheDir
  # always returns None, and we setup the default here.
  if options.cache_dir is None:
    # Note, options.sourceroot is set regardless of the path
    # actually existing.
    if options.buildroot is not None:
      options.cache_dir = os.path.join(options.buildroot, '.cache')
    elif os.path.exists(options.sourceroot):
      options.cache_dir = os.path.join(options.sourceroot, '.cache')
    else:
      options.cache_dir = parser.FindCacheDir(parser, options)
    options.cache_dir = os.path.abspath(options.cache_dir)
    parser.ConfigureCacheDir(options.cache_dir)

  osutils.SafeMakedirsNonRoot(options.cache_dir)

  if options.local_patches:
    options.local_patches = _CheckLocalPatches(
        options.sourceroot, options.local_patches)

  default = os.environ.get('CBUILDBOT_DEFAULT_MODE')
  if (default and not any([options.local, options.buildbot,
                           options.remote, options.remote_trybot])):
    cros_build_lib.Info('CBUILDBOT_DEFAULT_MODE=%s env var detected, using it.'
                        % default)
    default = default.lower()
    if default == 'local':
      options.local = True
    elif default == 'remote':
      options.remote = True
    elif default == 'buildbot':
      options.buildbot = True
    else:
      cros_build_lib.Die("CBUILDBOT_DEFAULT_MODE value %s isn't supported. "
                         % default)

  # Ensure that all args are legitimate config targets.
  invalid_targets = []
  for arg in args:
    build_config = _GetConfig(arg)

    if not build_config:
      invalid_targets.append(arg)
      cros_build_lib.Error('No such configuraton target: "%s".', arg)
      continue

    is_payloads_build = build_config.build_type == constants.PAYLOADS_TYPE

    if options.channels and not is_payloads_build:
      cros_build_lib.Die('--channel must only be used with a payload config,'
                         ' not target (%s).' % arg)

    if not options.channels and is_payloads_build:
      cros_build_lib.Die('payload configs (%s) require --channel to do anything'
                         ' useful.' % arg)

    # The --version option is not compatible with an external target unless the
    # --buildbot option is specified.  More correctly, only "paladin versions"
    # will work with external targets, and those are only used with --buildbot.
    # If --buildbot is specified, then user should know what they are doing and
    # only specify a version that will work.  See crbug.com/311648.
    if (options.force_version and
        not (options.buildbot or build_config.internal)):
      cros_build_lib.Die('Cannot specify --version without --buildbot for an'
                         ' external target (%s).' % arg)

  if invalid_targets:
    cros_build_lib.Die('One or more invalid configuration targets specified. '
                       'You can check the available configs by running '
                       '`cbuildbot --list --all`')


def _ParseCommandLine(parser, argv):
  """Completely parse the commandline arguments"""
  (options, args) = parser.parse_args(argv)

  # Strip out null arguments.
  # TODO(rcui): Remove when buildbot is fixed
  args = [arg for arg in args if arg]

  # A couple options, like --list, trigger a quick exit.
  if options.output_api_version:
    print(constants.REEXEC_API_VERSION)
    sys.exit(0)

  if options.list:
    if args:
      cros_build_lib.Die('No arguments expected with the --list options.')
    _PrintValidConfigs(options.print_all)
    sys.exit(0)

  if not args:
    parser.error('Invalid usage: no configuration targets provided.'
                 'Use -h to see usage.  Use -l to list supported configs.')

  _FinishParsing(options, args)
  return options, args


def _SetupCidb(options, build_config):
  """Set up CIDB the appropriate Setup call.

  Args:
    options: Command line options structure.
    build_config: Config object for this build.
  """
  # TODO(akeshet): This is a temporary workaround to make sure that the cidb
  # is not used on waterfalls that the db schema does not support (in particular
  # the chromeos.chrome waterfall).
  # See crbug.com/406940
  waterfall = os.environ.get('BUILDBOT_MASTERNAME', '')
  if not waterfall in constants.CIDB_KNOWN_WATERFALLS:
    cidb.CIDBConnectionFactory.SetupNoCidb()
    return

  # TODO(akeshet): Clean up this code once we have better defined flags to
  # specify on-or-off waterfall and on-or-off production runs of cbuildbot.
  # See crbug.com/331417

  # --buildbot runs should use the production database, unless the --debug flag
  # is also present in which case they should use the debug database.
  if options.buildbot:
    if options.debug:
      cidb.CIDBConnectionFactory.SetupDebugCidb()
      return
    else:
      cidb.CIDBConnectionFactory.SetupProdCidb()
      return

  # --remote-trybot runs should use the debug database. With the exception of
  # pre-cq builds, which should use the production database.
  if options.remote_trybot:
    if build_config['pre_cq']:
      cidb.CIDBConnectionFactory.SetupProdCidb()
      return
    else:
      cidb.CIDBConnectionFactory.SetupDebugCidb()
      return

  # If neither --buildbot nor --remote-trybot flag was used, don't use the
  # database.
  cidb.CIDBConnectionFactory.SetupNoCidb()


# TODO(build): This function is too damn long.
def main(argv):
  # Turn on strict sudo checks.
  cros_build_lib.STRICT_SUDO = True

  # Set umask to 022 so files created by buildbot are readable.
  os.umask(0o22)

  parser = _CreateParser()
  (options, args) = _ParseCommandLine(parser, argv)

  _PostParseCheck(parser, options, args)

  cros_build_lib.AssertOutsideChroot()

  if options.remote:
    cros_build_lib.logger.setLevel(logging.WARNING)

    # Verify configs are valid.
    # If hwtest flag is enabled, post a warning that HWTest step may fail if the
    # specified board is not a released platform or it is a generic overlay.
    for bot in args:
      build_config = _GetConfig(bot)
      if options.hwtest:
        cros_build_lib.Warning(
            'If %s is not a released platform or it is a generic overlay, '
            'the HWTest step will most likely not run; please ask the lab '
            'team for help if this is unexpected.' % build_config['boards'])

    # Verify gerrit patches are valid.
    print('Verifying patches...')
    patch_pool = trybot_patch_pool.TrybotPatchPool.FromOptions(
        gerrit_patches=options.gerrit_patches,
        local_patches=options.local_patches,
        sourceroot=options.sourceroot,
        remote_patches=options.remote_patches)

    # --debug need to be explicitly passed through for remote invocations.
    if options.buildbot and '--debug' not in options.pass_through_args:
      _ConfirmRemoteBuildbotRun()

    print('Submitting tryjob...')
    tryjob = remote_try.RemoteTryJob(options, args, patch_pool.local_patches)
    tryjob.Submit(testjob=options.test_tryjob, dryrun=False)
    print('Tryjob submitted!')
    print(('Go to %s to view the status of your job.'
           % tryjob.GetTrybotWaterfallLink()))
    sys.exit(0)

  elif (not options.buildbot and not options.remote_trybot
        and not options.resume and not options.local):
    cros_build_lib.Die('Please use --remote or --local to run trybots')

  # Only one config arg is allowed in this mode, which was confirmed earlier.
  bot_id = args[-1]
  build_config = _GetConfig(bot_id)

  # TODO: Re-enable this block when reference_repo support handles this
  #       properly. (see chromium:330775)
  # if options.reference_repo is None:
  #   repo_path = os.path.join(options.sourceroot, '.repo')
  #   # If we're being run from a repo checkout, reuse the repo's git pool to
  #   # cut down on sync time.
  #   if os.path.exists(repo_path):
  #     options.reference_repo = options.sourceroot

  if options.reference_repo:
    if not os.path.exists(options.reference_repo):
      parser.error('Reference path %s does not exist'
                   % (options.reference_repo,))
    elif not os.path.exists(os.path.join(options.reference_repo, '.repo')):
      parser.error('Reference path %s does not look to be the base of a '
                   'repo checkout; no .repo exists in the root.'
                   % (options.reference_repo,))

  if (options.buildbot or options.remote_trybot) and not options.resume:
    if not options.cgroups:
      parser.error('Options --buildbot/--remote-trybot and --nocgroups cannot '
                   'be used together.  Cgroup support is required for '
                   'buildbot/remote-trybot mode.')
    if not cgroups.Cgroup.IsSupported():
      parser.error('Option --buildbot/--remote-trybot was given, but this '
                   'system does not support cgroups.  Failing.')

    missing = osutils.FindMissingBinaries(_BUILDBOT_REQUIRED_BINARIES)
    if missing:
      parser.error('Option --buildbot/--remote-trybot requires the following '
                   "binaries which couldn't be found in $PATH: %s"
                   % (', '.join(missing)))

  if options.reference_repo:
    options.reference_repo = os.path.abspath(options.reference_repo)

  if not options.buildroot:
    if options.buildbot:
      parser.error('Please specify a buildroot with the --buildroot option.')

    options.buildroot = _DetermineDefaultBuildRoot(options.sourceroot,
                                                   build_config['internal'])
    # We use a marker file in the buildroot to indicate the user has
    # consented to using this directory.
    if not os.path.exists(repository.GetTrybotMarkerPath(options.buildroot)):
      _ConfirmBuildRoot(options.buildroot)

  # Sanity check of buildroot- specifically that it's not pointing into the
  # midst of an existing repo since git-repo doesn't support nesting.
  if (not repository.IsARepoRoot(options.buildroot) and
      git.FindRepoDir(options.buildroot)):
    parser.error('Configured buildroot %s points into a repository checkout, '
                 'rather than the root of it.  This is not supported.'
                 % options.buildroot)

  if not options.log_dir:
    options.log_dir = os.path.join(options.buildroot, _DEFAULT_LOG_DIR)

  log_file = None
  if options.tee:
    log_file = os.path.join(options.log_dir, _BUILDBOT_LOG_FILE)
    osutils.SafeMakedirs(options.log_dir)
    _BackupPreviousLog(log_file)

  with cros_build_lib.ContextManagerStack() as stack:
    # TODO(ferringb): update this once
    # https://chromium-review.googlesource.com/25359
    # is landed- it's sensitive to the manifest-versions cache path.
    options.preserve_paths = set(['manifest-versions', '.cache',
                                  'manifest-versions-internal',
                                  'chromite-bootstrap'])
    if log_file is not None:
      # We don't want the critical section to try to clean up the tee process,
      # so we run Tee (forked off) outside of it. This prevents a deadlock
      # because the Tee process only exits when its pipe is closed, and the
      # critical section accidentally holds on to that file handle.
      stack.Add(tee.Tee, log_file)
      options.preserve_paths.add(_DEFAULT_LOG_DIR)

    critical_section = stack.Add(cleanup.EnforcedCleanupSection)
    stack.Add(sudo.SudoKeepAlive)

    if not options.resume:
      # If we're in resume mode, use our parents tempdir rather than
      # nesting another layer.
      stack.Add(osutils.TempDir, prefix='cbuildbot-tmp', set_global=True)
      logging.debug('Cbuildbot tempdir is %r.', os.environ.get('TMP'))

    if options.cgroups:
      stack.Add(cgroups.SimpleContainChildren, 'cbuildbot')

    # Mark everything between EnforcedCleanupSection and here as having to
    # be rolled back via the contextmanager cleanup handlers.  This
    # ensures that sudo bits cannot outlive cbuildbot, that anything
    # cgroups would kill gets killed, etc.
    stack.Add(critical_section.ForkWatchdog)

    if not options.buildbot:
      build_config = cbuildbot_config.OverrideConfigForTrybot(
          build_config, options)

    if options.mock_tree_status is not None:
      stack.Add(mock.patch.object, tree_status, '_GetStatus',
                return_value=options.mock_tree_status)

    if options.mock_slave_status is not None:
      with open(options.mock_slave_status, 'r') as f:
        mock_statuses = pickle.load(f)
        for key, value in mock_statuses.iteritems():
          mock_statuses[key] = manifest_version.BuilderStatus(**value)
      stack.Add(mock.patch.object,
                completion_stages.MasterSlaveSyncCompletionStage,
                '_FetchSlaveStatuses',
                return_value=mock_statuses)

    _SetupCidb(options, build_config)
    retry_stats.SetupStats()

    # For master-slave builds: Update slave's timeout using master's published
    # deadline.
    if options.buildbot and options.master_build_id is not None:
      slave_timeout = None
      if cidb.CIDBConnectionFactory.IsCIDBSetup():
        cidb_handle = cidb.CIDBConnectionFactory.GetCIDBConnectionForBuilder()
        if cidb_handle:
          slave_timeout = cidb_handle.GetTimeToDeadline(options.master_build_id)

      if slave_timeout is not None:
        # Cut me some slack. We artificially add a a small time here to the
        # slave_timeout because '0' is handled specially, and because we don't
        # want to timeout while trying to set things up.
        slave_timeout = slave_timeout + 20
        if options.timeout == 0 or slave_timeout < options.timeout:
          logging.info('Updating slave build timeout to %d seconds enforced '
                       'by the master',
                       slave_timeout)
          options.timeout = slave_timeout
      else:
        logging.warning('Could not get master deadline for master-slave build. '
                        'Can not set slave timeout.')

    if options.timeout > 0:
      stack.Add(timeout_util.FatalTimeout, options.timeout)

    _RunBuildStagesWrapper(options, build_config)
