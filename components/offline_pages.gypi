# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN: //components/offline_pages:offline_pages
      'target_name': 'offline_pages',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        'components.gyp:leveldb_proto',
        'keyed_service_core',
        'offline_pages_proto',
      ],
      'sources': [
        'offline_pages/offline_page_archiver.h',
        'offline_pages/offline_page_feature.cc',
        'offline_pages/offline_page_feature.h',
        'offline_pages/offline_page_item.cc',
        'offline_pages/offline_page_item.h',
        'offline_pages/offline_page_model.cc',
        'offline_pages/offline_page_model.h',
        'offline_pages/offline_page_metadata_store.cc',
        'offline_pages/offline_page_metadata_store.h',
        'offline_pages/offline_page_metadata_store_impl.cc',
        'offline_pages/offline_page_metadata_store_impl.h',
        'offline_pages/offline_page_switches.cc',
        'offline_pages/offline_page_switches.h',
      ],
    },
    {
      # Protobuf compiler / generator for the offline page item protocol buffer.
      # GN version: //components/offline_pages/proto
      'target_name': 'offline_pages_proto',
      'type': 'static_library',
      'sources': [ 'offline_pages/proto/offline_pages.proto', ],
      'variables': {
        'proto_in_dir': 'offline_pages/proto',
        'proto_out_dir': 'components/offline_pages/proto',
      },
      'includes': [ '../build/protoc.gypi', ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN: //components/offline_pages:offline_pages_enums_java
          'target_name': 'offline_pages_enums_java',
          'type': 'none',
          'variables': {
            'source_file': 'offline_pages/offline_page_model.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
      ],
    }],
  ],
}
