// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"

#if defined(OS_CHROMEOS)
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#endif

class Profile;

namespace settings {

class ProfileInfoHandler : public SettingsPageUIHandler,
#if defined(OS_CHROMEOS)
                           public content::NotificationObserver,
#endif
                           public ProfileAttributesStorage::Observer {
 public:
  static const char kProfileInfoChangedEventName[];
  static const char kProfileManagesSupervisedUsersChangedEventName[];

  explicit ProfileInfoHandler(Profile* profile);
  ~ProfileInfoHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

#if defined(OS_CHROMEOS)
  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;
#endif

  // ProfileAttributesStorage::Observer implementation.
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest, GetProfileInfo);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest, PushProfileInfo);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest,
                           GetProfileManagesSupervisedUsers);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest,
                           PushProfileManagesSupervisedUsers);

  // Callbacks from the page.
  void HandleGetProfileInfo(const base::ListValue* args);
  void HandleGetProfileManagesSupervisedUsers(const base::ListValue* args);

  void PushProfileInfo();

  // Pushes whether the current profile manages supervised users to JavaScript.
  void PushProfileManagesSupervisedUsersStatus();

  // Returns true if this profile manages supervised users.
  bool IsProfileManagingSupervisedUsers() const;

  std::unique_ptr<base::DictionaryValue> GetAccountNameAndIcon() const;

  // Weak pointer.
  Profile* profile_;

  ScopedObserver<ProfileAttributesStorage, ProfileInfoHandler>
      profile_observer_;

  // Used to listen for changes in the list of managed supervised users.
  PrefChangeRegistrar profile_pref_registrar_;

#if defined(OS_CHROMEOS)
  // Used to listen to ChromeOS user image changes.
  content::NotificationRegistrar registrar_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_
