// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_
#define MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {

class Application;
class ApplicationImpl;

namespace internal {

class ServiceConnectorBase;

// A ServiceRegistry represents each half of a connection between two
// applications, allowing customization of which services are published to the
// other.
class ServiceRegistry : public ServiceProvider, public ApplicationConnection {
 public:
  ServiceRegistry();
  ServiceRegistry(ApplicationImpl* application_impl,
                  const std::string& connection_url,
                  const std::string& remote_url,
                  ServiceProviderPtr remote_services,
                  InterfaceRequest<ServiceProvider> local_services);
  ~ServiceRegistry() override;

  // ApplicationConnection overrides.
  void AddServiceConnector(ServiceConnectorBase* service_connector) override;
  const std::string& GetConnectionURL() override;
  const std::string& GetRemoteApplicationURL() override;
  ServiceProvider* GetServiceProvider() override;

  virtual void RemoveServiceConnector(ServiceConnectorBase* service_connector);

 private:
  // ServiceProvider method.
  void ConnectToService(const mojo::String& service_name,
                        ScopedMessagePipeHandle client_handle) override;

  ApplicationImpl* application_impl_;
  const std::string connection_url_;
  const std::string remote_url_;

 private:
  bool RemoveServiceConnectorInternal(ServiceConnectorBase* service_connector);

  Application* application_;
  typedef std::map<std::string, ServiceConnectorBase*>
      NameToServiceConnectorMap;
  NameToServiceConnectorMap name_to_service_connector_;
  Binding<ServiceProvider> local_binding_;
  ServiceProviderPtr remote_service_provider_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceRegistry);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_
