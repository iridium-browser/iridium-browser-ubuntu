// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/predictors/resource_prefetcher_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/mime_util/mime_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

// For reporting whether a subresource is handled or not, and for what reasons.
enum ResourceStatus {
  RESOURCE_STATUS_HANDLED = 0,
  RESOURCE_STATUS_NOT_HTTP_PAGE = 1,
  RESOURCE_STATUS_NOT_HTTP_RESOURCE = 2,
  RESOURCE_STATUS_UNSUPPORTED_MIME_TYPE = 4,
  RESOURCE_STATUS_NOT_GET = 8,
  RESOURCE_STATUS_URL_TOO_LONG = 16,
  RESOURCE_STATUS_NOT_CACHEABLE = 32,
  RESOURCE_STATUS_HEADERS_MISSING = 64,
  RESOURCE_STATUS_MAX = 128,
};

// For reporting various interesting events that occur during the loading of a
// single main frame.
enum NavigationEvent {
  NAVIGATION_EVENT_REQUEST_STARTED = 0,
  NAVIGATION_EVENT_REQUEST_REDIRECTED = 1,
  NAVIGATION_EVENT_REQUEST_REDIRECTED_EMPTY_URL = 2,
  NAVIGATION_EVENT_REQUEST_EXPIRED = 3,
  NAVIGATION_EVENT_RESPONSE_STARTED = 4,
  NAVIGATION_EVENT_ONLOAD = 5,
  NAVIGATION_EVENT_ONLOAD_EMPTY_URL = 6,
  NAVIGATION_EVENT_ONLOAD_UNTRACKED_URL = 7,
  NAVIGATION_EVENT_ONLOAD_TRACKED_URL = 8,
  NAVIGATION_EVENT_SHOULD_TRACK_URL = 9,
  NAVIGATION_EVENT_SHOULD_NOT_TRACK_URL = 10,
  NAVIGATION_EVENT_URL_TABLE_FULL = 11,
  NAVIGATION_EVENT_HAVE_PREDICTIONS_FOR_URL = 12,
  NAVIGATION_EVENT_NO_PREDICTIONS_FOR_URL = 13,
  NAVIGATION_EVENT_MAIN_FRAME_URL_TOO_LONG = 14,
  NAVIGATION_EVENT_HOST_TOO_LONG = 15,
  NAVIGATION_EVENT_COUNT = 16,
};

// For reporting events of interest that are not tied to any navigation.
enum ReportingEvent {
  REPORTING_EVENT_ALL_HISTORY_CLEARED = 0,
  REPORTING_EVENT_PARTIAL_HISTORY_CLEARED = 1,
  REPORTING_EVENT_COUNT = 2
};

void RecordNavigationEvent(NavigationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.NavigationEvent",
                            event,
                            NAVIGATION_EVENT_COUNT);
}

// These are additional connection types for
// net::NetworkChangeNotifier::ConnectionType. They have negative values in case
// the original network connection types expand.
enum AdditionalConnectionType {
  CONNECTION_ALL = -2,
  CONNECTION_CELLULAR = -1
};

std::string GetNetTypeStr() {
  switch (net::NetworkChangeNotifier::GetConnectionType()) {
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return "Ethernet";
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return "WiFi";
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return "2G";
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return "3G";
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return "4G";
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return "None";
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return "Bluetooth";
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    default:
      break;
  }
  return "Unknown";
}

void ReportPrefetchedNetworkType(int type) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "ResourcePrefetchPredictor.NetworkType.Prefetched",
      type);
}

void ReportNotPrefetchedNetworkType(int type) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "ResourcePrefetchPredictor.NetworkType.NotPrefetched",
      type);
}

}  // namespace

namespace predictors {

////////////////////////////////////////////////////////////////////////////////
// History lookup task.

// Used to fetch the visit count for a URL from the History database.
class GetUrlVisitCountTask : public history::HistoryDBTask {
 public:
  typedef ResourcePrefetchPredictor::URLRequestSummary URLRequestSummary;
  typedef base::Callback<void(
      size_t,   // Visit count.
      const NavigationID&,
      const std::vector<URLRequestSummary>&)> VisitInfoCallback;

  GetUrlVisitCountTask(
      const NavigationID& navigation_id,
      std::vector<URLRequestSummary>* requests,
      VisitInfoCallback callback)
      : visit_count_(0),
        navigation_id_(navigation_id),
        requests_(requests),
        callback_(callback) {
    DCHECK(requests_.get());
  }

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    history::URLRow url_row;
    if (db->GetRowForURL(navigation_id_.main_frame_url, &url_row))
      visit_count_ = url_row.visit_count();
    return true;
  }

  void DoneRunOnMainThread() override {
    callback_.Run(visit_count_, navigation_id_, *requests_);
  }

 private:
  ~GetUrlVisitCountTask() override {}

  int visit_count_;
  NavigationID navigation_id_;
  scoped_ptr<std::vector<URLRequestSummary> > requests_;
  VisitInfoCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetUrlVisitCountTask);
};

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor static functions.

// static
bool ResourcePrefetchPredictor::ShouldRecordRequest(
    net::URLRequest* request,
    content::ResourceType resource_type) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!request_info)
    return false;

  if (!request_info->IsMainFrame())
    return false;

  return resource_type == content::RESOURCE_TYPE_MAIN_FRAME &&
      IsHandledMainPage(request);
}

// static
bool ResourcePrefetchPredictor::ShouldRecordResponse(
    net::URLRequest* response) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(response);
  if (!request_info)
    return false;

  if (!request_info->IsMainFrame())
    return false;

  return request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME ?
      IsHandledMainPage(response) : IsHandledSubresource(response);
}

// static
bool ResourcePrefetchPredictor::ShouldRecordRedirect(
    net::URLRequest* response) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(response);
  if (!request_info)
    return false;

  if (!request_info->IsMainFrame())
    return false;

  return request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME &&
      IsHandledMainPage(response);
}

