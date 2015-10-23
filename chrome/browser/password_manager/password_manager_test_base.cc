// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_test_base.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/test_password_store_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

NavigationObserver::NavigationObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      quit_on_entry_committed_(false),
      message_loop_runner_(new content::MessageLoopRunner) {
}
NavigationObserver::~NavigationObserver() {
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

void NavigationObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (quit_on_entry_committed_)
    message_loop_runner_->Quit();
}

void NavigationObserver::Wait() {
  message_loop_runner_->Run();
}

PromptObserver::PromptObserver() {
}
PromptObserver::~PromptObserver() {
}

bool PromptObserver::IsShowingUpdatePrompt() const {
  // TODO(dvadym): Make this method pure virtual as soon as update UI is
  // implemented for infobar. http://crbug.com/359315
  return false;
}

void PromptObserver::Accept() const {
  EXPECT_TRUE(IsShowingPrompt());
  AcceptImpl();
}

void PromptObserver::AcceptUpdatePrompt(
    const autofill::PasswordForm& form) const {
  EXPECT_TRUE(IsShowingUpdatePrompt());
  AcceptUpdatePromptImpl(form);
}

class InfoBarObserver : public PromptObserver,
                        public infobars::InfoBarManager::Observer {
 public:
  explicit InfoBarObserver(content::WebContents* web_contents)
      : infobar_is_being_shown_(false),
        infobar_service_(InfoBarService::FromWebContents(web_contents)) {
    infobar_service_->AddObserver(this);
  }

  ~InfoBarObserver() override {
    if (infobar_service_)
      infobar_service_->RemoveObserver(this);
  }

 private:
  // PromptObserver:
  bool IsShowingPrompt() const override { return infobar_is_being_shown_; }

  void AcceptImpl() const override {
    EXPECT_EQ(1u, infobar_service_->infobar_count());
    if (!infobar_service_->infobar_count())
      return;  // Let the test finish to gather possibly more diagnostics.

    // ConfirmInfoBarDelegate::Accept returning true means the infobar is
    // immediately closed. Checking the return value is preferred to testing
    // IsShowingPrompt() here, for it avoids the delay until the closing
    // notification is received.
    EXPECT_TRUE(infobar_service_->infobar_at(0)
                    ->delegate()
                    ->AsConfirmInfoBarDelegate()
                    ->Accept());
  }

  // infobars::InfoBarManager::Observer:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override {
    infobar_is_being_shown_ = true;
  }

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    infobar_is_being_shown_ = false;
  }

  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override {
    ASSERT_EQ(infobar_service_, manager);
    infobar_service_->RemoveObserver(this);
    infobar_service_ = nullptr;
  }

  bool infobar_is_being_shown_;
  InfoBarService* infobar_service_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarObserver);
};

class BubbleObserver : public PromptObserver {
 public:
  explicit BubbleObserver(content::WebContents* web_contents)
      : ui_controller_(
            ManagePasswordsUIController::FromWebContents(web_contents)) {}

  ~BubbleObserver() override {}

 private:
  // PromptObserver:
  bool IsShowingPrompt() const override {
    return ui_controller_->PasswordPendingUserDecision();
  }

  bool IsShowingUpdatePrompt() const override {
    return ui_controller_->state() ==
           password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
  }

  void AcceptImpl() const override {
    ui_controller_->SavePassword();
    EXPECT_FALSE(IsShowingPrompt());
  }

  void AcceptUpdatePromptImpl(
      const autofill::PasswordForm& form) const override {
    ui_controller_->UpdatePassword(form);
    EXPECT_FALSE(IsShowingUpdatePrompt());
  }
  ManagePasswordsUIController* const ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(BubbleObserver);
};

scoped_ptr<PromptObserver> PromptObserver::Create(
    content::WebContents* web_contents) {
  if (ChromePasswordManagerClient::IsTheHotNewBubbleUIEnabled()) {
    return scoped_ptr<PromptObserver>(new BubbleObserver(web_contents));
  } else {
    return scoped_ptr<PromptObserver>(new InfoBarObserver(web_contents));
  }
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
      browser()->profile(), TestPasswordStoreService::Build);
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      password_manager::switches::kEnableAutomaticPasswordSaving));
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
