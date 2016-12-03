// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/native_window_tracker.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "extensions/common/extension.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

const int kRightColumnWidth = 210;
const int kIconSize = 64;

class ExtensionUninstallDialogDelegateView;

// Views implementation of the uninstall dialog.
class ExtensionUninstallDialogViews
    : public extensions::ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogViews(
      Profile* profile,
      gfx::NativeWindow parent,
      extensions::ExtensionUninstallDialog::Delegate* delegate);
  ~ExtensionUninstallDialogViews() override;

  // Called when the ExtensionUninstallDialogDelegate has been destroyed to make
  // sure we invalidate pointers.
  void DialogDelegateDestroyed() { view_ = NULL; }

  // Forwards the accept and cancels to the delegate.
  void DialogAccepted(bool handle_report_abuse);
  void DialogCanceled();

 private:
  void Show() override;

  ExtensionUninstallDialogDelegateView* view_;

  // The dialog's parent window.
  gfx::NativeWindow parent_;

  // Tracks whether |parent_| got destroyed.
  std::unique_ptr<NativeWindowTracker> parent_window_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogViews);
};

// The dialog's view, owned by the views framework.
class ExtensionUninstallDialogDelegateView : public views::DialogDelegateView {
 public:
  ExtensionUninstallDialogDelegateView(
      ExtensionUninstallDialogViews* dialog_view,
      bool triggered_by_extension,
      const gfx::ImageSkia* image);
  ~ExtensionUninstallDialogDelegateView() override;

  // Called when the ExtensionUninstallDialog has been destroyed to make sure
  // we invalidate pointers.
  void DialogDestroyed() { dialog_ = NULL; }

 private:
  // views::DialogDelegate:
  views::View* CreateExtraView() override;
  bool GetExtraViewPadding(int* padding) override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  int GetDefaultDialogButton() const override {
    // Default to accept when triggered via chrome://extensions page.
    return triggered_by_extension_ ?
        ui::DIALOG_BUTTON_CANCEL : ui::DIALOG_BUTTON_OK;
  }
  bool Accept() override;
  bool Cancel() override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_WINDOW; }
  base::string16 GetWindowTitle() const override;

  // views::View:
  gfx::Size GetPreferredSize() const override;

  void Layout() override;

  ExtensionUninstallDialogViews* dialog_;

  views::ImageView* icon_;
  views::Label* heading_;
  bool triggered_by_extension_;
  views::Checkbox* report_abuse_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstallDialogDelegateView);
};

ExtensionUninstallDialogViews::ExtensionUninstallDialogViews(
    Profile* profile,
    gfx::NativeWindow parent,
    extensions::ExtensionUninstallDialog::Delegate* delegate)
    : extensions::ExtensionUninstallDialog(profile, delegate),
      view_(NULL),
      parent_(parent) {
  if (parent_)
    parent_window_tracker_ = NativeWindowTracker::Create(parent_);
}

ExtensionUninstallDialogViews::~ExtensionUninstallDialogViews() {
  // Close the widget (the views framework will delete view_).
  if (view_) {
    view_->DialogDestroyed();
    view_->GetWidget()->CloseNow();
  }
}

void ExtensionUninstallDialogViews::Show() {
  if (parent_ && parent_window_tracker_->WasNativeWindowClosed()) {
    OnDialogClosed(CLOSE_ACTION_CANCELED);
    return;
  }

  view_ = new ExtensionUninstallDialogDelegateView(
      this, triggering_extension() != nullptr, &icon());
  constrained_window::CreateBrowserModalDialogViews(view_, parent_)->Show();
}

void ExtensionUninstallDialogViews::DialogAccepted(bool report_abuse_checked) {
  // The widget gets destroyed when the dialog is accepted.
  view_->DialogDestroyed();
  view_ = nullptr;
  OnDialogClosed(report_abuse_checked ?
      CLOSE_ACTION_UNINSTALL_AND_REPORT_ABUSE : CLOSE_ACTION_UNINSTALL);
}

void ExtensionUninstallDialogViews::DialogCanceled() {
  // The widget gets destroyed when the dialog is canceled.
  view_->DialogDestroyed();
  view_ = nullptr;
  OnDialogClosed(CLOSE_ACTION_CANCELED);
}

