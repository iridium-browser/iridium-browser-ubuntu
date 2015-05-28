// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_INTERSTITIAL_DELEGATE_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_INTERSTITIAL_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

class LoginHandler;

namespace content {
class WebContents;
}

namespace net {
class AuthChallengeInfo;
}

// Placeholder interstitial for HTTP login prompts. This interstitial makes the
// omnibox show the correct url when the login prompt is visible.
class LoginInterstitialDelegate : public content::InterstitialPageDelegate {
 public:
  // Interstitial type, used in tests.
  static content::InterstitialPageDelegate::TypeID kTypeForTesting;

  LoginInterstitialDelegate(content::WebContents* web_contents,
                            const GURL& request_url,
                            base::Closure& callback);

  ~LoginInterstitialDelegate() override;

  // content::InterstitialPageDelegate:
  void CommandReceived(const std::string& command) override;
  content::InterstitialPageDelegate::TypeID GetTypeForTesting() const override;

 protected:
  std::string GetHTMLContents() override;

 private:
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(LoginInterstitialDelegate);
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_INTERSTITIAL_DELEGATE_H_
