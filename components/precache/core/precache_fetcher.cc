// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_fetcher.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/sha1.h"
#include "base/strings/string_piece.h"
#include "base/task_runner_util.h"
#include "components/precache/core/precache_database.h"
#include "components/precache/core/precache_switches.h"
#include "components/precache/core/proto/precache.pb.h"
#include "components/precache/core/proto/quota.pb.h"
#include "components/precache/core/proto/unfinished_work.pb.h"
#include "net/base/completion_callback.h"
#include "net/base/escape.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace precache {

// The following flags are for privacy reasons. For example, if a user clears
// their cookies, but a tracking beacon is prefetched and the beacon specifies
// its source URL in a URL param, the beacon site would be able to rebuild a
// profile of the user. All three flags should occur together, or not at all,
// per
// https://groups.google.com/a/chromium.org/d/topic/net-dev/vvcodRV6SdM/discussion.
const int kNoTracking =
    net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
    net::LOAD_DO_NOT_SEND_AUTH_DATA;

namespace {

// The maximum number of URLFetcher requests that can be on flight in parallel.
const int kMaxParallelFetches = 10;

// The maximum for the Precache.Fetch.ResponseBytes.* histograms. We set this to
// a number we expect to be in the 99th percentile for the histogram, give or
// take.
const int kMaxResponseBytes = 500 * 1024 * 1024;

GURL GetDefaultConfigURL() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kPrecacheConfigSettingsURL)) {
    return GURL(
        command_line.GetSwitchValueASCII(switches::kPrecacheConfigSettingsURL));
  }

#if defined(PRECACHE_CONFIG_SETTINGS_URL)
  return GURL(PRECACHE_CONFIG_SETTINGS_URL);
#else
  // The precache config settings URL could not be determined, so return an
  // empty, invalid GURL.
  return GURL();
#endif
}

std::string GetDefaultManifestURLPrefix() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kPrecacheManifestURLPrefix)) {
    return command_line.GetSwitchValueASCII(
        switches::kPrecacheManifestURLPrefix);
  }

#if defined(PRECACHE_MANIFEST_URL_PREFIX)
  return PRECACHE_MANIFEST_URL_PREFIX;
#else
  // The precache manifest URL prefix could not be determined, so return an
  // empty string.
  return std::string();
#endif
}

// Attempts to parse a protobuf message from the response string of a
// URLFetcher. If parsing is successful, the message parameter will contain the
// parsed protobuf and this function will return true. Otherwise, returns false.
bool ParseProtoFromFetchResponse(const net::URLFetcher& source,
                                 ::google::protobuf::MessageLite* message) {
  std::string response_string;

  if (!source.GetStatus().is_success()) {
    DLOG(WARNING) << "Fetch failed: " << source.GetOriginalURL().spec();
    return false;
  }
  if (!source.GetResponseAsString(&response_string)) {
    DLOG(WARNING) << "No response string present: "
                  << source.GetOriginalURL().spec();
    return false;
  }
  if (!message->ParseFromString(response_string)) {
    DLOG(WARNING) << "Unable to parse proto served from "
                  << source.GetOriginalURL().spec();
    return false;
  }
  return true;
}

// Returns the resource selection bitset from the |manifest| for the given
// |experiment_id|. By default all resource will be selected if the experiment
// group is not found.
uint64_t GetResourceBitset(const PrecacheManifest& manifest,
                           uint32_t experiment_id) {
  if (manifest.has_experiments()) {
    const auto& resource_bitset_map =
        manifest.experiments().resources_by_experiment_group();
    const auto& resource_bitset_it = resource_bitset_map.find(experiment_id);
    if (resource_bitset_it != resource_bitset_map.end())
      return resource_bitset_it->second.bitset();
  }
  return ~0ULL;
}

// URLFetcherResponseWriter that ignores the response body, in order to avoid
// the unnecessary memory usage. Use it rather than the default if you don't
// care about parsing the response body. We use it below as a means to populate
// the cache with requested resource URLs.
class URLFetcherNullWriter : public net::URLFetcherResponseWriter {
 public:
  int Initialize(const net::CompletionCallback& callback) override {
    return net::OK;
  }

  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override {
    return num_bytes;
  }

