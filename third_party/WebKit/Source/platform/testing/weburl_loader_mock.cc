// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/weburl_loader_mock.h"

#include "platform/testing/weburl_loader_mock_factory_impl.h"
#include "public/platform/URLConversion.h"
#include "public/platform/WebData.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoaderClient.h"

namespace blink {

WebURLLoaderMock::WebURLLoaderMock(WebURLLoaderMockFactoryImpl* factory,
                                   WebURLLoader* default_loader)
    : factory_(factory),
      default_loader_(WTF::wrapUnique(default_loader)),
      weak_factory_(this) {}

WebURLLoaderMock::~WebURLLoaderMock() {
  cancel();
}

void WebURLLoaderMock::ServeAsynchronousRequest(
    WebURLLoaderTestDelegate* delegate,
    const WebURLResponse& response,
    const WebData& data,
    const WebURLError& error) {
  DCHECK(!using_default_loader_);
  if (!client_)
    return;

  // If no delegate is provided then create an empty one. The default behavior
  // will just proxy to the client.
  std::unique_ptr<WebURLLoaderTestDelegate> default_delegate;
  if (!delegate) {
    default_delegate = WTF::wrapUnique(new WebURLLoaderTestDelegate());
    delegate = default_delegate.get();
  }

  // didReceiveResponse() and didReceiveData() might end up getting ::cancel()
  // to be called which will make the ResourceLoader to delete |this|.
  WeakPtr<WebURLLoaderMock> self = weak_factory_.createWeakPtr();

  delegate->didReceiveResponse(client_, response);
  if (!self)
    return;

  if (error.reason) {
    delegate->didFail(client_, error, data.size(), 0);
    return;
  }
  delegate->didReceiveData(client_, data.data(), data.size());
  if (!self)
    return;

  delegate->didFinishLoading(client_, 0, data.size(), data.size());
}

WebURLRequest WebURLLoaderMock::ServeRedirect(
    const WebURLRequest& request,
    const WebURLResponse& redirectResponse) {
  KURL redirectURL(
      ParsedURLString, redirectResponse.httpHeaderField("Location"));

  WebURLRequest newRequest(redirectURL);
  newRequest.setFirstPartyForCookies(redirectURL);
  newRequest.setDownloadToFile(request.downloadToFile());
  newRequest.setUseStreamOnResponse(request.useStreamOnResponse());
  newRequest.setRequestContext(request.getRequestContext());
  newRequest.setFrameType(request.getFrameType());
  newRequest.setSkipServiceWorker(request.skipServiceWorker());
  newRequest.setShouldResetAppCache(request.shouldResetAppCache());
  newRequest.setFetchRequestMode(request.getFetchRequestMode());
  newRequest.setFetchCredentialsMode(request.getFetchCredentialsMode());
  newRequest.setHTTPMethod(request.httpMethod());
  newRequest.setHTTPBody(request.httpBody());

  WeakPtr<WebURLLoaderMock> self = weak_factory_.createWeakPtr();

  bool follow = client_->willFollowRedirect(newRequest, redirectResponse);
  if (!follow)
    newRequest = WebURLRequest();

  // |this| might be deleted in willFollowRedirect().
  if (!self)
    return newRequest;

  if (!follow)
    cancel();

  return newRequest;
}

void WebURLLoaderMock::loadSynchronously(const WebURLRequest& request,
                                         WebURLResponse& response,
                                         WebURLError& error,
                                         WebData& data,
                                         int64_t& encoded_data_length,
                                         int64_t& encoded_body_length) {
  if (factory_->IsMockedURL(request.url())) {
      factory_->LoadSynchronously(request, &response, &error, &data,
                                  &encoded_data_length);
    return;
  }
  DCHECK(KURL(request.url()).protocolIsData())
      << "loadSynchronously shouldn't be falling back: "
      << request.url().string().utf8();
  using_default_loader_ = true;
  default_loader_->loadSynchronously(request, response, error, data,
                                     encoded_data_length, encoded_body_length);
}

void WebURLLoaderMock::loadAsynchronously(const WebURLRequest& request,
                                          WebURLLoaderClient* client) {
  DCHECK(client);
  if (factory_->IsMockedURL(request.url())) {
    client_ = client;
    factory_->LoadAsynchronouly(request, this);
    return;
  }
  DCHECK(KURL(request.url()).protocolIsData())
      << "loadAsynchronously shouldn't be falling back: "
      << request.url().string().utf8();
  using_default_loader_ = true;
  default_loader_->loadAsynchronously(request, client);
}

void WebURLLoaderMock::cancel() {
  if (using_default_loader_) {
    default_loader_->cancel();
    return;
  }
  client_ = nullptr;
  factory_->CancelLoad(this);
}

void WebURLLoaderMock::setDefersLoading(bool deferred) {
  is_deferred_ = deferred;
  if (using_default_loader_) {
    default_loader_->setDefersLoading(deferred);
    return;
  }

  // Ignores setDefersLoading(false) safely.
  if (!deferred)
    return;

  // setDefersLoading(true) is not implemented.
  NOTIMPLEMENTED();
}

void WebURLLoaderMock::setLoadingTaskRunner(
    base::SingleThreadTaskRunner* runner) {
  // In principle this is NOTIMPLEMENTED(), but if we put that here it floods
  // the console during webkit unit tests, so we leave the function empty.
  DCHECK(runner);
}

WeakPtr<WebURLLoaderMock> WebURLLoaderMock::GetWeakPtr() {
  return weak_factory_.createWeakPtr();
}

} // namespace blink
