// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_IMPL_SYNCAPI_SERVER_CONNECTION_MANAGER_H_
#define COMPONENTS_SYNC_CORE_IMPL_SYNCAPI_SERVER_CONNECTION_MANAGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"

namespace syncer {

class HttpPostProviderFactory;
class HttpPostProviderInterface;

// This provides HTTP Post functionality through the interface provided
// to the sync API by the application hosting the syncer backend.
class SyncAPIBridgedConnection : public ServerConnectionManager::Connection {
 public:
  SyncAPIBridgedConnection(ServerConnectionManager* scm,
                           HttpPostProviderFactory* factory);

  ~SyncAPIBridgedConnection() override;

  bool Init(const char* path,
            const std::string& auth_token,
            const std::string& payload,
            HttpResponse* response) override;

  void Abort() override;

 private:
  // Pointer to the factory we use for creating HttpPostProviders. We do not
  // own |factory_|.
  HttpPostProviderFactory* factory_;

  HttpPostProviderInterface* post_provider_;

  DISALLOW_COPY_AND_ASSIGN(SyncAPIBridgedConnection);
};

// A ServerConnectionManager subclass used by the syncapi layer. We use a
// subclass so that we can override MakePost() to generate a POST object using
// an instance of the HttpPostProviderFactory class.
class SyncAPIServerConnectionManager : public ServerConnectionManager {
 public:
  // Takes ownership of factory.
  SyncAPIServerConnectionManager(const std::string& server,
                                 int port,
                                 bool use_ssl,
                                 HttpPostProviderFactory* factory,
                                 CancelationSignal* cancelation_signal);
  ~SyncAPIServerConnectionManager() override;

  // ServerConnectionManager overrides.
  Connection* MakeConnection() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncAPIServerConnectionManagerTest,
                           VeryEarlyAbortPost);
  FRIEND_TEST_ALL_PREFIXES(SyncAPIServerConnectionManagerTest, EarlyAbortPost);
  FRIEND_TEST_ALL_PREFIXES(SyncAPIServerConnectionManagerTest, AbortPost);
  FRIEND_TEST_ALL_PREFIXES(SyncAPIServerConnectionManagerTest,
                           FailPostWithTimedOut);

  // A factory creating concrete HttpPostProviders for use whenever we need to
  // issue a POST to sync servers.
  std::unique_ptr<HttpPostProviderFactory> post_provider_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncAPIServerConnectionManager);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_IMPL_SYNCAPI_SERVER_CONNECTION_MANAGER_H_
