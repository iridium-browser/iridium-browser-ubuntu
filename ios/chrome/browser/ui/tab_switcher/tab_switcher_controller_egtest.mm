// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/mac/scoped_nsobject.h"
#include "base/test/scoped_command_line.h"
#import "ios/chrome/app/main_controller_private.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#import "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"

using chrome_test_util::buttonWithAccessibilityLabel;
using chrome_test_util::buttonWithAccessibilityLabelId;
using chrome_test_util::staticTextWithAccessibilityLabelId;

namespace {

// Returns the GREYMatcher for the button that closes the tab switcher.
id<GREYMatcher> tabSwitcherCloseButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_TAB_STRIP_LEAVE_TAB_SWITCHER);
}

// Returns the GREYMatcher for the button that creates new non incognito tabs
// from within the tab switcher.
id<GREYMatcher> tabSwitcherNewTabButton() {
  return grey_allOf(
      buttonWithAccessibilityLabelId(IDS_IOS_TAB_SWITCHER_CREATE_NEW_TAB),
      grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the button that creates new incognito tabs from
// within the tab switcher.
id<GREYMatcher> tabSwitcherNewIncognitoTabButton() {
  return grey_allOf(buttonWithAccessibilityLabelId(
                        IDS_IOS_TAB_SWITCHER_CREATE_NEW_INCOGNITO_TAB),
                    grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the button to go to the non incognito panel in
// the tab switcher.
id<GREYMatcher> tabSwitcherHeaderPanelButton() {
  NSString* accessibility_label = l10n_util::GetNSStringWithFixup(
      IDS_IOS_TAB_SWITCHER_HEADER_NON_INCOGNITO_TABS);
  return grey_accessibilityLabel(accessibility_label);
}

// Returns the GREYMatcher for the button that closes tabs on iPad.
id<GREYMatcher> closeTabButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_TOOLS_MENU_CLOSE_TAB);
}

// Opens a new incognito tabs using the tools menu.
void openNewIncognitoTabUsingUI() {
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newIncognitoTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:newIncognitoTabButtonMatcher]
      performAction:grey_tap()];
}

// Triggers the opening of the tab switcher by launching a command. Should be
// called only when the tab switcher is not presented.
void enterTabSwitcherWithCommand() {
  base::scoped_nsobject<GenericChromeCommand> command(
      [[GenericChromeCommand alloc] initWithTag:IDC_TOGGLE_TAB_SWITCHER]);
  chrome_test_util::RunCommandWithActiveViewController(command);
}

}  // namespace

@interface TabSwitcherControllerTestCase : ChromeTestCase
@end

@implementation TabSwitcherControllerTestCase {
  std::unique_ptr<base::test::ScopedCommandLine> scoped_command_line_;
}

- (void)setUp {
  [super setUp];
  scoped_command_line_.reset(new base::test::ScopedCommandLine());
  scoped_command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kEnableTabSwitcher);
}

// Checks that the tab switcher is not presented.
- (void)assertTabSwitcherIsInactive {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  MainController* main_controller = chrome_test_util::GetMainController();
  GREYAssertTrue(![main_controller isTabSwitcherActive],
                 @"Tab Switcher should be inactive");
}

// Checks that the tab switcher is active.
- (void)assertTabSwitcherIsActive {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  MainController* main_controller = chrome_test_util::GetMainController();
  GREYAssertTrue([main_controller isTabSwitcherActive],
                 @"Tab Switcher should be active");
}

// Checks that the text associated with |messageId| is somewhere on screen.
- (void)assertMessageIsVisible:(int)messageId {
  id<GREYMatcher> messageMatcher =
      grey_allOf(staticTextWithAccessibilityLabelId(messageId),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:messageMatcher]
      assertWithMatcher:grey_notNil()];
}

// Checks that the text associated with |messageId| is not visible.
- (void)assertMessageIsNotVisible:(int)messageId {
  id<GREYMatcher> messageMatcher =
      grey_allOf(staticTextWithAccessibilityLabelId(messageId),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:messageMatcher]
      assertWithMatcher:grey_nil()];
}

// Tests entering and leaving the tab switcher.
- (void)testEnteringTabSwitcher {
  if (!IsIPadIdiom())
    return;

  [self assertTabSwitcherIsInactive];

  enterTabSwitcherWithCommand();
  [self assertTabSwitcherIsActive];

  // Check that the "No Open Tabs" message is not displayed.
  [self assertMessageIsNotVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS_TITLE];

  // Press the :: icon to exit the tab switcher.
  [[EarlGrey selectElementWithMatcher:tabSwitcherCloseButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsInactive];
}

// Tests entering tab switcher by closing all tabs, and leaving the tab switcher
// by creating a new tab.
- (void)testClosingAllTabsAndCreatingNewTab {
  if (!IsIPadIdiom())
    return;

  [self assertTabSwitcherIsInactive];

  // Close the tab.
  [[EarlGrey selectElementWithMatcher:closeTabButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsActive];

  // Check that the "No Open Tabs" message is displayed.
  [self assertMessageIsVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_NON_INCOGNITO_TABS_TITLE];

  // Create a new tab.
  [[EarlGrey selectElementWithMatcher:tabSwitcherNewTabButton()]
      performAction:grey_tap()];

  [self assertTabSwitcherIsInactive];
}

// Tests entering tab switcher from incognito mode.
- (void)testIncognitoTabs {
  if (!IsIPadIdiom())
    return;

  [self assertTabSwitcherIsInactive];

  // Create new incognito tab from tools menu.
  openNewIncognitoTabUsingUI();

  // Close the incognito tab and check that the we are entering the tab
  // switcher.
  [[EarlGrey selectElementWithMatcher:closeTabButton()]
      performAction:grey_tap()];
  [self assertTabSwitcherIsActive];

  // Check that the "No Incognito Tabs" message is shown.
  [self assertMessageIsVisible:
            IDS_IOS_TAB_SWITCHER_NO_LOCAL_INCOGNITO_TABS_PROMO];

  // Create new incognito tab.
  [[EarlGrey selectElementWithMatcher:tabSwitcherNewIncognitoTabButton()]
      performAction:grey_tap()];

  // Verify that we've left the tab switcher.
  [self assertTabSwitcherIsInactive];

  // Close tab and verify we've entered the tab switcher again.
  [[EarlGrey selectElementWithMatcher:closeTabButton()]
      performAction:grey_tap()];
  [self assertTabSwitcherIsActive];

  // Switch to the non incognito panel.
  [[EarlGrey selectElementWithMatcher:tabSwitcherHeaderPanelButton()]
      performAction:grey_tap()];

  // Press the :: icon to exit the tab switcher.
  [[EarlGrey selectElementWithMatcher:tabSwitcherCloseButton()]
      performAction:grey_tap()];

  // Verify that we've left the tab switcher.
  [self assertTabSwitcherIsInactive];
}

@end
