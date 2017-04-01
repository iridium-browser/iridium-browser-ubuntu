// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/issues_observer.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"
#include "chrome/browser/media/router/mojo/media_router_type_converters.h"
#include "chrome/browser/media/router/route_message.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/process_manager.h"

#define DVLOG_WITH_INSTANCE(level) \
  DVLOG(level) << "MR #" << instance_id_ << ": "

#define DLOG_WITH_INSTANCE(level) DLOG(level) << "MR #" << instance_id_ << ": "

namespace media_router {

namespace {

void RunRouteRequestCallbacks(
    std::unique_ptr<RouteRequestResult> result,
    const std::vector<MediaRouteResponseCallback>& callbacks) {
  for (const MediaRouteResponseCallback& callback : callbacks)
    callback.Run(*result);
}

}  // namespace

using SinkAvailability = mojom::MediaRouter::SinkAvailability;

MediaRouterMojoImpl::MediaRoutesQuery::MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaRoutesQuery::~MediaRoutesQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaSinksQuery::~MediaSinksQuery() = default;

MediaRouterMojoImpl::MediaRouterMojoImpl(
    extensions::EventPageTracker* event_page_tracker)
    : event_page_tracker_(event_page_tracker),
      instance_id_(base::GenerateGUID()),
      availability_(mojom::MediaRouter::SinkAvailability::UNAVAILABLE),
      current_wake_reason_(MediaRouteProviderWakeReason::TOTAL_COUNT),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(event_page_tracker_);
#if defined(OS_WIN)
  CanFirewallUseLocalPorts(
      base::Bind(&MediaRouterMojoImpl::OnFirewallCheckComplete,
                 weak_factory_.GetWeakPtr()));
#endif
}

MediaRouterMojoImpl::~MediaRouterMojoImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

// static
void MediaRouterMojoImpl::BindToRequest(
    const extensions::Extension* extension,
    content::BrowserContext* context,
    mojo::InterfaceRequest<mojom::MediaRouter> request) {
  MediaRouterMojoImpl* impl = static_cast<MediaRouterMojoImpl*>(
      MediaRouterFactory::GetApiForBrowserContext(context));
  DCHECK(impl);

  impl->BindToMojoRequest(std::move(request), *extension);
}

void MediaRouterMojoImpl::BindToMojoRequest(
    mojo::InterfaceRequest<mojom::MediaRouter> request,
    const extensions::Extension& extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  binding_.reset(
      new mojo::Binding<mojom::MediaRouter>(this, std::move(request)));
  binding_->set_connection_error_handler(base::Bind(
      &MediaRouterMojoImpl::OnConnectionError, base::Unretained(this)));

  media_route_provider_extension_id_ = extension.id();
  if (!provider_version_was_recorded_) {
    MediaRouterMojoMetrics::RecordMediaRouteProviderVersion(extension);
    provider_version_was_recorded_ = true;
  }
}

void MediaRouterMojoImpl::OnConnectionError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  media_route_provider_.reset();
  binding_.reset();

  // If |OnConnectionError| is invoked while there are pending requests, then
  // it means we tried to wake the extension, but weren't able to complete the
  // connection to media route provider. Since we do not know whether the error
  // is transient, reattempt the wakeup.
  if (!pending_requests_.empty()) {
    DLOG_WITH_INSTANCE(ERROR) << "A connection error while there are pending "
                                 "requests.";
    SetWakeReason(MediaRouteProviderWakeReason::CONNECTION_ERROR);
    AttemptWakeEventPage();
  }
}

