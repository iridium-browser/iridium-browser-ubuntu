// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/rappor/rappor_utils.h"
#include "components/rappor/test_rappor_service.h"

namespace {

const char kDefaultTestUrl[] = "https://google.com";
const char kDefaultTestUrlAnchor[] = "https://google.com#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com";

}  // namespace

class CorePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(new CorePageLoadMetricsObserver()));
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
    TestingBrowserProcess::GetGlobal()->SetRapporService(&rappor_tester_);
  }

  rappor::TestRapporService rappor_tester_;
};

TEST_F(CorePageLoadMetricsObserverTest, NoMetrics) {
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, SamePageNoTriggerUntilTrueNavCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  NavigateAndCommit(GURL(kDefaultTestUrlAnchor));
  // A same page navigation shouldn't trigger logging UMA for the original.
  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 0);

  // But we should keep the timing info and log it when we get another
  // navigation.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, SingleMetricAfterCommit) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta parse_start = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta parse_stop = base::TimeDelta::FromMilliseconds(5);
  base::TimeDelta parse_script_block_duration =
      base::TimeDelta::FromMilliseconds(3);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;
  timing.parse_start = parse_start;
  timing.parse_stop = parse_stop;
  timing.parse_blocked_on_script_load_duration = parse_script_block_duration;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 0);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramParseDuration,
      (parse_stop - parse_start).InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramParseBlockedOnScriptLoad,
      parse_script_block_duration.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, MultipleMetricsAfterCommits) {
  base::TimeDelta response = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta first_layout_1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta first_layout_2 = base::TimeDelta::FromMilliseconds(20);
  base::TimeDelta first_text_paint = base::TimeDelta::FromMilliseconds(30);
  base::TimeDelta first_contentful_paint = first_text_paint;
  base::TimeDelta dom_content = base::TimeDelta::FromMilliseconds(40);
  base::TimeDelta load = base::TimeDelta::FromMilliseconds(100);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.response_start = response;
  timing.first_layout = first_layout_1;
  timing.first_text_paint = first_text_paint;
  timing.first_contentful_paint = first_contentful_paint;
  timing.dom_content_loaded_event_start = dom_content;
  timing.load_event_start = load;
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  histogram_tester().ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstContentfulPaint,
                                       first_contentful_paint.InMilliseconds(),
                                       1);

  NavigateAndCommit(GURL(kDefaultTestUrl2));

  page_load_metrics::PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(200);
  timing2.first_layout = first_layout_2;
  PopulateRequiredTimingFields(&timing2);

  SimulateTimingUpdate(timing2);

  NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 2);

  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 2);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout_1.InMilliseconds(), 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout_2.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramFirstContentfulPaint,
                                      1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstContentfulPaint,
                                       first_contentful_paint.InMilliseconds(),
                                       1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstTextPaint,
                                       first_text_paint.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramDomContentLoaded,
                                       dom_content.InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramLoad,
                                       load.InMilliseconds(), 1);
}

TEST_F(CorePageLoadMetricsObserverTest, BackgroundDifferentHistogram) {
  base::TimeDelta first_layout = base::TimeDelta::FromSeconds(2);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  // Simulate "Open link in new tab."
  web_contents()->WasHidden();
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Simulate switching to the tab and making another navigation.
  web_contents()->WasShown();

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramFirstLayout,
                                      1);
  histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstLayout, first_layout.InMilliseconds(),
      1);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstTextPaint, 0);

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest,
       BackgroundCommitHistogramClockResolutionNonDeterministic) {
  base::TimeDelta first_layout = base::TimeDelta::FromMilliseconds(1);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  // Start a provisional load.
  GURL url(kDefaultTestUrl2);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(url);

  // Background and then commit.
  web_contents()->WasHidden();
  rfh_tester->SimulateNavigationCommit(url);
  SimulateTimingUpdate(timing);
  rfh_tester->SimulateNavigationStop();

  page_load_metrics::PageLoadExtraInfo info =
      GetPageLoadExtraInfoForCommittedLoad();

  // Navigate again to force histograms to be logged.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // If the system clock is low resolution PageLoadTracker's commit_time_ may
  // be = first_background_time_.
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          info.time_to_commit, info)) {
    histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramCommit,
                                        0);
    histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  } else {
    histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramCommit,
                                        1);
    histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 0);
  }
}

