// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/protocol/sync_protocol_error.h"

using bookmarks::BookmarkNode;
using bookmarks_helper::AddFolder;
using bookmarks_helper::SetTitle;
using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {

class SyncDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncDisabledChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return !service()->setup_in_progress() &&
           !service()->HasSyncSetupCompleted();
  }

  std::string GetDebugMessage() const override { return "Sync Disabled"; }
};

class TypeDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit TypeDisabledChecker(ProfileSyncService* service,
                               syncer::ModelType type)
      : SingleClientStatusChangeChecker(service), type_(type) {}

  bool IsExitConditionSatisfied() override {
    return !service()->GetActiveDataTypes().Has(type_);
  }

  std::string GetDebugMessage() const override { return "Type disabled"; }
 private:
   syncer::ModelType type_;
};

bool AwaitSyncDisabled(ProfileSyncService* service) {
  SyncDisabledChecker checker(service);
  checker.Wait();
  return !checker.TimedOut();
}

bool AwaitTypeDisabled(ProfileSyncService* service,
                       syncer::ModelType type) {
  TypeDisabledChecker checker(service, type);
  checker.Wait();
  return !checker.TimedOut();
}

class SyncErrorTest : public SyncTest {
 public:
  SyncErrorTest() : SyncTest(SINGLE_CLIENT) {}
  ~SyncErrorTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncErrorTest);
};

// Helper class that waits until the sync engine has hit an actionable error.
class ActionableErrorChecker : public SingleClientStatusChangeChecker {
 public:
  explicit ActionableErrorChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  ~ActionableErrorChecker() override {}

  // Checks if an actionable error has been hit. Called repeatedly each time PSS
  // notifies observers of a state change.
  bool IsExitConditionSatisfied() override {
    ProfileSyncService::Status status;
    service()->QueryDetailedSyncStatus(&status);
    return (status.sync_protocol_error.action != syncer::UNKNOWN_ACTION &&
            service()->HasUnrecoverableError());
  }

  std::string GetDebugMessage() const override {
    return "ActionableErrorChecker";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ActionableErrorChecker);
};

IN_PROC_BROWSER_TEST_F(SyncErrorTest, BirthdayErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Add an item, wait for sync, and trigger a birthday error on the server.
  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  GetFakeServer()->ClearServerData();

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");
  ASSERT_TRUE(AwaitSyncDisabled(GetSyncService((0))));
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest, ActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  std::string description = "Not My Fault";
  std::string url = "www.google.com";
  EXPECT_TRUE(GetFakeServer()->TriggerActionableError(
      sync_pb::SyncEnums::TRANSIENT_ERROR,
      description,
      url,
      sync_pb::SyncEnums::UPGRADE_CLIENT));

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");

  // Wait until an actionable error is encountered.
  ActionableErrorChecker actionable_error_checker(GetSyncService((0)));
  actionable_error_checker.Wait();
  ASSERT_FALSE(actionable_error_checker.TimedOut());

  ProfileSyncService::Status status;
  GetSyncService((0))->QueryDetailedSyncStatus(&status);
  ASSERT_EQ(status.sync_protocol_error.error_type, syncer::TRANSIENT_ERROR);
  ASSERT_EQ(status.sync_protocol_error.action, syncer::UPGRADE_CLIENT);
  ASSERT_EQ(status.sync_protocol_error.url, url);
  ASSERT_EQ(status.sync_protocol_error.error_description, description);
}

// TODO(sync): Fix failing test on Chrome OS: http://crbug.com/351160
IN_PROC_BROWSER_TEST_F(SyncErrorTest, DISABLED_ErrorWhileSettingUpAutoStart) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetSyncService(0)->auto_start_enabled());

  // In auto start enabled platforms like chrome os we should be
  // able to set up even if the first sync while setting up fails.
  EXPECT_TRUE(GetFakeServer()->TriggerError(
      sync_pb::SyncEnums::TRANSIENT_ERROR));
  EXPECT_TRUE(GetFakeServer()->EnableAlternatingTriggeredErrors());
  // Now setup sync and it should succeed.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
}

#if defined(OS_CHROMEOS)
#define MAYBE_ErrorWhileSettingUp DISABLED_ErrorWhileSettingUp
#else
#define MAYBE_ErrorWhileSettingUp ErrorWhileSettingUp
#endif
IN_PROC_BROWSER_TEST_F(SyncErrorTest, MAYBE_ErrorWhileSettingUp) {
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(GetSyncService(0)->auto_start_enabled());

  // In Non auto start enabled environments if the setup sync fails then
  // the setup would fail. So setup sync normally.
  ASSERT_TRUE(SetupSync()) << "Setup sync failed";
  ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::AUTOFILL));

  EXPECT_TRUE(GetFakeServer()->TriggerError(
      sync_pb::SyncEnums::TRANSIENT_ERROR));
  EXPECT_TRUE(GetFakeServer()->EnableAlternatingTriggeredErrors());

  // Now enable a datatype, whose first 2 syncs would fail, but we should
  // recover and setup succesfully on the third attempt.
  ASSERT_TRUE(GetClient(0)->EnableSyncForDatatype(syncer::AUTOFILL));
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest, BirthdayErrorUsingActionableErrorTest) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));

  std::string description = "Not My Fault";
  std::string url = "www.google.com";
  EXPECT_TRUE(GetFakeServer()->TriggerActionableError(
      sync_pb::SyncEnums::NOT_MY_BIRTHDAY,
      description,
      url,
      sync_pb::SyncEnums::DISABLE_SYNC_ON_CLIENT));

  // Now make one more change so we will do another sync.
  const BookmarkNode* node2 = AddFolder(0, 0, "title2");
  SetTitle(0, node2, "new_title2");
  ASSERT_TRUE(AwaitSyncDisabled(GetSyncService((0))));
  ProfileSyncService::Status status;
  GetSyncService((0))->QueryDetailedSyncStatus(&status);
  ASSERT_EQ(status.sync_protocol_error.error_type, syncer::NOT_MY_BIRTHDAY);
  ASSERT_EQ(status.sync_protocol_error.action, syncer::DISABLE_SYNC_ON_CLIENT);
  ASSERT_EQ(status.sync_protocol_error.url, url);
  ASSERT_EQ(status.sync_protocol_error.error_description, description);
}

IN_PROC_BROWSER_TEST_F(SyncErrorTest, DisableDatatypeWhileRunning) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  syncer::ModelTypeSet synced_datatypes =
      GetSyncService((0))->GetActiveDataTypes();
  ASSERT_TRUE(synced_datatypes.Has(syncer::TYPED_URLS));
  ASSERT_TRUE(synced_datatypes.Has(syncer::SESSIONS));
  GetProfile(0)->GetPrefs()->SetBoolean(
      prefs::kSavingBrowserHistoryDisabled, true);

  // Wait for reconfigurations.
  ASSERT_TRUE(AwaitTypeDisabled(GetSyncService(0), syncer::TYPED_URLS));
  ASSERT_TRUE(AwaitTypeDisabled(GetSyncService(0), syncer::SESSIONS));

  const BookmarkNode* node1 = AddFolder(0, 0, "title1");
  SetTitle(0, node1, "new_title1");
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  // TODO(lipalani)" Verify initial sync ended for typed url is false.
}

}  // namespace
