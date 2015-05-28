// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_view.h"

#include "base/i18n/rtl.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/autofill/form_suggestion_label.h"
#import "ios/chrome/browser/autofill/form_suggestion_view_client.h"

namespace {

// Vertical margin between suggestions and the edge of the suggestion content
// frame.
const CGFloat kSuggestionVerticalMargin = 4;

// Horizontal margin around suggestions (i.e. between suggestions, and between
// the end suggestions and the suggestion content frame).
const CGFloat kSuggestionHorizontalMargin = 2;

}  // namespace

@implementation FormSuggestionView {
  // The FormSuggestions that are displayed by this view.
  base::scoped_nsobject<NSArray> _suggestions;
}

- (instancetype)initWithFrame:(CGRect)frame
                       client:(id<FormSuggestionViewClient>)client
                  suggestions:(NSArray*)suggestions {
  self = [super initWithFrame:frame];
  if (self) {
    _suggestions.reset([suggestions copy]);

    self.showsVerticalScrollIndicator = NO;
    self.showsHorizontalScrollIndicator = NO;
    self.bounces = NO;
    self.canCancelContentTouches = YES;

    // Total height occupied by the label content, padding, border and margin.
    const CGFloat labelHeight =
        CGRectGetHeight(frame) - kSuggestionVerticalMargin * 2;

    BOOL isRTL = base::i18n::IsRTL();

    NSUInteger suggestionCount = [_suggestions count];
    // References to labels. These references are used to adjust the labels'
    // positions if they don't take up the whole suggestion view area for RTL.
    base::scoped_nsobject<NSMutableArray> labels(
        [[NSMutableArray alloc] initWithCapacity:suggestionCount]);
    __block CGFloat currentX = kSuggestionHorizontalMargin;
    void (^setupBlock)(FormSuggestion* suggestion, NSUInteger idx, BOOL* stop) =
        ^(FormSuggestion* suggestion, NSUInteger idx, BOOL* stop) {
          // FormSuggestionLabel will adjust the width, so here 0 is used for
          // the width.
          CGRect proposedFrame =
              CGRectMake(currentX, kSuggestionVerticalMargin, 0, labelHeight);
          base::scoped_nsobject<UIView> label(
              [[FormSuggestionLabel alloc] initWithSuggestion:suggestion
                                                proposedFrame:proposedFrame
                                                       client:client]);
          [self addSubview:label];
          [labels addObject:label];
          currentX +=
              CGRectGetWidth([label frame]) + kSuggestionHorizontalMargin;
        };
    [_suggestions enumerateObjectsWithOptions:(isRTL ? NSEnumerationReverse : 0)
                                   usingBlock:setupBlock];

    if (isRTL) {
      if (currentX < CGRectGetWidth(frame)) {
        self.contentSize = frame.size;
        // Offsets labels for right alignment.
        CGFloat offset = CGRectGetWidth(frame) - currentX;
        for (UIView* label in labels.get()) {
          label.frame = CGRectOffset(label.frame, offset, 0);
        }
      } else {
        self.contentSize = CGSizeMake(currentX, CGRectGetHeight(frame));
        // Sets the visible rectangle so suggestions at the right end are
        // initially visible.
        CGRect initRect = {{currentX - CGRectGetWidth(frame), 0}, frame.size};
        [self scrollRectToVisible:initRect animated:NO];
      }
    } else {
      self.contentSize = CGSizeMake(currentX, CGRectGetHeight(frame));
    }
  }
  return self;
}

- (NSArray*)suggestions {
  return _suggestions.get();
}

@end
