// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"

class Browser;
class Profile;

namespace extensions {

// Class to encapsulate logic to control the browser UI for hosted apps.
class HostedAppBrowserController {
 public:
  // Indicates whether |browser| is a hosted app browser.
  static bool IsForHostedApp(Browser* browser);

  explicit HostedAppBrowserController(Browser* browser);
  ~HostedAppBrowserController();

  // Whether the browser being controlled can ever show the location bar.
  bool SupportsLocationBar() const;

  // Whether the browser being controlled should be currently showing the
  // location bar.
  bool ShouldShowLocationBar() const;

  // Updates the location bar visibility based on whether it should be
  // currently visible or not. If |animate| is set, the change will be
  // animated.
  void UpdateLocationBarVisibility(bool animate) const;

  // Whether the controlled browser should use the web app style frame.
  bool should_use_web_app_frame() const { return should_use_web_app_frame_; }

 private:
  Browser* browser_;
  const std::string extension_id_;
  bool should_use_web_app_frame_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppBrowserController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
