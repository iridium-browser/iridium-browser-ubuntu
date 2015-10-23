// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/web_view_application_delegate.h"

#include "mandoline/tab/web_view_impl.h"
#include "mojo/application/public/cpp/application_connection.h"

namespace web_view {

WebViewApplicationDelegate::WebViewApplicationDelegate() : app_(nullptr) {}
WebViewApplicationDelegate::~WebViewApplicationDelegate() {}

void WebViewApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
}

bool WebViewApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<mojom::WebViewFactory>(this);
  return true;
}

void WebViewApplicationDelegate::CreateWebView(
    mojom::WebViewClientPtr client,
    mojo::InterfaceRequest<mojom::WebView> web_view) {
  new WebViewImpl(app_, client.Pass(), web_view.Pass());
}

void WebViewApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojom::WebViewFactory> request) {
  factory_bindings_.AddBinding(this, request.Pass());
}

}  // namespace web_view
