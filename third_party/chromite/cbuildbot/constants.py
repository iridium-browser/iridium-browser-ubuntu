# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains constants used by cbuildbot and related code."""

from __future__ import print_function

import os


def _FindSourceRoot():
  """Try and find the root check out of the chromiumos tree"""
  source_root = path = os.path.realpath(os.path.join(
      os.path.abspath(__file__), '..', '..', '..'))
  while True:
    if os.path.isdir(os.path.join(path, '.repo')):
      return path
    elif path == '/':
      break
    path = os.path.dirname(path)
  return source_root


SOURCE_ROOT = _FindSourceRoot()
CHROOT_SOURCE_ROOT = '/mnt/host/source'
CHROOT_WORKSPACE_ROOT = '/mnt/host/workspace'
CHROOT_CACHE_ROOT = '/var/cache/chromeos-cache'

CROSUTILS_DIR = os.path.join(SOURCE_ROOT, 'src/scripts')
CHROMITE_DIR = os.path.realpath(os.path.join(
    os.path.abspath(__file__), '..', '..'))
BOOTSTRAP_DIR = os.path.join(CHROMITE_DIR, 'bootstrap')
DEPOT_TOOLS_DIR = os.path.join(SOURCE_ROOT, 'chromium/tools/depot_tools')
CHROMITE_BIN_SUBDIR = 'chromite/bin'
CHROMITE_BIN_DIR = os.path.join(CHROMITE_DIR, 'bin')
PATH_TO_CBUILDBOT = os.path.join(CHROMITE_BIN_SUBDIR, 'cbuildbot')
DEFAULT_CHROOT_DIR = 'chroot'

SITE_CONFIG_DIR = os.path.join(CHROMITE_DIR, 'config')
SITE_CONFIG_FILE = os.path.join(SITE_CONFIG_DIR, 'config_dump.json')

CHROMEOS_CONFIG_FILE = os.path.join(CHROMITE_DIR, 'cbuildbot',
                                    'config_dump.json')

# The following define the location for storing toolchain packages and
# SDK overlay tarballs created during SDK builder runs. The paths are relative
# to the build root's chroot, which guarantees that they are reachable from it
# and get cleaned up when it is removed.
SDK_TOOLCHAINS_OUTPUT = 'tmp/toolchain-pkgs'
SDK_OVERLAYS_OUTPUT = 'tmp/sdk-overlays'

AUTOTEST_BUILD_PATH = 'usr/local/build/autotest'
CQ_CONFIG_FILENAME = 'COMMIT-QUEUE.ini'

# Path to the lsb-release file on the device.
LSB_RELEASE_PATH = '/etc/lsb-release'

HOME_DIRECTORY = os.path.expanduser('~')

# If cbuiltbot is running on a bot, then the cidb access credentials will be
# available here. This directory will not exist otherwise.
CIDB_PROD_BOT_CREDS = os.path.join(HOME_DIRECTORY, '.cidb_creds',
                                   'prod_cidb_bot')
CIDB_DEBUG_BOT_CREDS = os.path.join(HOME_DIRECTORY, '.cidb_creds',
                                    'debug_cidb_bot')

WATERFALL_INTERNAL = 'chromeos'
WATERFALL_EXTERNAL = 'chromiumos'
WATERFALL_TRYBOT = 'chromiumos.tryserver'
WATERFALL_RELEASE = 'chromeos_release'
WATERFALL_BRANCH = 'chromeos.branch'
# These waterfalls are not yet using cidb.
WATERFALL_CHROMIUM = 'chromiumos.chromium'
WATERFALL_CHROME = 'chromeos.chrome'

CIDB_KNOWN_WATERFALLS = (WATERFALL_INTERNAL,
                         WATERFALL_EXTERNAL,
                         WATERFALL_TRYBOT,
                         WATERFALL_RELEASE,
                         WATERFALL_BRANCH,
                         WATERFALL_CHROMIUM,
                         WATERFALL_CHROME,)

ALL_WATERFALLS = CIDB_KNOWN_WATERFALLS

# URLs to the various waterfalls.
BUILD_DASHBOARD = 'http://build.chromium.org/p/chromiumos'
BUILD_INT_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos'
TRYBOT_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromiumos.tryserver'
RELEASE_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos_release'
BRANCH_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos.branch'
CHROMIUM_DASHBOARD = ('https://uberchromegw.corp.google.com/'
                      'i/chromiumos.chromium')
CHROME_DASHBOARD = 'https://uberchromegw.corp.google.com/i/chromeos.chrome'

# Waterfall to dashboard URL mapping.
WATERFALL_TO_DASHBOARD = {
    WATERFALL_INTERNAL: BUILD_INT_DASHBOARD,
    WATERFALL_EXTERNAL: BUILD_DASHBOARD,
    WATERFALL_TRYBOT: TRYBOT_DASHBOARD,
    WATERFALL_RELEASE: RELEASE_DASHBOARD,
    WATERFALL_BRANCH: BRANCH_DASHBOARD,
    WATERFALL_CHROMIUM: CHROMIUM_DASHBOARD,
    WATERFALL_CHROME: CHROME_DASHBOARD,
}

# Builder status strings
BUILDER_STATUS_FAILED = 'fail'
BUILDER_STATUS_PASSED = 'pass'
BUILDER_STATUS_INFLIGHT = 'inflight'
BUILDER_STATUS_MISSING = 'missing'
BUILDER_STATUS_ABORTED = 'aborted'
# The following statuses are currently only used for build stages.
BUILDER_STATUS_PLANNED = 'planned'
BUILDER_STATUS_SKIPPED = 'skipped'
BUILDER_STATUS_FORGIVEN = 'forgiven'
BUILDER_COMPLETED_STATUSES = (BUILDER_STATUS_PASSED,
                              BUILDER_STATUS_FAILED,
                              BUILDER_STATUS_ABORTED,
                              BUILDER_STATUS_SKIPPED,
                              BUILDER_STATUS_FORGIVEN)
