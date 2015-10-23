// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_permission_broker_client.h"

#include <fcntl.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "dbus/file_descriptor.h"

namespace chromeos {

namespace {

// So that real devices can be accessed by tests and "Chromium OS on Linux" this
// function implements a simplified version of the method implemented by the
// permission broker by opening the path specified and returning the resulting
// file descriptor.
void OpenPathAndValidate(
    const std::string& path,
    const PermissionBrokerClient::OpenPathCallback& callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  int fd = HANDLE_EINTR(open(path.c_str(), O_RDWR));
  if (fd < 0) {
    PLOG(ERROR) << "Failed to open '" << path << "'";
  } else {
    dbus::FileDescriptor dbus_fd;
    dbus_fd.PutValue(fd);
    dbus_fd.CheckValidity();
    task_runner->PostTask(FROM_HERE,
                          base::Bind(callback, base::Passed(&dbus_fd)));
  }
}

}  // namespace

FakePermissionBrokerClient::FakePermissionBrokerClient() {}

FakePermissionBrokerClient::~FakePermissionBrokerClient() {}

void FakePermissionBrokerClient::Init(dbus::Bus* bus) {}

void FakePermissionBrokerClient::CheckPathAccess(
    const std::string& path,
    const ResultCallback& callback) {
  callback.Run(true);
}

void FakePermissionBrokerClient::RequestPathAccess(
    const std::string& path,
    int interface_id,
    const ResultCallback& callback) {
  callback.Run(true);
}

void FakePermissionBrokerClient::OpenPath(const std::string& path,
                                          const OpenPathCallback& callback) {
  base::WorkerPool::PostTask(FROM_HERE,
                             base::Bind(&OpenPathAndValidate, path, callback,
                                        base::ThreadTaskRunnerHandle::Get()),
                             false);
}

void FakePermissionBrokerClient::RequestTcpPortAccess(
    uint16 port,
    const std::string& interface,
    const dbus::FileDescriptor& lifeline_fd,
    const ResultCallback& callback) {
  DCHECK(lifeline_fd.is_valid());
  callback.Run(true);
}

void FakePermissionBrokerClient::RequestUdpPortAccess(
    uint16 port,
    const std::string& interface,
    const dbus::FileDescriptor& lifeline_fd,
    const ResultCallback& callback) {
  DCHECK(lifeline_fd.is_valid());
  callback.Run(true);
}

void FakePermissionBrokerClient::ReleaseTcpPort(
    uint16 port,
    const std::string& interface,
    const ResultCallback& callback) {
  callback.Run(true);
}

void FakePermissionBrokerClient::ReleaseUdpPort(
    uint16 port,
    const std::string& interface,
    const ResultCallback& callback) {
  callback.Run(true);
}

}  // namespace chromeos
