// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_
#define UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/views/views_delegate.h"

namespace wm {
class WMState;
}

namespace views {

class TestViewsDelegate : public ViewsDelegate {
 public:
  TestViewsDelegate();
  ~TestViewsDelegate() override;

  // If set to |true|, forces widgets that do not provide a native widget to use
  // DesktopNativeWidgetAura instead of whatever the default native widget would
  // be. This has no effect on ChromeOS.
  void set_use_desktop_native_widgets(bool desktop) {
    use_desktop_native_widgets_ = desktop;
  }

  void set_use_transparent_windows(bool transparent) {
    use_transparent_windows_ = transparent;
  }

  // Allows tests to provide a ContextFactory via the ViewsDelegate interface.
  void set_context_factory(ui::ContextFactory* context_factory) {
    context_factory_ = context_factory;
  }

  // ViewsDelegate:
#if defined(OS_WIN)
  HICON GetSmallWindowIcon() const override;
#endif
  void OnBeforeWidgetInit(Widget::InitParams* params,
                          internal::NativeWidgetDelegate* delegate) override;
  ui::ContextFactory* GetContextFactory() override;

 private:
  ui::ContextFactory* context_factory_;
  bool use_desktop_native_widgets_;
  bool use_transparent_windows_;

#if defined(USE_AURA)
  std::unique_ptr<wm::WMState> wm_state_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestViewsDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_VIEWS_DELEGATE_H_
