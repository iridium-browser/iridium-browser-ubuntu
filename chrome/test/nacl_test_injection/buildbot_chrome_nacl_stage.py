#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Do all the steps required to build and test against nacl."""


import optparse
import os.path
import re
import shutil
import subprocess
import sys

import find_chrome

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
NACL_DIR = os.path.join(CHROMIUM_DIR, 'native_client')

sys.path.append(os.path.join(CHROMIUM_DIR, 'build'))
sys.path.append(NACL_DIR)
import detect_host_arch
import pynacl.platform


# Copied from buildbot/buildbot_lib.py
def TryToCleanContents(path, file_name_filter=lambda fn: True):
  """
  Remove the contents of a directory without touching the directory itself.
  Ignores all failures.
  """
  if os.path.exists(path):
    for fn in os.listdir(path):
      TryToCleanPath(os.path.join(path, fn), file_name_filter)


# Copied from buildbot/buildbot_lib.py
def TryToCleanPath(path, file_name_filter=lambda fn: True):
  """
  Removes a file or directory.
  Ignores all failures.
  """
  if os.path.exists(path):
    if file_name_filter(path):
      print 'Trying to remove %s' % path
      if os.path.isdir(path):
        shutil.rmtree(path, ignore_errors=True)
      else:
        try:
          os.remove(path)
        except Exception:
          pass
    else:
      print 'Skipping %s' % path


# TODO(ncbray): this is somewhat unsafe.  We should fix the underlying problem.
def CleanTempDir():
  # Only delete files and directories like:
  # a) C:\temp\83C4.tmp
  # b) /tmp/.org.chromium.Chromium.EQrEzl
  file_name_re = re.compile(
      r'[\\/]([0-9a-fA-F]+\.tmp|\.org\.chrom\w+\.Chrom\w+\..+)$')
  file_name_filter = lambda fn: file_name_re.search(fn) is not None

  path = os.environ.get('TMP', os.environ.get('TEMP', '/tmp'))
  if len(path) >= 4 and os.path.isdir(path):
    print
    print "Cleaning out the temp directory."
    print
    TryToCleanContents(path, file_name_filter)
  else:
    print
    print "Cannot find temp directory, not cleaning it."
    print


def RunCommand(cmd, cwd, env):
  sys.stdout.write('\nRunning %s\n\n' % ' '.join(cmd))
  sys.stdout.flush()
  retcode = subprocess.call(cmd, cwd=cwd, env=env)
  if retcode != 0:
    sys.stdout.write('\nFailed: %s\n\n' % ' '.join(cmd))
    sys.exit(retcode)


def RunTests(name, cmd, env):
  sys.stdout.write('\n\nBuilding files needed for %s testing...\n\n' % name)
  RunCommand(cmd + ['do_not_run_tests=1', '-j8'], NACL_DIR, env)
  sys.stdout.write('\n\nRunning %s tests...\n\n' % name)
  RunCommand(cmd, NACL_DIR, env)


