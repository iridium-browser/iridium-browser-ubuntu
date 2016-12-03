// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

class ResourceMessageFilter;

// This class is an implementation of mojom::URLLoaderFactory that creates
// a mojom::URLLoader.
class URLLoaderFactoryImpl final : public mojom::URLLoaderFactory {
 public:
  ~URLLoaderFactoryImpl() override;

  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t request_id,
                            const ResourceRequest& url_request,
                            mojom::URLLoaderClientPtr client) override;

  // Creates a URLLoaderFactoryImpl instance. The instance is held by the
  // StrongBinding in it, so this function doesn't return the instance.
  CONTENT_EXPORT static void Create(
      scoped_refptr<ResourceMessageFilter> resource_message_filter,
      mojo::InterfaceRequest<mojom::URLLoaderFactory> request);

 private:
  URLLoaderFactoryImpl(
      scoped_refptr<ResourceMessageFilter> resource_message_filter,
      mojo::InterfaceRequest<mojom::URLLoaderFactory> request);

  scoped_refptr<ResourceMessageFilter> resource_message_filter_;
  mojo::StrongBinding<mojom::URLLoaderFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_IMPL_H_