  int Finish(const net::CompletionCallback& callback) override {
    return net::OK;
  }
};

// Returns the base64 encoded resource URL hashes. The resource URLs are hashed
// individually, and 8 bytes of each hash is appended together, which is then
// encoded to base64.
std::string GetResourceURLBase64Hash(const std::vector<GURL>& urls) {
  // Each resource hash uses 8 bytes, instead of the 20 bytes of sha1 hash, as a
  // tradeoff between sending more bytes and reducing hash collisions.
  const size_t kHashBytesSize = 8;
  std::string hashes;
  hashes.reserve(urls.size() * kHashBytesSize);

  for (const auto& url : urls) {
    const std::string& url_spec = url.spec();
    unsigned char sha1_hash[base::kSHA1Length];
    base::SHA1HashBytes(
        reinterpret_cast<const unsigned char*>(url_spec.c_str()),
        url_spec.size(), sha1_hash);
    hashes.append(reinterpret_cast<const char*>(sha1_hash), kHashBytesSize);
  }
  base::Base64Encode(hashes, &hashes);
  return hashes;
}

// Retrieves the manifest info on the DB thread. Manifest info for each of the
// hosts in |hosts_to_fetch|, is added to |hosts_info|.
std::deque<ManifestHostInfo> RetrieveManifestInfo(
    const base::WeakPtr<PrecacheDatabase>& precache_database,
    std::vector<std::string> hosts_to_fetch) {
  std::deque<ManifestHostInfo> hosts_info;
  if (!precache_database)
    return hosts_info;

  for (const auto& host : hosts_to_fetch) {
    auto referrer_host_info = precache_database->GetReferrerHost(host);
    if (referrer_host_info.id != PrecacheReferrerHostEntry::kInvalidId) {
      std::vector<GURL> used_urls, unused_urls;
      precache_database->GetURLListForReferrerHost(referrer_host_info.id,
                                                   &used_urls, &unused_urls);
      hosts_info.push_back(ManifestHostInfo(
          referrer_host_info.id, host, GetResourceURLBase64Hash(used_urls),
          GetResourceURLBase64Hash(unused_urls)));
    } else {
      hosts_info.push_back(
          ManifestHostInfo(PrecacheReferrerHostEntry::kInvalidId, host,
                           std::string(), std::string()));
    }
  }
  return hosts_info;
}

PrecacheQuota RetrieveQuotaInfo(
    const base::WeakPtr<PrecacheDatabase>& precache_database) {
  PrecacheQuota quota;
  if (precache_database) {
    quota = precache_database->GetQuota();
  }
  return quota;
}

// Returns true if the |quota| time has expired.
bool IsQuotaTimeExpired(const PrecacheQuota& quota,
                        const base::Time& time_now) {
  // Quota expires one day after the start time.
  base::Time start_time = base::Time::FromInternalValue(quota.start_time());
  return start_time > time_now ||
         start_time + base::TimeDelta::FromDays(1) < time_now;
}

}  // namespace

PrecacheFetcher::Fetcher::Fetcher(
    net::URLRequestContextGetter* request_context,
    const GURL& url,
    const std::string& referrer,
    const base::Callback<void(const Fetcher&)>& callback,
    bool is_resource_request,
    size_t max_bytes)
    : request_context_(request_context),
      url_(url),
      referrer_(referrer),
      callback_(callback),
      is_resource_request_(is_resource_request),
      max_bytes_(max_bytes),
      response_bytes_(0),
      network_response_bytes_(0),
      was_cached_(false) {
  DCHECK(url.is_valid());
  if (is_resource_request_)
    LoadFromCache();
  else
    LoadFromNetwork();
}

PrecacheFetcher::Fetcher::~Fetcher() {}

void PrecacheFetcher::Fetcher::LoadFromCache() {
  fetch_stage_ = FetchStage::CACHE;
  cache_url_fetcher_ =
      net::URLFetcher::Create(url_, net::URLFetcher::GET, this);
  cache_url_fetcher_->SetRequestContext(request_context_);
  cache_url_fetcher_->SetLoadFlags(net::LOAD_ONLY_FROM_CACHE | kNoTracking);
  std::unique_ptr<URLFetcherNullWriter> null_writer(new URLFetcherNullWriter);
  cache_url_fetcher_->SaveResponseWithWriter(std::move(null_writer));
  cache_url_fetcher_->Start();
}

