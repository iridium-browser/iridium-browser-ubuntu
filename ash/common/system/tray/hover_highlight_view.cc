// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/hover_highlight_view.h"

#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/resources/grit/views_resources.h"

namespace {

const int kCheckLabelPadding = 4;

const gfx::FontList& GetFontList(bool highlight) {
  return ui::ResourceBundle::GetSharedInstance().GetFontList(
      highlight ? ui::ResourceBundle::BoldFont : ui::ResourceBundle::BaseFont);
}

}  // namespace

namespace ash {

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : ActionableView(nullptr),
      listener_(listener),
      highlight_color_(kHoverBackgroundColor) {
  set_notify_enter_exit_on_child(true);
}

HoverHighlightView::~HoverHighlightView() {}

bool HoverHighlightView::GetTooltipText(const gfx::Point& p,
                                        base::string16* tooltip) const {
  if (tooltip_.empty())
    return false;
  *tooltip = tooltip_;
  return true;
}

void HoverHighlightView::AddRightIcon(const gfx::ImageSkia& image,
                                      int icon_size) {
  DCHECK(box_layout_);
  DCHECK(!right_icon_);

  right_icon_ = new FixedSizedImageView(icon_size, icon_size);
  right_icon_->SetImage(image);
  right_icon_->SetEnabled(enabled());
  AddChildView(right_icon_);
}

void HoverHighlightView::SetRightIconVisible(bool visible) {
  if (!right_icon_)
    return;

  right_icon_->SetVisible(visible);
  Layout();
}

void HoverHighlightView::AddIconAndLabel(const gfx::ImageSkia& image,
                                         const base::string16& text,
                                         bool highlight) {
  box_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 3,
                                     kTrayPopupPaddingBetweenItems);
  SetLayoutManager(box_layout_);
  DoAddIconAndLabel(image, kTrayPopupDetailsIconWidth, text, highlight);
}

void HoverHighlightView::AddIconAndLabelCustomSize(const gfx::ImageSkia& image,
                                                   const base::string16& text,
                                                   bool highlight,
                                                   int icon_size,
                                                   int indent,
                                                   int space_between_items) {
  box_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, indent, 0,
                                     space_between_items);
  SetLayoutManager(box_layout_);
  DoAddIconAndLabel(image, icon_size, text, highlight);
}

void HoverHighlightView::DoAddIconAndLabel(const gfx::ImageSkia& image,
                                           int icon_size,
                                           const base::string16& text,
                                           bool highlight) {
  DCHECK(box_layout_);

  views::ImageView* image_view = new FixedSizedImageView(icon_size, 0);
  image_view->SetImage(image);
  image_view->SetEnabled(enabled());
  AddChildView(image_view);

  text_label_ = new views::Label(text);
  text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label_->SetFontList(GetFontList(highlight));
  if (text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  text_label_->SetEnabled(enabled());
  AddChildView(text_label_);
  box_layout_->SetFlexForView(text_label_, 1);

  SetAccessibleName(text);
}

views::Label* HoverHighlightView::AddLabel(const base::string16& text,
                                           gfx::HorizontalAlignment alignment,
                                           bool highlight) {
  box_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  SetLayoutManager(box_layout_);
  text_label_ = new views::Label(text);
  int left_margin = kTrayPopupPaddingHorizontal;
  int right_margin = kTrayPopupPaddingHorizontal;
  if (alignment != gfx::ALIGN_CENTER) {
    if (base::i18n::IsRTL())
      right_margin += kTrayPopupDetailsLabelExtraLeftMargin;
    else
      left_margin += kTrayPopupDetailsLabelExtraLeftMargin;
  }
  text_label_->SetBorder(
      views::Border::CreateEmptyBorder(5, left_margin, 5, right_margin));
  text_label_->SetHorizontalAlignment(alignment);
  text_label_->SetFontList(GetFontList(highlight));
  // Do not set alpha value in disable color. It will have issue with elide
  // blending filter in disabled state for rendering label text color.
  text_label_->SetDisabledColor(SkColorSetARGB(255, 127, 127, 127));
  if (text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  text_label_->SetEnabled(enabled());
  AddChildView(text_label_);
  box_layout_->SetFlexForView(text_label_, 1);

  SetAccessibleName(text);
  return text_label_;
}

views::Label* HoverHighlightView::AddCheckableLabel(const base::string16& text,
                                                    bool highlight,
                                                    bool checked) {
  checkable_ = true;
  checked_ = checked;
  if (checked) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia* check =
        rb.GetImageNamed(IDR_MENU_CHECK).ToImageSkia();
    int margin = kTrayPopupPaddingHorizontal +
                 kTrayPopupDetailsLabelExtraLeftMargin - kCheckLabelPadding;
    box_layout_ = new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 3,
                                       kCheckLabelPadding);
    SetLayoutManager(box_layout_);
    views::ImageView* image_view = new FixedSizedImageView(margin, 0);
    image_view->SetImage(check);
    image_view->SetHorizontalAlignment(views::ImageView::TRAILING);
    image_view->SetEnabled(enabled());
    AddChildView(image_view);

    text_label_ = new views::Label(text);
    text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    text_label_->SetFontList(GetFontList(highlight));
    text_label_->SetDisabledColor(SkColorSetARGB(127, 0, 0, 0));
    if (text_default_color_)
      text_label_->SetEnabledColor(text_default_color_);
    text_label_->SetEnabled(enabled());
    AddChildView(text_label_);

    SetAccessibleName(text);
    return text_label_;
  }
  return AddLabel(text, gfx::ALIGN_LEFT, highlight);
}