BUILDER_ALL_STATUSES = (BUILDER_STATUS_FAILED,
                        BUILDER_STATUS_PASSED,
                        BUILDER_STATUS_INFLIGHT,
                        BUILDER_STATUS_MISSING,
                        BUILDER_STATUS_ABORTED,
                        BUILDER_STATUS_PLANNED,
                        BUILDER_STATUS_SKIPPED,
                        BUILDER_STATUS_FORGIVEN)

# CL status strings
CL_STATUS_FAILED = BUILDER_STATUS_FAILED
CL_STATUS_INFLIGHT = BUILDER_STATUS_INFLIGHT
CL_STATUS_PASSED = BUILDER_STATUS_PASSED
CL_STATUS_LAUNCHING = 'launching'
CL_STATUS_WAITING = 'waiting'
CL_STATUS_READY_TO_SUBMIT = 'ready-to-submit'
CL_STATUS_FULLY_VERIFIED = 'fully-verified'

# Change sources
CHANGE_SOURCE_INTERNAL = 'internal'
CHANGE_SOURCE_EXTERNAL = 'external'

# Build failure categories
FAILURE_CATEGORY_BAD_CL = 'bad_cl'
FAILURE_CATEGORY_BUG_IN_TOT = 'bug_in_tot'
FAILURE_CATEGORY_MERGE_CONFLICT = 'merge_conflict'
FAILURE_CATEGORY_TREE_CLOSED = 'tree_closed'
FAILURE_CATEGORY_SCHEDULED_ABORT = 'scheduled_abort'
FAILURE_CATEGORY_CL_NOT_READY = 'cl_not_ready'
FAILURE_CATEGORY_BAD_CHROME = 'bad_chrome'
FAILURE_CATEGORY_INFRA_FAILURE = 'infra_failure'
FAILURE_CATEGORY_TEST_FLAKE = 'test_flake'
FAILURE_CATEGORY_GERRIT_FAILURE = 'gerrit_failure'
FAILURE_CATEGORY_GS_FAILURE = 'gs_failure'
FAILURE_CATEGORY_LAB_FAILURE = 'lab_failure'
FAILURE_CATEGORY_BAD_BINARY_PACKAGE = 'bad_binary_package'
FAILURE_CATEGORY_BUILD_FLAKE = 'build_flake'
FAILURE_CATEGORY_MYSTERY = 'mystery'

FAILURE_CATEGORY_ALL_CATEGORIES = (
    FAILURE_CATEGORY_BAD_CL,
    FAILURE_CATEGORY_BUG_IN_TOT,
    FAILURE_CATEGORY_MERGE_CONFLICT,
    FAILURE_CATEGORY_TREE_CLOSED,
    FAILURE_CATEGORY_SCHEDULED_ABORT,
    FAILURE_CATEGORY_CL_NOT_READY,
    FAILURE_CATEGORY_BAD_CHROME,
    FAILURE_CATEGORY_INFRA_FAILURE,
    FAILURE_CATEGORY_TEST_FLAKE,
    FAILURE_CATEGORY_GERRIT_FAILURE,
    FAILURE_CATEGORY_GS_FAILURE,
    FAILURE_CATEGORY_LAB_FAILURE,
    FAILURE_CATEGORY_BAD_BINARY_PACKAGE,
    FAILURE_CATEGORY_BUILD_FLAKE,
    FAILURE_CATEGORY_MYSTERY,
)


# Exception categories, as recorded in cidb
EXCEPTION_CATEGORY_UNKNOWN = 'unknown'
EXCEPTION_CATEGORY_BUILD = 'build'
EXCEPTION_CATEGORY_TEST = 'test'
EXCEPTION_CATEGORY_INFRA = 'infra'
EXCEPTION_CATEGORY_LAB = 'lab'

EXCEPTION_CATEGORY_ALL_CATEGORIES = (
    EXCEPTION_CATEGORY_UNKNOWN,
    EXCEPTION_CATEGORY_BUILD,
    EXCEPTION_CATEGORY_TEST,
    EXCEPTION_CATEGORY_INFRA,
    EXCEPTION_CATEGORY_LAB,
)

# TODO: Eliminate these or merge with manifest_version.py:STATUS_PASSED
# crbug.com/318930
FINAL_STATUS_PASSED = 'passed'
FINAL_STATUS_FAILED = 'failed'

# Re-execution API constants.
# Used by --resume and --bootstrap to decipher which options they
# can pass to the target cbuildbot (since it may not have that
# option).
# Format is Major:Minor.  Minor is used for tracking new options added
# that aren't critical to the older version if it's not ran.
# Major is used for tracking heavy API breakage- for example, no longer
# supporting the --resume option.
REEXEC_API_MAJOR = 0
REEXEC_API_MINOR = 3
REEXEC_API_VERSION = '%i.%i' % (REEXEC_API_MAJOR, REEXEC_API_MINOR)

# Minor version 3 is the first to support --master-build-id
REEXEC_API_MASTER_BUILD_ID = 3

# We rely on the (waterfall, builder name, build number) to uniquely identify
# a build. However, future migrations or state wipes of the buildbot master may
# cause it to reset its build number counter. When that happens, this value
# should be incremented, ensuring that (waterfall, builder name, build number,
# buildbot generation) is a unique identifier of builds.
BUILDBOT_GENERATION = 1

ISOLATESERVER = 'https://isolateserver.appspot.com'

GOOGLE_EMAIL = '@google.com'
CHROMIUM_EMAIL = '@chromium.org'

