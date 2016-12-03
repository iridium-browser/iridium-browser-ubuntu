// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/runner/child/test_native_main.h"
#include "services/shell/runner/init.h"
#include "services/shell/tests/shell/shell_unittest.mojom.h"

using shell::test::mojom::CreateInstanceTestPtr;

namespace {

class Target : public shell::Service {
 public:
  Target() {}
  ~Target() override {}

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override {
    CreateInstanceTestPtr service;
    connector()->ConnectToInterface("mojo:shell_unittest", &service);
    service->SetTargetIdentity(identity);
  }

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
