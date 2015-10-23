# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configuration options for various cbuildbot builders."""

from __future__ import print_function

import copy
import itertools
import json

from chromite.cbuildbot import constants
from chromite.lib import osutils


GS_PATH_DEFAULT = 'default' # Means gs://chromeos-image-archive/ + bot_id

# Contains the valid build config suffixes in the order that they are dumped.
CONFIG_TYPE_PRECQ = 'pre-cq'
CONFIG_TYPE_PALADIN = 'paladin'
CONFIG_TYPE_RELEASE = 'release'
CONFIG_TYPE_FULL = 'full'
CONFIG_TYPE_FIRMWARE = 'firmware'
CONFIG_TYPE_FACTORY = 'factory'
CONFIG_TYPE_RELEASE_AFDO = 'release-afdo'

# This is only used for unitests... find a better solution?
CONFIG_TYPE_DUMP_ORDER = (
    CONFIG_TYPE_PALADIN,
    CONFIG_TYPE_PRECQ,
    constants.PRE_CQ_LAUNCHER_CONFIG,
    'incremental',
    'telemetry',
    CONFIG_TYPE_FULL,
    'full-group',
    CONFIG_TYPE_RELEASE,
    'release-group',
    'release-afdo',
    'release-afdo-generate',
    'release-afdo-use',
    'sdk',
    'chromium-pfq',
    'chromium-pfq-informational',
    'chrome-perf',
    'chrome-pfq',
    'chrome-pfq-informational',
    'pre-flight-branch',
    CONFIG_TYPE_FACTORY,
    CONFIG_TYPE_FIRMWARE,
    'toolchain-major',
    'toolchain-minor',
    'llvm',
    'asan',
    'asan-informational',
    'refresh-packages',
    'test-ap',
    'test-ap-group',
    constants.BRANCH_UTIL_CONFIG,
    constants.PAYLOADS_TYPE,
    'cbuildbot',
)


# In the Json, this special build config holds the default values for all
# other configs.
DEFAULT_BUILD_CONFIG = '_default'


def IsPFQType(b_type):
  """Returns True if this build type is a PFQ."""
  return b_type in (constants.PFQ_TYPE, constants.PALADIN_TYPE,
                    constants.CHROME_PFQ_TYPE)


def IsCQType(b_type):
  """Returns True if this build type is a Commit Queue."""
  return b_type == constants.PALADIN_TYPE


def IsCanaryType(b_type):
  """Returns True if this build type is a Canary."""
  return b_type == constants.CANARY_TYPE


def OverrideConfigForTrybot(build_config, options):
  """Apply trybot-specific configuration settings.

  Args:
    build_config: The build configuration dictionary to override.
      The dictionary is not modified.
    options: The options passed on the commandline.

  Returns:
    A build configuration dictionary with the overrides applied.
  """
  # TODO: crbug.com/504653 is about deleting this method fully.

  copy_config = copy.deepcopy(build_config)
  for my_config in [copy_config] + copy_config['child_configs']:
    # Force uprev. This is so patched in changes are always built.
    my_config['uprev'] = True
    if my_config['internal']:
      my_config['overlays'] = constants.BOTH_OVERLAYS

    # Use the local manifest which only requires elevated access if it's really
    # needed to build.
    if not options.remote_trybot:
      my_config['manifest'] = my_config['dev_manifest']

    my_config['push_image'] = False

    if my_config['build_type'] != constants.PAYLOADS_TYPE:
      my_config['paygen'] = False

    if options.hwtest and my_config['hw_tests_override'] is not None:
      my_config['hw_tests'] = my_config['hw_tests_override']

    # Default to starting with a fresh chroot on remote trybot runs.
    if options.remote_trybot:
      my_config['chroot_replace'] = True

    # In trybots, we want to always run VM tests and all unit tests, so that
    # developers will get better testing for their changes.
    if my_config['vm_tests_override'] is not None:
      my_config['vm_tests'] = my_config['vm_tests_override']

  return copy_config


