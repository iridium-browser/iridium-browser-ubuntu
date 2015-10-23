// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_

#include "base/basictypes.h"

namespace password_manager {
namespace prefs {

// Alphabetical list of preference names specific to the PasswordManager
// component.

// The value of this parameter is boolean that indicates whether
// "Allow to collect URL?" bubble was shown or not.
extern const char kAllowToCollectURLBubbleWasShown[];

// The value of this parameter is used to calculate the start day of the
// period, in which the "Allow to collect URL?" bubble can be shown.
extern const char kAllowToCollectURLBubbleActivePeriodStartFactor[];

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
// The local profile id for this profile.
extern const char kLocalProfileId[];
#endif

#if defined(OS_WIN)
// Whether the password was blank, only valid if OS password was last changed
// on or before the value contained in kOsPasswordLastChanged.
extern const char kOsPasswordBlank[];

// The number of seconds since epoch that the OS password was last changed.
extern const char kOsPasswordLastChanged[];
#endif

#if defined(OS_MACOSX)
// The current status of migrating the passwords from the Keychain to the
// database. Stores a value from MigrationStatus.
extern const char kKeychainMigrationStatus[];
#endif

// Boolean controlling whether the password manager allows to retrieve passwords
// in clear text.
extern const char kPasswordManagerAllowShowPasswords[];

// Boolean controlling whether the password manager allows automatic signing in
// through Credential Manager API.
extern const char kPasswordManagerAutoSignin[];

// Boolean that is true if password saving is on (will record new
// passwords and fill in known passwords). When it is false, it doesn't
// ask if you want to save passwords but will continue to fill passwords.
// Constant name and its value differ because of historical reasons as it
// was not deemed important enough to add migration code just for name
// change.
// See http://crbug.com/392387
extern const char kPasswordManagerSavingEnabled[];

// A list of numbers. Each number corresponds to one of the domains monitored
// for save-password-prompt breakages. That number is a random index into
// the array of groups containing the monitored domain. That group should be
// used for reporting that domain.
extern const char kPasswordManagerGroupsForDomains[];

}  // namespace prefs
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_PREF_NAMES_H_
