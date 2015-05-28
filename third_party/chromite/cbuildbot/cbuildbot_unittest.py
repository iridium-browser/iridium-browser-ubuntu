# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

from __future__ import print_function

import copy
import glob
import mox
import optparse
import os
import sys

from chromite.cbuildbot import cbuildbot_config as config
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot import manifest_version
from chromite.lib import cidb
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.scripts import cbuildbot


DEFAULT_CHROME_BRANCH = '27'


# pylint: disable=protected-access


class BuilderRunMock(partial_mock.PartialMock):
  """Partial mock for BuilderRun class."""

  TARGET = 'chromite.cbuildbot.cbuildbot_run._BuilderRunBase'
  ATTRS = ('GetVersionInfo', 'DetermineChromeVersion')
  VERSION = '3333.1.0'
  CHROME_VERSION = '35.0.1234.5'

  def GetVersionInfo(self, _build_root):
    return manifest_version.VersionInfo(
        version_string=self.VERSION, chrome_branch=DEFAULT_CHROME_BRANCH)

  def DetermineChromeVersion(self, _inst):
    return self.CHROME_VERSION


class TestExitedException(Exception):
  """Exception used by sys.exit() mock to halt execution."""


class TestHaltedException(Exception):
  """Exception used by mocks to halt execution without indicating failure."""


class TestFailedException(Exception):
  """Exception used by mocks to halt execution and indicate failure."""


class RunBuildStagesTest(cros_test_lib.MoxTempDirTestCase,
                         cros_test_lib.MockTestCase):
  """Test that cbuildbot runs the appropriate stages for a given config."""

  VERSION = '1234.5.6'

  def setUp(self):
    self.buildroot = os.path.join(self.tempdir, 'buildroot')
    osutils.SafeMakedirs(self.buildroot)
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-paladin'
    self.build_config = copy.deepcopy(config.config[self.bot_id])
    self.build_config['master'] = False
    self.build_config['important'] = False

    # Use the cbuildbot parser to create properties and populate default values.
    self.parser = cbuildbot._CreateParser()

    argv = ['-r', self.buildroot, '--buildbot', '--debug',
            'x86-generic-paladin']
    (self.options, _) = cbuildbot._ParseCommandLine(self.parser, argv)
    self.options.bootstrap = False
    self.options.clean = False
    self.options.resume = False
    self.options.sync = False
    self.options.build = False
    self.options.uprev = False
    self.options.tests = False
    self.options.archive = False
    self.options.remote_test_status = False
    self.options.patches = None
    self.options.prebuilts = False

    self._manager = cbuildbot.parallel.Manager()
    self._manager.__enter__()
    self.run = cbuildbot_run.BuilderRun(self.options, self.build_config,
                                        self._manager)

    self.StartPatcher(BuilderRunMock())

  def tearDown(self):
    # Mimic exiting a 'with' statement.
    self._manager.__exit__(None, None, None)

  def testChromeosOfficialSet(self):
    """Verify that CHROMEOS_OFFICIAL is set correctly."""
    self.build_config['chromeos_official'] = True

    # Clean up before
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

    cidb.CIDBConnectionFactory.SetupNoCidb()

    api = self.mox.CreateMock(cros_build_lib.CommandResult)
    api.returncode = 0
    api.output = constants.REEXEC_API_VERSION
    cros_build_lib.RunCommand(
        [constants.PATH_TO_CBUILDBOT, '--reexec-api-version'],
        cwd=self.buildroot, capture_output=True, error_code_ok=True
        ).AndReturn(api)

    result = self.mox.CreateMock(cros_build_lib.CommandResult)
    result.returncode = 0
    cros_build_lib.RunCommand(mox.IgnoreArg(), cwd=self.buildroot,
                              error_code_ok=True,
                              kill_timeout=mox.IgnoreArg()).AndReturn(result)
    self.mox.ReplayAll()

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    cbuildbot.SimpleBuilder(self.run).Run()

    self.assertTrue('CHROMEOS_OFFICIAL' in os.environ)

    self.mox.VerifyAll()

    # Clean up after the test
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

  def testChromeosOfficialNotSet(self):
    """Verify that CHROMEOS_OFFICIAL is not always set."""

    self.build_config['chromeos_official'] = False

    # Clean up before
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']

    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

    cidb.CIDBConnectionFactory.SetupNoCidb()

    api = self.mox.CreateMock(cros_build_lib.CommandResult)
    api.returncode = 0
    api.output = constants.REEXEC_API_VERSION
    cros_build_lib.RunCommand(
        [constants.PATH_TO_CBUILDBOT, '--reexec-api-version'],
        cwd=self.buildroot, capture_output=True, error_code_ok=True
        ).AndReturn(api)

    result = self.mox.CreateMock(cros_build_lib.CommandResult)
    result.returncode = 0
    cros_build_lib.RunCommand(mox.IgnoreArg(), cwd=self.buildroot,
                              error_code_ok=True,
                              kill_timeout=mox.IgnoreArg()).AndReturn(result)

    self.mox.ReplayAll()

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    cbuildbot.SimpleBuilder(self.run).Run()

    self.assertFalse('CHROMEOS_OFFICIAL' in os.environ)

    self.mox.VerifyAll()

    # Clean up after the test
    if 'CHROMEOS_OFFICIAL' in os.environ:
      del os.environ['CHROMEOS_OFFICIAL']


