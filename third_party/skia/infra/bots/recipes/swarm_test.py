# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Recipe module for Skia Swarming test.


DEPS = [
  'build/file',
  'core',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'flavor',
  'run',
  'vars',
]


TEST_BUILDERS = {
  'client.skia': {
    'skiabot-linux-swarm-000': [
      'Test-Android-GCC-AndroidOne-GPU-Mali400MP2-Arm7-Release',
      'Test-Android-GCC-GalaxyS3-GPU-Mali400-Arm7-Debug',
      'Test-Android-GCC-Nexus10-GPU-MaliT604-Arm7-Release',
      'Test-Android-GCC-Nexus6-GPU-Adreno420-Arm7-Debug',
      'Test-Android-GCC-Nexus7-GPU-Tegra3-Arm7-Debug',
      'Test-Android-GCC-Nexus9-CPU-Denver-Arm64-Debug',
      'Test-Android-GCC-NexusPlayer-CPU-SSE4-x86-Release',
      'Test-Android-GCC-NVIDIA_Shield-GPU-TegraX1-Arm64-Debug',
      'Test-iOS-Clang-iPad4-GPU-SGX554-Arm7-Debug',
      'Test-Mac-Clang-MacMini4.1-GPU-GeForce320M-x86_64-Debug',
      'Test-Mac-Clang-MacMini6.2-CPU-AVX-x86_64-Debug',
      'Test-Mac-Clang-MacMini6.2-GPU-HD4000-x86_64-Debug-CommandBuffer',
      'Test-Ubuntu-Clang-GCE-CPU-AVX2-x86_64-Coverage-Trybot',
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86-Debug',
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Debug',
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Debug-MSAN',
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-Shared',
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-TSAN',
      'Test-Ubuntu-GCC-ShuttleA-GPU-GTX550Ti-x86_64-Release-Valgrind',
      'Test-Win10-MSVC-ShuttleA-GPU-GTX660-x86_64-Debug-Vulkan',
      'Test-Win8-MSVC-ShuttleB-CPU-AVX2-x86_64-Release-Trybot',
      'Test-Win8-MSVC-ShuttleB-GPU-GTX960-x86_64-Debug-ANGLE',
    ],
  },
}


