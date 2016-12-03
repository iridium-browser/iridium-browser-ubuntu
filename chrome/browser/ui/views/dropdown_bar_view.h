// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "ui/views/accessible_pane_view.h"

namespace gfx {
class Canvas;
class ImageSkia;
}  // namespace gfx

////////////////////////////////////////////////////////////////////////////////
//
// The DropdownBarView is an abstract view to draw the UI controls of the
// DropdownBarHost.
//
////////////////////////////////////////////////////////////////////////////////
class DropdownBarView : public views::AccessiblePaneView,
                        public DropdownBarHostDelegate {
 public:
  explicit DropdownBarView(DropdownBarHost* host);
  ~DropdownBarView() override;

 protected:
  // Returns the DropdownBarHost that manages this view.
  DropdownBarHost* host() const { return host_; }

  void SetBackground(const gfx::ImageSkia* left_alpha_mask,
                     const gfx::ImageSkia* right_alpha_mask);
  void SetBorderFromIds(int left_border_image_id,
                        int middle_border_image_id,
                        int right_border_image_id);

 private:
  // The dropdown bar host that controls this view.
  DropdownBarHost* host_;

  DISALLOW_COPY_AND_ASSIGN(DropdownBarView);
};
#endif  // CHROME_BROWSER_UI_VIEWS_DROPDOWN_BAR_VIEW_H_
