// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/data_type_manager_mock.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::SyncBackendHostMock;
using content::BrowserThread;
using sync_driver::DataTypeManager;
using sync_driver::DataTypeManagerMock;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::InvokeArgument;
using testing::Mock;
using testing::Return;
using testing::SaveArg;

namespace {

const char kGaiaId[] = "12345";
const char kEmail[] = "test_user@gmail.com";
const char kDummyPassword[] = "";

}  // namespace

ACTION_P(InvokeOnConfigureStart, pss) {
  ProfileSyncService* service =
      static_cast<ProfileSyncService*>(pss);
  service->OnConfigureStart();
}

ACTION_P3(InvokeOnConfigureDone, pss, error_callback, result) {
  ProfileSyncService* service =
      static_cast<ProfileSyncService*>(pss);
  DataTypeManager::ConfigureResult configure_result =
      static_cast<DataTypeManager::ConfigureResult>(result);
  if (result.status == sync_driver::DataTypeManager::ABORTED)
    error_callback.Run(&configure_result);
  service->OnConfigureDone(configure_result);
}

class TestProfileSyncServiceNoBackup : public ProfileSyncService {
 public:
  TestProfileSyncServiceNoBackup(
      scoped_ptr<ProfileSyncComponentsFactory> factory,
      Profile* profile,
      scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper,
      ProfileOAuth2TokenService* oauth2_token_service,
      browser_sync::ProfileSyncServiceStartBehavior start_behavior)
     : ProfileSyncService(factory.Pass(), profile, signin_wrapper.Pass(),
                          oauth2_token_service, start_behavior) {}

 protected:
  bool NeedBackup() const override { return false; }
};

class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_DB_THREAD |
                       content::TestBrowserThreadBundle::REAL_FILE_THREAD |
                       content::TestBrowserThreadBundle::REAL_IO_THREAD),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        sync_(NULL) {}

  ~ProfileSyncServiceStartupTest() override {}

  void SetUp() override {
    CHECK(profile_manager_.SetUp());

    TestingProfile::TestingFactories testing_facotries;
    testing_facotries.push_back(
        std::make_pair(SigninManagerFactory::GetInstance(),
                       FakeSigninManagerBase::Build));
    testing_facotries.push_back(
            std::make_pair(ProfileOAuth2TokenServiceFactory::GetInstance(),
                           BuildAutoIssuingFakeProfileOAuth2TokenService));
    testing_facotries.push_back(
            std::make_pair(ProfileSyncServiceFactory::GetInstance(),
                           BuildService));

    profile_ = profile_manager_.CreateTestingProfile(
        "sync-startup-test", scoped_ptr<PrefServiceSyncable>(),
        base::UTF8ToUTF16("sync-startup-test"), 0, std::string(),
        testing_facotries);
  }

  void TearDown() override { sync_->RemoveObserver(&observer_); }

  static KeyedService* BuildService(content::BrowserContext* browser_context) {
    Profile* profile = static_cast<Profile*>(browser_context);
    return new TestProfileSyncServiceNoBackup(
        scoped_ptr<ProfileSyncComponentsFactory>(
            new ProfileSyncComponentsFactoryMock()),
        profile,
        make_scoped_ptr(new SupervisedUserSigninManagerWrapper(
            profile, SigninManagerFactory::GetForProfile(profile))),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        browser_sync::MANUAL_START);
  }

  void CreateSyncService() {
    sync_ = static_cast<ProfileSyncService*>(
        ProfileSyncServiceFactory::GetForProfile(profile_));
    sync_->AddObserver(&observer_);
  }

  void IssueTestTokens(const std::string& account_id) {
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
        ->UpdateCredentials(account_id, "oauth2_login_token");
  }

  ProfileSyncComponentsFactoryMock* components_factory_mock() {
    return static_cast<ProfileSyncComponentsFactoryMock*>(sync_->factory());
  }

  FakeSigninManagerForTesting* fake_signin() {
    return static_cast<FakeSigninManagerForTesting*>(sync_->signin());
  }

  void SetError(DataTypeManager::ConfigureResult* result) {
    sync_driver::DataTypeStatusTable::TypeErrorMap errors;
    errors[syncer::BOOKMARKS] =
        syncer::SyncError(FROM_HERE,
                          syncer::SyncError::UNRECOVERABLE_ERROR,
                          "Error",
                          syncer::BOOKMARKS);
    result->data_type_status_table.UpdateFailedDataTypes(errors);
  }

 protected:
  static std::string SimulateTestUserSignin(
      Profile* profile,
      FakeSigninManagerForTesting* fake_signin,
      ProfileSyncService* sync) {
    std::string account_id =
        AccountTrackerServiceFactory::GetForProfile(profile)
            ->SeedAccountInfo(kGaiaId, kEmail);
    profile->GetPrefs()->SetString(prefs::kGoogleServicesAccountId,
                                   account_id);
#if !defined(OS_CHROMEOS)
    fake_signin->SignIn(kGaiaId, kEmail, kDummyPassword);
#else
    fake_signin->SetAuthenticatedAccountInfo(kGaiaId, kEmail);
    if (sync)
      sync->GoogleSigninSucceeded(account_id, kEmail, kDummyPassword);
#endif
    return account_id;
  }

  DataTypeManagerMock* SetUpDataTypeManager() {
    DataTypeManagerMock* data_type_manager = new DataTypeManagerMock();
    EXPECT_CALL(*components_factory_mock(),
                CreateDataTypeManager(_, _, _, _, _)).
        WillOnce(Return(data_type_manager));
    return data_type_manager;
  }

  browser_sync::SyncBackendHostMock* SetUpSyncBackendHost() {
    browser_sync::SyncBackendHostMock* sync_backend_host =
        new browser_sync::SyncBackendHostMock();
    EXPECT_CALL(*components_factory_mock(),
                CreateSyncBackendHost(_, _, _, _, _)).
        WillOnce(Return(sync_backend_host));
    return sync_backend_host;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  ProfileSyncService* sync_;
  SyncServiceObserverMock observer_;
  sync_driver::DataTypeStatusTable data_type_status_table_;
};

