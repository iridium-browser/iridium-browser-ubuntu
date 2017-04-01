// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetcher.h"

#include <iterator>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request_context.h"
#include "url/origin.h"

namespace {

// The size of the buffer used to read the resource.
static const size_t kResourceBufferSizeBytes = 50000;

}  // namespace

namespace predictors {

ResourcePrefetcher::ResourcePrefetcher(
    Delegate* delegate,
    const ResourcePrefetchPredictorConfig& config,
    const GURL& main_frame_url,
    const std::vector<GURL>& urls)
    : state_(INITIALIZED),
      delegate_(delegate),
      config_(config),
      main_frame_url_(main_frame_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::copy(urls.begin(), urls.end(), std::back_inserter(request_queue_));
}

ResourcePrefetcher::~ResourcePrefetcher() {}

void ResourcePrefetcher::Start() {
  TRACE_EVENT_ASYNC_BEGIN0("browser", "ResourcePrefetcher::Prefetch", this);
  DCHECK(thread_checker_.CalledOnValidThread());

  CHECK_EQ(state_, INITIALIZED);
  state_ = RUNNING;

  TryToLaunchPrefetchRequests();
}

void ResourcePrefetcher::Stop() {
  TRACE_EVENT_ASYNC_END0("browser", "ResourcePrefetcher::Prefetch", this);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == FINISHED)
    return;

  state_ = STOPPED;
}

void ResourcePrefetcher::TryToLaunchPrefetchRequests() {
  CHECK(state_ == RUNNING || state_ == STOPPED);

  // Try to launch new requests if the state is RUNNING.
  if (state_ == RUNNING) {
    bool request_available = true;

    // Loop through the requests while we are under the
    // max_prefetches_inflight_per_host_per_navigation limit, looking for a URL
    // for which the max_prefetches_inflight_per_host_per_navigation limit has
    // not been reached. Try to launch as many requests as possible.
    while ((inflight_requests_.size() <
                config_.max_prefetches_inflight_per_navigation) &&
           request_available) {
      auto request_it = request_queue_.begin();
      for (; request_it != request_queue_.end(); ++request_it) {
        const std::string& host = request_it->host();

        std::map<std::string, size_t>::iterator host_it =
            host_inflight_counts_.find(host);
        if (host_it == host_inflight_counts_.end() ||
            host_it->second <
                config_.max_prefetches_inflight_per_host_per_navigation)
          break;
      }
      request_available = request_it != request_queue_.end();

      if (request_available) {
        SendRequest(*request_it);
        request_queue_.erase(request_it);
      }
    }
  }

  // If the inflight_requests_ is empty, we can't launch any more. Finish.
  if (inflight_requests_.empty()) {
    CHECK(host_inflight_counts_.empty());
    CHECK(request_queue_.empty() || state_ == STOPPED);

    state_ = FINISHED;
    delegate_->ResourcePrefetcherFinished(this);
  }
}

void ResourcePrefetcher::SendRequest(const GURL& url) {
  std::unique_ptr<net::URLRequest> url_request =
      delegate_->GetURLRequestContext()->CreateRequest(url, net::LOW, this);
  host_inflight_counts_[url.host()] += 1;

  url_request->set_method("GET");
  url_request->set_first_party_for_cookies(main_frame_url_);
  url_request->set_initiator(url::Origin(main_frame_url_));
  url_request->SetReferrer(main_frame_url_.spec());
  url_request->SetLoadFlags(url_request->load_flags() | net::LOAD_PREFETCH);
  StartURLRequest(url_request.get());
  inflight_requests_.insert(
      std::make_pair(url_request.get(), std::move(url_request)));
}

void ResourcePrefetcher::StartURLRequest(net::URLRequest* request) {
  request->Start();
}

void ResourcePrefetcher::FinishRequest(net::URLRequest* request) {
  auto request_it = inflight_requests_.find(request);
  CHECK(request_it != inflight_requests_.end());

  const std::string host = request->original_url().host();
  std::map<std::string, size_t>::iterator host_it = host_inflight_counts_.find(
      host);
  CHECK_GT(host_it->second, 0U);
  host_it->second -= 1;
  if (host_it->second == 0)
    host_inflight_counts_.erase(host);

  inflight_requests_.erase(request_it);

  TryToLaunchPrefetchRequests();
}

void ResourcePrefetcher::ReadFullResponse(net::URLRequest* request) {
  int bytes_read = 0;
  do {
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(
        kResourceBufferSizeBytes));
    bytes_read = request->Read(buffer.get(), kResourceBufferSizeBytes);
    if (bytes_read == net::ERR_IO_PENDING) {
      return;
    } else if (bytes_read <= 0) {
      FinishRequest(request);
      return;
    }

  } while (bytes_read > 0);
}

void ResourcePrefetcher::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  FinishRequest(request);
}

void ResourcePrefetcher::OnAuthRequired(net::URLRequest* request,
                                        net::AuthChallengeInfo* auth_info) {
  FinishRequest(request);
}

void ResourcePrefetcher::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  FinishRequest(request);
}

void ResourcePrefetcher::OnSSLCertificateError(net::URLRequest* request,
                                               const net::SSLInfo& ssl_info,
                                               bool fatal) {
  FinishRequest(request);
}

void ResourcePrefetcher::OnResponseStarted(net::URLRequest* request,
                                           int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_error != net::OK) {
    FinishRequest(request);
    return;
  }

  // TODO(shishir): Do not read cached entries, or ones that are not cacheable.
  ReadFullResponse(request);
}

void ResourcePrefetcher::OnReadCompleted(net::URLRequest* request,
                                         int bytes_read) {
  DCHECK_NE(net::ERR_IO_PENDING, bytes_read);

  if (bytes_read <= 0) {
    FinishRequest(request);
    return;
  }

  if (bytes_read > 0)
    ReadFullResponse(request);
}

}  // namespace predictors