// static
bool ResourcePrefetchPredictor::IsHandledMainPage(net::URLRequest* request) {
  return request->original_url().scheme() == url::kHttpScheme;
}

// static
bool ResourcePrefetchPredictor::IsHandledSubresource(
    net::URLRequest* response) {
  int resource_status = 0;
  if (response->first_party_for_cookies().scheme() != url::kHttpScheme)
    resource_status |= RESOURCE_STATUS_NOT_HTTP_PAGE;

  if (response->original_url().scheme() != url::kHttpScheme)
    resource_status |= RESOURCE_STATUS_NOT_HTTP_RESOURCE;

  std::string mime_type;
  response->GetMimeType(&mime_type);
  if (!mime_type.empty() && !mime_util::IsSupportedImageMimeType(mime_type) &&
      !mime_util::IsSupportedJavascriptMimeType(mime_type) &&
      !net::MatchesMimeType("text/css", mime_type)) {
    resource_status |= RESOURCE_STATUS_UNSUPPORTED_MIME_TYPE;
  }

  if (response->method() != "GET")
    resource_status |= RESOURCE_STATUS_NOT_GET;

  if (response->original_url().spec().length() >
      ResourcePrefetchPredictorTables::kMaxStringLength) {
    resource_status |= RESOURCE_STATUS_URL_TOO_LONG;
  }

  if (!response->response_info().headers.get())
    resource_status |= RESOURCE_STATUS_HEADERS_MISSING;

  if (!IsCacheable(response))
    resource_status |= RESOURCE_STATUS_NOT_CACHEABLE;

  UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.ResourceStatus",
                            resource_status,
                            RESOURCE_STATUS_MAX);

  return resource_status == 0;
}

// static
bool ResourcePrefetchPredictor::IsCacheable(const net::URLRequest* response) {
  if (response->was_cached())
    return true;

  // For non cached responses, we will ensure that the freshness lifetime is
  // some sane value.
  const net::HttpResponseInfo& response_info = response->response_info();
  if (!response_info.headers.get())
    return false;
  base::Time response_time(response_info.response_time);
  response_time += base::TimeDelta::FromSeconds(1);
  base::TimeDelta freshness =
      response_info.headers->GetFreshnessLifetimes(response_time).freshness;
  return freshness > base::TimeDelta();
}

// static
content::ResourceType ResourcePrefetchPredictor::GetResourceTypeFromMimeType(
    const std::string& mime_type,
    content::ResourceType fallback) {
  if (mime_util::IsSupportedImageMimeType(mime_type))
    return content::RESOURCE_TYPE_IMAGE;
  else if (mime_util::IsSupportedJavascriptMimeType(mime_type))
    return content::RESOURCE_TYPE_SCRIPT;
  else if (net::MatchesMimeType("text/css", mime_type))
    return content::RESOURCE_TYPE_STYLESHEET;
  else
    return fallback;
}

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor structs.

ResourcePrefetchPredictor::URLRequestSummary::URLRequestSummary()
    : resource_type(content::RESOURCE_TYPE_LAST_TYPE),
      was_cached(false) {
}

ResourcePrefetchPredictor::URLRequestSummary::URLRequestSummary(
    const URLRequestSummary& other)
        : navigation_id(other.navigation_id),
          resource_url(other.resource_url),
          resource_type(other.resource_type),
          mime_type(other.mime_type),
          was_cached(other.was_cached),
          redirect_url(other.redirect_url) {
}

ResourcePrefetchPredictor::URLRequestSummary::~URLRequestSummary() {
}

ResourcePrefetchPredictor::Result::Result(
    PrefetchKeyType i_key_type,
    ResourcePrefetcher::RequestVector* i_requests)
    : key_type(i_key_type),
      requests(i_requests) {
}

ResourcePrefetchPredictor::Result::~Result() {
}

////////////////////////////////////////////////////////////////////////////////
// ResourcePrefetchPredictor.

ResourcePrefetchPredictor::ResourcePrefetchPredictor(
    const ResourcePrefetchPredictorConfig& config,
    Profile* profile)
    : profile_(profile),
      config_(config),
      initialization_state_(NOT_INITIALIZED),
      tables_(PredictorDatabaseFactory::GetForProfile(profile)
                  ->resource_prefetch_tables()),
      history_service_observer_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Some form of learning has to be enabled.
  DCHECK(config_.IsLearningEnabled());
  if (config_.IsURLPrefetchingEnabled(profile_))
    DCHECK(config_.IsURLLearningEnabled());
  if (config_.IsHostPrefetchingEnabled(profile_))
    DCHECK(config_.IsHostLearningEnabled());
}

ResourcePrefetchPredictor::~ResourcePrefetchPredictor() {
}

void ResourcePrefetchPredictor::RecordURLRequest(
    const URLRequestSummary& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  CHECK_EQ(request.resource_type, content::RESOURCE_TYPE_MAIN_FRAME);
  OnMainFrameRequest(request);
}

void ResourcePrefetchPredictor::RecordURLResponse(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  if (response.resource_type == content::RESOURCE_TYPE_MAIN_FRAME)
    OnMainFrameResponse(response);
  else
    OnSubresourceResponse(response);
}

void ResourcePrefetchPredictor::RecordURLRedirect(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  CHECK_EQ(response.resource_type, content::RESOURCE_TYPE_MAIN_FRAME);
  OnMainFrameRedirect(response);
}

void ResourcePrefetchPredictor::RecordMainFrameLoadComplete(
    const NavigationID& navigation_id) {
  switch (initialization_state_) {
    case NOT_INITIALIZED:
      StartInitialization();
      break;
    case INITIALIZING:
      break;
    case INITIALIZED: {
      RecordNavigationEvent(NAVIGATION_EVENT_ONLOAD);
      // WebContents can return an empty URL if the navigation entry
      // corresponding to the navigation has not been created yet.
      if (navigation_id.main_frame_url.is_empty())
        RecordNavigationEvent(NAVIGATION_EVENT_ONLOAD_EMPTY_URL);
      else
        OnNavigationComplete(navigation_id);
      break;
    }
    default:
      NOTREACHED() << "Unexpected initialization_state_: "
                   << initialization_state_;
  }
}

