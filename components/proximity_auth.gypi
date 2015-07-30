# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/proximity_auth and
      # //components/proximity_auth/ble.
      'target_name': 'proximity_auth',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        ':proximity_auth_logging',
        '../base/base.gyp:base',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
        '../net/net.gyp:net',
      ],
      'sources': [
        "proximity_auth/ble/bluetooth_low_energy_connection_finder.cc",
        "proximity_auth/ble/bluetooth_low_energy_connection_finder.h",
        "proximity_auth/ble/proximity_auth_ble_system.cc",
        "proximity_auth/ble/proximity_auth_ble_system.h",
        "proximity_auth/bluetooth_connection.cc",
        "proximity_auth/bluetooth_connection.h",
        "proximity_auth/bluetooth_connection_finder.cc",
        "proximity_auth/bluetooth_connection_finder.h",
        "proximity_auth/bluetooth_throttler.h",
        "proximity_auth/bluetooth_throttler_impl.cc",
        "proximity_auth/bluetooth_throttler_impl.h",
        "proximity_auth/bluetooth_util.cc",
        "proximity_auth/bluetooth_util.h",
        "proximity_auth/bluetooth_util_chromeos.cc",
        "proximity_auth/client.cc",
        "proximity_auth/client.h",
        "proximity_auth/client_observer.h",
        "proximity_auth/connection.cc",
        "proximity_auth/connection.h",
        "proximity_auth/connection_finder.h",
        "proximity_auth/connection_observer.h",
        "proximity_auth/proximity_auth_client.h",
        "proximity_auth/proximity_auth_system.cc",
        "proximity_auth/proximity_auth_system.h",
        "proximity_auth/remote_device.h",
        "proximity_auth/remote_status_update.cc",
        "proximity_auth/remote_status_update.h",
        "proximity_auth/screenlock_bridge.cc",
        "proximity_auth/screenlock_bridge.h",
        "proximity_auth/screenlock_state.h",
        "proximity_auth/secure_context.h",
        "proximity_auth/switches.cc",
        "proximity_auth/switches.h",
        "proximity_auth/throttled_bluetooth_connection_finder.cc",
        "proximity_auth/throttled_bluetooth_connection_finder.h",
        "proximity_auth/wire_message.cc",
        "proximity_auth/wire_message.h",
      ],
    },
    {
      # GN version: //components/proximity_auth/logging
      'target_name': 'proximity_auth_logging',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        "proximity_auth/logging/log_buffer.cc",
        "proximity_auth/logging/log_buffer.h",
        "proximity_auth/logging/logging.h",
        "proximity_auth/logging/logging.cc",
      ]
    },
    {
      # GN version: //components/proximity_auth/cryptauth/proto
      'target_name': 'cryptauth_proto',
      'type': 'static_library',
      'sources': [
        'proximity_auth/cryptauth/proto/cryptauth_api.proto',
        'proximity_auth/cryptauth/proto/securemessage.proto',
      ],
      'variables': {
        'proto_in_dir': 'proximity_auth/cryptauth/proto',
        'proto_out_dir': 'components/proximity_auth/cryptauth/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
    {
      'target_name': 'cryptauth',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'cryptauth_proto',
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
      ],
      'sources': [
        "proximity_auth/cryptauth/base64url.cc",
        "proximity_auth/cryptauth/base64url.h",
        "proximity_auth/cryptauth/cryptauth_access_token_fetcher.h",
        "proximity_auth/cryptauth/cryptauth_access_token_fetcher_impl.cc",
        "proximity_auth/cryptauth/cryptauth_access_token_fetcher_impl.h",
        "proximity_auth/cryptauth/cryptauth_api_call_flow.cc",
        "proximity_auth/cryptauth/cryptauth_api_call_flow.h",
        "proximity_auth/cryptauth/cryptauth_client.h",
        "proximity_auth/cryptauth/cryptauth_client_impl.cc",
        "proximity_auth/cryptauth/cryptauth_client_impl.h",
        "proximity_auth/cryptauth/cryptauth_enroller.h",
        "proximity_auth/cryptauth/cryptauth_enroller_impl.cc",
        "proximity_auth/cryptauth/cryptauth_enroller_impl.h",
        "proximity_auth/cryptauth/cryptauth_enrollment_utils.cc",
        "proximity_auth/cryptauth/cryptauth_enrollment_utils.h",
        "proximity_auth/cryptauth/secure_message_delegate.cc",
        "proximity_auth/cryptauth/secure_message_delegate.h",
      ],
      'export_dependent_settings': [
        'cryptauth_proto',
      ],
    },
    {
      'target_name': 'cryptauth_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'cryptauth_proto',
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
      ],
      'sources': [
        "proximity_auth/cryptauth/fake_secure_message_delegate.cc",
        "proximity_auth/cryptauth/fake_secure_message_delegate.h",
        "proximity_auth/cryptauth/mock_cryptauth_client.cc",
        "proximity_auth/cryptauth/mock_cryptauth_client.h",
      ],
      'export_dependent_settings': [
        'cryptauth_proto',
      ],
    },
    {
      # GN version: //components/proximity_auth/webui
      'target_name': 'proximity_auth_webui',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../content/content.gyp:content_browser',
        '../ui/resources/ui_resources.gyp:ui_resources',
        'components_resources.gyp:components_resources',
        'proximity_auth',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'proximity_auth/webui/proximity_auth_ui.cc',
        'proximity_auth/webui/proximity_auth_ui.h',
        'proximity_auth/webui/proximity_auth_webui_handler.cc',
        'proximity_auth/webui/proximity_auth_webui_handler.h',
        'proximity_auth/webui/url_constants.cc',
        'proximity_auth/webui/url_constants.h',
      ],
    },
  ],
}
