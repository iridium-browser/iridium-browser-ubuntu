// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/page_load_metrics/browser_page_track_decider.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/page_load_metrics_messages.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    page_load_metrics::MetricsWebContentsObserver);

namespace page_load_metrics {

namespace {

UserInitiatedInfo CreateUserInitiatedInfo(
    content::NavigationHandle* navigation_handle,
    PageLoadTracker* committed_load) {
  if (!navigation_handle->IsRendererInitiated())
    return UserInitiatedInfo::BrowserInitiated();

  return UserInitiatedInfo::RenderInitiated(
      navigation_handle->HasUserGesture(),
      committed_load &&
          committed_load->input_tracker()->FindAndConsumeInputEventsBefore(
              navigation_handle->NavigationStart()));
}

}  // namespace

// static
MetricsWebContentsObserver::MetricsWebContentsObserver(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface)
    : content::WebContentsObserver(web_contents),
      in_foreground_(false),
      embedder_interface_(std::move(embedder_interface)),
      has_navigated_(false) {
  RegisterInputEventObserver(web_contents->GetRenderViewHost());
}

MetricsWebContentsObserver* MetricsWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface) {
  DCHECK(web_contents);

  MetricsWebContentsObserver* metrics = FromWebContents(web_contents);
  if (!metrics) {
    metrics = new MetricsWebContentsObserver(web_contents,
                                             std::move(embedder_interface));
    web_contents->SetUserData(UserDataKey(), metrics);
  }
  return metrics;
}

MetricsWebContentsObserver::~MetricsWebContentsObserver() {
  // TODO(csharrison): Use a more user-initiated signal for CLOSE.
  NotifyAbortAllLoads(ABORT_CLOSE, UserInitiatedInfo::NotUserInitiated());
}

void MetricsWebContentsObserver::RegisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->AddInputEventObserver(this);
}

void MetricsWebContentsObserver::UnregisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->RemoveInputEventObserver(this);
}

void MetricsWebContentsObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  UnregisterInputEventObserver(old_host);
  RegisterInputEventObserver(new_host);
}

bool MetricsWebContentsObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MetricsWebContentsObserver, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(PageLoadMetricsMsg_TimingUpdated, OnTimingUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MetricsWebContentsObserver::WillStartNavigationRequest(
    content::NavigationHandle* navigation_handle) {
  // Same-page navigations should never go through WillStartNavigationRequest.
  DCHECK(!navigation_handle->IsSamePage());

  if (!navigation_handle->IsInMainFrame())
    return;

  UserInitiatedInfo user_initiated_info(
      CreateUserInitiatedInfo(navigation_handle, committed_load_.get()));
  std::unique_ptr<PageLoadTracker> last_aborted =
      NotifyAbortedProvisionalLoadsNewNavigation(navigation_handle,
                                                 user_initiated_info);

  int chain_size_same_url = 0;
  int chain_size = 0;
  if (last_aborted) {
    if (last_aborted->MatchesOriginalNavigation(navigation_handle)) {
      chain_size_same_url = last_aborted->aborted_chain_size_same_url() + 1;
    } else if (last_aborted->aborted_chain_size_same_url() > 0) {
      LogAbortChainSameURLHistogram(
          last_aborted->aborted_chain_size_same_url());
    }
    chain_size = last_aborted->aborted_chain_size() + 1;
  }

  if (!ShouldTrackNavigation(navigation_handle))
    return;

  // Pass in the last committed url to the PageLoadTracker. If the MWCO has
  // never observed a committed load, use the last committed url from this
  // WebContent's opener. This is more accurate than using referrers due to
  // referrer sanitizing and origin referrers. Note that this could potentially
  // be inaccurate if the opener has since navigated.
  content::WebContents* opener = web_contents()->GetOpener();
  const GURL& opener_url =
      !has_navigated_ && opener
          ? web_contents()->GetOpener()->GetLastCommittedURL()
          : GURL::EmptyGURL();
  const GURL& currently_committed_url =
      committed_load_ ? committed_load_->committed_url() : opener_url;
  has_navigated_ = true;

  // We can have two provisional loads in some cases. E.g. a same-site
  // navigation can have a concurrent cross-process navigation started
  // from the omnibox.
  DCHECK_GT(2ul, provisional_loads_.size());
  // Passing raw pointers to observers_ and embedder_interface_ is safe because
  // the MetricsWebContentsObserver owns them both list and they are torn down
  // after the PageLoadTracker. The PageLoadTracker does not hold on to
  // committed_load_ or navigation_handle beyond the scope of the constructor.
  provisional_loads_.insert(std::make_pair(
      navigation_handle,
      base::MakeUnique<PageLoadTracker>(
          in_foreground_, embedder_interface_.get(), currently_committed_url,
          navigation_handle, user_initiated_info, chain_size,
          chain_size_same_url)));
}

void MetricsWebContentsObserver::WillProcessNavigationResponse(
    content::NavigationHandle* navigation_handle) {
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end())
    return;
  it->second->WillProcessNavigationResponse(navigation_handle);
}

