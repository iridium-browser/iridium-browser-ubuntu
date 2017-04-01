# Copyright 2015 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Gyp file for building opts target.
{
  # (To be honest, I'm not sure why we need to include common.gypi.  I thought it was automatic.)
  'variables': {
    'includes': [ 'common.gypi' ],
  },

  # Generally we shove things into one 'opts' target conditioned on platform.
  # If a particular platform needs some files built with different flags,
  # those become separate targets: opts_ssse3, opts_sse41, opts_neon.

  'targets': [
    {
      'target_name': 'opts',
      'product_name': 'skia_opts',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'core.gyp:*',
        'effects.gyp:*'
      ],
      'include_dirs': [
        '../include/private',
        '../src/core',
        '../src/opts',
        '../src/utils',
        '../include/utils',
      ],
      'conditions': [
        [ '"x86" in skia_arch_type and skia_os != "ios"', {
          'cflags': [ '-msse2' ],
          'dependencies': [ 'opts_ssse3', 'opts_sse41', 'opts_sse42', 'opts_avx', 'opts_hsw' ],
          'sources': [ '<!@(python read_gni.py ../gn/opts.gni sse2)' ],
        }],

        [ 'skia_arch_type == "mips"', {
          'conditions': [
            [ '(mips_arch_variant == "mips32r2") and (mips_dsp == 1 or mips_dsp == 2)', {
                'sources': [ '<!@(python read_gni.py ../gn/opts.gni mips_dsp)' ],
            },{
                'sources': [ '<!@(python read_gni.py ../gn/opts.gni none)' ],
            }],
          ]
        }],

        [ '(skia_arch_type == "arm" and arm_version < 7) \
            or (skia_os == "ios") \
            or (skia_os == "android" \
                and skia_arch_type not in ["x86", "x86_64", "arm", "mips", \
                                           "arm64"])', {
          'sources': [ '<!@(python read_gni.py ../gn/opts.gni none)' ],
        }],

        [ 'skia_arch_type == "arm" and arm_version >= 7', {
          # The assembly uses the frame pointer register (r7 in Thumb/r11 in
          # ARM), the compiler doesn't like that.
          'cflags!': [ '-fno-omit-frame-pointer', '-mapcs-frame', '-mapcs' ],
          'cflags':  [ '-fomit-frame-pointer' ],
          'sources': [ '<!@(python read_gni.py ../gn/opts.gni armv7)' ],
          'conditions': [
            [ 'arm_neon == 1', {
              'dependencies': [ 'opts_neon' ]
            }],
          ],
        }],

        [ 'skia_arch_type == "arm64"', {
          'sources': [ '<!@(python read_gni.py ../gn/opts.gni arm64)' ],
          'dependencies': [ 'opts_crc32' ]
        }],

        [ 'skia_android_framework', {
          'cflags!': [
            '-msse2',
            '-mfpu=neon',
            '-fomit-frame-pointer',
          ]
        }],
      ],
    },
    {
      'target_name': 'opts_crc32',
      'product_name': 'skia_opts_crc32',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [ 'core.gyp:*' ],
      'include_dirs': [
          '../include/private',
          '../src/core',
          '../src/utils',
      ],
      'sources': [ '<!@(python read_gni.py ../gn/opts.gni crc32)' ],
      'conditions': [
        [ 'not skia_android_framework', { 'cflags': [ '-march=armv8-a+crc' ] }],
      ],
    },
    {
      'target_name': 'opts_ssse3',
      'product_name': 'skia_opts_ssse3',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [ 'core.gyp:*' ],
      'include_dirs': [
          '../include/private',
          '../src/core',
          '../src/utils',
      ],
      'sources': [ '<!@(python read_gni.py ../gn/opts.gni ssse3)' ],
      'conditions': [
        [ 'skia_os == "win"', { 'defines' : [ 'SK_CPU_SSE_LEVEL=31' ] }],
        [ 'not skia_android_framework', { 'cflags': [ '-mssse3' ] }],
      ],
    },
    {
      'target_name': 'opts_sse41',
      'product_name': 'skia_opts_sse41',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [ 'core.gyp:*' ],
      'include_dirs': [
          '../include/private',
          '../src/core',
          '../src/utils',
      ],
      'sources': [ '<!@(python read_gni.py ../gn/opts.gni sse41)' ],
      'xcode_settings': { 'GCC_ENABLE_SSE41_EXTENSIONS': 'YES' },
      'conditions': [
        [ 'skia_os == "win"', { 'defines' : [ 'SK_CPU_SSE_LEVEL=41' ] }],
        [ 'not skia_android_framework', { 'cflags': [ '-msse4.1' ] }],
      ],
    },
    {
      'target_name': 'opts_sse42',
      'product_name': 'skia_opts_sse42',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [ 'core.gyp:*' ],
      'include_dirs': [
          '../include/private',
          '../src/core',
          '../src/utils',
      ],
      'sources': [ '<!@(python read_gni.py ../gn/opts.gni sse42)' ],
      'xcode_settings': { 'GCC_ENABLE_SSE42_EXTENSIONS': 'YES' },
      'conditions': [
        [ 'skia_os == "win"', { 'defines' : [ 'SK_CPU_SSE_LEVEL=42' ] }],
        [ 'not skia_android_framework', { 'cflags': [ '-msse4.2' ] }],
      ],
    },
    {
      'target_name': 'opts_avx',
      'product_name': 'skia_opts_avx',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [ 'core.gyp:*' ],
      'include_dirs': [
          '../include/private',
          '../src/core',
          '../src/utils',
      ],
      'sources': [ '<!@(python read_gni.py ../gn/opts.gni avx)' ],
      'msvs_settings': { 'VCCLCompilerTool': { 'EnableEnhancedInstructionSet': '3' } },
      'xcode_settings': { 'OTHER_CPLUSPLUSFLAGS': [ '-mavx' ] },
      'conditions': [
        [ 'not skia_android_framework', { 'cflags': [ '-mavx' ] }],
      ],
    },
    {
      'target_name': 'opts_hsw',
      'product_name': 'skia_opts_hsw',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [ 'core.gyp:*' ],
      'include_dirs': [
          '../include/private',
          '../src/core',
          '../src/utils',
      ],
      'sources': [ '<!@(python read_gni.py ../gn/opts.gni hsw)' ],
      'msvs_settings': { 'VCCLCompilerTool': { 'EnableEnhancedInstructionSet': '5' } },
      'xcode_settings': {
          'OTHER_CPLUSPLUSFLAGS': [ '-mavx2', '-mbmi', '-mbmi2', '-mf16c', '-mfma' ]
      },
      'conditions': [
        [ 'not skia_android_framework', {
            'cflags': [ '-mavx2', '-mbmi', '-mbmi2', '-mf16c', '-mfma' ]
        }],
      ],
    },
    {
      'target_name': 'opts_neon',
      'product_name': 'skia_opts_neon',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'core.gyp:*',
        'effects.gyp:*'
      ],
      'include_dirs': [
        '../include/private',
        '../src/core',
        '../src/opts',
        '../src/utils',
      ],
      'sources': [ '<!@(python read_gni.py ../gn/opts.gni neon)' ],
      'cflags!': [
        '-fno-omit-frame-pointer',
        '-mfpu=vfp',  # remove them all, just in case.
        '-mfpu=vfpv3',
        '-mfpu=vfpv3-d16',
      ],
      'conditions': [
        [ 'not skia_android_framework', {
          'cflags': [
            '-mfpu=neon',
            '-fomit-frame-pointer',
          ],
        }],
      ],
      'ldflags': [
        '-march=armv7-a',
        '-Wl,--fix-cortex-a8',
      ],
    },
  ],
}
