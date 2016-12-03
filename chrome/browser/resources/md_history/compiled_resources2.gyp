# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'constants',
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'browser_service',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:cr',
        '../history/compiled_resources2.gyp:externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'externs',
      'dependencies': [
        'constants',
        '../history/compiled_resources2.gyp:externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/include_js.gypi'],
    },
    {
      'target_name': 'grouped_list',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-collapse/compiled_resources2.gyp:iron-collapse-extracted',
        'constants',
        'history_item',
        'history_list_behavior',
        '../history/compiled_resources2.gyp:externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'history_item',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:icon',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:util',
        'constants',
        'browser_service',
        '../history/compiled_resources2.gyp:externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'history_list',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-list/compiled_resources2.gyp:iron-list-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-scroll-threshold/compiled_resources2.gyp:iron-scroll-threshold-extracted',
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_shared_menu/compiled_resources2.gyp:cr_shared_menu',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:util',
        'constants',
        'history_item',
        'history_list_behavior',
        '../history/compiled_resources2.gyp:externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'history_list_behavior',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        'browser_service',
        'constants',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'history_toolbar',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_toolbar/compiled_resources2.gyp:cr_toolbar',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:util',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'history',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:icon',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        'constants',
        'app',
        '<(EXTERNS_GYP):chrome_send',
        '../history/compiled_resources2.gyp:externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'lazy_render',
      'dependencies': [
        'externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'list_container',
      'dependencies': [
        'grouped_list',
        'history_list',
        'history_list_behavior',
        'lazy_render',
        '<(EXTERNS_GYP):chrome_send',
        '../history/compiled_resources2.gyp:externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'app',
      'dependencies': [
        # TODO(calamity): Add app-route elements after closure issues are fixed.
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:util',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:command',
        'constants',
        'history_toolbar',
        'list_container',
        'side_bar',
        'synced_device_card',
        'synced_device_manager',
        '../history/compiled_resources2.gyp:externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'side_bar',
      'dependencies': [
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/app-layout/app-drawer/compiled_resources2.gyp:app-drawer-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-a11y-keys-behavior/compiled_resources2.gyp:iron-a11y-keys-behavior-extracted',
        '<(DEPTH)/third_party/polymer/v1_0/components-chromium/iron-selector/compiled_resources2.gyp:iron-selector-extracted',
        'browser_service',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'synced_device_card',
      'dependencies': [
        '../history/compiled_resources2.gyp:externs',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:icon',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:load_time_data',
        'browser_service',
        'constants',
        'externs',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
    {
      'target_name': 'synced_device_manager',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/cr_elements/cr_shared_menu/compiled_resources2.gyp:cr_shared_menu',
        'browser_service',
        'lazy_render',
        'synced_device_card',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
