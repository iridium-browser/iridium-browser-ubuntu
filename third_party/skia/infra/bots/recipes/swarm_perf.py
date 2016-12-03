# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Recipe module for Skia Swarming perf.


DEPS = [
  'build/file',
  'core',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/raw_io',
  'run',
  'flavor',
  'vars',
]


TEST_BUILDERS = {
  'client.skia': {
    'skiabot-linux-swarm-000': [
      'Perf-Android-GCC-GalaxyS3-GPU-Mali400-Arm7-Release',
      'Perf-Android-GCC-Nexus5-GPU-Adreno330-Arm7-Debug',
      'Perf-Android-GCC-Nexus6-GPU-Adreno420-Arm7-Release',
      'Perf-Android-GCC-Nexus7-GPU-Tegra3-Arm7-Release',
      'Perf-Android-GCC-NexusPlayer-GPU-PowerVR-x86-Release',
      'Perf-Android-GCC-NVIDIA_Shield-GPU-TegraX1-Arm64-Debug-Vulkan',
      'Perf-iOS-Clang-iPad4-GPU-SGX554-Arm7-Debug',
      'Perf-Mac-Clang-MacMini6.2-CPU-AVX-x86_64-Release-GN',
      'Perf-Ubuntu-GCC-ShuttleA-GPU-GTX550Ti-x86_64-Release-Valgrind',
      'Perf-Ubuntu-GCC-ShuttleA-GPU-GTX550Ti-x86_64-Release-VisualBench',
      'Perf-Win-MSVC-GCE-CPU-AVX2-x86_64-Release',
      'Perf-Win-MSVC-GCE-CPU-AVX2-x86_64-Debug',
      'Perf-Win8-MSVC-ShuttleB-GPU-HD4600-x86_64-Release-Trybot',
    ],
  },
}


def nanobench_flags(bot):
  args = ['--pre_log']

  if 'GPU' in bot:
    args.append('--images')
    args.extend(['--gpuStatsDump', 'true'])

  if 'Android' in bot and 'GPU' in bot:
    args.extend(['--useThermalManager', '1,1,10,1000'])

  args.extend(['--scales', '1.0', '1.1'])

  if 'iOS' in bot:
    args.extend(['--skps', 'ignore_skps'])

  config = ['565', '8888', 'gpu', 'nonrendering', 'angle', 'hwui' ]
  config += [ 'f16', 'srgb' ]
  # The S4 crashes and the NP produces a long error stream when we run with
  # MSAA.
  if ('GalaxyS4'    not in bot and
      'NexusPlayer' not in bot):
    if 'Android' in bot:
      # The TegraX1 has a regular OpenGL implementation. We bench that instead
      # of ES.
      if 'TegraX1' in bot:
        config.remove('gpu')
        config.extend(['gl', 'glmsaa4', 'glnvpr4', 'glnvprdit4'])
      else:
        config.extend(['msaa4', 'nvpr4', 'nvprdit4'])
    else:
      config.extend(['msaa16', 'nvpr16', 'nvprdit16'])

  # Bench instanced rendering on a limited number of platforms
  if 'Nexus6' in bot:
    config.append('esinst') # esinst4 isn't working yet on Adreno.
  elif 'TegraX1' in bot:
    config.extend(['glinst', 'glinst4'])
  elif 'MacMini6.2' in bot:
    config.extend(['glinst', 'glinst16'])

  if 'Vulkan' in bot:
    config = ['vk']

  args.append('--config')
  args.extend(config)

  if 'Valgrind' in bot:
    # Don't care about Valgrind performance.
    args.extend(['--loops',   '1'])
    args.extend(['--samples', '1'])
    # Ensure that the bot framework does not think we have timed out.
    args.extend(['--keepAlive', 'true'])

  match = []
  if 'Android' in bot:
    # Segfaults when run as GPU bench. Very large texture?
    match.append('~blurroundrect')
    match.append('~patch_grid')  # skia:2847
    match.append('~desk_carsvg')
  if 'NexusPlayer' in bot:
    match.append('~desk_unicodetable')
  if 'Nexus5' in bot:
    match.append('~keymobi_shop_mobileweb_ebay_com.skp')  # skia:5178
  if 'iOS' in bot:
    match.append('~blurroundrect')
    match.append('~patch_grid')  # skia:2847
    match.append('~desk_carsvg')
    match.append('~keymobi')
    match.append('~path_hairline')
    match.append('~GLInstancedArraysBench') # skia:4714

  # the 32-bit GCE bots run out of memory in DM when running these large images
  # so defensively disable them in nanobench, too.
  # FIXME (scroggo): This may have just been due to SkImageDecoder's
  # buildTileIndex leaking memory (https://bug.skia.org/4360). That is
  # disabled by default for nanobench, so we may not need this.
  # FIXME (scroggo): Share image blacklists between dm and nanobench?
  if 'x86' in bot and not 'x86-64' in bot:
    match.append('~interlaced1.png')
    match.append('~interlaced2.png')
    match.append('~interlaced3.png')

  # This low-end Android bot crashes about 25% of the time while running the
  # (somewhat intense) shapes benchmarks.
  if 'Perf-Android-GCC-GalaxyS3-GPU-Mali400-Arm7-Release' in bot:
    match.append('~shapes_')

  # We do not need or want to benchmark the decodes of incomplete images.
  # In fact, in nanobench we assert that the full image decode succeeds.
  match.append('~inc0.gif')
  match.append('~inc1.gif')
  match.append('~incInterlaced.gif')
  match.append('~inc0.jpg')
  match.append('~incGray.jpg')
  match.append('~inc0.wbmp')
  match.append('~inc1.wbmp')
  match.append('~inc0.webp')
  match.append('~inc1.webp')
  match.append('~inc0.ico')
  match.append('~inc1.ico')
  match.append('~inc0.png')
  match.append('~inc1.png')
  match.append('~inc2.png')
  match.append('~inc12.png')
  match.append('~inc13.png')
  match.append('~inc14.png')
  match.append('~inc0.webp')
  match.append('~inc1.webp')

  if match:
    args.append('--match')
    args.extend(match)

  return args


