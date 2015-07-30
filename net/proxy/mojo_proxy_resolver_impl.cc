// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/mojo_proxy_resolver_impl.h"

#include "base/stl_util.h"
#include "mojo/common/url_type_converters.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/proxy/load_state_change_coalescer.h"
#include "net/proxy/mojo_proxy_type_converters.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_script_data.h"

namespace net {
namespace {
const int kLoadStateChangeCoalesceTimeoutMilliseconds = 10;
}

class MojoProxyResolverImpl::Job : public mojo::ErrorHandler {
 public:
  Job(interfaces::ProxyResolverRequestClientPtr client,
      MojoProxyResolverImpl* resolver,
      const GURL& url);
  ~Job() override;

  void Start();

  // Invoked when the LoadState for this job changes.
  void LoadStateChanged(LoadState load_state);

  net::ProxyResolver::RequestHandle request_handle() { return request_handle_; }

 private:
  // mojo::ErrorHandler override.
  // This is invoked in response to the client disconnecting, indicating
  // cancellation.
  void OnConnectionError() override;

  void GetProxyDone(int error);

  void SendLoadStateChanged(LoadState load_state);

  MojoProxyResolverImpl* resolver_;

  interfaces::ProxyResolverRequestClientPtr client_;
  ProxyInfo result_;
  GURL url_;
  net::ProxyResolver::RequestHandle request_handle_;
  bool done_;
  LoadStateChangeCoalescer load_state_change_coalescer_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

MojoProxyResolverImpl::MojoProxyResolverImpl(
    scoped_ptr<net::ProxyResolver> resolver,
    const base::Callback<
        void(const net::ProxyResolver::LoadStateChangedCallback&)>&
        load_state_change_callback_setter)
    : resolver_(resolver.Pass()) {
  load_state_change_callback_setter.Run(base::Bind(
      &MojoProxyResolverImpl::LoadStateChanged, base::Unretained(this)));
}

MojoProxyResolverImpl::~MojoProxyResolverImpl() {
  STLDeleteElements(&resolve_jobs_);
}

void MojoProxyResolverImpl::LoadStateChanged(
    net::ProxyResolver::RequestHandle handle,
    LoadState load_state) {
  auto it = request_handle_to_job_.find(handle);
  DCHECK(it != request_handle_to_job_.end());
  it->second->LoadStateChanged(load_state);
}

void MojoProxyResolverImpl::GetProxyForUrl(
    const mojo::String& url,
    interfaces::ProxyResolverRequestClientPtr client) {
  DVLOG(1) << "GetProxyForUrl(" << url << ")";
  Job* job = new Job(client.Pass(), this, url.To<GURL>());
  bool inserted = resolve_jobs_.insert(job).second;
  DCHECK(inserted);
  job->Start();
}

void MojoProxyResolverImpl::DeleteJob(Job* job) {
  if (job->request_handle())
    request_handle_to_job_.erase(job->request_handle());

  size_t num_erased = resolve_jobs_.erase(job);
  DCHECK(num_erased);
  delete job;
}

MojoProxyResolverImpl::Job::Job(
    interfaces::ProxyResolverRequestClientPtr client,
    MojoProxyResolverImpl* resolver,
    const GURL& url)
    : resolver_(resolver),
      client_(client.Pass()),
      url_(url),
      request_handle_(nullptr),
      done_(false),
      load_state_change_coalescer_(
          base::Bind(&MojoProxyResolverImpl::Job::SendLoadStateChanged,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(
              kLoadStateChangeCoalesceTimeoutMilliseconds),
          LOAD_STATE_RESOLVING_PROXY_FOR_URL) {
}

MojoProxyResolverImpl::Job::~Job() {
  if (request_handle_ && !done_)
    resolver_->resolver_->CancelRequest(request_handle_);
}

void MojoProxyResolverImpl::Job::Start() {
  int result = resolver_->resolver_->GetProxyForURL(
      url_, &result_, base::Bind(&Job::GetProxyDone, base::Unretained(this)),
      &request_handle_, BoundNetLog());
  if (result != ERR_IO_PENDING) {
    GetProxyDone(result);
    return;
  }
  client_.set_error_handler(this);
  resolver_->request_handle_to_job_.insert(
      std::make_pair(request_handle_, this));
}

void MojoProxyResolverImpl::Job::LoadStateChanged(LoadState load_state) {
  load_state_change_coalescer_.LoadStateChanged(load_state);
}

void MojoProxyResolverImpl::Job::GetProxyDone(int error) {
  done_ = true;
  DVLOG(1) << "GetProxyForUrl(" << url_ << ") finished with error " << error
           << ". " << result_.proxy_list().size() << " Proxies returned:";
  for (const auto& proxy : result_.proxy_list().GetAll()) {
    DVLOG(1) << proxy.ToURI();
  }
  mojo::Array<interfaces::ProxyServerPtr> result;
  if (error == OK) {
    result = mojo::Array<interfaces::ProxyServerPtr>::From(
        result_.proxy_list().GetAll());
  }
  client_->ReportResult(error, result.Pass());
  resolver_->DeleteJob(this);
}

void MojoProxyResolverImpl::Job::OnConnectionError() {
  resolver_->DeleteJob(this);
}

void MojoProxyResolverImpl::Job::SendLoadStateChanged(LoadState load_state) {
  client_->LoadStateChanged(load_state);
}

}  // namespace net
