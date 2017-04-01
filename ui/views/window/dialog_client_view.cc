// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/dialog_client_view.h"

#include <algorithm>

#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

namespace {

// The group used by the buttons.  This name is chosen voluntarily big not to
// conflict with other groups that could be in the dialog content.
const int kButtonGroup = 6666;

#if defined(OS_WIN) || defined(OS_CHROMEOS)
const bool kIsOkButtonOnLeftSide = true;
#else
const bool kIsOkButtonOnLeftSide = false;
#endif

// Returns true if the given view should be shown (i.e. exists and is
// visible).
bool ShouldShow(View* view) {
  return view && view->visible();
}

// Do the layout for a button.
void LayoutButton(LabelButton* button,
                  gfx::Rect* row_bounds,
                  int button_height) {
  if (!button)
    return;

  const gfx::Size size = button->GetPreferredSize();
  row_bounds->set_width(row_bounds->width() - size.width());
  DCHECK_LE(button_height, row_bounds->height());
  button->SetBounds(
      row_bounds->right(),
      row_bounds->y() + (row_bounds->height() - button_height) / 2,
      size.width(), button_height);
  int spacing = ViewsDelegate::GetInstance()
                    ? ViewsDelegate::GetInstance()
                          ->GetDialogRelatedButtonHorizontalSpacing()
                    : kRelatedButtonHSpacing;
  row_bounds->set_width(row_bounds->width() - spacing);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, public:

DialogClientView::DialogClientView(Widget* owner, View* contents_view)
    : ClientView(owner, contents_view),
      ok_button_(nullptr),
      cancel_button_(nullptr),
      extra_view_(nullptr),
      delegate_allowed_close_(false) {
  button_row_insets_ =
      ViewsDelegate::GetInstance()
          ? ViewsDelegate::GetInstance()->GetDialogButtonInsets()
          : gfx::Insets(0, kButtonHEdgeMarginNew, kButtonVEdgeMarginNew,
                        kButtonHEdgeMarginNew);
  // Doing this now ensures this accelerator will have lower priority than
  // one set by the contents view.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  if (ViewsDelegate::GetInstance())
    button_row_insets_ = ViewsDelegate::GetInstance()->GetDialogButtonInsets();
}

DialogClientView::~DialogClientView() {
}

void DialogClientView::AcceptWindow() {
  // Only notify the delegate once. See |delegate_allowed_close_|'s comment.
  if (!delegate_allowed_close_ && GetDialogDelegate()->Accept()) {
    delegate_allowed_close_ = true;
    GetWidget()->Close();
  }
}

void DialogClientView::CancelWindow() {
  // Only notify the delegate once. See |delegate_allowed_close_|'s comment.
  if (!delegate_allowed_close_ && GetDialogDelegate()->Cancel()) {
    delegate_allowed_close_ = true;
    GetWidget()->Close();
  }
}

void DialogClientView::UpdateDialogButtons() {
  const int buttons = GetDialogDelegate()->GetDialogButtons();

  if (buttons & ui::DIALOG_BUTTON_OK) {
    if (!ok_button_) {
      ok_button_ = CreateDialogButton(ui::DIALOG_BUTTON_OK);
      AddChildView(ok_button_);
    }

    GetDialogDelegate()->UpdateButton(ok_button_, ui::DIALOG_BUTTON_OK);
  } else if (ok_button_) {
    delete ok_button_;
    ok_button_ = nullptr;
  }

  if (buttons & ui::DIALOG_BUTTON_CANCEL) {
    if (!cancel_button_) {
      cancel_button_ = CreateDialogButton(ui::DIALOG_BUTTON_CANCEL);
      AddChildView(cancel_button_);
    }

    GetDialogDelegate()->UpdateButton(cancel_button_, ui::DIALOG_BUTTON_CANCEL);
  } else if (cancel_button_) {
    delete cancel_button_;
    cancel_button_ = nullptr;
  }

  SetupFocusChain();
}

///////////////////////////////////////////////////////////////////////////////
// DialogClientView, ClientView overrides:

bool DialogClientView::CanClose() {
  // If the dialog is closing but no Accept or Cancel action has been performed
  // before, it's a Close action.
  if (!delegate_allowed_close_)
    delegate_allowed_close_ = GetDialogDelegate()->Close();
  return delegate_allowed_close_;
}

DialogClientView* DialogClientView::AsDialogClientView() {
  return this;
}

const DialogClientView* DialogClientView::AsDialogClientView() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, View overrides:

gfx::Size DialogClientView::GetPreferredSize() const {
  // Initialize the size to fit the buttons and extra view row.
  int extra_view_padding = 0;
  if (!GetDialogDelegate()->GetExtraViewPadding(&extra_view_padding))
    extra_view_padding = ViewsDelegate::GetInstance()
                             ? ViewsDelegate::GetInstance()
                                   ->GetDialogRelatedButtonHorizontalSpacing()
                             : kRelatedButtonHSpacing;
  gfx::Size size(
      (ok_button_ ? ok_button_->GetPreferredSize().width() : 0) +
          (cancel_button_ ? cancel_button_->GetPreferredSize().width() : 0) +
          (cancel_button_ && ok_button_
           ? (ViewsDelegate::GetInstance()
                  ? ViewsDelegate::GetInstance()
                        ->GetDialogRelatedButtonHorizontalSpacing()
                  : kRelatedButtonHSpacing) : 0) +
      (ShouldShow(extra_view_) ? extra_view_->GetPreferredSize().width() : 0) +
      (ShouldShow(extra_view_) && has_dialog_buttons() ? extra_view_padding
                                                       : 0),
      0);

  int buttons_height = GetButtonsAndExtraViewRowHeight();
  if (buttons_height != 0) {
    size.Enlarge(0, buttons_height + GetButtonsAndExtraViewRowTopPadding());
    // Inset the buttons and extra view.
    const gfx::Insets insets = GetButtonRowInsets();
    size.Enlarge(insets.width(), insets.height());
  }

  // Increase the size as needed to fit the contents view.
  // NOTE: The contents view is not inset on the top or side client view edges.
  gfx::Size contents_size = contents_view()->GetPreferredSize();
  size.Enlarge(0, contents_size.height());
  size.set_width(std::max(size.width(), contents_size.width()));

  return size;
}

void DialogClientView::Layout() {
  gfx::Rect bounds = GetContentsBounds();

  // Layout the row containing the buttons and the extra view.
  if (has_dialog_buttons() || ShouldShow(extra_view_)) {
    bounds.Inset(GetButtonRowInsets());
    const int height = GetButtonsAndExtraViewRowHeight();
    gfx::Rect row_bounds(bounds.x(), bounds.bottom() - height,
                         bounds.width(), height);
    // If the |extra_view_| is a also button, then the |button_height| is the
    // maximum height of the three buttons, otherwise it is the maximum height
    // of the ok and cancel buttons.
    const int button_height =
        CustomButton::AsCustomButton(extra_view_) ? height : GetButtonHeight();
    if (kIsOkButtonOnLeftSide) {
      LayoutButton(cancel_button_, &row_bounds, button_height);
      LayoutButton(ok_button_, &row_bounds, button_height);
    } else {
      LayoutButton(ok_button_, &row_bounds, button_height);
      LayoutButton(cancel_button_, &row_bounds, button_height);
    }
    if (extra_view_) {
      int custom_padding = 0;
      if (has_dialog_buttons() &&
          GetDialogDelegate()->GetExtraViewPadding(&custom_padding)) {
        // The call to LayoutButton() will already have accounted for some of
        // the padding.
        custom_padding -= GetButtonsAndExtraViewRowTopPadding();
        row_bounds.set_width(row_bounds.width() - custom_padding);
      }
      row_bounds.set_width(std::min(row_bounds.width(),
          extra_view_->GetPreferredSize().width()));
      extra_view_->SetBoundsRect(row_bounds);
    }

    if (height > 0)
      bounds.Inset(0, 0, 0, height + GetButtonsAndExtraViewRowTopPadding());
  }

  // Layout the contents view to the top and side edges of the contents bounds.
  // NOTE: The local insets do not apply to the contents view sides or top.
  const gfx::Rect contents_bounds = GetContentsBounds();
  contents_view()->SetBounds(contents_bounds.x(), contents_bounds.y(),
      contents_bounds.width(), bounds.bottom() - contents_bounds.y());
}

bool DialogClientView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);

