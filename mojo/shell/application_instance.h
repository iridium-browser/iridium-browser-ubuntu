// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APPLICATION_INSTANCE_H_
#define MOJO_SHELL_APPLICATION_INSTANCE_H_

#include <set>

#include "base/callback.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/identity.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ApplicationManager;

// Encapsulates a connection to an instance of an application, tracked by the
// shell's ApplicationManager.
class ApplicationInstance : public Shell {
 public:
  ApplicationInstance(ApplicationPtr application,
                      ApplicationManager* manager,
                      const Identity& originator_identity,
                      const Identity& resolved_identity,
                      const CapabilityFilter& filter,
                      const base::Closure& on_application_end);

  ~ApplicationInstance() override;

  void InitializeApplication();

  void ConnectToClient(ApplicationInstance* originator,
                       const GURL& requested_url,
                       const GURL& requestor_url,
                       InterfaceRequest<ServiceProvider> services,
                       ServiceProviderPtr exposed_services,
                       const CapabilityFilter& filter);

  // Returns the set of interfaces this application instance is allowed to see
  // from an instance with |identity|.
  AllowedInterfaces GetAllowedInterfaces(const Identity& identity) const;

  Application* application() { return application_.get(); }
  const Identity& identity() const { return identity_; }
  base::Closure on_application_end() const { return on_application_end_; }

 private:
  // Shell implementation:
  void ConnectToApplication(URLRequestPtr app_request,
                            InterfaceRequest<ServiceProvider> services,
                            ServiceProviderPtr exposed_services,
                            CapabilityFilterPtr filter) override;
  void QuitApplication() override;

  void CallAcceptConnection(ApplicationInstance* originator,
                            const GURL& url,
                            InterfaceRequest<ServiceProvider> services,
                            ServiceProviderPtr exposed_services,
                            const GURL& requested_url);

  void OnConnectionError();

  void OnQuitRequestedResult(bool can_quit);

  struct QueuedClientRequest {
    QueuedClientRequest();
    ~QueuedClientRequest();
    ApplicationInstance* originator;
    GURL requested_url;
    GURL requestor_url;
    InterfaceRequest<ServiceProvider> services;
    ServiceProviderPtr exposed_services;
    CapabilityFilter filter;
  };

  ApplicationManager* const manager_;
  const Identity originator_identity_;
  const Identity identity_;
  const CapabilityFilter filter_;
  const bool allow_any_application_;
  base::Closure on_application_end_;
  ApplicationPtr application_;
  Binding<Shell> binding_;
  bool queue_requests_;
  std::vector<QueuedClientRequest*> queued_client_requests_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationInstance);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APPLICATION_INSTANCE_H_
