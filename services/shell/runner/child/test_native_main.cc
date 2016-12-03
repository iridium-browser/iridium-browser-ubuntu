// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/runner/child/test_native_main.h"

#include <utility>

#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/runner/common/client_util.h"
#include "services/shell/runner/init.h"

namespace shell {
namespace {

class ProcessDelegate : public mojo::edk::ProcessDelegate {
 public:
  ProcessDelegate() {}
  ~ProcessDelegate() override {}

 private:
  void OnShutdownComplete() override {}

  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

}  // namespace

int TestNativeMain(shell::Service* service) {
  shell::WaitForDebuggerIfNecessary();

#if !defined(OFFICIAL_BUILD)
  base::debug::EnableInProcessStackDumping();
#if defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
#endif

  {
    mojo::edk::Init();

    ProcessDelegate process_delegate;
    base::Thread io_thread("io_thread");
    base::Thread::Options io_thread_options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread.StartWithOptions(io_thread_options));

    mojo::edk::InitIPCSupport(&process_delegate, io_thread.task_runner());
    mojo::edk::SetParentPipeHandleFromCommandLine();

    base::MessageLoop loop;
    service->set_context(base::MakeUnique<shell::ServiceContext>(
        service, shell::GetServiceRequestFromCommandLine()));
    base::RunLoop().Run();

    mojo::edk::ShutdownIPCSupport();

    service->set_context(std::unique_ptr<ServiceContext>());
  }

  return 0;
}

}  // namespace shell