  GetWidget()->Close();
  return true;
}

void DialogClientView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  ClientView::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this) {
    UpdateDialogButtons();
    CreateExtraView();
  } else if (!details.is_add && details.child != this) {
    if (details.child == ok_button_)
      ok_button_ = nullptr;
    else if (details.child == cancel_button_)
      cancel_button_ = nullptr;
    else if (details.child == extra_view_)
      extra_view_ = nullptr;
  }
}

void DialogClientView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  // The old dialog style needs an explicit background color, while the new
  // dialog style simply inherits the bubble's frame view color.
  const DialogDelegate* dialog = GetDialogDelegate();

  if (dialog && !dialog->ShouldUseCustomFrame()) {
    set_background(views::Background::CreateSolidBackground(GetNativeTheme()->
        GetSystemColor(ui::NativeTheme::kColorId_DialogBackground)));
  }
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, ButtonListener implementation:

void DialogClientView::ButtonPressed(Button* sender, const ui::Event& event) {
  // Check for a valid delegate to avoid handling events after destruction.
  if (!GetDialogDelegate())
    return;

  if (sender == ok_button_)
    AcceptWindow();
  else if (sender == cancel_button_)
    CancelWindow();
  else
    NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, protected:

DialogClientView::DialogClientView(View* contents_view)
    : ClientView(nullptr, contents_view),
      ok_button_(nullptr),
      cancel_button_(nullptr),
      extra_view_(nullptr),
      delegate_allowed_close_(false) {}

DialogDelegate* DialogClientView::GetDialogDelegate() const {
  return GetWidget()->widget_delegate()->AsDialogDelegate();
}

void DialogClientView::CreateExtraView() {
  if (extra_view_)
    return;

  extra_view_ = GetDialogDelegate()->CreateExtraView();
  if (extra_view_) {
    extra_view_->SetGroup(kButtonGroup);
    AddChildView(extra_view_);
    SetupFocusChain();
  }
}

void DialogClientView::ChildPreferredSizeChanged(View* child) {
  if (child == extra_view_)
    Layout();
}

void DialogClientView::ChildVisibilityChanged(View* child) {
  ChildPreferredSizeChanged(child);
}

////////////////////////////////////////////////////////////////////////////////
// DialogClientView, private:

LabelButton* DialogClientView::CreateDialogButton(ui::DialogButton type) {
  const base::string16 title = GetDialogDelegate()->GetDialogButtonLabel(type);
  LabelButton* button = nullptr;

  const bool is_default =
      GetDialogDelegate()->GetDefaultDialogButton() == type &&
      (type != ui::DIALOG_BUTTON_CANCEL ||
       PlatformStyle::kDialogDefaultButtonCanBeCancel);

  // The default button is always blue in Harmony.
  if (is_default && (ui::MaterialDesignController::IsSecondaryUiMaterial() ||
                     GetDialogDelegate()->ShouldDefaultButtonBeBlue())) {
    button = MdTextButton::CreateSecondaryUiBlueButton(this, title);
  } else {
    button = MdTextButton::CreateSecondaryUiButton(this, title);
  }

  const int kDialogMinButtonWidth = 75;
  button->SetMinSize(gfx::Size(kDialogMinButtonWidth, 0));
  button->SetGroup(kButtonGroup);
  return button;
}

int DialogClientView::GetButtonHeight() const {
  return std::max(
      ok_button_ ? ok_button_->GetPreferredSize().height() : 0,
      cancel_button_ ? cancel_button_->GetPreferredSize().height() : 0);
}

int DialogClientView::GetExtraViewHeight() const {
  return ShouldShow(extra_view_) ? extra_view_->GetPreferredSize().height() : 0;
}

int DialogClientView::GetButtonsAndExtraViewRowHeight() const {
  return std::max(GetExtraViewHeight(), GetButtonHeight());
}

gfx::Insets DialogClientView::GetButtonRowInsets() const {
  return GetButtonsAndExtraViewRowHeight() == 0 ? gfx::Insets()
                                                : button_row_insets_;
}

int DialogClientView::GetButtonsAndExtraViewRowTopPadding() const {
  int spacing = button_row_insets_.top();
  // Some subclasses of DialogClientView, in order to do their own layout, set
  // button_row_insets_ to gfx::Insets(). To avoid breaking behavior of those
  // dialogs, supplying 0 for the top inset of the row falls back to
  // ViewsDelegate::GetRelatedControlVerticalSpacing or
  // kRelatedControlVerticalSpacing.
  if (!spacing)
    spacing = ViewsDelegate::GetInstance()
                  ? ViewsDelegate::GetInstance()
                        ->GetDialogRelatedControlVerticalSpacing()
                  : kRelatedControlVerticalSpacing;
  return spacing;
}

void DialogClientView::SetupFocusChain() {
  // Create a vector of child views in the order of intended focus.
  std::vector<View*> child_views;
  child_views.push_back(contents_view());
  child_views.push_back(extra_view_);
  if (kIsOkButtonOnLeftSide) {
    child_views.push_back(ok_button_);
    child_views.push_back(cancel_button_);
  } else {
    child_views.push_back(cancel_button_);
    child_views.push_back(ok_button_);
  }

  // Remove all null views from the vector.
  child_views.erase(
      std::remove(child_views.begin(), child_views.end(), nullptr),
      child_views.end());

  // Setup focus by reordering views. It is not safe to use SetNextFocusableView
  // since child views may be added externally to this view.
  for (size_t i = 0; i < child_views.size(); i++)
    ReorderChildView(child_views[i], i);
}

}  // namespace views
