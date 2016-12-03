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
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
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
#include "chromeos/login/user_names.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/sync/api/fake_sync_change_processor.h"
#include "components/sync/api/sync_error_factory_mock.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

const char kTestAuthCode[] = "4/Qa3CPIhh-WcMfWSf9HZaYcGUhEeax-F9sQK9CNRhZWs";

}  // namespace

class ArcAuthServiceTest : public testing::Test {
 public:
  ArcAuthServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        user_manager_enabler_(new chromeos::FakeChromeUserManager) {}
  ~ArcAuthServiceTest() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kEnableArc);
    ArcAuthService::DisableUIForTesting();

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.path().AppendASCII("TestArcProfile"));

    profile_ = profile_builder.Build();
    StartPreferenceSyncing();

    bridge_service_.reset(new FakeArcBridgeService());
    auth_service_.reset(new ArcAuthService(bridge_service_.get()));

    // Check initial conditions.
    EXPECT_EQ(bridge_service_.get(), ArcBridgeService::Get());
    EXPECT_TRUE(ArcBridgeService::Get()->stopped());

    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("user@gmail.com", "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    chromeos::WallpaperManager::Initialize();
  }

  void TearDown() override {
    chromeos::WallpaperManager::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 protected:
  Profile* profile() { return profile_.get(); }
  FakeArcBridgeService* bridge_service() { return bridge_service_.get(); }
  ArcAuthService* auth_service() { return auth_service_.get(); }

 private:
  void StartPreferenceSyncing() const {
    PrefServiceSyncableFromProfile(profile_.get())
        ->GetSyncableService(syncer::PREFERENCES)
        ->MergeDataAndStartSyncing(syncer::PREFERENCES, syncer::SyncDataList(),
                                   std::unique_ptr<syncer::SyncChangeProcessor>(
                                       new syncer::FakeSyncChangeProcessor),
                                   std::unique_ptr<syncer::SyncErrorFactory>(
                                       new syncer::SyncErrorFactoryMock()));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<FakeArcBridgeService> bridge_service_;
  std::unique_ptr<ArcAuthService> auth_service_;
  std::unique_ptr<TestingProfile> profile_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceTest);
};

