// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/search/instant_overlay_controller.h"

class Browser;
@class OverlayableContentsController;

class InstantOverlayControllerMac : public InstantOverlayController {
 public:
  InstantOverlayControllerMac(Browser* browser,
                              OverlayableContentsController* overlay);
  ~InstantOverlayControllerMac() override;

 private:
  // Overridden from InstantOverlayController:
  void OverlayStateChanged(const InstantOverlayModel& model) override;

  OverlayableContentsController* const overlay_;

  DISALLOW_COPY_AND_ASSIGN(InstantOverlayControllerMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_
