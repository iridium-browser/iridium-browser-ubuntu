// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
#define CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace shell {
class Connector;
}

namespace content {

// MojoShellContext manages the browser's connection to the ServiceManager,
// hosting a new in-process ServiceManager if the browser was not launched from
// an external one.
class CONTENT_EXPORT MojoShellContext {
 public:
  MojoShellContext();
  ~MojoShellContext();

  // Returns a shell::Connector that can be used on the IO thread.
  static shell::Connector* GetConnectorForIOThread();

 private:
  class InProcessServiceManagerContext;

  scoped_refptr<InProcessServiceManagerContext> in_process_context_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
