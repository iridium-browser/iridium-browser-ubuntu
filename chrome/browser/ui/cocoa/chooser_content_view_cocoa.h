// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"

class ChooserContentViewController;
class ChooserController;
@class SpinnerView;

// A chooser content view class that user can select an option.
@interface ChooserContentViewCocoa : NSView {
 @private
  base::scoped_nsobject<NSTextField> titleView_;
  base::scoped_nsobject<NSScrollView> scrollView_;
  base::scoped_nsobject<NSTableColumn> tableColumn_;
  base::scoped_nsobject<NSTableView> tableView_;
  base::scoped_nsobject<SpinnerView> spinner_;
  base::scoped_nsobject<NSTextField> status_;
  base::scoped_nsobject<NSButton> rescanButton_;
  base::scoped_nsobject<NSButton> connectButton_;
  base::scoped_nsobject<NSButton> cancelButton_;
  base::scoped_nsobject<NSBox> separator_;
  base::scoped_nsobject<NSTextField> message_;
  base::scoped_nsobject<NSButton> helpButton_;
  std::unique_ptr<ChooserController> chooserController_;
  std::unique_ptr<ChooserContentViewController> chooserContentViewController_;

  CGFloat titleHeight_;
  CGFloat statusHeight_;
  CGFloat rescanButtonHeight_;
  CGFloat connectButtonWidth_;
  CGFloat connectButtonHeight_;
  CGFloat cancelButtonWidth_;
  CGFloat cancelButtonHeight_;
  CGFloat messageHeight_;

  struct FrameAndOrigin {
    NSRect scroll_view_frame;
    NSPoint connect_button_origin;
    NSPoint cancel_button_origin;
  };

  // The cached |scrollView_| frame and |connectButton_| and |cancelButton_|
  // origins for views layout:
  // When |status_| is shown.
  FrameAndOrigin statusShown_;
  // When |rescanButton_| is shown.
  FrameAndOrigin rescanButtonShown_;
  // When neither |status_| nor |rescanButton_| is shown.
  FrameAndOrigin noStatusOrRescanButtonShown_;

  // The cached |status_| and |rescanButton_| origins.
  NSPoint statusOrigin_;
  NSPoint rescanButtonOrigin_;
}

// Designated initializer.
- (instancetype)initWithChooserTitle:(NSString*)chooserTitle
                   chooserController:
                       (std::unique_ptr<ChooserController>)chooserController;

// Creates the title for the chooser.
- (base::scoped_nsobject<NSTextField>)createChooserTitle:(NSString*)title;

// Creates a table row view for the chooser.
- (base::scoped_nsobject<NSView>)createTableRowView:(NSInteger)rowIndex;

// The height of a table row view.
- (CGFloat)tableRowViewHeight:(NSInteger)row;

// Creates a button with |title|.
- (base::scoped_nsobject<NSButton>)createButtonWithTitle:(NSString*)title;

// Creates the "Connect" button.
- (base::scoped_nsobject<NSButton>)createConnectButton;

// Creates the "Cancel" button.
- (base::scoped_nsobject<NSButton>)createCancelButton;

// Creates the separator.
- (base::scoped_nsobject<NSBox>)createSeparator;

// Creates a text field with |text|.
- (base::scoped_nsobject<NSTextField>)createTextField:(NSString*)text;

// Creates a hyperlink button with |text|.
- (base::scoped_nsobject<NSButton>)createHyperlinkButtonWithText:
    (NSString*)text;

// Calculates the frame for the |scrollView_|.
- (NSRect)calculateScrollViewFrame:(CGFloat)buttonRowHeight;

// Calculates the origin for the |status_| text.
- (NSPoint)calculateStatusOrigin:(CGFloat)buttonRowHeight;

// Calculates the origin for the "Re-scan" button.
- (NSPoint)calculateRescanButtonOrigin:(CGFloat)buttonRowHeight;

// Calculates the origin for the "Connect" button.
- (NSPoint)calculateConnectButtonOrigin:(CGFloat)buttonRowHeight;

// Calculates the origin for the "Cancel" button.
- (NSPoint)calculateCancelButtonOrigin:(CGFloat)buttonRowHeight;

// Updates the origin and size of the view.
- (void)updateView;

// Gets the table view for the chooser.
- (NSTableView*)tableView;

// Gets the spinner.
- (SpinnerView*)spinner;

// Gets the status text field.
- (NSTextField*)status;

// Gets the "Re-scan" button.
- (NSButton*)rescanButton;

// Gets the "Connect" button.
- (NSButton*)connectButton;

// Gets the "Cancel" button.
- (NSButton*)cancelButton;

// Gets the "Get help" button.
- (NSButton*)helpButton;

// The number of options in the |tableView_|.
- (NSInteger)numberOfOptions;

// The |index|th option string which is listed in the chooser.
- (NSString*)optionAtIndex:(NSInteger)index;

// Update |tableView_| when chooser options changed.
- (void)updateTableView;

// Called when the "Connect" button is pressed.
- (void)accept;

// Called when the "Cancel" button is pressed.
- (void)cancel;

// Called when the chooser is closed.
- (void)close;

// Called when "Re-scan" button is pressed.
- (void)onRescan:(id)sender;

// Called when the "Get help" button is pressed.
- (void)onHelpPressed:(id)sender;

// Gets the image from table row view. For testing only.
- (NSImageView*)tableRowViewImage:(NSInteger)row;

// Gets the text from table row view. For testing only.
- (NSTextField*)tableRowViewText:(NSInteger)row;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CHOOSER_CONTENT_VIEW_COCOA_H_
