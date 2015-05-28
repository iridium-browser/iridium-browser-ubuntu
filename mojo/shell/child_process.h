// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_CHILD_PROCESS_H_
#define SHELL_CHILD_PROCESS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace base {
class CommandLine;
}

namespace mojo {
namespace shell {

// A base class for child processes -- i.e., code that is actually run within
// the child process. (Instances are manufactured by |Create()|.)
class ChildProcess {
 public:
  virtual ~ChildProcess();

  // Returns null if the command line doesn't indicate that this is a child
  // process. |main()| should call this, and if it returns non-null it should
  // call |Main()| (without a message loop on the current thread).
  static scoped_ptr<ChildProcess> Create(const base::CommandLine& command_line);

  // To be implemented by subclasses. This is the "entrypoint" for a child
  // process. Run with no message loop for the main thread.
  virtual void Main() = 0;

 protected:
  ChildProcess();

  embedder::ScopedPlatformHandle* platform_channel() {
    return &platform_channel_;
  }

 private:
  // Available in |Main()| (after a successful |Create()|).
  embedder::ScopedPlatformHandle platform_channel_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcess);
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_CHILD_PROCESS_H_
