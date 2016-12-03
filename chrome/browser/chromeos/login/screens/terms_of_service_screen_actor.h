// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_ACTOR_H_

#include <string>

namespace chromeos {

// Interface for dependency injection between TermsOfServiceScreen and its
// WebUI representation.
class TermsOfServiceScreenActor {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the user declines the Terms of Service.
    virtual void OnDecline() = 0;

    // Called when the user accepts the Terms of Service.
    virtual void OnAccept() = 0;

    // Called when actor is destroyed so there is no dead reference to it.
    virtual void OnActorDestroyed(TermsOfServiceScreenActor* actor) = 0;
  };

  virtual ~TermsOfServiceScreenActor() {}

  // Sets screen this actor belongs to.
  virtual void SetDelegate(Delegate* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Sets the domain name whose Terms of Service are being shown.
  virtual void SetDomain(const std::string& domain) = 0;

  // Called when the download of the Terms of Service fails. Show an error
  // message to the user.
  virtual void OnLoadError() = 0;

  // Called when the download of the Terms of Service is successful. Shows the
  // downloaded |terms_of_service| to the user.
  virtual void OnLoadSuccess(const std::string& terms_of_service) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_ACTOR_H_
