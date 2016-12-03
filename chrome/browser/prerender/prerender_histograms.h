// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_HISTOGRAMS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_HISTOGRAMS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "url/gurl.h"

namespace prerender {

// Navigation type for histograms.
enum NavigationType {
  // A normal completed navigation.
  NAVIGATION_TYPE_NORMAL,
  // A completed navigation or swap that began as a prerender.
  NAVIGATION_TYPE_PRERENDERED,
  // A normal completed navigation in the control group or with a control
  // prerender that would have been prerendered.
  NAVIGATION_TYPE_WOULD_HAVE_BEEN_PRERENDERED,
  NAVIGATION_TYPE_MAX,
};

// PrerenderHistograms is responsible for recording all prerender specific
// histograms for PrerenderManager.  It keeps track of the type of prerender
// currently underway (based on the PrerenderOrigin of the most recent
// prerenders, and any experiments detected).
// PrerenderHistograms does not necessarily record all histograms related to
// prerendering, only the ones in the context of PrerenderManager.
class PrerenderHistograms {
 public:
  // Owned by a PrerenderManager object for the lifetime of the
  // PrerenderManager.
  PrerenderHistograms();

  // Records the perceived page load time for a page - effectively the time from
  // when the user navigates to a page to when it finishes loading.  The actual
  // load may have started prior to navigation due to prerender hints.
  void RecordPerceivedPageLoadTime(Origin origin,
                                   base::TimeDelta perceived_page_load_time,
                                   NavigationType navigation_type,
                                   const GURL& url);

  // Records, in a histogram, the percentage of the page load time that had
  // elapsed by the time it is swapped in.  Values outside of [0, 1.0] are
  // invalid and ignored.
  void RecordPercentLoadDoneAtSwapin(Origin origin, double fraction) const;

  // Records the actual pageload time of a prerender that has not been swapped
  // in yet, but finished loading.
  void RecordPageLoadTimeNotSwappedIn(Origin origin,
                                      base::TimeDelta page_load_time,
                                      const GURL& url) const;

  // Records the time from when a page starts prerendering to when the user
  // navigates to it. This must be called on the UI thread.
  void RecordTimeUntilUsed(Origin origin,
                           base::TimeDelta time_until_used) const;

  // Records the time from when a prerender is abandoned to when the user
  // navigates to it. This must be called on the UI thread.
  void RecordAbandonTimeUntilUsed(Origin origin,
                                  base::TimeDelta time_until_used) const;

  // Record a PerSessionCount data point.
  void RecordPerSessionCount(Origin origin, int count) const;

  // Record time between two prerender requests.
  void RecordTimeBetweenPrerenderRequests(Origin origin,
                                          base::TimeDelta time) const;

  // Record a final status of a prerendered page in a histogram.
  void RecordFinalStatus(Origin origin, FinalStatus final_status) const;

  // To be called when a new prerender is added.
  void RecordPrerender(Origin origin, const GURL& url);

  // To be called when a new prerender is started.
  void RecordPrerenderStarted(Origin origin) const;

  // To be called when we know how many prerenders are running after starting
  // a prerender.
  void RecordConcurrency(size_t prerender_count) const;

  // Called when we swap in a prerender.
  void RecordUsedPrerender(Origin origin) const;

  // Record the time since a page was recently visited.
  void RecordTimeSinceLastRecentVisit(Origin origin,
                                      base::TimeDelta time) const;

  // Record the bytes in the prerender, whether it was used or not, and the
  // total number of bytes fetched for this profile since the last call to
  // RecordBytes.
  void RecordNetworkBytes(Origin origin,
                          bool used,
                          int64_t prerender_bytes,
                          int64_t profile_bytes);

 private:
  base::TimeTicks GetCurrentTimeTicks() const;

  // Returns the time elapsed since the last prerender happened.
  base::TimeDelta GetTimeSinceLastPrerender() const;

  // Returns whether the PrerenderManager is currently within the prerender
  // window - effectively, up to 30 seconds after a prerender tag has been
  // observed.
  bool WithinWindow() const;

  // Returns whether or not there is currently an origin wash.
  bool IsOriginWash() const;

  // Origin of the last prerender seen.
  Origin last_origin_;

  // A boolean indicating that we have recently encountered a combination of
  // different origins, making an attribution of PPLT's to origins impossible.
  bool origin_wash_;

  // The time when we last saw a prerender request coming from a renderer.
  // This is used to record perceived PLT's for a certain amount of time
  // from the point that we last saw a <link rel=prerender> tag.
  base::TimeTicks last_prerender_seen_time_;

  // Indicates whether we have recorded page load events after the most
  // recent prerender.  These must be initialized to true, so that we don't
  // start recording events before the first prerender occurs.
  bool seen_any_pageload_;
  bool seen_pageload_started_after_prerender_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderHistograms);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_HISTOGRAMS_H_
