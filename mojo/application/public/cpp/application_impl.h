// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_IMPL_H_
#define MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_IMPL_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/application/public/cpp/app_lifetime_helper.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/lib/service_registry.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

// TODO(beng): This comment is hilariously out of date.
// Utility class for communicating with the Shell, and providing Services
// to clients.
//
// To use define a class that implements your specific server api, e.g. FooImpl
// to implement a service named Foo.
// That class must subclass an InterfaceImpl specialization.
//
// If there is context that is to be shared amongst all instances, define a
// constructor with that class as its only argument, otherwise define an empty
// constructor.
//
// class FooImpl : public InterfaceImpl<Foo> {
//  public:
//   FooImpl(ApplicationContext* app_context) {}
// };
//
// or
//
// class BarImpl : public InterfaceImpl<Bar> {
//  public:
//   // contexts will remain valid for the lifetime of BarImpl.
//   BarImpl(ApplicationContext* app_context, BarContext* service_context)
//          : app_context_(app_context), servicecontext_(context) {}
//
// Create an ApplicationImpl instance that collects any service implementations.
//
// ApplicationImpl app(service_provider_handle);
// app.AddService<FooImpl>();
//
// BarContext context;
// app.AddService<BarImpl>(&context);
//
//
class ApplicationImpl : public Application {
 public:
  class TestApi {
   public:
    explicit TestApi(ApplicationImpl* application)
        : application_(application) {}

    void UnbindConnections(InterfaceRequest<Application>* application_request,
                           ShellPtr* shell) {
      application_->UnbindConnections(application_request, shell);
    }

   private:
    ApplicationImpl* application_;
  };

  // Does not take ownership of |delegate|, which must remain valid for the
  // lifetime of ApplicationImpl.
  ApplicationImpl(ApplicationDelegate* delegate,
                  InterfaceRequest<Application> request);
  // Constructs an ApplicationImpl with a custom termination closure. This
  // closure is invoked on Quit() instead of the default behavior of quitting
  // the current base::MessageLoop.
  ApplicationImpl(ApplicationDelegate* delegate,
                  InterfaceRequest<Application> request,
                  const Closure& termination_closure);
  ~ApplicationImpl() override;

  // The Mojo shell. This will return a valid pointer after Initialize() has
  // been invoked. It will remain valid until UnbindConnections() is invoked or
  // the ApplicationImpl is destroyed.
  Shell* shell() const { return shell_.get(); }

  const std::string& url() const { return url_; }

  AppLifetimeHelper* app_lifetime_helper() { return &app_lifetime_helper_; }

  // Requests a new connection to an application. Returns a pointer to the
  // connection if the connection is permitted by this application's delegate,
  // or nullptr otherwise. Caller takes ownership.
  scoped_ptr<ApplicationConnection> ConnectToApplication(URLRequestPtr request);
  scoped_ptr<ApplicationConnection> ConnectToApplicationWithCapabilityFilter(
      URLRequestPtr request,
      CapabilityFilterPtr filter);

  // Connect to application identified by |request->url| and connect to the
  // service implementation of the interface identified by |Interface|.
  template <typename Interface>
  void ConnectToService(mojo::URLRequestPtr request,
                        InterfacePtr<Interface>* ptr) {
    scoped_ptr<ApplicationConnection> connection =
        ConnectToApplication(request.Pass());
    if (!connection.get())
      return;
    connection->ConnectToService(ptr);
  }

  // Initiate shutdown of this application. This may involve a round trip to the
  // Shell to ensure there are no inbound service requests.
  void Quit();

 private:
  // Application implementation.
  void Initialize(ShellPtr shell, const mojo::String& url) override;
  void AcceptConnection(const String& requestor_url,
                        InterfaceRequest<ServiceProvider> services,
                        ServiceProviderPtr exposed_services,
                        Array<String> allowed_interfaces,
                        const String& url) override;
  void OnQuitRequested(const Callback<void(bool)>& callback) override;

  void OnConnectionError();

  // Called from Quit() when there is no Shell connection, or asynchronously
  // from Quit() once the Shell has OK'ed shutdown.
  void QuitNow();

  // Unbinds the Shell and Application connections. Can be used to re-bind the
  // handles to another implementation of ApplicationImpl, for instance when
  // running apptests.
  void UnbindConnections(InterfaceRequest<Application>* application_request,
                         ShellPtr* shell);

  // We track the lifetime of incoming connection registries as it more
  // convenient for the client.
  ScopedVector<ApplicationConnection> incoming_connections_;
  ApplicationDelegate* delegate_;
  Binding<Application> binding_;
  ShellPtr shell_;
  std::string url_;
  Closure termination_closure_;
  AppLifetimeHelper app_lifetime_helper_;
  bool quit_requested_;
  base::WeakPtrFactory<ApplicationImpl> weak_factory_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ApplicationImpl);
};

}  // namespace mojo

#endif  // MOJO_APPLICATION_PUBLIC_CPP_APPLICATION_IMPL_H_
