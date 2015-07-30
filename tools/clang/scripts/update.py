#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Windows can't run .sh files, so this is a Python implementation of
update.sh. This script should replace update.sh on all platforms eventually."""

import argparse
import os
import re
import shutil
import subprocess
import stat
import sys
import time

# Do NOT CHANGE this if you don't know what you're doing -- see
# https://code.google.com/p/chromium/wiki/UpdatingClang
# Reverting problematic clang rolls is safe, though.
# Note: this revision is only used for Windows. Other platforms use update.sh.
LLVM_WIN_REVISION = 'HEAD'

# ASan on Windows is useful enough to use it even while the clang/win is still
# in bringup. Use a pinned revision to make it slightly more stable.
use_head_revision = ('LLVM_FORCE_HEAD_REVISION' in os.environ or
  not re.search(r'\b(asan)=1', os.environ.get('GYP_DEFINES', '')))

if not use_head_revision:
  LLVM_WIN_REVISION = '235968'

# Path constants. (All of these should be absolute paths.)
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
LLVM_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm')
CHROME_TOOLS_SHIM_DIR = os.path.join(LLVM_DIR, 'tools', 'chrometools')
LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                              'Release+Asserts')
COMPILER_RT_BUILD_DIR = os.path.join(LLVM_BUILD_DIR, '32bit-compiler-rt')
CLANG_DIR = os.path.join(LLVM_DIR, 'tools', 'clang')
LLD_DIR = os.path.join(LLVM_DIR, 'tools', 'lld')
COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'projects', 'compiler-rt')
STAMP_FILE = os.path.join(LLVM_BUILD_DIR, 'cr_build_revision')
VERSION = '3.7.0'

LLVM_REPO_URL='https://llvm.org/svn/llvm-project'
if 'LLVM_REPO_URL' in os.environ:
  LLVM_REPO_URL = os.environ['LLVM_REPO_URL']


def ReadStampFile():
  """Return the contents of the stamp file, or '' if it doesn't exist."""
  try:
    with open(STAMP_FILE, 'r') as f:
      return f.read();
  except IOError:
    return ''


def WriteStampFile(s):
  """Write s to the stamp file."""
  if not os.path.exists(LLVM_BUILD_DIR):
    os.makedirs(LLVM_BUILD_DIR)
  with open(STAMP_FILE, 'w') as f:
    f.write(s)


def PrintRevision():
  """Print the current Clang revision."""
  # gyp runs update.py --print-revision even when clang isn't used.
  # It won't use the value, but we must not error.
  if not os.path.exists(LLVM_DIR):
    print "0"
    return

  # TODO(hans): This needs an update when we move to prebuilt Clang binaries.
  svn_info = subprocess.check_output(['svn', 'info', LLVM_DIR], shell=True)
  m = re.search(r'Revision: (\d+)', svn_info)
  assert m
  print m.group(1)


def RmTree(dir):
  """Delete dir."""
  def ChmodAndRetry(func, path, _):
    # Subversion can leave read-only files around.
    if not os.access(path, os.W_OK):
      os.chmod(path, stat.S_IWUSR)
      return func(path)
    raise

  shutil.rmtree(dir, onerror=ChmodAndRetry)


def RunCommand(command, fail_hard=True):
  """Run command and return success (True) or failure; or if fail_hard is
     True, exit on failure."""

  print 'Running %s' % (str(command))
  if subprocess.call(command, shell=True) == 0:
    return True
  print 'Failed.'
  if fail_hard:
    sys.exit(1)
  return False


def CopyFile(src, dst):
  """Copy a file from src to dst."""
  shutil.copy(src, dst)
  print "Copying %s to %s" % (src, dst)


def CopyDirectoryContents(src, dst, filename_filter=None):
  """Copy the files from directory src to dst
  with an optional filename filter."""
  if not os.path.exists(dst):
    os.makedirs(dst)
  for root, _, files in os.walk(src):
    for f in files:
      if filename_filter and not re.match(filename_filter, f):
        continue
      CopyFile(os.path.join(root, f), dst)


def Checkout(name, url, dir):
  """Checkout the SVN module at url into dir. Use name for the log message."""
  print "Checking out %s r%s into '%s'" % (name, LLVM_WIN_REVISION, dir)

  command = ['svn', 'checkout', '--force', url + '@' + LLVM_WIN_REVISION, dir]
  if RunCommand(command, fail_hard=False):
    return

  if os.path.isdir(dir):
    print "Removing %s." % (dir)
    RmTree(dir)

  print "Retrying."
  RunCommand(command)


def DeleteChromeToolsShim():
  shutil.rmtree(CHROME_TOOLS_SHIM_DIR, ignore_errors=True)


