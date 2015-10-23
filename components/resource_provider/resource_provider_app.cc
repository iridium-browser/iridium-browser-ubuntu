// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/resource_provider/resource_provider_app.h"

#include "components/resource_provider/file_utils.h"
#include "components/resource_provider/resource_provider_impl.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "url/gurl.h"

namespace resource_provider {

ResourceProviderApp::ResourceProviderApp(
    const std::string& resource_provider_app_url)
    : resource_provider_app_url_(resource_provider_app_url) {
}

ResourceProviderApp::~ResourceProviderApp() {
}

void ResourceProviderApp::Initialize(mojo::ApplicationImpl* app) {
}

bool ResourceProviderApp::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  const base::FilePath app_path(
      GetPathForApplicationUrl(GURL(connection->GetRemoteApplicationURL())));
  if (app_path.empty())
    return false;  // The specified app has no resources.

  connection->AddService<ResourceProvider>(this);
  return true;
}

void ResourceProviderApp::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<ResourceProvider> request) {
  const base::FilePath app_path(
      GetPathForApplicationUrl(GURL(connection->GetRemoteApplicationURL())));
  // We validated path at ConfigureIncomingConnection() time, so it should still
  // be valid.
  CHECK(!app_path.empty());
  bindings_.AddBinding(
      new ResourceProviderImpl(app_path, resource_provider_app_url_),
      request.Pass());
}

}  // namespace resource_provider
