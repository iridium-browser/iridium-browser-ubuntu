// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_controller.h"

#import <UIKit/UIKit.h>

#include <memory>

#import "base/mac/scoped_nsautorelease_pool.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/ntp/centering_scrollview.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_controller.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_table_view_controller.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::_;
using testing::AtLeast;
using testing::Return;

namespace {

std::unique_ptr<KeyedService> CreateSyncSetupService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  syncer::SyncService* sync_service =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(
          chrome_browser_state);
  return base::MakeUnique<SyncSetupServiceMock>(
      sync_service, chrome_browser_state->GetPrefs());
}

class ProfileSyncServiceMockForRecentTabsPanelController
    : public browser_sync::ProfileSyncServiceMock {
 public:
  explicit ProfileSyncServiceMockForRecentTabsPanelController(
      InitParams init_params)
      : browser_sync::ProfileSyncServiceMock(std::move(init_params)) {}
  ~ProfileSyncServiceMockForRecentTabsPanelController() override {}

  MOCK_METHOD0(GetOpenTabsUIDelegate, sync_sessions::OpenTabsUIDelegate*());
};

std::unique_ptr<KeyedService>
BuildMockProfileSyncServiceForRecentTabsPanelController(
    web::BrowserState* context) {
  return base::MakeUnique<ProfileSyncServiceMockForRecentTabsPanelController>(
      CreateProfileSyncServiceParamsForTest(
          nullptr, ios::ChromeBrowserState::FromBrowserState(context)));
}

class OpenTabsUIDelegateMock : public sync_sessions::OpenTabsUIDelegate {
 public:
  OpenTabsUIDelegateMock() {}
  ~OpenTabsUIDelegateMock() override {}

  MOCK_CONST_METHOD2(GetSyncedFaviconForPageURL,
                     bool(const std::string& pageurl,
                          scoped_refptr<base::RefCountedMemory>* favicon_png));
  MOCK_METHOD1(
      GetAllForeignSessions,
      bool(std::vector<const sync_sessions::SyncedSession*>* sessions));
  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID::id_type tab_id,
                    const sessions::SessionTab** tab));
  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));
  MOCK_METHOD2(GetForeignSession,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionWindow*>* windows));
  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));
  MOCK_METHOD1(GetLocalSession,
               bool(const sync_sessions::SyncedSession** local));
};

class RecentTabsPanelControllerTest : public BlockCleanupTest {
 public:
  RecentTabsPanelControllerTest() : no_error_(GoogleServiceAuthError::NONE) {}

 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(SyncSetupServiceFactory::GetInstance(),
                                       &CreateSyncSetupService);
    test_cbs_builder.AddTestingFactory(
        IOSChromeProfileSyncServiceFactory::GetInstance(),
        &BuildMockProfileSyncServiceForRecentTabsPanelController);
    chrome_browser_state_ = test_cbs_builder.Build();

    ProfileSyncServiceMockForRecentTabsPanelController* sync_service =
        static_cast<ProfileSyncServiceMockForRecentTabsPanelController*>(
            IOSChromeProfileSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
    EXPECT_CALL(*sync_service, GetAuthError())
        .WillRepeatedly(::testing::ReturnRef(no_error_));
    ON_CALL(*sync_service, GetRegisteredDataTypes())
        .WillByDefault(Return(syncer::ModelTypeSet()));
    sync_service->Initialize();
    EXPECT_CALL(*sync_service, IsEngineInitialized())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_service, GetOpenTabsUIDelegate())
        .WillRepeatedly(Return(nullptr));

