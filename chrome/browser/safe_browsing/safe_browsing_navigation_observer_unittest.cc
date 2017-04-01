// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

class SBNavigationObserverTest : public BrowserWithTestWindowTest {
 public:
  SBNavigationObserverTest() {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("http://foo/0"));
    navigation_observer_manager_ = new SafeBrowsingNavigationObserverManager();
    navigation_observer_ = new SafeBrowsingNavigationObserver(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        navigation_observer_manager_);
  }
  void TearDown() override {
    delete navigation_observer_;
    BrowserWithTestWindowTest::TearDown();
  }
  void VerifyNavigationEvent(const GURL& expected_source_url,
                             const GURL& expected_source_main_frame_url,
                             const GURL& expected_original_request_url,
                             const GURL& expected_destination_url,
                             int expected_source_tab,
                             int expected_target_tab,
                             bool expected_is_user_initiated,
                             bool expected_has_committed,
                             bool expected_has_server_redirect,
                             const NavigationEvent& actual_nav_event) {
    EXPECT_EQ(expected_source_url, actual_nav_event.source_url);
    EXPECT_EQ(expected_source_main_frame_url,
              actual_nav_event.source_main_frame_url);
    EXPECT_EQ(expected_original_request_url,
              actual_nav_event.original_request_url);
    EXPECT_EQ(expected_destination_url, actual_nav_event.destination_url);
    EXPECT_EQ(expected_source_tab, actual_nav_event.source_tab_id);
    EXPECT_EQ(expected_target_tab, actual_nav_event.target_tab_id);
    EXPECT_EQ(expected_is_user_initiated, actual_nav_event.is_user_initiated);
    EXPECT_EQ(expected_has_committed, actual_nav_event.has_committed);
    EXPECT_EQ(expected_has_server_redirect,
              actual_nav_event.has_server_redirect);
  }

  SafeBrowsingNavigationObserverManager::NavigationMap* navigation_map() {
    return navigation_observer_manager_->navigation_map();
  }

  SafeBrowsingNavigationObserverManager::UserGestureMap* user_gesture_map() {
    return &navigation_observer_manager_->user_gesture_map_;
  }

  SafeBrowsingNavigationObserverManager::HostToIpMap* host_to_ip_map() {
    return &navigation_observer_manager_->host_to_ip_map_;
  }

  NavigationEvent CreateNavigationEvent(const GURL& destination_url,
                                        const base::Time& timestamp) {
    NavigationEvent nav_event;
    nav_event.destination_url = destination_url;
    nav_event.last_updated = timestamp;
    return nav_event;
  }

  void CleanUpNavigationEvents() {
    navigation_observer_manager_->CleanUpNavigationEvents();
  }

  void CleanUpIpAddresses() {
    navigation_observer_manager_->CleanUpIpAddresses();
  }

  void CleanUpUserGestures() {
    navigation_observer_manager_->CleanUpUserGestures();
  }

 protected:
  SafeBrowsingNavigationObserverManager* navigation_observer_manager_;
  SafeBrowsingNavigationObserver* navigation_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SBNavigationObserverTest);
};

TEST_F(SBNavigationObserverTest, BasicNavigationAndCommit) {
  // Navigation in current tab.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  browser()->OpenURL(
      content::OpenURLParams(GURL("http://foo/1"), content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
  CommitPendingLoad(controller);
  int tab_id = SessionTabHelper::IdForTab(controller->GetWebContents());
  auto nav_map = navigation_map();
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(GURL("http://foo/1")).size());
  VerifyNavigationEvent(GURL(),                // source_url
                        GURL(),                // source_main_frame_url
                        GURL("http://foo/1"),  // original_request_url
                        GURL("http://foo/1"),  // destination_url
                        tab_id,                // source_tab_id
                        tab_id,                // target_tab_id
                        true,                  // is_user_initiated
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_map->at(GURL("http://foo/1")).at(0));
}