CORP_DOMAIN = 'corp.google.com'
GOLO_DOMAIN = 'golo.chromium.org'
CHROME_DOMAIN = 'chrome.' + CORP_DOMAIN

GOB_HOST = '%s.googlesource.com'

EXTERNAL_GOB_INSTANCE = 'chromium'
EXTERNAL_GERRIT_INSTANCE = 'chromium-review'
EXTERNAL_GOB_HOST = GOB_HOST % EXTERNAL_GOB_INSTANCE
EXTERNAL_GERRIT_HOST = GOB_HOST % EXTERNAL_GERRIT_INSTANCE
EXTERNAL_GOB_URL = 'https://%s' % EXTERNAL_GOB_HOST
EXTERNAL_GERRIT_URL = 'https://%s' % EXTERNAL_GERRIT_HOST

INTERNAL_GOB_INSTANCE = 'chrome-internal'
INTERNAL_GERRIT_INSTANCE = 'chrome-internal-review'
INTERNAL_GOB_HOST = GOB_HOST % INTERNAL_GOB_INSTANCE
INTERNAL_GERRIT_HOST = GOB_HOST % INTERNAL_GERRIT_INSTANCE
INTERNAL_GOB_URL = 'https://%s' % INTERNAL_GOB_HOST
INTERNAL_GERRIT_URL = 'https://%s' % INTERNAL_GERRIT_HOST

AOSP_GOB_INSTANCE = 'android'
AOSP_GERRIT_INSTANCE = 'android-review'
AOSP_GOB_HOST = GOB_HOST % AOSP_GOB_INSTANCE
AOSP_GERRIT_HOST = GOB_HOST % AOSP_GERRIT_INSTANCE
AOSP_GOB_URL = 'https://%s' % AOSP_GOB_HOST
AOSP_GERRIT_URL = 'https://%s' % AOSP_GERRIT_HOST

GOB_COOKIE_PATH = os.path.expanduser('~/.git-credential-cache/cookie')
GITCOOKIES_PATH = os.path.expanduser('~/.gitcookies')

# Timestamps in the JSON from GoB's web interface is of the form 'Tue
# Dec 02 17:48:06 2014' and is assumed to be in UTC.
GOB_COMMIT_TIME_FORMAT = '%a %b %d %H:%M:%S %Y'

REPO_PROJECT = 'external/repo'
REPO_URL = '%s/%s' % (EXTERNAL_GOB_URL, REPO_PROJECT)

CHROMITE_PROJECT = 'chromiumos/chromite'
CHROMITE_URL = '%s/%s' % (EXTERNAL_GOB_URL, CHROMITE_PROJECT)
CHROMIUM_SRC_PROJECT = 'chromium/src'
CHROMIUM_GOB_URL = '%s/%s.git' % (EXTERNAL_GOB_URL, CHROMIUM_SRC_PROJECT)
CHROME_INTERNAL_PROJECT = 'chrome/src-internal'
CHROME_INTERNAL_GOB_URL = '%s/%s.git' % (
    INTERNAL_GOB_URL, CHROME_INTERNAL_PROJECT)

MANIFEST_PROJECT = 'chromiumos/manifest'
MANIFEST_INT_PROJECT = 'chromeos/manifest-internal'
MANIFEST_PROJECTS = (MANIFEST_PROJECT, MANIFEST_INT_PROJECT)

MANIFEST_URL = '%s/%s' % (EXTERNAL_GOB_URL, MANIFEST_PROJECT)
MANIFEST_INT_URL = '%s/%s' % (INTERNAL_GERRIT_URL, MANIFEST_INT_PROJECT)

DEFAULT_MANIFEST = 'default.xml'
OFFICIAL_MANIFEST = 'official.xml'
LKGM_MANIFEST = 'LKGM/lkgm.xml'

SHARED_CACHE_ENVVAR = 'CROS_CACHEDIR'

# These projects can be responsible for infra failures.
INFRA_PROJECTS = (CHROMITE_PROJECT,)

# CrOS remotes specified in the manifests.
EXTERNAL_REMOTE = 'cros'
INTERNAL_REMOTE = 'cros-internal'
# TODO(dgarrett): Reconsider when crbug.com/428215 is fixed.
KAYLE_INTERNAL_REMOTE = 'kayle-cros-internal'
CHROMIUM_REMOTE = 'chromium'
CHROME_REMOTE = 'chrome'
AOSP_REMOTE = 'aosp'

GERRIT_HOSTS = {
    EXTERNAL_REMOTE: EXTERNAL_GERRIT_HOST,
    INTERNAL_REMOTE: INTERNAL_GERRIT_HOST,
    AOSP_REMOTE: AOSP_GERRIT_HOST,
}

# Only remotes listed in CROS_REMOTES are considered branchable.
# CROS_REMOTES and BRANCHABLE_PROJECTS must be kept in sync.
CROS_REMOTES = {
    EXTERNAL_REMOTE: EXTERNAL_GOB_URL,
    INTERNAL_REMOTE: INTERNAL_GOB_URL,
    KAYLE_INTERNAL_REMOTE: INTERNAL_GOB_URL,
    AOSP_REMOTE: AOSP_GOB_URL,
}

GIT_REMOTES = {
    CHROMIUM_REMOTE: EXTERNAL_GOB_URL,
    CHROME_REMOTE: INTERNAL_GOB_URL,
}
GIT_REMOTES.update(CROS_REMOTES)

# Prefix to distinguish internal and external changes. This is used
# when user specifies a patch with "-g", when generating a key for
# a patch to used in our PatchCache, and when display a custom string
# for the patch.
INTERNAL_CHANGE_PREFIX = '*'
EXTERNAL_CHANGE_PREFIX = ''