TEST_F(CorePageLoadMetricsObserverTest, OnlyBackgroundLaterEvents) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.dom_content_loaded_event_start = base::TimeDelta::FromMicroseconds(1);
  PopulateRequiredTimingFields(&timing);

  // Make sure first_text_paint hasn't been set (wasn't set by
  // PopulateRequiredTimingFields), since we want to defer setting it until
  // after backgrounding.
  ASSERT_FALSE(timing.first_text_paint);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Background the tab, then foreground it.
  web_contents()->WasHidden();
  web_contents()->WasShown();
  timing.first_text_paint = base::TimeDelta::FromSeconds(4);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);

  // If the system clock is low resolution, PageLoadTracker's
  // first_background_time_ may be same as other times such as
  // dom_content_loaded_event_start.
  page_load_metrics::PageLoadExtraInfo info =
      GetPageLoadExtraInfoForCommittedLoad();

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramCommit, 0);

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded,
                                        1);
    histogram_tester().ExpectBucketCount(
        internal::kHistogramDomContentLoaded,
        timing.dom_content_loaded_event_start.value().InMilliseconds(), 1);
    histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramDomContentLoaded, 0);
  } else {
    histogram_tester().ExpectTotalCount(
        internal::kBackgroundHistogramDomContentLoaded, 1);
    histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded,
                                        0);
  }

  histogram_tester().ExpectTotalCount(internal::kBackgroundHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(
      internal::kBackgroundHistogramFirstTextPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kBackgroundHistogramFirstTextPaint,
      timing.first_text_paint.value().InMilliseconds(), 1);

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, DontBackgroundQuickerLoad) {
  // Set this event at 1 microsecond so it occurs before we foreground later in
  // the test.
  base::TimeDelta first_layout = base::TimeDelta::FromMicroseconds(1);

  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_layout = first_layout;
  PopulateRequiredTimingFields(&timing);

  web_contents()->WasHidden();

  // Open in new tab
  StartNavigation(GURL(kDefaultTestUrl));

  // Switch to the tab
  web_contents()->WasShown();

  // Start another provisional load
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  SimulateTimingUpdate(timing);

  // Navigate again to see if the timing updated for the foregrounded load.
  NavigateAndCommit(GURL(kDefaultTestUrl));

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 1);
  histogram_tester().ExpectBucketCount(internal::kHistogramFirstLayout,
                                       first_layout.InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, FailedProvisionalLoad) {
  GURL url(kDefaultTestUrl);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(url);
  rfh_tester->SimulateNavigationError(url, net::ERR_TIMED_OUT);
  rfh_tester->SimulateNavigationStop();

  histogram_tester().ExpectTotalCount(internal::kHistogramCommit, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramDomContentLoaded, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramLoad, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstLayout, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFirstTextPaint, 0);
  histogram_tester().ExpectTotalCount(internal::kHistogramFailedProvisionalLoad,
                                      1);
}

TEST_F(CorePageLoadMetricsObserverTest, FailedBackgroundProvisionalLoad) {
  // Test that failed provisional event does not get logged in the
  // histogram if it happened in the background
  GURL url(kDefaultTestUrl);
  web_contents()->WasHidden();
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  rfh_tester->SimulateNavigationStart(url);
  rfh_tester->SimulateNavigationError(url, net::ERR_TIMED_OUT);
  rfh_tester->SimulateNavigationStop();

  histogram_tester().ExpectTotalCount(internal::kHistogramFailedProvisionalLoad,
                                      0);
}

TEST_F(CorePageLoadMetricsObserverTest, BackgroundBeforePaint) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_paint = base::TimeDelta::FromSeconds(10);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kDefaultTestUrl));
  // Background the tab and go for a coffee or something.
  web_contents()->WasHidden();
  SimulateTimingUpdate(timing);
  // Come back and start browsing again.
  web_contents()->WasShown();
  // Simulate the user performaning another navigation.
  NavigateAndCommit(GURL("https://www.example.com"));
  histogram_tester().ExpectTotalCount(internal::kHistogramBackgroundBeforePaint,
                                      1);
}

TEST_F(CorePageLoadMetricsObserverTest, NoRappor) {
  rappor::TestSample::Shadow* sample_obj =
      rappor_tester_.GetRecordedSampleForMetric(
          internal::kRapporMetricsNameCoarseTiming);
  EXPECT_EQ(sample_obj, nullptr);
}

