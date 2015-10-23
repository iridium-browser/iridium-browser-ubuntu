// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/chrome_guest_view_manager_delegate.h"

#include "chrome/browser/task_management/web_contents_tags.h"

namespace extensions {

ChromeGuestViewManagerDelegate::ChromeGuestViewManagerDelegate(
    content::BrowserContext* context)
    : ExtensionsGuestViewManagerDelegate(context) {
}

ChromeGuestViewManagerDelegate::~ChromeGuestViewManagerDelegate() {
}

void ChromeGuestViewManagerDelegate::AttachTaskManagerGuestTag(
    content::WebContents* guest_web_contents) const {
  task_management::WebContentsTags::CreateForGuestContents(guest_web_contents);
}

}  // namespace extensions
