// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/service_context.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "services/shell/public/cpp/lib/connector_impl.h"
#include "services/shell/public/cpp/service.h"

namespace shell {

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, public:

ServiceContext::ServiceContext(shell::Service* service,
                               mojom::ServiceRequest request,
                               std::unique_ptr<Connector> connector,
                               mojom::ConnectorRequest connector_request)
    : pending_connector_request_(std::move(connector_request)),
      service_(service),
      binding_(this, std::move(request)),
      connector_(std::move(connector)) {
  DCHECK(binding_.is_bound());
  binding_.set_connection_error_handler(
      base::Bind(&ServiceContext::OnConnectionError, base::Unretained(this)));
  if (!connector_) {
    connector_ = Connector::Create(&pending_connector_request_);
  } else {
    DCHECK(pending_connector_request_.is_pending());
  }
}

ServiceContext::~ServiceContext() {}

void ServiceContext::SetConnectionLostClosure(const base::Closure& closure) {
  connection_lost_closure_ = closure;
  if (should_run_connection_lost_closure_ &&
      !connection_lost_closure_.is_null())
    connection_lost_closure_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, mojom::Service implementation:

void ServiceContext::OnStart(const shell::Identity& identity,
                             const OnStartCallback& callback) {
  identity_ = identity;
  if (!initialize_handler_.is_null())
    initialize_handler_.Run();

  callback.Run(std::move(pending_connector_request_));

  service_->OnStart(identity_);
}

void ServiceContext::OnConnect(
    const Identity& source,
    mojom::InterfaceProviderRequest interfaces,
    const CapabilityRequest& allowed_capabilities) {
  std::unique_ptr<InterfaceRegistry> registry(
      new InterfaceRegistry(source, allowed_capabilities));
  registry->Bind(std::move(interfaces));

  if (!service_->OnConnect(source, registry.get()))
    return;

  // TODO(beng): it appears we never prune this list. We should, when the
  //             registry's remote interface provider pipe breaks.
  incoming_connections_.push_back(std::move(registry));
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, private:

void ServiceContext::OnConnectionError() {
  // Note that the Service doesn't technically have to quit now, it may live
  // on to service existing connections. All existing Connectors however are
  // invalid.
  should_run_connection_lost_closure_ = service_->OnStop();
  if (should_run_connection_lost_closure_ &&
      !connection_lost_closure_.is_null())
    connection_lost_closure_.Run();
  // We don't reset the connector as clients may have taken a raw pointer to it.
  // Connect() will return nullptr if they try to connect to anything.
}

}  // namespace shell