void ResourcePrefetchPredictor::FinishedPrefetchForNavigation(
    const NavigationID& navigation_id,
    PrefetchKeyType key_type,
    ResourcePrefetcher::RequestVector* requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_ptr<Result> result(new Result(key_type, requests));
  // Add the results to the results map.
  if (!results_map_.insert(navigation_id, result.Pass()).second)
    DLOG(FATAL) << "Returning results for existing navigation.";
}

void ResourcePrefetchPredictor::Shutdown() {
  if (prefetch_manager_.get()) {
    prefetch_manager_->ShutdownOnUIThread();
    prefetch_manager_ = NULL;
  }
  history_service_observer_.RemoveAll();
}

void ResourcePrefetchPredictor::OnMainFrameRequest(
    const URLRequestSummary& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZED, initialization_state_);

  RecordNavigationEvent(NAVIGATION_EVENT_REQUEST_STARTED);

  StartPrefetching(request.navigation_id);

  // Cleanup older navigations.
  CleanupAbandonedNavigations(request.navigation_id);

  // New empty navigation entry.
  inflight_navigations_.insert(std::make_pair(
      request.navigation_id,
      make_linked_ptr(new std::vector<URLRequestSummary>())));
}

void ResourcePrefetchPredictor::OnMainFrameResponse(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (initialization_state_ != INITIALIZED)
    return;

  RecordNavigationEvent(NAVIGATION_EVENT_RESPONSE_STARTED);

  StopPrefetching(response.navigation_id);
}

void ResourcePrefetchPredictor::OnMainFrameRedirect(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RecordNavigationEvent(NAVIGATION_EVENT_REQUEST_REDIRECTED);

  // TODO(shishir): There are significant gains to be had here if we can use the
  // start URL in a redirect chain as the key to start prefetching. We can save
  // of redirect times considerably assuming that the redirect chains do not
  // change.

  // Stop any inflight prefetching. Remove the older navigation.
  StopPrefetching(response.navigation_id);
  inflight_navigations_.erase(response.navigation_id);

  // A redirect will not lead to another OnMainFrameRequest call, so record the
  // redirect url as a new navigation.

  // The redirect url may be empty if the url was invalid.
  if (response.redirect_url.is_empty()) {
    RecordNavigationEvent(NAVIGATION_EVENT_REQUEST_REDIRECTED_EMPTY_URL);
    return;
  }

  NavigationID navigation_id(response.navigation_id);
  navigation_id.main_frame_url = response.redirect_url;
  inflight_navigations_.insert(std::make_pair(
      navigation_id,
      make_linked_ptr(new std::vector<URLRequestSummary>())));
}

void ResourcePrefetchPredictor::OnSubresourceResponse(
    const URLRequestSummary& response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  NavigationMap::const_iterator nav_it =
        inflight_navigations_.find(response.navigation_id);
  if (nav_it == inflight_navigations_.end()) {
    return;
  }

  nav_it->second->push_back(response);
}

base::TimeDelta ResourcePrefetchPredictor::OnNavigationComplete(
    const NavigationID& nav_id_without_timing_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  NavigationMap::iterator nav_it =
      inflight_navigations_.find(nav_id_without_timing_info);
  if (nav_it == inflight_navigations_.end()) {
    RecordNavigationEvent(NAVIGATION_EVENT_ONLOAD_UNTRACKED_URL);
    return base::TimeDelta();
  }
  RecordNavigationEvent(NAVIGATION_EVENT_ONLOAD_TRACKED_URL);

  // Get and use the navigation ID stored in |inflight_navigations_| because it
  // has the timing infomation.
  const NavigationID navigation_id(nav_it->first);

  // Report any stats.
  base::TimeDelta plt = base::TimeTicks::Now() - navigation_id.creation_time;
  ReportPageLoadTimeStats(plt);
  if (prefetch_manager_.get()) {
    ResultsMap::const_iterator results_it = results_map_.find(navigation_id);
    bool have_prefetch_results = results_it != results_map_.end();
    UMA_HISTOGRAM_BOOLEAN("ResourcePrefetchPredictor.HavePrefetchResults",
                          have_prefetch_results);
    if (have_prefetch_results) {
      ReportAccuracyStats(results_it->second->key_type,
                          *(nav_it->second),
                          results_it->second->requests.get());
      ReportPageLoadTimePrefetchStats(
          plt,
          true,
          base::Bind(&ReportPrefetchedNetworkType),
          results_it->second->key_type);
    } else {
      ReportPageLoadTimePrefetchStats(
          plt,
          false,
          base::Bind(&ReportNotPrefetchedNetworkType),
          PREFETCH_KEY_TYPE_URL);
    }
  } else {
    scoped_ptr<ResourcePrefetcher::RequestVector> requests(
        new ResourcePrefetcher::RequestVector);
    PrefetchKeyType key_type;
    if (GetPrefetchData(navigation_id, requests.get(), &key_type)) {
      RecordNavigationEvent(NAVIGATION_EVENT_HAVE_PREDICTIONS_FOR_URL);
      ReportPredictedAccuracyStats(key_type,
                                   *(nav_it->second),
                                   *requests);
    } else {
      RecordNavigationEvent(NAVIGATION_EVENT_NO_PREDICTIONS_FOR_URL);
    }
  }

  // Remove the navigation from the inflight navigations.
  std::vector<URLRequestSummary>* requests = (nav_it->second).release();
  inflight_navigations_.erase(nav_it);

  // Kick off history lookup to determine if we should record the URL.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history_service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new GetUrlVisitCountTask(
              navigation_id,
              requests,
              base::Bind(&ResourcePrefetchPredictor::OnVisitCountLookup,
                         AsWeakPtr()))),
      &history_lookup_consumer_);

  return plt;
}