ExtensionUninstallDialogDelegateView::ExtensionUninstallDialogDelegateView(
    ExtensionUninstallDialogViews* dialog_view,
    bool triggered_by_extension,
    const gfx::ImageSkia* image)
    : dialog_(dialog_view),
      triggered_by_extension_(triggered_by_extension),
      report_abuse_checkbox_(nullptr) {
  // Scale down to icon size, but allow smaller icons (don't scale up).
  gfx::Size size(image->width(), image->height());
  if (size.width() > kIconSize || size.height() > kIconSize)
    size = gfx::Size(kIconSize, kIconSize);
  icon_ = new views::ImageView();
  icon_->SetImageSize(size);
  icon_->SetImage(*image);
  AddChildView(icon_);

  heading_ = new views::Label(base::UTF8ToUTF16(dialog_->GetHeadingText()));
  heading_->SetMultiLine(true);
  heading_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  heading_->SetAllowCharacterBreak(true);
  AddChildView(heading_);
}

ExtensionUninstallDialogDelegateView::~ExtensionUninstallDialogDelegateView() {
  // If we're here, 2 things could have happened. Either the user closed the
  // dialog nicely and one of the installed/canceled methods has been called
  // (in which case dialog_ will be null), *or* neither of them have been
  // called and we are being forced closed by our parent widget. In this case,
  // we need to make sure to notify dialog_ not to call us again, since we're
  // about to be freed by the Widget framework.
  if (dialog_)
    dialog_->DialogDelegateDestroyed();
}

views::View* ExtensionUninstallDialogDelegateView::CreateExtraView() {
  if (dialog_->ShouldShowReportAbuseCheckbox()) {
    report_abuse_checkbox_ = new views::Checkbox(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_REPORT_ABUSE));
  }
  return report_abuse_checkbox_;
}

bool ExtensionUninstallDialogDelegateView::GetExtraViewPadding(int* padding) {
  // We want a little more padding between the "report abuse" checkbox and the
  // buttons.
  *padding = views::kUnrelatedControlLargeHorizontalSpacing;
  return true;
}

base::string16 ExtensionUninstallDialogDelegateView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16((button == ui::DIALOG_BUTTON_OK) ?
      IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON : IDS_CANCEL);
}

bool ExtensionUninstallDialogDelegateView::Accept() {
  if (dialog_) {
    dialog_->DialogAccepted(report_abuse_checkbox_ &&
                            report_abuse_checkbox_->checked());
  }
  return true;
}

bool ExtensionUninstallDialogDelegateView::Cancel() {
  if (dialog_)
    dialog_->DialogCanceled();
  return true;
}

base::string16 ExtensionUninstallDialogDelegateView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE);
}

gfx::Size ExtensionUninstallDialogDelegateView::GetPreferredSize() const {
  int width = kRightColumnWidth;
  width += kIconSize;
  width += views::kButtonHEdgeMarginNew * 2;
  width += views::kRelatedControlHorizontalSpacing;

  int height = views::kPanelVertMargin * 2;
  height += heading_->GetHeightForWidth(kRightColumnWidth);

  return gfx::Size(width,
                   std::max(height, kIconSize + views::kPanelVertMargin * 2));
}

void ExtensionUninstallDialogDelegateView::Layout() {
  int x = views::kButtonHEdgeMarginNew;
  int y = views::kPanelVertMargin;

  heading_->SizeToFit(kRightColumnWidth);

  if (heading_->height() <= kIconSize) {
    icon_->SetBounds(x, y, kIconSize, kIconSize);
    x += kIconSize;
    x += views::kRelatedControlHorizontalSpacing;

    heading_->SetX(x);
    heading_->SetY(y + (kIconSize - heading_->height()) / 2);
  } else {
    icon_->SetBounds(x,
                     y + (heading_->height() - kIconSize) / 2,
                     kIconSize,
                     kIconSize);
    x += kIconSize;
    x += views::kRelatedControlHorizontalSpacing;

    heading_->SetX(x);
    heading_->SetY(y);
  }
}

}  // namespace

// static
extensions::ExtensionUninstallDialog*
extensions::ExtensionUninstallDialog::Create(Profile* profile,
                                             gfx::NativeWindow parent,
                                             Delegate* delegate) {
  return new ExtensionUninstallDialogViews(profile, parent, delegate);
}
