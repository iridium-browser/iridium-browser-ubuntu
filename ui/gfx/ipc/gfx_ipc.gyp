# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
     {
      # GN version: //ui/gfx/ipc
      'target_name': 'gfx_ipc',
      'type': '<(component)',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../ipc/ipc.gyp:ipc',
        '../gfx.gyp:gfx',
        '../gfx.gyp:gfx_geometry',
        '../gfx.gyp:gfx_range',
        'color/gfx_ipc_color.gyp:gfx_ipc_color',
        'geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
      ],
      'defines': [
        'GFX_IPC_IMPLEMENTATION',
      ],
      'include_dirs': [
        '../../..',
      ],
      'sources': [
        'gfx_param_traits.cc',
        'gfx_param_traits.h',
        'gfx_param_traits_macros.h',
      ],
    },
  ],
}
