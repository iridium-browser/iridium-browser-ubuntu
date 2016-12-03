// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"

#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_BOOKMARK_MENU
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/browser/user_metrics.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/menu_controller.h"

using base::UserMetricsAction;
using bookmarks::BookmarkNode;
using content::OpenURLParams;
using content::Referrer;

namespace {

// Menus more than this many pixels wide will get trimmed
// TODO(jrg): ask UI dudes what a good value is.
const NSUInteger kMaximumMenuPixelsWide = 300;

}

@implementation BookmarkMenuCocoaController

+ (NSString*)menuTitleForNode:(const BookmarkNode*)node {
  base::string16 title = [MenuController elideMenuTitle:node->GetTitle()
                                          toWidth:kMaximumMenuPixelsWide];
  return base::SysUTF16ToNSString(title);
}

+ (NSString*)tooltipForNode:(const BookmarkNode*)node {
  NSString* title = base::SysUTF16ToNSString(node->GetTitle());
  if (node->is_folder())
    return title;
  std::string urlString = node->url().possibly_invalid_spec();
  NSString* url = base::SysUTF8ToNSString(urlString);
  return cocoa_l10n_util::TooltipForURLAndTitle(url, title);
}

- (id)initWithBridge:(BookmarkMenuBridge*)bridge
             andMenu:(NSMenu*)menu {
  if ((self = [super init])) {
    bridge_ = bridge;
    DCHECK(bridge_);
    menu_.reset([menu retain]);
    [[self menu] setDelegate:self];
  }
  return self;
}

- (void)dealloc {
  if ([[self menu] delegate] == self)
    [[self menu] setDelegate:nil];
  [super dealloc];
}

- (NSMenu*)menu {
  return menu_;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  AppController* controller =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  return ![controller keyWindowIsModal];
}

// NSMenu delegate method: called just before menu is displayed.
- (void)menuNeedsUpdate:(NSMenu*)menu {
  bridge_->UpdateMenu(menu);
}

// Return the a BookmarkNode that has the given id (called
// "identifier" here to avoid conflict with objc's concept of "id").
- (const BookmarkNode*)nodeForIdentifier:(int)identifier {
  return bookmarks::GetBookmarkNodeByID(bridge_->GetBookmarkModel(),
                                        identifier);
}

// Open the URL of the given BookmarkNode in the current tab.
- (void)openURLForNode:(const BookmarkNode*)node {
  Browser* browser = chrome::FindTabbedBrowser(bridge_->GetProfile(), true);
  if (!browser) {
    browser = new Browser(Browser::CreateParams(bridge_->GetProfile()));
  }
  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  OpenURLParams params(
      node->url(), Referrer(), disposition,
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  browser->OpenURL(params);
}

// Open sites under BookmarkNode with the specified disposition.
- (void)openAll:(NSInteger)tag
    withDisposition:(WindowOpenDisposition)disposition {
  int identifier = tag;

  const BookmarkNode* node = [self nodeForIdentifier:identifier];
  DCHECK(node);

  Browser* browser = chrome::FindTabbedBrowser(bridge_->GetProfile(), true);
  if (!browser) {
    browser = new Browser(Browser::CreateParams(bridge_->GetProfile()));
  }
  DCHECK(browser);

  if (!node || !browser)
    return; // shouldn't be reached

  chrome::OpenAll(NULL, browser, node, disposition, browser->profile());

  if (disposition == NEW_FOREGROUND_TAB) {
    content::RecordAction(UserMetricsAction("OpenAllBookmarks"));
  } else if (disposition == NEW_WINDOW) {
    content::RecordAction(UserMetricsAction("OpenAllBookmarksNewWindow"));
  } else {
    content::RecordAction(
        UserMetricsAction("OpenAllBookmarksIncognitoWindow"));
  }
}

- (IBAction)openBookmarkMenuItem:(id)sender {
  NSInteger tag = [sender tag];
  int identifier = tag;
  const BookmarkNode* node = [self nodeForIdentifier:identifier];
  DCHECK(node);
  if (!node)
    return;  // shouldn't be reached

  [self openURLForNode:node];
}

- (IBAction)openAllBookmarks:(id)sender {
  [self openAll:[sender tag] withDisposition:NEW_FOREGROUND_TAB];
}

- (IBAction)openAllBookmarksNewWindow:(id)sender {
  [self openAll:[sender tag] withDisposition:NEW_WINDOW];
}

- (IBAction)openAllBookmarksIncognitoWindow:(id)sender {
  [self openAll:[sender tag] withDisposition:OFF_THE_RECORD];
}

@end  // BookmarkMenuCocoaController