class BuildConfig(dict):
  """Dictionary of explicit configuration settings for a cbuildbot config

  Each dictionary entry is in turn a dictionary of config_param->value.

  See _settings for details on known configurations, and their documentation.
  """

  _delete_key_sentinel = object()

  @classmethod
  def delete_key(cls):
    """Used to remove the given key from inherited config.

    Usage:
      new_config = base_config.derive(foo=delete_key())
    """
    return cls._delete_key_sentinel

  @classmethod
  def delete_keys(cls, keys):
    """Used to remove a set of keys from inherited config.

    Usage:
      new_config = base_config.derive(delete_keys(set_of_keys))
    """
    return {k: cls._delete_key_sentinel for k in keys}

  def __getattr__(self, name):
    """Support attribute-like access to each dict entry."""
    if name in self:
      return self[name]

    # Super class (dict) has no __getattr__ method, so use __getattribute__.
    return super(BuildConfig, self).__getattribute__(name)

  def GetBotId(self, remote_trybot=False):
    """Get the 'bot id' of a particular bot.

    The bot id is used to specify the subdirectory where artifacts are stored
    in Google Storage. To avoid conflicts between remote trybots and regular
    bots, we add a 'trybot-' prefix to any remote trybot runs.

    Args:
      remote_trybot: Whether this run is a remote trybot run.
    """
    return 'trybot-%s' % self.name if remote_trybot else self.name

  def deepcopy(self):
    """Create a deep copy of this object.

    This is a specialized version of copy.deepcopy() for BuildConfig objects. It
    speeds up deep copies by 10x because we know in advance what is stored
    inside a BuildConfig object and don't have to do as much introspection. This
    function is called a lot during setup of the config objects so optimizing it
    makes a big difference. (It saves seconds off the load time of this module!)
    """
    new_config = BuildConfig(self)
    for k, v in self.iteritems():
      # type(v) is faster than isinstance.
      if type(v) is list:
        new_config[k] = v[:]

    if new_config.get('child_configs'):
      new_config['child_configs'] = [
          x.deepcopy() for x in new_config['child_configs']]

    if new_config.get('hw_tests'):
      new_config['hw_tests'] = [copy.copy(x) for x in new_config['hw_tests']]

    if new_config.get('hw_tests_override'):
      new_config['hw_tests_override'] = [
          copy.copy(x) for x in new_config['hw_tests_override']
      ]

    return new_config

  def derive(self, *args, **kwargs):
    """Create a new config derived from this one.

    Note: If an override is callable, it will be called and passed the prior
    value for the given key (or None) to compute the new value.

    Args:
      args: Mapping instances to mixin.
      kwargs: Settings to inject; see _settings for valid values.

    Returns:
      A new _config instance.
    """
    inherits = list(args)
    inherits.append(kwargs)
    new_config = self.deepcopy()

    for update_config in inherits:
      for k, v in update_config.iteritems():
        if callable(v):
          new_config[k] = v(new_config.get(k))
        else:
          new_config[k] = v

      keys_to_delete = [k for k in new_config if
                        new_config[k] is self._delete_key_sentinel]

      for k in keys_to_delete:
        new_config.pop(k, None)

    return new_config


class HWTestConfig(object):
  """Config object for hardware tests suites.

  Members:
    suite: Name of the test suite to run.
    timeout: Number of seconds to wait before timing out waiting for
             results.
    pool: Pool to use for hw testing.
    blocking: Suites that set this true run sequentially; each must pass
              before the next begins.  Tests that set this false run in
              parallel after all blocking tests have passed.
    async: Fire-and-forget suite.
    warn_only: Failure on HW tests warns only (does not generate error).
    critical: Usually we consider structural failures here as OK.
    priority:  Priority at which tests in the suite will be scheduled in
               the hw lab.
    file_bugs: Should we file bugs if a test fails in a suite run.
    num: Maximum number of DUTs to use when scheduling tests in the hw lab.
    minimum_duts: minimum number of DUTs required for testing in the hw lab.
    retry: Whether we should retry tests that fail in a suite run.
    max_retries: Integer, maximum job retries allowed at suite level.
                 None for no max.
    suite_min_duts: Preferred minimum duts. Lab will prioritize on getting such
                    number of duts even if the suite is competing with
                    other suites that have higher priority.

  Some combinations of member settings are invalid:
    * A suite config may not specify both blocking and async.
    * A suite config may not specify both retry and async.
    * A suite config may not specify both warn_only and critical.
  """
  # This timeout is larger than it needs to be because of autotest overhead.
  # TODO(davidjames): Reduce this timeout once http://crbug.com/366141 is fixed.
  DEFAULT_HW_TEST_TIMEOUT = 60 * 220
  BRANCHED_HW_TEST_TIMEOUT = 10 * 60 * 60

  def __init__(self, suite, num=constants.HWTEST_DEFAULT_NUM,
               pool=constants.HWTEST_MACH_POOL, timeout=DEFAULT_HW_TEST_TIMEOUT,
               async=False, warn_only=False, critical=False, blocking=False,
               file_bugs=False, priority=constants.HWTEST_BUILD_PRIORITY,
               retry=True, max_retries=10, minimum_duts=0, suite_min_duts=0,
               offload_failures_only=False):
    """Constructor -- see members above."""
    assert not async or (not blocking and not retry)
    assert not warn_only or not critical
    self.suite = suite
    self.num = num
    self.pool = pool
    self.timeout = timeout
    self.blocking = blocking
    self.async = async
    self.warn_only = warn_only
    self.critical = critical
    self.file_bugs = file_bugs
    self.priority = priority
    self.retry = retry
    self.max_retries = max_retries
    self.minimum_duts = minimum_duts
    self.suite_min_duts = suite_min_duts
    self.offload_failures_only = offload_failures_only

  def SetBranchedValues(self):
    """Changes the HW Test timeout/priority values to branched values."""
    self.timeout = max(HWTestConfig.BRANCHED_HW_TEST_TIMEOUT, self.timeout)

    # Set minimum_duts default to 0, which means that lab will not check the
    # number of available duts to meet the minimum requirement before creating
    # a suite job for branched build.
    self.minimum_duts = 0

    # Only reduce priority if it's lower.
    new_priority = constants.HWTEST_DEFAULT_PRIORITY
    if (constants.HWTEST_PRIORITIES_MAP[self.priority] >
        constants.HWTEST_PRIORITIES_MAP[new_priority]):
      self.priority = new_priority

  @property
  def timeout_mins(self):
    return int(self.timeout / 60)

  def __eq__(self, other):
    return self.__dict__ == other.__dict__

