// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_LINUX_SANDBOX_H_
#define MOJO_RUNNER_LINUX_SANDBOX_H_

#include "base/files/scoped_file.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/policy.h"
#include "sandbox/linux/syscall_broker/broker_process.h"

namespace mandoline {

// Encapsulates all tasks related to raising the sandbox for mandoline.
class LinuxSandbox {
 public:
  explicit LinuxSandbox(
      const std::vector<sandbox::syscall_broker::BrokerFilePermission>&
          permissions);
  ~LinuxSandbox();

  // Grabs a file descriptor to /proc.
  void Warmup();

  // Puts the user in a new PID namespace.
  void EngageNamespaceSandbox();

  // Starts a broker process and sets up seccomp-bpf to delegate decisions to
  // it.
  void EngageSeccompSandbox();

  // Performs the dropping of access to the outside world (drops the reference
  // to /proc acquired in Warmup().
  void Seal();

 private:
  bool warmed_up_;
  base::ScopedFD proc_fd_;
  scoped_ptr<sandbox::syscall_broker::BrokerProcess> broker_;
  scoped_ptr<sandbox::bpf_dsl::Policy> policy_;

  DISALLOW_COPY_AND_ASSIGN(LinuxSandbox);
};

}  // namespace mandoline

#endif  // MOJO_RUNNER_LINUX_SANDBOX_H_
