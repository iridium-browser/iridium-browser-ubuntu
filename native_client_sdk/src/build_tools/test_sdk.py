#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for a testing an existing SDK.

This script is normally run immediately after build_sdk.py.
"""

import argparse
import os
import subprocess
import sys

import buildbot_common
import build_projects
import build_sdk
import build_version
import parse_dsc

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_LIBRARY_DIR = os.path.join(SDK_SRC_DIR, 'libraries')
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
OUT_DIR = os.path.join(SRC_DIR, 'out')

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import getos

def StepBuildExamples(pepperdir):
  for config in ('Debug', 'Release'):
    build_sdk.BuildStepMakeAll(pepperdir, 'getting_started',
                               'Build Getting Started (%s)' % config,
                               deps=False, config=config)

    build_sdk.BuildStepMakeAll(pepperdir, 'examples',
                               'Build Examples (%s)' % config,
                               deps=False, config=config)


def StepCopyTests(pepperdir, toolchains, build_experimental):
  buildbot_common.BuildStep('Copy Tests')

  # Update test libraries and test apps
  filters = {
    'DEST': ['tests']
  }
  if not build_experimental:
    filters['EXPERIMENTAL'] = False

  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, include=filters)
  build_projects.UpdateHelpers(pepperdir, clobber=False)
  build_projects.UpdateProjects(pepperdir, tree, clobber=False,
                                toolchains=toolchains)


def StepBuildTests(pepperdir):
  for config in ('Debug', 'Release'):
    build_sdk.BuildStepMakeAll(pepperdir, 'tests',
                                   'Build Tests (%s)' % config,
                                   deps=False, config=config)


def StepRunSelLdrTests(pepperdir, sanitizer):
  filters = {
    'SEL_LDR': True
  }

  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, include=filters)

  def RunTest(test, toolchain, config, arch=None):
    args = ['STANDALONE=1', 'TOOLCHAIN=%s' % toolchain]
    if arch is not None:
      args.append('NACL_ARCH=%s' % arch)
    deps = False

    if sanitizer is not None:
      # For sanitizer builds we pass extra argument for make, and do
      # full clean build to make sure everything is rebuilt with the
      # correct flags
      deps = True
      if sanitizer == 'valgrind':
        args += ['RUN_UNDER=valgrind']
      elif sanitizer == 'address':
        args += ['ASAN=1']
      elif sanitizer == 'thread':
        args += ['TSAN=1']

    build_projects.BuildProjectsBranch(pepperdir, test, clean=False,
                                       deps=deps, config=config,
                                       args=args + ['run'])

  if getos.GetPlatform() == 'win':
    # On win32 we only support running on the system
    # arch
    archs = (getos.GetSystemArch('win'),)
  elif getos.GetPlatform() == 'mac':
    # We only ship 32-bit version of sel_ldr on mac.
    archs = ('x86_32',)
  else:
    # On linux we can run both 32 and 64-bit, and arm (via qemu)
    archs = ('x86_64', 'x86_32', 'arm')

  for root, projects in tree.iteritems():
    for project in projects:
      if sanitizer:
        sanitizer_name = '[sanitizer=%s]'  % sanitizer
      else:
        sanitizer_name = ''
      title = 'standalone test%s: %s' % (sanitizer_name,
                                         os.path.basename(project['NAME']))
      location = os.path.join(root, project['NAME'])
      buildbot_common.BuildStep(title)
      configs = ('Debug', 'Release')

      # On linux we can run the standalone tests natively using the host
      # compiler.
      if getos.GetPlatform() == 'linux':
        if sanitizer:
          configs = ('Debug',)
        for config in configs:
          RunTest(location, 'linux', config)

      if sanitizer:
        continue

      for toolchain in ('clang-newlib', 'newlib', 'glibc', 'pnacl'):
        for arch in archs:
          # TODO(sbc): Remove this once we get elf_loader.nexe added to the SDK
          if toolchain == 'glibc' and arch == 'arm':
            continue
          for config in configs:
            RunTest(location, toolchain, config, arch)


def StepRunBrowserTests(toolchains, experimental):
  buildbot_common.BuildStep('Run Tests')

  args = [
    sys.executable,
    os.path.join(SCRIPT_DIR, 'test_projects.py'),
    '--retry-times=3',
  ]

  if experimental:
    args.append('-x')
  for toolchain in toolchains:
    args.extend(['-t', toolchain])

  try:
    subprocess.check_call(args)
  except subprocess.CalledProcessError:
    buildbot_common.ErrorExit('Error running tests.')


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--experimental', help='build experimental tests',
                      action='store_true')
  parser.add_argument('--sanitizer',
                      help='Run sanitizer (asan/tsan/valgrind) tests',
                      action='store_true')
  parser.add_argument('--verbose', '-v', help='Verbose output',
                      action='store_true')
  parser.add_argument('phases', nargs="*")

  if 'NACL_SDK_ROOT' in os.environ:
    # We don't want the currently configured NACL_SDK_ROOT to have any effect
    # of the build.
    del os.environ['NACL_SDK_ROOT']

  # To setup bash completion for this command first install optcomplete
  # and then add this line to your .bashrc:
  #  complete -F _optcomplete test_sdk.py
  try:
    import optcomplete
    optcomplete.autocomplete(parser)
  except ImportError:
    pass

  options = parser.parse_args(args)

  pepper_ver = str(int(build_version.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  toolchains = ['clang-newlib', 'newlib', 'glibc', 'pnacl']
  toolchains.append(getos.GetPlatform())

  if options.verbose:
    build_projects.verbose = True

  phases = [
    ('build_examples', StepBuildExamples, pepperdir),
    ('copy_tests', StepCopyTests, pepperdir, toolchains, options.experimental),
    ('build_tests', StepBuildTests, pepperdir),
    ('sel_ldr_tests', StepRunSelLdrTests, pepperdir, None),
    ('browser_tests', StepRunBrowserTests, toolchains, options.experimental),
  ]

  if options.sanitizer:
    if getos.GetPlatform() != 'linux':
      buildbot_common.ErrorExit('sanitizer tests only run on linux.')
    phases += [
      ('sel_ldr_tests_asan', StepRunSelLdrTests, pepperdir, 'address'),
      ('sel_ldr_tests_tsan', StepRunSelLdrTests, pepperdir, 'thread'),
      ('sel_ldr_tests_valgrind', StepRunSelLdrTests, pepperdir, 'valgrind')
    ]

  if options.phases:
    phase_names = [p[0] for p in phases]
    for arg in options.phases:
      if arg not in phase_names:
        msg = 'Invalid argument: %s\n' % arg
        msg += 'Possible arguments:\n'
        for name in phase_names:
          msg += '   %s\n' % name
        parser.error(msg.strip())

  for phase in phases:
    phase_name = phase[0]
    if options.phases and phase_name not in options.phases:
      continue
    phase_func = phase[1]
    phase_args = phase[2:]
    phase_func(*phase_args)

  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('test_sdk: interrupted')
