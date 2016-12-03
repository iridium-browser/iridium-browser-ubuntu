// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_page_load_metrics_observer.h"

#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/common/features.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

namespace previews {

namespace internal {

const char kHistogramOfflinePreviewsDOMContentLoadedEventFired[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToDOMContentLoadedEventFired";
const char kHistogramOfflinePreviewsFirstLayout[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToFirstLayout";
const char kHistogramOfflinePreviewsLoadEventFired[] =
    "PageLoad.Clients.Previews.OfflinePages.DocumentTiming."
    "NavigationToLoadEventFired";
const char kHistogramOfflinePreviewsFirstContentfulPaint[] =
    "PageLoad.Clients.Previews.OfflinePages.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramOfflinePreviewsParseStart[] =
    "PageLoad.Clients.Previews.OfflinePages.ParseTiming.NavigationToParseStart";

}  // namespace internal

PreviewsPageLoadMetricsObserver::PreviewsPageLoadMetricsObserver()
    : is_offline_preview_(false) {}

PreviewsPageLoadMetricsObserver::~PreviewsPageLoadMetricsObserver() {}

void PreviewsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  is_offline_preview_ = IsOfflinePreview(navigation_handle->GetWebContents());
}

void PreviewsPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!is_offline_preview_ ||
      !WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramOfflinePreviewsDOMContentLoadedEventFired,
      timing.dom_content_loaded_event_start.value());
}

void PreviewsPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!is_offline_preview_ ||
      !WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsLoadEventFired,
                      timing.load_event_start.value());
}

void PreviewsPageLoadMetricsObserver::OnFirstLayout(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!is_offline_preview_ ||
      !WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsFirstLayout,
                      timing.first_layout.value());
}

void PreviewsPageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!is_offline_preview_ ||
      !WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsFirstContentfulPaint,
                      timing.first_contentful_paint.value());
}

void PreviewsPageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (!is_offline_preview_ ||
      !WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramOfflinePreviewsParseStart,
                      timing.parse_start.value());
}

bool PreviewsPageLoadMetricsObserver::IsOfflinePreview(
    content::WebContents* web_contents) const {
#if BUILDFLAG(ANDROID_JAVA_UI)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->is_offline_preview();
#else
  return false;
#endif  // BUILDFLAG(ANDROID_JAVA_UI)
}

}  // namespace previews
