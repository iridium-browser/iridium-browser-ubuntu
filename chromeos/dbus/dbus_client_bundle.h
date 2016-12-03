// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DBUS_CLIENT_BUNDLE_H_
#define CHROMEOS_DBUS_DBUS_CLIENT_BUNDLE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

class AmplifierClient;
class ApManagerClient;
class ArcObbMounterClient;
class AudioDspClient;
class CrasAudioClient;
class CrosDisksClient;
class CryptohomeClient;
class DebugDaemonClient;
class EasyUnlockClient;
class GsmSMSClient;
class ImageBurnerClient;
class IntrospectableClient;
class LorgnetteManagerClient;
class ModemMessagingClient;
class NfcAdapterClient;
class NfcDeviceClient;
class NfcManagerClient;
class NfcRecordClient;
class NfcTagClient;
class PeerDaemonManagerClient;
class PermissionBrokerClient;
class PowerManagerClient;
class PrivetDaemonManagerClient;
class SMSClient;
class SessionManagerClient;
class ShillDeviceClient;
class ShillIPConfigClient;
class ShillManagerClient;
class ShillProfileClient;
class ShillServiceClient;
class ShillThirdPartyVpnDriverClient;
class SystemClockClient;
class UpdateEngineClient;

// The bundle of all D-Bus clients used in DBusThreadManager. The bundle
// is used to delete them at once in the right order before shutting down the
// system bus. See also the comment in the destructor of DBusThreadManager.
class CHROMEOS_EXPORT DBusClientBundle {
 public:
  typedef int DBusClientTypeMask;

  // TODO(zelidrag): We might want to collapse few more of these subsystems if
  // their dbus interfaced correspond to the same daemon.
  enum DBusClientType {
    NO_CLIENT = 0,
    BLUETOOTH = 1 << 0,
    CRAS = 1 << 1,
    CROS_DISKS = 1 << 2,
    CRYPTOHOME = 1 << 3,
    DEBUG_DAEMON = 1 << 4,
    EASY_UNLOCK = 1 << 5,
    LORGNETTE_MANAGER = 1 << 6,
    SHILL = 1 << 7,
    GSM_SMS = 1 << 8,
    IMAGE_BURNER = 1 << 9,
    INTROSPECTABLE = 1 << 10,
    MODEM_MESSAGING = 1 << 11,
    NFC = 1 << 12,
    PERMISSION_BROKER = 1 << 13,
    POWER_MANAGER = 1 << 14,
    SESSION_MANAGER = 1 << 15,
    SMS = 1 << 16,
    SYSTEM_CLOCK = 1 << 17,
    UPDATE_ENGINE = 1 << 18,
    PEER_DAEMON = 1 << 19,
    AP_MANAGER = 1 << 20,
    PRIVET_DAEMON = 1 << 21,
    AMPLIFIER = 1 << 22,
    AUDIO_DSP = 1 << 23,
    ARC_OBB_MOUNTER = 1 << 24,
  };

  explicit DBusClientBundle(DBusClientTypeMask unstub_client_mask);
  ~DBusClientBundle();

  // Returns true if |client| is stubbed.
  bool IsUsingStub(DBusClientType client);

  // Returns true if any real DBusClient is used.
  bool IsUsingAnyRealClient();

  // Initialize proper runtime environment for its dbus clients.
  void SetupDefaultEnvironment();

  // Parses command line param values for dbus subsystem that should be
  // un-stubbed.
  static DBusClientTypeMask ParseUnstubList(const std::string& unstub_list);

  AmplifierClient* amplifier_client() { return amplifier_client_.get(); }

  ApManagerClient* ap_manager_client() { return ap_manager_client_.get(); }

  ArcObbMounterClient* arc_obb_mounter_client() {
    return arc_obb_mounter_client_.get();
  }

  AudioDspClient* audio_dsp_client() { return audio_dsp_client_.get(); }

  CrasAudioClient* cras_audio_client() {
    return cras_audio_client_.get();
  }

  CrosDisksClient* cros_disks_client() {
    return cros_disks_client_.get();
  }

  CryptohomeClient* cryptohome_client() {
    return cryptohome_client_.get();
  }

  DebugDaemonClient* debug_daemon_client() {
    return debug_daemon_client_.get();
  }

  EasyUnlockClient* easy_unlock_client() {
    return easy_unlock_client_.get();
  }

  LorgnetteManagerClient* lorgnette_manager_client() {
    return lorgnette_manager_client_.get();
  }

  ShillDeviceClient* shill_device_client() {
    return shill_device_client_.get();
  }

  ShillIPConfigClient* shill_ipconfig_client() {
    return shill_ipconfig_client_.get();
  }

  ShillManagerClient* shill_manager_client() {
    return shill_manager_client_.get();
  }

  ShillServiceClient* shill_service_client() {
    return shill_service_client_.get();
  }