PageLoadTracker* MetricsWebContentsObserver::GetTrackerOrNullForRequest(
    const content::GlobalRequestID& request_id,
    content::ResourceType resource_type,
    base::TimeTicks creation_time) {
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    // The main frame request can complete either before or after commit, so we
    // look at both provisional loads and the committed load to find a
    // PageLoadTracker with a matching request id. See https://goo.gl/6TzCYN for
    // more details.
    for (const auto& kv : provisional_loads_) {
      PageLoadTracker* candidate = kv.second.get();
      if (candidate->HasMatchingNavigationRequestID(request_id)) {
        return candidate;
      }
    }
    if (committed_load_ &&
        committed_load_->HasMatchingNavigationRequestID(request_id)) {
      return committed_load_.get();
    }
  } else {
    // Non main frame resources are always associated with the currently
    // committed load. If the resource request was started before this
    // navigation then it should be ignored.

    // TODO(jkarlin): There is a race here. Consider the following sequence:
    // 1. renderer has a committed page A
    // 2. navigation is initiated to page B
    // 3. page A initiates URLRequests (e.g. in the unload handler)
    // 4. page B commits
    // 5. the URLRequests initiated by A complete
    // In the above example, the URLRequests initiated by A will be attributed
    // to page load B. This should be relatively rare but we may want to fix
    // this at some point. We could fix this by comparing the URLRequest
    // creation time against the committed load's commit time, however more
    // investigation is needed to confirm that all cases would be handled
    // correctly (for example Link: preloads).
    if (committed_load_ &&
        creation_time >= committed_load_->navigation_start()) {
      return committed_load_.get();
    }
  }
  return nullptr;
}

void MetricsWebContentsObserver::OnRequestComplete(
    const content::GlobalRequestID& request_id,
    content::ResourceType resource_type,
    bool was_cached,
    bool used_data_reduction_proxy,
    int64_t raw_body_bytes,
    int64_t original_content_length,
    base::TimeTicks creation_time) {
  PageLoadTracker* tracker =
      GetTrackerOrNullForRequest(request_id, resource_type, creation_time);
  if (tracker) {
    ExtraRequestInfo extra_request_info(
        was_cached, raw_body_bytes, used_data_reduction_proxy,
        was_cached ? 0 : original_content_length);
    tracker->OnLoadedResource(extra_request_info);
  }
}

const PageLoadExtraInfo
MetricsWebContentsObserver::GetPageLoadExtraInfoForCommittedLoad() {
  DCHECK(committed_load_);
  return committed_load_->ComputePageLoadExtraInfo();
}

void MetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  std::unique_ptr<PageLoadTracker> finished_nav(
      std::move(provisional_loads_[navigation_handle]));
  provisional_loads_.erase(navigation_handle);

  // Ignore same-page navigations.
  if (navigation_handle->HasCommitted() && navigation_handle->IsSamePage()) {
    if (finished_nav)
      finished_nav->StopTracking();
    return;
  }

  // Ignore internally generated aborts for navigations with HTTP responses that
  // don't commit, such as HTTP 204 responses and downloads.
  if (!navigation_handle->HasCommitted() &&
      navigation_handle->GetNetErrorCode() == net::ERR_ABORTED &&
      navigation_handle->GetResponseHeaders()) {
    if (finished_nav)
      finished_nav->StopTracking();
    return;
  }

  const bool should_track =
      finished_nav && ShouldTrackNavigation(navigation_handle);

  if (finished_nav && !should_track)
    finished_nav->StopTracking();

  if (navigation_handle->HasCommitted()) {
    UserInitiatedInfo user_initiated_info =
        finished_nav
            ? finished_nav->user_initiated_info()
            : CreateUserInitiatedInfo(navigation_handle, committed_load_.get());

    // Notify other loads that they may have been aborted by this committed
    // load. is_certainly_browser_timestamp is set to false because
    // NavigationStart() could be set in either the renderer or browser process.
    NotifyAbortAllLoadsWithTimestamp(
        AbortTypeForPageTransition(navigation_handle->GetPageTransition()),
        user_initiated_info, navigation_handle->NavigationStart(), false);

    if (should_track) {
      HandleCommittedNavigationForTrackedLoad(navigation_handle,
                                              std::move(finished_nav));
    } else {
      committed_load_.reset();
    }
  } else if (should_track) {
    HandleFailedNavigationForTrackedLoad(navigation_handle,
                                         std::move(finished_nav));
  }
}