def dm_flags(bot):
  args = []

  # 32-bit desktop bots tend to run out of memory, because they have relatively
  # far more cores than RAM (e.g. 32 cores, 3G RAM).  Hold them back a bit.
  if '-x86-' in bot and not 'NexusPlayer' in bot:
    args.extend('--threads 4'.split(' '))

  # These are the canonical configs that we would ideally run on all bots. We
  # may opt out or substitute some below for specific bots
  configs = ['565', '8888', 'gpu', 'gpusrgb', 'pdf']
  # Add in either msaa4 or msaa16 to the canonical set of configs to run
  if 'Android' in bot or 'iOS' in bot:
    configs.append('msaa4')
  else:
    configs.append('msaa16')

  # With msaa, the S4 crashes and the NP produces a long error stream when we
  # run with MSAA. The Tegra2 and Tegra3 just don't support it. No record of
  # why we're not running msaa on iOS, probably started with gpu config and just
  # haven't tried.
  if ('GalaxyS4'    in bot or
      'NexusPlayer' in bot or
      'Tegra3'      in bot or
      'iOS'         in bot):
    configs = [x for x in configs if 'msaa' not in x]

  # Runs out of memory on Android bots and Daisy.  Everyone else seems fine.
  if 'Android' in bot or 'Daisy' in bot:
    configs.remove('pdf')

  if '-GCE-' in bot:
    configs.extend(['f16', 'srgb'])              # Gamma-correct formats.
    configs.extend(['sp-8888', '2ndpic-8888'])   # Test niche uses of SkPicture.
    configs.extend(['lite-8888'])                # Experimental display list.

  if '-TSAN' not in bot:
    if ('TegraK1'  in bot or
        'TegraX1'  in bot or
        'GTX550Ti' in bot or
        'GTX660'   in bot or
        'GT610'    in bot):
      if 'Android' in bot:
        configs.append('nvprdit4')
      else:
        configs.append('nvprdit16')

  # We want to test the OpenGL config not the GLES config on the X1
  if 'TegraX1' in bot:
    configs = [x.replace('gpu', 'gl') for x in configs]
    configs = [x.replace('msaa', 'glmsaa') for x in configs]
    configs = [x.replace('nvpr', 'glnvpr') for x in configs]

  # NP is running out of RAM when we run all these modes.  skia:3255
  if 'NexusPlayer' not in bot:
    configs.extend(mode + '-8888' for mode in
                   ['serialize', 'tiles_rt', 'pic'])

  if 'ANGLE' in bot:
    configs.append('angle')

  # We want to run gpudft on atleast the mali 400
  if 'GalaxyS3' in bot:
    configs.append('gpudft')

  # Test instanced rendering on a limited number of platforms
  if 'Nexus6' in bot:
    configs.append('esinst') # esinst4 isn't working yet on Adreno.
  elif 'TegraX1' in bot:
    # Multisampled instanced configs use nvpr.
    configs = [x.replace('glnvpr', 'glinst') for x in configs]
    configs.append('glinst')
  elif 'MacMini6.2' in bot:
    configs.extend(['glinst', 'glinst16'])

  # CommandBuffer bot *only* runs the command_buffer config.
  if 'CommandBuffer' in bot:
    configs = ['commandbuffer']

  # Vulkan bot *only* runs the vk config.
  if 'Vulkan' in bot:
    configs = ['vk']

  args.append('--config')
  args.extend(configs)

  # Run tests, gms, and image decoding tests everywhere.
  args.extend('--src tests gm image colorImage svg'.split(' '))

  if 'GalaxyS' in bot:
    args.extend(('--threads', '0'))

  blacklist = []

  # TODO: ???
  blacklist.extend('f16 _ _ dstreadshuffle'.split(' '))
  blacklist.extend('f16 image _ _'.split(' '))
  blacklist.extend('srgb image _ _'.split(' '))
  blacklist.extend('gpusrgb image _ _'.split(' '))

  if 'Valgrind' in bot:
    # These take 18+ hours to run.
    blacklist.extend('pdf gm _ fontmgr_iter'.split(' '))
    blacklist.extend('pdf _ _ PANO_20121023_214540.jpg'.split(' '))
    blacklist.extend('pdf skp _ worldjournal'.split(' '))
    blacklist.extend('pdf skp _ desk_baidu.skp'.split(' '))
    blacklist.extend('pdf skp _ desk_wikipedia.skp'.split(' '))

  if 'iOS' in bot:
    blacklist.extend('gpu skp _ _ msaa skp _ _'.split(' '))
    blacklist.extend('msaa16 gm _ tilemodesProcess'.split(' '))

  if 'Mac' in bot or 'iOS' in bot:
    # CG fails on questionable bmps
    blacklist.extend('_ image gen_platf rgba32abf.bmp'.split(' '))
    blacklist.extend('_ image gen_platf rgb24prof.bmp'.split(' '))
    blacklist.extend('_ image gen_platf rgb24lprof.bmp'.split(' '))
    blacklist.extend('_ image gen_platf 8bpp-pixeldata-cropped.bmp'.split(' '))
    blacklist.extend('_ image gen_platf 4bpp-pixeldata-cropped.bmp'.split(' '))
    blacklist.extend('_ image gen_platf 32bpp-pixeldata-cropped.bmp'.split(' '))
    blacklist.extend('_ image gen_platf 24bpp-pixeldata-cropped.bmp'.split(' '))

    # CG has unpredictable behavior on this questionable gif
    # It's probably using uninitialized memory
    blacklist.extend('_ image gen_platf frame_larger_than_image.gif'.split(' '))

  # WIC fails on questionable bmps
  if 'Win' in bot:
    blacklist.extend('_ image gen_platf rle8-height-negative.bmp'.split(' '))
    blacklist.extend('_ image gen_platf rle4-height-negative.bmp'.split(' '))
    blacklist.extend('_ image gen_platf pal8os2v2.bmp'.split(' '))
    blacklist.extend('_ image gen_platf pal8os2v2-16.bmp'.split(' '))
    blacklist.extend('_ image gen_platf rgba32abf.bmp'.split(' '))
    blacklist.extend('_ image gen_platf rgb24prof.bmp'.split(' '))
    blacklist.extend('_ image gen_platf rgb24lprof.bmp'.split(' '))
    blacklist.extend('_ image gen_platf 8bpp-pixeldata-cropped.bmp'.split(' '))
    blacklist.extend('_ image gen_platf 4bpp-pixeldata-cropped.bmp'.split(' '))
    blacklist.extend('_ image gen_platf 32bpp-pixeldata-cropped.bmp'.split(' '))
    blacklist.extend('_ image gen_platf 24bpp-pixeldata-cropped.bmp'.split(' '))
    if 'x86_64' in bot and 'CPU' in bot:
      # This GM triggers a SkSmallAllocator assert.
      blacklist.extend('_ gm _ composeshader_bitmap'.split(' '))

  if 'Android' in bot or 'iOS' in bot:
    # This test crashes the N9 (perhaps because of large malloc/frees). It also
    # is fairly slow and not platform-specific. So we just disable it on all of
    # Android and iOS. skia:5438
    blacklist.extend('_ test _ GrShape'.split(' '))

  if 'Win8' in bot:
    # bungeman: "Doesn't work on Windows anyway, produces unstable GMs with
    # 'Unexpected error' from DirectWrite"
    blacklist.extend('_ gm _ fontscalerdistortable'.split(' '))
    # skia:5636
    blacklist.extend('_ svg _ Nebraska-StateSeal.svg'.split(' '))

  # skia:4095
  bad_serialize_gms = ['bleed_image',
                       'c_gms',
                       'colortype',
                       'colortype_xfermodes',
                       'drawfilter',
                       'fontmgr_bounds_0.75_0',
                       'fontmgr_bounds_1_-0.25',
                       'fontmgr_bounds',
                       'fontmgr_match',
                       'fontmgr_iter']

  # skia:5589
  bad_serialize_gms.extend(['bitmapfilters',
                            'bitmapshaders',
                            'bleed',
                            'bleed_alpha_bmp',
                            'bleed_alpha_bmp_shader',
                            'convex_poly_clip',
                            'extractalpha',
                            'filterbitmap_checkerboard_32_32_g8',
                            'filterbitmap_image_mandrill_64',
                            'shadows',
                            'simpleaaclip_aaclip'])
  # skia:5595
  bad_serialize_gms.extend(['composeshader_bitmap',
                            'scaled_tilemodes_npot',
                            'scaled_tilemodes'])
  for test in bad_serialize_gms:
    blacklist.extend(['serialize-8888', 'gm', '_', test])

  if 'Mac' not in bot:
    for test in ['bleed_alpha_image', 'bleed_alpha_image_shader']:
      blacklist.extend(['serialize-8888', 'gm', '_', test])
  # It looks like we skip these only for out-of-memory concerns.
  if 'Win' in bot or 'Android' in bot:
    for test in ['verylargebitmap', 'verylarge_picture_image']:
      blacklist.extend(['serialize-8888', 'gm', '_', test])

  # skia:4769
  for test in ['drawfilter']:
    blacklist.extend([    'sp-8888', 'gm', '_', test])
    blacklist.extend([   'pic-8888', 'gm', '_', test])
    blacklist.extend(['2ndpic-8888', 'gm', '_', test])
    blacklist.extend([  'lite-8888', 'gm', '_', test])
  # skia:4703
  for test in ['image-cacherator-from-picture',
               'image-cacherator-from-raster',
               'image-cacherator-from-ctable']:
    blacklist.extend([       'sp-8888', 'gm', '_', test])
    blacklist.extend([      'pic-8888', 'gm', '_', test])
    blacklist.extend([   '2ndpic-8888', 'gm', '_', test])
    blacklist.extend(['serialize-8888', 'gm', '_', test])

  # SaveLayerDrawRestoreNooper diffs
  for test in ['car.svg',
               'gallardo.svg',
               'rg1024_green_grapes.svg',
               'Seal_of_Kansas.svg']:
    blacklist.extend([       'sp-8888', 'svg', '_', test])
    blacklist.extend([      'pic-8888', 'svg', '_', test])
    blacklist.extend([   '2ndpic-8888', 'svg', '_', test])
    blacklist.extend(['serialize-8888', 'svg', '_', test])

  # Extensions for RAW images
  r = ["arw", "cr2", "dng", "nef", "nrw", "orf", "raf", "rw2", "pef", "srw",
       "ARW", "CR2", "DNG", "NEF", "NRW", "ORF", "RAF", "RW2", "PEF", "SRW"]

  # skbug.com/4888
  # Blacklist RAW images (and a few large PNGs) on GPU bots
  # until we can resolve failures
  if 'GPU' in bot:
    blacklist.extend('_ image _ interlaced1.png'.split(' '))
    blacklist.extend('_ image _ interlaced2.png'.split(' '))
    blacklist.extend('_ image _ interlaced3.png'.split(' '))
    for raw_ext in r:
      blacklist.extend(('_ image _ .%s' % raw_ext).split(' '))

  if 'Nexus9' in bot:
    for raw_ext in r:
      blacklist.extend(('_ image _ .%s' % raw_ext).split(' '))

  # Large image that overwhelms older Mac bots
  if 'MacMini4.1-GPU' in bot:
    blacklist.extend('_ image _ abnormal.wbmp'.split(' '))
    blacklist.extend(['msaa16', 'gm', '_', 'blurcircles'])

  match = []
  if 'Valgrind' in bot: # skia:3021
    match.append('~Threaded')

  if 'GalaxyS3' in bot:  # skia:1699
    match.append('~WritePixels')

  if 'AndroidOne' in bot:  # skia:4711
    match.append('~WritePixels')

  if 'NexusPlayer' in bot:
    match.append('~ResourceCache')

  if 'Nexus10' in bot: # skia:5509
    match.append('~CopySurface')

  if 'ANGLE' in bot and 'Debug' in bot:
    match.append('~GLPrograms') # skia:4717

  if 'MSAN' in bot:
    match.extend(['~Once', '~Shared'])  # Not sure what's up with these tests.

  if 'TSAN' in bot:
    match.extend(['~ReadWriteAlpha'])   # Flaky on TSAN-covered on nvidia bots.

  if blacklist:
    args.append('--blacklist')
    args.extend(blacklist)

  if match:
    args.append('--match')
    args.extend(match)

  # These bots run out of memory running RAW codec tests. Do not run them in
  # parallel
  if ('NexusPlayer' in bot or 'Nexus5' in bot or 'Nexus9' in bot
      or 'Win8-MSVC-ShuttleB' in bot):
    args.append('--noRAW_threading')

  return args