bool ResourcePrefetchPredictor::GetPrefetchData(
    const NavigationID& navigation_id,
    ResourcePrefetcher::RequestVector* prefetch_requests,
    PrefetchKeyType* key_type) {
  DCHECK(prefetch_requests);
  DCHECK(key_type);

  *key_type = PREFETCH_KEY_TYPE_URL;
  const GURL& main_frame_url = navigation_id.main_frame_url;

  bool use_url_data = config_.IsPrefetchingEnabled(profile_) ?
      config_.IsURLPrefetchingEnabled(profile_) :
      config_.IsURLLearningEnabled();
  if (use_url_data) {
    PrefetchDataMap::const_iterator iterator =
        url_table_cache_->find(main_frame_url.spec());
    if (iterator != url_table_cache_->end())
      PopulatePrefetcherRequest(iterator->second, prefetch_requests);
  }
  if (!prefetch_requests->empty())
    return true;

  bool use_host_data = config_.IsPrefetchingEnabled(profile_) ?
      config_.IsHostPrefetchingEnabled(profile_) :
      config_.IsHostLearningEnabled();
  if (use_host_data) {
    PrefetchDataMap::const_iterator iterator =
        host_table_cache_->find(main_frame_url.host());
    if (iterator != host_table_cache_->end()) {
      *key_type = PREFETCH_KEY_TYPE_HOST;
      PopulatePrefetcherRequest(iterator->second, prefetch_requests);
    }
  }

  return !prefetch_requests->empty();
}

void ResourcePrefetchPredictor::PopulatePrefetcherRequest(
    const PrefetchData& data,
    ResourcePrefetcher::RequestVector* requests) {
  for (ResourceRows::const_iterator it = data.resources.begin();
       it != data.resources.end(); ++it) {
    float confidence = static_cast<float>(it->number_of_hits) /
        (it->number_of_hits + it->number_of_misses);
    if (confidence < config_.min_resource_confidence_to_trigger_prefetch ||
        it->number_of_hits < config_.min_resource_hits_to_trigger_prefetch) {
      continue;
    }

    ResourcePrefetcher::Request* req = new ResourcePrefetcher::Request(
        it->resource_url);
    requests->push_back(req);
  }
}

void ResourcePrefetchPredictor::StartPrefetching(
    const NavigationID& navigation_id) {
  if (!prefetch_manager_.get())  // Prefetching not enabled.
    return;

  // Prefer URL based data first.
  scoped_ptr<ResourcePrefetcher::RequestVector> requests(
      new ResourcePrefetcher::RequestVector);
  PrefetchKeyType key_type;
  if (!GetPrefetchData(navigation_id, requests.get(), &key_type)) {
    // No prefetching data at host or URL level.
    return;
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::MaybeAddPrefetch,
                 prefetch_manager_,
                 navigation_id,
                 key_type,
                 base::Passed(&requests)));
}

void ResourcePrefetchPredictor::StopPrefetching(
    const NavigationID& navigation_id) {
  if (!prefetch_manager_.get())  // Not enabled.
    return;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ResourcePrefetcherManager::MaybeRemovePrefetch,
                 prefetch_manager_,
                 navigation_id));
}

void ResourcePrefetchPredictor::StartInitialization() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(NOT_INITIALIZED, initialization_state_);
  initialization_state_ = INITIALIZING;

  // Create local caches using the database as loaded.
  scoped_ptr<PrefetchDataMap> url_data_map(new PrefetchDataMap());
  scoped_ptr<PrefetchDataMap> host_data_map(new PrefetchDataMap());
  PrefetchDataMap* url_data_ptr = url_data_map.get();
  PrefetchDataMap* host_data_ptr = host_data_map.get();

  BrowserThread::PostTaskAndReply(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::GetAllData,
                 tables_, url_data_ptr, host_data_ptr),
      base::Bind(&ResourcePrefetchPredictor::CreateCaches, AsWeakPtr(),
                 base::Passed(&url_data_map), base::Passed(&host_data_map)));
}

void ResourcePrefetchPredictor::CreateCaches(
    scoped_ptr<PrefetchDataMap> url_data_map,
    scoped_ptr<PrefetchDataMap> host_data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(INITIALIZING, initialization_state_);
  DCHECK(!url_table_cache_);
  DCHECK(!host_table_cache_);
  DCHECK(inflight_navigations_.empty());

  url_table_cache_.reset(url_data_map.release());
  host_table_cache_.reset(host_data_map.release());

  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.UrlTableMainFrameUrlCount",
                       url_table_cache_->size());
  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HostTableHostCount",
                       host_table_cache_->size());

  ConnectToHistoryService();
}

void ResourcePrefetchPredictor::OnHistoryAndCacheLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(INITIALIZING, initialization_state_);

  // Initialize the prefetch manager only if prefetching is enabled.
  if (config_.IsPrefetchingEnabled(profile_)) {
    prefetch_manager_ = new ResourcePrefetcherManager(
        this, config_, profile_->GetRequestContext());
  }
  initialization_state_ = INITIALIZED;
}

