// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {

class MockObserver : public AvatarMenuObserver {
 public:
  MockObserver() : count_(0) {}
  ~MockObserver() override {}

  void OnAvatarMenuChanged(AvatarMenu* avatar_menu) override { ++count_; }

  int change_count() const { return count_; }

 private:
  int count_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

}  // namespace

namespace chromeos {

class ProfileListChromeOSTest : public testing::Test {
 public:
  ProfileListChromeOSTest()
      : manager_(TestingBrowserProcess::GetGlobal()) {
  }

  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());

    // AvatarMenu and multiple profiles works after user logged in.
    manager_.SetLoggedIn(true);

    // Initialize the UserManager singleton to a fresh FakeChromeUserManager
    // instance.
    user_manager_enabler_.reset(
        new ScopedUserManagerEnabler(new FakeChromeUserManager));
  }

  FakeChromeUserManager* GetFakeChromeUserManager() {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void AddProfile(base::string16 name, bool log_in) {
    std::string email_string = base::UTF16ToASCII(name) + "@example.com";

    // Add a user to the fake user manager.
    GetFakeChromeUserManager()->AddUser(email_string);
    if (log_in)
      GetFakeChromeUserManager()->LoginUser(email_string);

    // Create a profile for the user.
    manager()->CreateTestingProfile(email_string);
  }

  AvatarMenu* GetAvatarMenu() {
    // Reset the MockObserver.
    mock_observer_.reset(new MockObserver());
    EXPECT_EQ(0, change_count());

    // Reset the menu.
    avatar_menu_.reset(new AvatarMenu(
        manager()->profile_info_cache(),
        mock_observer_.get(),
        NULL));
    avatar_menu_->RebuildMenu();
    EXPECT_EQ(0, change_count());
    return avatar_menu_.get();
  }

  void ActiveUserChanged(const base::string16& name) {
    std::string email_string = base::UTF16ToASCII(name) + "@example.com";
    GetFakeChromeUserManager()->SwitchActiveUser(email_string);
  }

  TestingProfileManager* manager() { return &manager_; }

  int change_count() const { return mock_observer_->change_count(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager manager_;
  scoped_ptr<MockObserver> mock_observer_;
  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  scoped_ptr<AvatarMenu> avatar_menu_;
  ChromeShellDelegate chrome_shell_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ProfileListChromeOSTest);
};

TEST_F(ProfileListChromeOSTest, InitialCreation) {
  base::string16 name1(ASCIIToUTF16("p1"));

  AddProfile(name1, true);

  AvatarMenu* menu = GetAvatarMenu();

  ASSERT_EQ(1U, menu->GetNumberOfItems());

  const AvatarMenu::Item& item1 = menu->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(name1, item1.name);
}

TEST_F(ProfileListChromeOSTest, ShowLoggedInUsers) {
  base::string16 name1(ASCIIToUTF16("p1"));
  base::string16 name2(ASCIIToUTF16("p2"));
  base::string16 name3(ASCIIToUTF16("p3"));
  base::string16 name4(ASCIIToUTF16("p4"));

  AddProfile(name1, true);
  AddProfile(name2, false);
  AddProfile(name3, true);
  AddProfile(name4, false);

  AvatarMenu* menu = GetAvatarMenu();

  ASSERT_EQ(2U, menu->GetNumberOfItems());

  const AvatarMenu::Item& item1 = menu->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(name1, item1.name);

  const AvatarMenu::Item& item3 = menu->GetItemAt(1);
  EXPECT_EQ(1U, item3.menu_index);
  EXPECT_EQ(name3, item3.name);
}

TEST_F(ProfileListChromeOSTest, DontShowSupervisedUsers) {
  base::string16 name1(ASCIIToUTF16("p1"));
  base::string16 supervised_name(ASCIIToUTF16("p2@example.com"));

  AddProfile(name1, true);

  // Add a managed user profile.
  ProfileInfoCache* cache = manager()->profile_info_cache();
  manager()->profile_info_cache()->AddProfileToCache(
      cache->GetUserDataDir().AppendASCII("p2"), supervised_name, std::string(),
      base::string16(), 0, "TEST_ID");

  GetFakeChromeUserManager()->AddUser(base::UTF16ToASCII(supervised_name));

  AvatarMenu* menu = GetAvatarMenu();
  ASSERT_EQ(1U, menu->GetNumberOfItems());

  const AvatarMenu::Item& item1 = menu->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(name1, item1.name);
}

TEST_F(ProfileListChromeOSTest, ShowAddProfileLink) {
  base::string16 name1(ASCIIToUTF16("p1.com"));
  base::string16 name2(ASCIIToUTF16("p2.com"));

  AddProfile(name1, true);
  AddProfile(name2, false);

  AvatarMenu* menu = GetAvatarMenu();

  ASSERT_EQ(1U, menu->GetNumberOfItems());
  EXPECT_TRUE(menu->ShouldShowAddNewProfileLink());
}

TEST_F(ProfileListChromeOSTest, DontShowAddProfileLink) {
  base::string16 name1(ASCIIToUTF16("p1.com"));
  base::string16 name2(ASCIIToUTF16("p2.com"));

  AddProfile(name1, true);
  AddProfile(name2, true);

  AvatarMenu* menu = GetAvatarMenu();

  ASSERT_EQ(2U, menu->GetNumberOfItems());
  EXPECT_FALSE(menu->ShouldShowAddNewProfileLink());
}

TEST_F(ProfileListChromeOSTest, ActiveItem) {
  base::string16 name1(ASCIIToUTF16("p1.com"));
  base::string16 name2(ASCIIToUTF16("p2.com"));

  AddProfile(name1, true);
  AddProfile(name2, true);

  ActiveUserChanged(name1);

  AvatarMenu* menu = GetAvatarMenu();

  ASSERT_EQ(2U, menu->GetNumberOfItems());
  // TODO(jeremy): Expand test to verify active profile index other than 0
  // crbug.com/100871
  ASSERT_EQ(0U, menu->GetActiveProfileIndex());
}

TEST_F(ProfileListChromeOSTest, ModifyingNameResortsCorrectly) {
  base::string16 name1(ASCIIToUTF16("Alpha"));
  base::string16 name2(ASCIIToUTF16("Beta"));
  base::string16 newname1(ASCIIToUTF16("Gamma"));

  AddProfile(name1, true);
  AddProfile(name2, true);

  AvatarMenu* menu = GetAvatarMenu();
  EXPECT_EQ(0, change_count());
  ASSERT_EQ(2U, menu->GetNumberOfItems());

  const AvatarMenu::Item& item1 = menu->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(name1, item1.name);

  const AvatarMenu::Item& item2 = menu->GetItemAt(1);
  EXPECT_EQ(1U, item2.menu_index);
  EXPECT_EQ(name2, item2.name);

  // Change name of the first profile, to trigger resorting of the profiles:
  // now the first menu item should be named "beta", and the second be "gamma".
  GetFakeChromeUserManager()->SaveUserDisplayName(
      base::UTF16ToASCII(name1) + "@example.com", newname1);
  manager()->profile_info_cache()->SetNameOfProfileAtIndex(0, newname1);
  EXPECT_EQ(1, change_count());

  const AvatarMenu::Item& item1next = menu->GetItemAt(0);
  EXPECT_EQ(0U, item1next.menu_index);
  EXPECT_EQ(name2, item1next.name);

  const AvatarMenu::Item& item2next = menu->GetItemAt(1);
  EXPECT_EQ(1U, item2next.menu_index);
  EXPECT_EQ(newname1, item2next.name);
}

TEST_F(ProfileListChromeOSTest, ChangeOnNotify) {
  base::string16 name1(ASCIIToUTF16("p1.com"));
  base::string16 name2(ASCIIToUTF16("p2.com"));

  AddProfile(name1, true);
  AddProfile(name2, true);

  AvatarMenu* menu = GetAvatarMenu();
  EXPECT_EQ(2U, menu->GetNumberOfItems());
  EXPECT_EQ(0, change_count());

  base::string16 name3(ASCIIToUTF16("p3.com"));
  AddProfile(name3, true);

  // Three changes happened via the call to CreateTestingProfile: adding the
  // profile to the cache, setting the user name (which rebuilt the list of
  // profiles after the name change), and changing the avatar.
  EXPECT_EQ(change_count(), 3);
  ASSERT_EQ(3U, menu->GetNumberOfItems());

  const AvatarMenu::Item& item1 = menu->GetItemAt(0);
  EXPECT_EQ(0U, item1.menu_index);
  EXPECT_EQ(name1, item1.name);

  const AvatarMenu::Item& item2 = menu->GetItemAt(1);
  EXPECT_EQ(1U, item2.menu_index);
  EXPECT_EQ(name2, item2.name);

  const AvatarMenu::Item& item3 = menu->GetItemAt(2);
  EXPECT_EQ(2U, item3.menu_index);
  EXPECT_EQ(name3, item3.name);
}

TEST_F(ProfileListChromeOSTest, DontShowAvatarMenu) {
  // If in the new M-32 UX mode the icon gets shown, the menu will not.
  base::string16 name1(ASCIIToUTF16("p1"));
  base::string16 name2(ASCIIToUTF16("p2"));

  AddProfile(name1, true);

  // Should only show avatar menu with multiple users.
  EXPECT_FALSE(AvatarMenu::ShouldShowAvatarMenu());

  AddProfile(name2, false);

  EXPECT_FALSE(AvatarMenu::ShouldShowAvatarMenu());
}

}  // namespace chromeos
