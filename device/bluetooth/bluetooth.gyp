# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      # GN version: //device/bluetooth
      'target_name': 'device_bluetooth',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../crypto/crypto.gyp:crypto',
        '../../net/net.gyp:net',
        '../../ui/base/ui_base.gyp:ui_base',
        'bluetooth_strings.gyp:bluetooth_strings',
        'uribeacon',
      ],
      'defines': [
        'DEVICE_BLUETOOTH_IMPLEMENTATION',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'android/bluetooth_jni_registrar.cc',
        'android/bluetooth_jni_registrar.h',
        'android/wrappers.cc',
        'android/wrappers.h',
        'bluetooth_adapter.cc',
        'bluetooth_adapter.h',
        'bluetooth_adapter_android.cc',
        'bluetooth_adapter_android.h',
        'bluetooth_adapter_factory.cc',
        'bluetooth_adapter_factory.h',
        'bluetooth_adapter_factory_wrapper.cc',
        'bluetooth_adapter_factory_wrapper.h',
        'bluetooth_adapter_mac.h',
        'bluetooth_adapter_mac.mm',
        'bluetooth_adapter_win.cc',
        'bluetooth_adapter_win.h',
        'bluetooth_advertisement.cc',
        'bluetooth_advertisement.h',
        'bluetooth_audio_sink.cc',
        'bluetooth_audio_sink.h',
        'bluetooth_channel_mac.mm',
        'bluetooth_channel_mac.h',
        'bluetooth_classic_device_mac.mm',
        'bluetooth_classic_device_mac.h',
        'bluetooth_classic_win.cc',
        'bluetooth_classic_win.h',
        'bluetooth_common.h',
        'bluetooth_device.cc',
        'bluetooth_device.h',
        'bluetooth_device_android.h',
        'bluetooth_device_android.cc',
        'bluetooth_device_mac.mm',
        'bluetooth_device_mac.h',
        'bluetooth_device_win.cc',
        'bluetooth_device_win.h',
        'bluetooth_discovery_filter.cc',
        'bluetooth_discovery_filter.h',
        'bluetooth_discovery_manager_mac.mm',
        'bluetooth_discovery_manager_mac.h',
        'bluetooth_discovery_session.cc',
        'bluetooth_discovery_session.h',
        'bluetooth_discovery_session_outcome.h',
        'bluetooth_gatt_characteristic.cc',
        'bluetooth_gatt_characteristic.h',
        'bluetooth_gatt_connection.cc',
        'bluetooth_gatt_connection.h',
        'bluetooth_gatt_descriptor.cc',
        'bluetooth_gatt_descriptor.h',
        'bluetooth_gatt_notify_session.cc',
        'bluetooth_gatt_notify_session.h',
        'bluetooth_gatt_service.cc',
        'bluetooth_gatt_service.h',
        'bluetooth_init_win.cc',
        'bluetooth_init_win.h',
        'bluetooth_l2cap_channel_mac.mm',
        'bluetooth_l2cap_channel_mac.h',
        'bluetooth_local_gatt_characteristic.cc',
        'bluetooth_local_gatt_characteristic.h',
        'bluetooth_local_gatt_descriptor.cc',
        'bluetooth_local_gatt_descriptor.h',
        'bluetooth_local_gatt_service.cc',
        'bluetooth_local_gatt_service.h',
        'bluetooth_low_energy_central_manager_delegate.mm',
        'bluetooth_low_energy_central_manager_delegate.h',
        'bluetooth_low_energy_defs_win.cc',
        'bluetooth_low_energy_defs_win.h',
        'bluetooth_low_energy_device_mac.h',
        'bluetooth_low_energy_device_mac.mm',
        'bluetooth_low_energy_discovery_manager_mac.h',
        'bluetooth_low_energy_discovery_manager_mac.mm',
        'bluetooth_low_energy_peripheral_delegate.mm',
        'bluetooth_low_energy_peripheral_delegate.h',
        'bluetooth_low_energy_win.cc',
        'bluetooth_low_energy_win.h',
        'bluetooth_remote_gatt_characteristic.cc',
        'bluetooth_remote_gatt_characteristic.h',
        'bluetooth_remote_gatt_characteristic_android.cc',
        'bluetooth_remote_gatt_characteristic_android.h',
        'bluetooth_remote_gatt_characteristic_mac.h',
        'bluetooth_remote_gatt_characteristic_mac.mm',
        'bluetooth_remote_gatt_characteristic_win.cc',
        'bluetooth_remote_gatt_characteristic_win.h',
        'bluetooth_remote_gatt_descriptor.cc',
        'bluetooth_remote_gatt_descriptor.h',
        'bluetooth_remote_gatt_descriptor_android.cc',
        'bluetooth_remote_gatt_descriptor_android.h',
        'bluetooth_remote_gatt_descriptor_win.cc',
        'bluetooth_remote_gatt_descriptor_win.h',
        'bluetooth_remote_gatt_service.cc',
        'bluetooth_remote_gatt_service.h',
        'bluetooth_remote_gatt_service_android.cc',
        'bluetooth_remote_gatt_service_android.h',
        'bluetooth_remote_gatt_service_mac.h',
        'bluetooth_remote_gatt_service_mac.mm',
        'bluetooth_remote_gatt_service_win.cc',
        'bluetooth_remote_gatt_service_win.h',
        'bluetooth_rfcomm_channel_mac.mm',
        'bluetooth_rfcomm_channel_mac.h',
        'bluetooth_service_record_win.cc',
        'bluetooth_service_record_win.h',
        'bluetooth_socket.cc',
        'bluetooth_socket.h',
        'bluetooth_socket_mac.h',
        'bluetooth_socket_mac.mm',
        'bluetooth_socket_net.cc',
        'bluetooth_socket_net.h',
        'bluetooth_socket_thread.cc',
        'bluetooth_socket_thread.h',
        'bluetooth_socket_win.cc',
        'bluetooth_socket_win.h',
        'bluetooth_task_manager_win.cc',
        'bluetooth_task_manager_win.h',
        'bluetooth_uuid.cc',
        'bluetooth_uuid.h',
      ],
      'conditions': [
        ['chromeos==1 or OS=="linux"', {
         'conditions': [
           ['use_dbus==1', {
             'defines': [
                'DEVICE_BLUETOOTH_IMPLEMENTATION',
              ],
              'sources': [
                'bluez/bluetooth_adapter_bluez.cc',
                'bluez/bluetooth_adapter_bluez.h',
                'bluez/bluetooth_adapter_profile_bluez.cc',
                'bluez/bluetooth_adapter_profile_bluez.h',
                'bluez/bluetooth_advertisement_bluez.cc',
                'bluez/bluetooth_advertisement_bluez.h',
                'bluez/bluetooth_audio_sink_bluez.cc',
                'bluez/bluetooth_audio_sink_bluez.h',
                'bluez/bluetooth_device_bluez.cc',
                'bluez/bluetooth_device_bluez.h',
                'bluez/bluetooth_gatt_characteristic_bluez.cc',
                'bluez/bluetooth_gatt_characteristic_bluez.h',
                'bluez/bluetooth_gatt_connection_bluez.cc',
                'bluez/bluetooth_gatt_connection_bluez.h',
                'bluez/bluetooth_gatt_descriptor_bluez.cc',
                'bluez/bluetooth_gatt_descriptor_bluez.h',
                'bluez/bluetooth_gatt_service_bluez.cc',
                'bluez/bluetooth_gatt_service_bluez.h',
                'bluez/bluetooth_local_gatt_characteristic_bluez.cc',
                'bluez/bluetooth_local_gatt_characteristic_bluez.h',
                'bluez/bluetooth_local_gatt_descriptor_bluez.cc',
                'bluez/bluetooth_local_gatt_descriptor_bluez.h',
                'bluez/bluetooth_local_gatt_service_bluez.cc',
                'bluez/bluetooth_local_gatt_service_bluez.h',
                'bluez/bluetooth_pairing_bluez.cc',
                'bluez/bluetooth_pairing_bluez.h',
                'bluez/bluetooth_remote_gatt_characteristic_bluez.cc',
                'bluez/bluetooth_remote_gatt_characteristic_bluez.h',
                'bluez/bluetooth_remote_gatt_descriptor_bluez.cc',
                'bluez/bluetooth_remote_gatt_descriptor_bluez.h',
                'bluez/bluetooth_remote_gatt_service_bluez.cc',
                'bluez/bluetooth_remote_gatt_service_bluez.h',
                'bluez/bluetooth_service_attribute_value_bluez.cc',
                'bluez/bluetooth_service_attribute_value_bluez.h',
                'bluez/bluetooth_service_record_bluez.cc',
                'bluez/bluetooth_service_record_bluez.h',
                'bluez/bluetooth_socket_bluez.cc',
                'bluez/bluetooth_socket_bluez.h',
                'dbus/bluetooth_adapter_client.cc',
                'dbus/bluetooth_adapter_client.h',
                'dbus/bluetooth_le_advertising_manager_client.cc',
                'dbus/bluetooth_le_advertising_manager_client.h',
                'dbus/bluetooth_le_advertisement_service_provider.cc',
                'dbus/bluetooth_le_advertisement_service_provider.h',
                'dbus/bluetooth_agent_manager_client.cc',
                'dbus/bluetooth_agent_manager_client.h',
                'dbus/bluetooth_agent_service_provider.cc',
                'dbus/bluetooth_agent_service_provider.h',
                'dbus/bluetooth_dbus_client_bundle.cc',
                'dbus/bluetooth_dbus_client_bundle.h',
                'dbus/bluetooth_device_client.cc',
                'dbus/bluetooth_device_client.h',
                'dbus/bluetooth_gatt_application_service_provider.cc',
                'dbus/bluetooth_gatt_application_service_provider.h',
                'dbus/bluetooth_gatt_application_service_provider_impl.cc',
                'dbus/bluetooth_gatt_application_service_provider_impl.h',
                'dbus/bluetooth_gatt_attribute_helpers.cc',
                'dbus/bluetooth_gatt_attribute_helpers.h',
                'dbus/bluetooth_gatt_attribute_value_delegate.cc',
                'dbus/bluetooth_gatt_attribute_value_delegate.h',
                'dbus/bluetooth_gatt_characteristic_client.cc',
                'dbus/bluetooth_gatt_characteristic_client.h',
                'dbus/bluetooth_gatt_characteristic_delegate_wrapper.cc',
                'dbus/bluetooth_gatt_characteristic_delegate_wrapper.h',
                'dbus/bluetooth_gatt_characteristic_service_provider_impl.cc',
                'dbus/bluetooth_gatt_characteristic_service_provider_impl.h',
                'dbus/bluetooth_gatt_characteristic_service_provider.cc',
                'dbus/bluetooth_gatt_characteristic_service_provider.h',
                'dbus/bluetooth_gatt_descriptor_delegate_wrapper.cc',
                'dbus/bluetooth_gatt_descriptor_delegate_wrapper.h',
                'dbus/bluetooth_gatt_descriptor_client.cc',
                'dbus/bluetooth_gatt_descriptor_client.h',
                'dbus/bluetooth_gatt_descriptor_service_provider_impl.cc',
                'dbus/bluetooth_gatt_descriptor_service_provider_impl.h',
                'dbus/bluetooth_gatt_descriptor_service_provider.cc',
                'dbus/bluetooth_gatt_descriptor_service_provider.h',
                'dbus/bluetooth_gatt_manager_client.cc',
                'dbus/bluetooth_gatt_manager_client.h',
                'dbus/bluetooth_gatt_service_client.cc',
                'dbus/bluetooth_gatt_service_client.h',
                'dbus/bluetooth_gatt_service_service_provider_impl.cc',
                'dbus/bluetooth_gatt_service_service_provider_impl.h',
                'dbus/bluetooth_gatt_service_service_provider.cc',
                'dbus/bluetooth_gatt_service_service_provider.h',
                'dbus/bluetooth_input_client.cc',
                'dbus/bluetooth_input_client.h',
                'dbus/bluetooth_media_client.cc',
                'dbus/bluetooth_media_client.h',
                'dbus/bluetooth_media_endpoint_service_provider.cc',
                'dbus/bluetooth_media_endpoint_service_provider.h',
                'dbus/bluetooth_media_transport_client.cc',
                'dbus/bluetooth_media_transport_client.h',
                'dbus/bluetooth_profile_manager_client.cc',
                'dbus/bluetooth_profile_manager_client.h',
                'dbus/bluetooth_profile_service_provider.cc',
                'dbus/bluetooth_profile_service_provider.h',
                'dbus/bluez_dbus_client.h',
                'dbus/bluez_dbus_manager.cc',
                'dbus/bluez_dbus_manager.h',
                'dbus/fake_bluetooth_adapter_client.cc',
                'dbus/fake_bluetooth_adapter_client.h',
                'dbus/fake_bluetooth_le_advertising_manager_client.cc',
                'dbus/fake_bluetooth_le_advertising_manager_client.h',
                'dbus/fake_bluetooth_le_advertisement_service_provider.cc',
                'dbus/fake_bluetooth_le_advertisement_service_provider.h',
                'dbus/fake_bluetooth_agent_manager_client.cc',
                'dbus/fake_bluetooth_agent_manager_client.h',
                'dbus/fake_bluetooth_agent_service_provider.cc',
                'dbus/fake_bluetooth_agent_service_provider.h',
                'dbus/fake_bluetooth_device_client.cc',
                'dbus/fake_bluetooth_device_client.h',
                'dbus/fake_bluetooth_gatt_application_service_provider.cc',
                'dbus/fake_bluetooth_gatt_application_service_provider.h',
                'dbus/fake_bluetooth_gatt_characteristic_client.cc',
                'dbus/fake_bluetooth_gatt_characteristic_client.h',
                'dbus/fake_bluetooth_gatt_characteristic_service_provider.cc',
                'dbus/fake_bluetooth_gatt_characteristic_service_provider.h',
                'dbus/fake_bluetooth_gatt_descriptor_client.cc',
                'dbus/fake_bluetooth_gatt_descriptor_client.h',
                'dbus/fake_bluetooth_gatt_descriptor_service_provider.cc',
                'dbus/fake_bluetooth_gatt_descriptor_service_provider.h',
                'dbus/fake_bluetooth_gatt_manager_client.cc',
                'dbus/fake_bluetooth_gatt_manager_client.h',
                'dbus/fake_bluetooth_gatt_service_client.cc',
                'dbus/fake_bluetooth_gatt_service_client.h',
                'dbus/fake_bluetooth_gatt_service_service_provider.cc',
                'dbus/fake_bluetooth_gatt_service_service_provider.h',
                'dbus/fake_bluetooth_input_client.cc',
                'dbus/fake_bluetooth_input_client.h',
                'dbus/fake_bluetooth_media_client.cc',
                'dbus/fake_bluetooth_media_client.h',
                'dbus/fake_bluetooth_media_endpoint_service_provider.cc',
                'dbus/fake_bluetooth_media_endpoint_service_provider.h',
                'dbus/fake_bluetooth_media_transport_client.cc',
                'dbus/fake_bluetooth_media_transport_client.h',
                'dbus/fake_bluetooth_profile_manager_client.cc',
                'dbus/fake_bluetooth_profile_manager_client.h',
                'dbus/fake_bluetooth_profile_service_provider.cc',
                'dbus/fake_bluetooth_profile_service_provider.h',
              ],
              'conditions': [
                 ['OS=="linux"', {
                   'sources': [
                     'dbus/dbus_bluez_manager_wrapper_linux.cc',
                     'dbus/dbus_bluez_manager_wrapper_linux.h',
                     'dbus/dbus_thread_manager_linux.cc',
                     'dbus/dbus_thread_manager_linux.h',
                   ]
                }]
              ],
              'dependencies': [
                '../../build/linux/system.gyp:dbus',
                '../../dbus/dbus.gyp:dbus',
              ],
              'export_dependent_settings': [
                '../../build/linux/system.gyp:dbus'
              ]
            }, {  # !use_dbus
              'sources': [ 'bluetooth_adapter_stub.cc' ],
              'conditions': [
                ['OS=="linux"', {
                  'sources': [
                    'dbus/dbus_bluez_manager_wrapper_linux.h',
                    'dbus/dbus_bluez_manager_wrapper_stub_linux.cc',
                  ]
               }],
              ]
            }],
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../../chromeos/chromeos.gyp:chromeos',
          ],
        }],
        ['OS == "android"', {
          'dependencies': [
            'device_bluetooth_java',
            'device_bluetooth_jni_headers',
          ],
        }],
        ['OS=="win"', {
          # The following two blocks are duplicated. They apply to static lib
          # and shared lib configurations respectively.
          'all_dependent_settings': {  # For static lib, apply to dependents.
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  'BluetoothApis.dll',
                  # Despite MSDN stating that Bthprops.dll contains the
                  # symbols declared by bthprops.lib, they actually reside here:
                  'Bthprops.cpl',
                  'setupapi.dll',
                ],
                'AdditionalDependencies': [
                  # Bthprops must be listed before BluetoothApis or else delay
                  # loading crashes.
                  'Bthprops.lib',
                  'BluetoothApis.lib',
                ],
              },
            },
          },
          'msvs_settings': {  # For shared lib, apply to self.
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'BluetoothApis.dll',
                # Despite MSDN stating that Bthprops.dll contains the
                # symbols declared by bthprops.lib, they actually reside here:
                'Bthprops.cpl',
                'setupapi.dll',
              ],
              'AdditionalDependencies': [
                # Bthprops must be listed before BluetoothApis or else delay
                # loading crashes.
                'Bthprops.lib',
                'BluetoothApis.lib',
              ],
            },
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOBluetooth.framework',
            ],
            'conditions': [
              ['mac_sdk == "10.10"', {
                'xcode_settings': {
                  # In the OSX 10.10 SDK, CoreBluetooth became a top level
                  # framework. Previously, it was nested in IOBluetooth. In
                  # order for Chrome to run on OSes older than OSX 10.10, the
                  # top level CoreBluetooth framework must be weakly linked.
                  'OTHER_LDFLAGS': [
                    '-weak_framework CoreBluetooth',
                  ],
                },
              }],
            ],
          },
        }],
      ],
    },
    {
      # GN version: //device/bluetooth/uribeacon
      'target_name': 'uribeacon',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'uribeacon/uri_encoder.cc',
        'uribeacon/uri_encoder.h'
      ]
    },
    {
      # GN version: //device/bluetooth:mocks
      'target_name': 'device_bluetooth_mocks',
      'type': 'static_library',
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        'device_bluetooth',
      ],
      'include_dirs': [
        '../../',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'test/mock_bluetooth_adapter.cc',
        'test/mock_bluetooth_adapter.h',
        'test/mock_bluetooth_advertisement.cc',
        'test/mock_bluetooth_advertisement.h',
        'test/mock_bluetooth_device.cc',
        'test/mock_bluetooth_device.h',
        'test/mock_bluetooth_discovery_session.cc',
        'test/mock_bluetooth_discovery_session.h',
        'test/mock_bluetooth_gatt_characteristic.cc',
        'test/mock_bluetooth_gatt_characteristic.h',
        'test/mock_bluetooth_gatt_connection.cc',
        'test/mock_bluetooth_gatt_connection.h',
        'test/mock_bluetooth_gatt_descriptor.cc',
        'test/mock_bluetooth_gatt_descriptor.h',
        'test/mock_bluetooth_gatt_notify_session.cc',
        'test/mock_bluetooth_gatt_notify_session.h',
        'test/mock_bluetooth_gatt_service.cc',
        'test/mock_bluetooth_gatt_service.h',
        'test/mock_bluetooth_socket.cc',
        'test/mock_bluetooth_socket.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'device_bluetooth_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/device/bluetooth/ChromeBluetoothAdapter.java',
            'android/java/src/org/chromium/device/bluetooth/ChromeBluetoothDevice.java',
            'android/java/src/org/chromium/device/bluetooth/ChromeBluetoothRemoteGattCharacteristic.java',
            'android/java/src/org/chromium/device/bluetooth/ChromeBluetoothRemoteGattDescriptor.java',
            'android/java/src/org/chromium/device/bluetooth/ChromeBluetoothRemoteGattService.java',
            'android/java/src/org/chromium/device/bluetooth/Wrappers.java',
          ],
          'variables': {
            'jni_gen_package': 'device_bluetooth',
          },
          'includes': [ '../../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'device_bluetooth_java',
          'type': 'none',
          'dependencies': [
            '../../base/base.gyp:base',
          ],
          'variables': {
            'java_in_dir': '../../device/bluetooth/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
      ],
    }],
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'bluetooth_interfaces_mojom',
          'type': 'none',
          'variables': {
            'mojom_files': [
              'public/interfaces/bluetooth_uuid.mojom',
            ],
            'mojom_typemaps': [
              'public/interfaces/bluetooth_uuid.typemap',
            ],
            'use_new_wrapper_types': 'false',
          },
          'includes': [ '../../mojo/mojom_bindings_generator_explicit.gypi' ],
        },
        {
          'target_name': 'bluetooth_interfaces_blink_mojom',
          'type': 'none',
          'variables': {
            'mojom_files': [
              'public/interfaces/bluetooth_uuid.mojom',
            ],
            'for_blink': 'true',
            'use_new_wrapper_types': 'false',
          },
          'includes': [ '../../mojo/mojom_bindings_generator_explicit.gypi' ],
        },
        {
          'target_name': 'bluetooth_mojom',
          'type': 'static_library',
          'export_dependent_settings': [
            '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
          ],
          'dependencies': [
            '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
            'bluetooth_interfaces_blink_mojom',
            'bluetooth_interfaces_mojom',
            'device_bluetooth',
          ],
        },
      ],
    }],
  ],
}