CHANGE_PREFIX = {
    INTERNAL_REMOTE: INTERNAL_CHANGE_PREFIX,
    EXTERNAL_REMOTE: EXTERNAL_CHANGE_PREFIX,
}

# List of remotes that are ok to include in the external manifest.
EXTERNAL_REMOTES = (EXTERNAL_REMOTE, CHROMIUM_REMOTE)

# Mapping 'remote name' -> regexp that matches names of repositories on that
# remote that can be branched when creating CrOS branch. Branching script will
# actually create a new git ref when branching these projects. It won't attempt
# to create a git ref for other projects that may be mentioned in a manifest.
# If a remote is missing from this dictionary, all projects on that remote are
# considered to not be branchable.
BRANCHABLE_PROJECTS = {
    EXTERNAL_REMOTE: r'chromiumos/(.+)',
    INTERNAL_REMOTE: r'chromeos/(.+)',
    KAYLE_INTERNAL_REMOTE: r'chromeos/(.+)',
}

# The manifest contains extra attributes in the 'project' nodes to determine our
# branching strategy for the project.
#   create: Create a new branch on the project repo for the new CrOS branch.
#           This is the default.
#   pin: On the CrOS branch, pin the project to the current revision.
#   tot: On the CrOS branch, the project still tracks ToT.
MANIFEST_ATTR_BRANCHING = 'branch-mode'
MANIFEST_ATTR_BRANCHING_CREATE = 'create'
MANIFEST_ATTR_BRANCHING_PIN = 'pin'
MANIFEST_ATTR_BRANCHING_TOT = 'tot'
MANIFEST_ATTR_BRANCHING_ALL = (
    MANIFEST_ATTR_BRANCHING_CREATE,
    MANIFEST_ATTR_BRANCHING_PIN,
    MANIFEST_ATTR_BRANCHING_TOT,
)

# TODO(sosa): Move to manifest-versions-external once its created
MANIFEST_VERSIONS_GOB_URL = EXTERNAL_GOB_URL + '/chromiumos/manifest-versions'
MANIFEST_VERSIONS_GOB_URL_TEST = MANIFEST_VERSIONS_GOB_URL + '-test'

MANIFEST_VERSIONS_INT_GOB_URL = INTERNAL_GOB_URL + '/chromeos/manifest-versions'
MANIFEST_VERSIONS_INT_GOB_URL_TEST = MANIFEST_VERSIONS_INT_GOB_URL + '-test'

MANIFEST_VERSIONS_GS_URL = 'gs://chromeos-manifest-versions'

# Standard directories under buildroot for cloning these repos.
EXTERNAL_MANIFEST_VERSIONS_PATH = 'manifest-versions'
INTERNAL_MANIFEST_VERSIONS_PATH = 'manifest-versions-internal'

STREAK_COUNTERS = 'streak_counters'

PATCH_BRANCH = 'patch_branch'
STABLE_EBUILD_BRANCH = 'stabilizing_branch'
MERGE_BRANCH = 'merge_branch'

# These branches are deleted at the beginning of every buildbot run.
CREATED_BRANCHES = [
    PATCH_BRANCH,
    STABLE_EBUILD_BRANCH,
    MERGE_BRANCH
]

# Constants for uprevving Chrome

# Portage category and package name for Chrome.
CHROME_PN = 'chromeos-chrome'
CHROME_CP = 'chromeos-base/%s' % CHROME_PN

# Other packages to uprev while uprevving Chrome.
OTHER_CHROME_PACKAGES = ['chromeos-base/chromium-source']

# Chrome use flags
USE_CHROME_INTERNAL = 'chrome_internal'
USE_AFDO_USE = 'afdo_use'


# Builds and validates _alpha ebuilds.  These builds sync to the latest
# revsion of the Chromium src tree and build with that checkout.
CHROME_REV_TOT = 'tot'

# Builds and validates chrome at a given revision through cbuildbot
# --chrome_version
CHROME_REV_SPEC = 'spec'

# Builds and validates the latest Chromium release as defined by
# ~/trunk/releases in the Chrome src tree.  These ebuilds are suffixed with rc.
CHROME_REV_LATEST = 'latest_release'

# Builds and validates the latest Chromium release for a specific Chromium
# branch that we want to watch.  These ebuilds are suffixed with rc.
CHROME_REV_STICKY = 'stable_release'

# Builds and validates Chromium for a pre-populated directory.
# Also uses _alpha, since portage doesn't have anything lower.
CHROME_REV_LOCAL = 'local'
VALID_CHROME_REVISIONS = [CHROME_REV_TOT, CHROME_REV_LATEST,
                          CHROME_REV_STICKY, CHROME_REV_LOCAL, CHROME_REV_SPEC]


# Build types supported.

# TODO(sosa): Deprecate PFQ type.
# Incremental builds that are built using binary packages when available.
# These builds have less validation than other build types.
INCREMENTAL_TYPE = 'binary'

# These builds serve as PFQ builders.  This is being deprecated.
PFQ_TYPE = 'pfq'

# Hybrid Commit and PFQ type.  Ultimate protection.  Commonly referred to
# as simply "commit queue" now.
PALADIN_TYPE = 'paladin'

# A builder that kicks off Pre-CQ builders that bless the purest CLs.
PRE_CQ_LAUNCHER_TYPE = 'priest'

# Chrome PFQ type.  Incremental build type that builds and validates new
# versions of Chrome.  Only valid if set with CHROME_REV.  See
# VALID_CHROME_REVISIONS for more information.
CHROME_PFQ_TYPE = 'chrome'

# Builds from source and non-incremental.  This builds fully wipe their
# chroot before the start of every build and no not use a BINHOST.
BUILD_FROM_SOURCE_TYPE = 'full'

# Full but with versioned logic.
CANARY_TYPE = 'canary'

# Generate payloads for an already built build/version.
PAYLOADS_TYPE = 'payloads'

