// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcSessionManagerTestBase : public testing::Test {
 public:
  ArcSessionManagerTestBase()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        user_manager_enabler_(new chromeos::FakeChromeUserManager()) {}
  ~ArcSessionManagerTestBase() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        base::MakeUnique<chromeos::FakeSessionManagerClient>());

    chromeos::DBusThreadManager::Initialize();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kEnableArc);
    ArcSessionManager::DisableUIForTesting();

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));

    profile_ = profile_builder.Build();
    StartPreferenceSyncing();

    arc_service_manager_ = base::MakeUnique<ArcServiceManager>(nullptr);
    arc_session_manager_ = base::MakeUnique<ArcSessionManager>(
        base::MakeUnique<ArcSessionRunner>(base::Bind(FakeArcSession::Create)));

    // Check initial conditions.
    EXPECT_TRUE(arc_session_manager_->IsSessionStopped());

    chromeos::WallpaperManager::Initialize();
  }

  void TearDown() override {
    chromeos::WallpaperManager::Shutdown();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 protected:
  Profile* profile() { return profile_.get(); }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  bool WaitForDataRemoved(ArcSessionManager::State expected_state) {
    if (arc_session_manager()->state() !=
        ArcSessionManager::State::REMOVING_DATA_DIR)
      return false;

    base::RunLoop().RunUntilIdle();
    if (arc_session_manager()->state() != expected_state)
      return false;

    return true;
  }

 private:
  void StartPreferenceSyncing() const {
    PrefServiceSyncableFromProfile(profile_.get())
        ->GetSyncableService(syncer::PREFERENCES)
        ->MergeDataAndStartSyncing(
            syncer::PREFERENCES, syncer::SyncDataList(),
            base::MakeUnique<syncer::FakeSyncChangeProcessor>(),
            base::MakeUnique<syncer::SyncErrorFactoryMock>());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerTestBase);
};

class ArcSessionManagerTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerTest() = default;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerTest);
};

