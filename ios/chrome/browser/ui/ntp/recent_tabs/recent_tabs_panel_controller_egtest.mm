// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import <map>
#import <string>

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"

namespace {
const char kURLOfTestPage[] = "http://testPage";
std::string const kHTMLOfTestPage =
    "<head><title>TestPageTitle</title></head><body>hello</body>";
NSString* const kTitleOfTestPage = @"TestPageTitle";

// Makes sure at least one tab is opened and opens the recent tab panel.
void OpenRecentTabsPanel() {
  // At least one tab is needed to be able to open the recent tabs panel.
  if (chrome_test_util::GetMainTabCount() == 0)
    chrome_test_util::OpenNewTab();

  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> open_recent_tabs_button_matcher =
      grey_accessibilityID(kToolsMenuOtherDevicesId);
  [[EarlGrey selectElementWithMatcher:open_recent_tabs_button_matcher]
      performAction:grey_tap()];
}

// Closes the recent tabs panel, on iPhone.
void CloseRecentTabsPanelOnIphone() {
  DCHECK(!IsIPadIdiom());

  id<GREYMatcher> exit_button_matcher = grey_accessibilityID(@"Exit");
  [[EarlGrey selectElementWithMatcher:exit_button_matcher]
      performAction:grey_tap()];
}

// Returns the matcher for the entry of the page in the recent tabs panel.
id<GREYMatcher> titleOfTestPageMatcher() {
  return grey_allOf(
      chrome_test_util::staticTextWithAccessibilityLabel(kTitleOfTestPage),
      grey_sufficientlyVisible(), nil);
}

// Returns the matcher for the back button.
id<GREYMatcher> backButtonMatcher() {
  return chrome_test_util::buttonWithAccessibilityLabelId(IDS_ACCNAME_BACK);
}

// Returns the matcher for the Recently closed label.
id<GREYMatcher> recentlyClosedLabelMatcher() {
  return chrome_test_util::staticTextWithAccessibilityLabelId(
      IDS_IOS_RECENT_TABS_RECENTLY_CLOSED);
}

}  // namespace

// Earl grey integration tests for Recent Tabs Panel Controller.
@interface RecentTabsPanelControllerTestCase : ChromeTestCase
@end

@implementation RecentTabsPanelControllerTestCase

- (void)setUp {
  [ChromeEarlGrey clearBrowsingHistory];
  [super setUp];
  std::map<GURL, std::string> responses;
  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);
  responses[testPageURL] = kHTMLOfTestPage;
  web::test::SetUpSimpleHttpServer(responses);
}

- (void)tearDown {
  if (IsIPadIdiom()) {
    chrome_test_util::OpenNewTab();
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:recentlyClosedLabelMatcher()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    // If the Recent Tabs panel is shown, then switch back to the Most Visited
    // panel so that tabs opened in other tests will show the Most Visited panel
    // instead of the Recent Tabs panel.
    if (!error) {
      [[EarlGrey selectElementWithMatcher:recentlyClosedLabelMatcher()]
          performAction:grey_swipeFastInDirection(kGREYDirectionRight)];
    }
    chrome_test_util::CloseCurrentTab();
  }
}

// Tests that a closed tab appears in the Recent Tabs panel, and that tapping
// the entry in the Recent Tabs panel re-opens the closed tab.
- (void)testClosedTabAppearsInRecentTabsPanel {
  const GURL testPageURL = web::test::HttpServer::MakeUrl(kURLOfTestPage);

  // Open the test page in a new tab.
  [ChromeEarlGrey loadURL:testPageURL];
  id<GREYMatcher> webViewMatcher =
      chrome_test_util::webViewContainingText("hello");
  [[EarlGrey selectElementWithMatcher:webViewMatcher]
      assertWithMatcher:grey_notNil()];

  // Open the Recent Tabs panel, check that the test page is not
  // present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:titleOfTestPageMatcher()]
      assertWithMatcher:grey_nil()];

  // Get rid of the Recent Tabs Panel.
  if (IsIPadIdiom()) {
    // On iPad, the Recent Tabs panel is a new page in the navigation history.
    // Go back to the previous page to restore the test page.
    [[EarlGrey selectElementWithMatcher:backButtonMatcher()]
        performAction:grey_tap()];
    [ChromeEarlGrey waitForPageToFinishLoading];
  } else {
    // On iPhone, the Recent Tabs panel is shown in a modal view.
    // Close that modal.
    CloseRecentTabsPanelOnIphone();
    // Wait until the recent tabs panel is dismissed.
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  }

  // Close the tab containing the test page.
  chrome_test_util::CloseCurrentTab();

  // Open the Recent Tabs panel and check that the test page is present.
  OpenRecentTabsPanel();
  [[EarlGrey selectElementWithMatcher:titleOfTestPageMatcher()]
      assertWithMatcher:grey_notNil()];

  // Tap on the entry for the test page in the Recent Tabs panel and check that
  // a tab containing the test page was opened.
  [[EarlGrey selectElementWithMatcher:titleOfTestPageMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::omnibox()]
      assertWithMatcher:chrome_test_util::omniboxText(
                            testPageURL.GetContent())];
}

@end