TEST_F(ArcAuthServiceTest, PrefChangeTriggersService) {
  ASSERT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());

  PrefService* const pref = profile()->GetPrefs();
  ASSERT_FALSE(pref->GetBoolean(prefs::kArcEnabled));

  auth_service()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcAuthService::State::STOPPED, auth_service()->state());

  pref->SetBoolean(prefs::kArcEnabled, true);
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  pref->SetBoolean(prefs::kArcEnabled, false);
  ASSERT_EQ(ArcAuthService::State::STOPPED, auth_service()->state());

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, DisabledForEphemeralDataUsers) {
  PrefService* const prefs = profile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  prefs->SetBoolean(prefs::kArcEnabled, true);

  chromeos::FakeChromeUserManager* const fake_user_manager =
      GetFakeUserManager();

  fake_user_manager->AddUser(fake_user_manager->GetGuestAccountId());
  fake_user_manager->SwitchActiveUser(fake_user_manager->GetGuestAccountId());
  auth_service()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());

  fake_user_manager->AddUser(chromeos::login::DemoAccountId());
  fake_user_manager->SwitchActiveUser(chromeos::login::DemoAccountId());
  auth_service()->Shutdown();
  auth_service()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());

  const AccountId public_account_id(
      AccountId::FromUserEmail("public_user@gmail.com"));
  fake_user_manager->AddPublicAccountUser(public_account_id);
  fake_user_manager->SwitchActiveUser(public_account_id);
  auth_service()->Shutdown();
  auth_service()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());

  const AccountId not_in_list_account_id(
      AccountId::FromUserEmail("not_in_list_user@gmail.com"));
  fake_user_manager->set_ephemeral_users_enabled(true);
  fake_user_manager->AddUser(not_in_list_account_id);
  fake_user_manager->SwitchActiveUser(not_in_list_account_id);
  fake_user_manager->RemoveUserFromList(not_in_list_account_id);
  auth_service()->Shutdown();
  auth_service()->OnPrimaryUserProfilePrepared(profile());
  ASSERT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, BaseWorkflow) {
  ASSERT_FALSE(bridge_service()->ready());
  ASSERT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());
  ASSERT_EQ(std::string(), auth_service()->GetAndResetAuthCode());

  auth_service()->OnPrimaryUserProfilePrepared(profile());

  // By default ARC is not enabled.
  ASSERT_EQ(ArcAuthService::State::STOPPED, auth_service()->state());

  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  // Setting profile and pref initiates a code fetching process.
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  auth_service()->SetAuthCodeAndStartArc(kTestAuthCode);

  ASSERT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());
  ASSERT_TRUE(bridge_service()->ready());
  // Auth code valid only for one call.
  ASSERT_EQ(kTestAuthCode, auth_service()->GetAndResetAuthCode());
  ASSERT_EQ(std::string(), auth_service()->GetAndResetAuthCode());

  auth_service()->Shutdown();
  ASSERT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());
  ASSERT_FALSE(bridge_service()->ready());
  ASSERT_EQ(std::string(), auth_service()->GetAndResetAuthCode());

  // Send profile and don't provide a code.
  auth_service()->OnPrimaryUserProfilePrepared(profile());

  // Setting profile initiates a code fetching process.
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // UI is disabled in unit tests and this code is unchanged.
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, CancelFetchingDisablesArc) {
  PrefService* const pref = profile()->GetPrefs();

  auth_service()->OnPrimaryUserProfilePrepared(profile());
  pref->SetBoolean(prefs::kArcEnabled, true);
  ASSERT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());

  auth_service()->CancelAuthCode();
  ASSERT_EQ(ArcAuthService::State::STOPPED, auth_service()->state());
  ASSERT_FALSE(pref->GetBoolean(prefs::kArcEnabled));

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, CloseUIKeepsArcEnabled) {
  PrefService* const pref = profile()->GetPrefs();

  auth_service()->OnPrimaryUserProfilePrepared(profile());
  pref->SetBoolean(prefs::kArcEnabled, true);

  auth_service()->SetAuthCodeAndStartArc(kTestAuthCode);

  ASSERT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());

  auth_service()->CancelAuthCode();
  ASSERT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());
  ASSERT_TRUE(pref->GetBoolean(prefs::kArcEnabled));

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, EnableDisablesArc) {
  const PrefService* pref = profile()->GetPrefs();
  auth_service()->OnPrimaryUserProfilePrepared(profile());

  EXPECT_FALSE(pref->GetBoolean(prefs::kArcEnabled));
  auth_service()->EnableArc();
  EXPECT_TRUE(pref->GetBoolean(prefs::kArcEnabled));
  auth_service()->DisableArc();
  EXPECT_FALSE(pref->GetBoolean(prefs::kArcEnabled));

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, SignInStatus) {
  PrefService* const prefs = profile()->GetPrefs();

  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  prefs->SetBoolean(prefs::kArcEnabled, true);

  auth_service()->OnPrimaryUserProfilePrepared(profile());
  EXPECT_EQ(ArcAuthService::State::FETCHING_CODE, auth_service()->state());
  auth_service()->SetAuthCodeAndStartArc(kTestAuthCode);
  EXPECT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());
  EXPECT_TRUE(bridge_service()->ready());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  auth_service()->OnSignInComplete();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());
  EXPECT_TRUE(bridge_service()->ready());

  // Second start, no fetching code is expected.
  auth_service()->Shutdown();
  EXPECT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());
  EXPECT_FALSE(bridge_service()->ready());
  auth_service()->OnPrimaryUserProfilePrepared(profile());
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());
  EXPECT_TRUE(bridge_service()->ready());

  // Report failure.
  auth_service()->OnSignInFailed(
      mojom::ArcSignInFailureReason::GMS_NETWORK_ERROR);
  // On error, UI to send feedback is showing. In that case,
  // the ARC is still necessary to run on background for gathering the logs.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());
  EXPECT_TRUE(bridge_service()->ready());

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, DisabledForDeviceLocalAccount) {
  PrefService* const prefs = profile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  prefs->SetBoolean(prefs::kArcEnabled, true);
  auth_service()->OnPrimaryUserProfilePrepared(profile());
  auth_service()->SetAuthCodeAndStartArc(kTestAuthCode);
  EXPECT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());

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
  auth_service()->OnPrimaryUserProfilePrepared(device_local_profile.get());
  EXPECT_EQ(ArcAuthService::State::NOT_INITIALIZED, auth_service()->state());

  // Correctly stop service.
  auth_service()->Shutdown();
}

TEST_F(ArcAuthServiceTest, DisabledForNonPrimaryProfile) {
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  auth_service()->OnPrimaryUserProfilePrepared(profile());
  auth_service()->SetAuthCodeAndStartArc(kTestAuthCode);
  EXPECT_EQ(ArcAuthService::State::ACTIVE, auth_service()->state());

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

  auth_service()->Shutdown();
}

}  // namespace arc