void MediaRouterMojoImpl::RegisterMediaRouteProvider(
    mojom::MediaRouteProviderPtr media_route_provider_ptr,
    const mojom::MediaRouter::RegisterMediaRouteProviderCallback&
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(OS_WIN)
  // The MRPM may have been upgraded or otherwise reload such that we could be
  // seeing an MRPM that doesn't know mDNS is enabled, even if we've told a
  // previously registered MRPM it should be enabled. Furthermore, there may be
  // a pending request to enable mDNS, so don't clear this flag after
  // ExecutePendingRequests().
  is_mdns_enabled_ = false;
#endif
  if (event_page_tracker_->IsEventPageSuspended(
          media_route_provider_extension_id_)) {
    DVLOG_WITH_INSTANCE(1)
        << "RegisterMediaRouteProvider was called while extension is "
           "suspended.";
    media_route_provider_.reset();
    SetWakeReason(MediaRouteProviderWakeReason::REGISTER_MEDIA_ROUTE_PROVIDER);
    AttemptWakeEventPage();
    return;
  }

  media_route_provider_ = std::move(media_route_provider_ptr);
  media_route_provider_.set_connection_error_handler(base::Bind(
      &MediaRouterMojoImpl::OnConnectionError, base::Unretained(this)));
  callback.Run(instance_id_);
  ExecutePendingRequests();
  wakeup_attempt_count_ = 0;
#if defined(OS_WIN)
  // The MRPM extension already turns on mDNS discovery for platforms other than
  // Windows. It only relies on this signalling from MR on Windows to avoid
  // triggering a firewall prompt out of the context of MR from the user's
  // perspective. This particular call reminds the extension to enable mDNS
  // discovery when it wakes up, has been upgraded, etc.
  if (should_enable_mdns_discovery_) {
    DoEnsureMdnsDiscoveryEnabled();
  }
#endif
}

void MediaRouterMojoImpl::OnIssue(const IssueInfo& issue) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG_WITH_INSTANCE(1) << "OnIssue " << issue.title;
  issue_manager_.AddIssue(issue);
}

void MediaRouterMojoImpl::OnSinksReceived(
    const std::string& media_source,
    std::vector<mojom::MediaSinkPtr> sinks,
    const std::vector<std::string>& origins) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG_WITH_INSTANCE(1) << "OnSinksReceived";
  auto it = sinks_queries_.find(media_source);
  if (it == sinks_queries_.end()) {
    DVLOG_WITH_INSTANCE(1) << "Received sink list without MediaSinksQuery.";
    return;
  }

  std::vector<GURL> origin_list;
  origin_list.reserve(origins.size());
  for (size_t i = 0; i < origins.size(); ++i) {
    GURL origin(origins[i]);
    if (!origin.is_valid()) {
      LOG(WARNING) << "Received invalid origin: " << origin
                   << ". Dropping result.";
      return;
    }
    origin_list.push_back(origin);
  }

  std::vector<MediaSink> sink_list;
  sink_list.reserve(sinks.size());
  for (size_t i = 0; i < sinks.size(); ++i)
    sink_list.push_back(sinks[i].To<MediaSink>());

  auto* sinks_query = it->second.get();
  sinks_query->has_cached_result = true;
  sinks_query->origins.swap(origin_list);
  sinks_query->cached_sink_list.swap(sink_list);

  if (!sinks_query->observers.might_have_observers()) {
    DVLOG_WITH_INSTANCE(1)
        << "Received sink list without any active observers: " << media_source;
  } else {
    for (auto& observer : sinks_query->observers) {
      observer.OnSinksUpdated(sinks_query->cached_sink_list,
                              sinks_query->origins);
    }
  }
}

void MediaRouterMojoImpl::OnRoutesUpdated(
    std::vector<mojom::MediaRoutePtr> routes,
    const std::string& media_source,
    const std::vector<std::string>& joinable_route_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG_WITH_INSTANCE(1) << "OnRoutesUpdated";
  auto it = routes_queries_.find(media_source);
  if (it == routes_queries_.end() ||
      !(it->second->observers.might_have_observers())) {
    DVLOG_WITH_INSTANCE(1)
        << "Received route list without any active observers: " << media_source;
    return;
  }

  std::vector<MediaRoute> routes_converted;
  routes_converted.reserve(routes.size());
  for (size_t i = 0; i < routes.size(); ++i)
    routes_converted.push_back(routes[i].To<MediaRoute>());

  for (auto& observer : it->second->observers)
    observer.OnRoutesUpdated(routes_converted, joinable_route_ids);
}

