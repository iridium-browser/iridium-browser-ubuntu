// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_test_base.h"

#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

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

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreResultsObserver);
};

}  // namespace

NavigationObserver::NavigationObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      quit_on_entry_committed_(false),
      message_loop_runner_(new content::MessageLoopRunner) {
}
NavigationObserver::~NavigationObserver() {
}

void NavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (quit_on_entry_committed_)
    message_loop_runner_->Quit();
}

void NavigationObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  render_frame_host_ = render_frame_host;
  if (!wait_for_path_.empty()) {
    if (validated_url.path() == wait_for_path_)
      message_loop_runner_->Quit();
  } else if (!render_frame_host->GetParent()) {
    message_loop_runner_->Quit();
  }
}

void NavigationObserver::Wait() {
  message_loop_runner_->Run();
}

BubbleObserver::BubbleObserver(content::WebContents* web_contents)
    : passwords_ui_controller_(
          ManagePasswordsUIController::FromWebContents(web_contents)) {}

bool BubbleObserver::IsShowingSavePrompt() const {
  return passwords_ui_controller_->GetState() ==
      password_manager::ui::PENDING_PASSWORD_STATE;
}

bool BubbleObserver::IsShowingUpdatePrompt() const {
  return passwords_ui_controller_->GetState() ==
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
}

void BubbleObserver::Dismiss() const  {
  passwords_ui_controller_->OnBubbleHidden();
  // Navigate away to reset the state to inactive.
  static_cast<content::WebContentsObserver*>(passwords_ui_controller_)
      ->DidNavigateMainFrame(content::LoadCommittedDetails(),
                             content::FrameNavigateParams());
  ASSERT_EQ(password_manager::ui::INACTIVE_STATE,
            passwords_ui_controller_->GetState());
}

void BubbleObserver::AcceptSavePrompt() const {
  ASSERT_TRUE(IsShowingSavePrompt());
  passwords_ui_controller_->SavePassword();
  EXPECT_FALSE(IsShowingSavePrompt());
}

void BubbleObserver::AcceptUpdatePrompt(
    const autofill::PasswordForm& form) const {
  ASSERT_TRUE(IsShowingUpdatePrompt());
  passwords_ui_controller_->UpdatePassword(form);
  EXPECT_FALSE(IsShowingUpdatePrompt());
}

PasswordManagerBrowserTestBase::PasswordManagerBrowserTestBase() {
}
PasswordManagerBrowserTestBase::~PasswordManagerBrowserTestBase() {
}

void PasswordManagerBrowserTestBase::SetUpOnMainThread() {
  // Use TestPasswordStore to remove a possible race. Normally the
  // PasswordStore does its database manipulation on the DB thread, which
  // creates a possible race during navigation. Specifically the
  // PasswordManager will ignore any forms in a page if the load from the
  // PasswordStore has not completed.
  PasswordStoreFactory::GetInstance()->SetTestingFactory(
      browser()->profile(),
      password_manager::BuildPasswordStore<
          content::BrowserContext, password_manager::TestPasswordStore>);
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      password_manager::features::kEnableAutomaticPasswordSaving));
}

void PasswordManagerBrowserTestBase::TearDownOnMainThread() {
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

content::WebContents* PasswordManagerBrowserTestBase::WebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

content::RenderViewHost* PasswordManagerBrowserTestBase::RenderViewHost() {
  return WebContents()->GetRenderViewHost();
}

void PasswordManagerBrowserTestBase::NavigateToFile(const std::string& path) {
  NavigationObserver observer(WebContents());
  GURL url = embedded_test_server()->GetURL(path);
  ui_test_utils::NavigateToURL(browser(), url);
  observer.Wait();
}

void PasswordManagerBrowserTestBase::VerifyPasswordIsSavedAndFilled(
    const std::string& filename,
    const std::string& submission_script,
    const std::string& expected_element,
    const std::string& expected_value) {
  password_manager::TestPasswordStore* password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  EXPECT_TRUE(password_store->IsEmpty());

  NavigateToFile(filename);

  NavigationObserver observer(WebContents());
  std::unique_ptr<BubbleObserver> prompt_observer(
      new BubbleObserver(WebContents()));
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submission_script));
  observer.Wait();

  prompt_observer->AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  ASSERT_FALSE(password_store->IsEmpty());

  NavigateToFile(filename);

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::Left, gfx::Point(1, 1));

  // Wait until that interaction causes the password value to be revealed.
  WaitForElementValue(expected_element, expected_value);
}

