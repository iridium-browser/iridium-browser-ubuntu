// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_POWER_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_POWER_POWER_PREFS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefChangeRegistrar;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class PowerPolicyController;

// Sends an updated power policy to the |power_policy_controller| whenever one
// of the power-related prefs changes.
class PowerPrefs : public content::NotificationObserver {
 public:
  explicit PowerPrefs(PowerPolicyController* power_policy_controller);
  ~PowerPrefs() override;

  // Register power prefs with default values applicable to a user profile.
  static void RegisterUserProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry);

  // Register power prefs with default values applicable to the login profile.
  static void RegisterLoginProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void UpdatePowerPolicyFromPrefs();

 private:
  friend class PowerPrefsTest;

  // Register power prefs whose default values are the same in user profiles and
  // the login profile.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void SetProfile(Profile* profile);

  PowerPolicyController* power_policy_controller_;  // Not owned.

  content::NotificationRegistrar notification_registrar_;

  Profile* profile_;  // Not owned.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // True while the screen is locked (but not while the login screen is shown).
  bool screen_is_locked_;

  DISALLOW_COPY_AND_ASSIGN(PowerPrefs);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_POWER_PREFS_H_
