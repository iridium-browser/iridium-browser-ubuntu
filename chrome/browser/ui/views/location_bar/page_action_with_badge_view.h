// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_WITH_BADGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_WITH_BADGE_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

class PageActionImageView;

namespace content {
class WebContents;
}

// A container for the PageActionImageView plus its badge.
class PageActionWithBadgeView : public views::View {
 public:
  explicit PageActionWithBadgeView(PageActionImageView* image_view);

  PageActionImageView* image_view() { return image_view_; }

  // views::View:
  void GetAccessibleState(ui::AXViewState* state) override;
  gfx::Size GetPreferredSize() const override;

  void UpdateVisibility(content::WebContents* contents);

 private:
  // views::View:
  const char* GetClassName() const override;

  // The button this view contains.
  PageActionImageView* image_view_;

  DISALLOW_COPY_AND_ASSIGN(PageActionWithBadgeView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_WITH_BADGE_VIEW_H_
