// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_INTERFACE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_INTERFACE_H_

#include "base/macros.h"
#include "base/values.h"

class GURL;

namespace vr_shell {

class UiCommandHandler {
 public:
  virtual void SendCommandToUi(const base::Value& value) = 0;
};

// This class manages the communication of browser state from VR shell to the
// HTML UI. State information is asynchronous and unidirectional.
class UiInterface {
 public:
  enum Mode {
    STANDARD,
    WEB_VR
  };

  explicit UiInterface(Mode initial_mode, bool fullscreen);
  virtual ~UiInterface();

  void SetMode(Mode mode);
  Mode GetMode() { return mode_; }
  void SetMenuMode(bool enabled);
  bool GetMenuMode() { return menu_mode_; }
  void SetFullscreen(bool enabled);
  bool GetFullscreen() { return fullscreen_; }
  void SetSecurityLevel(int level);
  void SetWebVRSecureOrigin(bool secure);
  void SetLoading(bool loading);
  void SetLoadProgress(double progress);
  void SetURL(const GURL& url);

  // Called by WebUI when starting VR.
  void OnDomContentsLoaded();
  void SetUiCommandHandler(UiCommandHandler* handler);

 private:
  void FlushUpdates();
  void FlushModeState();

  Mode mode_;
  bool menu_mode_ = false;
  bool fullscreen_ = false;
  UiCommandHandler* handler_;
  bool loaded_ = false;
  base::DictionaryValue updates_;

  DISALLOW_COPY_AND_ASSIGN(UiInterface);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_INTERFACE_H_
