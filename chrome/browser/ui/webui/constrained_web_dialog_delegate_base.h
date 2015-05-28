// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace content {
class BrowserContext;
}

namespace ui {
class WebDialogDelegate;
}

// Platform-agnostic base implementation of ConstrainedWebDialogDelegate.
class ConstrainedWebDialogDelegateBase
    : public ConstrainedWebDialogDelegate,
      public ui::WebDialogWebContentsDelegate {
 public:
  ConstrainedWebDialogDelegateBase(content::BrowserContext* browser_context,
                                   ui::WebDialogDelegate* delegate,
                                   WebDialogWebContentsDelegate* tab_delegate);
  ~ConstrainedWebDialogDelegateBase() override;

  bool closed_via_webui() const;

  // ConstrainedWebDialogDelegate interface.
  const ui::WebDialogDelegate* GetWebDialogDelegate() const override;
  ui::WebDialogDelegate* GetWebDialogDelegate() override;
  void OnDialogCloseFromWebUI() override;
  void ReleaseWebContentsOnDialogClose() override;
  content::WebContents* GetWebContents() override;
  gfx::NativeWindow GetNativeDialog() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  gfx::Size GetPreferredSize() const override;

  // WebDialogWebContentsDelegate interface.
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

 private:
  scoped_ptr<ui::WebDialogDelegate> web_dialog_delegate_;

  // Holds the HTML to display in the constrained dialog.
  scoped_ptr<content::WebContents> web_contents_;

  // Was the dialog closed from WebUI (in which case |web_dialog_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;

  // If true, release |web_contents_| on close instead of destroying it.
  bool release_contents_on_close_;

  scoped_ptr<WebDialogWebContentsDelegate> override_tab_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWebDialogDelegateBase);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_DELEGATE_BASE_H_