class SimpleBuilderTest(cros_test_lib.MockTempDirTestCase):
  """Tests for the main code paths in cbuildbot.SimpleBuilder"""

  def setUp(self):
    self.buildroot = os.path.join(self.tempdir, 'buildroot')
    chroot_path = os.path.join(self.buildroot, constants.DEFAULT_CHROOT_DIR)
    osutils.SafeMakedirs(os.path.join(chroot_path, 'tmp'))

    self.PatchObject(cbuildbot.Builder, '_RunStage')
    self.PatchObject(cbuildbot.SimpleBuilder, '_RunParallelStages')
    self.PatchObject(cbuildbot_run._BuilderRunBase, 'GetVersion',
                     return_value='R32-1234.0.0')
    self.StartPatcher(parallel_unittest.ParallelMock())

    self._manager = cbuildbot.parallel.Manager()
    self._manager.__enter__()

  def tearDown(self):
    # Mimic exiting a 'with' statement.
    self._manager.__exit__(None, None, None)

  def _initConfig(self, bot_id, extra_argv=None):
    """Return normal options/build_config for |bot_id|"""
    build_config = copy.deepcopy(config.config[bot_id])
    build_config['master'] = False
    build_config['important'] = False

    # Use the cbuildbot parser to create properties and populate default values.
    parser = cbuildbot._CreateParser()
    argv = (['-r', self.buildroot, '--buildbot', '--debug', '--nochromesdk'] +
            (extra_argv if extra_argv else []) + [bot_id])
    (options, _) = cbuildbot._ParseCommandLine(parser, argv)

    # Yikes.
    options.managed_chrome = build_config['sync_chrome']

    return cbuildbot_run.BuilderRun(options, build_config, self._manager)

  def testRunStagesPreCQ(self):
    """Verify RunStages for PRE_CQ_LAUNCHER_TYPE builders"""
    builder_run = self._initConfig('pre-cq-launcher')
    cbuildbot.SimpleBuilder(builder_run).RunStages()

  def testRunStagesBranchUtil(self):
    """Verify RunStages for CREATE_BRANCH_TYPE builders"""
    extra_argv = ['--branch-name', 'foo', '--version', '1234']
    builder_run = self._initConfig(constants.BRANCH_UTIL_CONFIG,
                                   extra_argv=extra_argv)
    cbuildbot.SimpleBuilder(builder_run).RunStages()

  def testRunStagesChrootBuilder(self):
    """Verify RunStages for CHROOT_BUILDER_TYPE builders"""
    builder_run = self._initConfig('chromiumos-sdk')
    cbuildbot.SimpleBuilder(builder_run).RunStages()

  def testRunStagesRefreshPackages(self):
    """Verify RunStages for REFRESH_PACKAGES_TYPE builders"""
    builder_run = self._initConfig('refresh-packages')
    cbuildbot.SimpleBuilder(builder_run).RunStages()

  def testRunStagesDefaultBuild(self):
    """Verify RunStages for standard board builders"""
    builder_run = self._initConfig('x86-generic-full')
    builder_run.attrs.chrome_version = 'TheChromeVersion'
    cbuildbot.SimpleBuilder(builder_run).RunStages()

  def testRunStagesDefaultBuildCompileCheck(self):
    """Verify RunStages for standard board builders (compile only)"""
    extra_argv = ['--compilecheck']
    builder_run = self._initConfig('x86-generic-full', extra_argv=extra_argv)
    builder_run.attrs.chrome_version = 'TheChromeVersion'
    cbuildbot.SimpleBuilder(builder_run).RunStages()

  def testRunStagesDefaultBuildHwTests(self):
    """Verify RunStages for boards w/hwtests"""
    extra_argv = ['--hwtest']
    builder_run = self._initConfig('lumpy-release', extra_argv=extra_argv)
    builder_run.attrs.chrome_version = 'TheChromeVersion'
    cbuildbot.SimpleBuilder(builder_run).RunStages()