void MediaRouterMojoImpl::RouteResponseReceived(
    const std::string& presentation_id,
    bool is_incognito,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    bool is_join,
    mojom::MediaRoutePtr media_route,
    const base::Optional<std::string>& error_text,
    mojom::RouteRequestResultCode result_code) {
  std::unique_ptr<RouteRequestResult> result;
  if (media_route.is_null()) {
    // An error occurred.
    const std::string& error = (error_text && !error_text->empty())
        ? *error_text : std::string("Unknown error.");
    result = RouteRequestResult::FromError(
        error, mojo::RouteRequestResultCodeFromMojo(result_code));
  } else if (media_route->is_incognito != is_incognito) {
    std::string error = base::StringPrintf(
        "Mismatch in incognito status: request = %d, response = %d",
        is_incognito, media_route->is_incognito);
    result = RouteRequestResult::FromError(
        error, RouteRequestResult::INCOGNITO_MISMATCH);
  } else {
    result = RouteRequestResult::FromSuccess(
        media_route.To<std::unique_ptr<MediaRoute>>(), presentation_id);
  }

  if (is_join)
    MediaRouterMojoMetrics::RecordJoinRouteResultCode(result->result_code());
  else
    MediaRouterMojoMetrics::RecordCreateRouteResultCode(result->result_code());

  RunRouteRequestCallbacks(std::move(result), callbacks);
}

void MediaRouterMojoImpl::CreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Invalid origin", RouteRequestResult::INVALID_ORIGIN);
    MediaRouterMojoMetrics::RecordCreateRouteResultCode(result->result_code());
    RunRouteRequestCallbacks(std::move(result), callbacks);
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::CREATE_ROUTE);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoCreateRoute,
                        base::Unretained(this), source_id, sink_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks, timeout, incognito));
}

void MediaRouterMojoImpl::JoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<RouteRequestResult> error_result;
  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    error_result = RouteRequestResult::FromError(
        "Invalid origin", RouteRequestResult::INVALID_ORIGIN);
  } else if (!HasJoinableRoute()) {
    DVLOG_WITH_INSTANCE(1) << "No joinable routes";
    error_result = RouteRequestResult::FromError(
        "Route not found", RouteRequestResult::ROUTE_NOT_FOUND);
  }

  if (error_result) {
    MediaRouterMojoMetrics::RecordJoinRouteResultCode(
        error_result->result_code());
    RunRouteRequestCallbacks(std::move(error_result), callbacks);
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::JOIN_ROUTE);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoJoinRoute,
                        base::Unretained(this), source_id, presentation_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks, timeout, incognito));
}

void MediaRouterMojoImpl::ConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const GURL& origin,
    content::WebContents* web_contents,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!origin.is_valid()) {
    DVLOG_WITH_INSTANCE(1) << "Invalid origin: " << origin;
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Invalid origin", RouteRequestResult::INVALID_ORIGIN);
    MediaRouterMojoMetrics::RecordJoinRouteResultCode(result->result_code());
    RunRouteRequestCallbacks(std::move(result), callbacks);
    return;
  }

  SetWakeReason(MediaRouteProviderWakeReason::CONNECT_ROUTE_BY_ROUTE_ID);
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoConnectRouteByRouteId,
                        base::Unretained(this), source_id, route_id,
                        origin.is_empty() ? "" : origin.spec(), tab_id,
                        callbacks, timeout, incognito));
}

void MediaRouterMojoImpl::TerminateRoute(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(2) << "TerminateRoute " << route_id;
  SetWakeReason(MediaRouteProviderWakeReason::TERMINATE_ROUTE);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoTerminateRoute,
                        base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::DetachRoute(const MediaRoute::Id& route_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SetWakeReason(MediaRouteProviderWakeReason::DETACH_ROUTE);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoDetachRoute,
                        base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::SendRouteMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const SendRouteMessageCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SetWakeReason(MediaRouteProviderWakeReason::SEND_SESSION_MESSAGE);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoSendSessionMessage,
                        base::Unretained(this), route_id, message, callback));
}

void MediaRouterMojoImpl::SendRouteBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data,
    const SendRouteMessageCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SetWakeReason(MediaRouteProviderWakeReason::SEND_SESSION_BINARY_MESSAGE);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoSendSessionBinaryMessage,
                        base::Unretained(this), route_id,
                        base::Passed(std::move(data)), callback));
}

void MediaRouterMojoImpl::AddIssue(const IssueInfo& issue_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issue_manager_.AddIssue(issue_info);
}

void MediaRouterMojoImpl::ClearIssue(const Issue::Id& issue_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issue_manager_.ClearIssue(issue_id);
}

