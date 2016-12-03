// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_web_dialog.h"

#include "ash/shell.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/wm/public/activation_client.h"

namespace chromeos {

typedef InProcessBrowserTest LoginWebDialogTest;

// Test that LoginWebDialog is not minimizable.
IN_PROC_BROWSER_TEST_F(LoginWebDialogTest, CannotMinimize) {
  LoginWebDialog* dialog = new LoginWebDialog(
      browser()->profile(), NULL, NULL, base::string16(), GURL());
  dialog->Show();

  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(
          ash::Shell::GetInstance()->GetPrimaryRootWindow());
  aura::Window* active_window = activation_client->GetActiveWindow();
  ASSERT_TRUE(active_window != NULL);
  EXPECT_FALSE(active_window->GetProperty(aura::client::kCanMinimizeKey));
}

}  // namespace chromeos
