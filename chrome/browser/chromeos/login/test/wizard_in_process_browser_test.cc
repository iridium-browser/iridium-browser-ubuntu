// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

WizardInProcessBrowserTest::WizardInProcessBrowserTest(const char* screen_name)
    : screen_name_(screen_name),
      host_(NULL) {
}

void WizardInProcessBrowserTest::SetUp() {
  WizardController::SetZeroDelays();
  InProcessBrowserTest::SetUp();
}

void WizardInProcessBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(::switches::kNoStartupWindow);
  command_line->AppendSwitch(switches::kLoginManager);
}

void WizardInProcessBrowserTest::SetUpOnMainThread() {
  SetUpWizard();
  if (!screen_name_.empty()) {
    ShowLoginWizard(screen_name_);
    host_ = LoginDisplayHost::default_host();
  }
}

void WizardInProcessBrowserTest::TearDownOnMainThread() {
  // LoginDisplayHost owns controllers and all windows.
  base::MessageLoopForUI::current()->task_runner()->DeleteSoon(FROM_HERE,
                                                               host_);

  base::MessageLoopForUI::current()->RunUntilIdle();
}

}  // namespace chromeos
