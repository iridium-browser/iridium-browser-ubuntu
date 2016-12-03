// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/runner/child/test_native_main.h"
#include "services/shell/runner/init.h"
#include "services/shell/tests/connect/connect_test.mojom.h"

using shell::test::mojom::ConnectTestService;
using shell::test::mojom::ConnectTestServiceRequest;

namespace {

class Target : public shell::Service,
               public shell::InterfaceFactory<ConnectTestService>,
               public ConnectTestService {
 public:
  Target() {}
  ~Target() override {}

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override {
    identity_ = identity;
  }
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override {
    registry->AddInterface<ConnectTestService>(this);
    return true;
  }

  // shell::InterfaceFactory<ConnectTestService>:
  void Create(const shell::Identity& remote_identity,
              ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("connect_test_exe");
  }
  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(identity_.instance());
  }

  shell::Identity identity_;
  mojo::BindingSet<ConnectTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Target);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  shell::InitializeLogging();

  Target target;
  return shell::TestNativeMain(&target);
}
