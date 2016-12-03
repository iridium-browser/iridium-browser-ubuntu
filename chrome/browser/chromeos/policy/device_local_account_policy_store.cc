// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_policy_store.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/ownership/owner_key_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

DeviceLocalAccountPolicyStore::DeviceLocalAccountPolicyStore(
    const std::string& account_id,
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : UserCloudPolicyStoreBase(background_task_runner),
      account_id_(account_id),
      session_manager_client_(session_manager_client),
      device_settings_service_(device_settings_service),
      weak_factory_(this) {}

DeviceLocalAccountPolicyStore::~DeviceLocalAccountPolicyStore() {}

void DeviceLocalAccountPolicyStore::Load() {
  weak_factory_.InvalidateWeakPtrs();
  session_manager_client_->RetrieveDeviceLocalAccountPolicy(
      account_id_,
      base::Bind(&DeviceLocalAccountPolicyStore::ValidateLoadedPolicyBlob,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyStore::Store(
    const em::PolicyFetchResponse& policy) {
  weak_factory_.InvalidateWeakPtrs();
  CheckKeyAndValidate(
      true, base::MakeUnique<em::PolicyFetchResponse>(policy),
      base::Bind(&DeviceLocalAccountPolicyStore::StoreValidatedPolicy,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyStore::ValidateLoadedPolicyBlob(
    const std::string& policy_blob) {
  if (policy_blob.empty()) {
    status_ = CloudPolicyStore::STATUS_LOAD_ERROR;
    NotifyStoreError();
  } else {
    std::unique_ptr<em::PolicyFetchResponse> policy(
        new em::PolicyFetchResponse());
    if (policy->ParseFromString(policy_blob)) {
      CheckKeyAndValidate(
          false, std::move(policy),
          base::Bind(&DeviceLocalAccountPolicyStore::UpdatePolicy,
                     weak_factory_.GetWeakPtr()));
    } else {
      status_ = CloudPolicyStore::STATUS_PARSE_ERROR;
      NotifyStoreError();
    }
  }
}

void DeviceLocalAccountPolicyStore::UpdatePolicy(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  InstallPolicy(std::move(validator->policy_data()),
                std::move(validator->payload()));
  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

void DeviceLocalAccountPolicyStore::StoreValidatedPolicy(
    UserCloudPolicyValidator* validator) {
  if (!validator->success()) {
    status_ = CloudPolicyStore::STATUS_VALIDATION_ERROR;
    validation_status_ = validator->status();
    NotifyStoreError();
    return;
  }

  std::string policy_blob;
  if (!validator->policy()->SerializeToString(&policy_blob)) {
    status_ = CloudPolicyStore::STATUS_SERIALIZE_ERROR;
    NotifyStoreError();
    return;
  }

  session_manager_client_->StoreDeviceLocalAccountPolicy(
      account_id_,
      policy_blob,
      base::Bind(&DeviceLocalAccountPolicyStore::HandleStoreResult,
                 weak_factory_.GetWeakPtr()));
}

void DeviceLocalAccountPolicyStore::HandleStoreResult(bool success) {
  if (!success) {
    status_ = CloudPolicyStore::STATUS_STORE_ERROR;
    NotifyStoreError();
  } else {
    Load();
  }
}

void DeviceLocalAccountPolicyStore::CheckKeyAndValidate(
    bool valid_timestamp_required,
    std::unique_ptr<em::PolicyFetchResponse> policy,
    const UserCloudPolicyValidator::CompletionCallback& callback) {
  device_settings_service_->GetOwnershipStatusAsync(
      base::Bind(&DeviceLocalAccountPolicyStore::Validate,
                 weak_factory_.GetWeakPtr(),
                 valid_timestamp_required,
                 base::Passed(&policy),
                 callback));
}

void DeviceLocalAccountPolicyStore::Validate(
    bool valid_timestamp_required,
    std::unique_ptr<em::PolicyFetchResponse> policy_response,
    const UserCloudPolicyValidator::CompletionCallback& callback,
    chromeos::DeviceSettingsService::OwnershipStatus ownership_status) {
  DCHECK_NE(chromeos::DeviceSettingsService::OWNERSHIP_UNKNOWN,
            ownership_status);
  const em::PolicyData* device_policy_data =
      device_settings_service_->policy_data();
  scoped_refptr<ownership::PublicKey> key =
      device_settings_service_->GetPublicKey();
  if (!key.get() || !key->is_loaded() || !device_policy_data) {
    status_ = CloudPolicyStore::STATUS_BAD_STATE;
    NotifyStoreLoaded();
    return;
  }

  std::unique_ptr<UserCloudPolicyValidator> validator(
      UserCloudPolicyValidator::Create(std::move(policy_response),
                                       background_task_runner()));
  validator->ValidateUsername(account_id_, false);
  validator->ValidatePolicyType(dm_protocol::kChromePublicAccountPolicyType);
  // The timestamp is verified when storing a new policy downloaded from the
  // server but not when loading a cached policy from disk.
  // See SessionManagerOperation::ValidateDeviceSettings for the rationale.
  validator->ValidateAgainstCurrentPolicy(
      policy(),
      valid_timestamp_required
          ? CloudPolicyValidatorBase::TIMESTAMP_FULLY_VALIDATED
          : CloudPolicyValidatorBase::TIMESTAMP_NOT_VALIDATED,
      CloudPolicyValidatorBase::DM_TOKEN_NOT_REQUIRED);

  // Validate the DMToken to match what device policy has.
  validator->ValidateDMToken(device_policy_data->request_token(),
                             CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);

  validator->ValidatePayload();
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  validator->ValidateSignature(key->as_string(),
                               GetPolicyVerificationKey(),
                               connector->GetEnterpriseDomain(),
                               false);
  validator.release()->StartValidation(callback);
}

}  // namespace policy