class ProfileSyncServiceStartupCrosTest : public ProfileSyncServiceStartupTest {
 public:
  void SetUp() override {
    ProfileSyncServiceStartupTest::SetUp();
    sync_ = static_cast<ProfileSyncService*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_, BuildCrosService));
    sync_->AddObserver(&observer_);
  }

  static KeyedService* BuildCrosService(content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    FakeSigninManagerForTesting* signin =
        static_cast<FakeSigninManagerForTesting*>(
        SigninManagerFactory::GetForProfile(profile));
    SimulateTestUserSignin(profile, signin, nullptr);
    ProfileOAuth2TokenService* oauth2_token_service =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
    EXPECT_TRUE(signin->IsAuthenticated());
    return new TestProfileSyncServiceNoBackup(
        scoped_ptr<ProfileSyncComponentsFactory>(
            new ProfileSyncComponentsFactoryMock()),
        profile,
        make_scoped_ptr(new SupervisedUserSigninManagerWrapper(profile,
                                                               signin)),
        oauth2_token_service,
        browser_sync::AUTO_START);
  }
};

TEST_F(ProfileSyncServiceStartupTest, StartFirstTime) {
  // We've never completed startup.
  profile_->GetPrefs()->ClearPref(sync_driver::prefs::kSyncHasSetupCompleted);
  CreateSyncService();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(
      0,
      profile_->GetPrefs()->GetInt64(sync_driver::prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncHasSetupCompleted));
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Then start things up.
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(1);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED)).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  sync_->SetSetupInProgress(true);

  // Simulate successful signin as test_user.
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);
  // Create some tokens in the token service.
  IssueTestTokens(account_id);

  // Simulate the UI telling sync it has finished setting up.
  sync_->SetSetupInProgress(false);
  EXPECT_TRUE(sync_->SyncActive());
}