BRANCH_UTIL_CONFIG = 'branch-util'

# Special build type for Chroot builders.  These builds focus on building
# toolchains and validate that they work.
CHROOT_BUILDER_TYPE = 'chroot'
CHROOT_BUILDER_BOARD = 'amd64-host'

VALID_BUILD_TYPES = (
    PALADIN_TYPE,
    INCREMENTAL_TYPE,
    BUILD_FROM_SOURCE_TYPE,
    CANARY_TYPE,
    CHROOT_BUILDER_TYPE,
    CHROOT_BUILDER_BOARD,
    CHROME_PFQ_TYPE,
    PFQ_TYPE,
    PRE_CQ_LAUNCHER_TYPE,
    PAYLOADS_TYPE,
)

# The default list of pre-cq configs to use.
PRE_CQ_DEFAULT_CONFIGS = ['rambi-pre-cq', 'mixed-a-pre-cq', 'mixed-b-pre-cq',
                          'mixed-c-pre-cq']

# The name of the pre-cq launching config.
PRE_CQ_LAUNCHER_CONFIG = 'pre-cq-launcher'

# The name of the Pre-CQ launcher on the waterfall.
PRE_CQ_LAUNCHER_NAME = 'Pre-CQ Launcher'

# The COMMIT-QUEUE.ini and commit message option that overrides pre-cq configs
# to test with.
PRE_CQ_CONFIGS_OPTION = 'pre-cq-configs'
PRE_CQ_CONFIGS_OPTION_REGEX = PRE_CQ_CONFIGS_OPTION + ':'

# Define pool of machines for Hardware tests.
HWTEST_DEFAULT_NUM = 6
HWTEST_TRYBOT_NUM = 3
HWTEST_MACH_POOL = 'bvt'
HWTEST_PALADIN_POOL = 'cq'
HWTEST_TOT_PALADIN_POOL = 'tot-cq'
HWTEST_PFQ_POOL = 'pfq'
HWTEST_SUITES_POOL = 'suites'
HWTEST_CHROME_PERF_POOL = 'chromeperf'
HWTEST_TRYBOT_POOL = HWTEST_SUITES_POOL
HWTEST_WIFICELL_PRE_CQ_POOL = 'wificell-pre-cq'


# Master build timeouts in seconds. This is the overall timeout set by the
# master for the lock-step master-slave builds.
MASTER_BUILD_TIMEOUT_SECONDS = {
    PFQ_TYPE: 20 * 60,
    # Canaries are scheduled to run every 8 hours. Leave some gap.
    CANARY_TYPE: (7 * 60 + 50) * 60,
}
MASTER_BUILD_TIMEOUT_DEFAULT_SECONDS = 4 * 60 * 60


# Defines for the various hardware test suites:
#   AU: Blocking suite run against all canaries; tests basic AU
#       functionality.
#   BVT:  Basic blocking suite to be run against any build that
#       requires a HWTest phase.
#   COMMIT:  Suite of basic tests required for commits to the source
#       tree.  Runs as a blocking suite on the CQ and PFQ; runs as
#       a non-blocking suite on canaries.
#   CANARY:  Non-blocking suite run only against the canaries.
#   AFDO:  Non-blocking suite run only AFDO builders.
#   MOBLAB: Blocking Suite run only on *_moblab builders.
HWTEST_AU_SUITE = 'au'
HWTEST_BVT_SUITE = 'bvt-inline'
HWTEST_COMMIT_SUITE = 'bvt-cq'
HWTEST_CANARY_SUITE = 'bvt-perbuild'
HWTEST_AFDO_SUITE = 'AFDO_record'
HWTEST_MOBLAB_SUITE = 'moblab'
HWTEST_MOBLAB_QUICK_SUITE = 'moblab_quick'
HWTEST_SANITY_SUITE = 'sanity'
HWTEST_PROVISION_SUITE = 'bvt-provision'


# Additional timeout to wait for autotest to abort a suite if the test takes
# too long to run. This is meant to be overly conservative as a timeout may
# indicate that autotest is at capacity.
HWTEST_TIMEOUT_EXTENSION = 10 * 60

HWTEST_DEFAULT_PRIORITY = 'DEFAULT'
HWTEST_CQ_PRIORITY = 'CQ'
HWTEST_BUILD_PRIORITY = 'Build'
HWTEST_PFQ_PRIORITY = 'PFQ'
HWTEST_POST_BUILD_PRIORITY = 'PostBuild'

# Ordered by priority (first item being lowest).
HWTEST_VALID_PRIORITIES = ['Weekly',
                           'Daily',
                           HWTEST_POST_BUILD_PRIORITY,
                           HWTEST_DEFAULT_PRIORITY,
                           HWTEST_BUILD_PRIORITY,
                           HWTEST_PFQ_PRIORITY,
                           HWTEST_CQ_PRIORITY]

# Creates a mapping of priorities to make easy comparsions.
HWTEST_PRIORITIES_MAP = dict(
    (p, i) for i, p in enumerate(HWTEST_VALID_PRIORITIES))

# Defines VM Test types.
FULL_AU_TEST_TYPE = 'full_suite'
SIMPLE_AU_TEST_TYPE = 'pfq_suite'
SMOKE_SUITE_TEST_TYPE = 'smoke_suite'
TELEMETRY_SUITE_TEST_TYPE = 'telemetry_suite'
CROS_VM_TEST_TYPE = 'cros_vm_test'
DEV_MODE_TEST_TYPE = 'dev_mode_test'
VALID_VM_TEST_TYPES = [FULL_AU_TEST_TYPE, SIMPLE_AU_TEST_TYPE,
                       SMOKE_SUITE_TEST_TYPE, TELEMETRY_SUITE_TEST_TYPE,
                       CROS_VM_TEST_TYPE, DEV_MODE_TEST_TYPE]

