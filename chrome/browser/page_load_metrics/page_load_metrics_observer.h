// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "url/gurl.h"

namespace page_load_metrics {

// This enum represents how a page load ends. If the action occurs before the
// page load finishes (or reaches some point like first paint), then we consider
// the load to be aborted.
enum UserAbortType {
  // Represents no abort.
  ABORT_NONE,

  // If the user presses reload or shift-reload.
  ABORT_RELOAD,

  // The user presses the back/forward button.
  ABORT_FORWARD_BACK,

  // The navigation is replaced with a navigation with the qualifier
  // ui::PAGE_TRANSITION_CLIENT_REDIRECT, which is caused by Javascript, or the
  // meta refresh tag.
  ABORT_CLIENT_REDIRECT,

  // If the page load is replaced by a new navigation. This includes link
  // clicks, typing in the omnibox (not a reload), and form submissions.
  ABORT_NEW_NAVIGATION,

  // If the user presses the stop X button.
  ABORT_STOP,

  // If the page load is aborted by closing the tab or browser.
  ABORT_CLOSE,

  // The page load was backgrounded, e.g. the browser was minimized or the user
  // switched tabs. Note that the same page may be foregrounded in the future,
  // so this is not a 'terminal' abort type.
  ABORT_BACKGROUND,

  // We don't know why the page load aborted. This is the value we assign to an
  // aborted load if the only signal we get is a provisional load finishing
  // without committing, either without error or with net::ERR_ABORTED.
  ABORT_OTHER,

  // Add values before this final count.
  ABORT_LAST_ENTRY
};

// Information related to failed provisional loads.
struct FailedProvisionalLoadInfo {
  FailedProvisionalLoadInfo(base::TimeDelta interval, net::Error error);
  ~FailedProvisionalLoadInfo();

  base::TimeDelta time_to_failed_provisional_load;
  net::Error error;
};

// Information related to whether an associated action, such as a navigation or
// an abort, was initiated by a user. Clicking a link or tapping on a UI
// element are examples of user initiation actions.
struct UserInitiatedInfo {
  static UserInitiatedInfo NotUserInitiated() {
    return UserInitiatedInfo(false, false, false);
  }

  static UserInitiatedInfo BrowserInitiated() {
    return UserInitiatedInfo(true, false, false);
  }

  static UserInitiatedInfo RenderInitiated(bool user_gesture,
                                           bool user_input_event) {
    return UserInitiatedInfo(false, user_gesture, user_input_event);
  }

  // Whether the associated action was initiated from the browser process, as
  // opposed to from the render process. We generally assume that all actions
  // initiated from the browser process are user initiated.
  bool browser_initiated;

  // Whether the associated action was initiated by a user, according to user
  // gesture tracking in content and Blink, as reported by NavigationHandle.
  bool user_gesture;

  // Whether the associated action was initiated by a user, based on our
  // heuristic-driven implementation that tests to see if there was an input
  // event that happened shortly before the given action.
  bool user_input_event;

 private:
  UserInitiatedInfo(bool browser_initiated,
                    bool user_gesture,
                    bool user_input_event)
      : browser_initiated(browser_initiated),
        user_gesture(user_gesture),
        user_input_event(user_input_event) {}
};

struct PageLoadExtraInfo {
  PageLoadExtraInfo(
      const base::Optional<base::TimeDelta>& first_background_time,
      const base::Optional<base::TimeDelta>& first_foreground_time,
      bool started_in_foreground,
      UserInitiatedInfo user_initiated_info,
      const GURL& committed_url,
      const GURL& start_url,
      UserAbortType abort_type,
      UserInitiatedInfo abort_user_initiated_info,
      const base::Optional<base::TimeDelta>& time_to_abort,
      const PageLoadMetadata& metadata);

  PageLoadExtraInfo(const PageLoadExtraInfo& other);

  ~PageLoadExtraInfo();

  // The first time that the page was backgrounded since the navigation started.
  const base::Optional<base::TimeDelta> first_background_time;

  // The first time that the page was foregrounded since the navigation started.
  const base::Optional<base::TimeDelta> first_foreground_time;

  // True if the page load started in the foreground.
  const bool started_in_foreground;

