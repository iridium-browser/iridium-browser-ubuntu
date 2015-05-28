// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_
#define CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/page_state.h"
#include "content/public/test/mock_render_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/web/WebFrame.h"

struct ViewMsg_Resize_Params;

namespace blink {
class WebWidget;
}

namespace gfx {
class Rect;
}

namespace content {
class ContentBrowserClient;
class ContentClient;
class ContentRendererClient;
class FakeCompositorDependencies;
class MockRenderProcess;
class PageState;
class RendererMainPlatformDelegate;
class RendererBlinkPlatformImplNoSandboxImpl;
class RendererScheduler;
class RenderView;

class RenderViewTest : public testing::Test {
 public:
  // A special BlinkPlatformImpl class for getting rid off the dependency to the
  // sandbox, which is not available in RenderViewTest.
  class RendererBlinkPlatformImplNoSandbox {
   public:
    RendererBlinkPlatformImplNoSandbox();
    ~RendererBlinkPlatformImplNoSandbox();
    blink::Platform* Get();

   private:
    scoped_ptr<RendererScheduler> renderer_scheduler_;
    scoped_ptr<RendererBlinkPlatformImplNoSandboxImpl> blink_platform_impl_;
  };

  RenderViewTest();
  ~RenderViewTest() override;

 protected:
  // Spins the message loop to process all messages that are currently pending.
  void ProcessPendingMessages();

  // Returns a pointer to the main frame.
  blink::WebLocalFrame* GetMainFrame();

  // Executes the given JavaScript in the context of the main frame. The input
  // is a NULL-terminated UTF-8 string.
  void ExecuteJavaScript(const char* js);

  // Executes the given JavaScript and sets the int value it evaluates to in
  // |result|.
  // Returns true if the JavaScript was evaluated correctly to an int value,
  // false otherwise.
  bool ExecuteJavaScriptAndReturnIntValue(const base::string16& script,
                                          int* result);

  // Loads the given HTML into the main frame as a data: URL and blocks until
  // the navigation is committed.
  void LoadHTML(const char* html);

  // Returns the current PageState.
  PageState GetCurrentPageState();

  // Navigates the main frame back or forward in session history and commits.
  // The caller must capture a PageState for the target page.
  void GoBack(const PageState& state);
  void GoForward(const PageState& state);

  // Sends one native key event over IPC.
  void SendNativeKeyEvent(const NativeWebKeyboardEvent& key_event);

  // Send a raw keyboard event to the renderer.
  void SendWebKeyboardEvent(const blink::WebKeyboardEvent& key_event);

  // Send a raw mouse event to the renderer.
  void SendWebMouseEvent(const blink::WebMouseEvent& key_event);

  // Returns the bounds (coordinates and size) of the element with id
  // |element_id|.  Returns an empty rect if such an element was not found.
  gfx::Rect GetElementBounds(const std::string& element_id);

  // Sends a left mouse click in the middle of the element with id |element_id|.
  // Returns true if the event was sent, false otherwise (typically because
  // the element was not found).
  bool SimulateElementClick(const std::string& element_id);

  // Sends a left mouse click at the |point|.
  void SimulatePointClick(const gfx::Point& point);

  // Sends a tap at the |rect|.
  void SimulateRectTap(const gfx::Rect& rect);

  // Simulates |node| being focused.
  void SetFocused(const blink::WebNode& node);

  // Simulates a navigation with a type of reload to the given url.
  void Reload(const GURL& url);

  // Returns the IPC message ID of the navigation message.
  uint32 GetNavigationIPCType();

  // Resize the view.
  void Resize(gfx::Size new_size,
              gfx::Rect resizer_rect,
              bool is_fullscreen);

  // These are all methods from RenderViewImpl that we expose to testing code.
  bool OnMessageReceived(const IPC::Message& msg);
  void DidNavigateWithinPage(blink::WebLocalFrame* frame,
                             bool is_new_navigation);
  void SendContentStateImmediately();
  blink::WebWidget* GetWebWidget();

  // Allows a subclass to override the various content client implementations.
  virtual ContentClient* CreateContentClient();
  virtual ContentBrowserClient* CreateContentBrowserClient();
  virtual ContentRendererClient* CreateContentRendererClient();

  // Allows a subclass to customize the initial size of the RenderView.
  virtual scoped_ptr<ViewMsg_Resize_Params> InitialSizeParams();

  // testing::Test
  void SetUp() override;

  void TearDown() override;

  base::MessageLoop msg_loop_;
  scoped_ptr<FakeCompositorDependencies> compositor_deps_;
  scoped_ptr<MockRenderProcess> mock_process_;
  // We use a naked pointer because we don't want to expose RenderViewImpl in
  // the embedder's namespace.
  RenderView* view_;
  RendererBlinkPlatformImplNoSandbox blink_platform_impl_;
  scoped_ptr<ContentClient> content_client_;
  scoped_ptr<ContentBrowserClient> content_browser_client_;
  scoped_ptr<ContentRendererClient> content_renderer_client_;
  scoped_ptr<MockRenderThread> render_thread_;

  // Used to setup the process so renderers can run.
  scoped_ptr<RendererMainPlatformDelegate> platform_;
  scoped_ptr<MainFunctionParams> params_;
  scoped_ptr<base::CommandLine> command_line_;

#if defined(OS_MACOSX)
  scoped_ptr<base::mac::ScopedNSAutoreleasePool> autorelease_pool_;
#endif

 private:
  void GoToOffset(int offset, const PageState& state);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_
