// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chromeos/settings/cros_settings_names.h"

namespace chromeos {

StubCrosSettingsProvider::StubCrosSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  SetDefaults();
}

StubCrosSettingsProvider::StubCrosSettingsProvider()
  : CrosSettingsProvider(CrosSettingsProvider::NotifyObserversCallback()) {
  SetDefaults();
}

StubCrosSettingsProvider::~StubCrosSettingsProvider() {
}

const base::Value* StubCrosSettingsProvider::Get(
    const std::string& path) const {
  DCHECK(HandlesSetting(path));
  const base::Value* value;
  if (values_.GetValue(path, &value))
    return value;
  return NULL;
}

CrosSettingsProvider::TrustedStatus
    StubCrosSettingsProvider::PrepareTrustedValues(const base::Closure& cb) {
  return trusted_status_;
}

bool StubCrosSettingsProvider::HandlesSetting(const std::string& path) const {
  return DeviceSettingsProvider::IsDeviceSetting(path);
}

void StubCrosSettingsProvider::SetTrustedStatus(TrustedStatus status) {
  trusted_status_ = status;
}

void StubCrosSettingsProvider::SetCurrentUserIsOwner(bool owner) {
  current_user_is_owner_ = owner;
}

void StubCrosSettingsProvider::DoSet(const std::string& path,
                                     const base::Value& value) {
  if (current_user_is_owner_)
    values_.SetValue(path, value.CreateDeepCopy());
  else
    LOG(WARNING) << "Changing settings from non-owner, setting=" << path;
  NotifyObservers(path);
}

void StubCrosSettingsProvider::SetDefaults() {
  values_.SetBoolean(kAccountsPrefAllowGuest, true);
  values_.SetBoolean(kAccountsPrefAllowNewUser, true);
  values_.SetBoolean(kAccountsPrefSupervisedUsersEnabled, true);
  values_.SetBoolean(kAccountsPrefShowUserNamesOnSignIn, true);
  values_.SetValue(kAccountsPrefUsers, make_scoped_ptr(new base::ListValue));
  values_.SetBoolean(kAttestationForContentProtectionEnabled, true);
  values_.SetBoolean(kStatsReportingPref, true);
  values_.SetValue(kAccountsPrefDeviceLocalAccounts,
                   make_scoped_ptr(new base::ListValue));
  // |kDeviceOwner| will be set to the logged-in user by |UserManager|.
}

}  // namespace chromeos
