// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

// A helper class that synchronously waits until the password store handles a
// GetLogins() request.
class PasswordStoreResultsObserver
    : public password_manager::PasswordStoreConsumer {
 public:
  PasswordStoreResultsObserver() = default;

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override {
    run_loop_.Quit();
  }

  void Wait() {
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreResultsObserver);
};

class CredentialManagerBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  CredentialManagerBrowserTest() = default;

  bool IsShowingAccountChooser() {
    return PasswordsModelDelegateFromWebContents(WebContents())->
        GetState() == password_manager::ui::CREDENTIAL_REQUEST_STATE;
  }

  // Make sure that the password store processed all the previous calls which
  // are executed on another thread.
  void WaitForPasswordStore() {
    scoped_refptr<password_manager::PasswordStore> password_store =
        PasswordStoreFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    PasswordStoreResultsObserver syncer;
    password_store->GetAutofillableLoginsWithAffiliatedRealms(&syncer);
    syncer.Wait();
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerBrowserTest);
};

// Tests.

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       AccountChooserWithOldCredentialAndNavigation) {
  // Save credentials with 'skip_zero_click'.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
  "document.getElementById('username_field').value = 'user';"
  "document.getElementById('password_field').value = 'password';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_password));

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "navigator.credentials.get({password: true})"
      ".then(cred => window.location = '/password/done.html')"));
  WaitForPasswordStore();
  ASSERT_EQ(
      password_manager::ui::CREDENTIAL_REQUEST_STATE,
      PasswordsModelDelegateFromWebContents(WebContents())->GetState());
  PasswordsModelDelegateFromWebContents(WebContents())->ChooseCredential(
      signin_form,
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  // Verify that the form's 'skip_zero_click' is updated and not overwritten
  // by the autofill password manager on successful login.
  WaitForPasswordStore();
  password_manager::TestPasswordStore::PasswordMap passwords_map =
      password_store->stored_passwords();
  ASSERT_EQ(1u, passwords_map.size());
  const std::vector<autofill::PasswordForm>& passwords_vector =
      passwords_map.begin()->second;
  ASSERT_EQ(1u, passwords_vector.size());
  const autofill::PasswordForm& form = passwords_vector[0];
  EXPECT_EQ(base::ASCIIToUTF16("user"), form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("password"), form.password_value);
  EXPECT_FALSE(form.skip_zero_click);
}


IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       AutoSigninOldCredentialAndNavigation) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = false;
  password_store->AddLogin(signin_form);

  // Enable 'auto signin' for the profile.
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      browser()->profile()->GetPrefs());

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
  "document.getElementById('username_field').value = 'trash';"
  "document.getElementById('password_field').value = 'trash';";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_password));

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "navigator.credentials.get({password: true})"
      ".then(cred => window.location = '/password/done.html');"));

  NavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  observer.Wait();

  std::unique_ptr<BubbleObserver> prompt_observer(
      new BubbleObserver(WebContents()));
  // The autofill password manager shouldn't react to the successful login
  // because it was suppressed when the site got the credential back.
  EXPECT_FALSE(prompt_observer->IsShowingSavePrompt());
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, SaveViaAPIAndAutofill) {
  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('input_submit_button').addEventListener('click',"
      "function(event) {"
        "var c = new PasswordCredential({ id: 'user', password: 'API' });"
        "navigator.credentials.store(c);"
      "});"));
  // Fill the password and click the button to submit the page. The API should
  // suppress the autofill password manager.
  NavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'autofill';"
      "document.getElementById('input_submit_button').click();"));
  form_submit_observer.Wait();

  WaitForPasswordStore();
  std::unique_ptr<BubbleObserver> prompt_observer(
      new BubbleObserver(WebContents()));
  ASSERT_TRUE(prompt_observer->IsShowingSavePrompt());
  prompt_observer->AcceptSavePrompt();

  WaitForPasswordStore();
  password_manager::TestPasswordStore::PasswordMap stored =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get())->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  autofill::PasswordForm signin_form = stored.begin()->second[0];
  EXPECT_EQ(base::ASCIIToUTF16("user"), signin_form.username_value);
  EXPECT_EQ(base::ASCIIToUTF16("API"), signin_form.password_value);
  EXPECT_EQ(embedded_test_server()->base_url().spec(),
            signin_form.signon_realm);
  EXPECT_EQ(embedded_test_server()->base_url(), signin_form.origin);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, UpdateViaAPIAndAutofill) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("old_pass");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  signin_form.preferred = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('input_submit_button').addEventListener('click',"
      "function(event) {"
        "var c = new PasswordCredential({ id: 'user', password: 'API' });"
        "navigator.credentials.store(c);"
      "});"));
  // Fill the new password and click the button to submit the page later. The
  // API should suppress the autofill password manager and overwrite the
  // password.
  NavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecuteScript(
      RenderViewHost(),
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'autofill';"
      "document.getElementById('input_submit_button').click();"));
  form_submit_observer.Wait();

  // Wait for the password store before checking the prompt because it pops up
  // after the store replies.
  WaitForPasswordStore();
  std::unique_ptr<BubbleObserver> prompt_observer(
      new BubbleObserver(WebContents()));
  EXPECT_FALSE(prompt_observer->IsShowingSavePrompt());
  EXPECT_FALSE(prompt_observer->IsShowingUpdatePrompt());
  signin_form.skip_zero_click = false;
  signin_form.times_used = 1;
  signin_form.password_value = base::ASCIIToUTF16("API");
  password_manager::TestPasswordStore::PasswordMap stored =
      password_store->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  EXPECT_EQ(signin_form, stored[signin_form.signon_realm][0]);
}

}  // namespace
