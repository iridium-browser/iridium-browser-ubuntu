// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_

#include <memory>

#include "base/macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/test/histogram_tester.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;
class ManagePasswordsIconView;
class PasswordsClientUIDelegate;

// Test class for the various password management view bits and pieces. Provides
// some helper methods to poke at the bubble, icon, and controller's state.
class ManagePasswordsTest : public InProcessBrowserTest {
 public:
  ManagePasswordsTest();
  ~ManagePasswordsTest();

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

  // Get the icon view for the current WebContents.
  virtual ManagePasswordsIconView* view() = 0;

  // Execute the browser command to open the manage passwords bubble.
  void ExecuteManagePasswordsCommand();

  // Put the controller, icon, and bubble into a managing-password state.
  void SetupManagingPasswords();

  // Put the controller, icon, and bubble into the confirmation state.
  void SetupAutomaticPassword();

  // Put the controller, icon, and bubble into a pending-password state.
  void SetupPendingPassword();

  // Put the controller, icon, and bubble into an auto sign-in state.
  void SetupAutoSignin(
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials);

  // Get samples for |histogram|.
  std::unique_ptr<base::HistogramSamples> GetSamples(const char* histogram);

  autofill::PasswordForm* test_form() { return &test_form_; }

  // Get the UI controller for the current WebContents.
  PasswordsClientUIDelegate* GetController();

  MOCK_METHOD1(OnChooseCredential,
               void(const password_manager::CredentialInfo&));

 private:
  autofill::PasswordForm test_form_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsTest);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_TEST_H_
