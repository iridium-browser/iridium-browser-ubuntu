// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_PROCESS_OBSERVER_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_PROCESS_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_process_observer.h"
#include "ipc/ipc_platform_file.h"

namespace blink {
class WebFrame;
}

namespace content {

class RenderView;
class WebKitTestRunner;
class WebTestDelegate;
class WebTestInterfaces;

class LayoutTestRenderProcessObserver : public RenderProcessObserver {
 public:
  static LayoutTestRenderProcessObserver* GetInstance();

  LayoutTestRenderProcessObserver();
  ~LayoutTestRenderProcessObserver() override;

  void SetTestDelegate(WebTestDelegate* delegate);
  void SetMainWindow(RenderView* view);

  // RenderProcessObserver implementation.
  void WebKitInitialized() override;
  void OnRenderProcessShutdown() override;
  bool OnControlMessageReceived(const IPC::Message& message) override;

  WebTestDelegate* test_delegate() const {
    return test_delegate_;
  }
  WebTestInterfaces* test_interfaces() const {
    return test_interfaces_.get();
  }
  WebKitTestRunner* main_test_runner() const { return main_test_runner_; }
  const base::FilePath& webkit_source_dir() const { return webkit_source_dir_; }

 private:
  // Message handlers.
  void OnSetWebKitSourceDir(const base::FilePath& webkit_source_dir);

  WebKitTestRunner* main_test_runner_;
  WebTestDelegate* test_delegate_;
  scoped_ptr<WebTestInterfaces> test_interfaces_;

  base::FilePath webkit_source_dir_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestRenderProcessObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_PROCESS_OBSERVER_H_
