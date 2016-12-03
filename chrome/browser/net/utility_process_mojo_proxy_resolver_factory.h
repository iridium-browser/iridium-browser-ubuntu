// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_UTILITY_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_
#define CHROME_BROWSER_NET_UTILITY_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "net/proxy/mojo_proxy_resolver_factory.h"

namespace content {
class UtilityProcessHost;
}
namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

// A factory used to create connections to Mojo proxy resolver services run in a
// utility process. All Mojo proxy resolver services will be run in the same
// utility process. Utility process crashes are detected and the utility
// process is automatically restarted.
class UtilityProcessMojoProxyResolverFactory
    : public net::MojoProxyResolverFactory {
 public:
  static UtilityProcessMojoProxyResolverFactory* GetInstance();

  // Overridden from net::MojoProxyResolverFactory:
  std::unique_ptr<base::ScopedClosureRunner> CreateResolver(
      const mojo::String& pac_script,
      mojo::InterfaceRequest<net::interfaces::ProxyResolver> req,
      net::interfaces::ProxyResolverFactoryRequestClientPtr client) override;

 private:
  friend struct base::DefaultSingletonTraits<
      UtilityProcessMojoProxyResolverFactory>;
  UtilityProcessMojoProxyResolverFactory();
  ~UtilityProcessMojoProxyResolverFactory() override;

  // Error handler callback for |resolver_factory_|.
  void OnConnectionError();

  // Invoked each time a proxy resolver is destroyed.
  void OnResolverDestroyed();

  // Invoked once an idle timeout has elapsed after all proxy resolvers are
  // destroyed.
  void OnIdleTimeout();

  // Creates a new utility process and connects to its Mojo proxy resolver
  // factory.
  void CreateProcessAndConnect();

  net::interfaces::ProxyResolverFactoryPtr resolver_factory_;

  base::WeakPtr<content::UtilityProcessHost> weak_utility_process_host_;
  size_t num_proxy_resolvers_ = 0;

  base::OneShotTimer idle_timer_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(UtilityProcessMojoProxyResolverFactory);
};

#endif  // CHROME_BROWSER_NET_UTILITY_PROCESS_MOJO_PROXY_RESOLVER_FACTORY_H_
