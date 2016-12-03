// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/multiprocess_test_helper.h"

#include <functional>
#include <set>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mach_port_broker.h"
#endif

namespace mojo {
namespace edk {
namespace test {

namespace {

const char kMojoPrimordialPipeToken[] = "mojo-primordial-pipe-token";

template <typename Func>
int RunClientFunction(Func handler) {
  CHECK(MultiprocessTestHelper::primordial_pipe.is_valid());
  ScopedMessagePipeHandle pipe =
      std::move(MultiprocessTestHelper::primordial_pipe);
  return handler(pipe.get().value());
}

}  // namespace

MultiprocessTestHelper::MultiprocessTestHelper() {}

MultiprocessTestHelper::~MultiprocessTestHelper() {
  CHECK(!test_child_.IsValid());
}

ScopedMessagePipeHandle MultiprocessTestHelper::StartChild(
    const std::string& test_child_name,
    LaunchType launch_type) {
  return StartChildWithExtraSwitch(test_child_name, std::string(),
                                   std::string(), launch_type);
}

ScopedMessagePipeHandle MultiprocessTestHelper::StartChildWithExtraSwitch(
    const std::string& test_child_name,
    const std::string& switch_string,
    const std::string& switch_value,
    LaunchType launch_type) {
  CHECK(!test_child_name.empty());
  CHECK(!test_child_.IsValid());

  std::string test_child_main = test_child_name + "TestChildMain";

  // Manually construct the new child's commandline to avoid copying unwanted
  // values.
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine().GetProgram());

  std::set<std::string> uninherited_args;
  uninherited_args.insert("mojo-platform-channel-handle");
  uninherited_args.insert(switches::kTestChildProcess);

  // Copy commandline switches from the parent process, except for the
  // multiprocess client name and mojo message pipe handle; this allows test
  // clients to spawn other test clients.
  for (const auto& entry :
          base::CommandLine::ForCurrentProcess()->GetSwitches()) {
    if (uninherited_args.find(entry.first) == uninherited_args.end())
      command_line.AppendSwitchNative(entry.first, entry.second);
  }

  PlatformChannelPair channel;
  HandlePassingInformation handle_passing_info;
  channel.PrepareToPassClientHandleToChildProcess(&command_line,
                                                  &handle_passing_info);

  std::string pipe_token = mojo::edk::GenerateRandomToken();
  if (launch_type == LaunchType::CHILD)
    command_line.AppendSwitchASCII(kMojoPrimordialPipeToken, pipe_token);

  if (!switch_string.empty()) {
    CHECK(!command_line.HasSwitch(switch_string));
    if (!switch_value.empty())
      command_line.AppendSwitchASCII(switch_string, switch_value);
    else
      command_line.AppendSwitch(switch_string);
  }

  base::LaunchOptions options;
#if defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#elif defined(OS_WIN)
  options.start_hidden = true;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    options.handles_to_inherit = &handle_passing_info;
  else
    options.inherit_handles = true;
#else
#error "Not supported yet."
#endif

  ScopedMessagePipeHandle pipe;
  std::string child_token = mojo::edk::GenerateRandomToken();
  if (launch_type == LaunchType::CHILD)
    pipe = CreateParentMessagePipe(pipe_token, child_token);
  else if (launch_type == LaunchType::PEER)
    pipe = ConnectToPeerProcess(channel.PassServerHandle());

  test_child_ =
      base::SpawnMultiProcessTestChild(test_child_main, command_line, options);
  channel.ChildProcessLaunched();

  if (launch_type == LaunchType::CHILD) {
    ChildProcessLaunched(test_child_.Handle(), channel.PassServerHandle(),
                         child_token, process_error_callback_);
  }

  CHECK(test_child_.IsValid());
  return pipe;
}

int MultiprocessTestHelper::WaitForChildShutdown() {
  CHECK(test_child_.IsValid());

  int rv = -1;
#if defined(OS_ANDROID)
  // On Android, we need to use a special function to wait for the child.
  CHECK(AndroidWaitForChildExitWithTimeout(
      test_child_, TestTimeouts::action_timeout(), &rv));
#else
  CHECK(
      test_child_.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &rv));
#endif
  test_child_.Close();
  return rv;
}

bool MultiprocessTestHelper::WaitForChildTestShutdown() {
  return WaitForChildShutdown() == 0;
}

// static
void MultiprocessTestHelper::ChildSetup() {
  CHECK(base::CommandLine::InitializedForCurrentProcess());

  std::string primordial_pipe_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kMojoPrimordialPipeToken);
  if (!primordial_pipe_token.empty()) {
    primordial_pipe = CreateChildMessagePipe(primordial_pipe_token);
#if defined(OS_MACOSX) && !defined(OS_IOS)
    CHECK(base::MachPortBroker::ChildSendTaskPortToParent("mojo_test"));
#endif
    SetParentPipeHandle(PlatformChannelPair::PassClientHandleFromParentProcess(
        *base::CommandLine::ForCurrentProcess()));
  } else {
    primordial_pipe = ConnectToPeerProcess(
        PlatformChannelPair::PassClientHandleFromParentProcess(
            *base::CommandLine::ForCurrentProcess()));
  }
}

// static
int MultiprocessTestHelper::RunClientMain(
    const base::Callback<int(MojoHandle)>& main) {
  return RunClientFunction([main](MojoHandle handle){
    return main.Run(handle);
  });
}

// static
int MultiprocessTestHelper::RunClientTestMain(
    const base::Callback<void(MojoHandle)>& main) {
  return RunClientFunction([main](MojoHandle handle) {
    main.Run(handle);
    return (::testing::Test::HasFatalFailure() ||
            ::testing::Test::HasNonfatalFailure()) ? 1 : 0;
  });
}

// static
mojo::ScopedMessagePipeHandle MultiprocessTestHelper::primordial_pipe;

}  // namespace test
}  // namespace edk
}  // namespace mojo
