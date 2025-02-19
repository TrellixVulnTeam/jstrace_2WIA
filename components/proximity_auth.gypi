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
        ':cryptauth',
        ':cryptauth_proto',
        ':proximity_auth_logging',
        '../base/base.gyp:base',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
        '../net/net.gyp:net',
        'prefs/prefs.gyp:prefs',
      ],
      'sources': [
        "proximity_auth/authenticator.h",
        "proximity_auth/ble/bluetooth_low_energy_characteristics_finder.cc",
        "proximity_auth/ble/bluetooth_low_energy_characteristics_finder.h",
        "proximity_auth/ble/bluetooth_low_energy_connection.cc",
        "proximity_auth/ble/bluetooth_low_energy_connection.h",
        "proximity_auth/ble/bluetooth_low_energy_connection_finder.cc",
        "proximity_auth/ble/bluetooth_low_energy_connection_finder.h",
        "proximity_auth/ble/bluetooth_low_energy_device_whitelist.cc",
        "proximity_auth/ble/bluetooth_low_energy_device_whitelist.h",
	"proximity_auth/ble/bluetooth_low_energy_weave_client_connection.cc",
	"proximity_auth/ble/bluetooth_low_energy_weave_client_connection.h",
	"proximity_auth/ble/bluetooth_low_energy_weave_defines.h",
	"proximity_auth/ble/bluetooth_low_energy_weave_packet_generator.cc",
	"proximity_auth/ble/bluetooth_low_energy_weave_packet_generator.h",
	"proximity_auth/ble/bluetooth_low_energy_weave_packet_receiver.cc",
	"proximity_auth/ble/bluetooth_low_energy_weave_packet_receiver.h",
        "proximity_auth/ble/remote_attribute.h",
        "proximity_auth/ble/fake_wire_message.cc",
        "proximity_auth/ble/fake_wire_message.h",
        "proximity_auth/ble/pref_names.cc",
        "proximity_auth/ble/pref_names.h",
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
        "proximity_auth/cryptauth_enroller_factory_impl.cc",
        "proximity_auth/cryptauth_enroller_factory_impl.h",
        "proximity_auth/connection.cc",
        "proximity_auth/connection.h",
        "proximity_auth/connection_finder.h",
        "proximity_auth/connection_observer.h",
        "proximity_auth/device_to_device_authenticator.cc",
        "proximity_auth/device_to_device_authenticator.h",
        "proximity_auth/device_to_device_initiator_operations.cc",
        "proximity_auth/device_to_device_initiator_operations.h",
        "proximity_auth/device_to_device_secure_context.cc",
        "proximity_auth/device_to_device_secure_context.h",
        "proximity_auth/messenger.h",
        "proximity_auth/messenger_impl.cc",
        "proximity_auth/messenger_impl.h",
        "proximity_auth/messenger_observer.h",
        "proximity_auth/metrics.cc",
        "proximity_auth/metrics.h",
        "proximity_auth/proximity_auth_client.h",
        "proximity_auth/proximity_auth_pref_manager.cc",
        "proximity_auth/proximity_auth_pref_manager.h",
        "proximity_auth/proximity_auth_pref_names.cc",
        "proximity_auth/proximity_auth_pref_names.h",
        "proximity_auth/proximity_auth_system.cc",
        "proximity_auth/proximity_auth_system.h",
        "proximity_auth/proximity_monitor.h",
        "proximity_auth/proximity_monitor_impl.cc",
        "proximity_auth/proximity_monitor_impl.h",
        "proximity_auth/proximity_monitor_observer.h",
        "proximity_auth/remote_device.cc",
        "proximity_auth/remote_device.h",
        "proximity_auth/remote_device_loader.cc",
        "proximity_auth/remote_device_loader.h",
        "proximity_auth/remote_device_life_cycle.h",
        "proximity_auth/remote_device_life_cycle_impl.h",
        "proximity_auth/remote_device_life_cycle_impl.cc",
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
        "proximity_auth/unlock_manager.cc",
        "proximity_auth/unlock_manager.h",
        "proximity_auth/wire_message.cc",
        "proximity_auth/wire_message.h",
      ],

      'export_dependent_settings': [
        'cryptauth_proto',
      ],
    },
    {
      'target_name': 'proximity_auth_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        ':cryptauth_test_support',
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
      ],
      'sources': [
        "proximity_auth/device_to_device_responder_operations.cc",
        "proximity_auth/device_to_device_responder_operations.h",
        "proximity_auth/fake_connection.cc",
        "proximity_auth/fake_connection.h",
        "proximity_auth/fake_secure_context.cc",
        "proximity_auth/fake_secure_context.h",
        "proximity_auth/mock_proximity_auth_client.cc",
        "proximity_auth/mock_proximity_auth_client.h",
        "proximity_auth/proximity_auth_test_util.cc",
        "proximity_auth/proximity_auth_test_util.h",
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
        '../crypto/crypto.gyp:crypto',
        '../components/components.gyp:gcm_driver',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
      ],
      'sources': [
        "proximity_auth/cryptauth/cryptauth_access_token_fetcher.h",
        "proximity_auth/cryptauth/cryptauth_access_token_fetcher_impl.cc",
        "proximity_auth/cryptauth/cryptauth_access_token_fetcher_impl.h",
        "proximity_auth/cryptauth/cryptauth_api_call_flow.cc",
        "proximity_auth/cryptauth/cryptauth_api_call_flow.h",
        "proximity_auth/cryptauth/cryptauth_client.h",
        "proximity_auth/cryptauth/cryptauth_client_impl.cc",
        "proximity_auth/cryptauth/cryptauth_client_impl.h",
        "proximity_auth/cryptauth/cryptauth_device_manager.cc",
        "proximity_auth/cryptauth/cryptauth_device_manager.h",
        "proximity_auth/cryptauth/cryptauth_enroller.h",
        "proximity_auth/cryptauth/cryptauth_enroller_impl.cc",
        "proximity_auth/cryptauth/cryptauth_enroller_impl.h",
        "proximity_auth/cryptauth/cryptauth_enrollment_manager.cc",
        "proximity_auth/cryptauth/cryptauth_enrollment_manager.h",
        "proximity_auth/cryptauth/cryptauth_enrollment_utils.cc",
        "proximity_auth/cryptauth/cryptauth_gcm_manager.cc",
        "proximity_auth/cryptauth/cryptauth_gcm_manager.h",
        "proximity_auth/cryptauth/cryptauth_gcm_manager_impl.cc",
        "proximity_auth/cryptauth/cryptauth_gcm_manager_impl.h",
        "proximity_auth/cryptauth/pref_names.cc",
        "proximity_auth/cryptauth/pref_names.h",
        "proximity_auth/cryptauth/secure_message_delegate.cc",
        "proximity_auth/cryptauth/secure_message_delegate.h",
        "proximity_auth/cryptauth/switches.cc",
        "proximity_auth/cryptauth/switches.h",
        "proximity_auth/cryptauth/sync_scheduler.cc",
        "proximity_auth/cryptauth/sync_scheduler.h",
        "proximity_auth/cryptauth/sync_scheduler_impl.cc",
        "proximity_auth/cryptauth/sync_scheduler_impl.h",
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
        "proximity_auth/cryptauth/fake_cryptauth_gcm_manager.cc",
        "proximity_auth/cryptauth/fake_cryptauth_gcm_manager.h",
        "proximity_auth/cryptauth/fake_secure_message_delegate.cc",
        "proximity_auth/cryptauth/fake_secure_message_delegate.h",
        "proximity_auth/cryptauth/mock_cryptauth_client.cc",
        "proximity_auth/cryptauth/mock_cryptauth_client.h",
        "proximity_auth/cryptauth/mock_sync_scheduler.cc",
        "proximity_auth/cryptauth/mock_sync_scheduler.h",
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
        'cryptauth',
        'cryptauth_proto',
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
        'proximity_auth/webui/reachable_phone_flow.cc',
        'proximity_auth/webui/reachable_phone_flow.h',
        'proximity_auth/webui/url_constants.cc',
        'proximity_auth/webui/url_constants.h',
      ],
    },
  ],
}