  // Whether the page load was initiated by a user.
  const UserInitiatedInfo user_initiated_info;

  // Committed URL. If the page load did not commit, |committed_url| will be
  // empty.
  const GURL committed_url;

  // The URL that started the navigation, before redirects.
  const GURL start_url;

  // The abort time and time to abort for this page load. If the page was not
  // aborted, |abort_type| will be |ABORT_NONE|.
  const UserAbortType abort_type;

  // Whether the abort for this page load was user initiated. For example, if
  // this page load was aborted by a new navigation, this field tracks whether
  // that new navigation was user-initiated. This field is only useful if this
  // page load's abort type is a value other than ABORT_NONE. Note that this
  // value is currently experimental, and is subject to change. In particular,
  // this field is not currently set for some abort types, such as stop and
  // close, since we don't yet have sufficient instrumentation to know if a stop
  // or close was caused by a user action.
  //
  // TODO(csharrison): If more metadata for aborts is needed we should provide a
  // better abstraction. Note that this is an approximation.
  UserInitiatedInfo abort_user_initiated_info;

  const base::Optional<base::TimeDelta> time_to_abort;

  // Extra information supplied to the page load metrics system from the
  // renderer.
  const PageLoadMetadata metadata;
};

// Container for various information about a request within a page load.
struct ExtraRequestInfo {
  ExtraRequestInfo(bool was_cached,
                   int64_t raw_body_bytes,
                   bool data_reduction_proxy_used,
                   int64_t original_network_content_length);

  ExtraRequestInfo(const ExtraRequestInfo& other);

  ~ExtraRequestInfo();

  // True if the resource was loaded from cache.
  const bool was_cached;

  // The number of body (not header) prefilter bytes.
  const int64_t raw_body_bytes;

  // Whether this request used Data Reduction Proxy.
  const bool data_reduction_proxy_used;

  // The number of body (not header) bytes that the data reduction proxy saw
  // before it compressed the requests.
  const int64_t original_network_content_length;
};

// Interface for PageLoadMetrics observers. All instances of this class are
// owned by the PageLoadTracker tracking a page load.
class PageLoadMetricsObserver {
 public:
  // ObservePolicy is used as a return value on some PageLoadMetricsObserver
  // callbacks to indicate whether the observer would like to continue observing
  // metric callbacks. Observers that wish to continue observing metric
  // callbacks should return CONTINUE_OBSERVING; observers that wish to stop
  // observing callbacks should return STOP_OBSERVING. Observers that return
  // STOP_OBSERVING may be deleted.
  enum ObservePolicy {
    CONTINUE_OBSERVING,
    STOP_OBSERVING,
  };

  virtual ~PageLoadMetricsObserver() {}

  // The page load started, with the given navigation handle.
  // currently_committed_url contains the URL of the committed page load at the
  // time the navigation for navigation_handle was initiated, or the empty URL
  // if there was no committed page load at the time the navigation was
  // initiated.
  virtual ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                                const GURL& currently_committed_url,
                                bool started_in_foreground);

