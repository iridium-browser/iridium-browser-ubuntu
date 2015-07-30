#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script for launching application within the sel_ldr.
"""

import argparse
import os
import subprocess
import sys

import create_nmf
import getos

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_SDK_ROOT = os.path.dirname(SCRIPT_DIR)

if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)


class Error(Exception):
  pass


def Log(msg):
  if Log.verbose:
    sys.stderr.write(str(msg) + '\n')
Log.verbose = False


def FindQemu():
  qemu_locations = [os.path.join(SCRIPT_DIR, 'qemu_arm'),
                    os.path.join(SCRIPT_DIR, 'qemu-arm')]
  qemu_locations += [os.path.join(path, 'qemu_arm')
                     for path in os.environ["PATH"].split(os.pathsep)]
  qemu_locations += [os.path.join(path, 'qemu-arm')
                     for path in os.environ["PATH"].split(os.pathsep)]
  # See if qemu is in any of these locations.
  qemu_bin = None
  for loc in qemu_locations:
    if os.path.isfile(loc) and os.access(loc, os.X_OK):
      qemu_bin = loc
      break
  return qemu_bin


def main(argv):
  epilog = 'Example: sel_ldr.py my_nexe.nexe'
  parser = argparse.ArgumentParser(description=__doc__, epilog=epilog)
  parser.add_argument('-v', '--verbose', action='store_true',
                      help='Verbose output')
  parser.add_argument('-d', '--debug', action='store_true',
                      help='Enable debug stub')
  parser.add_argument('--debug-libs', action='store_true',
                      help='For dynamic executables, reference debug '
                           'libraries rather then release')
  parser.add_argument('executable', help='executable (.nexe) to run')
  parser.add_argument('args', nargs='*', help='argument to pass to exectuable')

  # To enable bash completion for this command first install optcomplete
  # and then add this line to your .bashrc:
  #  complete -F _optcomplete sel_ldr.py
  try:
    import optcomplete
    optcomplete.autocomplete(parser)
  except ImportError:
    pass

  options = parser.parse_args(argv)

  if options.verbose:
    Log.verbose = True

  osname = getos.GetPlatform()
  if not os.path.exists(options.executable):
    raise Error('executable not found: %s' % options.executable)
  if not os.path.isfile(options.executable):
    raise Error('not a file: %s' % options.executable)

  arch, dynamic = create_nmf.ParseElfHeader(options.executable)

  if arch == 'arm' and osname != 'linux':
    raise Error('Cannot run ARM executables under sel_ldr on ' + osname)

  arch_suffix = arch.replace('-', '_')

  sel_ldr = os.path.join(SCRIPT_DIR, 'sel_ldr_%s' % arch_suffix)
  irt = os.path.join(SCRIPT_DIR, 'irt_core_%s.nexe' % arch_suffix)
  if osname == 'win':
    sel_ldr += '.exe'
  Log('ROOT    = %s' % NACL_SDK_ROOT)
  Log('SEL_LDR = %s' % sel_ldr)
  Log('IRT     = %s' % irt)
  cmd = [sel_ldr]

  if osname == 'linux':
    # Run sel_ldr under nacl_helper_bootstrap
    helper = os.path.join(SCRIPT_DIR, 'nacl_helper_bootstrap_%s' % arch_suffix)
    Log('HELPER  = %s' % helper)
    cmd.insert(0, helper)
    cmd.append('--r_debug=0xXXXXXXXXXXXXXXXX')
    cmd.append('--reserved_at_zero=0xXXXXXXXXXXXXXXXX')

  cmd += ['-a', '-B', irt]

  if options.debug:
    cmd.append('-g')

  if not options.verbose:
    cmd += ['-l', os.devnull]

  if arch == 'arm':
    # Use the QEMU arm emulator if available.
    qemu_bin = FindQemu()
    if not qemu_bin:
      raise Error('Cannot run ARM executables under sel_ldr without an emulator'
          '. Try installing QEMU (http://wiki.qemu.org/).')

    arm_libpath = os.path.join(NACL_SDK_ROOT, 'tools', 'lib', 'arm_trusted')
    if not os.path.isdir(arm_libpath):
      raise Error('Could not find ARM library path: %s' % arm_libpath)
    qemu = [qemu_bin, '-cpu', 'cortex-a8', '-L', arm_libpath]
    # '-Q' disables platform qualification, allowing arm binaries to run.
    cmd = qemu + cmd + ['-Q']

  if dynamic:
    if options.debug_libs:
      libpath = os.path.join(NACL_SDK_ROOT, 'lib',
                            'glibc_%s' % arch_suffix, 'Debug')
    else:
      libpath = os.path.join(NACL_SDK_ROOT, 'lib',
                            'glibc_%s' % arch_suffix, 'Release')
    toolchain = '%s_x86_glibc' % osname
    sdk_lib_dir = os.path.join(NACL_SDK_ROOT, 'toolchain',
                               toolchain, 'x86_64-nacl')
    if arch == 'x86-64':
      sdk_lib_dir = os.path.join(sdk_lib_dir, 'lib')
    else:
      sdk_lib_dir = os.path.join(sdk_lib_dir, 'lib32')
    ldso = os.path.join(sdk_lib_dir, 'runnable-ld.so')
    cmd.append(ldso)
    Log('LD.SO = %s' % ldso)
    libpath += ':' + sdk_lib_dir
    cmd.append('--library-path')
    cmd.append(libpath)


  # Append arguments for the executable itself.
  cmd.append(options.executable)
  cmd += options.args

  Log(cmd)
  return subprocess.call(cmd)


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)
