// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/media_router_action_platform_delegate_views.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

// static
scoped_ptr<MediaRouterActionPlatformDelegate>
MediaRouterActionPlatformDelegate::Create(Browser* browser) {
  return make_scoped_ptr(new MediaRouterActionPlatformDelegateViews(browser));
}

MediaRouterActionPlatformDelegateViews::MediaRouterActionPlatformDelegateViews(
    Browser* browser)
    : MediaRouterActionPlatformDelegate(),
      browser_(browser) {
  DCHECK(browser_);
}

MediaRouterActionPlatformDelegateViews::
    ~MediaRouterActionPlatformDelegateViews() {
}

void MediaRouterActionPlatformDelegateViews::CloseOverflowMenuIfOpen() {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser_)->toolbar();
  if (toolbar->IsWrenchMenuShowing())
    toolbar->CloseAppMenu();
}