  // OnRedirect is triggered when a page load redirects to another URL.
  // The navigation handle holds relevant data for the navigation, but will
  // be destroyed soon after this call. Don't hold a reference to it. This can
  // be called multiple times.
  virtual ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle);

  // OnCommit is triggered when a page load commits, i.e. when we receive the
  // first data for the request. The navigation handle holds relevant data for
  // the navigation, but will be destroyed soon after this call. Don't hold a
  // reference to it.
  // Observers that return STOP_OBSERVING will not receive any additional
  // callbacks, and will be deleted after invocation of this method returns.
  virtual ObservePolicy OnCommit(content::NavigationHandle* navigation_handle);

  // OnHidden is triggered when a page leaves the foreground. It does not fire
  // when a foreground page is permanently closed; for that, listen to
  // OnComplete instead.
  virtual ObservePolicy OnHidden(const PageLoadTiming& timing,
                                 const PageLoadExtraInfo& extra_info);

  // OnShown is triggered when a page is brought to the foreground. It does not
  // fire when the page first loads; for that, listen for OnStart instead.
  virtual ObservePolicy OnShown();

  // The callbacks below are only invoked after a navigation commits, for
  // tracked page loads. Page loads that don't meet the criteria for being
  // tracked at the time a navigation commits will not receive any of the
  // callbacks below.

  // OnTimingUpdate is triggered when an updated PageLoadTiming is
  // available. This method may be called multiple times over the course of the
  // page load. This method is currently only intended for use in testing. Most
  // implementers should implement one of the On* callbacks, such as
  // OnFirstContentfulPaint or OnDomContentLoadedEventStart. Please email
  // loading-dev@chromium.org if you intend to override this method.
  virtual void OnTimingUpdate(const PageLoadTiming& timing,
                              const PageLoadExtraInfo& extra_info) {}
  // OnUserInput is triggered when a new user input is passed in to
  // web_contents. Contains a TimeDelta from navigation start.
  virtual void OnUserInput(const blink::WebInputEvent& event) {}

  // The following methods are invoked at most once, when the timing for the
  // associated event first becomes available.
  virtual void OnDomContentLoadedEventStart(
      const PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info) {}
  virtual void OnLoadEventStart(const PageLoadTiming& timing,
                                const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstLayout(const PageLoadTiming& timing,
                             const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstPaint(const PageLoadTiming& timing,
                            const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstTextPaint(const PageLoadTiming& timing,
                                const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstImagePaint(const PageLoadTiming& timing,
                                 const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstContentfulPaint(const PageLoadTiming& timing,
                                      const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstMeaningfulPaint(const PageLoadTiming& timing,
                                      const PageLoadExtraInfo& extra_info) {}
  virtual void OnParseStart(const PageLoadTiming& timing,
                            const PageLoadExtraInfo& extra_info) {}
  virtual void OnParseStop(const PageLoadTiming& timing,
                           const PageLoadExtraInfo& extra_info) {}

  // Invoked when there is a change in PageLoadMetadata's behavior_flags.
  virtual void OnLoadingBehaviorObserved(
      const page_load_metrics::PageLoadExtraInfo& extra_info) {}

  // Invoked when the UMA metrics subsystem is persisting metrics as the
  // application goes into the background, on platforms where the browser
  // process may be killed after backgrounding (Android). Implementers should
  // persist any metrics that have been buffered in memory in this callback, as
  // the application may be killed at any time after this method is invoked
  // without further notification. Note that this may be called both for
  // provisional loads as well as committed loads. Implementations that only
  // want to track committed loads should check whether extra_info.committed_url
  // is empty to determine if the load had committed. If the implementation
  // returns CONTINUE_OBSERVING, this method may be called multiple times per
  // observer, once for each time that the application enters the backround.
  //
  // The default implementation does nothing, and returns CONTINUE_OBSERVING.
  virtual ObservePolicy FlushMetricsOnAppEnterBackground(
      const PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info);

  // One of OnComplete or OnFailedProvisionalLoad is invoked for tracked page
  // loads, immediately before the observer is deleted. These callbacks will not
  // be invoked for page loads that did not meet the criteria for being tracked
  // at the time the navigation completed. The PageLoadTiming struct contains
  // timing data and the PageLoadExtraInfo struct contains other useful data
  // collected over the course of the page load. Most observers should not need
  // to implement these callbacks, and should implement the On* timing callbacks
  // instead.

  // OnComplete is invoked for tracked page loads that committed, immediately
  // before the observer is deleted. Observers that implement OnComplete may
  // also want to implement FlushMetricsOnAppEnterBackground, to avoid loss of
  // data if the application is killed while in the background (this happens
  // frequently on Android).
  virtual void OnComplete(const PageLoadTiming& timing,
                          const PageLoadExtraInfo& extra_info) {}

  // OnFailedProvisionalLoad is invoked for tracked page loads that did not
  // commit, immediately before the observer is deleted.
  virtual void OnFailedProvisionalLoad(
      const FailedProvisionalLoadInfo& failed_provisional_load_info,
      const PageLoadExtraInfo& extra_info) {}

  // Called whenever a request is loaded for this page load.
  virtual void OnLoadedResource(const ExtraRequestInfo& extra_request_info) {}
};

}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_OBSERVER_H_
