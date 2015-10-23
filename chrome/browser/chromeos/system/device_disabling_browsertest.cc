// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "dbus/object_path.h"
#include "policy/proto/device_management_backend.pb.h"

namespace chromeos {
namespace system {

namespace {

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

}  // namespace

class DeviceDisablingTest
    : public OobeBaseTest,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  DeviceDisablingTest();

  // Sets up a device state blob that indicates the device is disabled, triggers
  // a policy plus device state fetch and waits for it to succeed.
  void MarkDisabledAndWaitForPolicyFetch();

  std::string GetCurrentScreenName(content::WebContents* web_contents);

 protected:
  base::RunLoop network_state_change_wait_run_loop_;

 private:
  // OobeBaseTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  // NetworkStateInformer::NetworkStateInformerObserver:
  void UpdateState(NetworkError::ErrorReason reason) override;

  FakeSessionManagerClient* fake_session_manager_client_;
  policy::DevicePolicyCrosTestHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingTest);
};


DeviceDisablingTest::DeviceDisablingTest()
    : fake_session_manager_client_(new FakeSessionManagerClient) {
}

void DeviceDisablingTest::MarkDisabledAndWaitForPolicyFetch() {
  base::RunLoop run_loop;
  // Set up an |observer| that will wait for the disabled setting to change.
  scoped_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(
           kDeviceDisabled,
           run_loop.QuitClosure());
  // Prepare a policy fetch response that indicates the device is disabled.
  test_helper_.device_policy()->policy_data().mutable_device_state()->
      set_device_mode(enterprise_management::DeviceState::DEVICE_MODE_DISABLED);
  test_helper_.device_policy()->Build();
  fake_session_manager_client_->set_device_policy(
      test_helper_.device_policy()->GetBlob());
  // Trigger a policy fetch.
  fake_session_manager_client_->OnPropertyChangeComplete(true);
  // Wait for the policy fetch to complete and the disabled setting to change.
  run_loop.Run();
}

std::string DeviceDisablingTest::GetCurrentScreenName(
    content::WebContents* web_contents ) {
  std::string screen_name;
  if (!content::ExecuteScriptAndExtractString(
          web_contents,
          "domAutomationController.send(Oobe.getInstance().currentScreen.id);",
          &screen_name)) {
    ADD_FAILURE();
  }
  return screen_name;
}

void DeviceDisablingTest::SetUpInProcessBrowserTestFixture() {
  OobeBaseTest::SetUpInProcessBrowserTestFixture();

  DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
      scoped_ptr<SessionManagerClient>(fake_session_manager_client_));

  test_helper_.InstallOwnerKey();
  test_helper_.MarkAsEnterpriseOwned();
}

void DeviceDisablingTest::SetUpOnMainThread() {
  OobeBaseTest::SetUpOnMainThread();

  // Set up fake networks.
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      SetupDefaultEnvironment();
}

void DeviceDisablingTest::UpdateState(NetworkError::ErrorReason reason) {
  network_state_change_wait_run_loop_.Quit();
}

IN_PROC_BROWSER_TEST_F(DeviceDisablingTest, DisableDuringNormalOperation) {
  // Mark the device as disabled and wait until cros settings update.
  MarkDisabledAndWaitForPolicyFetch();

  // Verify that the device disabled screen is being shown.
  WizardController* const wizard_controller =
      WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  EXPECT_EQ(wizard_controller->GetScreen(
                WizardController::kDeviceDisabledScreenName),
            wizard_controller->current_screen());
}

// Verifies that device disabling works when the ephemeral users policy is
// enabled. This case warrants its own test because the UI behaves somewhat
// differently when the policy is set: A background job runs on startup that
// causes the UI to try and show the login screen after some delay. It must
// be ensured that the login screen does not show and does not clobber the
// disabled screen.
IN_PROC_BROWSER_TEST_F(DeviceDisablingTest, DisableWithEphemeralUsers) {
  // Connect to the fake Ethernet network. This ensures that Chrome OS will not
  // try to show the offline error screen.
  base::RunLoop connect_run_loop;
  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath("/service/eth1"),
      connect_run_loop.QuitClosure(),
      base::Bind(&ErrorCallbackFunction));
  connect_run_loop.Run();

  // Skip to the login screen.
  WizardController* wizard_controller = WizardController::default_controller();
  ASSERT_TRUE(wizard_controller);
  wizard_controller->SkipToLoginForTesting(LoginScreenContext());
  OobeScreenWaiter(OobeDisplay::SCREEN_GAIA_SIGNIN).Wait();

  // Mark the device as disabled and wait until cros settings update.
  MarkDisabledAndWaitForPolicyFetch();

  // When the ephemeral users policy is enabled, Chrome OS removes any non-owner
  // cryptohomes on startup. At the end of that process, JavaScript attempts to
  // show the login screen. Simulate this.
  const LoginDisplayHostImpl* const host =
      static_cast<LoginDisplayHostImpl*>(LoginDisplayHostImpl::default_host());
  ASSERT_TRUE(host);
  WebUILoginView* const webui_login_view = host->GetWebUILoginView();
  ASSERT_TRUE(webui_login_view);
  content::WebContents* const web_contents = webui_login_view->GetWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::ExecuteScript(web_contents,
                                     "Oobe.showAddUserForTesting();"));

  // The login profile is scrubbed before attempting to show the login screen.
  // Wait for the scrubbing to finish.
  base::RunLoop run_loop;
  ProfileHelper::Get()->ClearSigninProfile(run_loop.QuitClosure());
  run_loop.Run();
  base::RunLoop().RunUntilIdle();

  // Verify that the login screen was not shown and the device disabled screen
  // is still being shown instead.
  EXPECT_EQ(OobeUI::kScreenDeviceDisabled, GetCurrentScreenName(web_contents));

  // Disconnect from the fake Ethernet network.
  OobeUI* const oobe_ui = host->GetOobeUI();
  ASSERT_TRUE(oobe_ui);
  const scoped_refptr<NetworkStateInformer> network_state_informer =
      oobe_ui->network_state_informer_for_test();
  ASSERT_TRUE(network_state_informer);
  network_state_informer->AddObserver(this);
  SigninScreenHandler* const signin_screen_handler =
      oobe_ui->signin_screen_handler_for_test();
  ASSERT_TRUE(signin_screen_handler);
  signin_screen_handler->ZeroOfflineTimeoutForTesting();
  SimulateNetworkOffline();
  network_state_change_wait_run_loop_.Run();
  network_state_informer->RemoveObserver(this);
  base::RunLoop().RunUntilIdle();

  // Verify that the offline error screen was not shown and the device disabled
  // screen is still being shown instead.
  EXPECT_EQ(OobeUI::kScreenDeviceDisabled, GetCurrentScreenName(web_contents));
}

}  // namespace system
}  // namespace chromeos
