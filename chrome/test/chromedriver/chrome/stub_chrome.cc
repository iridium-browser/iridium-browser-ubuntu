// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

StubChrome::StubChrome() {}

StubChrome::~StubChrome() {}

Status StubChrome::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "not supported");
}

const BrowserInfo* StubChrome::GetBrowserInfo() const {
  return &browser_info_;
}

bool StubChrome::HasCrashedWebView() {
  return false;
}

Status StubChrome::GetWebViewIds(std::list<std::string>* web_view_ids) {
  return Status(kOk);
}

Status StubChrome::GetWebViewById(const std::string& id, WebView** web_view) {
  return Status(kOk);
}

Status StubChrome::CloseWebView(const std::string& id) {
  return Status(kOk);
}

Status StubChrome::ActivateWebView(const std::string& id) {
  return Status(kOk);
}

std::string StubChrome::GetOperatingSystemName() {
  return std::string();
}

bool StubChrome::IsMobileEmulationEnabled() const {
  return false;
}

bool StubChrome::HasTouchScreen() const {
  return false;
}

Status StubChrome::Quit() {
  return Status(kOk);
}
