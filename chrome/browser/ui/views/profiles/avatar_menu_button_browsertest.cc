// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/avatar_menu_button.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/signin/core/common/profile_management_switches.h"

class AvatarMenuButtonTest : public InProcessBrowserTest {
 public:
  AvatarMenuButtonTest();
  ~AvatarMenuButtonTest() override;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void CreateTestingProfile();
  AvatarMenuButton* GetAvatarMenuButton();
  void StartAvatarMenu();

 private:
  DISALLOW_COPY_AND_ASSIGN(AvatarMenuButtonTest);
};

AvatarMenuButtonTest::AvatarMenuButtonTest() {
}

AvatarMenuButtonTest::~AvatarMenuButtonTest() {
}

void AvatarMenuButtonTest::SetUpCommandLine(base::CommandLine* command_line) {
  switches::DisableNewAvatarMenuForTesting(command_line);
}

void AvatarMenuButtonTest::CreateTestingProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_EQ(1u, profile_manager->GetNumberOfProfiles());

  base::FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII("test_profile");
  if (!base::PathExists(path))
    ASSERT_TRUE(base::CreateDirectory(path));
  Profile* profile =
      Profile::CreateProfile(path, NULL, Profile::CREATE_MODE_SYNCHRONOUS);
  profile_manager->RegisterTestingProfile(profile, true, false);
  EXPECT_EQ(2u, profile_manager->GetNumberOfProfiles());
}

AvatarMenuButton* AvatarMenuButtonTest::GetAvatarMenuButton() {
  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  return browser_view->frame()->GetAvatarMenuButton();
}