CHROMIUMOS_OVERLAY_DIR = 'src/third_party/chromiumos-overlay'
VERSION_FILE = os.path.join(CHROMIUMOS_OVERLAY_DIR,
                            'chromeos/config/chromeos_version.sh')
SDK_VERSION_FILE = os.path.join(CHROMIUMOS_OVERLAY_DIR,
                                'chromeos/binhost/host/sdk_version.conf')
SDK_GS_BUCKET = 'chromiumos-sdk'

PUBLIC = 'public'
PRIVATE = 'private'

BOTH_OVERLAYS = 'both'
PUBLIC_OVERLAYS = PUBLIC
PRIVATE_OVERLAYS = PRIVATE
VALID_OVERLAYS = [BOTH_OVERLAYS, PUBLIC_OVERLAYS, PRIVATE_OVERLAYS, None]

# Common default logging settings for use with the logging module.
LOGGER_FMT = '%(asctime)s: %(levelname)s: %(message)s'
LOGGER_DATE_FMT = '%H:%M:%S'

# Used by remote patch serialization/deserialzation.
INTERNAL_PATCH_TAG = 'i'
EXTERNAL_PATCH_TAG = 'e'
PATCH_TAGS = (INTERNAL_PATCH_TAG, EXTERNAL_PATCH_TAG)

# Tree status strings
TREE_OPEN = 'open'
TREE_THROTTLED = 'throttled'
TREE_CLOSED = 'closed'
TREE_MAINTENANCE = 'maintenance'
# The statuses are listed in the order of increasing severity.
VALID_TREE_STATUSES = (TREE_OPEN, TREE_THROTTLED, TREE_CLOSED, TREE_MAINTENANCE)


# Common parts of query used for CQ, THROTTLED_CQ, and PRECQ.
# "NOT is:draft" in this query doesn't work, it finds any non-draft revision.
# We want to match drafts anyway, so we can comment on them.
_QUERIES = {
    # CLs that are open and not vetoed.
    'open': 'status:open AND -label:CodeReview=-2 AND -label:Verified=-1',

    # CLs that are approved and verified.
    'approved': 'label:Code-Review=+2 AND label:Verified=+1',
}

#
# Please note that requiring the +2 code review (or Trybot-Ready) for all CQ
# and PreCQ runs is a security requirement. Otherwise arbitrary people can
# run code on our servers.
#
# The Verified and Commit-Queue flags can be set by any registered user (you
# don't need commit access to set them.)
#


# Default gerrit query used to find changes for CQ.
# Permits CQ+1 or CQ+2 changes.
CQ_READY_QUERY = (
    '%(open)s AND %(approved)s AND label:Commit-Queue>=1' % _QUERIES,
    lambda change: change.IsMergeable())

# Gerrit query used to find changes for CQ when tree is throttled.
# Permits only CQ+2 changes.
THROTTLED_CQ_READY_QUERY = (
    '%(open)s AND %(approved)s AND label:Commit-Queue>=2' % _QUERIES,
    lambda change: change.IsMergeable() and change.HasApproval('COMR', '2'))

# The PreCQ does not require the CQ bit to be set if it's a recent CL, or if
# the Trybot-Ready flag has been set.
PRECQ_READY_QUERY = (
    '%(open)s AND (%(approved)s AND label:Commit-Queue>=1 OR '
    'label:Code-Review=+2 AND -age:2h OR label:Trybot-Ready=+1)' % _QUERIES,
    lambda change: (not change.IsBeingMerged() and
                    change.HasApproval('CRVW', '2') or
                    change.HasApproval('TRY', '1')))

GERRIT_ON_BORG_LABELS = {
    'Code-Review': 'CRVW',
    'Commit-Queue': 'COMR',
    'Verified': 'VRIF',
    'Trybot-Ready': 'TRY',
}

# Actions that a CQ run can take on a CL
CL_ACTION_PICKED_UP = 'picked_up'         # CL picked up in CommitQueueSync
CL_ACTION_SUBMITTED = 'submitted'         # CL submitted successfully
CL_ACTION_KICKED_OUT = 'kicked_out'       # CL CQ-Ready value set to zero
CL_ACTION_SUBMIT_FAILED = 'submit_failed' # CL submitted but submit failed
CL_ACTION_VERIFIED = 'verified'           # CL was verified by the builder
CL_ACTION_FORGIVEN = 'forgiven'           # Build failed, but CL not kicked out

# Actions the Pre-CQ Launcher can take on a CL
# See cbuildbot/stages/sync_stages.py:PreCQLauncherStage for more info
CL_ACTION_PRE_CQ_INFLIGHT = 'pre_cq_inflight'
CL_ACTION_PRE_CQ_PASSED = 'pre_cq_passed'
CL_ACTION_PRE_CQ_FAILED = 'pre_cq_failed'
CL_ACTION_PRE_CQ_LAUNCHING = 'pre_cq_launching'
CL_ACTION_PRE_CQ_WAITING = 'pre_cq_waiting'
CL_ACTION_PRE_CQ_FULLY_VERIFIED = 'pre_cq_fully_verified'
CL_ACTION_PRE_CQ_READY_TO_SUBMIT = 'pre_cq_ready_to_submit'
# Recording this action causes the pre-cq status and all per-config statuses to
# be reset.
CL_ACTION_PRE_CQ_RESET = 'pre_cq_reset'

# Miscellaneous actions

# Recorded by pre-cq launcher for a change when it is noticed that a previously
# rejected change is again in the queue.
# This is a best effort detection for developers re-marking their changes, to
# help calculate true CQ handling time. It is susceptible to developers
# un-marking their change after is requeued or to the CQ picking up a CL before
# it is seen by the pre-cq-launcher.
CL_ACTION_REQUEUED = 'requeued'

