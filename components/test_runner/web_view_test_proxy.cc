// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_view_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "components/test_runner/accessibility_controller.h"
#include "components/test_runner/event_sender.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/test_runner_for_specific_view.h"
#include "components/test_runner/text_input_controller.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_interfaces.h"

namespace test_runner {

WebViewTestProxyBase::WebViewTestProxyBase()
    : test_interfaces_(nullptr),
      delegate_(nullptr),
      web_view_(nullptr),
      web_widget_(nullptr),
      accessibility_controller_(new AccessibilityController(this)),
      event_sender_(new EventSender(this)),
      text_input_controller_(new TextInputController(this)),
      view_test_runner_(new TestRunnerForSpecificView(this)) {}

WebViewTestProxyBase::~WebViewTestProxyBase() {
  test_interfaces_->WindowClosed(this);
}

void WebViewTestProxyBase::SetInterfaces(WebTestInterfaces* interfaces) {
  test_interfaces_ = interfaces->GetTestInterfaces();
  test_interfaces_->WindowOpened(this);
}

void WebViewTestProxyBase::Reset() {
  accessibility_controller_->Reset();
  event_sender_->Reset();
  // text_input_controller_ doesn't have any state to reset.
  view_test_runner_->Reset();
}

void WebViewTestProxyBase::BindTo(blink::WebLocalFrame* frame) {
  accessibility_controller_->Install(frame);
  event_sender_->Install(frame);
  text_input_controller_->Install(frame);
  view_test_runner_->Install(frame);
}

}  // namespace test_runner