void PrecacheFetcher::Fetcher::LoadFromNetwork() {
  fetch_stage_ = FetchStage::NETWORK;
  network_url_fetcher_ =
      net::URLFetcher::Create(url_, net::URLFetcher::GET, this);
  network_url_fetcher_->SetRequestContext(request_context_);
  if (is_resource_request_) {
    // LOAD_VALIDATE_CACHE allows us to refresh Date headers for resources
    // already in the cache. The Date headers are updated from 304s as well as
    // 200s.
    network_url_fetcher_->SetLoadFlags(net::LOAD_VALIDATE_CACHE | kNoTracking);
    // We don't need a copy of the response body for resource requests. The
    // request is issued only to populate the browser cache.
    std::unique_ptr<URLFetcherNullWriter> null_writer(new URLFetcherNullWriter);
    network_url_fetcher_->SaveResponseWithWriter(std::move(null_writer));
  } else {
    // Config and manifest requests do not need to be revalidated. It's okay if
    // they expire from the cache minutes after we request them.
    network_url_fetcher_->SetLoadFlags(kNoTracking);
  }
  network_url_fetcher_->Start();
}

void PrecacheFetcher::Fetcher::OnURLFetchDownloadProgress(
    const net::URLFetcher* source,
    int64_t current,
    int64_t total,
    int64_t current_network_bytes) {
  // If network bytes going over the per-resource download cap.
  if (fetch_stage_ == FetchStage::NETWORK &&
      // |current_network_bytes| is guaranteed to be non-negative, so this cast
      // is safe.
      static_cast<size_t>(current_network_bytes) > max_bytes_) {
    VLOG(1) << "Cancelling " << url_ << ": (" << current << "/" << total
            << ") is over " << max_bytes_;

    // Call the completion callback, to attempt the next download, or to trigger
    // cleanup in precache_delegate_->OnDone().
    response_bytes_ = current;
    network_response_bytes_ = current_network_bytes;
    was_cached_ = source->WasCached();

    UMA_HISTOGRAM_CUSTOM_COUNTS("Precache.Fetch.ResponseBytes.NetworkWasted",
                                network_response_bytes_, 1,
                                1024 * 1024 /* 1 MB */, 100);
    // Cancel the download.
    network_url_fetcher_.reset();
    callback_.Run(*this);
  }
}

void PrecacheFetcher::Fetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  CHECK(source);
  if (fetch_stage_ == FetchStage::CACHE &&
      (source->GetStatus().error() == net::ERR_CACHE_MISS ||
       (source->GetResponseHeaders() &&
        source->GetResponseHeaders()->HasValidators()))) {
    // If the resource was not found in the cache, request it from the
    // network.
    //
    // If the resource was found in the cache, but contains validators,
    // request a refresh. The presence of validators increases the chance that
    // we get a 304 response rather than a full one, thus allowing us to
    // refresh the cache with minimal network load.
    LoadFromNetwork();
    return;
  }

  // If any of:
  // - The request was for a config or manifest.
  // - The resource was a cache hit without validators.
  // - The response came from the network.
  // Then Fetcher is done with this URL and can return control to the caller.
  response_bytes_ = source->GetReceivedResponseContentLength();
  network_response_bytes_ = source->GetTotalReceivedBytes();
  was_cached_ = source->WasCached();
  callback_.Run(*this);
}

