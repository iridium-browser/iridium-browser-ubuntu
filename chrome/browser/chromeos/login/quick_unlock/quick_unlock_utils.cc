// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

namespace {
bool enable_for_testing_ = false;
// Options for the quick unlock whitelist.
const char kQuickUnlockWhitelistOptionAll[] = "all";
const char kQuickUnlockWhitelistOptionPin[] = "PIN";
}  // namespace

void RegisterQuickUnlockProfilePrefs(PrefRegistrySimple* registry) {
  base::ListValue quick_unlock_whitelist_default;
  quick_unlock_whitelist_default.AppendString(kQuickUnlockWhitelistOptionPin);
  registry->RegisterListPref(prefs::kQuickUnlockModeWhitelist,
                             quick_unlock_whitelist_default.DeepCopy());
  registry->RegisterIntegerPref(
      prefs::kQuickUnlockTimeout,
      static_cast<int>(QuickUnlockPasswordConfirmationFrequency::DAY));

  // Preferences related the lock screen pin unlock.
  registry->RegisterIntegerPref(prefs::kPinUnlockMinimumLength, 4);
  // 0 indicates no maximum length for the pin.
  registry->RegisterIntegerPref(prefs::kPinUnlockMaximumLength, 0);
  registry->RegisterBooleanPref(prefs::kPinUnlockWeakPinsAllowed, true);
}

bool IsPinUnlockEnabled(PrefService* pref_service) {
  if (enable_for_testing_)
    return true;

  // Check if policy allows PIN.
  const base::ListValue* quick_unlock_whitelist =
      pref_service->GetList(prefs::kQuickUnlockModeWhitelist);
  base::StringValue all_value(kQuickUnlockWhitelistOptionAll);
  base::StringValue pin_value(kQuickUnlockWhitelistOptionPin);
  if (quick_unlock_whitelist->Find(all_value) ==
          quick_unlock_whitelist->end() &&
      quick_unlock_whitelist->Find(pin_value) ==
          quick_unlock_whitelist->end()) {
    return false;
  }

  // TODO(jdufault): Disable PIN for supervised users until we allow the owner
  // to set the PIN. See crbug.com/632797.
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (user && user->IsSupervised())
    return false;

  // Enable quick unlock only if the switch is present.
  return base::FeatureList::IsEnabled(features::kQuickUnlockPin);
}

void EnableQuickUnlockForTesting() {
  enable_for_testing_ = true;
}

}  // namespace chromeos
