// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/view_event_test_platform_part.h"

#include "base/memory/scoped_ptr.h"
#include "ui/aura/env.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/wm/core/wm_state.h"

namespace {

// ChromeViewsTestHelper implementation for non-ChromeOS environments, where the
// Ash desktop environment is available (use_ash=1, chromeos=0).
class ViewEventTestPlatformPartAsh : public ViewEventTestPlatformPart {
 public:
  explicit ViewEventTestPlatformPartAsh(ui::ContextFactory* context_factory);
  ~ViewEventTestPlatformPartAsh() override;

  // Overridden from ViewEventTestPlatformPart:
  gfx::NativeWindow GetContext() override {
    return NULL;  // No context, so that desktop tree hosts are used by default.
  }

 private:
  scoped_ptr<gfx::Screen> screen_;
  wm::WMState wm_state_;

  DISALLOW_COPY_AND_ASSIGN(ViewEventTestPlatformPartAsh);
};

ViewEventTestPlatformPartAsh::ViewEventTestPlatformPartAsh(
    ui::ContextFactory* context_factory)
    : screen_(views::CreateDesktopScreen()) {
  // http://crbug.com/154081 use ash::Shell code path below on win_ash bots when
  // interactive_ui_tests is brought up on that platform.
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
  aura::Env::CreateInstance(true);
  aura::Env::GetInstance()->set_context_factory(context_factory);
}

ViewEventTestPlatformPartAsh::~ViewEventTestPlatformPartAsh() {
  aura::Env::DeleteInstance();
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, nullptr);
}

}  // namespace

// static
ViewEventTestPlatformPart* ViewEventTestPlatformPart::Create(
    ui::ContextFactory* context_factory) {
  return new ViewEventTestPlatformPartAsh(context_factory);
}