def DefaultSettings():
  # Enumeration of valid settings; any/all config settings must be in this.
  # All settings must be documented.
  return dict(
      # The name of the template we inherit settings from.
      _template=None,

      # The name of the config.
      name=None,

      # A list of boards to build.
      boards=None,

      # The profile of the variant to set up and build.
      profile=None,

      # This bot pushes changes to the overlays.
      master=False,

      # If False, this flag indicates that the CQ should not check whether
      # this bot passed or failed. Set this to False if you are setting up a
      # new bot. Once the bot is on the waterfall and is consistently green,
      # mark the builder as important=True.
      important=False,

      # An integer. If this builder fails this many times consecutively, send
      # an alert email to the recipients health_alert_recipients. This does
      # not apply to tryjobs. This feature is similar to the ERROR_WATERMARK
      # feature of upload_symbols, and it may make sense to merge the features
      # at some point.
      health_threshold=0,

      # List of email addresses to send health alerts to for this builder. It
      # supports automatic email address lookup for the following sheriff
      # types:
      #     'tree': tree sheriffs
      #     'chrome': chrome gardeners
      health_alert_recipients=[],

      # Whether this is an internal build config.
      internal=False,

      # Whether this is a branched build config. Used for pfq logic.
      branch=False,

      # The name of the manifest to use. E.g., to use the buildtools manifest,
      # specify 'buildtools'.
      manifest=constants.DEFAULT_MANIFEST,

      # The name of the manifest to use if we're building on a local trybot.
      # This should only require elevated access if it's really needed to
      # build this config.
      dev_manifest=constants.DEFAULT_MANIFEST,

      # Applies only to paladin builders. If true, Sync to the manifest
      # without applying any test patches, then do a fresh build in a new
      # chroot. Then, apply the patches and build in the existing chroot.
      build_before_patching=False,

      # Applies only to paladin builders. If True, Sync to the master manifest
      # without applying any of the test patches, rather than running
      # CommitQueueSync. This is basically ToT immediately prior to the
      # current commit queue run.
      do_not_apply_cq_patches=False,

      # Applies only to master builders. List of the names of slave builders
      # to be treated as sanity checkers. If only sanity check builders fail,
      # then the master will ignore the failures. In a CQ run, if any of the
      # sanity check builders fail and other builders fail as well, the master
      # will treat the build as failed, but will not reset the ready bit of
      # the tested patches.
      sanity_check_slaves=None,

      # emerge use flags to use while setting up the board, building packages,
      # making images, etc.
      useflags=[],

      # Set the variable CHROMEOS_OFFICIAL for the build. Known to affect
      # parallel_emerge, cros_set_lsb_release, and chromeos_version.sh. See
      # bug chromium-os:14649
      chromeos_official=False,

      # Use binary packages for building the toolchain. (emerge --getbinpkg)
      usepkg_toolchain=True,

      # Use binary packages for build_packages and setup_board.
      usepkg_build_packages=True,

      # If set, run BuildPackages in the background and allow subsequent
      # stages to run in parallel with this one.
      #
      # For each release group, the first builder should be set to run in the
      # foreground (to build binary packages), and the remainder of the
      # builders should be set to run in parallel (to install the binary
      # packages.)
      build_packages_in_background=False,

      # Only use binaries in build_packages for Chrome itself.
      chrome_binhost_only=False,

      # Does this profile need to sync chrome?  If None, we guess based on
      # other factors.  If True/False, we always do that.
      sync_chrome=None,

      # Use the newest ebuilds for all the toolchain packages.
      latest_toolchain=False,

      # This is only valid when latest_toolchain is True. If you set this to a
      # commit-ish, the gcc ebuild will use it to build the toolchain
      # compiler.
      gcc_githash=None,

      # Wipe and replace the board inside the chroot.
      board_replace=False,

      # Wipe and replace chroot, but not source.
      chroot_replace=True,

      # Uprevs the local ebuilds to build new changes since last stable.
      # build.  If master then also pushes these changes on success. Note that
      # we uprev on just about every bot config because it gives us a more
      # deterministic build system (the tradeoff being that some bots build
      # from source more frequently than if they never did an uprev). This way
      # the release/factory/etc... builders will pick up changes that devs
      # pushed before it runs, but after the correspoding PFQ bot ran (which
      # is what creates+uploads binpkgs).  The incremental bots are about the
      # only ones that don't uprev because they mimic the flow a developer
      # goes through on their own local systems.
      uprev=True,

      # Select what overlays to look at for revving and prebuilts. This can be
      # any constants.VALID_OVERLAYS.
      overlays=constants.PUBLIC_OVERLAYS,

      # Select what overlays to push at. This should be a subset of overlays
      # for the particular builder.  Must be None if not a master.  There
      # should only be one master bot pushing changes to each overlay per
      # branch.
      push_overlays=None,

      # Uprev Chrome, values of 'tot', 'stable_release', or None.
      chrome_rev=None,

      # Exit the builder right after checking compilation.
      # TODO(mtennant): Should be something like "compile_check_only".
      compilecheck=False,

      # Test CLs to verify they're ready for the commit queue.
      pre_cq=False,

      # Runs the tests that the signer would run. This should only be set if
      # 'recovery' is in images.
      signer_tests=False,

      # Runs unittests for packages.
      unittests=True,

      # A list of the packages to blacklist from unittests.
      unittest_blacklist=[],

      # Builds autotest tests.  Must be True if vm_tests is set.
      build_tests=True,

      # Generates AFDO data. Will capture a profile of chrome using a hwtest
      # to run a predetermined set of benchmarks.
      afdo_generate=False,

      # Generates AFDO data, builds the minimum amount of artifacts and
      # assumes a non-distributed builder (i.e.: the whole process in a single
      # builder).
      afdo_generate_min=False,

      # Update the Chrome ebuild with the AFDO profile info.
      afdo_update_ebuild=False,

      # Uses AFDO data. The Chrome build will be optimized using the AFDO
      # profile information found in the chrome ebuild file.
      afdo_use=False,

      # A list of the vm_tests to run by default.
      vm_tests=[constants.SMOKE_SUITE_TEST_TYPE,
                constants.SIMPLE_AU_TEST_TYPE],

      # A list of all VM Tests to use if VM Tests are forced on (--vmtest
      # command line or trybot). None means no override.
      vm_tests_override=None,

      # The number of times to run the VMTest stage. If this is >1, then we
      # will run the stage this many times, stopping if we encounter any
      # failures.
      vm_test_runs=1,

      # A list of HWTestConfig objects to run.
      hw_tests=[],

      # A list of all HW Tests to use if HW Tests are forced on (--hwtest
      # command line or trybot). None means no override.
      hw_tests_override=None,

      # If true, uploads artifacts for hw testing. Upload payloads for test
      # image if the image is built. If not, dev image is used and then base
      # image.
      upload_hw_test_artifacts=True,

      # If true, uploads individual image tarballs.
      upload_standalone_images=True,

      # upload_gce_images -- If true, uploads tarballs that can be used as the
      #                      basis for GCE images.
      upload_gce_images=False,

      # Google Storage path to offload files to.
      #   None - No upload
      #   GS_PATH_DEFAULT - 'gs://chromeos-image-archive/' + bot_id
      #   value - Upload to explicit path
      gs_path=GS_PATH_DEFAULT,

      # TODO(sosa): Deprecate binary.
      # Type of builder.  Check constants.VALID_BUILD_TYPES.
      build_type=constants.PFQ_TYPE,

      # The class name used to build this config.  See the modules in
      # cbuildbot / builders/*_builders.py for possible values.  This should
      # be the name in string form -- e.g. "simple_builders.SimpleBuilder" to
      # get the SimpleBuilder class in the simple_builders module.  If not
      # specified, we'll fallback to legacy probing behavior until everyone
      # has been converted (see the scripts/cbuildbot.py file for details).
      builder_class_name=None,

      # List of images we want to build -- see build_image for more details.
      images=['test'],

      # Image from which we will build update payloads.  Must either be None
      # or name one of the images in the 'images' list, above.
      payload_image=None,

      # Whether to build a netboot image.
      factory_install_netboot=True,

      # Whether to build the factory toolkit.
      factory_toolkit=True,

      # Whether to build factory packages in BuildPackages.
      factory=True,

      # Tuple of specific packages we want to build.  Most configs won't
      # specify anything here and instead let build_packages calculate.
      packages=[],

      # Do we push a final release image to chromeos-images.
      push_image=False,

      # Do we upload debug symbols.
      upload_symbols=False,

      # Whether we upload a hwqual tarball.
      hwqual=False,

      # Run a stage that generates release payloads for signed images.
      paygen=False,

      # If the paygen stage runs, generate tests, and schedule auto-tests for
      # them.
      paygen_skip_testing=False,

      # If the paygen stage runs, don't generate any delta payloads. This is
      # only done if deltas are broken for a given board.
      paygen_skip_delta_payloads=False,

      # Run a stage that generates and uploads package CPE information.
      cpe_export=True,

      # Run a stage that generates and uploads debug symbols.
      debug_symbols=True,

      # Do not package the debug symbols in the binary package. The debug
      # symbols will be in an archive with the name cpv.debug.tbz2 in
      # /build/${BOARD}/packages and uploaded with the prebuilt.
      separate_debug_symbols=True,

      # Include *.debug files for debugging core files with gdb in debug.tgz.
      # These are very large. This option only has an effect if debug_symbols
      # and archive are set.
      archive_build_debug=False,

      # Run a stage that archives build and test artifacts for developer
      # consumption.
      archive=True,

      # git repository URL for our manifests.
      #  https://chromium.googlesource.com/chromiumos/manifest
      #  https://chrome-internal.googlesource.com/chromeos/manifest-internal
      manifest_repo_url=constants.MANIFEST_URL,

      # Whether we are using the manifest_version repo that stores per-build
      # manifests.
      manifest_version=False,

      # Use the Last Known Good Manifest blessed by Paladin.
      use_lkgm=False,

      # If we use_lkgm -- What is the name of the manifest to look for?
      lkgm_manifest=constants.LKGM_MANIFEST,

      # LKGM for Chrome OS generated for Chrome builds that are blessed from
      # canary runs.
      use_chrome_lkgm=False,

      # True if this build config is critical for the chrome_lkgm decision.
      critical_for_chrome=False,

      # Upload prebuilts for this build. Valid values are PUBLIC, PRIVATE, or
      # False.
      prebuilts=False,

      # Use SDK as opposed to building the chroot from source.
      use_sdk=True,

      # List this config when user runs cbuildbot with --list option without
      # the --all flag.
      trybot_list=False,

      # The description string to print out for config when user runs --list.
      description=None,

      # Boolean that enables parameter --git-sync for upload_prebuilts.
      git_sync=False,

      # A list of the child config groups, if applicable. See the AddGroup
      # method.
      child_configs=[],

      # Set shared user password for "chronos" user in built images. Use
      # "None" (default) to remove the shared user password. Note that test
      # images will always set the password to "test0000".
      shared_user_password=None,

      # Whether this config belongs to a config group.
      grouped=False,

      # layout of build_image resulting image. See
      # scripts/build_library/legacy_disk_layout.json or
      # overlay-<board>/scripts/disk_layout.json for possible values.
      disk_layout=None,

      # If enabled, run the PatchChanges stage.  Enabled by default. Can be
      # overridden by the --nopatch flag.
      postsync_patch=True,

      # Reexec into the buildroot after syncing.  Enabled by default.
      postsync_reexec=True,

      # Create delta sysroot during ArchiveStage. Disabled by default.
      create_delta_sysroot=False,

      # Run the binhost_test stage. Only makes sense for builders that have no
      # boards.
      binhost_test=False,

      # TODO(sosa): Collapse to one option.
      # ========== Dev installer prebuilts options =======================

      # Upload prebuilts for this build to this bucket. If it equals None the
      # default buckets are used.
      binhost_bucket=None,

      # Parameter --key for upload_prebuilts. If it equals None, the default
      # values are used, which depend on the build type.
      binhost_key=None,

      # Parameter --binhost-base-url for upload_prebuilts. If it equals None,
      # the default value is used.
      binhost_base_url=None,

      # Upload dev installer prebuilts.
      dev_installer_prebuilts=False,

      # Enable rootfs verification on the image.
      rootfs_verification=True,

      # Build the Chrome SDK.
      chrome_sdk=False,

      # If chrome_sdk is set to True, this determines whether we attempt to
      # build Chrome itself with the generated SDK.
      chrome_sdk_build_chrome=True,

      # If chrome_sdk is set to True, this determines whether we use goma to
      # build chrome.
      chrome_sdk_goma=False,

      # Run image tests. This should only be set if 'base' is in our list of
      # images.
      image_test=False,

      # ==================================================================
      # The documentation associated with the config.
      doc=None,

      # ==================================================================
      # Hints to Buildbot master UI

      # If set, tells buildbot what name to give to the corresponding builder
      # on its waterfall.
      buildbot_waterfall_name=None,

      # If not None, the name (in constants.CIDB_KNOWN_WATERFALLS) of the
      # waterfall that this target should be active on.
      active_waterfall=None,
  )