void HoverHighlightView::SetExpandable(bool expandable) {
  if (expandable != expandable_) {
    expandable_ = expandable;
    InvalidateLayout();
  }
}

void HoverHighlightView::SetHighlight(bool highlight) {
  DCHECK(text_label_);
  text_label_->SetFontList(GetFontList(highlight));
  text_label_->InvalidateLayout();
}

void HoverHighlightView::SetHoverHighlight(bool hover) {
  if (!enabled() && hover)
    return;
  if (hover_ == hover)
    return;
  hover_ = hover;
  if (!text_label_)
    return;
  if (hover_ && text_highlight_color_)
    text_label_->SetEnabledColor(text_highlight_color_);
  if (!hover_ && text_default_color_)
    text_label_->SetEnabledColor(text_default_color_);
  SchedulePaint();
}

bool HoverHighlightView::PerformAction(const ui::Event& event) {
  if (!listener_)
    return false;
  listener_->OnViewClicked(this);
  return true;
}

void HoverHighlightView::GetAccessibleState(ui::AXViewState* state) {
  ActionableView::GetAccessibleState(state);

  if (checkable_) {
    state->role = ui::AX_ROLE_CHECK_BOX;
    if (checked_)
      state->AddStateFlag(ui::AX_STATE_CHECKED);
  }
}

gfx::Size HoverHighlightView::GetPreferredSize() const {
  gfx::Size size = ActionableView::GetPreferredSize();
  int height = GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT);
  if (!expandable_ || size.height() < height)
    size.set_height(height);
  return size;
}

int HoverHighlightView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void HoverHighlightView::OnMouseEntered(const ui::MouseEvent& event) {
  SetHoverHighlight(true);
}

void HoverHighlightView::OnMouseExited(const ui::MouseEvent& event) {
  SetHoverHighlight(false);
}

void HoverHighlightView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    SetHoverHighlight(true);
  } else if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
             event->type() == ui::ET_GESTURE_TAP) {
    SetHoverHighlight(false);
  }
  ActionableView::OnGestureEvent(event);
}

void HoverHighlightView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetHoverHighlight(IsMouseHovered());
}

void HoverHighlightView::OnEnabledChanged() {
  if (!enabled())
    SetHoverHighlight(false);
  for (int i = 0; i < child_count(); ++i)
    child_at(i)->SetEnabled(enabled());
}

void HoverHighlightView::OnPaintBackground(gfx::Canvas* canvas) {
  canvas->DrawColor(hover_ ? highlight_color_ : default_color_);
}

void HoverHighlightView::OnFocus() {
  ScrollRectToVisible(gfx::Rect(gfx::Point(), size()));
  ActionableView::OnFocus();
}

}  // namespace ash
