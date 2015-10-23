// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/request_sender.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/update_client/configurator.h"
#include "components/update_client/utils.h"
#include "net/url_request/url_fetcher.h"

namespace update_client {

RequestSender::RequestSender(const Configurator& config) : config_(config) {
}

RequestSender::~RequestSender() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void RequestSender::Send(const std::string& request_string,
                         const std::vector<GURL>& urls,
                         const RequestSenderCallback& request_sender_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (urls.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(request_sender_callback, nullptr));
    return;
  }

  request_string_ = request_string;
  urls_ = urls;
  request_sender_callback_ = request_sender_callback;

  cur_url_ = urls_.begin();

  SendInternal();
}

void RequestSender::SendInternal() {
  DCHECK(cur_url_ != urls_.end());
  DCHECK(cur_url_->is_valid());
  DCHECK(thread_checker_.CalledOnValidThread());

  url_fetcher_ = SendProtocolRequest(*cur_url_, request_string_, this,
                                     config_.RequestContext());
}

void RequestSender::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (GetFetchError(*source) == 0) {
    request_sender_callback_.Run(source);
    return;
  }

  if (++cur_url_ != urls_.end() &&
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&RequestSender::SendInternal, base::Unretained(this)))) {
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(request_sender_callback_, source));
}

}  // namespace update_client
