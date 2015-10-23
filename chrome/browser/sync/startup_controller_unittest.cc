// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/startup_controller.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/supervised_user_signin_manager_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

static const char kTestUser[] = "test@gmail.com";
static const char kTestToken[] = "testToken";

// These are coupled to the implementation of StartupController's
// GetBackendInitializationStateString which is used by about:sync. We use it
// as a convenient way to verify internal state and that the class is
// outputting the correct values for the debug string.
static const char kStateStringStarted[] = "Started";
static const char kStateStringDeferred[] = "Deferred";
static const char kStateStringNotStarted[] = "Not started";

class FakeSupervisedUserSigninManagerWrapper
    : public SupervisedUserSigninManagerWrapper {
 public:
  FakeSupervisedUserSigninManagerWrapper()
      : SupervisedUserSigninManagerWrapper(NULL, NULL) {}
  std::string GetEffectiveUsername() const override { return account_; }

  std::string GetAccountIdToUse() const override { return account_; }

  void set_account(const std::string& account) { account_ = account; }

 private:
  std::string account_;
};

class StartupControllerTest : public testing::Test {
 public:
  StartupControllerTest() : started_(false) {}

  void SetUp() override {
    profile_.reset(new TestingProfile());
    sync_prefs_.reset(new sync_driver::SyncPrefs(profile_->GetPrefs()));
    token_service_.reset(static_cast<FakeProfileOAuth2TokenService*>(
        BuildFakeProfileOAuth2TokenService(profile_.get()).release()));
    signin_.reset(new FakeSupervisedUserSigninManagerWrapper());

    ProfileSyncServiceStartBehavior behavior =
        browser_defaults::kSyncAutoStarts ? AUTO_START : MANUAL_START;
    base::Closure fake_start_backend = base::Bind(
        &StartupControllerTest::FakeStartBackend, base::Unretained(this));
    controller_.reset(new StartupController(behavior, token_service(),
                                            sync_prefs_.get(), signin_.get(),
                                            fake_start_backend));
    controller_->Reset(syncer::UserTypes());
    controller_->OverrideFallbackTimeoutForTest(
        base::TimeDelta::FromSeconds(0));
  }

  void TearDown() override {
    controller_.reset();
    signin_.reset();
    token_service_->Shutdown();
    token_service_.reset();
    sync_prefs_.reset();
    started_ = false;
  }

  void FakeStartBackend() {
    started_ = true;
  }

  bool started() const { return started_; }
  void clear_started() { started_ = false; }
  StartupController* controller() { return controller_.get(); }
  FakeSupervisedUserSigninManagerWrapper* signin() { return signin_.get(); }
  FakeProfileOAuth2TokenService* token_service() {
    return token_service_.get();
  }
  sync_driver::SyncPrefs* sync_prefs() { return sync_prefs_.get(); }
  Profile* profile() { return profile_.get(); }

 private:
  bool started_;
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<StartupController> controller_;
  scoped_ptr<FakeSupervisedUserSigninManagerWrapper> signin_;
  scoped_ptr<FakeProfileOAuth2TokenService> token_service_;
  scoped_ptr<sync_driver::SyncPrefs> sync_prefs_;
  scoped_ptr<TestingProfile> profile_;
};

// Test that sync doesn't start until all conditions are met.
TEST_F(StartupControllerTest, Basic) {
  controller()->TryStart();
  EXPECT_FALSE(started());
  sync_prefs()->SetSyncSetupCompleted();
  controller()->TryStart();
  EXPECT_FALSE(started());
  signin()->set_account(kTestUser);
  controller()->TryStart();
  EXPECT_FALSE(started());
  token_service()->UpdateCredentials(kTestUser, kTestToken);
  const bool deferred_start =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSyncDisableDeferredStartup);
  controller()->TryStart();
  EXPECT_EQ(!deferred_start, started());
  std::string state(controller()->GetBackendInitializationStateString());
  EXPECT_TRUE(deferred_start ? state == kStateStringDeferred :
                               state == kStateStringStarted);
}

