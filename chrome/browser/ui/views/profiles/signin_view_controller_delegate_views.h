// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/profile_chooser_constants.h"
#include "chrome/browser/ui/signin_view_controller_delegate.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class Profile;

namespace content {
class WebContentsDelegate;
}

namespace signin_metrics {
enum class AccessPoint;
}

namespace views {
class WebView;
}

// Views implementation of SigninViewControllerDelegate. It's responsible for
// managing the Signin and Sync Confirmation tab-modal dialogs.
// Instances of this class delete themselves when the window they're managing
// closes (in the DeleteDelegate callback).
class SigninViewControllerDelegateViews : public views::DialogDelegateView,
                                          public SigninViewControllerDelegate {
 public:
  // Creates and displays a constrained window containing |web_contents|. If
  // |wait_for_size| is true, the delegate will wait for ResizeNativeView() to
  // be called by the base class before displaying the constrained window.
  SigninViewControllerDelegateViews(
      SigninViewController* signin_view_controller,
      views::WebView* content_view,
      Browser* browser,
      bool wait_for_size);

  // Creates the web view that contains the signin flow in |mode| using
  // |profile| as the web content's profile, then sets |delegate| as the created
  // web content's delegate.
  static views::WebView* CreateGaiaWebView(
      content::WebContentsDelegate* delegate,
      profiles::BubbleViewMode mode,
      Browser* browser,
      signin_metrics::AccessPoint access_point);

  static views::WebView* CreateSyncConfirmationWebView(Browser* browser);

  // views::DialogDelegateView:
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void DeleteDelegate() override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;
  int GetDialogButtons() const override;

 private:
  void PerformClose() override;
  void ResizeNativeView(int height) override;

  void DisplayModal();

  ~SigninViewControllerDelegateViews() override;

  views::WebView* content_view_;
  views::Widget* modal_signin_widget_;  // Not owned.

  // wait_for_size_ stores whether the dialog should only be shown after its
  // content's size has been laid out and measured so that the constrained
  // window is sized to the content.
  bool wait_for_size_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerDelegateViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_SIGNIN_VIEW_CONTROLLER_DELEGATE_VIEWS_H_
