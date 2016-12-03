// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_container_view.h"

#include "base/logging.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/public/provider/chrome/browser/ui/infobar_view_protocol.h"
#include "ui/base/device_form_factor.h"

@implementation InfoBarContainerView

- (void)addInfoBar:(InfoBarIOS*)infoBarIOS position:(NSInteger)position {
  DCHECK_LE((NSUInteger)position, [[self subviews] count]);
  CGRect containerBounds = [self bounds];
  infoBarIOS->Layout(containerBounds);
  UIView* view = infoBarIOS->view();
  [self insertSubview:view atIndex:position];
}

- (CGSize)sizeThatFits:(CGSize)size {
  CGFloat height = 0;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    for (UIView* view in self.subviews) {
      CGSize elementSize = [view sizeThatFits:size];
      height += elementSize.height;
    }
  } else {
    for (UIView* view in self.subviews) {
      CGFloat elementHeight = [view sizeThatFits:size].height;
      if (elementHeight > height)
        height = elementHeight;
    }
  }
  size.height = height;
  return size;
}

- (void)layoutSubviews {
  for (UIView<InfoBarViewProtocol>* view in self.subviews) {
    [view sizeToFit];
    CGRect frame = view.frame;
    frame.origin.y = CGRectGetHeight(frame) - [view visibleHeight];
    [view setFrame:frame];
  }
}

- (CGFloat)topmostVisibleInfoBarHeight {
  for (UIView<InfoBarViewProtocol>* view in
       [self.subviews reverseObjectEnumerator]) {
    return [view sizeThatFits:self.frame.size].height;
  }
  return 0;
}

@end