class SiteParameters(dict):
  """This holds the site-wide configuration parameters for a SiteConfig."""

  def __getattr__(self, name):
    """Support attribute-like access to each SiteValue entry."""
    if name in self:
      return self[name]

    return super(SiteParameters, self).__getattribute__(name)


class SiteConfig(dict):
  """This holds a set of named BuildConfig values."""

  def __init__(self, defaults=None, templates=None, site_params=None):
    """Init.

    Args:
      defaults: Dictionary of key value pairs to use as BuildConfig values.
                All BuildConfig values should be defined here. If None,
                the DefaultSettings() is used. Most sites should use
                DefaultSettings(), and then update to add any site specific
                values needed.
      templates: Dictionary of template names to partial BuildConfigs
                 other BuildConfigs can be based on. Mostly used to reduce
                 verbosity of the config dump file format.
      site_params: Dictionary of site-wide configuration parameters. Keys
                   of the site_params dictionary should be strings.
    """
    super(SiteConfig, self).__init__()
    self._defaults = DefaultSettings() if defaults is None else defaults
    self._templates = {} if templates is None else templates
    self._site_params = {} if site_params is None else site_params

  def GetDefault(self):
    """Create the cannonical default build configuration."""
    # Enumeration of valid settings; any/all config settings must be in this.
    # All settings must be documented.
    return BuildConfig(**self._defaults)

  def GetTemplates(self):
    """Create the cannonical default build configuration."""
    return self._templates

  @property
  def params(self):
    """Create the canonical default build configuration."""
    return SiteParameters(**self._site_params)

  #
  # Methods for searching a SiteConfig's contents.
  #
  def GetBoards(self):
    """Return an iterable of all boards in the SiteConfig."""
    return set(itertools.chain.from_iterable(
        x.boards for x in self.itervalues() if x.boards))

  def FindFullConfigsForBoard(self, board=None):
    """Returns full builder configs for a board.

    Args:
      board: The board to match. By default, match all boards.

    Returns:
      A tuple containing a list of matching external configs and a list of
      matching internal release configs for a board.
    """
    ext_cfgs = []
    int_cfgs = []

    for name, c in self.iteritems():
      if c['boards'] and (board is None or board in c['boards']):
        if (name.endswith('-%s' % CONFIG_TYPE_RELEASE) and
            c['internal']):
          int_cfgs.append(c.deepcopy())
        elif (name.endswith('-%s' % CONFIG_TYPE_FULL) and
              not c['internal']):
          ext_cfgs.append(c.deepcopy())

    return ext_cfgs, int_cfgs

  def FindCanonicalConfigForBoard(self, board, allow_internal=True):
    """Get the canonical cbuildbot builder config for a board."""
    ext_cfgs, int_cfgs = self.FindFullConfigsForBoard(board)
    # If both external and internal builds exist for this board, prefer the
    # internal one unless instructed otherwise.
    both = (int_cfgs if allow_internal else []) + ext_cfgs

    if not both:
      raise ValueError('Invalid board specified: %s.' % board)
    return both[0]

  def GetSlavesForMaster(self, master_config, options=None):
    """Gets the important slave builds corresponding to this master.

    A slave config is one that matches the master config in build_type,
    chrome_rev, and branch.  It also must be marked important.  For the
    full requirements see the logic in code below.

    The master itself is eligible to be a slave (of itself) if it has boards.

    TODO(dgarrett): Replace this with explicit master/slave defitions to make
    the concept less Chrome OS specific. crbug.com/492382.

    Args:
      master_config: A build config for a master builder.
      options: The options passed on the commandline. This argument is optional,
               and only makes sense when called from cbuildbot.

    Returns:
      A list of build configs corresponding to the slaves for the master
        represented by master_config.

    Raises:
      AssertionError if the given config is not a master config or it does
        not have a manifest_version.
    """
    assert master_config['manifest_version']
    assert master_config['master']

    slave_configs = []
    if options is not None and options.remote_trybot:
      return slave_configs

    # TODO(davidjames): In CIDB the master isn't considered a slave of itself,
    # so we probably shouldn't consider it a slave here either.
    for build_config in self.itervalues():
      if (build_config['important'] and
          build_config['manifest_version'] and
          (not build_config['master'] or build_config['boards']) and
          build_config['build_type'] == master_config['build_type'] and
          build_config['chrome_rev'] == master_config['chrome_rev'] and
          build_config['branch'] == master_config['branch']):
        slave_configs.append(build_config)

    return slave_configs

  #
  # Methods used when creating a Config programatically.
  #
  def Add(self, name, *args, **kwargs):
    """Add a new BuildConfig to the SiteConfig.

    Example usage:
      # Creates default build named foo.
      site_config.Add('foo')

      # Creates default build with board 'foo_board'
      site_config.Add('foo',
                      boards=['foo_board'])

      # Creates build based on template_build for 'foo_board'.
      site_config.Add('foo',
                      template_build,
                      boards=['foo_board'])

      # Creates build based on template for 'foo_board'. with mixin.
      # Inheritance order is default, template, mixin, arguments.
      site_config.Add('foo',
                      template_build,
                      mixin_build_config,
                      boards=['foo_board'])

    Args:
      name: The name to label this configuration; this is what cbuildbot
            would see.
      args: BuildConfigs to patch into this config. First one (if present) is
            considered the template.
      kwargs: BuildConfig values to explicitly set on this config.

    Returns:
      The BuildConfig just added to the SiteConfig.
    """
    inherits, overrides = args, kwargs

    assert name not in self, '%s already exists.' % (name,)
    overrides['name'] = name

    # Remember our template, if we have one.
    if '_template' not in overrides and args and '_template' in args[0]:
      overrides['_template'] = args[0]['_template']

    if '_template' in overrides:
      assert overrides['_template'] in self.GetTemplates(), \
          '%s inherits from non-template' % (name,)

    result = self.GetDefault().derive(*inherits, **overrides)

    self[name] = result
    return result

  def AddConfig(self, config, name, *args, **kwargs):
    """Derive and add the config to cbuildbot's usable config targets

    Args:
      config: BuildConfig to derive the new config from.
      name: The name to label this configuration; this is what cbuildbot
            would see.
      args: See the docstring of derive.
      kwargs: See the docstring of derive.

    Returns:
      See the docstring of derive.
    """
    inherits, overrides = args, kwargs

    # Overrides 'name' and '_template' so that we consistently use the
    # provided names and not the names from mix-ins. E.g., If this config
    # inherits from multiple templates, we only pay attention to the first
    # one listed. TODO(davidjames): Clean up the inheritance more so that
    # this isn't needed.
    overrides['name'] = name
    overrides['_template'] = config.get('_template')
    if config:
      assert overrides['_template'], '%s inherits from non-template' % (name,)

    # Add ourselves into the global dictionary, adding in the defaults.
    new_config = config.derive(*inherits, **overrides)
    self[name] = self.GetDefault().derive(config, new_config)

    # Return a BuildConfig object without the defaults, so that other objects
    # can derive from us without inheriting the defaults.
    return new_config

  def AddConfigWithoutTemplate(self, name, *args, **kwargs):
    """Add a config containing only explicitly listed values (no defaults)."""
    return self.AddConfig(BuildConfig(), name, *args, **kwargs)

  def AddGroup(self, name, *args, **kwargs):
    """Create a new group of build configurations.

    Args:
      name: The name to label this configuration; this is what cbuildbot
            would see.
      args: Configurations to build in this group. The first config in
            the group is considered the primary configuration and is used
            for syncing and creating the chroot.
      kwargs: Override values to use for the parent config.

    Returns:
      A new BuildConfig instance.
    """
    child_configs = [self.GetDefault().derive(x, grouped=True) for x in args]
    return self.AddConfig(args[0], name, child_configs=child_configs, **kwargs)

  def SaveConfigToFile(self, config_file):
    """Save this Config to a Json file.

    Args:
      config_file: The file to write too.
    """
    json_string = self.SaveConfigToString()
    osutils.WriteFile(config_file, json_string)

  def HideDefaults(self, cfg):
    """Hide the defaults from a given config entry.

    Args:
      cfg: A config entry.

    Returns:
      The same config entry, but without any defaults.
    """
    my_default = self.GetDefault()

    template = cfg.get('_template')
    if template:
      my_default.update(self._templates[template])
      my_default['_template'] = None

    d = {}
    for k, v in cfg.iteritems():
      if my_default.get(k) != v:
        if k == 'child_configs':
          d['child_configs'] = [self.HideDefaults(child) for child in v]
        else:
          d[k] = v

    return d

  def AddTemplate(self, name, *args, **kwargs):
    """Create a template named |name|.

    Args:
      name: The name of the template.
      args: See the docstring of BuildConfig.derive.
      kwargs: See the docstring of BuildConfig.derive.
    """
    kwargs['_template'] = name

    if args:
      cfg = args[0].derive(*args[1:], **kwargs)
    else:
      cfg = BuildConfig(*args, **kwargs)

    self._templates[name] = cfg

    return cfg

  class _JSONEncoder(json.JSONEncoder):
    """Json Encoder that encodes objects as their dictionaries."""
    # pylint: disable=method-hidden
    def default(self, obj):
      return self.encode(obj.__dict__)

  def SaveConfigToString(self):
    """Save this Config object to a Json format string."""
    default = self.GetDefault()
    site_params = self.params

    config_dict = {}
    for k, v in self.iteritems():
      config_dict[k] = self.HideDefaults(v)

    config_dict['_default'] = default
    config_dict['_templates'] = self._templates
    config_dict['_site_params'] = site_params

    return json.dumps(config_dict, cls=self._JSONEncoder,
                      sort_keys=True, indent=4, separators=(',', ': '))

  def DumpExpandedConfigToString(self):
    """Dump the SiteConfig to Json with all configs full expanded.

    This is intended for debugging default/template behavior. The dumped JSON
    can't be reloaded (at least not reliably).
    """
    return json.dumps(self, cls=self._JSONEncoder,
                      sort_keys=True, indent=4, separators=(',', ': '))