void ResourcePrefetchPredictor::CleanupAbandonedNavigations(
    const NavigationID& navigation_id) {
  static const base::TimeDelta max_navigation_age =
      base::TimeDelta::FromSeconds(config_.max_navigation_lifetime_seconds);

  base::TimeTicks time_now = base::TimeTicks::Now();
  for (NavigationMap::iterator it = inflight_navigations_.begin();
       it != inflight_navigations_.end();) {
    if (it->first.IsSameRenderer(navigation_id) ||
        (time_now - it->first.creation_time > max_navigation_age)) {
      inflight_navigations_.erase(it++);
      RecordNavigationEvent(NAVIGATION_EVENT_REQUEST_EXPIRED);
    } else {
      ++it;
    }
  }
  for (ResultsMap::const_iterator it = results_map_.begin();
       it != results_map_.end();) {
    if (it->first.IsSameRenderer(navigation_id) ||
        (time_now - it->first.creation_time > max_navigation_age)) {
      results_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

void ResourcePrefetchPredictor::DeleteAllUrls() {
  inflight_navigations_.clear();
  url_table_cache_->clear();
  host_table_cache_->clear();

  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::DeleteAllData, tables_));
}

void ResourcePrefetchPredictor::DeleteUrls(const history::URLRows& urls) {
  // Check all the urls in the database and pick out the ones that are present
  // in the cache.
  std::vector<std::string> urls_to_delete, hosts_to_delete;

  for (const auto& it : urls) {
    const std::string& url_spec = it.url().spec();
    if (url_table_cache_->find(url_spec) != url_table_cache_->end()) {
      urls_to_delete.push_back(url_spec);
      url_table_cache_->erase(url_spec);
    }

    const std::string& host = it.url().host();
    if (host_table_cache_->find(host) != host_table_cache_->end()) {
      hosts_to_delete.push_back(host);
      host_table_cache_->erase(host);
    }
  }

  if (!urls_to_delete.empty() || !hosts_to_delete.empty()) {
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteData,
                   tables_,
                   urls_to_delete,
                   hosts_to_delete));
  }
}

void ResourcePrefetchPredictor::RemoveOldestEntryInPrefetchDataMap(
    PrefetchKeyType key_type,
    PrefetchDataMap* data_map) {
  if (data_map->empty())
    return;

  base::Time oldest_time;
  std::string key_to_delete;
  for (PrefetchDataMap::iterator it = data_map->begin();
       it != data_map->end(); ++it) {
    if (key_to_delete.empty() || it->second.last_visit < oldest_time) {
      key_to_delete = it->first;
      oldest_time = it->second.last_visit;
    }
  }

  data_map->erase(key_to_delete);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteSingleDataPoint,
                   tables_,
                   key_to_delete,
                   key_type));
}

void ResourcePrefetchPredictor::OnVisitCountLookup(
    size_t visit_count,
    const NavigationID& navigation_id,
    const std::vector<URLRequestSummary>& requests) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HistoryVisitCountForUrl",
                       visit_count);

  // URL level data - merge only if we are already saving the data, or we it
  // meets the cutoff requirement.
  const std::string url_spec = navigation_id.main_frame_url.spec();
  bool already_tracking = url_table_cache_->find(url_spec) !=
      url_table_cache_->end();
  bool should_track_url = already_tracking ||
      (visit_count >= config_.min_url_visit_count);

  if (should_track_url) {
    RecordNavigationEvent(NAVIGATION_EVENT_SHOULD_TRACK_URL);

    if (config_.IsURLLearningEnabled()) {
      LearnNavigation(url_spec, PREFETCH_KEY_TYPE_URL, requests,
                      config_.max_urls_to_track, url_table_cache_.get());
    }
  } else {
    RecordNavigationEvent(NAVIGATION_EVENT_SHOULD_NOT_TRACK_URL);
  }

  // Host level data - no cutoff, always learn the navigation if enabled.
  if (config_.IsHostLearningEnabled()) {
    LearnNavigation(navigation_id.main_frame_url.host(),
                    PREFETCH_KEY_TYPE_HOST,
                    requests,
                    config_.max_hosts_to_track,
                    host_table_cache_.get());
  }

  // Remove the navigation from the results map.
  results_map_.erase(navigation_id);
}

