// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_INFO_H_
#define COMPONENTS_USER_MANAGER_USER_INFO_H_

#include <string>

#include "base/strings/string16.h"
#include "components/user_manager/user_id.h"
#include "components/user_manager/user_manager_export.h"

namespace gfx {
class ImageSkia;
}

namespace user_manager {

// A class that represents user related info.
class USER_MANAGER_EXPORT UserInfo {
 public:
  UserInfo();
  virtual ~UserInfo();

  // Gets the display name for the user.
  virtual base::string16 GetDisplayName() const = 0;

  // Gets the given name of the user.
  virtual base::string16 GetGivenName() const = 0;

  // Gets the display email address for the user.
  // The display email address might contains some periods in the email name
  // as well as capitalized letters. For example: "Foo.Bar@mock.com".
  virtual std::string GetEmail() const = 0;

  // Gets the user id (sanitized email address) for the user.
  // The function would return something like "foobar@mock.com".
  virtual UserID GetUserID() const = 0;

  // Gets the avatar image for the user.
  virtual const gfx::ImageSkia& GetImage() const = 0;
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_INFO_H_
