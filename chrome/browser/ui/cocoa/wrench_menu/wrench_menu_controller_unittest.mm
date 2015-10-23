// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sessions/sessions_sync_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/view_resizer_pong.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"
#include "chrome/browser/ui/toolbar/recent_tabs_builder_test_helper.h"
#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/wrench_menu_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_driver/local_device_info_provider_mock.h"
#include "grit/theme_resources.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class MockWrenchMenuModel : public WrenchMenuModel {
 public:
  MockWrenchMenuModel() : WrenchMenuModel() {}
  ~MockWrenchMenuModel() {
    // This dirty, ugly hack gets around a bug in the test. In
    // ~WrenchMenuModel(), there's a call to TabstripModel::RemoveObserver(this)
    // which mysteriously leads to this crash: http://crbug.com/49206 .  It
    // seems that the vector of observers is getting hosed somewhere between
    // |-[ToolbarController dealloc]| and ~MockWrenchMenuModel(). This line
    // short-circuits the parent destructor to avoid this crash.
    tab_strip_model_ = NULL;
  }
  MOCK_METHOD2(ExecuteCommand, void(int command_id, int event_flags));
};

class DummyRouter : public browser_sync::LocalSessionEventRouter {
 public:
  ~DummyRouter() override {}
  void StartRoutingTo(
      browser_sync::LocalSessionEventHandler* handler) override {}
  void Stop() override {}
};

class WrenchMenuControllerTest
    : public CocoaProfileTest {
 public:
  WrenchMenuControllerTest()
      : local_device_(new sync_driver::LocalDeviceInfoProviderMock(
            "WrenchMenuControllerTest",
            "Test Machine",
            "Chromium 10k",
            "Chrome 10k",
            sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
            "device_id")) {
  }

  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    controller_.reset([[WrenchMenuController alloc] initWithBrowser:browser()]);
    fake_model_.reset(new MockWrenchMenuModel);

    manager_.reset(new browser_sync::SessionsSyncManager(
        profile(),
        local_device_.get(),
        scoped_ptr<browser_sync::LocalSessionEventRouter>(
            new DummyRouter())));
    manager_->MergeDataAndStartSyncing(
        syncer::SESSIONS,
        syncer::SyncDataList(),
        scoped_ptr<syncer::SyncChangeProcessor>(
            new syncer::FakeSyncChangeProcessor),
        scoped_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock));
  }

  void RegisterRecentTabs(RecentTabsBuilderTestHelper* helper) {
    helper->ExportToSessionsSyncManager(manager_.get());
  }

  sync_driver::OpenTabsUIDelegate* GetOpenTabsDelegate() {
    return manager_.get();
  }

  void TearDown() override {
    fake_model_.reset();
    controller_.reset();
    manager_.reset();
    CocoaProfileTest::TearDown();
  }

  WrenchMenuController* controller() {
    return controller_.get();
  }

  base::scoped_nsobject<WrenchMenuController> controller_;

  scoped_ptr<MockWrenchMenuModel> fake_model_;

 private:
  scoped_ptr<browser_sync::SessionsSyncManager> manager_;
  scoped_ptr<sync_driver::LocalDeviceInfoProviderMock> local_device_;
};

TEST_F(WrenchMenuControllerTest, Initialized) {
  EXPECT_TRUE([controller() menu]);
  EXPECT_GE([[controller() menu] numberOfItems], 5);
}

TEST_F(WrenchMenuControllerTest, DispatchSimple) {
  base::scoped_nsobject<NSButton> button([[NSButton alloc] init]);
  [button setTag:IDC_ZOOM_PLUS];

  // Set fake model to test dispatching.
  EXPECT_CALL(*fake_model_, ExecuteCommand(IDC_ZOOM_PLUS, 0));
  [controller() setModel:fake_model_.get()];

  [controller() dispatchWrenchMenuCommand:button.get()];
  chrome::testing::NSRunLoopRunAllPending();
}

TEST_F(WrenchMenuControllerTest, RecentTabsFavIcon) {
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  recent_tabs_builder.AddTab(0, 0);
  RegisterRecentTabs(&recent_tabs_builder);

  RecentTabsSubMenuModel recent_tabs_sub_menu_model(
      NULL, browser(), GetOpenTabsDelegate());
  fake_model_->AddSubMenuWithStringId(
      IDC_RECENT_TABS_MENU, IDS_RECENT_TABS_MENU,
      &recent_tabs_sub_menu_model);

  [controller() setModel:fake_model_.get()];
  NSMenu* menu = [controller() menu];
  [controller() updateRecentTabsSubmenu];

  NSString* title = l10n_util::GetNSStringWithFixup(IDS_RECENT_TABS_MENU);
  NSMenu* recent_tabs_menu = [[menu itemWithTitle:title] submenu];
  EXPECT_TRUE(recent_tabs_menu);
  EXPECT_EQ(6, [recent_tabs_menu numberOfItems]);

  // Send a icon changed event and verify that the icon is updated.
  gfx::Image icon(ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_BOOKMARKS_FAVICON));
  recent_tabs_sub_menu_model.SetIcon(3, icon);
  EXPECT_NSNE(icon.ToNSImage(), [[recent_tabs_menu itemAtIndex:3] image]);
  recent_tabs_sub_menu_model.GetMenuModelDelegate()->OnIconChanged(3);
  EXPECT_TRUE([[recent_tabs_menu itemAtIndex:3] image]);
  EXPECT_NSEQ(icon.ToNSImage(), [[recent_tabs_menu itemAtIndex:3] image]);

  controller_.reset();
  fake_model_.reset();
}

