// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/defaults.h"

namespace browser_defaults {

const int kOmniboxFontPixelSize = 16;

#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
const bool kBrowserAliveWithNoWindows = true;
const bool kShowExitMenuItem = false;
#else
const bool kBrowserAliveWithNoWindows = false;
const bool kShowExitMenuItem = true;
#endif

#if defined(OS_CHROMEOS)
const bool kShowUpgradeMenuItem = false;
const bool kShowImportOnBookmarkBar = false;
const bool kAlwaysOpenIncognitoWindow = true;
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = false;
#else
const bool kShowUpgradeMenuItem = true;
const bool kShowImportOnBookmarkBar = true;
const bool kAlwaysOpenIncognitoWindow = false;
const bool kAlwaysCreateTabbedBrowserOnSessionRestore = true;
#endif

#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
const bool kShowHelpMenuItemIcon = true;
#else
const bool kShowHelpMenuItemIcon = false;
#endif
#endif

const bool kDownloadPageHasShowInFolder = true;
const bool kSizeTabButtonToTopOfTabStrip = false;

#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
const bool kSyncAutoStarts = true;
const bool kShowOtherBrowsersInAboutMemory = false;
#else
const bool kSyncAutoStarts = false;
const bool kShowOtherBrowsersInAboutMemory = true;
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
const bool kScrollEventChangesTab = true;
#else
const bool kScrollEventChangesTab = false;
#endif

const ui::ResourceBundle::FontStyle kAssociatedNetworkFontStyle =
    ui::ResourceBundle::BoldFont;

#if !defined(OS_ANDROID)
const bool kPasswordEchoEnabled = false;
#endif

bool bookmarks_enabled = true;

bool enable_help_app = true;

}  // namespace browser_defaults
