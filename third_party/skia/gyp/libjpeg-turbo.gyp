# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'xcode_settings': {
    'SYMROOT': '<(DEPTH)/xcodebuild',
  },
  'variables': {
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/externals/libjpeg-turbo',
    'conditions': [
      [ 'skia_os == "win"', {
        'object_suffix': 'obj',
      }, {
        'object_suffix': 'o',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'yasm-win',
      'type': 'executable',
      'sources': [
        '../third_party/externals/yasm/binaries/win/yasm.exe',
      ],
      'copies' : [{
        'destination': '<(PRODUCT_DIR)',
        'files': [ '../third_party/externals/yasm/binaries/win/yasm.exe' ],
      }],
    },
    {
      'target_name': 'libjpeg-turbo',
      'type': 'static_library',
      'include_dirs': [
        '../third_party/externals/libjpeg-turbo/',
      ],
      'defines': [
        'WITH_SIMD',
        'MOTION_JPEG_SUPPORTED',
        'NO_GETENV',
      ],
      'cflags': [
        '-w', # supresses warnings
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'WarningLevel': '0',
        },
      },
      'xcode_settings': {
        'WARNING_CFLAGS': [
          '-w',
        ],
      },
      'sources': [
        '../third_party/externals/libjpeg-turbo/jcapimin.c',
        '../third_party/externals/libjpeg-turbo/jcapistd.c',
        '../third_party/externals/libjpeg-turbo/jccoefct.c',
        '../third_party/externals/libjpeg-turbo/jccolor.c',
        '../third_party/externals/libjpeg-turbo/jcdctmgr.c',
        '../third_party/externals/libjpeg-turbo/jchuff.c',
        '../third_party/externals/libjpeg-turbo/jchuff.h',
        '../third_party/externals/libjpeg-turbo/jcinit.c',
        '../third_party/externals/libjpeg-turbo/jcmainct.c',
        '../third_party/externals/libjpeg-turbo/jcmarker.c',
        '../third_party/externals/libjpeg-turbo/jcmaster.c',
        '../third_party/externals/libjpeg-turbo/jcomapi.c',
        '../third_party/externals/libjpeg-turbo/jconfig.h',
        '../third_party/externals/libjpeg-turbo/jcparam.c',
        '../third_party/externals/libjpeg-turbo/jcphuff.c',
        '../third_party/externals/libjpeg-turbo/jcprepct.c',
        '../third_party/externals/libjpeg-turbo/jcsample.c',
        '../third_party/externals/libjpeg-turbo/jdapimin.c',
        '../third_party/externals/libjpeg-turbo/jdapistd.c',
        '../third_party/externals/libjpeg-turbo/jdcoefct.c',
        '../third_party/externals/libjpeg-turbo/jdcolor.c',
        '../third_party/externals/libjpeg-turbo/jdct.h',
        '../third_party/externals/libjpeg-turbo/jddctmgr.c',
        '../third_party/externals/libjpeg-turbo/jdhuff.c',
        '../third_party/externals/libjpeg-turbo/jdhuff.h',
        '../third_party/externals/libjpeg-turbo/jdinput.c',
        '../third_party/externals/libjpeg-turbo/jdmainct.c',
        '../third_party/externals/libjpeg-turbo/jdmarker.c',
        '../third_party/externals/libjpeg-turbo/jdmaster.c',
        '../third_party/externals/libjpeg-turbo/jdmerge.c',
        '../third_party/externals/libjpeg-turbo/jdphuff.c',
        '../third_party/externals/libjpeg-turbo/jdpostct.c',
        '../third_party/externals/libjpeg-turbo/jdsample.c',
        '../third_party/externals/libjpeg-turbo/jerror.c',
        '../third_party/externals/libjpeg-turbo/jerror.h',
        '../third_party/externals/libjpeg-turbo/jfdctflt.c',
        '../third_party/externals/libjpeg-turbo/jfdctfst.c',
        '../third_party/externals/libjpeg-turbo/jfdctint.c',
        '../third_party/externals/libjpeg-turbo/jidctflt.c',
        '../third_party/externals/libjpeg-turbo/jidctfst.c',
        '../third_party/externals/libjpeg-turbo/jidctint.c',
        '../third_party/externals/libjpeg-turbo/jidctred.c',
        '../third_party/externals/libjpeg-turbo/jinclude.h',
        '../third_party/externals/libjpeg-turbo/jmemmgr.c',
        '../third_party/externals/libjpeg-turbo/jmemnobs.c',
        '../third_party/externals/libjpeg-turbo/jmemsys.h',
        '../third_party/externals/libjpeg-turbo/jmorecfg.h',
        '../third_party/externals/libjpeg-turbo/jpegint.h',
        '../third_party/externals/libjpeg-turbo/jpeglib.h',
        '../third_party/externals/libjpeg-turbo/jpeglibmangler.h',
        '../third_party/externals/libjpeg-turbo/jquant1.c',
        '../third_party/externals/libjpeg-turbo/jquant2.c',
        '../third_party/externals/libjpeg-turbo/jutils.c',
        '../third_party/externals/libjpeg-turbo/jversion.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../third_party/externals/libjpeg-turbo/',
        ],
      },
      'msvs_disabled_warnings': [4018, 4101],
      # VS2010 does not correctly incrementally link obj files generated
      # from asm files. This flag disables UseLibraryDependencyInputs to
      # avoid this problem.
      'msvs_2010_disable_uldi_when_referenced': 1,

      # Add target-specific source files.
      'conditions': [
        # TODO (msarett): Is it possible to enable cross compiling for Android on other platforms?
        [ 'skia_os == "android" and host_os != "linux" and "x86" in skia_arch_type', {
           'sources': [
             '../third_party/externals/libjpeg-turbo/jsimd_none.c',
           ],
        }],
        [ 'skia_arch_type == "x86" and (skia_os != "android" or host_os == "linux")', {
          'sources': [
            '../third_party/externals/libjpeg-turbo/simd/jsimd_i386.c',
            '../third_party/externals/libjpeg-turbo/simd/jccolmmx.asm',
            '../third_party/externals/libjpeg-turbo/simd/jccolss2.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcgrammx.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcgrass2.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcqnt3dn.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcqntmmx.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcqnts2f.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcqnts2i.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcqntsse.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcsammmx.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcsamss2.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdcolmmx.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdcolss2.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdmermmx.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdmerss2.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdsammmx.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdsamss2.asm',
            '../third_party/externals/libjpeg-turbo/simd/jf3dnflt.asm',
            '../third_party/externals/libjpeg-turbo/simd/jfmmxfst.asm',
            '../third_party/externals/libjpeg-turbo/simd/jfmmxint.asm',
            '../third_party/externals/libjpeg-turbo/simd/jfss2fst.asm',
            '../third_party/externals/libjpeg-turbo/simd/jfss2int.asm',
            '../third_party/externals/libjpeg-turbo/simd/jfsseflt.asm',
            '../third_party/externals/libjpeg-turbo/simd/ji3dnflt.asm',
            '../third_party/externals/libjpeg-turbo/simd/jimmxfst.asm',
            '../third_party/externals/libjpeg-turbo/simd/jimmxint.asm',
            '../third_party/externals/libjpeg-turbo/simd/jimmxred.asm',
            '../third_party/externals/libjpeg-turbo/simd/jiss2flt.asm',
            '../third_party/externals/libjpeg-turbo/simd/jiss2fst.asm',
            '../third_party/externals/libjpeg-turbo/simd/jiss2int.asm',
            '../third_party/externals/libjpeg-turbo/simd/jiss2red.asm',
            '../third_party/externals/libjpeg-turbo/simd/jisseflt.asm',
            '../third_party/externals/libjpeg-turbo/simd/jsimdcpu.asm',
          ],
        }],
        [ 'skia_arch_type == "x86_64" and (skia_os != "android" or host_os == "linux")', {
          'sources': [
            '../third_party/externals/libjpeg-turbo/simd/jsimd_x86_64.c',
            '../third_party/externals/libjpeg-turbo/simd/jccolss2-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcgrass2-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcqnts2f-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcqnts2i-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jcsamss2-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdcolss2-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdmerss2-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jdsamss2-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jfss2fst-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jfss2int-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jfsseflt-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jiss2flt-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jiss2fst-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jiss2int-64.asm',
            '../third_party/externals/libjpeg-turbo/simd/jiss2red-64.asm',
          ],
        }],
        [ 'skia_arch_type == "arm64"', {
          'sources': [
            '../third_party/externals/libjpeg-turbo/simd/jsimd_arm64.c',
            '../third_party/externals/libjpeg-turbo/simd/jsimd_arm64_neon.S',
          ],
        }],
        [ 'skia_arch_type == "arm"', {
          'conditions': [
            [ 'arm_version >= 7 and (arm_neon == 1 or arm_neon_optional == 1)', {
              'sources': [
                '../third_party/externals/libjpeg-turbo/simd/jsimd_arm.c',
                '../third_party/externals/libjpeg-turbo/simd/jsimd_arm_neon.S',
              ],
            }, {
              'sources': [
                '../third_party/externals/libjpeg-turbo/jsimd_none.c',
              ],
            }],
          ],
        }],
        # mips simd is supported in later versions of libjpeg-turbo.  If we
        # choose to update our copy of libjpeg-turbo we can enable mips simd.
        [ '"mips" in skia_arch_type', {
          'sources': [
            '../third_party/externals/libjpeg-turbo/jsimd_none.c',
          ],
        }],
      
        # Build rules for an asm file.
        # On Windows, we use the precompiled yasm binary.
        # On Linux, we build our patched yasm and use it.
        # On Mac, we always build our patched yasm and use it.
        [ 'skia_os == "win"', {
          'dependencies': [
            'yasm-win',
          ],
          'variables': {
            'yasm_path': '<(PRODUCT_DIR)/yasm.exe',
            'conditions': [
              [ 'skia_arch_type == "x86"', {
                'yasm_format': '-fwin32',
                'yasm_flags': [
                  '-D__x86__',
                  '-DWIN32',
                  '-DMSVC',
                  '-I../third_party/externals/libjpeg-turbo/win/'
                ],
              }, {
                'yasm_format': '-fwin64',
                'yasm_flags': [
                  '-D__x86_64__',
                  '-DWIN64',
                  '-DMSVC',
                  '-I../third_party/externals/libjpeg-turbo/win/'
                ],
              }],
            ],
          },
        }],
        [ 'skia_os == "android" and host_os == "linux" and \
          (skia_arch_type == "x86" or skia_arch_type == "x86_64")', {
          'dependencies': [
            'yasm.gyp:yasm#host',
          ],
          'variables': {
            'yasm_path': '<(PRODUCT_DIR)/yasm',
            'conditions': [
              [ 'skia_arch_type == "x86"', {
                'yasm_format': '-felf',
                'yasm_flags': [
                  '-D__x86__',
                  '-DELF',
                  '-I../third_party/externals/libjpeg-turbo/linux/'
                ],
              }, {
                'yasm_format': '-felf64',
                'yasm_flags': [
                  '-D__x86_64__',
                  '-DELF',
                  '-I../third_party/externals/libjpeg-turbo/linux/'
                ],
              }],
            ],
          },
        }],
        [ '(skia_os == "mac" or skia_os == "ios") and \
           (skia_arch_type == "x86" or skia_arch_type == "x86_64")', {
          'dependencies': [
            'yasm.gyp:yasm#host',
          ],
          'variables': {
            'yasm_path': '<(PRODUCT_DIR)/yasm',
            'conditions': [
              [ 'skia_arch_type == "x86"', {
                'yasm_format': '-fmacho',
                'yasm_flags': [
                  '-D__x86__',
                  '-DMACHO',
                  '-I../third_party/externals/libjpeg-turbo/mac/'
                ],
              }, {
                'yasm_format': '-fmacho64',
                'yasm_flags': [
                  '-D__x86_64__',
                  '-DMACHO',
                  '-I../third_party/externals/libjpeg-turbo/mac/'
                ],
              }],
            ],
          },
        }],
        [ '(skia_os == "linux" or skia_os == "freebsd" or skia_os == "openbsd" or \
            skia_os == "solaris" or skia_os == "chromeos")', {
          'dependencies': [
            'yasm.gyp:yasm#host',
          ],
          'variables': {
            'yasm_path': '<(PRODUCT_DIR)/yasm',
            'conditions': [
              [ 'skia_arch_type == "x86"', {
                'yasm_format': '-felf',
                'yasm_flags': [
                  '-D__x86__',
                  '-DELF',
                  '-I../third_party/externals/libjpeg-turbo/linux/'
                ],
              }, {
                'yasm_format': '-felf64',
                'yasm_flags': [
                  '-D__x86_64__',
                  '-DELF',
                  '-I../third_party/externals/libjpeg-turbo/linux/'
                ],
              }],
            ],
          },
        }],
      ],
      'rules': [
        {
          'rule_name': 'assemble',
          'extension': 'asm',
          'conditions': [
            [ '(skia_arch_type == "x86" or skia_arch_type == "x86_64") and \
               (skia_os != "android" or host_os == "linux")', {
              'inputs': [],
              'outputs': [
                '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
              ],
              'action': [
                '<(yasm_path)',
                '<(yasm_format)',
                '<@(yasm_flags)',
                '-DRGBX_FILLER_0XFF',
                '-DSTRICT_MEMORY_ACCESS',
                '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
                '<(RULE_INPUT_PATH)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Building <(RULE_INPUT_ROOT).<(object_suffix)',
            }],
          ]
        },
      ],
    },
  ],
}