void ResourcePrefetchPredictor::LearnNavigation(
    const std::string& key,
    PrefetchKeyType key_type,
    const std::vector<URLRequestSummary>& new_resources,
    size_t max_data_map_size,
    PrefetchDataMap* data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the primary key is too long reject it.
  if (key.length() > ResourcePrefetchPredictorTables::kMaxStringLength) {
    if (key_type == PREFETCH_KEY_TYPE_HOST)
      RecordNavigationEvent(NAVIGATION_EVENT_HOST_TOO_LONG);
    else
      RecordNavigationEvent(NAVIGATION_EVENT_MAIN_FRAME_URL_TOO_LONG);
    return;
  }

  PrefetchDataMap::iterator cache_entry = data_map->find(key);
  if (cache_entry == data_map->end()) {
    if (data_map->size() >= max_data_map_size) {
      // The table is full, delete an entry.
      RemoveOldestEntryInPrefetchDataMap(key_type, data_map);
    }

    cache_entry = data_map->insert(std::make_pair(
        key, PrefetchData(key_type, key))).first;
    cache_entry->second.last_visit = base::Time::Now();
    size_t new_resources_size = new_resources.size();
    std::set<GURL> resources_seen;
    for (size_t i = 0; i < new_resources_size; ++i) {
      if (resources_seen.find(new_resources[i].resource_url) !=
          resources_seen.end()) {
        continue;
      }
      ResourceRow row_to_add;
      row_to_add.resource_url = new_resources[i].resource_url;
      row_to_add.resource_type = new_resources[i].resource_type;
      row_to_add.number_of_hits = 1;
      row_to_add.average_position = i + 1;
      cache_entry->second.resources.push_back(row_to_add);
      resources_seen.insert(new_resources[i].resource_url);
    }
  } else {
    ResourceRows& old_resources = cache_entry->second.resources;
    cache_entry->second.last_visit = base::Time::Now();

    // Build indices over the data.
    std::map<GURL, int> new_index, old_index;
    int new_resources_size = static_cast<int>(new_resources.size());
    for (int i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      // Take the first occurence of every url.
      if (new_index.find(summary.resource_url) == new_index.end())
        new_index[summary.resource_url] = i;
    }
    int old_resources_size = static_cast<int>(old_resources.size());
    for (int i = 0; i < old_resources_size; ++i) {
      const ResourceRow& row = old_resources[i];
      DCHECK(old_index.find(row.resource_url) == old_index.end());
      old_index[row.resource_url] = i;
    }

    // Go through the old urls and update their hit/miss counts.
    for (int i = 0; i < old_resources_size; ++i) {
      ResourceRow& old_row = old_resources[i];
      if (new_index.find(old_row.resource_url) == new_index.end()) {
        ++old_row.number_of_misses;
        ++old_row.consecutive_misses;
      } else {
        const URLRequestSummary& new_row =
            new_resources[new_index[old_row.resource_url]];

        // Update the resource type since it could have changed.
        if (new_row.resource_type != content::RESOURCE_TYPE_LAST_TYPE)
          old_row.resource_type = new_row.resource_type;

        int position = new_index[old_row.resource_url] + 1;
        int total = old_row.number_of_hits + old_row.number_of_misses;
        old_row.average_position =
            ((old_row.average_position * total) + position) / (total + 1);
        ++old_row.number_of_hits;
        old_row.consecutive_misses = 0;
      }
    }

    // Add the new ones that we have not seen before.
    for (int i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      if (old_index.find(summary.resource_url) != old_index.end())
        continue;

      // Only need to add new stuff.
      ResourceRow row_to_add;
      row_to_add.resource_url = summary.resource_url;
      row_to_add.resource_type = summary.resource_type;
      row_to_add.number_of_hits = 1;
      row_to_add.average_position = i + 1;
      old_resources.push_back(row_to_add);

      // To ensure we dont add the same url twice.
      old_index[summary.resource_url] = 0;
    }
  }

  // Trim and sort the resources after the update.
  ResourceRows& resources = cache_entry->second.resources;
  for (ResourceRows::iterator it = resources.begin();
       it != resources.end();) {
    it->UpdateScore();
    if (it->consecutive_misses >= config_.max_consecutive_misses)
      it = resources.erase(it);
    else
      ++it;
  }
  std::sort(resources.begin(), resources.end(),
            ResourcePrefetchPredictorTables::ResourceRowSorter());
  if (resources.size() > config_.max_resources_per_entry)
    resources.resize(config_.max_resources_per_entry);

  // If the row has no resources, remove it from the cache and delete the
  // entry in the database. Else update the database.
  if (resources.empty()) {
    data_map->erase(key);
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteSingleDataPoint,
                   tables_,
                   key,
                   key_type));
  } else {
    bool is_host = key_type == PREFETCH_KEY_TYPE_HOST;
    PrefetchData empty_data(
        !is_host ? PREFETCH_KEY_TYPE_HOST : PREFETCH_KEY_TYPE_URL,
        std::string());
    const PrefetchData& host_data = is_host ? cache_entry->second : empty_data;
    const PrefetchData& url_data = is_host ? empty_data : cache_entry->second;
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::UpdateData,
                   tables_,
                   url_data,
                   host_data));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Page load time and accuracy measurement.

// This is essentially UMA_HISTOGRAM_MEDIUM_TIMES, but it avoids using the
// STATIC_HISTOGRAM_POINTER_BLOCK in UMA_HISTOGRAM definitions.
#define RPP_HISTOGRAM_MEDIUM_TIMES(name, page_load_time) \
  do { \
    base::HistogramBase* histogram = base::Histogram::FactoryTimeGet( \
        name, \
        base::TimeDelta::FromMilliseconds(10), \
        base::TimeDelta::FromMinutes(3), \
        50, \
        base::HistogramBase::kUmaTargetedHistogramFlag); \
    histogram->AddTime(page_load_time); \
  } while (0)

void ResourcePrefetchPredictor::ReportPageLoadTimeStats(
    base::TimeDelta plt) const {
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();

  RPP_HISTOGRAM_MEDIUM_TIMES("ResourcePrefetchPredictor.PLT", plt);
  RPP_HISTOGRAM_MEDIUM_TIMES(
      "ResourcePrefetchPredictor.PLT_" + GetNetTypeStr(), plt);
  if (net::NetworkChangeNotifier::IsConnectionCellular(connection_type))
    RPP_HISTOGRAM_MEDIUM_TIMES("ResourcePrefetchPredictor.PLT_Cellular", plt);
}

void ResourcePrefetchPredictor::ReportPageLoadTimePrefetchStats(
    base::TimeDelta plt,
    bool prefetched,
    base::Callback<void(int)> report_network_type_callback,
    PrefetchKeyType key_type) const {
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  bool on_cellular =
      net::NetworkChangeNotifier::IsConnectionCellular(connection_type);

  report_network_type_callback.Run(CONNECTION_ALL);
  report_network_type_callback.Run(connection_type);
  if (on_cellular)
    report_network_type_callback.Run(CONNECTION_CELLULAR);

  std::string prefetched_str;
  if (prefetched)
    prefetched_str = "Prefetched";
  else
    prefetched_str = "NotPrefetched";

  RPP_HISTOGRAM_MEDIUM_TIMES(
      "ResourcePrefetchPredictor.PLT." + prefetched_str, plt);
  RPP_HISTOGRAM_MEDIUM_TIMES(
      "ResourcePrefetchPredictor.PLT." + prefetched_str + "_" + GetNetTypeStr(),
      plt);
  if (on_cellular) {
    RPP_HISTOGRAM_MEDIUM_TIMES(
        "ResourcePrefetchPredictor.PLT." + prefetched_str + "_Cellular", plt);
  }

  if (!prefetched)
    return;

  std::string type =
      key_type == PREFETCH_KEY_TYPE_HOST ? "Host" : "Url";
  RPP_HISTOGRAM_MEDIUM_TIMES(
      "ResourcePrefetchPredictor.PLT.Prefetched." + type, plt);
  RPP_HISTOGRAM_MEDIUM_TIMES(
      "ResourcePrefetchPredictor.PLT.Prefetched." + type + "_"
          + GetNetTypeStr(),
      plt);
  if (on_cellular) {
    RPP_HISTOGRAM_MEDIUM_TIMES(
        "ResourcePrefetchPredictor.PLT.Prefetched." + type + "_Cellular",
        plt);
  }
}

