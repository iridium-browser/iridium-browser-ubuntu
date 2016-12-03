// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/system_logs_fetcher_base.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace system_logs {

SystemLogsSource::SystemLogsSource(const std::string& source_name)
    : source_name_(source_name) {
}

SystemLogsSource::~SystemLogsSource() {
}

SystemLogsFetcherBase::SystemLogsFetcherBase()
    : response_(new SystemLogsResponse),
      num_pending_requests_(0) {
}

SystemLogsFetcherBase::~SystemLogsFetcherBase() {}

void SystemLogsFetcherBase::Fetch(const SysLogsFetcherCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = callback;
  for (size_t i = 0; i < data_sources_.size(); ++i) {
    VLOG(1) << "Fetching SystemLogSource: " << data_sources_[i]->source_name();
    data_sources_[i]->Fetch(base::Bind(&SystemLogsFetcherBase::OnFetched,
                                       AsWeakPtr(),
                                       data_sources_[i]->source_name()));
  }
}

void SystemLogsFetcherBase::OnFetched(const std::string& source_name,
                                      SystemLogsResponse* response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  VLOG(1) << "Received SystemLogSource: " << source_name;

  Rewrite(source_name, response);
  AddResponse(source_name, response);
}

void SystemLogsFetcherBase::Rewrite(const std::string& /* source_name */,
                                    SystemLogsResponse* /* response */) {
  // This implementation in the base class is intentionally empty.
}

void SystemLogsFetcherBase::AddResponse(const std::string& source_name,
                                        SystemLogsResponse* response) {
  for (SystemLogsResponse::const_iterator it = response->begin();
       it != response->end();
       ++it) {
    // It is an error to insert an element with a pre-existing key.
    bool ok = response_->insert(*it).second;
    DCHECK(ok) << "Duplicate key found: " << it->first;
  }

  --num_pending_requests_;
  if (num_pending_requests_ > 0)
    return;

  callback_.Run(std::move(response_));
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
}

}  // namespace system_logs