TEST_F(CorePageLoadMetricsObserverTest, RapporLongPageLoad) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_contentful_paint = base::TimeDelta::FromSeconds(40);
  PopulateRequiredTimingFields(&timing);
  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to force logging RAPPOR.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  rappor::TestSample::Shadow* sample_obj =
      rappor_tester_.GetRecordedSampleForMetric(
          internal::kRapporMetricsNameCoarseTiming);
  const auto& string_it = sample_obj->string_fields.find("Domain");
  EXPECT_NE(string_it, sample_obj->string_fields.end());
  EXPECT_EQ(rappor::GetDomainAndRegistrySampleFromGURL(GURL(kDefaultTestUrl)),
            string_it->second);

  const auto& flag_it = sample_obj->flag_fields.find("IsSlow");
  EXPECT_NE(flag_it, sample_obj->flag_fields.end());
  EXPECT_EQ(1u, flag_it->second);
}

TEST_F(CorePageLoadMetricsObserverTest, RapporQuickPageLoad) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.first_contentful_paint = base::TimeDelta::FromSeconds(1);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  // Navigate again to force logging RAPPOR.
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  rappor::TestSample::Shadow* sample_obj =
      rappor_tester_.GetRecordedSampleForMetric(
          internal::kRapporMetricsNameCoarseTiming);
  const auto& string_it = sample_obj->string_fields.find("Domain");
  EXPECT_NE(string_it, sample_obj->string_fields.end());
  EXPECT_EQ(rappor::GetDomainAndRegistrySampleFromGURL(GURL(kDefaultTestUrl)),
            string_it->second);

  const auto& flag_it = sample_obj->flag_fields.find("IsSlow");
  EXPECT_NE(flag_it, sample_obj->flag_fields.end());
  EXPECT_EQ(0u, flag_it->second);
}

TEST_F(CorePageLoadMetricsObserverTest, Reload) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.first_contentful_paint = base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  NavigateWithPageTransitionAndCommit(url, ui::PAGE_TRANSITION_RELOAD);
  SimulateTimingUpdate(timing);
  NavigateAndCommit(url);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload,
      timing.first_contentful_paint.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartReload,
      timing.parse_start.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, ForwardBack) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.first_contentful_paint = base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  // Back navigations to a page that was reloaded report a main transition type
  // of PAGE_TRANSITION_RELOAD with a PAGE_TRANSITION_FORWARD_BACK
  // modifier. This test verifies that when we encounter such a page, we log it
  // as a forward/back navigation.
  NavigateWithPageTransitionAndCommit(
      url, ui::PageTransitionFromInt(ui::PAGE_TRANSITION_RELOAD |
                                     ui::PAGE_TRANSITION_FORWARD_BACK));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(url);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack,
      timing.first_contentful_paint.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartForwardBack,
      timing.parse_start.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 0);
}

TEST_F(CorePageLoadMetricsObserverTest, NewNavigation) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.first_contentful_paint = base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  GURL url(kDefaultTestUrl);
  NavigateWithPageTransitionAndCommit(url, ui::PAGE_TRANSITION_LINK);
  SimulateTimingUpdate(timing);
  NavigateAndCommit(url);

  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintReload, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation,
      timing.first_contentful_paint.value().InMilliseconds(), 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartReload, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartForwardBack, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramLoadTypeParseStartNewNavigation, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramLoadTypeParseStartNewNavigation,
      timing.parse_start.value().InMilliseconds(), 1);
}

TEST_F(CorePageLoadMetricsObserverTest, FirstMeaningfulPaint) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.first_meaningful_paint = base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstMeaningfulPaint, 1);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 1);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_RECORDED, 1);
}

TEST_F(CorePageLoadMetricsObserverTest, FirstMeaningfulPaintAfterInteraction) {
  page_load_metrics::PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.parse_start = base::TimeDelta::FromMilliseconds(5);
  timing.first_paint = base::TimeDelta::FromMilliseconds(10);
  PopulateRequiredTimingFields(&timing);

  NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);

  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  SimulateInputEvent(mouse_event);

  timing.first_meaningful_paint = base::TimeDelta::FromMilliseconds(1000);
  PopulateRequiredTimingFields(&timing);
  SimulateTimingUpdate(timing);

  NavigateAndCommit(GURL(kDefaultTestUrl2));

  histogram_tester().ExpectTotalCount(
      internal::kHistogramFirstMeaningfulPaint, 0);
  histogram_tester().ExpectTotalCount(
      internal::kHistogramParseStartToFirstMeaningfulPaint, 0);
  histogram_tester().ExpectBucketCount(
      internal::kHistogramFirstMeaningfulPaintStatus,
      internal::FIRST_MEANINGFUL_PAINT_USER_INTERACTION_BEFORE_FMP, 1);
}