void PasswordManagerBrowserTestBase::WaitForElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  WaitForElementValue("null", element_id, expected_value);
}

void PasswordManagerBrowserTestBase::WaitForElementValue(
    const std::string& iframe_id,
    const std::string& element_id,
    const std::string& expected_value) {
  enum ReturnCodes {  // Possible results of the JavaScript code.
    RETURN_CODE_OK,
    RETURN_CODE_NO_ELEMENT,
    RETURN_CODE_WRONG_VALUE,
    RETURN_CODE_INVALID,
  };
  const std::string value_check_function = base::StringPrintf(
      "function valueCheck() {"
      "  if (%s)"
      "    var element = document.getElementById("
      "        '%s').contentDocument.getElementById('%s');"
      "  else "
      "    var element = document.getElementById('%s');"
      "  return element && element.value == '%s';"
      "}",
      iframe_id.c_str(), iframe_id.c_str(), element_id.c_str(),
      element_id.c_str(), expected_value.c_str());
  const std::string script =
      value_check_function +
      base::StringPrintf(
          "if (valueCheck()) {"
          "  /* Spin the event loop with setTimeout. */"
          "  setTimeout(window.domAutomationController.send(%d), 0);"
          "} else {"
          "  if (%s)"
          "    var element = document.getElementById("
          "        '%s').contentDocument.getElementById('%s');"
          "  else "
          "    var element = document.getElementById('%s');"
          "  if (!element)"
          "    window.domAutomationController.send(%d);"
          "  element.onchange = function() {"
          "    if (valueCheck()) {"
          "      /* Spin the event loop with setTimeout. */"
          "      setTimeout(window.domAutomationController.send(%d), 0);"
          "    } else {"
          "      window.domAutomationController.send(%d);"
          "    }"
          "  };"
          "}",
          RETURN_CODE_OK, iframe_id.c_str(), iframe_id.c_str(),
          element_id.c_str(), element_id.c_str(), RETURN_CODE_NO_ELEMENT,
          RETURN_CODE_OK, RETURN_CODE_WRONG_VALUE);
  int return_value = RETURN_CODE_INVALID;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(RenderViewHost(), script,
                                                  &return_value));
  EXPECT_EQ(RETURN_CODE_OK, return_value)
      << "element_id = " << element_id
      << ", expected_value = " << expected_value;
}

void PasswordManagerBrowserTestBase::WaitForPasswordStore() {
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(browser()->profile(),
                                          ServiceAccessType::IMPLICIT_ACCESS);
  PasswordStoreResultsObserver syncer;
  password_store->GetAutofillableLoginsWithAffiliatedRealms(&syncer);
  syncer.Wait();
}

void PasswordManagerBrowserTestBase::CheckElementValue(
    const std::string& element_id,
    const std::string& expected_value) {
  CheckElementValue("null", element_id, expected_value);
}

void PasswordManagerBrowserTestBase::CheckElementValue(
    const std::string& iframe_id,
    const std::string& element_id,
    const std::string& expected_value) {
  const std::string value_check_script = base::StringPrintf(
      "if (%s)"
      "  var element = document.getElementById("
      "      '%s').contentDocument.getElementById('%s');"
      "else "
      "  var element = document.getElementById('%s');"
      "window.domAutomationController.send(element && element.value == '%s');",
      iframe_id.c_str(), iframe_id.c_str(), element_id.c_str(),
      element_id.c_str(), expected_value.c_str());
  bool return_value = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      RenderViewHost(), value_check_script, &return_value));
  EXPECT_TRUE(return_value) << "element_id = " << element_id
                            << ", expected_value = " << expected_value;
}
