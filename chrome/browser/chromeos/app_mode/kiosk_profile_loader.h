// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chromeos/login/auth/login_performer.h"

class Profile;

namespace chromeos {

// KioskProfileLoader loads a special profile for a given app. It first
// attempts to login for the app's generated user id. If the login is
// successful, it prepares app profile then calls the delegate.
class KioskProfileLoader : public LoginPerformer::Delegate,
                           public UserSessionManagerDelegate {
 public:
  class Delegate {
   public:
    virtual void OnProfileLoaded(Profile* profile) = 0;
    virtual void OnProfileLoadFailed(KioskAppLaunchError::Error error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  KioskProfileLoader(const std::string& app_user_id,
                     bool use_guest_mount,
                     Delegate* delegate);

  ~KioskProfileLoader() override;

  // Starts profile load. Calls delegate on success or failure.
  void Start();

 private:
  class CryptohomedChecker;

  void LoginAsKioskAccount();
  void ReportLaunchResult(KioskAppLaunchError::Error error);

  // LoginPerformer::Delegate overrides:
  void OnAuthSuccess(const UserContext& user_context) override;
  void OnAuthFailure(const AuthFailure& error) override;
  void WhiteListCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;
  void OnOnlineChecked(const std::string& email, bool success) override;

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;

  std::string user_id_;
  bool use_guest_mount_;
  Delegate* delegate_;
  scoped_ptr<CryptohomedChecker> cryptohomed_checker_;
  scoped_ptr<LoginPerformer> login_performer_;

  DISALLOW_COPY_AND_ASSIGN(KioskProfileLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_