void MediaRouterMojoImpl::OnUserGesture() {
  // Allow MRPM to intelligently update sinks and observers by passing in a
  // media source.
  UpdateMediaSinks(MediaSourceForDesktop().id());

#if defined(OS_WIN)
  EnsureMdnsDiscoveryEnabled();
#endif
}

void MediaRouterMojoImpl::SearchSinks(
    const MediaSink::Id& sink_id,
    const MediaSource::Id& source_id,
    const std::string& search_input,
    const std::string& domain,
    const MediaSinkSearchResponseCallback& sink_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SetWakeReason(MediaRouteProviderWakeReason::SEARCH_SINKS);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoSearchSinks,
                        base::Unretained(this), sink_id, source_id,
                        search_input, domain, sink_callback));
}

bool MediaRouterMojoImpl::RegisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Create an observer list for the media source and add |observer|
  // to it. Fail if |observer| is already registered.
  const std::string& source_id = observer->source().id();
  std::unique_ptr<MediaSinksQuery>& sinks_query = sinks_queries_[source_id];
  bool new_query = false;
  if (!sinks_query) {
    new_query = true;
    sinks_query = base::MakeUnique<MediaSinksQuery>();
  } else {
    DCHECK(!sinks_query->observers.HasObserver(observer));
  }

  // If sink availability is UNAVAILABLE, then there is no need to call MRPM.
  // |observer| can be immediately notified with an empty list.
  sinks_query->observers.AddObserver(observer);
  if (availability_ == mojom::MediaRouter::SinkAvailability::UNAVAILABLE) {
    observer->OnSinksUpdated(std::vector<MediaSink>(), std::vector<GURL>());
  } else {
    // Need to call MRPM to start observing sinks if the query is new.
    if (new_query) {
      SetWakeReason(MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_SINKS);
      RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaSinks,
                            base::Unretained(this), source_id));
    } else if (sinks_query->has_cached_result) {
      observer->OnSinksUpdated(sinks_query->cached_sink_list,
                               sinks_query->origins);
    }
  }
  return true;
}

void MediaRouterMojoImpl::UnregisterMediaSinksObserver(
    MediaSinksObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const MediaSource::Id& source_id = observer->source().id();
  auto it = sinks_queries_.find(source_id);
  if (it == sinks_queries_.end() ||
      !it->second->observers.HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing sinks for it.
  // might_have_observers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  it->second->observers.RemoveObserver(observer);
  if (!it->second->observers.might_have_observers()) {
    // Only ask MRPM to stop observing media sinks if the availability is not
    // UNAVAILABLE.
    // Otherwise, the MRPM would have discarded the queries already.
    if (availability_ !=
        mojom::MediaRouter::SinkAvailability::UNAVAILABLE) {
      SetWakeReason(MediaRouteProviderWakeReason::STOP_OBSERVING_MEDIA_SINKS);
      // The |sinks_queries_| entry will be removed in the immediate or deferred
      // |DoStopObservingMediaSinks| call.
      RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStopObservingMediaSinks,
                            base::Unretained(this), source_id));
    } else {
      sinks_queries_.erase(source_id);
    }
  }
}

void MediaRouterMojoImpl::RegisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const MediaSource::Id source_id = observer->source_id();
  auto& routes_query = routes_queries_[source_id];
  if (!routes_query) {
    routes_query = base::MakeUnique<MediaRoutesQuery>();
  } else {
    DCHECK(!routes_query->observers.HasObserver(observer));
  }

  routes_query->observers.AddObserver(observer);
  SetWakeReason(MediaRouteProviderWakeReason::START_OBSERVING_MEDIA_ROUTES);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaRoutes,
                        base::Unretained(this), source_id));
}

void MediaRouterMojoImpl::UnregisterMediaRoutesObserver(
    MediaRoutesObserver* observer) {
  const MediaSource::Id source_id = observer->source_id();
  auto it = routes_queries_.find(source_id);
  if (it == routes_queries_.end() ||
      !it->second->observers.HasObserver(observer)) {
    return;
  }

  // If we are removing the final observer for the source, then stop
  // observing routes for it.
  // might_have_observers() is reliable here on the assumption that this call
  // is not inside the ObserverList iteration.
  it->second->observers.RemoveObserver(observer);
  if (!it->second->observers.might_have_observers()) {
    SetWakeReason(MediaRouteProviderWakeReason::STOP_OBSERVING_MEDIA_ROUTES);
    RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStopObservingMediaRoutes,
                          base::Unretained(this), source_id));
  }
}

