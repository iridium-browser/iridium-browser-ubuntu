// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/view_id_util.h"

#import <Cocoa/Cocoa.h>

#include <map>
#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"

namespace {

// TODO(suzhe): After migrating to Mac OS X 10.6, we may use Objective-C's new
// "Associative References" feature to attach the ViewID to the view directly
// rather than using a separated map.
typedef std::map<NSView*, ViewID> ViewIDMap;

static base::LazyInstance<ViewIDMap> g_view_id_map = LAZY_INSTANCE_INITIALIZER;

// Returns the view's nearest descendant (including itself) with a specific
// ViewID, or nil if no subview has that ViewID.
NSView* FindViewWithID(NSView* view, ViewID viewID) {
  if ([view viewID] == viewID)
    return view;

  for (NSView* subview in [view subviews]) {
    NSView* result = FindViewWithID(subview, viewID);
    if (result != nil)
      return result;
  }
  return nil;
}

}  // anonymous namespace

namespace view_id_util {

void SetID(NSView* view, ViewID viewID) {
  DCHECK(view);
  DCHECK(viewID != VIEW_ID_NONE);
  // We handle VIEW_ID_TAB_0 to VIEW_ID_TAB_LAST in GetView() function directly.
  DCHECK(!((viewID >= VIEW_ID_TAB_0) && (viewID <= VIEW_ID_TAB_LAST)));
  g_view_id_map.Get()[view] = viewID;
}

void UnsetID(NSView* view) {
  DCHECK(view);
  g_view_id_map.Get().erase(view);
}

NSView* GetView(NSWindow* window, ViewID viewID) {
  DCHECK(viewID != VIEW_ID_NONE);
  DCHECK(window);

  // As tabs can be created, destroyed or rearranged dynamically, we handle them
  // here specially.
  if (viewID >= VIEW_ID_TAB_0 && viewID <= VIEW_ID_TAB_LAST) {
    BrowserWindowController* windowController = [window windowController];
    DCHECK([windowController isKindOfClass:[BrowserWindowController class]]);
    TabStripController* tabStripController =
        [windowController tabStripController];
    DCHECK(tabStripController);
    NSUInteger count = [tabStripController viewsCount];
    DCHECK(count);
    NSUInteger index =
        (viewID == VIEW_ID_TAB_LAST ? count - 1 : viewID - VIEW_ID_TAB_0);
    return index < count ? [tabStripController viewAtIndex:index] : nil;
  }

  return FindViewWithID([[window contentView] superview], viewID);
}

}  // namespace view_id_util

@implementation NSView (ViewID)

- (ViewID)viewID {
  ViewIDMap* map = g_view_id_map.Pointer();
  ViewIDMap::const_iterator iter = map->find(self);
  return iter != map->end() ? iter->second : VIEW_ID_NONE;
}

- (NSView*)ancestorWithViewID:(ViewID)viewID {
  NSView* ancestor = self;
  while (ancestor && [ancestor viewID] != viewID)
    ancestor = [ancestor superview];
  return ancestor;
}

@end
