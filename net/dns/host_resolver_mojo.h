// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_MOJO_H_
#define NET_DNS_HOST_RESOLVER_MOJO_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/interfaces/host_resolver_service.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
class AddressList;
class BoundNetLog;

// A HostResolver implementation that delegates to an interfaces::HostResolver
// mojo interface.
class HostResolverMojo : public HostResolver, public mojo::ErrorHandler {
 public:
  HostResolverMojo(interfaces::HostResolverPtr resolver,
                   const base::Closure& disconnect_callback);
  ~HostResolverMojo() override;

  // HostResolver overrides.
  int Resolve(const RequestInfo& info,
              RequestPriority priority,
              AddressList* addresses,
              const CompletionCallback& callback,
              RequestHandle* request_handle,
              const BoundNetLog& source_net_log) override;
  int ResolveFromCache(const RequestInfo& info,
                       AddressList* addresses,
                       const BoundNetLog& source_net_log) override;
  void CancelRequest(RequestHandle req) override;
  HostCache* GetHostCache() override;

  void set_disconnect_callback(const base::Closure& disconnect_callback) {
    disconnect_callback_ = disconnect_callback;
  }

 private:
  class Job;

  // mojo::ErrorHandler override.
  void OnConnectionError() override;

  int ResolveFromCacheInternal(const RequestInfo& info,
                               const HostCache::Key& key,
                               AddressList* addresses);

  interfaces::HostResolverPtr resolver_;

  base::Closure disconnect_callback_;

  scoped_ptr<HostCache> host_cache_;
  base::WeakPtrFactory<HostCache> host_cache_weak_factory_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverMojo);
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_MOJO_H_