def BuildAndTest(options):
  # Refuse to run under cygwin.
  if sys.platform == 'cygwin':
    raise Exception('I do not work under cygwin, sorry.')

  # By default, use the version of Python is being used to run this script.
  python = sys.executable
  if sys.platform == 'darwin':
    # Mac 10.5 bots tend to use a particularlly old version of Python, look for
    # a newer version.
    macpython27 = '/Library/Frameworks/Python.framework/Versions/2.7/bin/python'
    if os.path.exists(macpython27):
      python = macpython27


  os_name = pynacl.platform.GetOS()
  arch_name = pynacl.platform.GetArch()
  toolchain_dir = os.path.join(NACL_DIR, 'toolchain',
                               '%s_%s' % (os_name, arch_name))
  nacl_newlib_dir = os.path.join(toolchain_dir, 'nacl_%s_newlib' % arch_name)
  nacl_glibc_dir = os.path.join(toolchain_dir, 'nacl_%s_glibc' % arch_name)
  pnacl_newlib_dir = os.path.join(toolchain_dir, 'pnacl_newlib')

  # Decide platform specifics.
  if options.browser_path:
    chrome_filename = options.browser_path
  else:
    chrome_filename = find_chrome.FindChrome(CHROMIUM_DIR, [options.mode])
    if chrome_filename is None:
      raise Exception('Cannot find a chrome binary - specify one with '
                      '--browser_path?')

  env = dict(os.environ)
  if sys.platform in ['win32', 'cygwin']:
    if options.bits == 64:
      bits = 64
    elif options.bits == 32:
      bits = 32
    elif '64' in os.environ.get('PROCESSOR_ARCHITECTURE', '') or \
         '64' in os.environ.get('PROCESSOR_ARCHITEW6432', ''):
      bits = 64
    else:
      bits = 32
    msvs_path = ';'.join([
        r'c:\Program Files\Microsoft Visual Studio 9.0\VC',
        r'c:\Program Files (x86)\Microsoft Visual Studio 9.0\VC',
        r'c:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools',
        r'c:\Program Files (x86)\Microsoft Visual Studio 9.0\Common7\Tools',
        r'c:\Program Files\Microsoft Visual Studio 8\VC',
        r'c:\Program Files (x86)\Microsoft Visual Studio 8\VC',
        r'c:\Program Files\Microsoft Visual Studio 8\Common7\Tools',
        r'c:\Program Files (x86)\Microsoft Visual Studio 8\Common7\Tools',
    ])
    env['PATH'] += ';' + msvs_path
    scons = [python, 'scons.py']
  elif sys.platform == 'darwin':
    if options.bits == 64:
      bits = 64
    elif options.bits == 32:
      bits = 32
    else:
      p = subprocess.Popen(['file', chrome_filename], stdout=subprocess.PIPE)
      (p_stdout, _) = p.communicate()
      assert p.returncode == 0
      if p_stdout.find('executable x86_64') >= 0:
        bits = 64
      else:
        bits = 32
    scons = [python, 'scons.py']
  else:
    if options.bits == 64:
      bits = 64
    elif options.bits == 32:
      bits = 32
    elif '64' in detect_host_arch.HostArch():
      bits = 64
    else:
      bits = 32
    # xvfb-run has a 2-second overhead per invocation, so it is cheaper to wrap
    # the entire build step rather than each test (browser_headless=1).
    # We also need to make sure that there are at least 24 bits per pixel.
    # https://code.google.com/p/chromium/issues/detail?id=316687
    scons = [
        'xvfb-run',
        '--auto-servernum',
        '--server-args', '-screen 0 1024x768x24',
        python, 'scons.py',
    ]

  if options.jobs > 1:
    scons.append('-j%d' % options.jobs)

  scons.append('disable_tests=%s' % options.disable_tests)

  if options.buildbot is not None:
    scons.append('buildbot=%s' % (options.buildbot,))

  # Clean the output of the previous build.
  # Incremental builds can get wedged in weird ways, so we're trading speed
  # for reliability.
  shutil.rmtree(os.path.join(NACL_DIR, 'scons-out'), True)

  # check that the HOST (not target) is 64bit
  # this is emulating what msvs_env.bat is doing
  if '64' in os.environ.get('PROCESSOR_ARCHITECTURE', '') or \
     '64' in os.environ.get('PROCESSOR_ARCHITEW6432', ''):
    # 64bit HOST
    env['VS90COMNTOOLS'] = ('c:\\Program Files (x86)\\'
                            'Microsoft Visual Studio 9.0\\Common7\\Tools\\')
    env['VS80COMNTOOLS'] = ('c:\\Program Files (x86)\\'
                            'Microsoft Visual Studio 8.0\\Common7\\Tools\\')
  else:
    # 32bit HOST
    env['VS90COMNTOOLS'] = ('c:\\Program Files\\Microsoft Visual Studio 9.0\\'
                            'Common7\\Tools\\')
    env['VS80COMNTOOLS'] = ('c:\\Program Files\\Microsoft Visual Studio 8.0\\'
                            'Common7\\Tools\\')

  # Run nacl/chrome integration tests.
  # Note that we have to add nacl_irt_test to --mode in order to get
  # inbrowser_test_runner to run.
  # TODO(mseaborn): Change it so that inbrowser_test_runner is not a
  # special case.
  cmd = scons + ['--verbose', '-k', 'platform=x86-%d' % bits,
      '--mode=opt-host,nacl,nacl_irt_test',
      'chrome_browser_path=%s' % chrome_filename,
      'nacl_newlib_dir=%s' % nacl_newlib_dir,
      'nacl_glibc_dir=%s' % nacl_glibc_dir,
      'pnacl_newlib_dir=%s' % pnacl_newlib_dir,
  ]
  if not options.integration_bot and not options.morenacl_bot:
    cmd.append('disable_flaky_tests=1')
  cmd.append('chrome_browser_tests')

  # Propagate path to JSON output if present.
  # Note that RunCommand calls sys.exit on errors, so potential errors
  # from one command won't be overwritten by another one. Overwriting
  # a successful results file with either success or failure is fine.
  if options.json_build_results_output_file:
    cmd.append('json_build_results_output_file=%s' %
               options.json_build_results_output_file)

  CleanTempDir()

  if options.enable_newlib:
    RunTests('nacl-newlib', cmd, env)

  if options.enable_glibc:
    RunTests('nacl-glibc', cmd + ['--nacl_glibc'], env)


