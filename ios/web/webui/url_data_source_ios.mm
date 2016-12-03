// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/url_data_source_ios.h"

#include "ios/web/public/web_client.h"
#include "ios/web/webui/url_data_manager_ios.h"
#include "net/url_request/url_request.h"

namespace web {

void URLDataSourceIOS::Add(BrowserState* browser_state,
                           URLDataSourceIOS* source) {
  URLDataManagerIOS::AddDataSource(browser_state, source);
}

bool URLDataSourceIOS::ShouldReplaceExistingSource() const {
  return true;
}

bool URLDataSourceIOS::AllowCaching() const {
  return true;
}

std::string URLDataSourceIOS::GetContentSecurityPolicyObjectSrc() const {
  return "object-src 'none';";
}

bool URLDataSourceIOS::ShouldDenyXFrameOptions() const {
  return true;
}

bool URLDataSourceIOS::ShouldServiceRequest(
    const net::URLRequest* request) const {
  if (GetWebClient()->IsAppSpecificURL(request->url()))
    return true;
  return false;
}

}  // namespace web