void ResourcePrefetchPredictor::ReportAccuracyStats(
    PrefetchKeyType key_type,
    const std::vector<URLRequestSummary>& actual,
    ResourcePrefetcher::RequestVector* prefetched) const {
  // Annotate the results.
  std::map<GURL, bool> actual_resources;
  for (std::vector<URLRequestSummary>::const_iterator it = actual.begin();
       it != actual.end(); ++it) {
    actual_resources[it->resource_url] = it->was_cached;
  }

  int prefetch_cancelled = 0, prefetch_failed = 0, prefetch_not_started = 0;
  // 'a_' -> actual, 'p_' -> predicted.
  int p_cache_a_cache = 0, p_cache_a_network = 0, p_cache_a_notused = 0,
      p_network_a_cache = 0, p_network_a_network = 0, p_network_a_notused = 0;

  for (ResourcePrefetcher::RequestVector::iterator it = prefetched->begin();
       it != prefetched->end(); ++it) {
    ResourcePrefetcher::Request* req = *it;

    // Set the usage states if the resource was actually used.
    std::map<GURL, bool>::const_iterator actual_it =
        actual_resources.find(req->resource_url);
    if (actual_it != actual_resources.end()) {
      if (actual_it->second) {
        req->usage_status =
            ResourcePrefetcher::Request::USAGE_STATUS_FROM_CACHE;
      } else {
        req->usage_status =
            ResourcePrefetcher::Request::USAGE_STATUS_FROM_NETWORK;
      }
    }

    switch (req->prefetch_status) {
      // TODO(shishir): Add histogram for each cancellation reason.
      case ResourcePrefetcher::Request::PREFETCH_STATUS_REDIRECTED:
      case ResourcePrefetcher::Request::PREFETCH_STATUS_AUTH_REQUIRED:
      case ResourcePrefetcher::Request::PREFETCH_STATUS_CERT_REQUIRED:
      case ResourcePrefetcher::Request::PREFETCH_STATUS_CERT_ERROR:
      case ResourcePrefetcher::Request::PREFETCH_STATUS_CANCELLED:
        ++prefetch_cancelled;
        break;

      case ResourcePrefetcher::Request::PREFETCH_STATUS_FAILED:
        ++prefetch_failed;
        break;

      case ResourcePrefetcher::Request::PREFETCH_STATUS_FROM_CACHE:
        if (req->usage_status ==
            ResourcePrefetcher::Request::USAGE_STATUS_FROM_CACHE)
          ++p_cache_a_cache;
        else if (req->usage_status ==
                 ResourcePrefetcher::Request::USAGE_STATUS_FROM_NETWORK)
          ++p_cache_a_network;
        else
          ++p_cache_a_notused;
        break;

      case ResourcePrefetcher::Request::PREFETCH_STATUS_FROM_NETWORK:
          if (req->usage_status ==
              ResourcePrefetcher::Request::USAGE_STATUS_FROM_CACHE)
            ++p_network_a_cache;
          else if (req->usage_status ==
                   ResourcePrefetcher::Request::USAGE_STATUS_FROM_NETWORK)
            ++p_network_a_network;
          else
            ++p_network_a_notused;
        break;

      case ResourcePrefetcher::Request::PREFETCH_STATUS_NOT_STARTED:
        ++prefetch_not_started;
        break;

      case ResourcePrefetcher::Request::PREFETCH_STATUS_STARTED:
        DLOG(FATAL) << "Invalid prefetch status";
        break;
    }
  }

  int total_prefetched = p_cache_a_cache + p_cache_a_network + p_cache_a_notused
      + p_network_a_cache + p_network_a_network + p_network_a_notused;

  std::string histogram_type = key_type == PREFETCH_KEY_TYPE_HOST ? "Host." :
      "Url.";

  // Macros to avoid using the STATIC_HISTOGRAM_POINTER_BLOCK in UMA_HISTOGRAM
  // definitions.
#define RPP_HISTOGRAM_PERCENTAGE(suffix, value) \
  { \
    std::string name = "ResourcePrefetchPredictor." + histogram_type + suffix; \
    std::string g_name = "ResourcePrefetchPredictor." + std::string(suffix); \
    base::HistogramBase* histogram = base::LinearHistogram::FactoryGet( \
        name, 1, 101, 102, base::Histogram::kUmaTargetedHistogramFlag); \
    histogram->Add(value); \
    UMA_HISTOGRAM_PERCENTAGE(g_name, value); \
  }

  RPP_HISTOGRAM_PERCENTAGE("PrefetchCancelled",
                           prefetch_cancelled * 100.0 / total_prefetched);
  RPP_HISTOGRAM_PERCENTAGE("PrefetchFailed",
                           prefetch_failed * 100.0 / total_prefetched);
  RPP_HISTOGRAM_PERCENTAGE("PrefetchFromCacheUsedFromCache",
                           p_cache_a_cache * 100.0 / total_prefetched);
  RPP_HISTOGRAM_PERCENTAGE("PrefetchFromCacheUsedFromNetwork",
                           p_cache_a_network * 100.0 / total_prefetched);
  RPP_HISTOGRAM_PERCENTAGE("PrefetchFromCacheNotUsed",
                           p_cache_a_notused * 100.0 / total_prefetched);
  RPP_HISTOGRAM_PERCENTAGE("PrefetchFromNetworkUsedFromCache",
                           p_network_a_cache * 100.0 / total_prefetched);
  RPP_HISTOGRAM_PERCENTAGE("PrefetchFromNetworkUsedFromNetwork",
                           p_network_a_network * 100.0 / total_prefetched);
  RPP_HISTOGRAM_PERCENTAGE("PrefetchFromNetworkNotUsed",
                           p_network_a_notused * 100.0 / total_prefetched);

  RPP_HISTOGRAM_PERCENTAGE(
      "PrefetchNotStarted",
      prefetch_not_started * 100.0 / (prefetch_not_started + total_prefetched));

#undef RPP_HISTOGRAM_PERCENTAGE
}