class LogTest(cros_test_lib.TempDirTestCase):
  """Test logging functionality."""

  def _generateLogs(self, num):
    """Generates cbuildbot.log and num backups."""
    with open(os.path.join(self.tempdir, 'cbuildbot.log'), 'w') as f:
      f.write(str(num + 1))

    for i in range(1, num + 1):
      with open(os.path.join(self.tempdir, 'cbuildbot.log.' + str(i)),
                'w') as f:
        f.write(str(i))

  def testZeroToOneLogs(self):
    """Test beginning corner case."""
    self._generateLogs(0)
    cbuildbot._BackupPreviousLog(os.path.join(self.tempdir, 'cbuildbot.log'),
                                 backup_limit=25)
    with open(os.path.join(self.tempdir, 'cbuildbot.log.1')) as f:
      self.assertEquals(f.readline(), '1')

  def testNineToTenLogs(self):
    """Test handling *.log.9 to *.log.10 (correct sorting)."""
    self._generateLogs(9)
    cbuildbot._BackupPreviousLog(os.path.join(self.tempdir, 'cbuildbot.log'),
                                 backup_limit=25)
    with open(os.path.join(self.tempdir, 'cbuildbot.log.10')) as f:
      self.assertEquals(f.readline(), '10')

  def testOverLimit(self):
    """Test going over the limit and having to purge old logs."""
    self._generateLogs(25)
    cbuildbot._BackupPreviousLog(os.path.join(self.tempdir, 'cbuildbot.log'),
                                 backup_limit=25)
    with open(os.path.join(self.tempdir, 'cbuildbot.log.26')) as f:
      self.assertEquals(f.readline(), '26')

    self.assertEquals(len(glob.glob(os.path.join(self.tempdir, 'cbuildbot*'))),
                      25)


