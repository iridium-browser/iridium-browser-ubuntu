// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_client.h"

#include <Foundation/Foundation.h>

namespace web {

static WebClient* g_client;

void SetWebClient(WebClient* client) {
  g_client = client;
}

WebClient* GetWebClient() {
  return g_client;
}

WebClient::WebClient() {
}

WebClient::~WebClient() {
}

WebMainParts* WebClient::CreateWebMainParts() {
  return nullptr;
}

WebViewFactory* WebClient::GetWebViewFactory() const {
  return nullptr;
}

std::string WebClient::GetAcceptLangs(BrowserState* state) const {
  return std::string();
}

std::string WebClient::GetApplicationLocale() const {
  return "en-US";
}

bool WebClient::IsAppSpecificURL(const GURL& url) const {
  return false;
}

base::string16 WebClient::GetPluginNotSupportedText() const {
  return base::string16();
}

std::string WebClient::GetProduct() const {
  return std::string();
}

std::string WebClient::GetUserAgent(bool desktop_user_agent) const {
  return std::string();
}

base::string16 WebClient::GetLocalizedString(int message_id) const {
  return base::string16();
}

base::StringPiece WebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return base::StringPiece();
}

base::RefCountedStaticMemory* WebClient::GetDataResourceBytes(
    int resource_id) const {
  return nullptr;
}

NSString* WebClient::GetEarlyPageScript(WebViewType web_view_type) const {
  return @"";
}

}  // namespace web