  ShillProfileClient* shill_profile_client() {
    return shill_profile_client_.get();
  }

  ShillThirdPartyVpnDriverClient* shill_third_party_vpn_driver_client() {
    return shill_third_party_vpn_driver_client_.get();
  }

  GsmSMSClient* gsm_sms_client() {
    return gsm_sms_client_.get();
  }

  ImageBurnerClient* image_burner_client() {
    return image_burner_client_.get();
  }

  IntrospectableClient* introspectable_client() {
    return introspectable_client_.get();
  }

  ModemMessagingClient* modem_messaging_client() {
    return modem_messaging_client_.get();
  }

  NfcManagerClient* nfc_manager_client() {
    return nfc_manager_client_.get();
  }

  NfcAdapterClient* nfc_adapter_client() {
    return nfc_adapter_client_.get();
  }

  NfcDeviceClient* nfc_device_client() {
    return nfc_device_client_.get();
  }

  NfcTagClient* nfc_tag_client() {
    return nfc_tag_client_.get();
  }

  NfcRecordClient* nfc_record_client() {
    return nfc_record_client_.get();
  }

  PeerDaemonManagerClient* peer_daemon_manager_client() {
    return peer_daemon_manager_client_.get();
  }

  PermissionBrokerClient* permission_broker_client() {
    return permission_broker_client_.get();
  }

  PrivetDaemonManagerClient* privet_daemon_manager_client() {
    return privet_daemon_manager_client_.get();
  }

  SystemClockClient* system_clock_client() {
    return system_clock_client_.get();
  }

  PowerManagerClient* power_manager_client() {
    return power_manager_client_.get();
  }

  SessionManagerClient* session_manager_client() {
    return session_manager_client_.get();
  }

  SMSClient* sms_client() {
    return sms_client_.get();
  }

  UpdateEngineClient* update_engine_client() {
    return update_engine_client_.get();
  }

 private:
  friend class DBusThreadManagerSetter;

  // Bitmask that defines which dbus clients are not stubbed out. Bitmap flags
  // are defined within DBusClientType enum.
  DBusClientTypeMask unstub_client_mask_;

  std::unique_ptr<AmplifierClient> amplifier_client_;
  std::unique_ptr<ApManagerClient> ap_manager_client_;
  std::unique_ptr<ArcObbMounterClient> arc_obb_mounter_client_;
  std::unique_ptr<AudioDspClient> audio_dsp_client_;
  std::unique_ptr<CrasAudioClient> cras_audio_client_;
  std::unique_ptr<CrosDisksClient> cros_disks_client_;
  std::unique_ptr<CryptohomeClient> cryptohome_client_;
  std::unique_ptr<DebugDaemonClient> debug_daemon_client_;
  std::unique_ptr<EasyUnlockClient> easy_unlock_client_;
  std::unique_ptr<LorgnetteManagerClient> lorgnette_manager_client_;
  std::unique_ptr<PeerDaemonManagerClient> peer_daemon_manager_client_;
  std::unique_ptr<PrivetDaemonManagerClient> privet_daemon_manager_client_;
  std::unique_ptr<ShillDeviceClient> shill_device_client_;
  std::unique_ptr<ShillIPConfigClient> shill_ipconfig_client_;
  std::unique_ptr<ShillManagerClient> shill_manager_client_;
  std::unique_ptr<ShillServiceClient> shill_service_client_;
  std::unique_ptr<ShillProfileClient> shill_profile_client_;
  std::unique_ptr<ShillThirdPartyVpnDriverClient>
      shill_third_party_vpn_driver_client_;
  std::unique_ptr<GsmSMSClient> gsm_sms_client_;
  std::unique_ptr<ImageBurnerClient> image_burner_client_;
  std::unique_ptr<IntrospectableClient> introspectable_client_;
  std::unique_ptr<ModemMessagingClient> modem_messaging_client_;
  // The declaration order for NFC client objects is important. See
  // DBusThreadManager::InitializeClients for the dependencies.
  std::unique_ptr<NfcManagerClient> nfc_manager_client_;
  std::unique_ptr<NfcAdapterClient> nfc_adapter_client_;
  std::unique_ptr<NfcDeviceClient> nfc_device_client_;
  std::unique_ptr<NfcTagClient> nfc_tag_client_;
  std::unique_ptr<NfcRecordClient> nfc_record_client_;
  std::unique_ptr<PermissionBrokerClient> permission_broker_client_;
  std::unique_ptr<SystemClockClient> system_clock_client_;
  std::unique_ptr<PowerManagerClient> power_manager_client_;
  std::unique_ptr<SessionManagerClient> session_manager_client_;
  std::unique_ptr<SMSClient> sms_client_;
  std::unique_ptr<UpdateEngineClient> update_engine_client_;

  DISALLOW_COPY_AND_ASSIGN(DBusClientBundle);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DBUS_CLIENT_BUNDLE_H_