// Test that sync doesn't start when not requested even if all other
// conditons are met.
TEST_F(StartupControllerTest, NotRequested) {
  sync_prefs()->SetSyncSetupCompleted();
  sync_prefs()->SetSyncRequested(false);
  signin()->set_account(kTestUser);
  token_service()->UpdateCredentials(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_FALSE(started());
  EXPECT_EQ(kStateStringNotStarted,
            controller()->GetBackendInitializationStateString());
}

// Test that sync doesn't when managed even if all other conditons are met.
TEST_F(StartupControllerTest, Managed) {
  sync_prefs()->SetSyncSetupCompleted();
  sync_prefs()->SetManagedForTest(true);
  signin()->set_account(kTestUser);
  token_service()->UpdateCredentials(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_FALSE(started());
  EXPECT_EQ(kStateStringNotStarted,
            controller()->GetBackendInitializationStateString());
}

// Test that sync doesn't start until all conditions are met and a
// data type triggers sync startup.
TEST_F(StartupControllerTest, DataTypeTriggered) {
  sync_prefs()->SetSyncSetupCompleted();
  signin()->set_account(kTestUser);
  token_service()->UpdateCredentials(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_FALSE(started());
  EXPECT_EQ(kStateStringDeferred,
            controller()->GetBackendInitializationStateString());
  controller()->OnDataTypeRequestsSyncStartup(syncer::SESSIONS);
  EXPECT_TRUE(started());
  EXPECT_EQ(kStateStringStarted,
            controller()->GetBackendInitializationStateString());

  // The fallback timer shouldn't result in another invocation of the closure
  // we passed to the StartupController.
  clear_started();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(started());
}

// Test that the fallback timer starts sync in the event all
// conditions are met and no data type requests sync.
TEST_F(StartupControllerTest, FallbackTimer) {
  sync_prefs()->SetSyncSetupCompleted();
  signin()->set_account(kTestUser);
  token_service()->UpdateCredentials(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_FALSE(started());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(started());
}

// Test that we start immediately if sessions is disabled.
TEST_F(StartupControllerTest, NoDeferralWithoutSessionsSync) {
  syncer::ModelTypeSet types(syncer::UserTypes());
  // Disabling sessions means disabling 4 types due to groupings.
  types.Remove(syncer::SESSIONS);
  types.Remove(syncer::PROXY_TABS);
  types.Remove(syncer::TYPED_URLS);
  types.Remove(syncer::SUPERVISED_USER_SETTINGS);
  sync_prefs()->SetKeepEverythingSynced(false);
  sync_prefs()->SetPreferredDataTypes(syncer::UserTypes(), types);
  controller()->Reset(syncer::UserTypes());
  sync_prefs()->SetSyncSetupCompleted();
  signin()->set_account(kTestUser);
  token_service()->UpdateCredentials(kTestUser, kTestToken);
  controller()->TryStart();
  EXPECT_TRUE(started());
}

// Sanity check that the fallback timer doesn't fire before startup
// conditions are met.
TEST_F(StartupControllerTest, FallbackTimerWaits) {
  controller()->TryStart();
  EXPECT_FALSE(started());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(started());
}

// Test that sync starts when the user first asks to setup sync (which
// may be implicit due to the platform).
TEST_F(StartupControllerTest, FirstSetup) {
  signin()->set_account(kTestUser);
  token_service()->UpdateCredentials(kTestUser, kTestToken);
  controller()->TryStart();

  if (browser_defaults::kSyncAutoStarts) {
    EXPECT_TRUE(started());
  } else {
    controller()->set_setup_in_progress(true);
    controller()->TryStart();
    EXPECT_TRUE(started());
  }
}

TEST_F(StartupControllerTest, Reset) {
  sync_prefs()->SetSyncSetupCompleted();
  signin()->set_account(kTestUser);
  token_service()->UpdateCredentials(kTestUser, kTestToken);
  controller()->TryStart();
  const bool deferred_start =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSyncDisableDeferredStartup);
  EXPECT_EQ(!deferred_start, started());
  controller()->OnDataTypeRequestsSyncStartup(syncer::SESSIONS);
  EXPECT_TRUE(started());
  clear_started();
  controller()->Reset(syncer::UserTypes());
  EXPECT_FALSE(started());
  controller()->TryStart();
  // Restart is not deferred.
  EXPECT_TRUE(started());
}

// Test that setup-in-progress tracking is persistent across a Reset.
TEST_F(StartupControllerTest, ResetDuringSetup) {
  signin()->set_account(kTestUser);
  token_service()->UpdateCredentials(kTestUser, kTestToken);

  // Simulate UI telling us setup is in progress.
  controller()->set_setup_in_progress(true);

  // This could happen if the UI triggers a stop-syncing permanently call.
  controller()->Reset(syncer::UserTypes());

  // From the UI's point of view, setup is still in progress.
  EXPECT_TRUE(controller()->setup_in_progress());
}

}  // namespace browser_sync
