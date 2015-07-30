// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_web_state.h"

namespace web {

TestWebState::TestWebState() : trust_level_(kAbsolute), content_is_html_(true) {
}

TestWebState::~TestWebState() = default;

UIView* TestWebState::GetView() {
  return nullptr;
}

WebViewType TestWebState::GetWebViewType() const {
  return web::UI_WEB_VIEW_TYPE;
}

BrowserState* TestWebState::GetBrowserState() const {
  return nullptr;
}

NavigationManager* TestWebState::GetNavigationManager() {
  return nullptr;
}

CRWJSInjectionReceiver* TestWebState::GetJSInjectionReceiver() const {
  return nullptr;
}

const std::string& TestWebState::GetContentsMimeType() const {
  return mime_type_;
}

const std::string& TestWebState::GetContentLanguageHeader() const {
  return content_language_;
}

bool TestWebState::ContentIsHTML() const {
  return content_is_html_;
}

const GURL& TestWebState::GetVisibleURL() const {
  return url_;
}

const GURL& TestWebState::GetLastCommittedURL() const {
  return url_;
}

GURL TestWebState::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  *trust_level = trust_level_;
  return url_;
}

bool TestWebState::IsShowingWebInterstitial() const {
  return false;
}

WebInterstitial* TestWebState::GetWebInterstitial() const {
  return nullptr;
}

void TestWebState::SetContentIsHTML(bool content_is_html) {
  content_is_html_ = content_is_html;
}

bool TestWebState::IsLoading() const {
  return false;
}

void TestWebState::SetCurrentURL(const GURL& url) {
  url_ = url;
}

void TestWebState::SetTrustLevel(URLVerificationTrustLevel trust_level) {
  trust_level_ = trust_level;
}

CRWWebViewProxyType TestWebState::GetWebViewProxy() const {
  return nullptr;
}

int TestWebState::DownloadImage(const GURL& url,
                                bool is_favicon,
                                uint32_t max_bitmap_size,
                                bool bypass_cache,
                                const ImageDownloadCallback& callback) {
  return 0;
}

}  // namespace web