TEST_F(SBNavigationObserverTest, ServerRedirect) {
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(
          browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
  rfh_tester->SimulateNavigationStart(GURL("http://foo/3"));
  GURL redirect("http://redirect/1");
  rfh_tester->SimulateRedirect(redirect);
  rfh_tester->SimulateNavigationCommit(redirect);
  int tab_id = SessionTabHelper::IdForTab(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  auto nav_map = navigation_map();
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(redirect).size());
  VerifyNavigationEvent(GURL("http://foo/0"),       // source_url
                        GURL("http://foo/0"),       // source_main_frame_url
                        GURL("http://foo/3"),       // original_request_url
                        GURL("http://redirect/1"),  // destination_url
                        tab_id,                     // source_tab_id
                        tab_id,                     // target_tab_id
                        false,                      // is_user_initiated
                        true,                       // has_committed
                        true,                       // has_server_redirect
                        nav_map->at(GURL("http://redirect/1")).at(0));
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleNavigationEvents) {
  // Sets up navigation_map() such that it includes fresh, stale and invalid
  // navigation events.
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);  // Stale
  base::Time one_minute_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0);  // Fresh
  base::Time in_an_hour =
      base::Time::FromDoubleT(now.ToDoubleT() + 60.0 * 60.0);  // Invalid
  GURL url_0("http://foo/0");
  GURL url_1("http://foo/1");
  navigation_map()->insert(
      std::make_pair(url_0, std::vector<NavigationEvent>()));
  navigation_map()->at(url_0).push_back(
      CreateNavigationEvent(url_0, one_hour_ago));
  navigation_map()->at(url_0).push_back(CreateNavigationEvent(url_0, now));
  navigation_map()->at(url_0).push_back(
      CreateNavigationEvent(url_0, one_minute_ago));
  navigation_map()->at(url_0).push_back(
      CreateNavigationEvent(url_0, in_an_hour));
  navigation_map()->insert(
      std::make_pair(url_1, std::vector<NavigationEvent>()));
  navigation_map()->at(url_1).push_back(
      CreateNavigationEvent(url_0, one_hour_ago));
  navigation_map()->at(url_1).push_back(
      CreateNavigationEvent(url_0, one_hour_ago));
  ASSERT_EQ(std::size_t(2), navigation_map()->size());
  ASSERT_EQ(std::size_t(4), navigation_map()->at(url_0).size());
  ASSERT_EQ(std::size_t(2), navigation_map()->at(url_1).size());

  // Cleans up navigation events.
  CleanUpNavigationEvents();

  // Verifies all stale and invalid navigation events are removed.
  ASSERT_EQ(std::size_t(1), navigation_map()->size());
  EXPECT_EQ(navigation_map()->end(), navigation_map()->find(url_1));
  EXPECT_EQ(std::size_t(2), navigation_map()->at(url_0).size());
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleUserGestures) {
  // Sets up user_gesture_map() such that it includes fresh, stale and invalid
  // user gestures.
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_minute_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0);  // Stale
  base::Time in_an_hour =
      base::Time::FromDoubleT(now.ToDoubleT() + 60.0 * 60.0);  // Invalid
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));
  content::WebContents* content0 =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* content1 =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  content::WebContents* content2 =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  user_gesture_map()->insert(std::make_pair(content0, now));
  user_gesture_map()->insert(std::make_pair(content1, one_minute_ago));
  user_gesture_map()->insert(std::make_pair(content2, in_an_hour));
  ASSERT_EQ(std::size_t(3), user_gesture_map()->size());

  // Cleans up user_gesture_map()
  CleanUpUserGestures();

  // Verifies all stale and invalid user gestures are removed.
  ASSERT_EQ(std::size_t(1), user_gesture_map()->size());
  EXPECT_NE(user_gesture_map()->end(), user_gesture_map()->find(content0));
  EXPECT_EQ(now, user_gesture_map()->at(content0));
}

TEST_F(SBNavigationObserverTest, TestCleanUpStaleIPAddresses) {
  // Sets up host_to_ip_map() such that it includes fresh, stale and invalid
  // user gestures.
  base::Time now = base::Time::Now();  // Fresh
  base::Time one_hour_ago =
      base::Time::FromDoubleT(now.ToDoubleT() - 60.0 * 60.0);  // Stale
  base::Time in_an_hour =
      base::Time::FromDoubleT(now.ToDoubleT() + 60.0 * 60.0);  // Invalid
  std::string host_0 = GURL("http://foo/0").host();
  std::string host_1 = GURL("http://bar/1").host();
  host_to_ip_map()->insert(
      std::make_pair(host_0, std::vector<ResolvedIPAddress>()));
  host_to_ip_map()->at(host_0).push_back(ResolvedIPAddress(now, "1.1.1.1"));
  host_to_ip_map()->at(host_0).push_back(
      ResolvedIPAddress(one_hour_ago, "2.2.2.2"));
  host_to_ip_map()->insert(
      std::make_pair(host_1, std::vector<ResolvedIPAddress>()));
  host_to_ip_map()->at(host_1).push_back(
      ResolvedIPAddress(in_an_hour, "3.3.3.3"));
  ASSERT_EQ(std::size_t(2), host_to_ip_map()->size());

  // Cleans up host_to_ip_map()
  CleanUpIpAddresses();

  // Verifies all stale and invalid IP addresses are removed.
  ASSERT_EQ(std::size_t(1), host_to_ip_map()->size());
  EXPECT_EQ(host_to_ip_map()->end(), host_to_ip_map()->find(host_1));
  ASSERT_EQ(std::size_t(1), host_to_ip_map()->at(host_0).size());
  EXPECT_EQ(now, host_to_ip_map()->at(host_0).front().timestamp);
}

}  // namespace safe_browsing
