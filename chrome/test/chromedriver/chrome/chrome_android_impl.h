// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"

class Device;
class DevToolsClient;
class DevToolsHttpClient;

class ChromeAndroidImpl : public ChromeImpl {
 public:
  ChromeAndroidImpl(
      scoped_ptr<DevToolsHttpClient> http_client,
      scoped_ptr<DevToolsClient> websocket_client,
      ScopedVector<DevToolsEventListener>& devtools_event_listeners,
      scoped_ptr<PortReservation> port_reservation,
      scoped_ptr<Device> device);
  ~ChromeAndroidImpl() override;

  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  std::string GetOperatingSystemName() override;

  // Overridden from ChromeImpl:
  bool HasTouchScreen() const override;
  Status QuitImpl() override;

 private:
  scoped_ptr<Device> device_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_