// static
void PrecacheFetcher::RecordCompletionStatistics(
    const PrecacheUnfinishedWork& unfinished_work,
    size_t remaining_manifest_urls_to_fetch,
    size_t remaining_resource_urls_to_fetch) {
  // These may be unset in tests.
  if (!unfinished_work.has_start_time())
    return;
  base::TimeDelta time_to_fetch =
      base::Time::Now() -
      base::Time::FromInternalValue(unfinished_work.start_time());
  UMA_HISTOGRAM_CUSTOM_TIMES("Precache.Fetch.TimeToComplete", time_to_fetch,
                             base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromHours(4), 50);

  // Number of manifests for which we have downloaded all resources.
  int manifests_completed =
      unfinished_work.num_manifest_urls() - remaining_manifest_urls_to_fetch;

  // If there are resource URLs left to fetch, the last manifest is not yet
  // completed.
  if (remaining_resource_urls_to_fetch > 0)
    --manifests_completed;

  DCHECK_GE(manifests_completed, 0);
  int percent_completed = unfinished_work.num_manifest_urls() == 0
                              ? 0
                              : (static_cast<double>(manifests_completed) /
                                  unfinished_work.num_manifest_urls() * 100);

  UMA_HISTOGRAM_PERCENTAGE("Precache.Fetch.PercentCompleted",
                           percent_completed);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Precache.Fetch.ResponseBytes.Total",
                                unfinished_work.total_bytes(),
                                1, kMaxResponseBytes, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Precache.Fetch.ResponseBytes.Network",
                              unfinished_work.network_bytes(),
                              1, kMaxResponseBytes,
                              100);
}

// static
std::string PrecacheFetcher::GetResourceURLBase64HashForTesting(
    const std::vector<GURL>& urls) {
  return GetResourceURLBase64Hash(urls);
}

PrecacheFetcher::PrecacheFetcher(
    net::URLRequestContextGetter* request_context,
    const GURL& config_url,
    const std::string& manifest_url_prefix,
    std::unique_ptr<PrecacheUnfinishedWork> unfinished_work,
    uint32_t experiment_id,
    const base::WeakPtr<PrecacheDatabase>& precache_database,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner,
    PrecacheFetcher::PrecacheDelegate* precache_delegate)
    : request_context_(request_context),
      config_url_(config_url),
      manifest_url_prefix_(manifest_url_prefix),
      precache_database_(precache_database),
      db_task_runner_(std::move(db_task_runner)),
      precache_delegate_(precache_delegate),
      pool_(kMaxParallelFetches),
      experiment_id_(experiment_id) {
  DCHECK(request_context_.get());  // Request context must be non-NULL.
  DCHECK(precache_delegate_);  // Precache delegate must be non-NULL.

  DCHECK_NE(GURL(), GetDefaultConfigURL())
      << "Could not determine the precache config settings URL.";
  DCHECK_NE(std::string(), GetDefaultManifestURLPrefix())
      << "Could not determine the default precache manifest URL prefix.";
  DCHECK(unfinished_work);

  // Copy resources to member variable as a convenience.
  // TODO(rajendrant): Consider accessing these directly from the proto, by
  // keeping track of the current resource index.
  for (const auto& resource : unfinished_work->resource()) {
    if (resource.has_url() && resource.has_top_host_name()) {
      resources_to_fetch_.emplace_back(GURL(resource.url()),
                                       resource.top_host_name());
    }
  }
  unfinished_work_ = std::move(unfinished_work);
}

PrecacheFetcher::~PrecacheFetcher() {
}

std::unique_ptr<PrecacheUnfinishedWork> PrecacheFetcher::CancelPrecaching() {
  // This could get called multiple times, and it should be handled gracefully.
  if (!unfinished_work_)
    return nullptr;

  unfinished_work_->clear_resource();
  if (unfinished_work_->has_config_settings()) {
    // If config fetch is incomplete, |top_hosts_to_fetch_| will be empty and
    // top hosts should be left as is in |unfinished_work_|.
    unfinished_work_->clear_top_host();
    for (const auto& top_host : top_hosts_to_fetch_) {
      unfinished_work_->add_top_host()->set_hostname(top_host.hostname);
    }
  }
  for (const auto& resource : resources_to_fetch_) {
    auto new_resource = unfinished_work_->add_resource();
    new_resource->set_url(resource.first.spec());
    new_resource->set_top_host_name(resource.second);
  }
  for (const auto& it : pool_.elements()) {
    const Fetcher* fetcher = it.first;
    GURL config_url =
        config_url_.is_empty() ? GetDefaultConfigURL() : config_url_;
    if (fetcher->is_resource_request()) {
      auto resource = unfinished_work_->add_resource();
      resource->set_url(fetcher->url().spec());
      resource->set_top_host_name(fetcher->referrer());
    } else if (fetcher->url() != config_url) {
      unfinished_work_->add_top_host()->set_hostname(fetcher->referrer());
    }
  }
  top_hosts_to_fetch_.clear();
  resources_to_fetch_.clear();
  pool_.DeleteAll();
  return std::move(unfinished_work_);
}

