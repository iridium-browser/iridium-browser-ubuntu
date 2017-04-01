// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_
#define CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/common/resource_response.h"
#include "net/url_request/redirect_info.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "url/gurl.h"

namespace mojo {
class AssociatedGroup;
}  // namespace mojo

namespace content {

class ResourceDispatcher;
struct ResourceResponseInfo;

// PlzNavigate: Used to override parameters of the navigation request.
struct CONTENT_EXPORT StreamOverrideParameters {
 public:
  StreamOverrideParameters();
  ~StreamOverrideParameters();
  // TODO(clamy): The browser should be made aware on destruction of this struct
  // that it can release its associated stream handle.
  GURL stream_url;
  ResourceResponseHead response;
  std::vector<GURL> redirects;
  std::vector<ResourceResponseInfo> redirect_responses;

  // The delta between the actual transfer size and the one reported by the
  // AsyncResourceLoader due to not having the ResourceResponse.
  int total_transfer_size_delta;
};

class CONTENT_EXPORT WebURLLoaderImpl
    : public NON_EXPORTED_BASE(blink::WebURLLoader) {
 public:

  // Takes ownership of |web_task_runner|.
  WebURLLoaderImpl(ResourceDispatcher* resource_dispatcher,
                   mojom::URLLoaderFactory* url_loader_factory,
                   mojo::AssociatedGroup* associated_group);
  ~WebURLLoaderImpl() override;

  static void PopulateURLResponse(const GURL& url,
                                  const ResourceResponseInfo& info,
                                  blink::WebURLResponse* response,
                                  bool report_security_info);
  static blink::WebURLRequest PopulateURLRequestForRedirect(
      const blink::WebURLRequest& request,
      const net::RedirectInfo& redirect_info,
      blink::WebURLRequest::SkipServiceWorker skip_service_worker);

  // WebURLLoader methods:
  void loadSynchronously(const blink::WebURLRequest& request,
                         blink::WebURLResponse& response,
                         blink::WebURLError& error,
                         blink::WebData& data,
                         int64_t& encoded_data_length,
                         int64_t& encoded_body_length) override;
  void loadAsynchronously(
      const blink::WebURLRequest& request,
      blink::WebURLLoaderClient* client) override;
  void cancel() override;
  void setDefersLoading(bool value) override;
  void didChangePriority(blink::WebURLRequest::Priority new_priority,
                         int intra_priority_value) override;
  void setLoadingTaskRunner(
      base::SingleThreadTaskRunner* loading_task_runner) override;

 private:
  class Context;
  class RequestPeerImpl;
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_URL_LOADER_IMPL_H_