// TODO(pavely): Reenable test once android is switched to oauth2.
TEST_F(ProfileSyncServiceStartupTest, DISABLED_StartNoCredentials) {
  // We've never completed startup.
  profile_->GetPrefs()->ClearPref(sync_driver::prefs::kSyncHasSetupCompleted);
  CreateSyncService();

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(*components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(
      0,
      profile_->GetPrefs()->GetInt64(sync_driver::prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncHasSetupCompleted));

  // Then start things up.
  sync_->SetSetupInProgress(true);

  // Simulate successful signin as test_user.
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);

  ProfileOAuth2TokenService* token_service =
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  token_service->LoadCredentials(account_id);

  sync_->SetSetupInProgress(false);
  // ProfileSyncService should try to start by requesting access token.
  // This request should fail as login token was not issued.
  EXPECT_FALSE(sync_->SyncActive());
  EXPECT_EQ(GoogleServiceAuthError::USER_NOT_SIGNED_UP,
      sync_->GetAuthError().state());
}

// TODO(pavely): Reenable test once android is switched to oauth2.
TEST_F(ProfileSyncServiceStartupTest, DISABLED_StartInvalidCredentials) {
  CreateSyncService();
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);
  SyncBackendHostMock* mock_sbh = SetUpSyncBackendHost();

  // Tell the backend to stall while downloading control types (simulating an
  // auth error).
  mock_sbh->set_fail_initial_download(true);

  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();
  EXPECT_FALSE(sync_->SyncActive());
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Update the credentials, unstalling the backend.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->SetSetupInProgress(true);

  // Simulate successful signin.
  SimulateTestUserSignin(profile_, fake_signin(), sync_);

  sync_->SetSetupInProgress(false);

  // Verify we successfully finish startup and configuration.
  EXPECT_TRUE(sync_->SyncActive());
}

#if defined(OS_WIN)
// http://crbug.com/396402
#define MAYBE_StartCrosNoCredentials DISABLED_StartCrosNoCredentials
#else
#define MAYBE_StartCrosNoCredentials StartCrosNoCredentials
#endif
TEST_F(ProfileSyncServiceStartupCrosTest, MAYBE_StartCrosNoCredentials) {
  EXPECT_CALL(*components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*components_factory_mock(),
              CreateSyncBackendHost(_, _, _, _, _)).Times(0);
  profile_->GetPrefs()->ClearPref(sync_driver::prefs::kSyncHasSetupCompleted);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  sync_->Initialize();
  // Sync should not start because there are no tokens yet.
  EXPECT_FALSE(sync_->SyncActive());
  sync_->SetSetupInProgress(false);

  // Sync should not start because there are still no tokens.
  EXPECT_FALSE(sync_->SyncActive());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartFirstTime) {
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  profile_->GetPrefs()->ClearPref(sync_driver::prefs::kSyncHasSetupCompleted);
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens(
      AccountTrackerServiceFactory::GetForProfile(profile_)
          ->PickAccountIdForAccount("12345", kEmail));
  sync_->Initialize();
  EXPECT_TRUE(sync_->SyncActive());
}

#if defined(OS_WIN)
// http://crbug.com/396402
#define MAYBE_StartNormal DISABLED_StartNormal
#else
#define MAYBE_StartNormal StartNormal
#endif
TEST_F(ProfileSyncServiceStartupTest, MAYBE_StartNormal) {
  // Pre load the tokens
  CreateSyncService();
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens(account_id);

  sync_->Initialize();
}

// Test that we can recover from a case where a bug in the code resulted in
// OnUserChoseDatatypes not being properly called and datatype preferences
// therefore being left unset.
TEST_F(ProfileSyncServiceStartupTest, StartRecoverDatatypePrefs) {
  // Clear the datatype preference fields (simulating bug 154940).
  profile_->GetPrefs()->ClearPref(
      sync_driver::prefs::kSyncKeepEverythingSynced);
  syncer::ModelTypeSet user_types = syncer::UserTypes();
  for (syncer::ModelTypeSet::Iterator iter = user_types.First();
       iter.Good(); iter.Inc()) {
    profile_->GetPrefs()->ClearPref(
        sync_driver::SyncPrefs::GetPrefNameForDataType(iter.Get()));
  }

  // Pre load the tokens
  CreateSyncService();
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  IssueTestTokens(account_id);
  sync_->Initialize();

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncKeepEverythingSynced));
}

