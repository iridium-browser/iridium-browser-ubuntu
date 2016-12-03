// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_

#include <vector>

#include "base/strings/string16.h"

class GlobalError;
class Profile;
class SigninManagerBase;

// Utility functions to gather status information from the various signed in
// services and construct messages suitable for showing in UI.
namespace signin_ui_util {

// The maximum number of times to show the welcome tutorial for an upgrade user.
const int kUpgradeWelcomeTutorialShowMax = 1;

// Returns the label that should be displayed in the signin menu (i.e.
// "Sign in to Chromium", "Signin Error...", etc).
base::string16 GetSigninMenuLabel(Profile* profile);

void GetStatusLabelsForAuthError(Profile* profile,
                                 const SigninManagerBase& signin_manager,
                                 base::string16* status_label,
                                 base::string16* link_label);

// Initializes signin-related preferences.
void InitializePrefsForProfile(Profile* profile);

// Shows a learn more page for signin errors.
void ShowSigninErrorLearnMorePage(Profile* profile);

// Returns the display email string for the given account.  If the profile
// has not been migrated to use gaia ids, then its possible for the display
// to not ne known yet.  In this case, use |account_id|, which is assumed to
// be an email address.
std::string GetDisplayEmail(Profile* profile, const std::string& account_id);

}  // namespace signin_ui_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_H_
