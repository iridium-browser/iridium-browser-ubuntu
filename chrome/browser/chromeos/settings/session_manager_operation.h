// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_SESSION_MANAGER_OPERATION_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_SESSION_MANAGER_OPERATION_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_validator.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "net/cert/x509_util_nss.h"

namespace enterprise_management {
class ChromeDeviceSettingsProto;
class PolicyData;
class PolicyFetchResponse;
}

namespace ownership {
class OwnerKeyUtil;
class PublicKey;
}

namespace chromeos {

class SessionManagerClient;

// Handles a single transaction with session manager. This is a virtual base
// class that contains common infrastructure for key and policy loading. There
// are subclasses for loading, storing and signing policy blobs.
class SessionManagerOperation {
 public:
  typedef base::Callback<void(SessionManagerOperation*,
                              DeviceSettingsService::Status)> Callback;

  // Creates a new load operation.
  explicit SessionManagerOperation(const Callback& callback);
  virtual ~SessionManagerOperation();

  // Starts the operation.
  void Start(SessionManagerClient* session_manager_client,
             scoped_refptr<ownership::OwnerKeyUtil> owner_key_util,
             scoped_refptr<ownership::PublicKey> public_key);

  // Restarts a load operation (if that part is already in progress).
  void RestartLoad(bool key_changed);

  // Accessors for recovering the loaded policy data after completion.
  std::unique_ptr<enterprise_management::PolicyData>& policy_data() {
    return policy_data_;
  }
  std::unique_ptr<enterprise_management::ChromeDeviceSettingsProto>&
  device_settings() {
    return device_settings_;
  }

  // Public part of the owner key as configured/loaded from disk.
  scoped_refptr<ownership::PublicKey> public_key() { return public_key_; }

  // Whether the load operation is underway.
  bool is_loading() const { return is_loading_; }

  void set_force_key_load(bool force_key_load) {
    force_key_load_ = force_key_load;
  }

 protected:
  // Runs the operation. The result is reported through |callback_|.
  virtual void Run() = 0;

  // Ensures the public key is loaded.
  void EnsurePublicKey(const base::Closure& callback);

  // Starts a load operation.
  void StartLoading();

  // Reports the result status of the operation. Once this gets called, the
  // operation should not perform further processing or trigger callbacks.
  void ReportResult(DeviceSettingsService::Status status);

  SessionManagerClient* session_manager_client() {
    return session_manager_client_;
  }

 private:
  // Loads the owner key from disk. Must be run on a thread that can do I/O.
  static scoped_refptr<ownership::PublicKey> LoadPublicKey(
      scoped_refptr<ownership::OwnerKeyUtil> util,
      scoped_refptr<ownership::PublicKey> current_key);

  // Stores the owner key loaded by LoadOwnerKey and calls |callback|.
  void StorePublicKey(const base::Closure& callback,
                      scoped_refptr<ownership::PublicKey> new_key);

  // Triggers a device settings load.
  void RetrieveDeviceSettings();

  // Validates device settings after retrieval from session_manager.
  void ValidateDeviceSettings(const std::string& policy_blob);

  // Extracts status and device settings from the validator and reports them.
  void ReportValidatorStatus(policy::DeviceCloudPolicyValidator* validator);

  SessionManagerClient* session_manager_client_;
  scoped_refptr<ownership::OwnerKeyUtil> owner_key_util_;

  Callback callback_;

  scoped_refptr<ownership::PublicKey> public_key_;
  bool force_key_load_;

  bool is_loading_;
  std::unique_ptr<enterprise_management::PolicyData> policy_data_;
  std::unique_ptr<enterprise_management::ChromeDeviceSettingsProto>
      device_settings_;

  base::WeakPtrFactory<SessionManagerOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SessionManagerOperation);
};

// This operation loads the public owner key from disk if appropriate, fetches
// the policy blob from session manager, and validates the loaded policy blob.
class LoadSettingsOperation : public SessionManagerOperation {
 public:
  // Creates a new load operation.
  explicit LoadSettingsOperation(const Callback& callback);
  ~LoadSettingsOperation() override;

 protected:
  // SessionManagerOperation:
  void Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoadSettingsOperation);
};

// Stores a pre-generated policy blob and reloads the device settings from
// session_manager.
class StoreSettingsOperation : public SessionManagerOperation {
 public:
  // Creates a new store operation.
  StoreSettingsOperation(
      const Callback& callback,
      std::unique_ptr<enterprise_management::PolicyFetchResponse> policy);
  ~StoreSettingsOperation() override;

 protected:
  // SessionManagerOperation:
  void Run() override;

 private:
  // Handles the result of the store operation and triggers the load.
  void HandleStoreResult(bool success);

  std::unique_ptr<enterprise_management::PolicyFetchResponse> policy_;

  base::WeakPtrFactory<StoreSettingsOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StoreSettingsOperation);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_SESSION_MANAGER_OPERATION_H_