class InterfaceTest(cros_test_lib.MoxTestCase, cros_test_lib.LoggingTestCase):
  """Test the command line interface."""

  _X86_PREFLIGHT = 'x86-generic-paladin'
  _BUILD_ROOT = '/b/test_build1'

  def setUp(self):
    self.parser = cbuildbot._CreateParser()

  def assertDieSysExit(self, *args, **kwargs):
    self.assertRaises(cros_build_lib.DieSystemExit, *args, **kwargs)

  def testDepotTools(self):
    """Test that the entry point used by depot_tools works."""
    path = os.path.join(constants.SOURCE_ROOT, 'chromite', 'buildbot',
                        'cbuildbot')

    # Verify the tests below actually are testing correct behaviour;
    # specifically that it doesn't always just return 0.
    self.assertRaises(cros_build_lib.RunCommandError,
                      cros_build_lib.RunCommand,
                      ['cbuildbot', '--monkeys'], cwd=constants.SOURCE_ROOT)

    # Validate depot_tools lookup.
    cros_build_lib.RunCommand(
        ['cbuildbot', '--help'], cwd=constants.SOURCE_ROOT, capture_output=True)

    # Validate buildbot invocation pathway.
    cros_build_lib.RunCommand(
        [path, '--help'], cwd=constants.SOURCE_ROOT, capture_output=True)

  def testDebugBuildBotSetByDefault(self):
    """Test that debug and buildbot flags are set by default."""
    args = ['--local', '-r', self._BUILD_ROOT, self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.debug, True)
    self.assertEquals(options.buildbot, False)

  def testBuildBotOption(self):
    """Test that --buildbot option unsets debug flag."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.debug, False)
    self.assertEquals(options.buildbot, True)

  def testBuildBotWithDebugOption(self):
    """Test that --debug option overrides --buildbot option."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', '--debug',
            self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.debug, True)
    self.assertEquals(options.buildbot, True)

  def testLocalTrybotWithSpacesInPatches(self):
    """Test that we handle spaces in patch arguments."""
    args = ['-r', self._BUILD_ROOT, '--remote', '--local-patches',
            ' proj:br \t  proj2:b2 ',
            self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.local_patches, ['proj:br', 'proj2:b2'])

  def testBuildBotWithRemotePatches(self):
    """Test that --buildbot errors out with patches."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', '-g', '1234',
            self._X86_PREFLIGHT]
    self.assertDieSysExit(cbuildbot._ParseCommandLine, self.parser, args)

  def testRemoteBuildBotWithRemotePatches(self):
    """Test that --buildbot and --remote errors out with patches."""
    args = ['-r', self._BUILD_ROOT, '--buildbot', '--remote', '-g', '1234',
            self._X86_PREFLIGHT]
    self.assertDieSysExit(cbuildbot._ParseCommandLine, self.parser, args)

  def testBuildbotDebugWithPatches(self):
    """Test we can test patches with --buildbot --debug."""
    args = ['--remote', '-g', '1234', '--debug', '--buildbot',
            self._X86_PREFLIGHT]
    cbuildbot._ParseCommandLine(self.parser, args)

  def testBuildBotWithoutProfileOption(self):
    """Test that no --profile option gets defaulted."""
    args = ['--buildbot', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.profile, None)

  def testBuildBotWithProfileOption(self):
    """Test that --profile option gets parsed."""
    args = ['--buildbot', '--profile', 'carp', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.profile, 'carp')

  def testValidateClobberUserDeclines_1(self):
    """Test case where user declines in prompt."""
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cros_build_lib, 'GetInput')

    os.path.exists(self._BUILD_ROOT).AndReturn(True)
    cros_build_lib.GetInput(mox.IgnoreArg()).AndReturn('No')

    self.mox.ReplayAll()
    self.assertFalse(commands.ValidateClobber(self._BUILD_ROOT))
    self.mox.VerifyAll()

  def testValidateClobberUserDeclines_2(self):
    """Test case where user does not enter the full 'yes' pattern."""
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(cros_build_lib, 'GetInput')

    os.path.exists(self._BUILD_ROOT).AndReturn(True)
    cros_build_lib.GetInput(mox.IgnoreArg()).AndReturn('asdf')
    cros_build_lib.GetInput(mox.IgnoreArg()).AndReturn('No')

    self.mox.ReplayAll()
    self.assertFalse(commands.ValidateClobber(self._BUILD_ROOT))
    self.mox.VerifyAll()

  def testValidateClobberProtectRunningChromite(self):
    """User should not be clobbering our own source."""
    cwd = os.path.dirname(os.path.realpath(__file__))
    buildroot = os.path.dirname(cwd)
    self.assertDieSysExit(commands.ValidateClobber, buildroot)

  def testValidateClobberProtectRoot(self):
    """User should not be clobbering /"""
    self.assertDieSysExit(commands.ValidateClobber, '/')

  def testBuildBotWithBadChromeRevOption(self):
    """chrome_rev can't be passed an invalid option after chrome_root."""
    args = [
        '--local',
        '--buildroot=/tmp',
        '--chrome_root=.',
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        self._X86_PREFLIGHT,
    ]
    self.assertDieSysExit(cbuildbot._ParseCommandLine, self.parser, args)

  def testBuildBotWithBadChromeRootOption(self):
    """chrome_root can't get passed after non-local chrome_rev."""
    args = [
        '--local',
        '--buildroot=/tmp',
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        '--chrome_root=.',
        self._X86_PREFLIGHT,
    ]
    self.assertDieSysExit(cbuildbot._ParseCommandLine, self.parser, args)

  def testBuildBotWithBadChromeRevOptionLocal(self):
    """chrome_rev can't be local without chrome_root."""
    args = [
        '--local',
        '--buildroot=/tmp',
        '--chrome_rev=%s' % constants.CHROME_REV_LOCAL,
        self._X86_PREFLIGHT,
    ]
    self.assertDieSysExit(cbuildbot._ParseCommandLine, self.parser, args)

  def testBuildBotWithGoodChromeRootOption(self):
    """chrome_root can be set without chrome_rev."""
    args = [
        '--local',
        '--buildroot=/tmp',
        '--chrome_root=.',
        self._X86_PREFLIGHT,
    ]
    self.mox.ReplayAll()
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.mox.VerifyAll()
    self.assertEquals(options.chrome_rev, constants.CHROME_REV_LOCAL)
    self.assertNotEquals(options.chrome_root, None)

  def testBuildBotWithGoodChromeRevAndRootOption(self):
    """chrome_rev can get reset around chrome_root."""
    args = [
        '--local',
        '--buildroot=/tmp',
        '--chrome_rev=%s' % constants.CHROME_REV_LATEST,
        '--chrome_rev=%s' % constants.CHROME_REV_STICKY,
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        '--chrome_rev=%s' % constants.CHROME_REV_STICKY,
        '--chrome_rev=%s' % constants.CHROME_REV_LATEST,
        '--chrome_rev=%s' % constants.CHROME_REV_LOCAL,
        '--chrome_root=.',
        '--chrome_rev=%s' % constants.CHROME_REV_TOT,
        '--chrome_rev=%s' % constants.CHROME_REV_LOCAL,
        self._X86_PREFLIGHT,
    ]
    self.mox.ReplayAll()
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.mox.VerifyAll()
    self.assertEquals(options.chrome_rev, constants.CHROME_REV_LOCAL)
    self.assertNotEquals(options.chrome_root, None)

  def testPassThroughOptions(self):
    """Test we are building up pass-through list properly."""
    args = ['--remote', '-g', '1234', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)

    self.assertEquals(options.pass_through_args, ['-g', '1234'])

  def testDebugPassThrough(self):
    """Test we are passing --debug through."""
    args = ['--remote', '--debug', '--buildbot', self._X86_PREFLIGHT]
    (options, args) = cbuildbot._ParseCommandLine(self.parser, args)
    self.assertEquals(options.pass_through_args, ['--debug', '--buildbot'])

  def testCreateBranch(self):
    """Test a normal create branch run."""
    args = ['--branch-name', 'refs/heads/test', constants.BRANCH_UTIL_CONFIG]
    self.assertDieSysExit(cbuildbot._ParseCommandLine, self.parser, args)

  def testCreateBranchNoVersion(self):
    """Test we require --version with branch-util."""
    with cros_test_lib.LoggingCapturer('chromite') as logger:
      args = [constants.BRANCH_UTIL_CONFIG]
      self.assertDieSysExit(cbuildbot._ParseCommandLine, self.parser, args)
      self.AssertLogsContain(logger, '--branch-name')

  def testCreateBranchDelete(self):
    """Test we don't require --version with --delete."""
    args = ['--delete-branch', '--branch-name', 'refs/heads/test',
            constants.BRANCH_UTIL_CONFIG]
    cbuildbot._ParseCommandLine(self.parser, args)

  def testBranchOptionsWithoutBranchConfig(self):
    """Error out when branch options passed in without branch-util config."""
    for extra_args in [['--delete-branch'],
                       ['--branch-name', 'refs/heads/test'],
                       ['--rename-to', 'abc']]:
      with cros_test_lib.LoggingCapturer('chromite') as logger:
        args = [self._X86_PREFLIGHT] + extra_args
        self.assertDieSysExit(cbuildbot._ParseCommandLine, self.parser, args)
        self.AssertLogsContain(logger, 'Cannot specify')