void PrecacheFetcher::Start() {
  if (unfinished_work_->has_config_settings()) {
    DCHECK(unfinished_work_->has_start_time());
    DetermineManifests();
    return;
  }

  GURL config_url =
      config_url_.is_empty() ? GetDefaultConfigURL() : config_url_;

  DCHECK(config_url.is_valid()) << "Config URL not valid: "
                                << config_url.possibly_invalid_spec();

  // Fetch the precache configuration settings from the server.
  DCHECK(pool_.IsEmpty()) << "All parallel requests should be available";
  VLOG(3) << "Fetching " << config_url;
  pool_.Add(base::WrapUnique(new Fetcher(
      request_context_.get(), config_url, std::string(),
      base::Bind(&PrecacheFetcher::OnConfigFetchComplete, AsWeakPtr()),
      false /* is_resource_request */, std::numeric_limits<int32_t>::max())));
}

void PrecacheFetcher::StartNextResourceFetch() {
  DCHECK(unfinished_work_->has_config_settings());
  while (!resources_to_fetch_.empty() && pool_.IsAvailable()) {
    const auto& resource = resources_to_fetch_.front();
    const size_t max_bytes = std::min(
        quota_.remaining(),
        std::min(unfinished_work_->config_settings().max_bytes_per_resource(),
                 unfinished_work_->config_settings().max_bytes_total() -
                     unfinished_work_->total_bytes()));
    VLOG(3) << "Fetching " << resource.first << " " << resource.second;
    pool_.Add(base::WrapUnique(new Fetcher(
        request_context_.get(), resource.first, resource.second,
        base::Bind(&PrecacheFetcher::OnResourceFetchComplete, AsWeakPtr()),
        true /* is_resource_request */, max_bytes)));

    resources_to_fetch_.pop_front();
  }
}

void PrecacheFetcher::StartNextManifestFetch() {
  if (top_hosts_to_fetch_.empty() || !pool_.IsAvailable())
    return;

  // We only fetch one manifest at a time to keep the size of
  // resources_to_fetch_ as small as possible.
  VLOG(3) << "Fetching " << top_hosts_to_fetch_.front().manifest_url;
  pool_.Add(base::WrapUnique(new Fetcher(
      request_context_.get(), top_hosts_to_fetch_.front().manifest_url,
      top_hosts_to_fetch_.front().hostname,
      base::Bind(&PrecacheFetcher::OnManifestFetchComplete, AsWeakPtr()),
      false /* is_resource_request */, std::numeric_limits<int32_t>::max())));
  top_hosts_to_fetch_.pop_front();
}

void PrecacheFetcher::NotifyDone(
    size_t remaining_manifest_urls_to_fetch,
    size_t remaining_resource_urls_to_fetch) {
  RecordCompletionStatistics(*unfinished_work_,
                             remaining_manifest_urls_to_fetch,
                             remaining_resource_urls_to_fetch);
  precache_delegate_->OnDone();
}

void PrecacheFetcher::StartNextFetch() {
  DCHECK(unfinished_work_->has_config_settings());

  // If over the precache total size cap or daily quota, then stop prefetching.
  if ((unfinished_work_->total_bytes() >
       unfinished_work_->config_settings().max_bytes_total()) ||
      quota_.remaining() == 0) {
    size_t pending_manifests_in_pool = 0;
    size_t pending_resources_in_pool = 0;
    for (const auto& element_pair : pool_.elements()) {
      const Fetcher* fetcher = element_pair.first;
      if (fetcher->is_resource_request())
        pending_resources_in_pool++;
      else if (fetcher->url() != config_url_)
        pending_manifests_in_pool++;
    }
    pool_.DeleteAll();
    NotifyDone(top_hosts_to_fetch_.size() + pending_manifests_in_pool,
               resources_to_fetch_.size() + pending_resources_in_pool);
    return;
  }

  StartNextResourceFetch();
  StartNextManifestFetch();
  if (top_hosts_to_fetch_.empty() && resources_to_fetch_.empty() &&
      pool_.IsEmpty()) {
    // There are no more URLs to fetch, so end the precache cycle.
    NotifyDone(0, 0);
    // OnDone may have deleted this PrecacheFetcher, so don't do anything after
    // it is called.
  }
}