// Handle a pre-commit error. Navigations that result in an error page will be
// ignored.
void MetricsWebContentsObserver::HandleFailedNavigationForTrackedLoad(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PageLoadTracker> tracker) {
  tracker->FailedProvisionalLoad(navigation_handle);

  net::Error error = navigation_handle->GetNetErrorCode();

  // net::OK: This case occurs when the NavigationHandle finishes and reports
  // !HasCommitted(), but reports no net::Error. This should not occur
  // pre-PlzNavigate, but afterwards it should represent the navigation stopped
  // by the user before it was ready to commit.
  // net::ERR_ABORTED: An aborted provisional load has error
  // net::ERR_ABORTED.
  if ((error == net::OK) || (error == net::ERR_ABORTED)) {
    tracker->NotifyAbort(ABORT_OTHER, UserInitiatedInfo::NotUserInitiated(),
                         base::TimeTicks::Now(), true);
    aborted_provisional_loads_.push_back(std::move(tracker));
  }
}

void MetricsWebContentsObserver::HandleCommittedNavigationForTrackedLoad(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PageLoadTracker> tracker) {
  if (!IsNavigationUserInitiated(navigation_handle) &&
      (navigation_handle->GetPageTransition() &
       ui::PAGE_TRANSITION_CLIENT_REDIRECT) != 0 &&
      committed_load_) {
    // TODO(bmcquade): consider carrying the user_gesture bit forward to the
    // redirected navigation.
    committed_load_->NotifyClientRedirectTo(*tracker);
  }

  committed_load_ = std::move(tracker);
  committed_load_->Commit(navigation_handle);
}

void MetricsWebContentsObserver::NavigationStopped() {
  // TODO(csharrison): Use a more user-initiated signal for STOP.
  NotifyAbortAllLoads(ABORT_STOP, UserInitiatedInfo::NotUserInitiated());
}

void MetricsWebContentsObserver::OnInputEvent(
    const blink::WebInputEvent& event) {
  // Ignore browser navigation or reload which comes with type Undefined.
  if (event.type() == blink::WebInputEvent::Type::Undefined)
    return;

  if (committed_load_)
    committed_load_->OnInputEvent(event);
}

void MetricsWebContentsObserver::FlushMetricsOnAppEnterBackground() {
  // Note that, while a call to FlushMetricsOnAppEnterBackground usually
  // indicates that the app is about to be backgrounded, there are cases where
  // the app may not end up getting backgrounded. Thus, we should not assume
  // anything about foreground / background state of the associated tab as part
  // of this method call.

  if (committed_load_)
    committed_load_->FlushMetricsOnAppEnterBackground();
  for (const auto& kv : provisional_loads_) {
    kv.second->FlushMetricsOnAppEnterBackground();
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    tracker->FlushMetricsOnAppEnterBackground();
  }
}

void MetricsWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end())
    return;
  it->second->Redirect(navigation_handle);
}

void MetricsWebContentsObserver::WasShown() {
  if (in_foreground_)
    return;
  in_foreground_ = true;
  if (committed_load_)
    committed_load_->WebContentsShown();
  for (const auto& kv : provisional_loads_) {
    kv.second->WebContentsShown();
  }
}

void MetricsWebContentsObserver::WasHidden() {
  if (!in_foreground_)
    return;
  in_foreground_ = false;
  if (committed_load_)
    committed_load_->WebContentsHidden();
  for (const auto& kv : provisional_loads_) {
    kv.second->WebContentsHidden();
  }
}