#
# Methods related to loading/saving Json.
#

def LoadConfigFromFile(config_file=constants.CHROMEOS_CONFIG_FILE):
  """Load a Config a Json encoded file."""
  json_string = osutils.ReadFile(config_file)
  return LoadConfigFromString(json_string)


def LoadConfigFromString(json_string):
  """Load a cbuildbot config from it's Json encoded string."""
  config_dict = json.loads(json_string, object_hook=_DecodeDict)

  # default is a dictionary of default build configuration values.
  defaults = config_dict.pop(DEFAULT_BUILD_CONFIG)
  templates = config_dict.pop('_templates', None)
  site_params = config_dict.pop('_site_params', None)

  defaultBuildConfig = BuildConfig(**defaults)

  builds = {n: _CreateBuildConfig(defaultBuildConfig, v, templates)
            for n, v in config_dict.iteritems()}

  # config is the struct that holds the complete cbuildbot config.
  result = SiteConfig(defaults=defaults, templates=templates,
                      site_params=site_params)
  result.update(builds)

  return result

# TODO(dgarrett): Remove Decode methods when we prove unicde strings work.
def _DecodeList(data):
  """Convert a JSON result list from unicode to utf-8."""
  rv = []
  for item in data:
    if isinstance(item, unicode):
      item = item.encode('utf-8')
    elif isinstance(item, list):
      item = _DecodeList(item)
    elif isinstance(item, dict):
      item = _DecodeDict(item)

    # Other types (None, int, float, etc) are stored unmodified.
    rv.append(item)
  return rv


