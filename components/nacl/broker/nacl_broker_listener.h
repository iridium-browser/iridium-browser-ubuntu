// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROKER_NACL_BROKER_LISTENER_H_
#define COMPONENTS_NACL_BROKER_NACL_BROKER_LISTENER_H_

#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "components/nacl/common/nacl_types.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Channel;
}

// The BrokerThread class represents the thread that handles the messages from
// the browser process and starts NaCl loader processes.
class NaClBrokerListener : public content::SandboxedProcessLauncherDelegate,
                           public IPC::Listener {
 public:
  NaClBrokerListener();
  ~NaClBrokerListener() override;

  void Listen();

  // content::SandboxedProcessLauncherDelegate implementation:
  void PreSpawnTarget(sandbox::TargetPolicy* policy, bool* success) override;

  // IPC::Listener implementation.
  void OnChannelConnected(int32 peer_pid) override;
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelError() override;

 private:
  void OnLaunchLoaderThroughBroker(const std::string& loader_channel_id);
  void OnLaunchDebugExceptionHandler(int32 pid,
                                     base::ProcessHandle process_handle,
                                     const std::string& startup_info);
  void OnStopBroker();

  base::Process browser_process_;
  scoped_ptr<IPC::Channel> channel_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerListener);
};

#endif  // COMPONENTS_NACL_BROKER_NACL_BROKER_LISTENER_H_
