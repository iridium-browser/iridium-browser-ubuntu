// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

class LoginScreenPolicyTest : public policy::DevicePolicyCrosBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kLoginManager);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InstallOwnerKey();
    MarkAsEnterpriseOwned();
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(LoginScreenPolicyTest, DisableSupervisedUsers) {
  EXPECT_FALSE(user_manager::UserManager::Get()->AreSupervisedUsersAllowed());

  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  std::unique_ptr<CrosSettings::ObserverSubscription> subscription(
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kAccountsPrefSupervisedUsersEnabled,
          runner->QuitClosure()));

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_supervised_users_settings()->set_supervised_users_enabled(true);
  RefreshDevicePolicy();

  runner->Run();

  EXPECT_TRUE(user_manager::UserManager::Get()->AreSupervisedUsersAllowed());
}

}  // namespace chromeos
