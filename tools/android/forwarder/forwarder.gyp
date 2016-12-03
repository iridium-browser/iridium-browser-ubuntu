# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'forwarder',
      'type': 'executable',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../common/common.gyp:android_tools_common',
      ],
      'include_dirs': [
        '../../..',
      ],
      'sources': [
        'forwarder.cc',
      ],
    },
  ],
}

