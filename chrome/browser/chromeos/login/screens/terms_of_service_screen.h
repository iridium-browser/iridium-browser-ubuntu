// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/terms_of_service_screen_actor.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
}

namespace chromeos {

class BaseScreenDelegate;

// A screen that shows Terms of Service which have been configured through
// policy. The screen is shown during login and requires the user to accept the
// Terms of Service before proceeding. Currently, Terms of Service are available
// for public sessions only.
class TermsOfServiceScreen : public BaseScreen,
                             public TermsOfServiceScreenActor::Delegate,
                             public net::URLFetcherDelegate {
 public:
  TermsOfServiceScreen(BaseScreenDelegate* base_screen_delegate,
                       TermsOfServiceScreenActor* actor);
  ~TermsOfServiceScreen() override;

  // BaseScreen:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  std::string GetName() const override;

  // TermsOfServiceScreenActor::Delegate:
  void OnDecline() override;
  void OnAccept() override;
  void OnActorDestroyed(TermsOfServiceScreenActor* actor) override;

 private:
  // Start downloading the Terms of Service.
  void StartDownload();

  // Abort the attempt to download the Terms of Service if it takes too long.
  void OnDownloadTimeout();

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  TermsOfServiceScreenActor* actor_;

  std::unique_ptr<net::URLFetcher> terms_of_service_fetcher_;

  // Timer that enforces a custom (shorter) timeout on the attempt to download
  // the Terms of Service.
  base::OneShotTimer download_timer_;

  DISALLOW_COPY_AND_ASSIGN(TermsOfServiceScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_
