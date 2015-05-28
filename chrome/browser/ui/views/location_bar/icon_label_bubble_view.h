// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class FontList;
class ImageSkia;
}

namespace views {
class ImageView;
class Label;
class Painter;
}

// View used to draw a bubble to the left of the address, containing an icon and
// a label.  We use this as a base for the classes that handle the EV bubble and
// tab-to-search UI.
class IconLabelBubbleView : public views::View {
 public:
  // |hover_background_images| is an optional set of images to be used in place
  // of |background_images| during mouse hover.
  IconLabelBubbleView(const int background_images[],
                      const int hover_background_images[],
                      int contained_image,
                      const gfx::FontList& font_list,
                      SkColor text_color,
                      SkColor parent_background_color,
                      bool elide_in_middle);
  ~IconLabelBubbleView() override;

  void SetLabel(const base::string16& label);
  void SetImage(const gfx::ImageSkia& image);
  void set_is_extension_icon(bool is_extension_icon) {
    is_extension_icon_ = is_extension_icon;
  }

 protected:
  // views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  const gfx::FontList& font_list() const { return label_->font_list(); }

  gfx::Size GetSizeForLabelWidth(int width) const;

 private:
  // Amount of padding at the edges of the bubble.  If |by_icon| is true, this
  // is the padding next to the icon; otherwise it's the padding next to the
  // label.  (We increase padding next to the label by the amount of padding
  // "built in" to the icon in order to make the bubble appear to have
  // symmetrical padding.)
  static int GetBubbleOuterPadding(bool by_icon);

  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  int GetPreLabelWidth() const;

  // For painting the background.
  scoped_ptr<views::Painter> background_painter_;
  scoped_ptr<views::Painter> hover_background_painter_;

  // The contents of the bubble.
  views::ImageView* image_;
  views::Label* label_;

  bool is_extension_icon_;
  bool in_hover_;

  DISALLOW_COPY_AND_ASSIGN(IconLabelBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ICON_LABEL_BUBBLE_VIEW_H_