def CreateChromeToolsShim():
  """Hooks the Chrome tools into the LLVM build.

  Several Chrome tools have dependencies on LLVM/Clang libraries. The LLVM build
  detects implicit tools in the tools subdirectory, so this helper install a
  shim CMakeLists.txt that forwards to the real directory for the Chrome tools.

  Note that the shim directory name intentionally has no - or _. The implicit
  tool detection logic munges them in a weird way."""
  assert not any(i in os.path.basename(CHROME_TOOLS_SHIM_DIR) for i in '-_')
  os.mkdir(CHROME_TOOLS_SHIM_DIR)
  with file(os.path.join(CHROME_TOOLS_SHIM_DIR, 'CMakeLists.txt'), 'w') as f:
    f.write('# Automatically generated by tools/clang/scripts/update.py. ' +
            'Do not edit.\n')
    f.write('# Since tools/clang is located in another directory, use the \n')
    f.write('# two arg version to specify where build artifacts go. CMake\n')
    f.write('# disallows reuse of the same binary dir for multiple source\n')
    f.write('# dirs, so the build artifacts need to go into a subdirectory.\n')
    f.write('add_subdirectory(${CHROMIUM_TOOLS_SRC} ' +
            '${CMAKE_CURRENT_BINARY_DIR}/a)\n')


def AddCMakeToPath():
  """Look for CMake and add it to PATH if it's not there already."""
  try:
    # First check if cmake is already on PATH.
    subprocess.call(['cmake', '--version'])
    return
  except OSError as e:
    if e.errno != os.errno.ENOENT:
      raise

  cmake_dir = 'C:\\Program Files (x86)\\CMake\\bin'
  if os.path.isdir(cmake_dir):
    os.environ['PATH'] = os.environ.get('PATH', '') + os.pathsep + cmake_dir
    return
  print 'Failed to find CMake!'
  sys.exit(1)


vs_version = None
def GetVSVersion():
  global vs_version
  if vs_version:
    return vs_version

  # Try using the toolchain in depot_tools.
  # This sets environment variables used by SelectVisualStudioVersion below.
  sys.path.append(os.path.join(CHROMIUM_DIR, 'build'))
  import vs_toolchain
  vs_toolchain.SetEnvironmentAndGetRuntimeDllDirs()

  # Use gyp to find the MSVS installation, either in depot_tools as per above,
  # or a system-wide installation otherwise.
  sys.path.append(os.path.join(CHROMIUM_DIR, 'tools', 'gyp', 'pylib'))
  import gyp.MSVSVersion
  vs_version = gyp.MSVSVersion.SelectVisualStudioVersion('2013')
  return vs_version


def SubversionCmakeArg():
  # Since cmake's find_program can only find .exe and .com,
  # svn.bat in depot_tools will be ignored.
  default_pathext = ('.com', '.exe', '.bat', '.cmd')
  for path in os.environ.get('PATH', '').split(os.pathsep):
    for ext in default_pathext:
      candidate = os.path.join(path, 'svn' + ext)
      if os.path.isfile(candidate):
        return '-DSubversion_SVN_EXECUTABLE=%s' % candidate
  return ''


