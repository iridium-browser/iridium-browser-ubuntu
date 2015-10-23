// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_TEST_INTERFACES_H_
#define COMPONENTS_TEST_RUNNER_TEST_INTERFACES_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/test_runner/mock_web_theme_engine.h"
#include "third_party/WebKit/public/platform/WebNonCopyable.h"

namespace blink {
class WebFrame;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace test_runner {

class AccessibilityController;
class AppBannerClient;
class EventSender;
class GamepadController;
class TestRunner;
class TextInputController;
class WebTestDelegate;
class WebTestProxyBase;

class TestInterfaces {
 public:
  TestInterfaces();
  ~TestInterfaces();

  void SetWebView(blink::WebView* web_view, WebTestProxyBase* proxy);
  void SetDelegate(WebTestDelegate* delegate);
  void BindTo(blink::WebFrame* frame);
  void ResetTestHelperControllers();
  void ResetAll();
  void SetTestIsRunning(bool running);
  void ConfigureForTestWithURL(const blink::WebURL& test_url,
                               bool generate_pixels);
  void SetAppBannerClient(AppBannerClient* app_banner_client);

  void WindowOpened(WebTestProxyBase* proxy);
  void WindowClosed(WebTestProxyBase* proxy);

  AccessibilityController* GetAccessibilityController();
  EventSender* GetEventSender();
  TestRunner* GetTestRunner();
  WebTestDelegate* GetDelegate();
  WebTestProxyBase* GetProxy();
  const std::vector<WebTestProxyBase*>& GetWindowList();
  blink::WebThemeEngine* GetThemeEngine();
  AppBannerClient* GetAppBannerClient();

 private:
  scoped_ptr<AccessibilityController> accessibility_controller_;
  scoped_ptr<EventSender> event_sender_;
  base::WeakPtr<GamepadController> gamepad_controller_;
  scoped_ptr<TextInputController> text_input_controller_;
  scoped_ptr<TestRunner> test_runner_;
  WebTestDelegate* delegate_;
  WebTestProxyBase* proxy_;
  AppBannerClient* app_banner_client_;

  std::vector<WebTestProxyBase*> window_list_;
  scoped_ptr<MockWebThemeEngine> theme_engine_;

  DISALLOW_COPY_AND_ASSIGN(TestInterfaces);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_TEST_INTERFACES_H_
