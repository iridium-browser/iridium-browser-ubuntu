#
# Copyright 2015 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

#!/usr/bin/env python

usage = '''
Write extra flags to outfile for DM based on the bot name:
  $ python dm_flags.py outfile Test-Ubuntu-GCC-GCE-CPU-AVX2-x86-Debug
Or run self-tests:
  $ python dm_flags.py test
'''

import inspect
import json
import os
import sys


def lineno():
  caller = inspect.stack()[1]  # Up one level to our caller.
  return inspect.getframeinfo(caller[0]).lineno


cov_start = lineno()+1   # We care about coverage starting just past this def.
def get_args(bot):
  args = []

  configs = ['565', '8888', 'gpu']
  # The S4 crashes and the NP produces a long error stream when we run with
  # MSAA.
  if ('GalaxyS4'    not in bot and
      'NexusPlayer' not in bot):
    if 'Android' in bot:
      configs.extend(['msaa4', 'nvprmsaa4'])
    else:
      configs.extend(['msaa16', 'nvprmsaa16'])
  # Runs out of memory on Android bots and Daisy.  Everyone else seems fine.
  if 'Android' not in bot and 'Daisy' not in bot:
    configs.append('pdf')

  # Xoom and NP are running out of RAM when we run all these modes.  skia:3255
  if ('Xoom'        not in bot and
      'NexusPlayer' not in bot):
    configs.extend(mode + '-8888' for mode in
                   ['serialize', 'tiles_rt', 'pipe'])
    configs.append('tiles_rt-gpu')
  if 'ANGLE' in bot:
    configs.append('angle')
  args.append('--config')
  args.extend(configs)

  blacklist = []
  # This image is too large to be a texture for many GPUs.
  blacklist.extend('gpu _ PANO_20121023_214540.jpg'.split(' '))
  blacklist.extend('msaa _ PANO_20121023_214540.jpg'.split(' '))

  # Several of the newest version bmps fail on SkImageDecoder
  blacklist.extend('_ image pal8os2v2.bmp'.split(' '))
  blacklist.extend('_ image pal8v4.bmp'.split(' '))
  blacklist.extend('_ image pal8v5.bmp'.split(' '))
  blacklist.extend('_ image rgb16-565.bmp'.split(' '))
  blacklist.extend('_ image rgb16-565pal.bmp'.split(' '))
  blacklist.extend('_ image rgb32-111110.bmp'.split(' '))
  blacklist.extend('_ image rgb32bf.bmp'.split(' '))
  blacklist.extend('_ image rgba32.bmp'.split(' '))
  blacklist.extend('_ image rgba32abf.bmp'.split(' '))
  blacklist.extend('_ image rgb24largepal.bmp'.split(' '))
  blacklist.extend('_ image pal8os2v2-16.bmp'.split(' '))
  blacklist.extend('_ image pal8oversizepal.bmp'.split(' '))
  blacklist.extend('_ subset rgb24largepal.bmp'.split(' '))
  blacklist.extend('_ subset pal8os2v2-16.bmp'.split(' '))
  blacklist.extend('_ subset pal8oversizepal.bmp'.split(' '))

  # New ico files that fail on SkImageDecoder
  blacklist.extend('_ image Hopstarter-Mac-Folders-Apple.ico'.split(' '))

  # Leon doesn't care about this, so why run it?
  if 'Win' in bot:
    blacklist.extend('_ image _'.split(' '))
    blacklist.extend('_ subset _'.split(' '))

  # Certain gm's on win7 gpu and pdf are never finishing and keeping the test
  # running forever
  if 'Win7' in bot:
    blacklist.extend('msaa16 gm colorwheelnative'.split(' '))
    blacklist.extend('pdf gm fontmgr_iter_factory'.split(' '))

  # Drawing SKPs or images into GPU canvases is a New Thing.
  # It seems like we're running out of RAM on some Android bots, so start off
  # with a very wide blacklist disabling all these tests on all Android bots.
  if 'Android' in bot:  # skia:3255
    blacklist.extend('gpu skp _ gpu image _ gpu subset _'.split(' '))
    blacklist.extend('msaa skp _ msaa image _ gpu subset _'.split(' '))

  if 'Valgrind' in bot:
    # PDF + .webp -> jumps depending on uninitialized memory.  skia:3505
    blacklist.extend('pdf _ .webp'.split(' '))
    # These take 18+ hours to run.
    blacklist.extend('pdf gm fontmgr_iter'.split(' '))
    blacklist.extend('pdf _ PANO_20121023_214540.jpg'.split(' '))
    blacklist.extend('pdf skp tabl_worldjournal.skp'.split(' '))
    blacklist.extend('pdf skp desk_baidu.skp'.split(' '))

  if blacklist:
    args.append('--blacklist')
    args.extend(blacklist)

  match = []
  if 'Valgrind' in bot: # skia:3021
    match.append('~Threaded')
  if 'TSAN' in bot: # skia:3562
    match.append('~Math')

  if 'Xoom' in bot or 'GalaxyS3' in bot:  # skia:1699
    match.append('~WritePixels')

  # skia:3249: these images flakily don't decode on Android.
  if 'Android' in bot:
    match.append('~tabl_mozilla_0')
    match.append('~desk_yahoonews_0')

  if 'NexusPlayer' in bot:
    match.append('~ResourceCache')

  if match:
    args.append('--match')
    args.extend(match)

  return args
cov_end = lineno()   # Don't care about code coverage past here.


def self_test():
  import coverage  # This way the bots don't need coverage.py to be installed.
  args = {}
  cases = [
    'Test-Android-GCC-GalaxyS3-GPU-Mali400-Arm7-Debug',
    'Test-Android-GCC-Nexus7-GPU-Tegra3-Arm7-Release',
    'Test-Android-GCC-NexusPlayer-CPU-SSSE3-x86-Release',
    'Test-Android-GCC-Xoom-GPU-Tegra2-Arm7-Release',
    'Test-Ubuntu-GCC-ShuttleA-GPU-GTX550Ti-x86_64-Release-Valgrind',
    'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-TSAN',
    'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-Valgrind',
    'Test-Win7-MSVC-ShuttleA-GPU-HD2000-x86-Debug-ANGLE',
  ]

  cov = coverage.coverage()
  cov.start()
  for case in cases:
    args[case] = get_args(case)
  cov.stop()

  this_file = os.path.basename(__file__)
  _, _, not_run, _ = cov.analysis(this_file)
  filtered = [line for line in not_run if line > cov_start and line < cov_end]
  if filtered:
    print 'Lines not covered by test cases: ', filtered
    sys.exit(1)

  golden = this_file.replace('.py', '.json')
  with open(os.path.join(os.path.dirname(__file__), golden), 'w') as f:
    json.dump(args, f, indent=2, sort_keys=True)


if __name__ == '__main__':
  if len(sys.argv) == 2 and sys.argv[1] == 'test':
    self_test()
    sys.exit(0)

  if len(sys.argv) != 3:
    print usage
    sys.exit(1)

  with open(sys.argv[1], 'w') as out:
    json.dump(get_args(sys.argv[2]), out)
