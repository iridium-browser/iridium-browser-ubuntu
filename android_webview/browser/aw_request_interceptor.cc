// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_request_interceptor.h"

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_web_resource_response.h"
#include "base/android/jni_string.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::ResourceRequestInfo;

namespace android_webview {

namespace {

const void* kRequestAlreadyQueriedDataKey = &kRequestAlreadyQueriedDataKey;

} // namespace

AwRequestInterceptor::AwRequestInterceptor() {
}

AwRequestInterceptor::~AwRequestInterceptor() {
}

scoped_ptr<AwWebResourceResponse>
AwRequestInterceptor::QueryForAwWebResourceResponse(
    net::URLRequest* request) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int render_process_id, render_frame_id;
  if (!ResourceRequestInfo::GetRenderFrameForRequest(
      request, &render_process_id, &render_frame_id))
    return scoped_ptr<AwWebResourceResponse>();

  scoped_ptr<AwContentsIoThreadClient> io_thread_client =
      AwContentsIoThreadClient::FromID(render_process_id, render_frame_id);

  if (!io_thread_client.get())
    return scoped_ptr<AwWebResourceResponse>();

  GURL referrer(request->referrer());
  if (referrer.is_valid() &&
      (!request->is_pending() || request->is_redirecting())) {
    request->SetExtraRequestHeaderByName(net::HttpRequestHeaders::kReferer,
                                         referrer.spec(), true);
  }
  return io_thread_client->ShouldInterceptRequest(request).Pass();
}

net::URLRequestJob* AwRequestInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // See if we've already found out the aw_web_resource_response for this
  // request.
  // This is done not only for efficiency reasons, but also for correctness
  // as it is possible for the Interceptor chain to be invoked more than once
  // in which case we don't want to query the embedder multiple times.
  // Note: The Interceptor chain is not invoked more than once if we create a
  // URLRequestJob in this method, so this is only caching negative hits.
  if (request->GetUserData(kRequestAlreadyQueriedDataKey))
    return NULL;
  request->SetUserData(kRequestAlreadyQueriedDataKey,
                       new base::SupportsUserData::Data());

  scoped_ptr<AwWebResourceResponse> aw_web_resource_response =
      QueryForAwWebResourceResponse(request);

  if (!aw_web_resource_response)
    return NULL;

  // The newly created job will own the AwWebResourceResponse.
  return AwWebResourceResponse::CreateJobFor(
      aw_web_resource_response.Pass(), request, network_delegate);
}

} // namespace android_webview
