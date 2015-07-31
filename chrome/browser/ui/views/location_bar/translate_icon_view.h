// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_TRANSLATE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_TRANSLATE_ICON_VIEW_H_

#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

class CommandUpdater;

// The icon to show the Translate bubble where the user can have the page
// tarnslated.
class TranslateIconView : public BubbleIconView {
 public:
  explicit TranslateIconView(CommandUpdater* command_updater);
  ~TranslateIconView() override;

  // Toggles the icon on or off.
  void SetToggled(bool on);

 protected:
  // BubbleIconView:
  void OnExecuting(BubbleIconView::ExecuteSource execute_source) override;
  views::BubbleDelegateView* GetBubble() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TranslateIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_TRANSLATE_ICON_VIEW_H_
