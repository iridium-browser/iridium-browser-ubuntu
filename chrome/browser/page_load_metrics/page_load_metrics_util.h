// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_UTIL_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_UTIL_H_

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/time/time.h"

#define PAGE_LOAD_HISTOGRAM(name, sample)                           \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                          \
                             base::TimeDelta::FromMilliseconds(10), \
                             base::TimeDelta::FromMinutes(10), 100)

namespace page_load_metrics {

struct PageLoadExtraInfo;

// Returns true if:
// - We have timing information for the event.
// - The page load started while the page was in the foreground.
// - The event occurred prior to the page being moved to the background.
// When a page is backgrounded, some events (e.g. paint) are delayed. Since
// these data points can skew the mean, they should not be mixed with timing
// events that occurred in the foreground.
// If the event time delta and background time delta are equal, we still
// consider the event to be logged in the foreground histogram since any
// background specific handling would not yet have been applied to that event.
bool WasStartedInForegroundOptionalEventInForeground(
    const base::Optional<base::TimeDelta>& event,
    const PageLoadExtraInfo& info);

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_UTIL_H_
