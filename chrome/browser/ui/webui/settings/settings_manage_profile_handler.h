// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MANAGE_PROFILE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MANAGE_PROFILE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class StringValue;
}

class Profile;

namespace settings {

// Chrome personal stuff profiles manage overlay UI handler.
class ManageProfileHandler : public settings::SettingsPageUIHandler,
                             public ProfileAttributesStorage::Observer {
 public:
  explicit ManageProfileHandler(Profile* profile);
  ~ManageProfileHandler() override;

  // settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ProfileAttributesStorage::Observer:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest,
                           HandleSetProfileIconAndName);
  FRIEND_TEST_ALL_PREFIXES(ManageProfileHandlerTest, HandleGetAvailableIcons);

  // Callback for the "getAvailableIcons" message.
  // Sends the array of default profile icon URLs and profile names to WebUI.
  void HandleGetAvailableIcons(const base::ListValue* args);

  // Get all the available profile icons to choose from.
  std::unique_ptr<base::ListValue> GetAvailableIcons();

  // Callback for the "setProfileIconAndName" message. Sets the name and icon
  // of a given profile.
  // |args| is of the form: [
  //   /*string*/ newProfileIconURL
  //   /*string*/ newProfileName,
  // ]
  void HandleSetProfileIconAndName(const base::ListValue* args);

  // Callback for the "requestHasProfileShortcuts" message, which is called
  // when editing an existing profile. Asks the profile shortcut manager whether
  // the profile has shortcuts and gets the result in |OnHasProfileShortcuts()|.
  // |args| is of the form: [ {string} profileFilePath ]
  void HandleRequestHasProfileShortcuts(const base::ListValue* args);

  // Callback invoked from the profile manager indicating whether the profile
  // being edited has any desktop shortcuts.
  void OnHasProfileShortcuts(bool has_shortcuts);

  // Callback for the "addProfileShortcut" message, which is called when editing
  // an existing profile and the user clicks the "Add desktop shortcut" button.
  // Adds a desktop shortcut for the profile.
  void HandleAddProfileShortcut(const base::ListValue* args);

  // Callback for the "removeProfileShortcut" message, which is called when
  // editing an existing profile and the user clicks the "Remove desktop
  // shortcut" button. Removes the desktop shortcut for the profile.
  void HandleRemoveProfileShortcut(const base::ListValue* args);

  // Non-owning pointer to the associated profile.
  Profile* profile_;

  // URL for the current profile's GAIA picture.
  std::string gaia_picture_url_;

  // Used to observe profile avatar updates.
  ScopedObserver<ProfileAttributesStorage, ManageProfileHandler> observer_;

  // For generating weak pointers to itself for callbacks.
  base::WeakPtrFactory<ManageProfileHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManageProfileHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_MANAGE_PROFILE_HANDLER_H_
