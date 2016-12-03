// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
#define CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "content/shell/browser/shell_browser_context.h"

#if defined(OS_ANDROID)
namespace breakpad {
class CrashDumpManager;
}
#endif

namespace base {
class Thread;
}

namespace devtools_http_handler {
class DevToolsHttpHandler;
}

namespace net {
class NetLog;
}

namespace content {

class ShellBrowserMainParts : public BrowserMainParts {
 public:
  explicit ShellBrowserMainParts(const MainFunctionParams& parameters);
  ~ShellBrowserMainParts() override;

  // BrowserMainParts overrides.
  void PreEarlyInitialization() override;
#if defined(OS_ANDROID)
  int PreCreateThreads() override;
#endif
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  devtools_http_handler::DevToolsHttpHandler* devtools_http_handler() {
    return devtools_http_handler_.get();
  }

  ShellBrowserContext* browser_context() { return browser_context_.get(); }
  ShellBrowserContext* off_the_record_browser_context() {
    return off_the_record_browser_context_.get();
  }

  net::NetLog* net_log() { return net_log_.get(); }

 protected:
  virtual void InitializeBrowserContexts();
  virtual void InitializeMessageLoopContext();

  void set_browser_context(ShellBrowserContext* context) {
    browser_context_.reset(context);
  }
  void set_off_the_record_browser_context(ShellBrowserContext* context) {
    off_the_record_browser_context_.reset(context);
  }

 private:
#if defined(OS_ANDROID)
  std::unique_ptr<breakpad::CrashDumpManager> crash_dump_manager_;
#endif
  std::unique_ptr<net::NetLog> net_log_;
  std::unique_ptr<ShellBrowserContext> browser_context_;
  std::unique_ptr<ShellBrowserContext> off_the_record_browser_context_;

  // For running content_browsertests.
  const MainFunctionParams parameters_;
  bool run_message_loop_;

  std::unique_ptr<devtools_http_handler::DevToolsHttpHandler>
      devtools_http_handler_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