def key_params(api):
  """Build a unique key from the builder name (as a list).

  E.g.  arch x86 gpu GeForce320M mode MacMini4.1 os Mac10.6
  """
  # Don't bother to include role, which is always Test.
  # TryBots are uploaded elsewhere so they can use the same key.
  blacklist = ['role', 'is_trybot']

  flat = []
  for k in sorted(api.vars.builder_cfg.keys()):
    if k not in blacklist:
      flat.append(k)
      flat.append(api.vars.builder_cfg[k])
  return flat


def test_steps(api):
  """Run the DM test."""
  use_hash_file = False
  if api.vars.upload_dm_results:
    # This must run before we write anything into
    # api.flavor.device_dirs.dm_dir or we may end up deleting our
    # output on machines where they're the same.
    api.flavor.create_clean_host_dir(api.vars.dm_dir)
    host_dm_dir = str(api.vars.dm_dir)
    device_dm_dir = str(api.flavor.device_dirs.dm_dir)
    if host_dm_dir != device_dm_dir:
      api.flavor.create_clean_device_dir(device_dm_dir)

    # Obtain the list of already-generated hashes.
    hash_filename = 'uninteresting_hashes.txt'

    # Ensure that the tmp_dir exists.
    api.run.run_once(api.file.makedirs,
                           'tmp_dir',
                           api.vars.tmp_dir,
                           infra_step=True)

    host_hashes_file = api.vars.tmp_dir.join(hash_filename)
    hashes_file = api.flavor.device_path_join(
        api.flavor.device_dirs.tmp_dir, hash_filename)
    api.run(
        api.python.inline,
        'get uninteresting hashes',
        program="""
        import contextlib
        import math
        import socket
        import sys
        import time
        import urllib2

        HASHES_URL = 'https://gold.skia.org/_/hashes'
        RETRIES = 5
        TIMEOUT = 60
        WAIT_BASE = 15

        socket.setdefaulttimeout(TIMEOUT)
        for retry in range(RETRIES):
          try:
            with contextlib.closing(
                urllib2.urlopen(HASHES_URL, timeout=TIMEOUT)) as w:
              hashes = w.read()
              with open(sys.argv[1], 'w') as f:
                f.write(hashes)
                break
          except Exception as e:
            print 'Failed to get uninteresting hashes from %s:' % HASHES_URL
            print e
            if retry == RETRIES:
              raise
            waittime = WAIT_BASE * math.pow(2, retry)
            print 'Retry in %d seconds.' % waittime
            time.sleep(waittime)
        """,
        args=[host_hashes_file],
        cwd=api.vars.skia_dir,
        abort_on_failure=False,
        fail_build_on_failure=False,
        infra_step=True)

    if api.path.exists(host_hashes_file):
      api.flavor.copy_file_to_device(host_hashes_file, hashes_file)
      use_hash_file = True

  # Run DM.
  properties = [
    'gitHash',      api.vars.got_revision,
    'master',       api.vars.master_name,
    'builder',      api.vars.builder_name,
    'build_number', api.vars.build_number,
  ]
  if api.vars.is_trybot:
    properties.extend([
      'issue',         api.vars.issue,
      'patchset',      api.vars.patchset,
      'patch_storage', api.vars.patch_storage,
    ])

  args = [
    'dm',
    '--undefok',   # This helps branches that may not know new flags.
    '--resourcePath', api.flavor.device_dirs.resource_dir,
    '--skps', api.flavor.device_dirs.skp_dir,
    '--images', api.flavor.device_path_join(
        api.flavor.device_dirs.images_dir, 'dm'),
    '--colorImages', api.flavor.device_path_join(
        api.flavor.device_dirs.images_dir, 'colorspace'),
    '--nameByHash',
    '--properties'
  ] + properties

  args.extend(['--svgs', api.flavor.device_dirs.svg_dir])

  args.append('--key')
  args.extend(key_params(api))
  if use_hash_file:
    args.extend(['--uninterestingHashesFile', hashes_file])
  if api.vars.upload_dm_results:
    args.extend(['--writePath', api.flavor.device_dirs.dm_dir])

  skip_flag = None
  if api.vars.builder_cfg.get('cpu_or_gpu') == 'CPU':
    skip_flag = '--nogpu'
  elif api.vars.builder_cfg.get('cpu_or_gpu') == 'GPU':
    skip_flag = '--nocpu'
  if skip_flag:
    args.append(skip_flag)
  args.extend(dm_flags(api.vars.builder_name))

  api.run(api.flavor.step, 'dm', cmd=args,
          abort_on_failure=False,
          env=api.vars.default_env)

  if api.vars.upload_dm_results:
    # Copy images and JSON to host machine if needed.
    api.flavor.copy_directory_contents_to_host(
        api.flavor.device_dirs.dm_dir, api.vars.dm_dir)

  # See skia:2789.
  if ('Valgrind' in api.vars.builder_name and
      api.vars.builder_cfg.get('cpu_or_gpu') == 'GPU'):
    abandonGpuContext = list(args)
    abandonGpuContext.append('--abandonGpuContext')
    api.run(api.flavor.step, 'dm --abandonGpuContext',
                  cmd=abandonGpuContext, abort_on_failure=False)
    preAbandonGpuContext = list(args)
    preAbandonGpuContext.append('--preAbandonGpuContext')
    api.run(api.flavor.step, 'dm --preAbandonGpuContext',
                  cmd=preAbandonGpuContext, abort_on_failure=False,
                  env=api.vars.default_env)


