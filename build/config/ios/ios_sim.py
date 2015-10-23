#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import errno
import os
import subprocess
import sys
import re

SIMCTL_PATH = [
  '/usr/bin/env',
  'xcrun',
  'simctl',
]

PLIST_BUDDY_PATH = [
  '/usr/bin/env',
  'xcrun',
  'PlistBuddy',
]


def ApplicationIdentifier(path):
  identifier = subprocess.check_output(PLIST_BUDDY_PATH + [
    '-c',
    'Print CFBundleIdentifier',
    '%s/Info.plist' % path,
  ])
  return identifier.strip()


def Install(args):
  return subprocess.check_call(SIMCTL_PATH + [
    'install',
    'booted',
    args.path,
  ])


def InstallLaunchAndWait(args, wait):
  res = Install(args)

  if res != 0:
    return res

  identifier = ApplicationIdentifier(args.path)

  launch_args = [ 'launch' ]

  if wait:
    launch_args += [ '-w' ]

  launch_args += [
    'booted',
    identifier,
  ]

  return subprocess.check_output(SIMCTL_PATH + launch_arg).strip()


def Launch(args):
  InstallLaunchAndWait(args, False)


def Debug(args):
  launch_res = InstallLaunchAndWait(args, True)
  launch_pid = re.search('.*: (\d+)', launch_res).group(1)
  return os.system(' '.join([
    '/usr/bin/env',
    'xcrun',
    'lldb',
    '-s',
    os.path.join(os.path.dirname(__file__), 'lldb_start_commands.txt'),
    '-p',
    launch_pid,
  ]))


def main():
  parser = argparse.ArgumentParser(description='A script that launches an'
                                   ' application in the simulator and attaches'
                                   ' the debugger to the same')

  parser.add_argument('-p', dest='path', required=True,
                      help='Path the the simulator application')

  subparsers = parser.add_subparsers()

  launch_parser = subparsers.add_parser('launch', help='Launch')
  launch_parser.set_defaults(func=Launch)

  install_parser = subparsers.add_parser('install', help='Install')
  install_parser.set_defaults(func=Install)

  debug_parser = subparsers.add_parser('debug', help='Debug')
  debug_parser.set_defaults(func=Debug)

  args = parser.parse_args()
  return args.func(args)


if __name__ == '__main__':
  sys.exit(main())
