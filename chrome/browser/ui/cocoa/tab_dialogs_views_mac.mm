// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_dialogs_views_mac.h"

#include "chrome/browser/ui/views/collected_cookies_views.h"

TabDialogsViewsMac::TabDialogsViewsMac(content::WebContents* contents)
    : TabDialogsCocoa(contents) {}

TabDialogsViewsMac::~TabDialogsViewsMac() {}

void TabDialogsViewsMac::ShowCollectedCookies() {
  // Deletes itself on close.
  new CollectedCookiesViews(web_contents());
}
