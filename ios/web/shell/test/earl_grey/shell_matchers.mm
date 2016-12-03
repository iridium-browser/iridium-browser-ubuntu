// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_matchers.h"

#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/testing/earl_grey/wait_util.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"
#import "ios/web/shell/view_controller.h"

namespace web {

id<GREYMatcher> webViewContainingText(std::string text) {
  WebState* web_state = shell_test_util::GetCurrentWebState();
  return webViewContainingText(std::move(text), web_state);
}

id<GREYMatcher> webViewCssSelector(std::string selector) {
  WebState* web_state = shell_test_util::GetCurrentWebState();
  return webViewCssSelector(std::move(selector), web_state);
}

id<GREYMatcher> webViewScrollView() {
  return webViewScrollView(shell_test_util::GetCurrentWebState());
}

id<GREYMatcher> addressFieldText(std::string text) {
  MatchesBlock matches = ^BOOL(UIView* view) {
    if (![view isKindOfClass:[UITextField class]]) {
      return NO;
    }
    if (![[view accessibilityLabel]
            isEqualToString:kWebShellAddressFieldAccessibilityLabel]) {
      return NO;
    }
    UITextField* text_field = base::mac::ObjCCastStrict<UITextField>(view);
    testing::WaitUntilCondition(testing::kWaitForUIElementTimeout, ^bool() {
      return [text_field.text isEqualToString:base::SysUTF8ToNSString(text)];
    });
    return YES;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"address field containing "];
    [description appendText:base::SysUTF8ToNSString(text)];
  };

  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
}

id<GREYMatcher> backButton() {
  return grey_accessibilityLabel(kWebShellBackButtonAccessibilityLabel);
}

id<GREYMatcher> forwardButton() {
  return grey_accessibilityLabel(kWebShellForwardButtonAccessibilityLabel);
}

id<GREYMatcher> addressField() {
  return grey_accessibilityLabel(kWebShellAddressFieldAccessibilityLabel);
}

}  // namespace web