def MakeCommandLineParser():
  parser = optparse.OptionParser()
  parser.add_option('-m', '--mode', dest='mode', default='Debug',
                    help='Debug/Release mode')
  parser.add_option('-j', dest='jobs', default=1, type='int',
                    help='Number of parallel jobs')

  parser.add_option('--enable_newlib', dest='enable_newlib', default=-1,
                    type='int', help='Run newlib tests?')
  parser.add_option('--enable_glibc', dest='enable_glibc', default=-1,
                    type='int', help='Run glibc tests?')

  parser.add_option('--json_build_results_output_file',
                    help='Path to a JSON file for machine-readable output.')

  # Deprecated, but passed to us by a script in the Chrome repo.
  # Replaced by --enable_glibc=0
  parser.add_option('--disable_glibc', dest='disable_glibc',
                    action='store_true', default=False,
                    help='Do not test using glibc.')

  parser.add_option('--disable_tests', dest='disable_tests',
                    type='string', default='',
                    help='Comma-separated list of tests to omit')
  builder_name = os.environ.get('BUILDBOT_BUILDERNAME', '')
  is_integration_bot = 'nacl-chrome' in builder_name
  parser.add_option('--integration_bot', dest='integration_bot',
                    type='int', default=int(is_integration_bot),
                    help='Is this an integration bot?')
  is_morenacl_bot = (
      'More NaCl' in builder_name or
      'naclmore' in builder_name)
  parser.add_option('--morenacl_bot', dest='morenacl_bot',
                    type='int', default=int(is_morenacl_bot),
                    help='Is this a morenacl bot?')

  # Not used on the bots, but handy for running the script manually.
  parser.add_option('--bits', dest='bits', action='store',
                    type='int', default=None,
                    help='32/64')
  parser.add_option('--browser_path', dest='browser_path', action='store',
                    type='string', default=None,
                    help='Path to the chrome browser.')
  parser.add_option('--buildbot', dest='buildbot', action='store',
                    type='string', default=None,
                    help='Value passed to scons as buildbot= option.')
  return parser


def Main():
  parser = MakeCommandLineParser()
  options, args = parser.parse_args()
  if options.integration_bot and options.morenacl_bot:
    parser.error('ERROR: cannot be both an integration bot and a morenacl bot')

  # Set defaults for enabling newlib.
  if options.enable_newlib == -1:
    options.enable_newlib = 1

  # Set defaults for enabling glibc.
  if options.enable_glibc == -1:
    if options.integration_bot or options.morenacl_bot:
      options.enable_glibc = 1
    else:
      options.enable_glibc = 0

  if args:
    parser.error('ERROR: invalid argument')
  BuildAndTest(options)


if __name__ == '__main__':
  Main()