void MediaRouterMojoImpl::RegisterIssuesObserver(IssuesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issue_manager_.RegisterObserver(observer);
}

void MediaRouterMojoImpl::UnregisterIssuesObserver(IssuesObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  issue_manager_.UnregisterObserver(observer);
}

void MediaRouterMojoImpl::RegisterRouteMessageObserver(
    RouteMessageObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);
  const MediaRoute::Id& route_id = observer->route_id();
  auto& observer_list = message_observers_[route_id];
  if (!observer_list) {
    observer_list =
        base::MakeUnique<base::ObserverList<RouteMessageObserver>>();
  } else {
    DCHECK(!observer_list->HasObserver(observer));
  }

  bool should_listen = !observer_list->might_have_observers();
  observer_list->AddObserver(observer);
  if (should_listen) {
    SetWakeReason(
        MediaRouteProviderWakeReason::START_LISTENING_FOR_ROUTE_MESSAGES);
    RunOrDefer(
        base::Bind(&MediaRouterMojoImpl::DoStartListeningForRouteMessages,
                   base::Unretained(this), route_id));
  }
}

void MediaRouterMojoImpl::UnregisterRouteMessageObserver(
    RouteMessageObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(observer);

  const MediaRoute::Id& route_id = observer->route_id();
  auto it = message_observers_.find(route_id);
  if (it == message_observers_.end() || !it->second->HasObserver(observer))
    return;

  it->second->RemoveObserver(observer);
  if (!it->second->might_have_observers()) {
    message_observers_.erase(route_id);
    SetWakeReason(
        MediaRouteProviderWakeReason::STOP_LISTENING_FOR_ROUTE_MESSAGES);
    RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStopListeningForRouteMessages,
                          base::Unretained(this), route_id));
  }
}

void MediaRouterMojoImpl::DoCreateRoute(
    const MediaSource::Id& source_id,
    const MediaSink::Id& sink_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  std::string presentation_id = MediaRouterBase::CreatePresentationId();
  DVLOG_WITH_INSTANCE(1) << "DoCreateRoute " << source_id << "=>" << sink_id
                         << ", presentation ID: " << presentation_id;

  media_route_provider_->CreateRoute(
      source_id, sink_id, presentation_id, origin, tab_id, timeout, incognito,
      base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
                 base::Unretained(this), presentation_id, incognito, callbacks,
                 false));
}

void MediaRouterMojoImpl::DoJoinRoute(
    const MediaSource::Id& source_id,
    const std::string& presentation_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  DVLOG_WITH_INSTANCE(1) << "DoJoinRoute " << source_id
                         << ", presentation ID: " << presentation_id;

  media_route_provider_->JoinRoute(
      source_id, presentation_id, origin, tab_id, timeout, incognito,
      base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
                 base::Unretained(this), presentation_id, incognito, callbacks,
                 true));
}

void MediaRouterMojoImpl::DoConnectRouteByRouteId(
    const MediaSource::Id& source_id,
    const MediaRoute::Id& route_id,
    const std::string& origin,
    int tab_id,
    const std::vector<MediaRouteResponseCallback>& callbacks,
    base::TimeDelta timeout,
    bool incognito) {
  std::string presentation_id = MediaRouterBase::CreatePresentationId();
  DVLOG_WITH_INSTANCE(1) << "DoConnectRouteByRouteId " << source_id
                         << ", route ID: " << route_id
                         << ", presentation ID: " << presentation_id;

  media_route_provider_->ConnectRouteByRouteId(
      source_id, route_id, presentation_id, origin, tab_id, timeout, incognito,
      base::Bind(&MediaRouterMojoImpl::RouteResponseReceived,
                 base::Unretained(this), presentation_id, incognito, callbacks,
                 true));
}

void MediaRouterMojoImpl::DoTerminateRoute(const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoTerminateRoute " << route_id;
  media_route_provider_->TerminateRoute(
      route_id,
      base::Bind(&MediaRouterMojoImpl::OnTerminateRouteResult,
                 base::Unretained(this), route_id));
}

