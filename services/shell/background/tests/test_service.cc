// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/background/tests/test.mojom.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"

namespace shell {

class TestClient : public Service,
                   public InterfaceFactory<mojom::TestService>,
                   public mojom::TestService {
 public:
  TestClient() {}
  ~TestClient() override {}

 private:
  // Service:
  bool OnConnect(const Identity& remote_identity,
                 InterfaceRegistry* registry) override {
    registry->AddInterface(this);
    return true;
  }
  bool OnStop() override {
    return true;
  }

  // InterfaceFactory<mojom::TestService>:
  void Create(const Identity& remote_identity,
              mojo::InterfaceRequest<mojom::TestService> request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::TestService
  void Test(const TestCallback& callback) override {
    callback.Run();
  }

  mojo::BindingSet<mojom::TestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace shell

MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::ServiceRunner runner(new shell::TestClient);
  return runner.Run(service_request_handle);
}
