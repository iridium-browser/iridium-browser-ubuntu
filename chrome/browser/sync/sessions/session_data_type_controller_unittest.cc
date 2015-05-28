// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/glue/synced_window_delegate.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/sessions/session_data_type_controller.h"
#include "chrome/browser/sync/sessions/synced_window_delegates_getter.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_driver::LocalDeviceInfoProviderMock;

namespace browser_sync {

namespace {

class MockSyncedWindowDelegate : public SyncedWindowDelegate {
 public:
  explicit MockSyncedWindowDelegate(Profile* profile)
    : is_restore_in_progress_(false),
      profile_(profile) {}
  ~MockSyncedWindowDelegate() override {}

  bool HasWindow() const override { return false; }
  SessionID::id_type GetSessionId() const override { return 0; }
  int GetTabCount() const override { return 0; }
  int GetActiveIndex() const override { return 0; }
  bool IsApp() const override { return false; }
  bool IsTypeTabbed() const override { return false; }
  bool IsTypePopup() const override { return false; }
  bool IsTabPinned(const SyncedTabDelegate* tab) const override {
    return false;
  }
  SyncedTabDelegate* GetTabAt(int index) const override { return NULL; }
  SessionID::id_type GetTabIdAt(int index) const override { return 0; }

  bool IsSessionRestoreInProgress() const override {
    return is_restore_in_progress_;
  }

  bool ShouldSync() const override { return false; }

  void SetSessionRestoreInProgress(bool is_restore_in_progress) {
    is_restore_in_progress_ = is_restore_in_progress;

    if (!is_restore_in_progress_) {
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_SESSION_RESTORE_COMPLETE,
          content::Source<Profile>(profile_),
          content::NotificationService::NoDetails());
    }
  }

 private:
  bool is_restore_in_progress_;
  Profile* profile_;
};

class MockSyncedWindowDelegatesGetter : public SyncedWindowDelegatesGetter {
 public:
  std::set<const SyncedWindowDelegate*> GetSyncedWindowDelegates() override {
    return delegates_;
  }

  void Add(SyncedWindowDelegate* delegate) {
    delegates_.insert(delegate);
  }

 private:
  std::set<const SyncedWindowDelegate*> delegates_;
};

class SessionDataTypeControllerTest
    : public testing::Test {
 public:
  SessionDataTypeControllerTest()
      : load_finished_(false),
        thread_bundle_(content::TestBrowserThreadBundle::DEFAULT),
        last_type_(syncer::UNSPECIFIED),
        weak_ptr_factory_(this) {}
  ~SessionDataTypeControllerTest() override {}

  void SetUp() override {
    synced_window_delegate_.reset(new MockSyncedWindowDelegate(&profile_));
    synced_window_getter_.reset(new MockSyncedWindowDelegatesGetter());
    synced_window_getter_->Add(synced_window_delegate_.get());

    local_device_.reset(new LocalDeviceInfoProviderMock(
        "cache_guid",
        "Wayne Gretzky's Hacking Box",
        "Chromium 10k",
        "Chrome 10k",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
        "device_id"));

    controller_ = new SessionDataTypeController(
        &profile_sync_factory_,
        &profile_,
        synced_window_getter_.get(),
        local_device_.get());

    load_finished_ = false;
    last_type_ = syncer::UNSPECIFIED;
    last_error_ = syncer::SyncError();
  }

  void TearDown() override {
    controller_ = NULL;
    local_device_.reset();
    synced_window_getter_.reset();
    synced_window_delegate_.reset();
  }

  void Start() {
    controller_->LoadModels(
      base::Bind(&SessionDataTypeControllerTest::OnLoadFinished,
                 weak_ptr_factory_.GetWeakPtr()));
  }

  void OnLoadFinished(syncer::ModelType type, syncer::SyncError error) {
    load_finished_ = true;
    last_type_ = type;
    last_error_ = error;
  }

  testing::AssertionResult LoadResult() {
    if (!load_finished_) {
      return testing::AssertionFailure() <<
          "OnLoadFinished wasn't called";
    }

    if (last_error_.IsSet()) {
      return testing::AssertionFailure() <<
          "OnLoadFinished was called with a SyncError: " <<
          last_error_.ToString();
    }

    if (last_type_ != syncer::SESSIONS) {
      return testing::AssertionFailure() <<
          "OnLoadFinished was called with a wrong sync type: " <<
          last_type_;
    }

    return testing::AssertionSuccess();
  }

 protected:
  scoped_refptr<SessionDataTypeController> controller_;
  scoped_ptr<MockSyncedWindowDelegatesGetter> synced_window_getter_;
  scoped_ptr<LocalDeviceInfoProviderMock> local_device_;
  scoped_ptr<MockSyncedWindowDelegate> synced_window_delegate_;
  bool load_finished_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ProfileSyncComponentsFactoryMock profile_sync_factory_;
  TestingProfile profile_;
  syncer::ModelType last_type_;
  syncer::SyncError last_error_;
  base::WeakPtrFactory<SessionDataTypeControllerTest> weak_ptr_factory_;
};

TEST_F(SessionDataTypeControllerTest, StartModels) {
  Start();
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest, StartModelsDelayedByLocalDevice) {
  local_device_->SetInitialized(false);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  local_device_->SetInitialized(true);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest, StartModelsDelayedByRestoreInProgress) {
  synced_window_delegate_->SetSessionRestoreInProgress(true);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  synced_window_delegate_->SetSessionRestoreInProgress(false);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest,
       StartModelsDelayedByLocalDeviceThenRestoreInProgress) {
  local_device_->SetInitialized(false);
  synced_window_delegate_->SetSessionRestoreInProgress(true);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  local_device_->SetInitialized(true);
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  synced_window_delegate_->SetSessionRestoreInProgress(false);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest,
       StartModelsDelayedByRestoreInProgressThenLocalDevice) {
  local_device_->SetInitialized(false);
  synced_window_delegate_->SetSessionRestoreInProgress(true);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  synced_window_delegate_->SetSessionRestoreInProgress(false);
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_STARTING,
            controller_->state());

  local_device_->SetInitialized(true);
  EXPECT_EQ(sync_driver::DataTypeController::MODEL_LOADED,
            controller_->state());
  EXPECT_TRUE(LoadResult());
}

}  // namespace

}  // namespace browser_sync