void MediaRouterMojoImpl::DoDetachRoute(const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoDetachRoute " << route_id;
  media_route_provider_->DetachRoute(route_id);
}

void MediaRouterMojoImpl::DoSendSessionMessage(
    const MediaRoute::Id& route_id,
    const std::string& message,
    const SendRouteMessageCallback& callback) {
  DVLOG_WITH_INSTANCE(1) << "SendRouteMessage " << route_id;
  media_route_provider_->SendRouteMessage(route_id, message, callback);
}

void MediaRouterMojoImpl::DoSendSessionBinaryMessage(
    const MediaRoute::Id& route_id,
    std::unique_ptr<std::vector<uint8_t>> data,
    const SendRouteMessageCallback& callback) {
  DVLOG_WITH_INSTANCE(1) << "SendRouteBinaryMessage " << route_id;
  media_route_provider_->SendRouteBinaryMessage(route_id, *data, callback);
}

void MediaRouterMojoImpl::DoStartListeningForRouteMessages(
    const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartListeningForRouteMessages";
  media_route_provider_->StartListeningForRouteMessages(route_id);
}

void MediaRouterMojoImpl::DoStopListeningForRouteMessages(
    const MediaRoute::Id& route_id) {
  DVLOG_WITH_INSTANCE(1) << "StopListeningForRouteMessages";
  media_route_provider_->StopListeningForRouteMessages(route_id);
}

void MediaRouterMojoImpl::DoSearchSinks(
    const MediaSink::Id& sink_id,
    const MediaSource::Id& source_id,
    const std::string& search_input,
    const std::string& domain,
    const MediaSinkSearchResponseCallback& sink_callback) {
  DVLOG_WITH_INSTANCE(1) << "SearchSinks";
  auto sink_search_criteria = mojom::SinkSearchCriteria::New();
  sink_search_criteria->input = search_input;
  sink_search_criteria->domain = domain;
  media_route_provider_->SearchSinks(
      sink_id, source_id, std::move(sink_search_criteria), sink_callback);
}

void MediaRouterMojoImpl::OnRouteMessagesReceived(
    const std::string& route_id,
    const std::vector<RouteMessage>& messages) {
  DVLOG_WITH_INSTANCE(1) << "OnRouteMessagesReceived";

  if (messages.empty())
    return;

  auto it = message_observers_.find(route_id);
  if (it == message_observers_.end()) {
    return;
  }

  for (auto& observer : *it->second)
    observer.OnMessagesReceived(messages);
}

void MediaRouterMojoImpl::OnSinkAvailabilityUpdated(
    SinkAvailability availability) {
  if (availability_ == availability)
    return;

  availability_ = availability;
  if (availability_ == mojom::MediaRouter::SinkAvailability::UNAVAILABLE) {
    // Sinks are no longer available. MRPM has already removed all sink queries.
    for (auto& source_and_query : sinks_queries_) {
      const auto& query = source_and_query.second;
      query->is_active = false;
      query->has_cached_result = false;
      query->cached_sink_list.clear();
      query->origins.clear();
    }
  } else {
    // Sinks are now available. Tell MRPM to start all sink queries again.
    for (const auto& source_and_query : sinks_queries_) {
      RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoStartObservingMediaSinks,
                            base::Unretained(this), source_and_query.first));
    }
  }
}

void MediaRouterMojoImpl::OnPresentationConnectionStateChanged(
    const std::string& route_id,
    mojom::MediaRouter::PresentationConnectionState state) {
  NotifyPresentationConnectionStateChange(
      route_id, mojo::PresentationConnectionStateFromMojo(state));
}

void MediaRouterMojoImpl::OnPresentationConnectionClosed(
    const std::string& route_id,
    mojom::MediaRouter::PresentationConnectionCloseReason reason,
    const std::string& message) {
  NotifyPresentationConnectionClose(
      route_id, mojo::PresentationConnectionCloseReasonFromMojo(reason),
      message);
}

void MediaRouterMojoImpl::OnTerminateRouteResult(
    const MediaRoute::Id& route_id,
    const base::Optional<std::string>& error_text,
    mojom::RouteRequestResultCode result_code) {
  if (result_code != mojom::RouteRequestResultCode::OK) {
    LOG(WARNING) << "Failed to terminate route " << route_id
                 << ": result_code = " << result_code << ", "
                 << error_text.value_or(std::string());
  }
  MediaRouterMojoMetrics::RecordMediaRouteProviderTerminateRoute(
      mojo::RouteRequestResultCodeFromMojo(result_code));
}

