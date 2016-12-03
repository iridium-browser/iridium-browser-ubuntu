# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../gpu/gpu.gyp:command_buffer_traits',
    '../gpu/gpu.gyp:gpu',
    '../gpu/gpu.gyp:gpu_ipc_service',
    '../media/gpu/ipc/media_ipc.gyp:media_gpu_ipc_service',
    '../media/media.gyp:media_gpu',
    '../skia/skia.gyp:skia',
    '../ui/gl/gl.gyp:gl',
    '../ui/gl/init/gl_init.gyp:gl_init',
    'content_common_mojo_bindings.gyp:content_common_mojo_bindings',
  ],
  'sources': [
    'gpu/gpu_child_thread.cc',
    'gpu/gpu_child_thread.h',
    'gpu/gpu_main.cc',
    'gpu/gpu_process.cc',
    'gpu/gpu_process.h',
    'gpu/gpu_service_factory.cc',
    'gpu/gpu_service_factory.h',
    'gpu/gpu_watchdog_thread.cc',
    'gpu/gpu_watchdog_thread.h',
    'gpu/in_process_gpu_thread.cc',
    'gpu/in_process_gpu_thread.h',
    'public/gpu/content_gpu_client.cc',
    'public/gpu/content_gpu_client.h',
  ],
  'include_dirs': [
    '..',
  ],
  'conditions': [
    ['OS=="win"', {
      'include_dirs': [
        '<(DEPTH)/third_party/khronos',
        # ANGLE libs picked up from ui/gl
        '<(angle_path)/src',
        '<(DEPTH)/third_party/wtl/include',
      ],
      'link_settings': {
        'libraries': [
          '-lsetupapi.lib',
        ],
      },
    }],
    ['target_arch!="arm" and chromeos == 1', {
      'include_dirs': [
        '<(DEPTH)/third_party/libva',
      ],
    }],
    ['OS=="android"', {
      'dependencies': [
        '<(DEPTH)/media/media.gyp:player_android',
      ],
    }],
  ],
}
