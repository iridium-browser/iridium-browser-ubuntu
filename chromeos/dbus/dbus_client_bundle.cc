// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus_client_bundle.h"

#include <stddef.h>

#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/amplifier_client.h"
#include "chromeos/dbus/ap_manager_client.h"
#include "chromeos/dbus/arc_obb_mounter_client.h"
#include "chromeos/dbus/audio_dsp_client.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "chromeos/dbus/fake_amplifier_client.h"
#include "chromeos/dbus/fake_ap_manager_client.h"
#include "chromeos/dbus/fake_arc_obb_mounter_client.h"
#include "chromeos/dbus/fake_audio_dsp_client.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_easy_unlock_client.h"
#include "chromeos/dbus/fake_gsm_sms_client.h"
#include "chromeos/dbus/fake_image_burner_client.h"
#include "chromeos/dbus/fake_introspectable_client.h"
#include "chromeos/dbus/fake_lorgnette_manager_client.h"
#include "chromeos/dbus/fake_modem_messaging_client.h"
#include "chromeos/dbus/fake_nfc_adapter_client.h"
#include "chromeos/dbus/fake_nfc_device_client.h"
#include "chromeos/dbus/fake_nfc_manager_client.h"
#include "chromeos/dbus/fake_nfc_record_client.h"
#include "chromeos/dbus/fake_nfc_tag_client.h"
#include "chromeos/dbus/fake_peer_daemon_manager_client.h"
#include "chromeos/dbus/fake_permission_broker_client.h"
#include "chromeos/dbus/fake_privet_daemon_manager_client.h"
#include "chromeos/dbus/fake_shill_device_client.h"
#include "chromeos/dbus/fake_shill_ipconfig_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/dbus/fake_shill_profile_client.h"
#include "chromeos/dbus/fake_shill_service_client.h"
#include "chromeos/dbus/fake_shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/fake_sms_client.h"
#include "chromeos/dbus/fake_system_clock_client.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "chromeos/dbus/image_burner_client.h"
#include "chromeos/dbus/introspectable_client.h"
#include "chromeos/dbus/lorgnette_manager_client.h"
#include "chromeos/dbus/modem_messaging_client.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "chromeos/dbus/nfc_device_client.h"
#include "chromeos/dbus/nfc_manager_client.h"
#include "chromeos/dbus/nfc_record_client.h"
#include "chromeos/dbus/nfc_tag_client.h"
#include "chromeos/dbus/peer_daemon_manager_client.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/privet_daemon_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/dbus/shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/sms_client.h"
#include "chromeos/dbus/system_clock_client.h"
#include "chromeos/dbus/update_engine_client.h"

