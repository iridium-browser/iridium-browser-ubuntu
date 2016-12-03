// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
#define COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/proximity_auth/authenticator.h"
#include "components/proximity_auth/connection_observer.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/cryptauth/cryptauth_device_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_gcm_manager.h"
#include "components/proximity_auth/logging/log_buffer.h"
#include "components/proximity_auth/messenger_observer.h"
#include "components/proximity_auth/proximity_auth_client.h"
#include "components/proximity_auth/remote_device_life_cycle.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace cryptauth {
class ExternalDeviceInfo;
}

namespace proximity_auth {

class ReachablePhoneFlow;
class RemoteDeviceLifeCycle;
class RemoteDeviceLoader;
struct RemoteStatusUpdate;

// Handles messages from the chrome://proximity-auth page.
class ProximityAuthWebUIHandler : public content::WebUIMessageHandler,
                                  public LogBuffer::Observer,
                                  public CryptAuthEnrollmentManager::Observer,
                                  public CryptAuthDeviceManager::Observer,
                                  public RemoteDeviceLifeCycle::Observer,
                                  public MessengerObserver {
 public:
  // |client_| is not owned and must outlive this instance.
  explicit ProximityAuthWebUIHandler(
      ProximityAuthClient* proximity_auth_client);
  ~ProximityAuthWebUIHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // LogBuffer::Observer:
  void OnLogMessageAdded(const LogBuffer::LogMessage& log_message) override;
  void OnLogBufferCleared() override;

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentStarted() override;
  void OnEnrollmentFinished(bool success) override;

  // CryptAuthDeviceManager::Observer:
  void OnSyncStarted() override;
  void OnSyncFinished(
      CryptAuthDeviceManager::SyncResult sync_result,
      CryptAuthDeviceManager::DeviceChangeResult device_change_result) override;

  // Message handler callbacks.
  void OnWebContentsInitialized(const base::ListValue* args);
  void GetLogMessages(const base::ListValue* args);
  void ClearLogBuffer(const base::ListValue* args);
  void ToggleUnlockKey(const base::ListValue* args);
  void FindEligibleUnlockDevices(const base::ListValue* args);
  void FindReachableDevices(const base::ListValue* args);
  void GetLocalState(const base::ListValue* args);
  void ForceEnrollment(const base::ListValue* args);
  void ForceDeviceSync(const base::ListValue* args);
  void ToggleConnection(const base::ListValue* args);

  // Initializes CryptAuth managers, used for development purposes.
  void InitGCMManager();
  void InitEnrollmentManager();
  void InitDeviceManager();

  // Called when a CryptAuth request fails.
  void OnCryptAuthClientError(const std::string& error_message);

  // Called when the toggleUnlock request succeeds.
  void OnEasyUnlockToggled(const cryptauth::ToggleEasyUnlockResponse& response);

  // Called when the findEligibleUnlockDevices request succeeds.
  void OnFoundEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesResponse& response);

  // Callback when |reachable_phone_flow_| completes.
  void OnReachablePhonesFound(
      const std::vector<cryptauth::ExternalDeviceInfo>& reachable_phones);

  // Called when the RemoteDevice is loaded so we can create a connection.
  void OnRemoteDevicesLoaded(const std::vector<RemoteDevice>& remote_devices);

  // Converts an ExternalDeviceInfo proto to a JSON dictionary used in
  // JavaScript.
  std::unique_ptr<base::DictionaryValue> ExternalDeviceInfoToDictionary(
      const cryptauth::ExternalDeviceInfo& device_info);

  // Converts an IneligibleDevice proto to a JSON dictionary used in JavaScript.
  std::unique_ptr<base::DictionaryValue> IneligibleDeviceToDictionary(
      const cryptauth::IneligibleDevice& ineligible_device);

  // Cleans up the connection to the selected remote device.
  void CleanUpRemoteDeviceLifeCycle();

  // RemoteDeviceLifeCycle::Observer:
  void OnLifeCycleStateChanged(RemoteDeviceLifeCycle::State old_state,
                               RemoteDeviceLifeCycle::State new_state) override;

  // MessengerObserver:
  void OnRemoteStatusUpdate(const RemoteStatusUpdate& status_update) override;

  // Returns the current enrollment state that can be used as a JSON object.
  std::unique_ptr<base::DictionaryValue> GetEnrollmentStateDictionary();

  // Returns the current device sync state that can be used as a JSON object.
  std::unique_ptr<base::DictionaryValue> GetDeviceSyncStateDictionary();

  // Returns the current unlock keys that can be used as a JSON object.
  std::unique_ptr<base::ListValue> GetUnlockKeysList();

  // The delegate used to fetch dependencies. Must outlive this instance.
  ProximityAuthClient* proximity_auth_client_;

  // Creates CryptAuth client instances to make API calls.
  std::unique_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // We only support one concurrent API call.
  std::unique_ptr<CryptAuthClient> cryptauth_client_;

  // The flow for getting a list of reachable phones.
  std::unique_ptr<ReachablePhoneFlow> reachable_phone_flow_;

  // True if we get a message from the loaded WebContents to know that it is
  // initialized, and we can inject JavaScript.
  bool web_contents_initialized_;

  // Member variables for connecting to and authenticating the remote device.
  // TODO(tengs): Support multiple simultaenous connections.
  std::unique_ptr<RemoteDeviceLoader> remote_device_loader_;
  RemoteDevice selected_remote_device_;
  std::unique_ptr<RemoteDeviceLifeCycle> life_cycle_;
  std::unique_ptr<RemoteStatusUpdate> last_remote_status_update_;

  base::WeakPtrFactory<ProximityAuthWebUIHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthWebUIHandler);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
