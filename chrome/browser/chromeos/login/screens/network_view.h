// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_VIEW_H_

#include "base/strings/string16.h"

namespace chromeos {

class NetworkModel;

// Interface for dependency injection between NetworkScreen and its actual
// representation, either views based or WebUI. Owned by NetworkScreen.
class NetworkView {
 public:
  virtual ~NetworkView() {}

  // Prepare the contents to showing.
  virtual void PrepareToShow() = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds |model| to the view.
  virtual void Bind(NetworkModel& model) = 0;

  // Unbinds model from the view.
  virtual void Unbind() = 0;

  // Shows error message in a bubble.
  virtual void ShowError(const base::string16& message) = 0;

  // Hides error messages showing no error state.
  virtual void ClearErrors() = 0;

  // Stops demo mode detection.
  virtual void StopDemoModeDetection() = 0;

  // Shows network connecting status or network selection otherwise.
  virtual void ShowConnectingStatus(bool connecting,
                                    const base::string16& network_id) = 0;

  // Reloads localized contents.
  virtual void ReloadLocalizedContent() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_VIEW_H_