TEST_F(WrenchMenuControllerTest, RecentTabsElideTitle) {
  // Add 1 session with 1 window and 2 tabs.
  RecentTabsBuilderTestHelper recent_tabs_builder;
  recent_tabs_builder.AddSession();
  recent_tabs_builder.AddWindow(0);
  base::string16 tab1_short_title = base::ASCIIToUTF16("Short");
  recent_tabs_builder.AddTabWithInfo(0, 0, base::Time::Now(), tab1_short_title);
  base::string16 tab2_long_title = base::ASCIIToUTF16(
      "Very very very very very very very very very very very very long");
  recent_tabs_builder.AddTabWithInfo(0, 0,
      base::Time::Now() - base::TimeDelta::FromMinutes(10), tab2_long_title);
  RegisterRecentTabs(&recent_tabs_builder);

  RecentTabsSubMenuModel recent_tabs_sub_menu_model(
      NULL, browser(), GetOpenTabsDelegate());
  fake_model_->AddSubMenuWithStringId(
      IDC_RECENT_TABS_MENU, IDS_RECENT_TABS_MENU,
      &recent_tabs_sub_menu_model);

  [controller() setModel:fake_model_.get()];
  NSMenu* menu = [controller() menu];
  [controller() updateRecentTabsSubmenu];

  NSString* title = l10n_util::GetNSStringWithFixup(IDS_RECENT_TABS_MENU);
  NSMenu* recent_tabs_menu = [[menu itemWithTitle:title] submenu];
  EXPECT_TRUE(recent_tabs_menu);
  EXPECT_EQ(7, [recent_tabs_menu numberOfItems]);

  // Item 1: separator.
  EXPECT_TRUE([[recent_tabs_menu itemAtIndex:1] isSeparatorItem]);

  // Index 2: restore tabs menu item.
  NSString* restore_tab_label = l10n_util::FixUpWindowsStyleLabel(
      recent_tabs_sub_menu_model.GetLabelAt(2));
  EXPECT_NSEQ(restore_tab_label, [[recent_tabs_menu itemAtIndex:2] title]);

  // Item 3: separator.
  EXPECT_TRUE([[recent_tabs_menu itemAtIndex:3] isSeparatorItem]);

  // Item 4: window title.
  EXPECT_NSEQ(
      base::SysUTF16ToNSString(recent_tabs_sub_menu_model.GetLabelAt(4)),
      [[recent_tabs_menu itemAtIndex:4] title]);

  // Item 5: short tab title.
  EXPECT_NSEQ(base::SysUTF16ToNSString(tab1_short_title),
              [[recent_tabs_menu itemAtIndex:5] title]);

  // Item 6: long tab title.
  NSString* tab2_actual_title = [[recent_tabs_menu itemAtIndex:6] title];
  NSUInteger title_length = [tab2_actual_title length];
  EXPECT_GT(tab2_long_title.size(), title_length);
  NSString* actual_substring =
      [tab2_actual_title substringToIndex:title_length - 1];
  NSString* expected_substring = [base::SysUTF16ToNSString(tab2_long_title)
      substringToIndex:title_length - 1];
  EXPECT_NSEQ(expected_substring, actual_substring);

  controller_.reset();
  fake_model_.reset();
}

// Verify that |RecentTabsMenuModelDelegate| is deleted before the model
// it's observing.
TEST_F(WrenchMenuControllerTest, RecentTabDeleteOrder) {
  [controller_ menuNeedsUpdate:[controller_ menu]];
  // If the delete order is wrong then the test will crash on exit.
}

class BrowserRemovedObserver : public chrome::BrowserListObserver {
 public:
  BrowserRemovedObserver() { BrowserList::AddObserver(this); }
  ~BrowserRemovedObserver() override { BrowserList::RemoveObserver(this); }
  void WaitUntilBrowserRemoved() { run_loop_.Run(); }
  void OnBrowserRemoved(Browser* browser) override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRemovedObserver);
};

// Test that WrenchMenuController can be destroyed after the Browser.
// This can happen because the WrenchMenuController's owner (ToolbarController)
// can outlive the Browser.
TEST_F(WrenchMenuControllerTest, DestroyedAfterBrowser) {
  BrowserRemovedObserver observer;
  // This is normally called by ToolbarController, but since |controller_| is
  // not owned by one, call it here.
  [controller_ browserWillBeDestroyed];
  CloseBrowserWindow();
  observer.WaitUntilBrowserRemoved();
  // |controller_| is released in TearDown().
}

}  // namespace
