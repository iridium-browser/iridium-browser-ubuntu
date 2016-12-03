// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_

#include "chrome/common/service_process_util.h"

#include <signal.h>

#include <memory>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "chrome/common/multi_process_lock.h"
MultiProcessLock* TakeServiceRunningLock(bool waiting);
#endif

#if defined(OS_MACOSX)
#include "base/files/file_path_watcher.h"
#include "base/mac/scoped_cftyperef.h"

namespace base {
class CommandLine;
}

CFDictionaryRef CreateServiceProcessLaunchdPlist(base::CommandLine* cmd_line,
                                                 bool for_auto_launch);
#endif  // OS_MACOSX

namespace base {
class WaitableEvent;
}

// Watches for |kTerminateMessage| to be written to the file descriptor it is
// watching. When it reads |kTerminateMessage|, it performs |terminate_task_|.
// Used here to monitor the socket listening to g_signal_socket.
class ServiceProcessTerminateMonitor : public base::MessageLoopForIO::Watcher {
 public:

  enum {
    kTerminateMessage = 0xdecea5e
  };

  explicit ServiceProcessTerminateMonitor(const base::Closure& terminate_task);
  ~ServiceProcessTerminateMonitor() override;

  // MessageLoopForIO::Watcher overrides
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  base::Closure terminate_task_;
};

struct ServiceProcessState::StateData
    : public base::RefCountedThreadSafe<ServiceProcessState::StateData> {
  StateData();

  // WatchFileDescriptor needs to be set up by the thread that is going
  // to be monitoring it.
  void SignalReady(base::WaitableEvent* signal, bool* success);

#if defined(OS_MACOSX)
  bool WatchExecutable();

  base::ScopedCFTypeRef<CFDictionaryRef> launchd_conf;
  base::FilePathWatcher executable_watcher;
#endif  // OS_MACOSX
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  std::unique_ptr<MultiProcessLock> initializing_lock;
  std::unique_ptr<MultiProcessLock> running_lock;
#endif
  std::unique_ptr<ServiceProcessTerminateMonitor> terminate_monitor;
  base::MessageLoopForIO::FileDescriptorWatcher watcher;
  int sockets[2];
  struct sigaction old_action;
  bool set_action;

 protected:
  friend class base::RefCountedThreadSafe<ServiceProcessState::StateData>;
  virtual ~StateData();
};

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_