def UpdateClang(args):
  print 'Updating Clang to %s...' % (LLVM_WIN_REVISION)
  if LLVM_WIN_REVISION != 'HEAD' and ReadStampFile() == LLVM_WIN_REVISION:
    print 'Already up to date.'
    return 0

  AddCMakeToPath()
  # Reset the stamp file in case the build is unsuccessful.
  WriteStampFile('')

  DeleteChromeToolsShim();
  Checkout('LLVM', LLVM_REPO_URL + '/llvm/trunk', LLVM_DIR)
  Checkout('Clang', LLVM_REPO_URL + '/cfe/trunk', CLANG_DIR)
  Checkout('LLD', LLVM_REPO_URL + '/lld/trunk', LLD_DIR)
  Checkout('compiler-rt', LLVM_REPO_URL + '/compiler-rt/trunk', COMPILER_RT_DIR)
  CreateChromeToolsShim();

  if not os.path.exists(LLVM_BUILD_DIR):
    os.makedirs(LLVM_BUILD_DIR)
  os.chdir(LLVM_BUILD_DIR)

  # If building at head, define a macro that plugins can use for #ifdefing
  # out code that builds at head, but not at CLANG_REVISION or vice versa.
  cflags = cxxflags = ''

  # TODO(thakis): Set this only conditionally if use_head_revision once posix
  # and win clang are in sync. At the moment, the plugins only build at clang
  # head on posix, but they build at both head and the pinned win version :-/
  cflags += ' -DLLVM_FORCE_HEAD_REVISION'
  cxxflags += ' -DLLVM_FORCE_HEAD_REVISION'

  cmake_args = ['-GNinja', '-DCMAKE_BUILD_TYPE=Release',
                '-DLLVM_ENABLE_ASSERTIONS=ON', SubversionCmakeArg(),
                '-DCMAKE_C_FLAGS=' + cflags,
                '-DCMAKE_CXX_FLAGS=' + cxxflags,
                '-DCHROMIUM_TOOLS_SRC=%s' % os.path.join(
                    CHROMIUM_DIR, 'tools', 'clang'),
                '-DCHROMIUM_TOOLS=%s' % ';'.join(args.tools)]

  RunCommand(GetVSVersion().SetupScript('x64') +
             ['&&', 'cmake'] + cmake_args + [LLVM_DIR])
  RunCommand(GetVSVersion().SetupScript('x64') + ['&&', 'ninja', 'all'])

  # Do an x86 build of compiler-rt to get the 32-bit ASan run-time.
  # TODO(hans): Remove once the regular build above produces this.
  if not os.path.exists(COMPILER_RT_BUILD_DIR):
    os.makedirs(COMPILER_RT_BUILD_DIR)
  os.chdir(COMPILER_RT_BUILD_DIR)
  RunCommand(GetVSVersion().SetupScript('x86') +
             ['&&', 'cmake'] + cmake_args + [LLVM_DIR])
  RunCommand(GetVSVersion().SetupScript('x86') + ['&&', 'ninja', 'compiler-rt'])

  # TODO(hans): Make this (and the .gypi and .isolate files) version number
  # independent.
  asan_rt_lib_src_dir = os.path.join(COMPILER_RT_BUILD_DIR, 'lib', 'clang',
                                     VERSION, 'lib', 'windows')
  asan_rt_lib_dst_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                     VERSION, 'lib', 'windows')
  CopyDirectoryContents(asan_rt_lib_src_dir, asan_rt_lib_dst_dir,
                        r'^.*-i386\.lib$')
  CopyDirectoryContents(asan_rt_lib_src_dir, asan_rt_lib_dst_dir,
                        r'^.*-i386\.dll$')

  CopyFile(os.path.join(asan_rt_lib_src_dir, '..', '..', 'asan_blacklist.txt'),
           os.path.join(asan_rt_lib_dst_dir, '..', '..'))

  # Make an extra copy of the sanitizer headers, to be put on the include path
  # of the fallback compiler.
  sanitizer_include_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang', VERSION,
                                       'include', 'sanitizer')
  aux_sanitizer_include_dir = os.path.join(LLVM_BUILD_DIR, 'lib', 'clang',
                                           VERSION, 'include_sanitizer',
                                           'sanitizer')
  if not os.path.exists(aux_sanitizer_include_dir):
    os.makedirs(aux_sanitizer_include_dir)
  for _, _, files in os.walk(sanitizer_include_dir):
    for f in files:
      CopyFile(os.path.join(sanitizer_include_dir, f),
               aux_sanitizer_include_dir)

  if args.run_tests:
    os.chdir(LLVM_BUILD_DIR)
    RunCommand(GetVSVersion().SetupScript('x64') +
               ['&&', 'ninja', 'cr-check-all'])

  WriteStampFile(LLVM_WIN_REVISION)
  print 'Clang update was successful.'
  return 0


def main():
  if not sys.platform in ['win32', 'cygwin']:
    # For non-Windows, fall back to update.sh.
    # TODO(hans): Make update.py replace update.sh completely.

    # This script is called by gclient. gclient opens its hooks subprocesses
    # with (stdout=subprocess.PIPE, stderr=subprocess.STDOUT) and then does
    # custom output processing that breaks printing '\r' characters for
    # single-line updating status messages as printed by curl and wget.
    # Work around this by setting stderr of the update.sh process to stdin (!):
    # gclient doesn't redirect stdin, and while stdin itself is read-only, a
    # dup()ed sys.stdin is writable, try
    #   fd2 = os.dup(sys.stdin.fileno()); os.write(fd2, 'hi')
    # TODO: Fix gclient instead, http://crbug.com/95350
    try:
      stderr = os.fdopen(os.dup(sys.stdin.fileno()))
    except:
      stderr = sys.stderr
    return subprocess.call(
        [os.path.join(os.path.dirname(__file__), 'update.sh')] + sys.argv[1:],
        stderr=stderr)

  parser = argparse.ArgumentParser(description='Build Clang.')
  parser.add_argument('--tools', nargs='*',
                      default=['plugins', 'blink_gc_plugin'])
  # For now, this flag is only used for the non-Windows flow, but argparser gets
  # mad if it sees a flag it doesn't recognize.
  parser.add_argument('--if-needed', action='store_true')
  parser.add_argument('--print-revision', action='store_true')
  parser.add_argument('--run-tests', action='store_true')

  args = parser.parse_args()

  if args.print_revision:
    PrintRevision()
    return 0

  if not re.search(r'\b(clang|asan)=1', os.environ.get('GYP_DEFINES', '')):
    print 'Skipping Clang update (clang=1 was not set in GYP_DEFINES).'
    return 0

  if re.search(r'\b(make_clang_dir)=', os.environ.get('GYP_DEFINES', '')):
    print 'Skipping Clang update (make_clang_dir= was set in GYP_DEFINES).'
    return 0

  return UpdateClang(args)


if __name__ == '__main__':
  sys.exit(main())