void MediaRouterMojoImpl::DoStartObservingMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartObservingMediaSinks: " << source_id;
  // No need to call MRPM if there are no sinks available.
  if (availability_ == mojom::MediaRouter::SinkAvailability::UNAVAILABLE)
    return;

  // No need to call MRPM if all observers have been removed in the meantime.
  auto* sinks_query = sinks_queries_[source_id].get();
  if (!sinks_query || !sinks_query->observers.might_have_observers())
    return;

  DVLOG_WITH_INSTANCE(1) << "MRPM.StartObservingMediaSinks: " << source_id;
  media_route_provider_->StartObservingMediaSinks(source_id);
  sinks_query->is_active = true;
}

void MediaRouterMojoImpl::DoStopObservingMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStopObservingMediaSinks: " << source_id;

  auto it = sinks_queries_.find(source_id);
  // No need to call MRPM if observers have been added in the meantime,
  // or StopObservingMediaSinks has already been called.
  if (it == sinks_queries_.end() || !it->second->is_active ||
      it->second->observers.might_have_observers()) {
    return;
  }

  DVLOG_WITH_INSTANCE(1) << "MRPM.StopObservingMediaSinks: " << source_id;
  media_route_provider_->StopObservingMediaSinks(source_id);
  sinks_queries_.erase(source_id);
}

void MediaRouterMojoImpl::DoStartObservingMediaRoutes(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStartObservingMediaRoutes";

  // No need to call MRPM if all observers have been removed in the meantime.
  auto it = routes_queries_.find(source_id);
  if (it == routes_queries_.end() ||
      !it->second->observers.might_have_observers())
    return;

  DVLOG_WITH_INSTANCE(1) << "MRPM.StartObservingMediaRoutes: " << source_id;
  media_route_provider_->StartObservingMediaRoutes(source_id);
  it->second->is_active = true;
}

void MediaRouterMojoImpl::DoStopObservingMediaRoutes(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoStopObservingMediaRoutes";

  // No need to call MRPM if observers have been added in the meantime,
  // or StopObservingMediaRoutes has already been called.
  auto it = routes_queries_.find(source_id);
  if (it == routes_queries_.end() || !it->second->is_active ||
      it->second->observers.might_have_observers()) {
    return;
  }

  DVLOG_WITH_INSTANCE(1) << "MRPM.StopObservingMediaRoutes: " << source_id;
  media_route_provider_->StopObservingMediaRoutes(source_id);
  routes_queries_.erase(source_id);
}

void MediaRouterMojoImpl::EnqueueTask(const base::Closure& closure) {
  pending_requests_.push_back(closure);
  if (pending_requests_.size() > kMaxPendingRequests) {
    DLOG_WITH_INSTANCE(ERROR) << "Reached max queue size. Dropping oldest "
                              << "request.";
    pending_requests_.pop_front();
  }
  DVLOG_WITH_INSTANCE(2) << "EnqueueTask (queue-length="
                         << pending_requests_.size() << ")";
}

void MediaRouterMojoImpl::RunOrDefer(const base::Closure& request) {
  DCHECK(event_page_tracker_);

  if (media_route_provider_extension_id_.empty()) {
    DVLOG_WITH_INSTANCE(1) << "Extension ID not known yet.";
    EnqueueTask(request);
  } else if (event_page_tracker_->IsEventPageSuspended(
                 media_route_provider_extension_id_)) {
    DVLOG_WITH_INSTANCE(1) << "Waking event page.";
    EnqueueTask(request);
    AttemptWakeEventPage();
    media_route_provider_.reset();
  } else if (!media_route_provider_) {
    DVLOG_WITH_INSTANCE(1) << "Extension is awake, awaiting ProvideMediaRouter "
                              " to be called.";
    EnqueueTask(request);
  } else {
    request.Run();
  }
}