class FullInterfaceTest(cros_test_lib.MoxTempDirTestCase):
  """Tests that run the cbuildbot.main() function directly.

  Note this explicitly suppresses automatic VerifyAll() calls; thus if you want
  that checked, you have to invoke it yourself.
  """

  mox_suppress_verify_all = True

  def MakeTestRootDir(self, relpath):
    abspath = os.path.join(self.root, relpath)
    os.makedirs(abspath)
    return abspath

  def setUp(self):
    self.root = self.tempdir
    self.buildroot = self.MakeTestRootDir('build_root')
    self.sourceroot = self.MakeTestRootDir('source_root')
    self.trybot_root = self.MakeTestRootDir('trybot')
    self.trybot_internal_root = self.MakeTestRootDir('trybot-internal')
    self.external_marker = os.path.join(self.trybot_root, '.trybot')
    self.internal_marker = os.path.join(self.trybot_internal_root, '.trybot')

    os.makedirs(os.path.join(self.sourceroot, '.repo', 'manifests'))
    os.makedirs(os.path.join(self.sourceroot, '.repo', 'repo'))

    # Create the parser before we stub out os.path.exists() - which the parser
    # creation code actually uses.
    parser = cbuildbot._CreateParser()

    # Stub out all relevant methods regardless of whether they are called in the
    # specific test case.  We can do this because we don't run VerifyAll() at
    # the end of every test.
    self.mox.StubOutWithMock(optparse.OptionParser, 'error')
    self.mox.StubOutWithMock(cros_build_lib, 'IsInsideChroot')
    self.mox.StubOutWithMock(cbuildbot, '_CreateParser')
    self.mox.StubOutWithMock(sys, 'exit')
    self.mox.StubOutWithMock(cros_build_lib, 'GetInput')
    self.mox.StubOutWithMock(cbuildbot, '_RunBuildStagesWrapper')

    parser.error(mox.IgnoreArg()).InAnyOrder().AndRaise(TestExitedException())
    cros_build_lib.IsInsideChroot().InAnyOrder().AndReturn(False)
    cbuildbot._CreateParser().InAnyOrder().AndReturn(parser)
    sys.exit(mox.IgnoreArg()).InAnyOrder().AndRaise(TestExitedException())
    cbuildbot._RunBuildStagesWrapper(
        mox.IgnoreArg(),
        mox.IgnoreArg()).InAnyOrder().AndReturn(True)

  def assertMain(self, args, common_options=True):
    if common_options:
      # Suppress cgroups code.  For cbuildbot invocation, it doesn't hugely
      # care about cgroups- that's a blackbox to it.  As such these unittests
      # should not be sensitive to it.
      args.extend(['--sourceroot', self.sourceroot, '--nocgroups',
                   '--notee'])
    return cbuildbot.main(args)

  def testNullArgsStripped(self):
    """Test that null args are stripped out and don't cause error."""
    self.mox.ReplayAll()
    self.assertMain(['--local', '-r', self.buildroot, '', '',
                     'x86-generic-paladin'])

  def testMultipleConfigsError(self):
    """Test that multiple configs cause error if --remote is not used."""
    self.mox.ReplayAll()
    self.assertRaises(cros_build_lib.DieSystemExit, self.assertMain,
                      ['--local',
                       '-r', self.buildroot,
                       'arm-generic-paladin',
                       'x86-generic-paladin'])

  def testDontInferBuildrootForBuildBotRuns(self):
    """Test that we don't infer buildroot if run with --buildbot option."""
    self.mox.ReplayAll()
    self.assertRaises(TestExitedException, self.assertMain,
                      ['--buildbot', 'x86-generic-paladin'])

  def testInferExternalBuildRoot(self):
    """Test that we default to correct buildroot for external config."""
    self.mox.StubOutWithMock(cbuildbot, '_ConfirmBuildRoot')
    cbuildbot._ConfirmBuildRoot(mox.IgnoreArg()).InAnyOrder().AndRaise(
        TestHaltedException())

    self.mox.ReplayAll()
    self.assertRaises(TestHaltedException, self.assertMain,
                      ['--local', 'x86-generic-paladin'])

  def testInferInternalBuildRoot(self):
    """Test that we default to correct buildroot for internal config."""
    self.mox.StubOutWithMock(cbuildbot, '_ConfirmBuildRoot')
    cbuildbot._ConfirmBuildRoot(mox.IgnoreArg()).InAnyOrder().AndRaise(
        TestHaltedException())

    self.mox.ReplayAll()
    self.assertRaises(TestHaltedException, self.assertMain,
                      ['--local', 'x86-mario-paladin'])

  def testInferBuildRootPromptNo(self):
    """Test that a 'no' answer on the prompt halts execution."""
    cros_build_lib.GetInput(mox.IgnoreArg()).InAnyOrder().AndReturn('no')

    self.mox.ReplayAll()
    self.assertRaises(TestExitedException, self.assertMain,
                      ['--local', 'x86-generic-paladin'])

  def testInferBuildRootExists(self):
    """Test that we don't prompt the user if buildroot already exists."""
    cros_build_lib.RunCommand(['touch', self.external_marker],
                              capture_output=True)
    os.utime(self.external_marker, None)
    cros_build_lib.GetInput(mox.IgnoreArg()).InAnyOrder().AndRaise(
        TestFailedException())

    self.mox.ReplayAll()
    self.assertMain(['--local', 'x86-generic-paladin'])

  def testBuildbotDiesInChroot(self):
    """Buildbot should quit if run inside a chroot."""
    # Need to do this since a cros_build_lib.IsInsideChroot() call is already
    # queued up in setup() and we can't Reset() an individual mock.
    # pylint: disable=not-callable
    new_is_inside_chroot = self.mox.CreateMockAnything()
    new_is_inside_chroot().InAnyOrder().AndReturn(True)
    cros_build_lib.IsInsideChroot = new_is_inside_chroot
    self.mox.ReplayAll()
    self.assertRaises(cros_build_lib.DieSystemExit, self.assertMain,
                      ['--local', '-r', self.buildroot, 'x86-generic-paladin'])
