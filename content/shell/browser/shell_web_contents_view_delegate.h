// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/context_menu_params.h"

namespace content {

class ShellWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  explicit ShellWebContentsViewDelegate(WebContents* web_contents);
  ~ShellWebContentsViewDelegate() override;

  // Overridden from WebContentsViewDelegate:
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;

#if defined(OS_MACOSX)
  void ActionPerformed(int id);
#elif defined(OS_WIN)
  void MenuItemSelected(int selection);
#endif

 private:
  WebContents* web_contents_;
  ContextMenuParams params_;

  DISALLOW_COPY_AND_ASSIGN(ShellWebContentsViewDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_WEB_CONTENTS_VIEW_DELEGATE_H_