// This will occur when the process for the main RenderFrameHost exits, either
// normally or from a crash. We eagerly log data from the last committed load if
// we have one. Don't notify aborts here because this is probably not user
// initiated. If it is (e.g. browser shutdown), other code paths will take care
// of notifying.
void MetricsWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  // Other code paths will be run for normal renderer shutdown. Note that we
  // sometimes get the STILL_RUNNING value on fast shutdown.
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }

  // If this is a crash, eagerly log the aborted provisional loads and the
  // committed load. |provisional_loads_| don't need to be destroyed here
  // because their lifetime is tied to the NavigationHandle.
  committed_load_.reset();
  aborted_provisional_loads_.clear();
}

void MetricsWebContentsObserver::NotifyAbortAllLoads(
    UserAbortType abort_type,
    UserInitiatedInfo user_initiated_info) {
  NotifyAbortAllLoadsWithTimestamp(abort_type, user_initiated_info,
                                   base::TimeTicks::Now(), true);
}

void MetricsWebContentsObserver::NotifyAbortAllLoadsWithTimestamp(
    UserAbortType abort_type,
    UserInitiatedInfo user_initiated_info,
    base::TimeTicks timestamp,
    bool is_certainly_browser_timestamp) {
  if (committed_load_) {
    committed_load_->NotifyAbort(abort_type, user_initiated_info, timestamp,
                                 is_certainly_browser_timestamp);
  }
  for (const auto& kv : provisional_loads_) {
    kv.second->NotifyAbort(abort_type, user_initiated_info, timestamp,
                           is_certainly_browser_timestamp);
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    if (tracker->IsLikelyProvisionalAbort(timestamp)) {
      tracker->UpdateAbort(abort_type, user_initiated_info, timestamp,
                           is_certainly_browser_timestamp);
    }
  }
  aborted_provisional_loads_.clear();
}

std::unique_ptr<PageLoadTracker>
MetricsWebContentsObserver::NotifyAbortedProvisionalLoadsNewNavigation(
    content::NavigationHandle* new_navigation,
    UserInitiatedInfo user_initiated_info) {
  // If there are multiple aborted loads that can be attributed to this one,
  // just count the latest one for simplicity. Other loads will fall into the
  // OTHER bucket, though there shouldn't be very many.
  if (aborted_provisional_loads_.size() == 0)
    return nullptr;
  if (aborted_provisional_loads_.size() > 1)
    RecordInternalError(ERR_NAVIGATION_SIGNALS_MULIPLE_ABORTED_LOADS);

  std::unique_ptr<PageLoadTracker> last_aborted_load =
      std::move(aborted_provisional_loads_.back());
  aborted_provisional_loads_.pop_back();

  base::TimeTicks timestamp = new_navigation->NavigationStart();
  if (last_aborted_load->IsLikelyProvisionalAbort(timestamp)) {
    last_aborted_load->UpdateAbort(
        AbortTypeForPageTransition(new_navigation->GetPageTransition()),
        user_initiated_info, timestamp, false);
  }

  aborted_provisional_loads_.clear();
  return last_aborted_load;
}

void MetricsWebContentsObserver::OnTimingUpdated(
    content::RenderFrameHost* render_frame_host,
    const PageLoadTiming& timing,
    const PageLoadMetadata& metadata) {
  // We may receive notifications from frames that have been navigated away
  // from. We simply ignore them.
  if (render_frame_host != web_contents()->GetMainFrame()) {
    RecordInternalError(ERR_IPC_FROM_WRONG_FRAME);
    return;
  }

  // While timings arriving for the wrong frame are expected, we do not expect
  // any of the errors below. Thus, we track occurrences of all errors below,
  // rather than returning early after encountering an error.

  bool error = false;
  if (!committed_load_) {
    RecordInternalError(ERR_IPC_WITH_NO_RELEVANT_LOAD);
    error = true;
  }

  if (!web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    RecordInternalError(ERR_IPC_FROM_BAD_URL_SCHEME);
    error = true;
  }

  if (error)
    return;

  if (!committed_load_->UpdateTiming(timing, metadata)) {
    // If the page load tracker cannot update its timing, something is wrong
    // with the IPC (it's from another load, or it's invalid in some other way).
    // We expect this to be a rare occurrence.
    RecordInternalError(ERR_BAD_TIMING_IPC);
  }
}

bool MetricsWebContentsObserver::ShouldTrackNavigation(
    content::NavigationHandle* navigation_handle) const {
  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(!navigation_handle->HasCommitted() ||
         !navigation_handle->IsSamePage());

  return BrowserPageTrackDecider(embedder_interface_.get(), web_contents(),
                                 navigation_handle).ShouldTrack();
}

}  // namespace page_load_metrics
