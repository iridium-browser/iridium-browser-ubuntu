# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

{
  'variables': {
    'audio_coding_dependencies': [
      'cng',
      'g711',
      'g722',
      'ilbc',
      'isac',
      'isac_fix',
      'pcm16b',
      'red',
      '<(webrtc_root)/common.gyp:webrtc_common',
      '<(webrtc_root)/common_audio/common_audio.gyp:common_audio',
      '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
    ],
    'audio_coding_defines': [],
    'conditions': [
      ['include_opus==1', {
        'audio_coding_dependencies': ['webrtc_opus',],
        'audio_coding_defines': ['WEBRTC_CODEC_OPUS',],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'audio_coding_module',
      'type': 'static_library',
      'defines': [
        '<@(audio_coding_defines)',
      ],
      'dependencies': [
        '<@(audio_coding_dependencies)',
        '<(webrtc_root)/common.gyp:webrtc_common',
        'neteq',
      ],
      'include_dirs': [
        'interface',
        '../../interface',
        '<(webrtc_root)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'interface',
          '../../interface',
          '<(webrtc_root)',
        ],
      },
      'sources': [
        'acm2/acm_codec_database.cc',
        'acm2/acm_codec_database.h',
        'acm2/acm_common_defs.h',
        'acm2/acm_receiver.cc',
        'acm2/acm_receiver.h',
        'acm2/acm_resampler.cc',
        'acm2/acm_resampler.h',
        'acm2/audio_coding_module.cc',
        'acm2/audio_coding_module_impl.cc',
        'acm2/audio_coding_module_impl.h',
        'acm2/call_statistics.cc',
        'acm2/call_statistics.h',
        'acm2/codec_manager.cc',
        'acm2/codec_manager.h',
        'acm2/codec_owner.cc',
        'acm2/codec_owner.h',
        'acm2/initial_delay_manager.cc',
        'acm2/initial_delay_manager.h',
        'acm2/nack.cc',
        'acm2/nack.h',
        'interface/audio_coding_module.h',
        'interface/audio_coding_module_typedefs.h',
      ],
    },
  ],
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'acm_receive_test',
          'type': 'static_library',
          'defines': [
            '<@(audio_coding_defines)',
          ],
          'dependencies': [
            '<@(audio_coding_dependencies)',
            'audio_coding_module',
            'neteq_unittest_tools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'acm2/acm_receive_test.cc',
            'acm2/acm_receive_test.h',
            'acm2/acm_receive_test_oldapi.cc',
            'acm2/acm_receive_test_oldapi.h',
          ],
        }, # acm_receive_test
        {
          'target_name': 'acm_send_test',
          'type': 'static_library',
          'defines': [
            '<@(audio_coding_defines)',
          ],
          'dependencies': [
            '<@(audio_coding_dependencies)',
            'audio_coding_module',
            'neteq_unittest_tools',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'sources': [
            'acm2/acm_send_test.cc',
            'acm2/acm_send_test.h',
            'acm2/acm_send_test_oldapi.cc',
            'acm2/acm_send_test_oldapi.h',
          ],
        }, # acm_send_test
        {
          'target_name': 'delay_test',
          'type': 'executable',
          'dependencies': [
            'audio_coding_module',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/common.gyp:webrtc_common',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
          ],
          'sources': [
             'test/delay_test.cc',
             'test/Channel.cc',
             'test/PCMFile.cc',
             'test/utility.cc',
           ],
        }, # delay_test
        {
          'target_name': 'insert_packet_with_timing',
          'type': 'executable',
          'dependencies': [
            'audio_coding_module',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(webrtc_root)/common.gyp:webrtc_common',
            '<(webrtc_root)/test/test.gyp:test_support',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers',
            '<(webrtc_root)/system_wrappers/system_wrappers.gyp:system_wrappers_default',
            '<(DEPTH)/third_party/gflags/gflags.gyp:gflags',
          ],
          'sources': [
             'test/insert_packet_with_timing.cc',
             'test/Channel.cc',
             'test/PCMFile.cc',
           ],
        }, # delay_test
      ],
    }],
  ],
}
