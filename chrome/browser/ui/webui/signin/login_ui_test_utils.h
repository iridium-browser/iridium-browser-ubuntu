// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_

#include <string>

class Browser;

namespace login_ui_test_utils {

// Blocks until the login UI is available and ready for authorization.
void WaitUntilUIReady(Browser* browser);

// Blocks until an element with id |element_id| exists in the signin page.
void WaitUntilElementExistsInSigninFrame(Browser* browser,
                                         const std::string& element_id);

// Returns whether an element with id |element_id| exists in the signin page.
bool ElementExistsInSigninFrame(Browser* browser,
                                const std::string& element_id);

// Executes JavaScript code to sign in a user with email and password to the
// auth iframe hosted by gaia_auth extension. This function automatically
// detects the version of GAIA sign in page to use.
void ExecuteJsToSigninInSigninFrame(Browser* browser,
                                    const std::string& email,
                                    const std::string& password);

// Executes JS to sign in the user in the new GAIA sign in flow.
void SigninInNewGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password);

// Executes JS to sign in the user in the old GAIA sign in flow.
void SigninInOldGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password);

// A function to sign in a user using Chrome sign-in UI interface.
// This will block until a signin succeeded or failed notification is observed.
bool SignInWithUI(Browser* browser,
                  const std::string& email,
                  const std::string& password);

}  // namespace login_ui_test_utils

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_