void MediaRouterMojoImpl::AttemptWakeEventPage() {
  ++wakeup_attempt_count_;
  if (wakeup_attempt_count_ > kMaxWakeupAttemptCount) {
    DLOG_WITH_INSTANCE(ERROR) << "Attempted too many times to wake up event "
                              << "page.";
    DrainPendingRequests();
    wakeup_attempt_count_ = 0;
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
        MediaRouteProviderWakeup::ERROR_TOO_MANY_RETRIES);
    return;
  }

  DVLOG_WITH_INSTANCE(1) << "Attempting to wake up event page: attempt "
                         << wakeup_attempt_count_;

  // This return false if the extension is already awake.
  // Callback is bound using WeakPtr because |event_page_tracker_| outlives
  // |this|.
  if (!event_page_tracker_->WakeEventPage(
          media_route_provider_extension_id_,
          base::Bind(&MediaRouterMojoImpl::EventPageWakeComplete,
                     weak_factory_.GetWeakPtr()))) {
    DLOG_WITH_INSTANCE(ERROR) << "Failed to schedule a wakeup for event page.";
  }
}

void MediaRouterMojoImpl::ExecutePendingRequests() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(media_route_provider_);
  DCHECK(event_page_tracker_);
  DCHECK(!media_route_provider_extension_id_.empty());

  for (const auto& next_request : pending_requests_)
    next_request.Run();

  pending_requests_.clear();
}

void MediaRouterMojoImpl::EventPageWakeComplete(bool success) {
  if (success) {
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeReason(
        current_wake_reason_);
    ClearWakeReason();
    MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
        MediaRouteProviderWakeup::SUCCESS);
    return;
  }

  // This is likely an non-retriable error. Drop the pending requests.
  DLOG_WITH_INSTANCE(ERROR)
      << "An error encountered while waking the event page.";
  ClearWakeReason();
  DrainPendingRequests();
  MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
      MediaRouteProviderWakeup::ERROR_UNKNOWN);
}

void MediaRouterMojoImpl::DrainPendingRequests() {
  DLOG_WITH_INSTANCE(ERROR)
      << "Draining request queue. (queue-length=" << pending_requests_.size()
      << ")";
  pending_requests_.clear();
}

void MediaRouterMojoImpl::SetWakeReason(MediaRouteProviderWakeReason reason) {
  DCHECK(reason != MediaRouteProviderWakeReason::TOTAL_COUNT);
  if (current_wake_reason_ == MediaRouteProviderWakeReason::TOTAL_COUNT)
    current_wake_reason_ = reason;
}

void MediaRouterMojoImpl::ClearWakeReason() {
  DCHECK(current_wake_reason_ != MediaRouteProviderWakeReason::TOTAL_COUNT);
  current_wake_reason_ = MediaRouteProviderWakeReason::TOTAL_COUNT;
}

#if defined(OS_WIN)
void MediaRouterMojoImpl::EnsureMdnsDiscoveryEnabled() {
  if (is_mdns_enabled_)
    return;

  SetWakeReason(MediaRouteProviderWakeReason::ENABLE_MDNS_DISCOVERY);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoEnsureMdnsDiscoveryEnabled,
                        base::Unretained(this)));
  should_enable_mdns_discovery_ = true;
}

void MediaRouterMojoImpl::DoEnsureMdnsDiscoveryEnabled() {
  DVLOG_WITH_INSTANCE(1) << "DoEnsureMdnsDiscoveryEnabled";
  if (!is_mdns_enabled_) {
    media_route_provider_->EnableMdnsDiscovery();
    is_mdns_enabled_ = true;
  }
}

void MediaRouterMojoImpl::OnFirewallCheckComplete(
    bool firewall_can_use_local_ports) {
  if (firewall_can_use_local_ports)
    EnsureMdnsDiscoveryEnabled();
}
#endif

void MediaRouterMojoImpl::UpdateMediaSinks(
    const MediaSource::Id& source_id) {
  SetWakeReason(MediaRouteProviderWakeReason::UPDATE_MEDIA_SINKS);
  RunOrDefer(base::Bind(&MediaRouterMojoImpl::DoUpdateMediaSinks,
                        base::Unretained(this), source_id));
}

void MediaRouterMojoImpl::DoUpdateMediaSinks(
    const MediaSource::Id& source_id) {
  DVLOG_WITH_INSTANCE(1) << "DoUpdateMediaSinks" << source_id;
  media_route_provider_->UpdateMediaSinks(source_id);
}

}  // namespace media_router