def perf_steps(api):
  """Run Skia benchmarks."""
  if api.vars.upload_perf_results:
    api.flavor.create_clean_device_dir(
        api.flavor.device_dirs.perf_data_dir)

  # Run nanobench.
  properties = [
    '--properties',
    'gitHash',      api.vars.got_revision,
    'build_number', api.vars.build_number,
  ]
  if api.vars.is_trybot:
    properties.extend([
      'issue',    api.vars.issue,
      'patchset', api.vars.patchset,
      'patch_storage', api.vars.patch_storage,
    ])

  target = 'nanobench'
  if 'VisualBench' in api.vars.builder_name:
    target = 'visualbench'
  args = [
      target,
      '--undefok',   # This helps branches that may not know new flags.
      '-i',       api.flavor.device_dirs.resource_dir,
      '--skps',   api.flavor.device_dirs.skp_dir,
      '--images', api.flavor.device_path_join(
          api.flavor.device_dirs.images_dir, 'nanobench'),
  ]

  skip_flag = None
  if api.vars.builder_cfg.get('cpu_or_gpu') == 'CPU':
    skip_flag = '--nogpu'
  elif api.vars.builder_cfg.get('cpu_or_gpu') == 'GPU':
    skip_flag = '--nocpu'
  if skip_flag:
    args.append(skip_flag)
  args.extend(nanobench_flags(api.vars.builder_name))

  if api.vars.upload_perf_results:
    json_path = api.flavor.device_path_join(
        api.flavor.device_dirs.perf_data_dir,
        'nanobench_%s.json' % api.vars.got_revision)
    args.extend(['--outResultsFile', json_path])
    args.extend(properties)

    keys_blacklist = ['configuration', 'role', 'is_trybot']
    args.append('--key')
    for k in sorted(api.vars.builder_cfg.keys()):
      if not k in keys_blacklist:
        args.extend([k, api.vars.builder_cfg[k]])

  api.run(api.flavor.step, target, cmd=args,
          abort_on_failure=False,
          env=api.vars.default_env)

  # See skia:2789.
  if ('Valgrind' in api.vars.builder_name and
      api.vars.builder_cfg.get('cpu_or_gpu') == 'GPU'):
    abandonGpuContext = list(args)
    abandonGpuContext.extend(['--abandonGpuContext', '--nocpu'])
    api.run(api.flavor.step,
            '%s --abandonGpuContext' % target,
            cmd=abandonGpuContext, abort_on_failure=False,
            env=api.vars.default_env)

  # Copy results to swarming out dir.
  if api.vars.upload_perf_results:
    api.file.makedirs('perf_dir', api.vars.perf_data_dir)
    api.flavor.copy_directory_contents_to_host(
        api.flavor.device_dirs.perf_data_dir,
        api.vars.perf_data_dir)