# Recorded by pre-cq launcher when it begins handling a change that isn't marked
# as CQ+1. This indicates that all actions between this and the next
# CL_ACTION_REQUEUED action have occured on a non-CQ+1 change.
CL_ACTION_SPECULATIVE = 'speculative'

# Recorded by pre-cq launcher when it has screened a change for necessary
# tryjobs
CL_ACTION_SCREENED_FOR_PRE_CQ = 'screened_for_pre_cq'
# Recorded by pre-cq launcher for each tryjob config necessary to validate
# a change, with |reason| field specifying the config.
CL_ACTION_VALIDATION_PENDING_PRE_CQ = 'validation_pending_pre_cq'

# Recorded by CQ slaves builds when a picked-up CL is determined to be
# irrelevant to that slave build.
CL_ACTION_IRRELEVANT_TO_SLAVE = 'irrelevant_to_slave'

# Recorded by pre-cq-launcher when it launches a tryjob with a particular
# config. The |reason| field of the action will be the config.
CL_ACTION_TRYBOT_LAUNCHING = 'trybot_launching'


CL_ACTIONS = (CL_ACTION_PICKED_UP,
              CL_ACTION_SUBMITTED,
              CL_ACTION_KICKED_OUT,
              CL_ACTION_SUBMIT_FAILED,
              CL_ACTION_VERIFIED,
              CL_ACTION_PRE_CQ_INFLIGHT,
              CL_ACTION_PRE_CQ_PASSED,
              CL_ACTION_PRE_CQ_FAILED,
              CL_ACTION_PRE_CQ_LAUNCHING,
              CL_ACTION_PRE_CQ_WAITING,
              CL_ACTION_PRE_CQ_READY_TO_SUBMIT,
              CL_ACTION_REQUEUED,
              CL_ACTION_SCREENED_FOR_PRE_CQ,
              CL_ACTION_VALIDATION_PENDING_PRE_CQ,
              CL_ACTION_IRRELEVANT_TO_SLAVE,
              CL_ACTION_TRYBOT_LAUNCHING,
              CL_ACTION_SPECULATIVE,
              CL_ACTION_FORGIVEN,
              CL_ACTION_PRE_CQ_FULLY_VERIFIED,
              CL_ACTION_PRE_CQ_RESET)

# Actions taken by a builder when making a decision about a CL.
CL_DECISION_ACTIONS = (
    CL_ACTION_SUBMITTED,
    CL_ACTION_KICKED_OUT,
    CL_ACTION_SUBMIT_FAILED,
    CL_ACTION_VERIFIED,
    CL_ACTION_FORGIVEN
)

# Per-config status strings for a CL.
CL_PRECQ_CONFIG_STATUS_PENDING = 'pending'
CL_PRECQ_CONFIG_STATUS_LAUNCHED = 'launched'
CL_PRECQ_CONFIG_STATUS_INFLIGHT = CL_STATUS_INFLIGHT
CL_PRECQ_CONFIG_STATUS_FAILED = BUILDER_STATUS_FAILED
CL_PRECQ_CONFIG_STATUS_VERIFIED = CL_ACTION_VERIFIED
CL_PRECQ_CONFIG_STATUSES = (CL_PRECQ_CONFIG_STATUS_PENDING,
                            CL_PRECQ_CONFIG_STATUS_LAUNCHED,
                            CL_PRECQ_CONFIG_STATUS_INFLIGHT,
                            CL_PRECQ_CONFIG_STATUS_FAILED,
                            CL_PRECQ_CONFIG_STATUS_VERIFIED)

# CL submission, rejection, or forgiven reasons (i.e. strategies).
STRATEGY_CQ_SUCCESS = 'strategy:cq-success'
STRATEGY_CQ_PARTIAL = 'strategy:cq-submit-partial-pool'
STRATEGY_PRECQ_SUBMIT = 'strategy:pre-cq-submit'
STRATEGY_NONMANIFEST = 'strategy:non-manifest-submit'

# CQ types.
CQ = 'cq'
PRE_CQ = 'pre-cq'

# Environment variables that should be exposed to all children processes
# invoked via cros_build_lib.RunCommand.
ENV_PASSTHRU = ('CROS_SUDO_KEEP_ALIVE', SHARED_CACHE_ENVVAR)

# List of variables to proxy into the chroot from the host, and to
# have sudo export if existent. Anytime this list is modified, a new
# chroot_version_hooks.d upgrade script that symlinks to 45_rewrite_sudoers.d
# should be created.
CHROOT_ENVIRONMENT_WHITELIST = (
    'CHROMEOS_OFFICIAL',
    'CHROMEOS_VERSION_AUSERVER',
    'CHROMEOS_VERSION_DEVSERVER',
    'CHROMEOS_VERSION_TRACK',
    'GCC_GITHASH',
    'GIT_AUTHOR_EMAIL',
    'GIT_AUTHOR_NAME',
    'GIT_COMMITTER_EMAIL',
    'GIT_COMMITTER_NAME',
    'GIT_PROXY_COMMAND',
    'GIT_SSH',
    'RSYNC_PROXY',
    'SSH_AGENT_PID',
    'SSH_AUTH_SOCK',
    'USE',
    'all_proxy',
    'ftp_proxy',
    'http_proxy',
    'https_proxy',
    'no_proxy',
)

# Paths for Chrome LKGM which are relative to the Chromium base url.
CHROME_LKGM_FILE = 'CHROMEOS_LKGM'
PATH_TO_CHROME_LKGM = 'chromeos/%s' % CHROME_LKGM_FILE

# Cache constants.
COMMON_CACHE = 'common'

# Artifact constants.
def _SlashToUnderscore(string):
  return string.replace('/', '_')