namespace chromeos {

namespace {

// Command line switch mapping for --dbus-unstub-clients.
const struct {
  const char* param_name;
  DBusClientBundle::DBusClientType client_type;
} client_type_map[] = {
    { "amplifier",  DBusClientBundle::AMPLIFIER },
    { "ap",  DBusClientBundle::AP_MANAGER },
    { "audio_dsp",  DBusClientBundle::AUDIO_DSP },
    { "bluetooth",  DBusClientBundle::BLUETOOTH },
    { "cras",  DBusClientBundle::CRAS },
    { "cros_disks",  DBusClientBundle::CROS_DISKS },
    { "cryptohome",  DBusClientBundle::CRYPTOHOME },
    { "debug_daemon",  DBusClientBundle::DEBUG_DAEMON },
    { "easy_unlock",  DBusClientBundle::EASY_UNLOCK },
    { "lorgnette_manager",  DBusClientBundle::LORGNETTE_MANAGER },
    { "shill",  DBusClientBundle::SHILL },
    { "gsm_sms",  DBusClientBundle::GSM_SMS },
    { "image_burner",  DBusClientBundle::IMAGE_BURNER },
    { "introspectable",  DBusClientBundle::INTROSPECTABLE },
    { "modem_messaging",  DBusClientBundle::MODEM_MESSAGING },
    { "nfc",  DBusClientBundle::NFC },
    { "peer_daemon",  DBusClientBundle::PEER_DAEMON },
    { "permission_broker",  DBusClientBundle::PERMISSION_BROKER },
    { "power_manager",  DBusClientBundle::POWER_MANAGER },
    { "privet_daemon", DBusClientBundle::PRIVET_DAEMON },
    { "session_manager",  DBusClientBundle::SESSION_MANAGER },
    { "sms",  DBusClientBundle::SMS },
    { "system_clock",  DBusClientBundle::SYSTEM_CLOCK },
    { "update_engine",  DBusClientBundle::UPDATE_ENGINE },
};

// Parses single command line param value for dbus subsystem. If successful,
// returns its enum representation. Otherwise returns NO_CLIENT.
DBusClientBundle::DBusClientType GetDBusClientType(
    const std::string& client_type_name) {
  for (size_t i = 0; i < arraysize(client_type_map); i++) {
    if (base::LowerCaseEqualsASCII(client_type_name,
                                   client_type_map[i].param_name))
      return client_type_map[i].client_type;
  }
  return DBusClientBundle::NO_CLIENT;
}

}  // namespace

DBusClientBundle::DBusClientBundle(DBusClientTypeMask unstub_client_mask)
    : unstub_client_mask_(unstub_client_mask) {
  if (!IsUsingStub(AMPLIFIER))
    amplifier_client_.reset(AmplifierClient::Create());
  else
    amplifier_client_.reset(new FakeAmplifierClient);

  if (!IsUsingStub(ARC_OBB_MOUNTER))
    arc_obb_mounter_client_.reset(ArcObbMounterClient::Create());
  else
    arc_obb_mounter_client_.reset(new FakeArcObbMounterClient);

  if (!IsUsingStub(AUDIO_DSP))
    audio_dsp_client_.reset(AudioDspClient::Create());
  else
    audio_dsp_client_.reset(new FakeAudioDspClient);

  if (!IsUsingStub(CRAS))
    cras_audio_client_.reset(CrasAudioClient::Create());
  else
    cras_audio_client_.reset(new FakeCrasAudioClient);

  cros_disks_client_.reset(CrosDisksClient::Create(
      IsUsingStub(CROS_DISKS) ? STUB_DBUS_CLIENT_IMPLEMENTATION
                              : REAL_DBUS_CLIENT_IMPLEMENTATION));

  if (!IsUsingStub(CRYPTOHOME))
    cryptohome_client_.reset(CryptohomeClient::Create());
  else
    cryptohome_client_.reset(new FakeCryptohomeClient);

  if (!IsUsingStub(DEBUG_DAEMON))
    debug_daemon_client_.reset(DebugDaemonClient::Create());
  else
    debug_daemon_client_.reset(new FakeDebugDaemonClient);

  if (!IsUsingStub(EASY_UNLOCK))
    easy_unlock_client_.reset(EasyUnlockClient::Create());
  else
    easy_unlock_client_.reset(new FakeEasyUnlockClient);

  if (!IsUsingStub(LORGNETTE_MANAGER))
    lorgnette_manager_client_.reset(LorgnetteManagerClient::Create());
  else
    lorgnette_manager_client_.reset(new FakeLorgnetteManagerClient);

  if (!IsUsingStub(SHILL)) {
    shill_manager_client_.reset(ShillManagerClient::Create());
    shill_device_client_.reset(ShillDeviceClient::Create());
    shill_ipconfig_client_.reset(ShillIPConfigClient::Create());
    shill_service_client_.reset(ShillServiceClient::Create());
    shill_profile_client_.reset(ShillProfileClient::Create());
    shill_third_party_vpn_driver_client_.reset(
        ShillThirdPartyVpnDriverClient::Create());
  } else {
    shill_manager_client_.reset(new FakeShillManagerClient);
    shill_device_client_.reset(new FakeShillDeviceClient);
    shill_ipconfig_client_.reset(new FakeShillIPConfigClient);
    shill_service_client_.reset(new FakeShillServiceClient);
    shill_profile_client_.reset(new FakeShillProfileClient);
    shill_third_party_vpn_driver_client_.reset(
        new FakeShillThirdPartyVpnDriverClient);
  }

  if (!IsUsingStub(GSM_SMS)) {
    gsm_sms_client_.reset(GsmSMSClient::Create());
  } else {
    FakeGsmSMSClient* gsm_sms_client = new FakeGsmSMSClient();
    gsm_sms_client->set_sms_test_message_switch_present(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kSmsTestMessages));
    gsm_sms_client_.reset(gsm_sms_client);
  }

  if (!IsUsingStub(IMAGE_BURNER))
    image_burner_client_.reset(ImageBurnerClient::Create());
  else
    image_burner_client_.reset(new FakeImageBurnerClient);

  if (!IsUsingStub(INTROSPECTABLE))
    introspectable_client_.reset(IntrospectableClient::Create());
  else
    introspectable_client_.reset(new FakeIntrospectableClient);

