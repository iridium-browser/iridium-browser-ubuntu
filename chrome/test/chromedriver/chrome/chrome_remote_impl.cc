// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_remote_impl.h"

#include <utility>

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/port_server.h"

ChromeRemoteImpl::ChromeRemoteImpl(
    std::unique_ptr<DevToolsHttpClient> http_client,
    std::unique_ptr<DevToolsClient> websocket_client,
    ScopedVector<DevToolsEventListener>& devtools_event_listeners)
    : ChromeImpl(std::move(http_client),
                 std::move(websocket_client),
                 devtools_event_listeners,
                 std::unique_ptr<PortReservation>()) {}

ChromeRemoteImpl::~ChromeRemoteImpl() {}

Status ChromeRemoteImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError,
                "operation is unsupported with remote debugging");
}

std::string ChromeRemoteImpl::GetOperatingSystemName() {
 return std::string();
}

Status ChromeRemoteImpl::QuitImpl() {
  return Status(kOk);
}