DEFAULT_ARCHIVE_BUCKET = 'gs://chromeos-image-archive'
RELEASE_BUCKET = 'gs://chromeos-releases'
TRASH_BUCKET = 'gs://chromeos-throw-away-bucket'
CHROME_SYSROOT_TAR = 'sysroot_%s.tar.xz' % _SlashToUnderscore(CHROME_CP)
CHROME_ENV_TAR = 'environment_%s.tar.xz' % _SlashToUnderscore(CHROME_CP)
CHROME_ENV_FILE = 'environment'
BASE_IMAGE_NAME = 'chromiumos_base_image'
BASE_IMAGE_TAR = '%s.tar.xz' % BASE_IMAGE_NAME
BASE_IMAGE_BIN = '%s.bin' % BASE_IMAGE_NAME
IMAGE_SCRIPTS_NAME = 'image_scripts'
IMAGE_SCRIPTS_TAR = '%s.tar.xz' % IMAGE_SCRIPTS_NAME
VM_IMAGE_NAME = 'chromiumos_qemu_image'
VM_IMAGE_BIN = '%s.bin' % VM_IMAGE_NAME
VM_DISK_PREFIX = 'chromiumos_qemu_disk.bin'
VM_MEM_PREFIX = 'chromiumos_qemu_mem.bin'
VM_TEST_RESULTS = 'vm_test_results_%(attempt)s'

TEST_IMAGE_NAME = 'chromiumos_test_image'
TEST_IMAGE_TAR = '%s.tar.xz' % TEST_IMAGE_NAME
TEST_IMAGE_BIN = '%s.bin' % TEST_IMAGE_NAME

DEV_IMAGE_NAME = 'chromiumos_image'
DEV_IMAGE_BIN = '%s.bin' % DEV_IMAGE_NAME

RECOVERY_IMAGE_NAME = 'recovery_image'
RECOVERY_IMAGE_BIN = '%s.bin' % RECOVERY_IMAGE_NAME

IMAGE_TYPE_TO_NAME = {
    'base': BASE_IMAGE_BIN,
    'dev': DEV_IMAGE_BIN,
    'recovery': RECOVERY_IMAGE_BIN,
    'test': TEST_IMAGE_BIN,
}
IMAGE_NAME_TO_TYPE = dict((v, k) for k, v in IMAGE_TYPE_TO_NAME.iteritems())

METADATA_JSON = 'metadata.json'
PARTIAL_METADATA_JSON = 'partial-metadata.json'
DELTA_SYSROOT_TAR = 'delta_sysroot.tar.xz'
DELTA_SYSROOT_BATCH = 'batch'

# Global configuration constants.
CHROMITE_CONFIG_DIR = os.path.expanduser('~/.chromite')
CHROME_SDK_BASHRC = os.path.join(CHROMITE_CONFIG_DIR, 'chrome_sdk.bashrc')
SYNC_RETRIES = 2
SLEEP_TIMEOUT = 30

# Lab status url.
LAB_STATUS_URL = 'http://chromiumos-lab.appspot.com/current?format=json'

GOLO_SMTP_SERVER = 'mail.golo.chromium.org'

# Valid sherrif types.
TREE_SHERIFF = 'tree'
CHROME_GARDENER = 'chrome'

# URLs to retrieve sheriff names from the waterfall.
TREE_SHERIFF_URL = '%s/sheriff.js' % (BUILD_DASHBOARD)
TREE_SHERIFF2_URL = '%s/sheriff2.js' % (BUILD_DASHBOARD)
CHROME_GARDENER_URL = '%s/sheriff_cr_cros_gardeners.js' % (BUILD_DASHBOARD)

SHERIFF_TYPE_TO_URL = {
    TREE_SHERIFF: (TREE_SHERIFF_URL, TREE_SHERIFF2_URL),
    CHROME_GARDENER: (CHROME_GARDENER_URL,)
}


# Useful config targets.
CQ_MASTER = 'master-paladin'
CANARY_MASTER = 'master-release'
PFQ_MASTER = 'master-chromium-pfq'
BINHOST_PRE_CQ = 'binhost-pre-cq'
WIFICELL_PRE_CQ = 'wificell-pre-cq'


# Email validation regex. Not quite fully compliant with RFC 2822, but good
# approximation.
EMAIL_REGEX = r'[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,4}'

# Blacklist of files not allowed to be uploaded into the Partner Project Google
# Storage Buckets:
# debug.tgz contains debug symbols.
# manifest.xml exposes all of our repo names.
# vm_test_results can contain symbolicated crash dumps.
EXTRA_BUCKETS_FILES_BLACKLIST = [
    'debug.tgz',
    'manifest.xml',
    'vm_test_results_*'
]

# AFDO common constants.
# How long does the AFDO_record autotest have to generate the AFDO perf data.
AFDO_GENERATE_TIMEOUT = 90 * 60

# Stats dashboard elastic search and statsd constants.
# Host and port information specified in topology.py.
ELASTIC_SEARCH_INDEX = 'metadata_index'
ELASTIC_SEARCH_USE_HTTP = False

STATSD_PROD_PREFIX = 'chromite'
STATSD_DEBUG_PREFIX = 'chromite_debug'

# Publication of Project SDK artifacts.
BRILLO_RELEASE_MANIFESTS_URL = 'gs://brillo-releases/sdk-releases'
BRILLO_LATEST_RELEASE_URL = os.path.join(BRILLO_RELEASE_MANIFESTS_URL,
                                         'LATEST')

# Gmail Credentials.
GMAIL_TOKEN_CACHE_FILE = os.path.expanduser('~/.gmail_credentials')
GMAIL_TOKEN_JSON_FILE = '/creds/refresh_tokens/chromeos_gmail_alerts'
