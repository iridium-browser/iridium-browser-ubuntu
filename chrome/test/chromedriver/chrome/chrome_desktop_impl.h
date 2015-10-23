// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"

namespace base {
class TimeDelta;
}

class AutomationExtension;
class DevToolsClient;
class DevToolsHttpClient;
class Status;
class WebView;

class ChromeDesktopImpl : public ChromeImpl {
 public:
  ChromeDesktopImpl(
      scoped_ptr<DevToolsHttpClient> http_client,
      scoped_ptr<DevToolsClient> websocket_client,
      ScopedVector<DevToolsEventListener>& devtools_event_listeners,
      scoped_ptr<PortReservation> port_reservation,
      base::Process process,
      const base::CommandLine& command,
      base::ScopedTempDir* user_data_dir,
      base::ScopedTempDir* extension_dir);
  ~ChromeDesktopImpl() override;

  // Waits for a page with the given URL to appear and finish loading.
  // Returns an error if the timeout is exceeded.
  Status WaitForPageToLoad(const std::string& url,
                           const base::TimeDelta& timeout,
                           scoped_ptr<WebView>* web_view);

  // Gets the installed automation extension.
  Status GetAutomationExtension(AutomationExtension** extension);

  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  std::string GetOperatingSystemName() override;

  // Overridden from ChromeImpl:
  bool IsMobileEmulationEnabled() const override;
  bool HasTouchScreen() const override;
  Status QuitImpl() override;

  const base::CommandLine& command() const;

 private:
  base::Process process_;
  base::CommandLine command_;
  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir extension_dir_;

  // Lazily initialized, may be null.
  scoped_ptr<AutomationExtension> automation_extension_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
