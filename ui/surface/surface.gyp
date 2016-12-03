# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'target_defaults': {
    'conditions': [
      ['use_x11 == 1', {
        'include_dirs': [
          '../../third_party/khronos',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'surface',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../skia/skia.gyp:skia',
        '../base/ui_base.gyp:ui_base',
        '../gfx/gfx.gyp:gfx_geometry',
        '../gl/gl.gyp:gl',
        '../gl/init/gl_init.gyp:gl_init',
      ],
      'sources': [
        'surface_export.h',
        'transport_dib.cc',
        'transport_dib.h',
        'transport_dib_posix.cc',
        'transport_dib_win.cc',
      ],
      'defines': [
        'SURFACE_IMPLEMENTATION',
      ],
    },
  ],
}
