// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/fake_device_cloud_policy_initializer.h"

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace policy {

FakeDeviceCloudPolicyInitializer::FakeDeviceCloudPolicyInitializer()
    : DeviceCloudPolicyInitializer(
          NULL,  // local_state
          NULL,  // enterprise_service
          NULL,  // consumer_service
          // background_task_runner
          scoped_refptr<base::SequencedTaskRunner>(NULL),
          NULL,  // install_attributes
          NULL,  // state_keys_broker
          NULL,  // device_store
          NULL),  // manager
      was_start_enrollment_called_(false),
      enrollment_status_(EnrollmentStatus::ForStatus(
          EnrollmentStatus::STATUS_SUCCESS)) {
}

void FakeDeviceCloudPolicyInitializer::Init() {
}

void FakeDeviceCloudPolicyInitializer::Shutdown() {
}

void FakeDeviceCloudPolicyInitializer::StartEnrollment(
    ManagementMode management_mode,
    DeviceManagementService* device_management_service,
    chromeos::OwnerSettingsServiceChromeOS* owner_settings_service,
    const EnrollmentConfig& enrollment_config,
    const std::string& auth_token,
    const AllowedDeviceModes& allowed_modes,
    const EnrollmentCallback& enrollment_callback) {
  was_start_enrollment_called_ = true;
  enrollment_callback.Run(enrollment_status_);
}

}  // namespace policy
