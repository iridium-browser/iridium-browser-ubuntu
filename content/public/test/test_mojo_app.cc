// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_mojo_app.h"

#include "base/logging.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace content {

const char kTestMojoAppUrl[] = "system:content_mojo_test";

TestMojoApp::TestMojoApp() : service_binding_(this), app_(nullptr) {
}

TestMojoApp::~TestMojoApp() {
}

void TestMojoApp::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
}

bool TestMojoApp::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  requestor_url_ = GURL(connection->GetRemoteApplicationURL());
  connection->AddService<TestMojoService>(this);
  return true;
}

void TestMojoApp::Create(mojo::ApplicationConnection* connection,
                         mojo::InterfaceRequest<TestMojoService> request) {
  DCHECK(!service_binding_.is_bound());
  service_binding_.Bind(request.Pass());
}

void TestMojoApp::DoSomething(const DoSomethingCallback& callback) {
  callback.Run();
  DCHECK(app_);
  app_->Quit();
}

void TestMojoApp::GetRequestorURL(const GetRequestorURLCallback& callback) {
  callback.Run(requestor_url_.spec());
}

}  // namespace content
