// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_

#include "base/basictypes.h"

namespace password_manager {

namespace ui {

// The current state of the password manager's UI.
enum State {
  // The password manager has nothing to do with the current site.
  INACTIVE_STATE,

  // A password is pending.
  PENDING_PASSWORD_STATE,

  // A password has been saved and we wish to display UI confirming the save
  // to the user.
  CONFIRMATION_STATE,

  // A password has been autofilled, or has just been saved. The icon needs
  // to be visible, in the management state.
  MANAGE_STATE,

  // The user has blacklisted the site rendered in the current WebContents.
  // The icon needs to be visible, in the blacklisted state.
  BLACKLIST_STATE,

  // The site has asked user to choose a credential.
  CREDENTIAL_REQUEST_STATE,

  // The user was auto signed in to the site. The icon and the auto-signin toast
  // should be visible.
  AUTO_SIGNIN_STATE,
};

// The position of a password item in a list of credentials.
enum PasswordItemPosition {
  // The password item is the first in the list.
  FIRST_ITEM,

  // The password item is not the first item in the list.
  SUBSEQUENT_ITEM,
};

}  // namespace ui

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_UI_H_
