# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Recipe module for Skia Swarming perf.


DEPS = [
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/raw_io',
  'skia-recipes/perf',
]


def RunSteps(api):
  api.perf.run()


def GenTests(api):
  yield (
    api.test('Perf-Ubuntu-Clang-GCE-CPU-AVX2-x86_64-Release') +
    api.properties(buildername='Perf-Ubuntu-Clang-GCE-CPU-AVX2-x86_64-Release',
                   mastername='fake-master',
                   slavename='fake-slave',
                   buildnumber=5,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
    api.path.exists(
        api.path['start_dir'].join('skia'),
        api.path['start_dir'].join('skia', 'infra', 'bots', 'assets',
                                   'skimage', 'VERSION'),
        api.path['start_dir'].join('skia', 'infra', 'bots', 'assets',
                                   'skp', 'VERSION'),
        api.path['start_dir'].join('tmp', 'uninteresting_hashes.txt')
    )
  )