def RunSteps(api):
  api.core.setup()
  api.flavor.install()
  test_steps(api)
  api.flavor.cleanup_steps()
  api.run.check_failure()


def GenTests(api):
  def AndroidTestData(builder, adb=None):
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
            'exists skia_dm',
            stdout=api.raw_io.output(''))
    )
    if 'GalaxyS3' not in builder:
      test_data += api.step_data(
          'adb root',
          stdout=api.raw_io.output('restarting adbd as root'))
    if adb:
      test_data += api.step_data(
          'which adb',
          stdout=api.raw_io.output(adb))
    else:
      test_data += api.step_data(
        'which adb',
        retcode=1)

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
              api.path['slave_build'].join('skia', 'infra', 'bots', 'assets',
                                           'svg', 'VERSION'),
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

  builder = 'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Debug'
  yield (
    api.test('failed_dm') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    api.step_data('dm', retcode=1)
  )

  builder = 'Test-Android-GCC-Nexus7-GPU-Tegra3-Arm7-Debug'
  yield (
    api.test('failed_get_hashes') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    AndroidTestData(builder) +
    api.step_data('read SKP_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SK_IMAGE_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SVG_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('get uninteresting hashes', retcode=1)
  )

  yield (
    api.test('download_and_push_skps') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    AndroidTestData(builder) +
    api.step_data('read SKP_VERSION',
                  stdout=api.raw_io.output('2')) +
    api.step_data('read SK_IMAGE_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SVG_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data(
        'exists skps',
        stdout=api.raw_io.output(''))
  )

  yield (
    api.test('missing_SKP_VERSION_device') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    AndroidTestData(builder) +
    api.step_data('read SKP_VERSION',
                  retcode=1) +
    api.step_data('read SK_IMAGE_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SVG_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data(
        'exists skps',
        stdout=api.raw_io.output(''))
  )

  yield (
    api.test('download_and_push_skimage') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    AndroidTestData(builder) +
    api.step_data('read SKP_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SK_IMAGE_VERSION',
                  stdout=api.raw_io.output('2')) +
    api.step_data('read SVG_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data(
        'exists skia_images',
        stdout=api.raw_io.output(''))
  )

  yield (
    api.test('missing_SK_IMAGE_VERSION_device') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    AndroidTestData(builder) +
    api.step_data('read SKP_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SK_IMAGE_VERSION',
                  retcode=1) +
    api.step_data('read SVG_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data(
        'exists skia_images',
        stdout=api.raw_io.output(''))
  )

  yield (
    api.test('download_and_push_svgs') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    AndroidTestData(builder) +
    api.step_data('read SKP_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SK_IMAGE_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SVG_VERSION',
                  stdout=api.raw_io.output('2')) +
    api.step_data(
        'exists svgs',
        stdout=api.raw_io.output(''))
  )

  yield (
    api.test('missing_SVG_VERSION_device') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    AndroidTestData(builder) +
    api.step_data('read SKP_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SK_IMAGE_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SVG_VERSION',
                  retcode=1) +
    api.step_data(
        'exists svgs',
        stdout=api.raw_io.output(''))
  )

  yield (
    api.test('adb_in_path') +
    api.properties(buildername=builder,
                   mastername='client.skia',
                   slavename='skiabot-linux-swarm-000',
                   buildnumber=6,
                   revision='abc123',
                   path_config='kitchen',
                   swarm_out_dir='[SWARM_OUT_DIR]') +
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
    AndroidTestData(builder, adb='/usr/bin/adb') +
    api.step_data('read SKP_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SK_IMAGE_VERSION',
                  stdout=api.raw_io.output('42')) +
    api.step_data('read SVG_VERSION',
                  stdout=api.raw_io.output('42'))
  )

  builder = 'Test-Win8-MSVC-ShuttleB-CPU-AVX2-x86_64-Release-Trybot'
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

  builder = 'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86-Debug-Trybot'
  gerrit_kwargs = {
    'patch_storage': 'gerrit',
    'repository': 'skia',
    'event.patchSet.ref': 'refs/changes/00/2100/2',
    'event.change.number': '2100',
  }
  yield (
      api.test('recipe_with_gerrit_patch') +
      api.properties(
          buildername=builder,
          mastername='client.skia',
          slavename='skiabot-linux-swarm-000',
          buildnumber=5,
          path_config='kitchen',
          swarm_out_dir='[SWARM_OUT_DIR]',
          revision='abc123',
          **gerrit_kwargs)
  )
