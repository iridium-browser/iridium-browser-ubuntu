// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/scoped_testing_web_client.h"

#include "base/logging.h"
#include "ios/web/public/web_client.h"

namespace web {

ScopedTestingWebClient::ScopedTestingWebClient(
    std::unique_ptr<WebClient> web_client)
    : web_client_(std::move(web_client)), original_web_client_(GetWebClient()) {
  SetWebClient(web_client_.get());
}

ScopedTestingWebClient::~ScopedTestingWebClient() {
  DCHECK_EQ(GetWebClient(), web_client_.get());
  SetWebClient(original_web_client_);
}

WebClient* ScopedTestingWebClient::Get() {
  return web_client_.get();
}

}  // namespace web