void PrecacheFetcher::OnConfigFetchComplete(const Fetcher& source) {
  UpdateStats(source.response_bytes(), source.network_response_bytes());
  if (source.network_url_fetcher() == nullptr) {
    pool_.DeleteAll();  // Cancel any other ongoing request.
  } else {
    // Attempt to parse the config proto. On failure, continue on with the
    // default configuration.
    ParseProtoFromFetchResponse(
        *source.network_url_fetcher(),
        unfinished_work_->mutable_config_settings());
    pool_.Delete(source);
    DetermineManifests();
  }
}

void PrecacheFetcher::DetermineManifests() {
  DCHECK(unfinished_work_->has_config_settings());

  std::vector<std::string> top_hosts_to_fetch;
  std::unique_ptr<std::deque<ManifestHostInfo>> top_hosts_info(
      new std::deque<ManifestHostInfo>);
  // Keep track of manifest URLs that are being fetched, in order to elide
  // duplicates.
  std::set<base::StringPiece> seen_top_hosts;
  int64_t rank = 0;

  for (const auto& host : unfinished_work_->top_host()) {
    ++rank;
    if (rank > unfinished_work_->config_settings().top_sites_count())
      break;
    if (seen_top_hosts.insert(host.hostname()).second)
      top_hosts_to_fetch.push_back(host.hostname());
  }

  // Attempt to fetch manifests for starting hosts up to the maximum top sites
  // count. If a manifest does not exist for a particular starting host, then
  // the fetch will fail, and that starting host will be ignored. Starting
  // hosts are not added if this is a continuation from a previous precache
  // session.
  if (resources_to_fetch_.empty()) {
    for (const std::string& host :
         unfinished_work_->config_settings().forced_site()) {
      if (seen_top_hosts.insert(host).second)
        top_hosts_to_fetch.push_back(host);
    }
  }
  // We only fetch one manifest at a time to keep the size of
  // resources_to_fetch_ as small as possible.
  PostTaskAndReplyWithResult(
      db_task_runner_.get(), FROM_HERE,
      base::Bind(&RetrieveManifestInfo, precache_database_,
                 std::move(top_hosts_to_fetch)),
      base::Bind(&PrecacheFetcher::OnManifestInfoRetrieved, AsWeakPtr()));
}

void PrecacheFetcher::OnManifestInfoRetrieved(
    std::deque<ManifestHostInfo> manifests_info) {
  const std::string prefix = manifest_url_prefix_.empty()
                                 ? GetDefaultManifestURLPrefix()
                                 : manifest_url_prefix_;
  if (!GURL(prefix).is_valid()) {
    // Don't attempt to fetch any manifests if the manifest URL prefix
    // is invalid.
    top_hosts_to_fetch_.clear();
    unfinished_work_->set_num_manifest_urls(manifests_info.size());
    NotifyDone(manifests_info.size(), resources_to_fetch_.size());
    return;
  }

  top_hosts_to_fetch_ = std::move(manifests_info);
  for (auto& manifest : top_hosts_to_fetch_) {
    manifest.manifest_url =
        GURL(prefix +
             net::EscapeQueryParamValue(
                 net::EscapeQueryParamValue(manifest.hostname, false), false));
    if (manifest.manifest_id != PrecacheReferrerHostEntry::kInvalidId) {
      manifest.manifest_url = net::AppendOrReplaceQueryParameter(
          manifest.manifest_url, "manifest",
          std::to_string(manifest.manifest_id));
      manifest.manifest_url = net::AppendOrReplaceQueryParameter(
          manifest.manifest_url, "used_resources", manifest.used_url_hash);
      manifest.manifest_url = net::AppendOrReplaceQueryParameter(
          manifest.manifest_url, "unused_resources", manifest.unused_url_hash);
      DCHECK(manifest.manifest_url.is_valid());
    }
  }
  unfinished_work_->set_num_manifest_urls(top_hosts_to_fetch_.size());

  PostTaskAndReplyWithResult(
      db_task_runner_.get(), FROM_HERE,
      base::Bind(&RetrieveQuotaInfo, precache_database_),
      base::Bind(&PrecacheFetcher::OnQuotaInfoRetrieved, AsWeakPtr()));
}

