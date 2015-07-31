// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_never_save_view_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "grit/generated_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"

using namespace password_manager::mac::ui;

@interface ManagePasswordsBubbleNeverSaveViewController ()
- (void)onConfirmClicked:(id)sender;
- (void)onUndoClicked:(id)sender;
@end

@implementation ManagePasswordsBubbleNeverSaveViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleNeverSaveViewDelegate>)delegate {
  if (([super initWithDelegate:delegate])) {
    model_ = model;
  }
  return self;
}

- (void)onConfirmClicked:(id)sender {
  model_->OnNeverForThisSiteClicked();
  [delegate_ viewShouldDismiss];
}

- (void)onUndoClicked:(id)sender {
  SEL selector = @selector(neverSavePasswordCancelled);
  if ([delegate_ respondsToSelector:selector])
    [delegate_ performSelector:selector];
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // -----------------------------------
  // |  Title                          |
  // |                                 |
  // |  Confirmation!                  |
  // |                                 |
  // |                [Undo] [Confirm] |
  // -----------------------------------

  // Create the elements and add them to the view.

  // Title.
  NSTextField* titleLabel =
      [self addTitleLabel:l10n_util::GetNSString(
                              IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_TITLE)
                   toView:view];

  // Blacklist confirmation.
  NSTextField* confirmationLabel =
      [self addLabel:l10n_util::GetNSString(
                         IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_TEXT)
              toView:view];

  // Undo button.
  undoButton_.reset([[self addButton:l10n_util::GetNSString(IDS_CANCEL)
                              toView:view
                              target:self
                              action:@selector(onUndoClicked:)] retain]);

  // Confirm button.
  confirmButton_.reset(
      [[self addButton:l10n_util::GetNSString(
                           IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_BUTTON)
                toView:view
                target:self
                action:@selector(onConfirmClicked:)] retain]);

  // Compute the bubble width using the title and confirmation labels.
  // The explanation label can wrap to multiple lines.
  const CGFloat minContentWidth = kDesiredBubbleWidth - 2 * kFramePadding;
  const CGFloat contentWidth =
      std::max(NSWidth([titleLabel frame]), minContentWidth);
  NSSize confirmationSize = [confirmationLabel frame].size;
  confirmationSize.width = contentWidth;
  [confirmationLabel setFrameSize:confirmationSize];
  [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:confirmationLabel];
  const CGFloat width = kFramePadding + contentWidth + kFramePadding;

  // Layout the elements, starting at the bottom and moving up.

  // Buttons go on the bottom row and are right-aligned.
  // Start with [Confirm].
  CGFloat curX = width - kFramePadding - NSWidth([confirmButton_ frame]);
  CGFloat curY = kFramePadding;
  [confirmButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // [Undo] goes to the left of [Confirm].
  curX = NSMinX([confirmButton_ frame]) -
         kRelatedControlHorizontalPadding -
         NSWidth([undoButton_ frame]);
  [undoButton_ setFrameOrigin:NSMakePoint(curX, curY)];

  // Confirmation label goes on the next row and is shifted right.
  curX = kFramePadding;
  curY = NSMaxY([undoButton_ frame]) + kUnrelatedControlVerticalPadding;
  [confirmationLabel setFrameOrigin:NSMakePoint(curX, curY)];

  // Title goes at the top after some padding.
  curY = NSMaxY([confirmationLabel frame]) + kUnrelatedControlVerticalPadding;
  [titleLabel setFrameOrigin:NSMakePoint(curX, curY)];

  // Update the bubble size.
  const CGFloat height = NSMaxY([titleLabel frame]) + kFramePadding;
  [view setFrame:NSMakeRect(0, 0, width, height)];

  [self setView:view];
}

@end

@implementation ManagePasswordsBubbleNeverSaveViewController (Testing)

- (NSButton*)undoButton {
  return undoButton_.get();
}

- (NSButton*)confirmButton {
  return confirmButton_.get();
}

@end