    mock_table_view_controller_.reset([[OCMockObject
        niceMockForClass:[RecentTabsTableViewController class]] retain]);
  }

  void SetupSyncState(BOOL signedIn,
                      BOOL syncEnabled,
                      BOOL hasForeignSessions) {
    SigninManager* siginManager = ios::SigninManagerFactory::GetForBrowserState(
        chrome_browser_state_.get());
    if (signedIn)
      siginManager->SetAuthenticatedAccountInfo("test", "test");
    else if (siginManager->IsAuthenticated())
      siginManager->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);

    SyncSetupServiceMock* syncSetupService = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    EXPECT_CALL(*syncSetupService, IsSyncEnabled())
        .WillRepeatedly(Return(syncEnabled));
    EXPECT_CALL(*syncSetupService, IsDataTypeEnabled(syncer::PROXY_TABS))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*syncSetupService, GetSyncServiceState())
        .WillRepeatedly(Return(SyncSetupService::kNoSyncServiceError));

    if (syncEnabled) {
      ProfileSyncServiceMockForRecentTabsPanelController* sync_service =
          static_cast<ProfileSyncServiceMockForRecentTabsPanelController*>(
              IOSChromeProfileSyncServiceFactory::GetForBrowserState(
                  chrome_browser_state_.get()));
      open_tabs_ui_delegate_.reset(new OpenTabsUIDelegateMock());
      EXPECT_CALL(*sync_service, GetOpenTabsUIDelegate())
          .WillRepeatedly(Return(open_tabs_ui_delegate_.get()));
      EXPECT_CALL(*open_tabs_ui_delegate_, GetAllForeignSessions(_))
          .WillRepeatedly(Return(hasForeignSessions));
    }
  }

  void CreateController() {
    // Sets up the test expectations for the Sync Service Observer Bridge.
    // RecentTabsPanelController must be added as an observer of
    // ProfileSyncService changes and removed when it is destroyed.
    browser_sync::ProfileSyncServiceMock* sync_service =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            IOSChromeProfileSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
    EXPECT_CALL(*sync_service, AddObserver(_)).Times(AtLeast(1));
    EXPECT_CALL(*sync_service, RemoveObserver(_)).Times(AtLeast(1));
    controller_.reset([[RecentTabsPanelController alloc]
        initWithController:(RecentTabsTableViewController*)
                               mock_table_view_controller_.get()
              browserState:chrome_browser_state_.get()]);
  }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  GoogleServiceAuthError no_error_;
  IOSChromeScopedTestingLocalState local_state_;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<OpenTabsUIDelegateMock> open_tabs_ui_delegate_;

  // Must be declared *after* |chrome_browser_state_| so it can outlive it.
  base::scoped_nsobject<OCMockObject> mock_table_view_controller_;
  base::scoped_nsobject<RecentTabsPanelController> controller_;

  // Sets up a private Autorelease Pool so objects retained by OCMockObject
  // are released as soon as possible. Otherwise, weak pointers in the
  // objects retained by OCMockObject may surface as a BADACC when the
  // unit test autorelease pool is released.
  base::mac::ScopedNSAutoreleasePool pool_;
};

TEST_F(RecentTabsPanelControllerTest, TestConstructorDestructor) {
  CreateController();
  EXPECT_TRUE(controller_.get());
}

TEST_F(RecentTabsPanelControllerTest, TestUserSignedOut) {
  [[mock_table_view_controller_ expect]
      refreshUserState:SessionsSyncUserState::USER_SIGNED_OUT];
  SetupSyncState(NO, NO, NO);
  CreateController();
  EXPECT_OCMOCK_VERIFY(mock_table_view_controller_);
}

TEST_F(RecentTabsPanelControllerTest, TestUserSignedInSyncOff) {
  [[mock_table_view_controller_ expect]
      refreshUserState:SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF];
  SetupSyncState(YES, NO, NO);
  CreateController();
  EXPECT_OCMOCK_VERIFY(mock_table_view_controller_);
}

TEST_F(RecentTabsPanelControllerTest, TestUserSignedInSyncInProgress) {
  [[mock_table_view_controller_ expect]
      refreshUserState:SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS];
  SetupSyncState(YES, YES, NO);
  CreateController();
  EXPECT_OCMOCK_VERIFY(mock_table_view_controller_);
}

TEST_F(RecentTabsPanelControllerTest, TestUserSignedInSyncOnWithSessions) {
  [[mock_table_view_controller_ expect]
      refreshUserState:SessionsSyncUserState::
                           USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS];
  SetupSyncState(YES, YES, YES);
  CreateController();
  EXPECT_OCMOCK_VERIFY(mock_table_view_controller_);
}

}  // namespace
