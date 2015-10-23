// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/experimental/supervised_user_async_url_checker.h"

#include <string>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/browser/google_util.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;
using net::URLRequestStatus;

namespace {

const char kApiUrl[] = "https://safesearch.googleapis.com/v1:classify";
const char kDataContentType[] = "application/x-www-form-urlencoded";
const char kDataFormat[] = "key=%s&urls=%s";

const size_t kDefaultCacheSize = 1000;

// Builds the POST data for SafeSearch API requests.
std::string BuildRequestData(const std::string& api_key, const GURL& url) {
  std::string query = net::EscapeQueryParamValue(url.spec(), true);
  return base::StringPrintf(kDataFormat, api_key.c_str(), query.c_str());
}

// Creates a URLFetcher to call the SafeSearch API for |url|.
scoped_ptr<net::URLFetcher> CreateFetcher(URLFetcherDelegate* delegate,
                                          URLRequestContextGetter* context,
                                          const std::string& api_key,
                                          const GURL& url) {
  scoped_ptr<net::URLFetcher> fetcher = URLFetcher::Create(
      0, GURL(kApiUrl), URLFetcher::POST, delegate);
  fetcher->SetUploadData(kDataContentType, BuildRequestData(api_key, url));
  fetcher->SetRequestContext(context);
  fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  return fetcher.Pass();
}

// Parses a SafeSearch API |response| and stores the result in |is_porn|.
// On errors, returns false and doesn't set |is_porn|.
bool ParseResponse(const std::string& response, bool* is_porn) {
  scoped_ptr<base::Value> value = base::JSONReader::Read(response);
  const base::DictionaryValue* dict = nullptr;
  if (!value || !value->GetAsDictionary(&dict)) {
    DLOG(WARNING) << "ParseResponse failed to parse global dictionary";
    return false;
  }
  const base::ListValue* classifications_list = nullptr;
  if (!dict->GetList("classifications", &classifications_list)) {
    DLOG(WARNING) << "ParseResponse failed to parse classifications list";
    return false;
  }
  if (classifications_list->GetSize() != 1) {
    DLOG(WARNING) << "ParseResponse expected exactly one result";
    return false;
  }
  const base::DictionaryValue* classification_dict = nullptr;
  if (!classifications_list->GetDictionary(0, &classification_dict)) {
    DLOG(WARNING) << "ParseResponse failed to parse classification dict";
    return false;
  }
  classification_dict->GetBoolean("pornography", is_porn);
  return true;
}

}  // namespace

struct SupervisedUserAsyncURLChecker::Check {
  Check(const GURL& url,
        scoped_ptr<net::URLFetcher> fetcher,
        const CheckCallback& callback);
  ~Check();

  GURL url;
  scoped_ptr<net::URLFetcher> fetcher;
  std::vector<CheckCallback> callbacks;
  base::Time start_time;
};

SupervisedUserAsyncURLChecker::Check::Check(
    const GURL& url,
    scoped_ptr<net::URLFetcher> fetcher,
    const CheckCallback& callback)
    : url(url),
      fetcher(fetcher.Pass()),
      callbacks(1, callback),
      start_time(base::Time::Now()) {
}

SupervisedUserAsyncURLChecker::Check::~Check() {}

SupervisedUserAsyncURLChecker::CheckResult::CheckResult(
    SupervisedUserURLFilter::FilteringBehavior behavior, bool uncertain)
    : behavior(behavior), uncertain(uncertain) {
}

SupervisedUserAsyncURLChecker::SupervisedUserAsyncURLChecker(
    URLRequestContextGetter* context)
    : context_(context), cache_(kDefaultCacheSize) {
}

SupervisedUserAsyncURLChecker::SupervisedUserAsyncURLChecker(
    URLRequestContextGetter* context,
    size_t cache_size)
    : context_(context), cache_(cache_size) {
}

SupervisedUserAsyncURLChecker::~SupervisedUserAsyncURLChecker() {}

bool SupervisedUserAsyncURLChecker::CheckURL(const GURL& url,
                                             const CheckCallback& callback) {
  // TODO(treib): Hack: For now, allow all Google URLs to save search QPS. If we
  // ever remove this, we should find a way to allow at least the NTP.
  if (google_util::IsGoogleDomainUrl(url,
                                     google_util::ALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS)) {
    callback.Run(url, SupervisedUserURLFilter::ALLOW, false);
    return true;
  }
  // TODO(treib): Hack: For now, allow all YouTube URLs since YouTube has its
  // own Safety Mode anyway.
  if (google_util::IsYoutubeDomainUrl(url,
                                      google_util::ALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS)) {
    callback.Run(url, SupervisedUserURLFilter::ALLOW, false);
    return true;
  }

  auto cache_it = cache_.Get(url);
  if (cache_it != cache_.end()) {
    const CheckResult& result = cache_it->second;
    DVLOG(1) << "Cache hit! " << url.spec() << " is "
             << (result.behavior == SupervisedUserURLFilter::BLOCK ? "NOT" : "")
             << " safe; certain: " << !result.uncertain;
    callback.Run(url, result.behavior, result.uncertain);
    return true;
  }

  // See if we already have a check in progress for this URL.
  for (Check* check : checks_in_progress_) {
    if (check->url == url) {
      DVLOG(1) << "Adding to pending check for " << url.spec();
      check->callbacks.push_back(callback);
      return false;
    }
  }

  DVLOG(1) << "Checking URL " << url;
  std::string api_key = google_apis::GetSafeSitesAPIKey();
  scoped_ptr<URLFetcher> fetcher(CreateFetcher(this, context_, api_key, url));
  fetcher->Start();
  checks_in_progress_.push_back(new Check(url, fetcher.Pass(), callback));
  return false;
}

void SupervisedUserAsyncURLChecker::OnURLFetchComplete(
    const net::URLFetcher* source) {
  ScopedVector<Check>::iterator it = checks_in_progress_.begin();
  while (it != checks_in_progress_.end()) {
    if (source == (*it)->fetcher.get())
      break;
    ++it;
  }
  DCHECK(it != checks_in_progress_.end());
  Check* check = *it;

  const URLRequestStatus& status = source->GetStatus();
  if (!status.is_success()) {
    DLOG(WARNING) << "URL request failed! Letting through...";
    for (size_t i = 0; i < check->callbacks.size(); i++)
      check->callbacks[i].Run(check->url, SupervisedUserURLFilter::ALLOW, true);
    checks_in_progress_.erase(it);
    return;
  }

  std::string response_body;
  source->GetResponseAsString(&response_body);
  bool is_porn = false;
  bool uncertain = !ParseResponse(response_body, &is_porn);
  SupervisedUserURLFilter::FilteringBehavior behavior =
      is_porn ? SupervisedUserURLFilter::BLOCK : SupervisedUserURLFilter::ALLOW;

  UMA_HISTOGRAM_TIMES("ManagedUsers.SafeSitesDelay",
                      base::Time::Now() - check->start_time);

  cache_.Put(check->url, CheckResult(behavior, uncertain));

  for (size_t i = 0; i < check->callbacks.size(); i++)
    check->callbacks[i].Run(check->url, behavior, uncertain);
  checks_in_progress_.erase(it);
}