// Verify that the recovery of datatype preferences doesn't overwrite a valid
// case where only bookmarks are enabled.
#if defined(OS_WIN)
// http://crbug.com/396402
#define MAYBE_StartDontRecoverDatatypePrefs DISABLED_StartDontRecoverDatatypePrefs
#else
#define MAYBE_StartDontRecoverDatatypePrefs StartDontRecoverDatatypePrefs
#endif
TEST_F(ProfileSyncServiceStartupTest, MAYBE_StartDontRecoverDatatypePrefs) {
  // Explicitly set Keep Everything Synced to false and have only bookmarks
  // enabled.
  profile_->GetPrefs()->SetBoolean(
      sync_driver::prefs::kSyncKeepEverythingSynced, false);

  // Pre load the tokens
  CreateSyncService();
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens(account_id);
  sync_->Initialize();

  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      sync_driver::prefs::kSyncKeepEverythingSynced));
}

#if defined(OS_WIN)
// http://crbug.com/396402
#define MAYBE_ManagedStartup DISABLED_ManagedStartup
#else
#define MAYBE_ManagedStartup ManagedStartup
#endif
TEST_F(ProfileSyncServiceStartupTest, MAYBE_ManagedStartup) {
  // Service should not be started by Initialize() since it's managed.
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesAccountId,
                                  kEmail);
  CreateSyncService();

  // Disable sync through policy.
  profile_->GetPrefs()->SetBoolean(sync_driver::prefs::kSyncManaged, true);
  EXPECT_CALL(*components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  sync_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SwitchManaged) {
  CreateSyncService();
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens(account_id);
  sync_->Initialize();

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_->GetPrefs()->SetBoolean(sync_driver::prefs::kSyncManaged, true);

  // When switching back to unmanaged, the state should change, but the service
  // should not start up automatically (kSyncSetupCompleted will be false).
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*components_factory_mock(),
              CreateDataTypeManager(_, _, _, _, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_->GetPrefs()->ClearPref(sync_driver::prefs::kSyncManaged);
}

TEST_F(ProfileSyncServiceStartupTest, StartFailure) {
  CreateSyncService();
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);
  sync_->SetSyncSetupCompleted();
  SetUpSyncBackendHost();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  DataTypeManager::ConfigureStatus status = DataTypeManager::ABORTED;
  DataTypeManager::ConfigureResult result(
      status,
      syncer::ModelTypeSet());
  EXPECT_CALL(*data_type_manager, Configure(_, _)).WillRepeatedly(
      DoAll(InvokeOnConfigureStart(sync_),
            InvokeOnConfigureDone(
                sync_,
                base::Bind(&ProfileSyncServiceStartupTest::SetError,
                           base::Unretained(this)),
                result)));
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::STOPPED));
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  IssueTestTokens(account_id);
  sync_->Initialize();
  EXPECT_TRUE(sync_->HasUnrecoverableError());
}

TEST_F(ProfileSyncServiceStartupTest, StartDownloadFailed) {
  // Pre load the tokens
  CreateSyncService();
  std::string account_id =
      SimulateTestUserSignin(profile_, fake_signin(), sync_);
  SyncBackendHostMock* mock_sbh = SetUpSyncBackendHost();
  mock_sbh->set_fail_initial_download(true);

  profile_->GetPrefs()->ClearPref(sync_driver::prefs::kSyncHasSetupCompleted);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  sync_->Initialize();

  sync_->SetSetupInProgress(true);
  IssueTestTokens(account_id);
  sync_->SetSetupInProgress(false);
  EXPECT_FALSE(sync_->SyncActive());
}