def RunSteps(api):
  api.core.setup()
  api.flavor.install()
  perf_steps(api)
  api.flavor.cleanup_steps()
  api.run.check_failure()


def GenTests(api):
  def AndroidTestData(builder):
    test_data = (
        api.step_data(
            'get EXTERNAL_STORAGE dir',
            stdout=api.raw_io.output('/storage/emulated/legacy')) +
        api.step_data(
            'read SKP_VERSION',
            stdout=api.raw_io.output('42')) +
        api.step_data(
            'read SK_IMAGE_VERSION',
            stdout=api.raw_io.output('42')) +
        api.step_data(
            'read SVG_VERSION',
            stdout=api.raw_io.output('42')) +
        api.step_data(
            'which adb',
            retcode=1)
    )
    if not 'Debug' in builder:
      test_data += api.step_data(
          'exists skia_perf',
          stdout=api.raw_io.output(''))
    if not 'GalaxyS3' in builder:
      test_data += api.step_data(
          'adb root',
          stdout=api.raw_io.output('restarting adbd as root'))
    return test_data

  for mastername, slaves in TEST_BUILDERS.iteritems():
    for slavename, builders_by_slave in slaves.iteritems():
      for builder in builders_by_slave:
        test = (
          api.test(builder) +
          api.properties(buildername=builder,
                         mastername=mastername,
                         slavename=slavename,
                         buildnumber=5,
                         revision='abc123',
                         path_config='kitchen',
                         swarm_out_dir='[SWARM_OUT_DIR]') +
          api.path.exists(
              api.path['slave_build'].join('skia'),
              api.path['slave_build'].join('skia', 'infra', 'bots', 'assets',
                                           'skimage', 'VERSION'),
              api.path['slave_build'].join('skia', 'infra', 'bots', 'assets',
                                           'skp', 'VERSION'),
              api.path['slave_build'].join('tmp', 'uninteresting_hashes.txt')
          )
        )
        if ('Android' in builder and
            ('Test' in builder or 'Perf' in builder) and
            not 'Appurify' in builder):
          test += AndroidTestData(builder)
        if 'Trybot' in builder:
          test += api.properties(issue=500,
                                 patchset=1,
                                 rietveld='https://codereview.chromium.org')
        if 'Win' in builder:
          test += api.platform('win', 64)

        yield test

  builder = 'Perf-Win8-MSVC-ShuttleB-GPU-HD4600-x86_64-Release-Trybot'
  yield (
    api.test('big_issue_number') +
    api.properties(buildername=builder,
                   mastername='client.skia.compile',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=5,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]',
                   rietveld='https://codereview.chromium.org',
                   patchset=1,
                   issue=2147533002L) +
    api.path.exists(
        api.path['slave_build'].join('skia'),
        api.path['slave_build'].join('skia', 'infra', 'bots', 'assets',
                                     'skimage', 'VERSION'),
        api.path['slave_build'].join('skia', 'infra', 'bots', 'assets',
                                     'skp', 'VERSION'),
        api.path['slave_build'].join('skia', 'infra', 'bots', 'assets',
                                     'svg', 'VERSION'),
        api.path['slave_build'].join('tmp', 'uninteresting_hashes.txt')
    ) +
    api.platform('win', 64)
  )

  gerrit_kwargs = {
    'patch_storage': 'gerrit',
    'repository': 'skia',
    'event.patchSet.ref': 'refs/changes/00/2100/2',
    'event.change.number': '2100',
  }
  yield (
      api.test('recipe_with_gerrit_patch') +
      api.properties(
          buildername='Perf-Ubuntu-GCC-ShuttleA-GPU-GTX550Ti-x86_64-Release-' +
                      'Valgrind-Trybot',
          mastername='client.skia',
          slavename='skiabot-linux-swarm-000',
          buildnumber=5,
          path_config='kitchen',
          swarm_out_dir='[SWARM_OUT_DIR]',
          revision='abc123',
          **gerrit_kwargs)
  )