TEST_F(ArcSessionManagerTest, PrefChangeTriggersService) {
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  PrefService* const pref = profile()->GetPrefs();
  ASSERT_FALSE(pref->GetBoolean(prefs::kArcEnabled));

  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());

  ASSERT_TRUE(WaitForDataRemoved(ArcSessionManager::State::STOPPED));

  pref->SetBoolean(prefs::kArcEnabled, true);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  pref->SetBoolean(prefs::kArcEnabled, false);

  ASSERT_TRUE(WaitForDataRemoved(ArcSessionManager::State::STOPPED));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, DisabledForEphemeralDataUsers) {
  PrefService* const prefs = profile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  prefs->SetBoolean(prefs::kArcEnabled, true);

  chromeos::FakeChromeUserManager* const fake_user_manager =
      GetFakeUserManager();

  fake_user_manager->AddUser(fake_user_manager->GetGuestAccountId());
  fake_user_manager->SwitchActiveUser(fake_user_manager->GetGuestAccountId());
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  fake_user_manager->AddUser(user_manager::DemoAccountId());
  fake_user_manager->SwitchActiveUser(user_manager::DemoAccountId());
  arc_session_manager()->Shutdown();
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  const AccountId public_account_id(
      AccountId::FromUserEmail("public_user@gmail.com"));
  fake_user_manager->AddPublicAccountUser(public_account_id);
  fake_user_manager->SwitchActiveUser(public_account_id);
  arc_session_manager()->Shutdown();
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  const AccountId not_in_list_account_id(
      AccountId::FromUserEmail("not_in_list_user@gmail.com"));
  fake_user_manager->set_ephemeral_users_enabled(true);
  fake_user_manager->AddUser(not_in_list_account_id);
  fake_user_manager->SwitchActiveUser(not_in_list_account_id);
  fake_user_manager->RemoveUserFromList(not_in_list_account_id);
  arc_session_manager()->Shutdown();
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, BaseWorkflow) {
  ASSERT_TRUE(arc_session_manager()->IsSessionStopped());
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());

  // By default ARC is not enabled.
  ASSERT_TRUE(WaitForDataRemoved(ArcSessionManager::State::STOPPED));

  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  base::RunLoop().RunUntilIdle();

  // Setting profile and pref initiates a code fetching process.
  ASSERT_EQ(ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // TODO(hidehiko): Verify state transition from SHOWING_TERMS_OF_SERVICE ->
  // CHECKING_ANDROID_MANAGEMENT, when we extract ArcSessionManager.
  arc_session_manager()->StartArc();

  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  ASSERT_TRUE(arc_session_manager()->IsSessionRunning());

  arc_session_manager()->Shutdown();
  ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());
  ASSERT_TRUE(arc_session_manager()->IsSessionStopped());

  // Send profile and don't provide a code.
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());

  // Setting profile initiates a code fetching process.
  ASSERT_EQ(ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // UI is disabled in unit tests and this code is unchanged.
  ASSERT_EQ(ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, CancelFetchingDisablesArc) {
  PrefService* const pref = profile()->GetPrefs();

  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  pref->SetBoolean(prefs::kArcEnabled, true);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  arc_session_manager()->CancelAuthCode();

  // Wait until data is removed.
  ASSERT_TRUE(WaitForDataRemoved(ArcSessionManager::State::STOPPED));

  ASSERT_FALSE(pref->GetBoolean(prefs::kArcEnabled));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, CloseUIKeepsArcEnabled) {
  PrefService* const pref = profile()->GetPrefs();

  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  pref->SetBoolean(prefs::kArcEnabled, true);
  base::RunLoop().RunUntilIdle();

  arc_session_manager()->StartArc();

  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->CancelAuthCode();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  ASSERT_TRUE(pref->GetBoolean(prefs::kArcEnabled));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, EnableDisablesArc) {
  const PrefService* pref = profile()->GetPrefs();
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());

  EXPECT_FALSE(pref->GetBoolean(prefs::kArcEnabled));
  arc_session_manager()->EnableArc();
  EXPECT_TRUE(pref->GetBoolean(prefs::kArcEnabled));
  arc_session_manager()->DisableArc();
  EXPECT_FALSE(pref->GetBoolean(prefs::kArcEnabled));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, SignInStatus) {
  PrefService* const prefs = profile()->GetPrefs();

  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  prefs->SetBoolean(prefs::kArcEnabled, true);

  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  EXPECT_EQ(ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  // Emulate to accept the terms of service.
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  arc_session_manager()->StartArc();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->IsSessionRunning());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->IsSessionRunning());

  // Second start, no fetching code is expected.
  arc_session_manager()->Shutdown();
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->IsSessionStopped());
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->IsSessionRunning());

  // Report failure.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::GMS_NETWORK_ERROR);
  // On error, UI to send feedback is showing. In that case,
  // the ARC is still necessary to run on background for gathering the logs.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->IsSessionRunning());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, DisabledForDeviceLocalAccount) {
  PrefService* const prefs = profile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  prefs->SetBoolean(prefs::kArcEnabled, true);
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  arc_session_manager()->StartArc();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Create device local account and set it as active.
  const std::string email = "device-local-account@fake-email.com";
  TestingProfile::Builder profile_builder;
  profile_builder.SetProfileName(email);
  std::unique_ptr<TestingProfile> device_local_profile(profile_builder.Build());
  const AccountId account_id(AccountId::FromUserEmail(email));
  GetFakeUserManager()->AddPublicAccountUser(account_id);

  // Remove |profile_| to set the device local account be the primary account.
  GetFakeUserManager()->RemoveUserFromList(
      multi_user_util::GetAccountIdFromProfile(profile()));
  GetFakeUserManager()->LoginUser(account_id);

  // Check that user without GAIA account can't use ARC.
  device_local_profile->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  arc_session_manager()->OnPrimaryUserProfilePrepared(
      device_local_profile.get());
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, DisabledForNonPrimaryProfile) {
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  arc_session_manager()->StartArc();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Create a second profile and set it as the active profile.
  const std::string email = "test@example.com";
  TestingProfile::Builder profile_builder;
  profile_builder.SetProfileName(email);
  std::unique_ptr<TestingProfile> second_profile(profile_builder.Build());
  const AccountId account_id(AccountId::FromUserEmail(email));
  GetFakeUserManager()->AddUser(account_id);
  GetFakeUserManager()->SwitchActiveUser(account_id);
  second_profile->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  // Check that non-primary user can't use Arc.
  EXPECT_FALSE(chromeos::ProfileHelper::IsPrimaryProfile(second_profile.get()));
  EXPECT_FALSE(ArcAppListPrefs::Get(second_profile.get()));

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RemoveDataFolder) {
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  // Starting session manager with prefs::kArcEnabled off automatically removes
  // Android's data folder.
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  // Enable ARC. Data is removed asyncronously. At this moment session manager
  // should be in REMOVING_DATA_DIR state.
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  // Wait until data is removed.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->StartArc();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Now request to remove data and stop session manager.
  arc_session_manager()->RemoveArcData();
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  arc_session_manager()->Shutdown();
  base::RunLoop().RunUntilIdle();
  // Request should persist.
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  // Emulate next sign-in. Data should be removed first and ARC started after.
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  ASSERT_TRUE(
      WaitForDataRemoved(ArcSessionManager::State::SHOWING_TERMS_OF_SERVICE));

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  arc_session_manager()->StartArc();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IgnoreSecondErrorReporting) {
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  arc_session_manager()->StartArc();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Report some failure that does not stop the bridge.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::GMS_SIGN_IN_FAILED);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Try to send another error that stops the bridge if sent first. It should
  // be ignored.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

class ArcSessionManagerKioskTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerKioskTest() = default;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    const AccountId account_id(
        AccountId::FromUserEmail(profile()->GetProfileUserName()));
    GetFakeUserManager()->AddArcKioskAppUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerKioskTest);
};

TEST_F(ArcSessionManagerKioskTest, AuthFailure) {
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  arc_session_manager()->OnPrimaryUserProfilePrepared(profile());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Replace chrome::AttemptUserExit() for testing.
  // At the end of test, leave the dangling pointer |terminated|,
  // assuming the callback is invoked exactly once in OnProvisioningFinished()
  // and not invoked then, including TearDown().
  bool terminated = false;
  arc_session_manager()->SetAttemptUserExitCallbackForTesting(
      base::Bind([](bool* terminated) { *terminated = true; }, &terminated));

  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  EXPECT_TRUE(terminated);
}

}  // namespace arc