def _DecodeDict(data):
  """Convert a JSON result dict from unicode to utf-8."""
  rv = {}
  for key, value in data.iteritems():
    if isinstance(key, unicode):
      key = key.encode('utf-8')

    if isinstance(value, unicode):
      value = value.encode('utf-8')
    elif isinstance(value, list):
      value = _DecodeList(value)
    elif isinstance(value, dict):
      value = _DecodeDict(value)

    # Other types (None, int, float, etc) are stored unmodified.
    rv[key] = value
  return rv


def _CreateHwTestConfig(jsonString):
  """Create a HWTestConfig object from a JSON string."""
  # Each HW Test is dumped as a json string embedded in json.
  hw_test_config = json.loads(jsonString, object_hook=_DecodeDict)
  return HWTestConfig(**hw_test_config)


def _CreateBuildConfig(default, build_dict, templates):
  """Create a BuildConfig object from it's parsed JSON dictionary encoding."""
  # These build config values need special handling.
  child_configs = build_dict.pop('child_configs', None)
  template = build_dict.get('_template')

  my_default = default
  if template:
    my_default = default.derive(templates[template])
  result = my_default.derive(**build_dict)

  hwtests = result.pop('hw_tests', None)
  if hwtests is not None:
    result['hw_tests'] = [_CreateHwTestConfig(hwtest) for hwtest in hwtests]

  hwtests = result.pop('hw_tests_override', None)
  if hwtests is not None:
    result['hw_tests_override'] = [
        _CreateHwTestConfig(hwtest) for hwtest in hwtests
    ]

  if child_configs is not None:
    result['child_configs'] = [_CreateBuildConfig(default, child, templates)
                               for child in child_configs]

  return result
