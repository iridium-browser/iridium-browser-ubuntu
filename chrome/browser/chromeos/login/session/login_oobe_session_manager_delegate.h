// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_LOGIN_OOBE_SESSION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_LOGIN_OOBE_SESSION_MANAGER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

class LoginOobeSessionManagerDelegate
    : public session_manager::SessionManagerDelegate {
 public:
  LoginOobeSessionManagerDelegate();
  ~LoginOobeSessionManagerDelegate() override;

 private:
  // session_manager::SessionManagerDelegate implementation:
  void Start() override;

  DISALLOW_COPY_AND_ASSIGN(LoginOobeSessionManagerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_LOGIN_OOBE_SESSION_MANAGER_DELEGATE_H_
