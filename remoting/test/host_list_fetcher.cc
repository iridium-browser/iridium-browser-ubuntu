// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/host_list_fetcher.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/url_request_context_getter.h"

namespace remoting {
namespace test {

HostListFetcher::HostListFetcher() {
}

HostListFetcher::~HostListFetcher() {
}

void HostListFetcher::RetrieveHostlist(
    const std::string& access_token,
    const HostlistCallback& callback) {

  VLOG(2) << "HostListFetcher::RetrieveHostlist() called";

  DCHECK(!access_token.empty());
  DCHECK(!callback.is_null());
  DCHECK(hostlist_callback_.is_null());

  hostlist_callback_ = callback;

  request_context_getter_ = new remoting::URLRequestContextGetter(
      base::ThreadTaskRunnerHandle::Get(),   // network_runner
      base::ThreadTaskRunnerHandle::Get());  // file_runner

  request_ = net::URLFetcher::Create(
      GURL(kHostListProdRequestUrl), net::URLFetcher::GET, this);
  request_->SetRequestContext(request_context_getter_.get());
  request_->AddExtraRequestHeader("Authorization: OAuth " + access_token);
  request_->Start();
}

bool HostListFetcher::ProcessResponse(
    std::vector<HostInfo>* hostlist) {
  int response_code = request_->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    LOG(ERROR) << "Hostlist request failed with error code: " << response_code;
    return false;
  }

  std::string response_string;
  if (!request_->GetResponseAsString(&response_string)) {
    LOG(ERROR) << "Failed to retrieve Hostlist response data";
    return false;
  }

  scoped_ptr<base::Value> response_value(
      base::JSONReader::Read(response_string));
  if (!response_value ||
      !response_value->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Failed to parse response string to JSON";
    return false;
  }

  const base::DictionaryValue* response;
  if (!response_value->GetAsDictionary(&response)) {
    LOG(ERROR) << "Failed to convert parsed JSON to a dictionary object";
    return false;
  }

  const base::DictionaryValue* data = nullptr;
  if (!response->GetDictionary("data", &data)) {
    LOG(ERROR) << "Hostlist response data is empty";
    return false;
  }

  const base::ListValue* hosts = nullptr;
  if (!data->GetList("items", &hosts)) {
    LOG(ERROR) << "Failed to find hosts in Hostlist response data";
    return false;
  }

  // Any host_info with malformed data will not be added to the hostlist.
  base::DictionaryValue* host_dict;
  for (base::Value* host_info : *hosts) {
    HostInfo host;
    if (host_info->GetAsDictionary(&host_dict) &&
        host.ParseHostInfo(*host_dict)) {
      hostlist->push_back(host);
    }
  }
  return true;
}

void HostListFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source);
  VLOG(2) << "URL Fetch Completed for: " << source->GetOriginalURL();

  std::vector<HostInfo> hostlist;

  if (!ProcessResponse(&hostlist)) {
    hostlist.clear();
  }
  base::ResetAndReturn(&hostlist_callback_).Run(hostlist);
}

}  // namespace test
}  // namespace remoting
