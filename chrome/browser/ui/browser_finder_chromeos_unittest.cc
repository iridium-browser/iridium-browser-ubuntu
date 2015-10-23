// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace test {

namespace {

const char kTestAccount1[] = "user1@test.com";
const char kTestAccount2[] = "user2@test.com";

}  // namespace

class BrowserFinderChromeOSTest : public BrowserWithTestWindowTest {
 protected:
  BrowserFinderChromeOSTest() : multi_user_window_manager_(nullptr) {}

  TestingProfile* CreateMultiUserProfile(const std::string& user_email) {
    TestingProfile* profile =
        profile_manager_->CreateTestingProfile(user_email);
    GetUserWindowManager()->AddUser(profile);
    return profile;
  }

  chrome::MultiUserWindowManagerChromeOS* GetUserWindowManager() {
    if (!multi_user_window_manager_) {
      multi_user_window_manager_ =
          new chrome::MultiUserWindowManagerChromeOS(kTestAccount1);
      multi_user_window_manager_->Init();
      chrome::MultiUserWindowManager::SetInstanceForTest(
          multi_user_window_manager_,
          chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED);
    }
    return multi_user_window_manager_;
  }

 private:
  void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_manager_->SetLoggedIn(true);
    chromeos::WallpaperManager::Initialize();
    BrowserWithTestWindowTest::SetUp();
    second_profile_ = CreateMultiUserProfile(kTestAccount2);
  }

  void TearDown() override {
    chrome::MultiUserWindowManager::DeleteInstance();
    BrowserWithTestWindowTest::TearDown();
    chromeos::WallpaperManager::Shutdown();
    if (second_profile_) {
      DestroyProfile(second_profile_);
      second_profile_ = nullptr;
    }
  }

  TestingProfile* CreateProfile() override {
    return CreateMultiUserProfile(kTestAccount1);
  }

  void DestroyProfile(TestingProfile* test_profile) override {
    profile_manager_->DeleteTestingProfile(test_profile->GetProfileUserName());
  }

  TestingProfile* second_profile_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFinderChromeOSTest);
};

TEST_F(BrowserFinderChromeOSTest, IncognitoBrowserMatchTest) {
  // GetBrowserCount() use kMatchAll to find all browser windows for profile().
  EXPECT_EQ(1u,
            chrome::GetBrowserCount(profile(), chrome::HOST_DESKTOP_TYPE_ASH));
  EXPECT_TRUE(
      chrome::FindAnyBrowser(profile(), true, chrome::HOST_DESKTOP_TYPE_ASH));
  EXPECT_TRUE(
      chrome::FindAnyBrowser(profile(), false, chrome::HOST_DESKTOP_TYPE_ASH));
  set_browser(nullptr);

  // Create an incognito browser.
  Browser::CreateParams params(profile()->GetOffTheRecordProfile(),
                               chrome::HOST_DESKTOP_TYPE_ASH);
  scoped_ptr<Browser> incognito_browser(
      chrome::CreateBrowserWithAuraTestWindowForParams(nullptr, &params));
  // Incognito windows are excluded in GetBrowserCount() because kMatchAll
  // doesn't match original profile of the browser with the given profile.
  EXPECT_EQ(0u,
            chrome::GetBrowserCount(profile(), chrome::HOST_DESKTOP_TYPE_ASH));
  EXPECT_TRUE(
      chrome::FindAnyBrowser(profile(), true, chrome::HOST_DESKTOP_TYPE_ASH));
  EXPECT_FALSE(
      chrome::FindAnyBrowser(profile(), false, chrome::HOST_DESKTOP_TYPE_ASH));
}

TEST_F(BrowserFinderChromeOSTest, FindBrowserOwnedByAnotherProfile) {
  set_browser(nullptr);

  Browser::CreateParams params(profile()->GetOriginalProfile(),
                               chrome::HOST_DESKTOP_TYPE_ASH);
  scoped_ptr<Browser> browser(
      chrome::CreateBrowserWithAuraTestWindowForParams(nullptr, &params));
  GetUserWindowManager()->SetWindowOwner(browser->window()->GetNativeWindow(),
                                         kTestAccount1);
  EXPECT_EQ(1u,
            chrome::GetBrowserCount(profile(), chrome::HOST_DESKTOP_TYPE_ASH));
  EXPECT_TRUE(
      chrome::FindAnyBrowser(profile(), true, chrome::HOST_DESKTOP_TYPE_ASH));
  EXPECT_TRUE(
      chrome::FindAnyBrowser(profile(), false, chrome::HOST_DESKTOP_TYPE_ASH));

  // Move the browser window to another user's desktop. Then no window should
  // be available for the current profile.
  GetUserWindowManager()->ShowWindowForUser(
      browser->window()->GetNativeWindow(), kTestAccount2);
  EXPECT_EQ(0u,
            chrome::GetBrowserCount(profile(), chrome::HOST_DESKTOP_TYPE_ASH));
  EXPECT_FALSE(
      chrome::FindAnyBrowser(profile(), true, chrome::HOST_DESKTOP_TYPE_ASH));
  EXPECT_FALSE(
      chrome::FindAnyBrowser(profile(), false, chrome::HOST_DESKTOP_TYPE_ASH));
}

}  // namespace test
