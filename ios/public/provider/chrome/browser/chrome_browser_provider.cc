// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#include "base/logging.h"

namespace ios {

namespace {
ChromeBrowserProvider* g_chrome_browser_provider = nullptr;
}  // namespace

void SetChromeBrowserProvider(ChromeBrowserProvider* provider) {
  g_chrome_browser_provider = provider;
}

ChromeBrowserProvider* GetChromeBrowserProvider() {
  return g_chrome_browser_provider;
}

ChromeBrowserProvider::~ChromeBrowserProvider() {
}

// A dummy implementation of ChromeBrowserProvider.

ChromeBrowserProvider::ChromeBrowserProvider() {
}

net::URLRequestContextGetter*
ChromeBrowserProvider::GetSystemURLRequestContext() {
  return nullptr;
}

PrefService* ChromeBrowserProvider::GetLocalState() {
  return nullptr;
}

ProfileOAuth2TokenServiceIOSProvider*
ChromeBrowserProvider::GetProfileOAuth2TokenServiceIOSProvider() {
  return nullptr;
}

UpdatableResourceProvider*
ChromeBrowserProvider::GetUpdatableResourceProvider() {
  return nullptr;
}

ChromeBrowserStateManager*
ChromeBrowserProvider::GetChromeBrowserStateManager() {
  return nullptr;
}

InfoBarViewPlaceholder ChromeBrowserProvider::CreateInfoBarView(
    CGRect frame,
    InfoBarViewDelegate* delegate) {
  return nullptr;
}

ChromeIdentityService* ChromeBrowserProvider::GetChromeIdentityService() {
  return nullptr;
}

StringProvider* ChromeBrowserProvider::GetStringProvider() {
  return nullptr;
}

GeolocationUpdaterProvider*
ChromeBrowserProvider::GetGeolocationUpdaterProvider() {
  return nullptr;
}

std::string ChromeBrowserProvider::GetDistributionBrandCode() {
  return std::string();
}

const char* ChromeBrowserProvider::GetChromeUIScheme() {
  return nullptr;
}

void ChromeBrowserProvider::SetUIViewAlphaWithAnimation(UIView* view,
                                                        float alpha) {
}

metrics::MetricsService* ChromeBrowserProvider::GetMetricsService() {
  return nullptr;
}

autofill::CardUnmaskPromptView*
ChromeBrowserProvider::CreateCardUnmaskPromptView(
    autofill::CardUnmaskPromptController* controller) {
  return nullptr;
}

std::string ChromeBrowserProvider::GetRiskData() {
  return std::string();
}

rappor::RapporService* ChromeBrowserProvider::GetRapporService() {
  return nullptr;
}

}  // namespace ios
