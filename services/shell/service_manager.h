// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_SERVICE_MANAGER_H_
#define SERVICES_SHELL_SERVICE_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/shell/connect_params.h"
#include "services/shell/native_runner.h"
#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"
#include "services/shell/public/interfaces/resolver.mojom.h"
#include "services/shell/public/interfaces/service.mojom.h"
#include "services/shell/public/interfaces/service_factory.mojom.h"
#include "services/shell/public/interfaces/service_manager.mojom.h"

namespace shell {
class ServiceContext;

// Creates an identity for the Service Manager, used when the Service Manager
// connects to services.
Identity CreateServiceManagerIdentity();

class ServiceManager : public Service {
 public:
  // API for testing.
  class TestAPI {
   public:
    explicit TestAPI(ServiceManager* service_manager);
    ~TestAPI();

    // Returns true if there is a Instance for this name.
    bool HasRunningInstanceForName(const std::string& name) const;
   private:
    ServiceManager* service_manager_;

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  // |native_runner_factory| is an instance of an object capable of vending
  // implementations of NativeRunner, e.g. for in or out-of-process execution.
  // See native_runner.h and RunNativeApplication().
  // |file_task_runner| provides access to a thread to perform file copy
  // operations on.
  ServiceManager(std::unique_ptr<NativeRunnerFactory> native_runner_factory,
                 mojom::ServicePtr catalog);
  ~ServiceManager() override;

  // Provide a callback to be notified whenever an instance is destroyed.
  // Typically the creator of the Service Manager will use this to determine
  // when some set of services it created are destroyed, so it can shut down.
  void SetInstanceQuitCallback(base::Callback<void(const Identity&)> callback);

  // Completes a connection between a source and target application as defined
  // by |params|, exchanging InterfaceProviders between them. If no existing
  // instance of the target application is running, one will be loaded.
  void Connect(std::unique_ptr<ConnectParams> params);

  // Creates a new Instance identified as |name|. This is intended for use by
  // the Service Manager's embedder to register itself. This must only be called
  // once.
  mojom::ServiceRequest StartEmbedderService(const std::string& name);

 private:
  class Instance;

  // Service:
  bool OnConnect(const Identity& remote_identity,
                 InterfaceRegistry* registry) override;

  void InitCatalog(mojom::ServicePtr catalog);

  // Returns the resolver to use for the specified identity.
  // NOTE: Resolvers are cached to ensure we service requests in order. If
  // we use a separate Resolver for each request ordering is not
  // guaranteed and can lead to random flake.
  mojom::Resolver* GetResolver(const Identity& identity);

  // Destroys all Service Manager-ends of connections established with Services.
  // Services connected by this Service Manager will observe pipe errors and
  // have a chance to shut down.
  void TerminateServiceManagerConnections();

  // Removes a Instance when it encounters an error.
  void OnInstanceError(Instance* instance);

  // Completes a connection between a source and target application as defined
  // by |params|, exchanging InterfaceProviders between them. If no existing
  // instance of the target application is running, one will be loaded.
  //
  // If |service| is not null, there must not be an instance of the target
  // application already running. The Service Manager will create a new instance
  // and use |service| to control it.
  //
  // If |instance| is not null, the lifetime of the connection request is
  // bounded by that of |instance|. The connection will be cancelled dropped if
  // |instance| is destroyed.
  void Connect(std::unique_ptr<ConnectParams> params,
               mojom::ServicePtr service,
               base::WeakPtr<Instance> source_instance);

  // Returns a running instance matching |identity|. This might be an instance
  // running as a different user if one is available that services all users.
  Instance* GetExistingInstance(const Identity& identity) const;

  void NotifyPIDAvailable(const Identity& identity, base::ProcessId pid);

  // Attempt to complete the connection requested by |params| by connecting to
  // an existing instance. If there is an existing instance, |params| is taken,
  // and this function returns true.
  bool ConnectToExistingInstance(std::unique_ptr<ConnectParams>* params);

  Instance* CreateInstance(const Identity& source,
                           const Identity& target,
                           const CapabilitySpec& spec);

  // Called from the instance implementing mojom::ServiceManager.
  void AddListener(mojom::ServiceManagerListenerPtr listener);

  void CreateServiceWithFactory(const Identity& service_factory,
                                const std::string& name,
                                mojom::ServiceRequest request);
  // Returns a running ServiceFactory for |service_factory_identity|.
  // If there is not one running one is started for |source_identity|.
  mojom::ServiceFactory* GetServiceFactory(
      const Identity& service_factory_identity);
  void OnServiceFactoryLost(const Identity& which);

  // Callback when remote Catalog resolves mojo:foo to mojo:bar.
  // |params| are the params passed to Connect().
  // |service| if provided is a ServicePtr which should be used to manage the
  // new application instance. This may be null.
  // |result| contains the result of the resolve operation.
  void OnGotResolvedName(std::unique_ptr<ConnectParams> params,
                         mojom::ServicePtr service,
                         bool has_source_instance,
                         base::WeakPtr<Instance> source_instance,
                         mojom::ResolveResultPtr result);

  base::WeakPtr<ServiceManager> GetWeakPtr();

  std::map<Identity, Instance*> identity_to_instance_;

  // Tracks the names of instances that are allowed to field connection requests
  // from all users.
  std::set<std::string> singletons_;

  std::map<Identity, mojom::ServiceFactoryPtr> service_factories_;
  std::map<Identity, mojom::ResolverPtr> identity_to_resolver_;
  mojo::InterfacePtrSet<mojom::ServiceManagerListener> listeners_;
  base::Callback<void(const Identity&)> instance_quit_callback_;
  std::unique_ptr<NativeRunnerFactory> native_runner_factory_;
  std::unique_ptr<ServiceContext> service_context_;
  base::WeakPtrFactory<ServiceManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

mojom::Connector::ConnectCallback EmptyConnectCallback();

}  // namespace shell

#endif  // SERVICES_SHELL_SERVICE_MANAGER_H_
