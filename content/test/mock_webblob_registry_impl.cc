// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_webblob_registry_impl.h"

#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebBlobData;
using blink::WebString;
using blink::WebURL;

namespace content {

MockWebBlobRegistryImpl::MockWebBlobRegistryImpl() {
}

MockWebBlobRegistryImpl::~MockWebBlobRegistryImpl() {
}

void MockWebBlobRegistryImpl::registerBlobData(const WebString& uuid,
                                               const WebBlobData& data) {
}

std::unique_ptr<blink::WebBlobRegistry::Builder>
MockWebBlobRegistryImpl::createBuilder(const blink::WebString& uuid,
                                       const blink::WebString& contentType) {
  NOTREACHED();
  return nullptr;
}

void MockWebBlobRegistryImpl::addBlobDataRef(const WebString& uuid) {
}

void MockWebBlobRegistryImpl::removeBlobDataRef(const WebString& uuid) {
}

void MockWebBlobRegistryImpl::registerPublicBlobURL(const WebURL& url,
                                                    const WebString& uuid) {
}

void MockWebBlobRegistryImpl::revokePublicBlobURL(const WebURL& url) {
}

void MockWebBlobRegistryImpl::registerStreamURL(const WebURL& url,
                                                const WebString& content_type) {
}

void MockWebBlobRegistryImpl::registerStreamURL(const WebURL& url,
                                                const blink::WebURL& src_url) {
}

void MockWebBlobRegistryImpl::addDataToStream(const WebURL& url,
                                              const char* data,
                                              size_t length) {
}

void MockWebBlobRegistryImpl::flushStream(const WebURL& url) {
}

void MockWebBlobRegistryImpl::finalizeStream(const WebURL& url) {
}

void MockWebBlobRegistryImpl::abortStream(const WebURL& url) {
}

void MockWebBlobRegistryImpl::unregisterStreamURL(const WebURL& url) {
}

}  // namespace content