void ResourcePrefetchPredictor::ReportPredictedAccuracyStats(
    PrefetchKeyType key_type,
    const std::vector<URLRequestSummary>& actual,
    const ResourcePrefetcher::RequestVector& predicted) const {
  std::map<GURL, bool> actual_resources;
  int from_network = 0;
  for (std::vector<URLRequestSummary>::const_iterator it = actual.begin();
       it != actual.end(); ++it) {
    actual_resources[it->resource_url] = it->was_cached;
    if (!it->was_cached)
      ++from_network;
  }

  // Measure the accuracy at 25, 50 predicted resources.
  ReportPredictedAccuracyStatsHelper(key_type, predicted, actual_resources,
                                     from_network, 25);
  ReportPredictedAccuracyStatsHelper(key_type, predicted, actual_resources,
                                     from_network, 50);
}

void ResourcePrefetchPredictor::ReportPredictedAccuracyStatsHelper(
    PrefetchKeyType key_type,
    const ResourcePrefetcher::RequestVector& predicted,
    const std::map<GURL, bool>& actual,
    size_t total_resources_fetched_from_network,
    size_t max_assumed_prefetched) const {
  int prefetch_cached = 0, prefetch_network = 0, prefetch_missed = 0;
  int num_assumed_prefetched = std::min(predicted.size(),
                                        max_assumed_prefetched);
  if (num_assumed_prefetched == 0)
    return;

  for (int i = 0; i < num_assumed_prefetched; ++i) {
    const ResourcePrefetcher::Request& row = *(predicted[i]);
    std::map<GURL, bool>::const_iterator it = actual.find(row.resource_url);
    if (it == actual.end()) {
      ++prefetch_missed;
    } else if (it->second) {
      ++prefetch_cached;
    } else {
      ++prefetch_network;
    }
  }

  std::string prefix = key_type == PREFETCH_KEY_TYPE_HOST ?
      "ResourcePrefetchPredictor.Host.Predicted" :
      "ResourcePrefetchPredictor.Url.Predicted";
  std::string suffix = "_" + base::IntToString(max_assumed_prefetched);

  // Macros to avoid using the STATIC_HISTOGRAM_POINTER_BLOCK in UMA_HISTOGRAM
  // definitions.
#define RPP_PREDICTED_HISTOGRAM_COUNTS(name, value) \
  { \
    std::string full_name = prefix + name + suffix; \
    base::HistogramBase* histogram = base::Histogram::FactoryGet( \
        full_name, 1, 1000000, 50, \
        base::Histogram::kUmaTargetedHistogramFlag); \
    histogram->Add(value); \
  }

#define RPP_PREDICTED_HISTOGRAM_PERCENTAGE(name, value) \
  { \
    std::string full_name = prefix + name + suffix; \
    base::HistogramBase* histogram = base::LinearHistogram::FactoryGet( \
        full_name, 1, 101, 102, base::Histogram::kUmaTargetedHistogramFlag); \
    histogram->Add(value); \
  }

  RPP_PREDICTED_HISTOGRAM_COUNTS("PrefetchCount", num_assumed_prefetched);
  RPP_PREDICTED_HISTOGRAM_COUNTS("PrefetchMisses_Count", prefetch_missed);
  RPP_PREDICTED_HISTOGRAM_COUNTS("PrefetchFromCache_Count", prefetch_cached);
  RPP_PREDICTED_HISTOGRAM_COUNTS("PrefetchFromNetwork_Count", prefetch_network);

  RPP_PREDICTED_HISTOGRAM_PERCENTAGE(
      "PrefetchMisses_PercentOfTotalPrefetched",
      prefetch_missed * 100.0 / num_assumed_prefetched);
  RPP_PREDICTED_HISTOGRAM_PERCENTAGE(
      "PrefetchFromCache_PercentOfTotalPrefetched",
      prefetch_cached * 100.0 / num_assumed_prefetched);
  RPP_PREDICTED_HISTOGRAM_PERCENTAGE(
      "PrefetchFromNetwork_PercentOfTotalPrefetched",
      prefetch_network * 100.0 / num_assumed_prefetched);

  // Measure the ratio of total number of resources prefetched from network vs
  // the total number of resources fetched by the page from the network.
  if (total_resources_fetched_from_network > 0) {
    RPP_PREDICTED_HISTOGRAM_PERCENTAGE(
        "PrefetchFromNetworkPercentOfTotalFromNetwork",
        prefetch_network * 100.0 / total_resources_fetched_from_network);
  }

#undef RPP_HISTOGRAM_MEDIUM_TIMES
#undef RPP_PREDICTED_HISTOGRAM_PERCENTAGE
#undef RPP_PREDICTED_HISTOGRAM_COUNTS
}

void ResourcePrefetchPredictor::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (INITIALIZED != initialization_state_)
    return;

  if (all_history) {
    DeleteAllUrls();
    UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.ReportingEvent",
                              REPORTING_EVENT_ALL_HISTORY_CLEARED,
                              REPORTING_EVENT_COUNT);
  } else {
    DeleteUrls(deleted_rows);
    UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.ReportingEvent",
                              REPORTING_EVENT_PARTIAL_HISTORY_CLEARED,
                              REPORTING_EVENT_COUNT);
  }
}

void ResourcePrefetchPredictor::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  OnHistoryAndCacheLoaded();
  history_service_observer_.Remove(history_service);
}

void ResourcePrefetchPredictor::ConnectToHistoryService() {
  // Register for HistoryServiceLoading if it is not ready.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service)
    return;
  if (history_service->BackendLoaded()) {
    // HistoryService is already loaded. Continue with Initialization.
    OnHistoryAndCacheLoaded();
    return;
  }
  DCHECK(!history_service_observer_.IsObserving(history_service));
  history_service_observer_.Add(history_service);
  return;
}

}  // namespace predictors
