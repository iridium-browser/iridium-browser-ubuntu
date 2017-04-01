// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_helper.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/mus/test_window_tree_client_setup.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_port_local.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/wm/core/wm_state.h"

#if defined(USE_X11)
#include "ui/aura/window_tree_host_x11.h"
#include "ui/base/x/x11_util.h"  // nogncheck
#endif

namespace aura {
namespace test {

AuraTestHelper::AuraTestHelper(base::MessageLoopForUI* message_loop)
    : setup_called_(false), teardown_called_(false) {
  DCHECK(message_loop);
  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
#if defined(USE_X11)
  test::SetUseOverrideRedirectWindowByDefault(true);
#endif
  InitializeAuraEventGeneratorDelegate();
}

AuraTestHelper::~AuraTestHelper() {
  CHECK(setup_called_)
      << "AuraTestHelper::SetUp() never called.";
  CHECK(teardown_called_)
      << "AuraTestHelper::TearDown() never called.";
}

void AuraTestHelper::EnableMusWithTestWindowTree(
    WindowTreeClientDelegate* window_tree_delegate,
    WindowManagerDelegate* window_manager_delegate) {
  DCHECK(!setup_called_);
  DCHECK_EQ(Mode::LOCAL, mode_);
  mode_ = Mode::MUS_CREATE_WINDOW_TREE_CLIENT;
  window_tree_delegate_ = window_tree_delegate;
  window_manager_delegate_ = window_manager_delegate;
}

void AuraTestHelper::EnableMusWithWindowTreeClient(
    WindowTreeClient* window_tree_client) {
  DCHECK(!setup_called_);
  DCHECK_EQ(Mode::LOCAL, mode_);
  mode_ = Mode::MUS;
  window_tree_client_ = window_tree_client;
}

void AuraTestHelper::SetUp(ui::ContextFactory* context_factory,
                           ui::ContextFactoryPrivate* context_factory_private) {
  setup_called_ = true;

  if (mode_ != Mode::MUS) {
    // Assume if an explicit WindowTreeClient was created then a WmState was
    // already created.
    wm_state_ = base::MakeUnique<wm::WMState>();
  }
  // Needs to be before creating WindowTreeClient.
  focus_client_ = base::MakeUnique<TestFocusClient>();
  capture_client_ = base::MakeUnique<client::DefaultCaptureClient>();
  const Env::Mode env_mode =
      (mode_ == Mode::LOCAL) ? Env::Mode::LOCAL : Env::Mode::MUS;
  if (mode_ == Mode::MUS_CREATE_WINDOW_TREE_CLIENT)
    InitWindowTreeClient();
  if (!Env::GetInstanceDontCreate())
    env_ = Env::CreateInstance(env_mode);
  EnvTestHelper env_helper;
  // Always reset the mode. This really only matters for if Env was created
  // above.
  env_helper.SetMode(env_mode);
  env_helper.SetWindowTreeClient(window_tree_client_);
  Env::GetInstance()->SetActiveFocusClient(focus_client_.get(), nullptr);
  Env::GetInstance()->set_context_factory(context_factory);
  Env::GetInstance()->set_context_factory_private(context_factory_private);
  // Unit tests generally don't want to query the system, rather use the state
  // from RootWindow.
  env_helper.SetInputStateLookup(nullptr);
  env_helper.ResetEventState();

  ui::InitializeInputMethodForTesting();

  display::Screen* screen = display::Screen::GetScreen();
  gfx::Size host_size(screen ? screen->GetPrimaryDisplay().GetSizeInPixel()
                             : gfx::Size(800, 600));
  // TODO(sky): creating the screen and host should not happen for mus.
  test_screen_.reset(TestScreen::Create(host_size));
  if (!screen)
    display::Screen::SetScreenInstance(test_screen_.get());
  host_.reset(test_screen_->CreateHostForPrimaryDisplay());

  client::SetFocusClient(root_window(), focus_client_.get());
  client::SetCaptureClient(root_window(), capture_client());
  parenting_client_.reset(new TestWindowParentingClient(root_window()));

  root_window()->Show();
  // Ensure width != height so tests won't confuse them.
  host()->SetBoundsInPixels(gfx::Rect(host_size));

  if (mode_ == Mode::MUS_CREATE_WINDOW_TREE_CLIENT)
    window_tree()->AckAllChanges();
}

void AuraTestHelper::TearDown() {
  teardown_called_ = true;
  parenting_client_.reset();
  client::SetFocusClient(root_window(), nullptr);
  client::SetCaptureClient(root_window(), nullptr);
  host_.reset();
  ui::GestureRecognizer::Reset();
  if (display::Screen::GetScreen() == test_screen_.get())
    display::Screen::SetScreenInstance(nullptr);
  test_screen_.reset();

  Env::GetInstance()->SetActiveFocusClient(nullptr, nullptr);
  window_tree_client_setup_.reset();
  focus_client_.reset();
  capture_client_.reset();

  ui::ShutdownInputMethodForTesting();

  if (env_)
    env_.reset();
  else
    EnvTestHelper().SetWindowTreeClient(nullptr);
  wm_state_.reset();
}

void AuraTestHelper::RunAllPendingInMessageLoop() {
  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TestWindowTree* AuraTestHelper::window_tree() {
  return window_tree_client_setup_->window_tree();
}

WindowTreeClient* AuraTestHelper::window_tree_client() {
  return window_tree_client_;
}

client::CaptureClient* AuraTestHelper::capture_client() {
  return capture_client_.get();
}

void AuraTestHelper::InitWindowTreeClient() {
  window_tree_client_setup_ = base::MakeUnique<TestWindowTreeClientSetup>();
  window_tree_client_setup_->InitForWindowManager(window_tree_delegate_,
                                                  window_manager_delegate_);
  window_tree_client_ = window_tree_client_setup_->window_tree_client();
}

}  // namespace test
}  // namespace aura
