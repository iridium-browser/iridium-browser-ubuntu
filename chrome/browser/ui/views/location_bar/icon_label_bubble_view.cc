// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"


IconLabelBubbleView::IconLabelBubbleView(const int background_images[],
                                         const int hover_background_images[],
                                         int contained_image,
                                         const gfx::FontList& font_list,
                                         SkColor text_color,
                                         SkColor parent_background_color,
                                         bool elide_in_middle)
    : background_painter_(
          views::Painter::CreateImageGridPainter(background_images)),
      image_(new views::ImageView()),
      label_(new views::Label(base::string16(), font_list)),
      is_extension_icon_(false),
      in_hover_(false) {
  if (contained_image) {
    image_->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            contained_image));
  }

  // Disable separate hit testing for |image_|.  This prevents views treating
  // |image_| as a separate mouse hover region from |this|.
  image_->set_interactive(false);
  AddChildView(image_);

  if (hover_background_images) {
    hover_background_painter_.reset(
        views::Painter::CreateImageGridPainter(hover_background_images));
  }

  label_->SetEnabledColor(text_color);
  // Calculate the actual background color for the label.  The background images
  // are painted atop |parent_background_color|.  We grab the color of the
  // middle pixel of the middle image of the background, which we treat as the
  // representative color of the entire background (reasonable, given the
  // current appearance of these images).  Then we alpha-blend it over the
  // parent background color to determine the actual color the label text will
  // sit atop.
  const SkBitmap& bitmap(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          background_images[4])->GetRepresentation(1.0f).sk_bitmap());
  SkAutoLockPixels pixel_lock(bitmap);
  SkColor background_image_color =
      bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2);
  // Tricky bit: We alpha blend an opaque version of |background_image_color|
  // against |parent_background_color| using the original image grid color's
  // alpha. This is because AlphaBlend(a, b, 255) always returns |a| unchanged
  // even if |a| is a color with non-255 alpha.
  label_->SetBackgroundColor(
      color_utils::AlphaBlend(SkColorSetA(background_image_color, 255),
                              parent_background_color,
                              SkColorGetA(background_image_color)));
  if (elide_in_middle)
    label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  AddChildView(label_);
}

IconLabelBubbleView::~IconLabelBubbleView() {
}

void IconLabelBubbleView::SetLabel(const base::string16& label) {
  label_->SetText(label);
}

void IconLabelBubbleView::SetImage(const gfx::ImageSkia& image_skia) {
  image_->SetImage(image_skia);
}

bool IconLabelBubbleView::ShouldShowBackground() const {
  return true;
}

double IconLabelBubbleView::WidthMultiplier() const {
  return 1.0;
}

gfx::Size IconLabelBubbleView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(label_->GetPreferredSize().width());
}

void IconLabelBubbleView::Layout() {
  const int image_width = image()->GetPreferredSize().width();
  image_->SetBounds(std::min((width() - image_width) / 2,
                             GetBubbleOuterPadding(!is_extension_icon_)),
                    0, image_->GetPreferredSize().width(), height());
  const int horizontal_item_padding = GetThemeProvider()->GetDisplayProperty(
      ThemeProperties::PROPERTY_LOCATION_BAR_HORIZONTAL_PADDING);
  int pre_label_width =
      GetBubbleOuterPadding(true) +
      (image_width ? (image_width + horizontal_item_padding) : 0);

  label_->SetBounds(pre_label_width, 0,
                    width() - pre_label_width - GetBubbleOuterPadding(false),
                    height());
}

gfx::Size IconLabelBubbleView::GetSizeForLabelWidth(int width) const {
  gfx::Size size(image_->GetPreferredSize());
  if (ShouldShowBackground()) {
    const int image_width = image_->GetPreferredSize().width();
    const int horizontal_item_padding = GetThemeProvider()->GetDisplayProperty(
        ThemeProperties::PROPERTY_LOCATION_BAR_HORIZONTAL_PADDING);
    const int non_label_width =
        GetBubbleOuterPadding(true) +
        (image_width ? (image_width + horizontal_item_padding) : 0) +
        GetBubbleOuterPadding(false);
    size = gfx::Size(WidthMultiplier() * (width + non_label_width), 0);
    size.SetToMax(background_painter_->GetMinimumSize());
  }

  return size;
}

void IconLabelBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  in_hover_ = true;
  if (hover_background_painter_.get())
    SchedulePaint();
}

void IconLabelBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  in_hover_ = false;
  if (hover_background_painter_.get())
    SchedulePaint();
}

int IconLabelBubbleView::GetBubbleOuterPadding(bool by_icon) const {
  ui::ThemeProvider* theme_provider = GetThemeProvider();
  const int bubble_horizontal_padding = theme_provider->GetDisplayProperty(
      ThemeProperties::PROPERTY_LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING);
  const int horizontal_item_padding = theme_provider->GetDisplayProperty(
      ThemeProperties::PROPERTY_LOCATION_BAR_HORIZONTAL_PADDING);
  const int right_padding = theme_provider->GetDisplayProperty(
      ThemeProperties::PROPERTY_ICON_LABEL_VIEW_TRAILING_PADDING);
  return horizontal_item_padding - bubble_horizontal_padding +
         (by_icon ? 0 : right_padding);
}

const char* IconLabelBubbleView::GetClassName() const {
  return "IconLabelBubbleView";
}

void IconLabelBubbleView::OnPaint(gfx::Canvas* canvas) {
  if (!ShouldShowBackground())
    return;
  views::Painter* painter = (in_hover_ && hover_background_painter_) ?
      hover_background_painter_.get() : background_painter_.get();
  painter->Paint(canvas, size());
}