  if (!IsUsingStub(MODEM_MESSAGING))
    modem_messaging_client_.reset(ModemMessagingClient::Create());
  else
    modem_messaging_client_.reset(new FakeModemMessagingClient);

  // Create the NFC clients in the correct order based on their dependencies.
  if (!IsUsingStub(NFC)) {
    nfc_manager_client_.reset(NfcManagerClient::Create());
    nfc_adapter_client_.reset(
        NfcAdapterClient::Create(nfc_manager_client_.get()));
    nfc_device_client_.reset(
        NfcDeviceClient::Create(nfc_adapter_client_.get()));
    nfc_tag_client_.reset(NfcTagClient::Create(nfc_adapter_client_.get()));
    nfc_record_client_.reset(NfcRecordClient::Create(nfc_device_client_.get(),
                                                     nfc_tag_client_.get()));
  } else {
    nfc_manager_client_.reset(new FakeNfcManagerClient);
    nfc_adapter_client_.reset(new FakeNfcAdapterClient);
    nfc_device_client_.reset(new FakeNfcDeviceClient);
    nfc_tag_client_.reset(new FakeNfcTagClient);
    nfc_record_client_.reset(new FakeNfcRecordClient);
  }

  if (!IsUsingStub(PEER_DAEMON))
    peer_daemon_manager_client_.reset(PeerDaemonManagerClient::Create());
  else
    peer_daemon_manager_client_.reset(new FakePeerDaemonManagerClient);

  if (!IsUsingStub(PERMISSION_BROKER))
    permission_broker_client_.reset(PermissionBrokerClient::Create());
  else
    permission_broker_client_.reset(new FakePermissionBrokerClient);

  if (!IsUsingStub(PRIVET_DAEMON))
    privet_daemon_manager_client_.reset(PrivetDaemonManagerClient::Create());
  else
    privet_daemon_manager_client_.reset(new FakePrivetDaemonManagerClient);

  if (!IsUsingStub(AP_MANAGER))
    ap_manager_client_.reset(ApManagerClient::Create());
  else
    ap_manager_client_.reset(new FakeApManagerClient);

  power_manager_client_.reset(PowerManagerClient::Create(
      IsUsingStub(POWER_MANAGER) ? STUB_DBUS_CLIENT_IMPLEMENTATION
                                 : REAL_DBUS_CLIENT_IMPLEMENTATION));

  session_manager_client_.reset(SessionManagerClient::Create(
      IsUsingStub(SESSION_MANAGER) ? STUB_DBUS_CLIENT_IMPLEMENTATION
                                   : REAL_DBUS_CLIENT_IMPLEMENTATION));

  if (!IsUsingStub(SMS))
    sms_client_.reset(SMSClient::Create());
  else
    sms_client_.reset(new FakeSMSClient);

  if (!IsUsingStub(SYSTEM_CLOCK))
    system_clock_client_.reset(SystemClockClient::Create());
  else
    system_clock_client_.reset(new FakeSystemClockClient);

  update_engine_client_.reset(UpdateEngineClient::Create(
      IsUsingStub(UPDATE_ENGINE) ? STUB_DBUS_CLIENT_IMPLEMENTATION
                                 : REAL_DBUS_CLIENT_IMPLEMENTATION));
}

DBusClientBundle::~DBusClientBundle() {
}

bool DBusClientBundle::IsUsingStub(DBusClientType client) {
  return !(unstub_client_mask_ & client);
}

bool DBusClientBundle::IsUsingAnyRealClient() {
  // 'Using any real client' is equivalent to 'Unstubbed any client'.
  return unstub_client_mask_ != 0;
}

void DBusClientBundle::SetupDefaultEnvironment() {
  ShillManagerClient::TestInterface* manager =
      shill_manager_client_->GetTestInterface();
  if (manager)
    manager->SetupDefaultEnvironment();
}

// static
DBusClientBundle::DBusClientTypeMask DBusClientBundle::ParseUnstubList(
    const std::string& unstub_list) {
  DBusClientTypeMask unstub_mask = 0;
  for (const std::string& cur : base::SplitString(
           unstub_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    DBusClientBundle::DBusClientType client = GetDBusClientType(cur);
    if (client != NO_CLIENT) {
      LOG(WARNING) << "Unstubbing dbus client for " << cur;
      unstub_mask |= client;
    } else {
      LOG(ERROR) << "Unknown dbus client: " << cur;
    }
  }

  return unstub_mask;
}

}  // namespace chromeos
