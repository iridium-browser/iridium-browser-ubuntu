// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_

#include <string>

#include "chrome/browser/chromeos/login/mixin_based_browser_test.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromeos {

class UserContext;

// Base class for Chrome OS out-of-box/login WebUI tests.
// If no special configuration is done launches out-of-box WebUI.
// To launch login UI use PRE_* test that will register user(s) and mark
// out-of-box as completed.
// Guarantees that WebUI has been initialized by waiting for
// NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE notification.
class LoginManagerTest : public MixinBasedBrowserTest {
 public:
  explicit LoginManagerTest(bool should_launch_browser);

  // Overridden from InProcessBrowserTest.
  void TearDownOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  bool SetUpUserDataDirectory() override;

  // Registers the user with the given |user_id| on the device.
  // This method should be called in PRE_* test.
  // TODO(dzhioev): Add the ability to register users without a PRE_* test.
  void RegisterUser(const std::string& user_id);

  // Set expected credentials for next login attempt.
  void SetExpectedCredentials(const UserContext& user_context);

  // Tries to login with the credentials in |user_context|. The return value
  // indicates whether the login attempt succeeded.
  bool TryToLogin(const UserContext& user_context);

  // Tries to add the user identified and authenticated by |user_context| to the
  // session. The return value indicates whether the attempt succeeded. This
  // method does the same as TryToLogin() but doesn't verify that the new user
  // has become the active user.
  bool AddUserToSession(const UserContext& user_context);

  // Log in user with |user_id|. User should be registered using RegisterUser().
  void LoginUser(const std::string& user_id);

  // Add user with |user_id| to session.
  void AddUser(const std::string& user_id);

  // Executes given JS |expression| in |web_contents_| and checks
  // that it is true.
  void JSExpect(const std::string& expression);

  content::WebContents* web_contents() { return web_contents_; }

  test::JSChecker& js_checker() { return js_checker_; }

  static std::string GetGaiaIDForUserID(const std::string& user_id);

 protected:
  bool use_webview() { return use_webview_; }
  void set_use_webview(bool use_webview) { use_webview_ = use_webview; }

  bool use_webview_;

 private:
  void InitializeWebContents();

  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

  bool should_launch_browser_;
  content::WebContents* web_contents_;
  test::JSChecker js_checker_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_
