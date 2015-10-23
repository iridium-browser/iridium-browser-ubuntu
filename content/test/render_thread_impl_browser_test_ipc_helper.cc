// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/render_thread_impl_browser_test_ipc_helper.h"

#include "content/common/mojo/channel_init.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class RenderThreadImplBrowserIPCTestHelper::DummyListener
    : public IPC::Listener {
 public:
  bool OnMessageReceived(const IPC::Message& message) override { return true; }
};

RenderThreadImplBrowserIPCTestHelper::RenderThreadImplBrowserIPCTestHelper() {
  message_loop_.reset(new base::MessageLoopForIO());

  channel_id_ = IPC::Channel::GenerateVerifiedChannelID("");
  dummy_listener_.reset(new DummyListener());

  SetupIpcThread();

  if (IPC::ChannelMojo::ShouldBeUsed()) {
    SetupMojo();
  } else {
    channel_ = IPC::ChannelProxy::Create(channel_id_, IPC::Channel::MODE_SERVER,
                                         dummy_listener_.get(),
                                         ipc_thread_->task_runner(), nullptr);
  }
}

RenderThreadImplBrowserIPCTestHelper::~RenderThreadImplBrowserIPCTestHelper() {
}

void RenderThreadImplBrowserIPCTestHelper::SetupIpcThread() {
  ipc_thread_.reset(new base::Thread("test_ipc_thread"));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  ASSERT_TRUE(ipc_thread_->StartWithOptions(options));
}

void RenderThreadImplBrowserIPCTestHelper::SetupMojo() {
  InitializeMojo();

  ipc_support_.reset(new IPC::ScopedIPCSupport(ipc_thread_->task_runner()));
  mojo_application_host_.reset(new MojoApplicationHost());
  mojo_application_host_->OverrideIOTaskRunnerForTest(
      ipc_thread_->task_runner());

  channel_ = IPC::ChannelProxy::Create(
      IPC::ChannelMojo::CreateServerFactory(ipc_thread_->task_runner(),
                                            channel_id_, nullptr),
      dummy_listener_.get(), ipc_thread_->task_runner());

  mojo_application_host_->Init();
  mojo_application_host_->Activate(channel_.get(),
                                   base::GetCurrentProcessHandle());
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImplBrowserIPCTestHelper::GetIOTaskRunner() const {
  return ipc_thread_->task_runner();
}

}  // namespace content