void PrecacheFetcher::OnQuotaInfoRetrieved(const PrecacheQuota& quota) {
  quota_ = quota;
  base::Time time_now = base::Time::Now();
  if (IsQuotaTimeExpired(quota_, time_now)) {
    // This is a new day. Update daily quota, that starts today and expires by
    // end of today.
    quota_.set_start_time(time_now.LocalMidnight().ToInternalValue());
    quota_.set_remaining(
        unfinished_work_->config_settings().daily_quota_total());
    db_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PrecacheDatabase::SaveQuota, precache_database_, quota_));
  }
  StartNextFetch();
}

ManifestHostInfo::ManifestHostInfo(int64_t manifest_id,
                                   const std::string& hostname,
                                   const std::string& used_url_hash,
                                   const std::string& unused_url_hash)
    : manifest_id(manifest_id),
      hostname(hostname),
      used_url_hash(used_url_hash),
      unused_url_hash(unused_url_hash) {}

ManifestHostInfo::~ManifestHostInfo() {}

ManifestHostInfo::ManifestHostInfo(ManifestHostInfo&&) = default;

ManifestHostInfo& ManifestHostInfo::operator=(ManifestHostInfo&&) = default;

void PrecacheFetcher::OnManifestFetchComplete(const Fetcher& source) {
  DCHECK(unfinished_work_->has_config_settings());
  UpdateStats(source.response_bytes(), source.network_response_bytes());
  if (source.network_url_fetcher() == nullptr) {
    pool_.DeleteAll();  // Cancel any other ongoing request.
  } else {
    PrecacheManifest manifest;

    if (ParseProtoFromFetchResponse(*source.network_url_fetcher(), &manifest)) {
      const int32_t len =
          std::min(manifest.resource_size(),
                   unfinished_work_->config_settings().top_resources_count());
      const uint64_t resource_bitset =
          GetResourceBitset(manifest, experiment_id_);
      for (int i = 0; i < len; ++i) {
        if (((0x1ULL << i) & resource_bitset) &&
            manifest.resource(i).has_url()) {
          GURL url(manifest.resource(i).url());
          if (url.is_valid()) {
            resources_to_fetch_.emplace_back(url, source.referrer());
          }
        }
      }
      db_task_runner_->PostTask(
          FROM_HERE, base::Bind(&PrecacheDatabase::UpdatePrecacheReferrerHost,
                                precache_database_, source.referrer(),
                                manifest.id().id(), base::Time::Now()));
    }
  }

  pool_.Delete(source);
  StartNextFetch();
}

void PrecacheFetcher::OnResourceFetchComplete(const Fetcher& source) {
  UpdateStats(source.response_bytes(), source.network_response_bytes());

  db_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PrecacheDatabase::RecordURLPrefetch, precache_database_,
                 source.url(), source.referrer(), base::Time::Now(),
                 source.was_cached(), source.response_bytes()));

  pool_.Delete(source);

  // The resource has already been put in the cache during the fetch process, so
  // nothing more needs to be done for the resource.
  StartNextFetch();
}

void PrecacheFetcher::UpdateStats(int64_t response_bytes,
                                  int64_t network_response_bytes) {
  DCHECK_LE(0, response_bytes);
  DCHECK_LE(0, network_response_bytes);

  unfinished_work_->set_total_bytes(
      unfinished_work_->total_bytes() + response_bytes);
  unfinished_work_->set_network_bytes(
      unfinished_work_->network_bytes() + network_response_bytes);

  if (!IsQuotaTimeExpired(quota_, base::Time::Now())) {
    uint64_t used_bytes = static_cast<uint64_t>(network_response_bytes);
    int64_t remaining =
        static_cast<int64_t>(quota_.remaining()) - network_response_bytes;
    if (remaining < 0)
      remaining = 0;
    quota_.set_remaining(
        used_bytes > quota_.remaining() ? 0U : quota_.remaining() - used_bytes);
    db_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PrecacheDatabase::SaveQuota, precache_database_, quota_));
  }
}

}  // namespace precache
