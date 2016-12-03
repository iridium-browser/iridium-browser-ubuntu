// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_window.h"

#import "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_constants.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_controller.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSColor+Luminance.h"
#include "ui/base/material_design/material_design_controller.h"

using bookmarks::kBookmarkBarMenuCornerRadius;

namespace {

// Material Design bookmark folder window background white.
const CGFloat kMDFolderWindowBackgroundColor = 237. / 255.;

}  // namespace

@implementation BookmarkBarFolderWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:NSBorderlessWindowMask // override
                                 backing:bufferingType
                                   defer:deferCreation])) {
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];
  }
  return self;
}

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (BOOL)canBecomeMainWindow {
  return NO;
}

// Override of keyDown as the NSWindow default implementation beeps.
- (void)keyDown:(NSEvent *)theEvent {
}

@end


@implementation BookmarkBarFolderWindowContentView

+ (NSColor*)backgroundColor {
  DCHECK(ui::MaterialDesignController::IsModeMaterial());
  static NSColor* backgroundColor =
      [[NSColor colorWithGenericGamma22White:kMDFolderWindowBackgroundColor
                                       alpha:1.0] retain];
  return backgroundColor;
}

- (void)drawRect:(NSRect)rect {
  // Like NSMenus, only the bottom corners are rounded.
  NSBezierPath* bezier =
      [NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                      xRadius:kBookmarkBarMenuCornerRadius
                                      yRadius:kBookmarkBarMenuCornerRadius];
  if (ui::MaterialDesignController::IsModeMaterial()) {
    [[BookmarkBarFolderWindowContentView backgroundColor] set];
    [bezier fill];
  } else {
    NSColor* startColor = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
    NSColor* midColor =
        [startColor gtm_colorAdjustedFor:GTMColorationLightMidtone faded:YES];
    NSColor* endColor =
        [startColor gtm_colorAdjustedFor:GTMColorationLightPenumbra faded:YES];

    base::scoped_nsobject<NSGradient> gradient(
        [[NSGradient alloc] initWithColorsAndLocations:startColor, 0.0,
                                                       midColor, 0.25,
                                                       endColor, 0.5,
                                                       midColor, 0.75,
                                                       startColor, 1.0,
                                                       nil]);
    [gradient drawInBezierPath:bezier angle:0.0];
  }
}

@end


@implementation BookmarkBarFolderWindowScrollView

// We want "draw background" of the NSScrollView in the xib to be NOT
// checked.  That allows us to round the bottom corners of the folder
// window.  However that also allows some scrollWheel: events to leak
// into the NSWindow behind it (even in a different application).
// Better to plug the scroll leak than to round corners for M5.
- (void)scrollWheel:(NSEvent *)theEvent {
  DCHECK([[[self window] windowController]
           respondsToSelector:@selector(scrollWheel:)]);
  [[[self window] windowController] scrollWheel:theEvent];
}

@end
