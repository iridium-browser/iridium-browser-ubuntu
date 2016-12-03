# Copyright 2015 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'include_dirs': [
    '../bench/subset',
    '../bench',
    '../include/private',
    '../src/core',
    '../src/effects',
    '../src/gpu',
    '../src/pdf',
    '../src/utils',
  ],
  'sources': [ '<!@(python find.py "*.cpp" ../bench)' ],

  'dependencies': [
    'etc1.gyp:libetc1',
    'pdf.gyp:pdf',
    'skia_lib.gyp:skia_lib',
    'tools.gyp:resources',
    'tools.gyp:sk_tool_utils',
    'tools.gyp:url_data_manager',
  ],
  'conditions': [
    ['skia_gpu == 1', {
      'include_dirs': [ '../src/gpu' ],
      'dependencies': [ 'gputest.gyp:skgputest' ],
    }],
    ['not skia_android_framework', {
        'sources!': [ '../bench/nanobenchAndroid.cpp' ],
    }],
  ],
}
