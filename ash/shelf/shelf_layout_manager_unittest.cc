// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/focus_cycler.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/shelf_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {
namespace {

void StepWidgetLayerAnimatorToEnd(views::Widget* widget) {
  widget->GetNativeView()->layer()->GetAnimator()->Step(
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
}

ShelfWidget* GetShelfWidget() {
  return Shell::GetPrimaryRootWindowController()->shelf();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();
}

SystemTray* GetSystemTray() {
  return Shell::GetPrimaryRootWindowController()->GetSystemTray();
}

// Class which waits till the shelf finishes animating to the target size and
// counts the number of animation steps.
class ShelfAnimationWaiter : views::WidgetObserver {
 public:
  explicit ShelfAnimationWaiter(const gfx::Rect& target_bounds)
      : target_bounds_(target_bounds),
        animation_steps_(0),
        done_waiting_(false) {
    GetShelfWidget()->AddObserver(this);
  }

  ~ShelfAnimationWaiter() override { GetShelfWidget()->RemoveObserver(this); }

  // Wait till the shelf finishes animating to its expected bounds.
  void WaitTillDoneAnimating() {
    if (IsDoneAnimating())
      done_waiting_ = true;
    else
      base::MessageLoop::current()->Run();
  }

  // Returns true if the animation has completed and it was valid.
  bool WasValidAnimation() const {
    return done_waiting_ && animation_steps_ > 0;
  }

 private:
  // Returns true if shelf has finished animating to the target size.
  bool IsDoneAnimating() const {
    ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
    gfx::Rect current_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
    int size = layout_manager->PrimaryAxisValue(current_bounds.height(),
        current_bounds.width());
    int desired_size = layout_manager->PrimaryAxisValue(target_bounds_.height(),
        target_bounds_.width());
    return (size == desired_size);
  }

  // views::WidgetObserver override.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (done_waiting_)
      return;

    ++animation_steps_;
    if (IsDoneAnimating()) {
      done_waiting_ = true;
      base::MessageLoop::current()->Quit();
    }
  }

  gfx::Rect target_bounds_;
  int animation_steps_;
  bool done_waiting_;

  DISALLOW_COPY_AND_ASSIGN(ShelfAnimationWaiter);
};

class ShelfDragCallback {
 public:
  ShelfDragCallback(const gfx::Rect& not_visible, const gfx::Rect& visible)
      : not_visible_bounds_(not_visible),
        visible_bounds_(visible),
        was_visible_on_drag_start_(false) {
    EXPECT_EQ(not_visible_bounds_.bottom(), visible_bounds_.bottom());
  }

  virtual ~ShelfDragCallback() {
  }

  void ProcessScroll(ui::EventType type, const gfx::Vector2dF& delta) {
    if (GetShelfLayoutManager()->visibility_state() == ash::SHELF_HIDDEN)
      return;

    if (type == ui::ET_GESTURE_SCROLL_BEGIN) {
      scroll_ = gfx::Vector2dF();
      was_visible_on_drag_start_ = GetShelfLayoutManager()->IsVisible();
      return;
    }

    // The state of the shelf at the end of the gesture is tested separately.
    if (type == ui::ET_GESTURE_SCROLL_END)
      return;

    if (type == ui::ET_GESTURE_SCROLL_UPDATE)
      scroll_.Add(delta);

    gfx::Rect shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
    if (GetShelfLayoutManager()->IsHorizontalAlignment()) {
      EXPECT_EQ(not_visible_bounds_.bottom(), shelf_bounds.bottom());
      EXPECT_EQ(visible_bounds_.bottom(), shelf_bounds.bottom());
    } else if (SHELF_ALIGNMENT_RIGHT ==
        GetShelfLayoutManager()->GetAlignment()){
      EXPECT_EQ(not_visible_bounds_.right(), shelf_bounds.right());
      EXPECT_EQ(visible_bounds_.right(), shelf_bounds.right());
    } else if (SHELF_ALIGNMENT_LEFT ==
        GetShelfLayoutManager()->GetAlignment()) {
      EXPECT_EQ(not_visible_bounds_.x(), shelf_bounds.x());
      EXPECT_EQ(visible_bounds_.x(), shelf_bounds.x());
    }

    // if the shelf is being dimmed test dimmer bounds as well.
    if (GetShelfWidget()->GetDimsShelf())
      EXPECT_EQ(GetShelfWidget()->GetWindowBoundsInScreen(),
                GetShelfWidget()->GetDimmerBoundsForTest());

    // The shelf should never be smaller than the hidden state.
    EXPECT_GE(shelf_bounds.height(), not_visible_bounds_.height());
    float scroll_delta = GetShelfLayoutManager()->PrimaryAxisValue(
        scroll_.y(),
        scroll_.x());
    bool increasing_drag =
        GetShelfLayoutManager()->SelectValueForShelfAlignment(
            scroll_delta < 0,
            scroll_delta > 0,
            scroll_delta < 0,
            scroll_delta > 0);
    int shelf_size = GetShelfLayoutManager()->PrimaryAxisValue(
        shelf_bounds.height(),
        shelf_bounds.width());
    int visible_bounds_size = GetShelfLayoutManager()->PrimaryAxisValue(
        visible_bounds_.height(),
        visible_bounds_.width());
    int not_visible_bounds_size = GetShelfLayoutManager()->PrimaryAxisValue(
        not_visible_bounds_.height(),
        not_visible_bounds_.width());
    if (was_visible_on_drag_start_) {
      if (increasing_drag) {
        // If dragging inwards from the visible state, then the shelf should
        // increase in size, but not more than the scroll delta.
        EXPECT_LE(visible_bounds_size, shelf_size);
        EXPECT_LE(std::abs(shelf_size - visible_bounds_size),
                  std::abs(scroll_delta));
      } else {
        if (shelf_size > not_visible_bounds_size) {
          // If dragging outwards from the visible state, then the shelf
          // should decrease in size, until it reaches the minimum size.
          EXPECT_EQ(shelf_size, visible_bounds_size - std::abs(scroll_delta));
        }
      }
    } else {
      if (std::abs(scroll_delta) <
          visible_bounds_size - not_visible_bounds_size) {
        // Tests that the shelf sticks with the touch point during the drag
        // until the shelf is completely visible.
        EXPECT_EQ(shelf_size, not_visible_bounds_size + std::abs(scroll_delta));
      } else {
        // Tests that after the shelf is completely visible, the shelf starts
        // resisting the drag.
        EXPECT_LT(shelf_size, not_visible_bounds_size + std::abs(scroll_delta));
      }
    }
  }

 private:
  const gfx::Rect not_visible_bounds_;
  const gfx::Rect visible_bounds_;
  gfx::Vector2dF scroll_;
  bool was_visible_on_drag_start_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDragCallback);
};

class ShelfLayoutObserverTest : public ShelfLayoutManagerObserver {
 public:
  ShelfLayoutObserverTest()
      : changed_auto_hide_state_(false) {
  }

  ~ShelfLayoutObserverTest() override {}

  bool changed_auto_hide_state() const { return changed_auto_hide_state_; }

 private:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override {
    changed_auto_hide_state_ = true;
  }

  bool changed_auto_hide_state_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutObserverTest);
};

// Trivial item implementation that tracks its views for testing.
class TestItem : public SystemTrayItem {
 public:
  TestItem()
      : SystemTrayItem(GetSystemTray()),
        tray_view_(NULL),
        default_view_(NULL),
        detailed_view_(NULL),
        notification_view_(NULL) {}

  views::View* CreateTrayView(user::LoginStatus status) override {
    tray_view_ = new views::View;
    // Add a label so it has non-zero width.
    tray_view_->SetLayoutManager(new views::FillLayout);
    tray_view_->AddChildView(new views::Label(base::UTF8ToUTF16("Tray")));
    return tray_view_;
  }

  views::View* CreateDefaultView(user::LoginStatus status) override {
    default_view_ = new views::View;
    default_view_->SetLayoutManager(new views::FillLayout);
    default_view_->AddChildView(new views::Label(base::UTF8ToUTF16("Default")));
    return default_view_;
  }

  views::View* CreateDetailedView(user::LoginStatus status) override {
    detailed_view_ = new views::View;
    detailed_view_->SetLayoutManager(new views::FillLayout);
    detailed_view_->AddChildView(
        new views::Label(base::UTF8ToUTF16("Detailed")));
    return detailed_view_;
  }

  views::View* CreateNotificationView(user::LoginStatus status) override {
    notification_view_ = new views::View;
    return notification_view_;
  }

  void DestroyTrayView() override { tray_view_ = NULL; }

  void DestroyDefaultView() override { default_view_ = NULL; }

  void DestroyDetailedView() override { detailed_view_ = NULL; }

  void DestroyNotificationView() override { notification_view_ = NULL; }

  void UpdateAfterLoginStatusChange(user::LoginStatus status) override {}

  views::View* tray_view() const { return tray_view_; }
  views::View* default_view() const { return default_view_; }
  views::View* detailed_view() const { return detailed_view_; }
  views::View* notification_view() const { return notification_view_; }

 private:
  views::View* tray_view_;
  views::View* default_view_;
  views::View* detailed_view_;
  views::View* notification_view_;

  DISALLOW_COPY_AND_ASSIGN(TestItem);
};

}  // namespace

class ShelfLayoutManagerTest : public ash::test::AshTestBase {
 public:
  ShelfLayoutManagerTest() {}

  void SetState(ShelfLayoutManager* shelf,
                ShelfVisibilityState state) {
    shelf->SetState(state);
  }

  void UpdateAutoHideStateNow() {
    GetShelfLayoutManager()->UpdateAutoHideStateNow();
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = new aura::Window(NULL);
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    ParentWindowInPrimaryRootWindow(window);
    return window;
  }

  aura::Window* CreateTestWindowInParent(aura::Window* root_window) {
    aura::Window* window = new aura::Window(NULL);
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(window, root_window, gfx::Rect());
    return window;
  }

  views::Widget* CreateTestWidgetWithParams(
      const views::Widget::InitParams& params) {
    views::Widget* out = new views::Widget;
    out->Init(params);
    out->Show();
    return out;
  }

  // Create a simple widget attached to the current context (will
  // delete on TearDown).
  views::Widget* CreateTestWidget() {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    params.context = CurrentContext();
    return CreateTestWidgetWithParams(params);
  }

  void RunGestureDragTests(gfx::Vector2d);

  // Turn on the lock screen.
  void LockScreen() {
    Shell::GetInstance()->session_state_delegate()->LockScreen();
    // The test session state delegate does not fire the lock state change.
    Shell::GetInstance()->OnLockStateChanged(true);
  }

  // Turn off the lock screen.
  void UnlockScreen() {
    Shell::GetInstance()->session_state_delegate()->UnlockScreen();
    // The test session state delegate does not fire the lock state change.
    Shell::GetInstance()->OnLockStateChanged(false);
  }

  // Open the add user screen if |show| is true, otherwise end it.
  void ShowAddUserScreen(bool show) {
    SetUserAddingScreenRunning(show);
    ShelfLayoutManager* manager = GetShelfWidget()->shelf_layout_manager();
    manager->SessionStateChanged(
        show ? SessionStateDelegate::SESSION_STATE_LOGIN_SECONDARY :
               SessionStateDelegate::SESSION_STATE_ACTIVE);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManagerTest);
};

void ShelfLayoutManagerTest::RunGestureDragTests(gfx::Vector2d delta) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  widget->Init(params);
  widget->Show();
  widget->Maximize();

  // The time delta should be large enough to prevent accidental fling creation.
  const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);

  aura::Window* window = widget->GetNativeWindow();
  shelf->LayoutShelf();

  gfx::Rect shelf_shown = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Rect bounds_shelf = window->bounds();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  gfx::Rect bounds_noshelf = window->bounds();
  gfx::Rect shelf_hidden = GetShelfWidget()->GetWindowBoundsInScreen();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->LayoutShelf();

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  const int kNumScrollSteps = 4;
  ShelfDragCallback handler(shelf_hidden, shelf_shown);

  // Swipe up on the shelf. This should not change any state.
  gfx::Point start = GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
  gfx::Point end = start + delta;

  // Swipe down on the shelf to hide it.
  generator.GestureScrollSequenceWithCallback(
      start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_NE(bounds_shelf.ToString(), window->bounds().ToString());
  EXPECT_NE(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up to show the shelf.
  generator.GestureScrollSequenceWithCallback(
      end,
      start,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(GetShelfWidget()->GetDimmerBoundsForTest(),
            GetShelfWidget()->GetWindowBoundsInScreen());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up again. The shelf should hide.
  end = start - delta;
  generator.GestureScrollSequenceWithCallback(
      start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up yet again to show it.
  end = start + delta;
  generator.GestureScrollSequenceWithCallback(
      end,
      start,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));

  // Swipe down very little. It shouldn't change any state.
  if (GetShelfLayoutManager()->IsHorizontalAlignment())
    end.set_y(start.y() + shelf_shown.height() * 3 / 10);
  else if (SHELF_ALIGNMENT_LEFT == GetShelfLayoutManager()->GetAlignment())
    end.set_x(start.x() - shelf_shown.width() * 3 / 10);
  else if (SHELF_ALIGNMENT_RIGHT == GetShelfLayoutManager()->GetAlignment())
    end.set_x(start.x() + shelf_shown.width() * 3 / 10);
  generator.GestureScrollSequence(start, end, kTimeDelta, 5);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  end = start + delta;
  generator.GestureScrollSequenceWithCallback(
      start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(GetShelfWidget()->GetDimmerBoundsForTest(), gfx::Rect());
  EXPECT_EQ(bounds_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up in extended hit region to show it.
  gfx::Point extended_start = start;
  if (GetShelfLayoutManager()->IsHorizontalAlignment())
    extended_start.set_y(GetShelfWidget()->GetWindowBoundsInScreen().y() -1);
  else if (SHELF_ALIGNMENT_LEFT == GetShelfLayoutManager()->GetAlignment())
    extended_start.set_x(
        GetShelfWidget()->GetWindowBoundsInScreen().right() + 1);
  else if (SHELF_ALIGNMENT_RIGHT == GetShelfLayoutManager()->GetAlignment())
    extended_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().x() - 1);
  end = extended_start - delta;
  generator.GestureScrollSequenceWithCallback(
      extended_start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(GetShelfWidget()->GetDimmerBoundsForTest(),
            GetShelfWidget()->GetWindowBoundsInScreen());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  end = start + delta;
  generator.GestureScrollSequenceWithCallback(
      start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(GetShelfWidget()->GetDimmerBoundsForTest(), gfx::Rect());
  EXPECT_EQ(bounds_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up outside the hit area. This should not change anything.
  gfx::Point outside_start = gfx::Point(
      (GetShelfWidget()->GetWindowBoundsInScreen().x() +
       GetShelfWidget()->GetWindowBoundsInScreen().right())/2,
      GetShelfWidget()->GetWindowBoundsInScreen().y() - 50);
  end = outside_start + delta;
  generator.GestureScrollSequence(
      outside_start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up from below the shelf where a bezel would be, this should show the
  // shelf.
  gfx::Point below_start = start;
  if (GetShelfLayoutManager()->IsHorizontalAlignment())
    below_start.set_y(GetShelfWidget()->GetWindowBoundsInScreen().bottom() + 1);
  else if (SHELF_ALIGNMENT_LEFT == GetShelfLayoutManager()->GetAlignment())
    below_start.set_x(
        GetShelfWidget()->GetWindowBoundsInScreen().x() - 1);
  else if (SHELF_ALIGNMENT_RIGHT == GetShelfLayoutManager()->GetAlignment())
    below_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().right() + 1);
  end = below_start - delta;
  generator.GestureScrollSequence(
      below_start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(GetShelfWidget()->GetDimmerBoundsForTest(),
            GetShelfWidget()->GetWindowBoundsInScreen());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  end = start + delta;
  generator.GestureScrollSequenceWithCallback(
      start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(GetShelfWidget()->GetDimmerBoundsForTest(), gfx::Rect());
  EXPECT_EQ(bounds_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Put |widget| into fullscreen. Set the shelf to be auto hidden when |widget|
  // is fullscreen. (eg browser immersive fullscreen).
  widget->SetFullscreen(true);
  wm::GetWindowState(window)->set_hide_shelf_when_fullscreen(false);
  shelf->UpdateVisibilityState();

  gfx::Rect bounds_fullscreen = window->bounds();
  EXPECT_TRUE(widget->IsFullscreen());
  EXPECT_NE(bounds_noshelf.ToString(), bounds_fullscreen.ToString());

  // Swipe up. This should show the shelf.
  end = below_start - delta;
  generator.GestureScrollSequenceWithCallback(
      below_start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(bounds_fullscreen.ToString(), window->bounds().ToString());

  // Swipe up again. This should hide the shelf.
  generator.GestureScrollSequenceWithCallback(
      below_start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(bounds_fullscreen.ToString(), window->bounds().ToString());

  // Set the shelf to be hidden when |widget| is fullscreen. (eg tab fullscreen
  // with or without immersive browser fullscreen).
  wm::GetWindowState(window)->set_hide_shelf_when_fullscreen(true);
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Swipe-up. This should not change anything.
  end = start - delta;
  generator.GestureScrollSequenceWithCallback(
      below_start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(bounds_fullscreen.ToString(), window->bounds().ToString());

  // Close actually, otherwise further event may be affected since widget
  // is fullscreen status.
  widget->Close();
  RunAllPendingInMessageLoop();

  // The shelf should be shown because there are no more visible windows.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Swipe-up to hide. This should have no effect because there are no visible
  // windows.
  end = below_start - delta;
  generator.GestureScrollSequenceWithCallback(
      below_start,
      end,
      kTimeDelta,
      kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
}

// Need to be implemented.  http://crbug.com/111279.
#if defined(OS_WIN)
#define MAYBE_SetVisible DISABLED_SetVisible
#else
#define MAYBE_SetVisible SetVisible
#endif
// Makes sure SetVisible updates work area and widget appropriately.
TEST_F(ShelfLayoutManagerTest, MAYBE_SetVisible) {
  ShelfWidget* shelf = GetShelfWidget();
  ShelfLayoutManager* manager = shelf->shelf_layout_manager();
  // Force an initial layout.
  manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());

  gfx::Rect status_bounds(
      shelf->status_area_widget()->GetWindowBoundsInScreen());
  gfx::Rect shelf_bounds(
      shelf->GetWindowBoundsInScreen());
  int shelf_height = manager->GetIdealBounds().height();
  gfx::Screen* screen = Shell::GetScreen();
  gfx::Display display = screen->GetDisplayNearestWindow(
      Shell::GetPrimaryRootWindow());
  ASSERT_NE(-1, display.id());
  // Bottom inset should be the max of widget heights.
  EXPECT_EQ(shelf_height, display.GetWorkAreaInsets().bottom());

  // Hide the shelf.
  SetState(manager, SHELF_HIDDEN);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf);
  StepWidgetLayerAnimatorToEnd(shelf->status_area_widget());
  EXPECT_EQ(SHELF_HIDDEN, manager->visibility_state());
  display = screen->GetDisplayNearestWindow(
      Shell::GetPrimaryRootWindow());

  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());

  // Make sure the bounds of the two widgets changed.
  EXPECT_GE(shelf->GetNativeView()->bounds().y(),
            screen->GetPrimaryDisplay().bounds().bottom());
  EXPECT_GE(shelf->status_area_widget()->GetNativeView()->bounds().y(),
            screen->GetPrimaryDisplay().bounds().bottom());

  // And show it again.
  SetState(manager, SHELF_VISIBLE);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf);
  StepWidgetLayerAnimatorToEnd(shelf->status_area_widget());
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());
  display = screen->GetDisplayNearestWindow(
      Shell::GetPrimaryRootWindow());
  EXPECT_EQ(shelf_height, display.GetWorkAreaInsets().bottom());

  // Make sure the bounds of the two widgets changed.
  shelf_bounds = shelf->GetNativeView()->bounds();
  EXPECT_LT(shelf_bounds.y(), screen->GetPrimaryDisplay().bounds().bottom());
  status_bounds = shelf->status_area_widget()->GetNativeView()->bounds();
  EXPECT_LT(status_bounds.y(),
            screen->GetPrimaryDisplay().bounds().bottom());
}

// Makes sure shelf alignment is correct for lock screen.
TEST_F(ShelfLayoutManagerTest, SideAlignmentInteractionWithLockScreen) {
  ShelfLayoutManager* manager = GetShelfWidget()->shelf_layout_manager();
  manager->SetAlignment(SHELF_ALIGNMENT_LEFT);
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, manager->GetAlignment());
  LockScreen();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, manager->GetAlignment());
  UnlockScreen();
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, manager->GetAlignment());
}

// Makes sure shelf alignment is correct for add user screen.
TEST_F(ShelfLayoutManagerTest, SideAlignmentInteractionWithAddUserScreen) {
  ShelfLayoutManager* manager = GetShelfWidget()->shelf_layout_manager();
  manager->SetAlignment(SHELF_ALIGNMENT_LEFT);
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, manager->GetAlignment());
  ShowAddUserScreen(true);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, manager->GetAlignment());
  ShowAddUserScreen(false);
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, manager->GetAlignment());
}

// Makes sure shelf alignment is correct for login screen.
TEST_F(ShelfLayoutManagerTest, SideAlignmentInteractionWithLoginScreen) {
  ShelfLayoutManager* manager = GetShelfWidget()->shelf_layout_manager();
  ASSERT_EQ(SHELF_ALIGNMENT_BOTTOM, manager->GetAlignment());
  SetUserLoggedIn(false);
  SetSessionStarted(false);

  // The test session state delegate does not fire state changes.
  SetSessionStarting();
  manager->SessionStateChanged(
      Shell::GetInstance()->session_state_delegate()->GetSessionState());

  // Login sets alignment preferences before the session completes startup.
  manager->SetAlignment(SHELF_ALIGNMENT_LEFT);
  SetUserLoggedIn(true);
  SetSessionStarted(true);

  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, manager->GetAlignment());
  // Ensure that the shelf has been notified.
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, GetShelfWidget()->shelf()->alignment());
}

// Makes sure LayoutShelf invoked while animating cleans things up.
TEST_F(ShelfLayoutManagerTest, LayoutShelfWhileAnimating) {
  ShelfWidget* shelf = GetShelfWidget();
  // Force an initial layout.
  shelf->shelf_layout_manager()->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->shelf_layout_manager()->visibility_state());

  // Hide the shelf.
  SetState(shelf->shelf_layout_manager(), SHELF_HIDDEN);
  shelf->shelf_layout_manager()->LayoutShelf();
  EXPECT_EQ(SHELF_HIDDEN, shelf->shelf_layout_manager()->visibility_state());
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestWindow(
      Shell::GetPrimaryRootWindow());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());

  // Make sure the bounds of the two widgets changed.
  EXPECT_GE(shelf->GetNativeView()->bounds().y(),
            Shell::GetScreen()->GetPrimaryDisplay().bounds().bottom());
  EXPECT_GE(shelf->status_area_widget()->GetNativeView()->bounds().y(),
            Shell::GetScreen()->GetPrimaryDisplay().bounds().bottom());
}

// Test that switching to a different visibility state does not restart the
// shelf show / hide animation if it is already running. (crbug.com/250918)
TEST_F(ShelfLayoutManagerTest, SetStateWhileAnimating) {
  ShelfWidget* shelf = GetShelfWidget();
  SetState(shelf->shelf_layout_manager(), SHELF_VISIBLE);
  gfx::Rect initial_shelf_bounds = shelf->GetWindowBoundsInScreen();
  gfx::Rect initial_status_bounds =
      shelf->status_area_widget()->GetWindowBoundsInScreen();

  ui::ScopedAnimationDurationScaleMode normal_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  SetState(shelf->shelf_layout_manager(), SHELF_HIDDEN);
  SetState(shelf->shelf_layout_manager(), SHELF_VISIBLE);

  gfx::Rect current_shelf_bounds = shelf->GetWindowBoundsInScreen();
  gfx::Rect current_status_bounds =
      shelf->status_area_widget()->GetWindowBoundsInScreen();

  const int small_change = initial_shelf_bounds.height() / 2;
  EXPECT_LE(
      std::abs(initial_shelf_bounds.height() - current_shelf_bounds.height()),
      small_change);
  EXPECT_LE(
      std::abs(initial_status_bounds.height() - current_status_bounds.height()),
      small_change);
}

// Makes sure the shelf is sized when the status area changes size.
TEST_F(ShelfLayoutManagerTest, ShelfUpdatedWhenStatusAreaChangesSize) {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  ASSERT_TRUE(shelf);
  ShelfWidget* shelf_widget = GetShelfWidget();
  ASSERT_TRUE(shelf_widget);
  ASSERT_TRUE(shelf_widget->status_area_widget());
  shelf_widget->status_area_widget()->SetBounds(
      gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ(200, shelf_widget->GetContentsView()->width() -
            test::ShelfTestAPI(shelf).shelf_view()->width());
}


#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_AutoHide DISABLED_AutoHide
#else
#define MAYBE_AutoHide AutoHide
#endif

// Various assertions around auto-hide.
TEST_F(ShelfLayoutManagerTest, MAYBE_AutoHide) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root, root);
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Maximize();
  widget->Show();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            Shell::GetScreen()->GetDisplayNearestWindow(
                root).work_area().bottom());

  // Move the mouse to the bottom of the screen.
  generator.MoveMouseTo(0, root->bounds().bottom() - 1);

  // Shelf should be shown again (but it shouldn't have changed the work area).
  SetState(shelf, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - shelf->GetIdealBounds().height(),
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            Shell::GetScreen()->GetDisplayNearestWindow(
                root).work_area().bottom());

  // Move mouse back up.
  generator.MoveMouseTo(0, 0);
  SetState(shelf, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  // Drag mouse to bottom of screen.
  generator.PressLeftButton();
  generator.MoveMouseTo(0, root->bounds().bottom() - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  generator.ReleaseLeftButton();
  generator.MoveMouseTo(1, root->bounds().bottom() - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  generator.PressLeftButton();
  generator.MoveMouseTo(1, root->bounds().bottom() - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
}

// Test the behavior of the shelf when it is auto hidden and it is on the
// boundary between the primary and the secondary display.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfOnScreenBoundary) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  DisplayLayout display_layout(DisplayLayout::RIGHT, 0);
  Shell::GetInstance()->display_manager()->SetLayoutForCurrentDisplays(
      display_layout);
  // Put the primary monitor's shelf on the display boundary.
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);

  // Create a window because the shelf is always shown when no windows are
  // visible.
  CreateTestWidget();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(root_windows[0],
            GetShelfWidget()->GetNativeWindow()->GetRootWindow());

  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  int right_edge = root_windows[0]->GetBoundsInScreen().right() - 1;
  int y = root_windows[0]->GetBoundsInScreen().y();

  // Start off the mouse nowhere near the shelf; the shelf should be hidden.
  ui::test::EventGenerator& generator(GetEventGenerator());
  generator.MoveMouseTo(right_edge - 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Moving the mouse over the light bar (but not to the edge of the screen)
  // should show the shelf.
  generator.MoveMouseTo(right_edge - 1, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_EQ(right_edge - 1, Shell::GetScreen()->GetCursorScreenPoint().x());

  // Moving the mouse off the light bar should hide the shelf.
  generator.MoveMouseTo(right_edge - 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Moving the mouse to the right edge of the screen crossing the light bar
  // should show the shelf despite the mouse cursor getting warped to the
  // secondary display.
  generator.MoveMouseTo(right_edge - 1, y);
  generator.MoveMouseTo(right_edge, y);
  UpdateAutoHideStateNow();
  EXPECT_NE(right_edge - 1, Shell::GetScreen()->GetCursorScreenPoint().x());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Hide the shelf.
  generator.MoveMouseTo(right_edge - 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Moving the mouse to the right edge of the screen crossing the light bar and
  // overshooting by a lot should keep the shelf hidden.
  generator.MoveMouseTo(right_edge - 1, y);
  generator.MoveMouseTo(right_edge + 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Moving the mouse to the right edge of the screen crossing the light bar and
  // overshooting a bit should show the shelf.
  generator.MoveMouseTo(right_edge - 1, y);
  generator.MoveMouseTo(right_edge + 2, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Keeping the mouse close to the left edge of the secondary display after the
  // shelf is shown should keep the shelf shown.
  generator.MoveMouseTo(right_edge + 2, y + 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Moving the mouse far from the left edge of the secondary display should
  // hide the shelf.
  generator.MoveMouseTo(right_edge + 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Moving to the left edge of the secondary display without first crossing
  // the primary display's right aligned shelf first should not show the shelf.
  generator.MoveMouseTo(right_edge + 2, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
}

// Assertions around the lock screen showing.
TEST_F(ShelfLayoutManagerTest, VisibleWhenLockScreenShowing) {
  if (!SupportsHostWindowResize())
    return;

  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Maximize();
  widget->Show();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  aura::Window* root = Shell::GetPrimaryRootWindow();
  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  shelf->LayoutShelf();
  EXPECT_EQ(root->bounds().bottom() - ShelfLayoutManager::kAutoHideSize,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  aura::Window* lock_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_LockScreenContainer);

  views::Widget* lock_widget = new views::Widget;
  views::Widget::InitParams lock_params(
      views::Widget::InitParams::TYPE_WINDOW);
  lock_params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  lock_params.parent = lock_container;
  // Widget is now owned by the parent window.
  lock_widget->Init(lock_params);
  lock_widget->Maximize();
  lock_widget->Show();

  // Lock the screen.
  LockScreen();
  // Showing a widget in the lock screen should force the shelf to be visibile.
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  UnlockScreen();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
}

// Assertions around SetAutoHideBehavior.
TEST_F(ShelfLayoutManagerTest, SetAutoHideBehavior) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect display_bounds(
      Shell::GetScreen()->GetDisplayNearestWindow(window).bounds());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  widget->Maximize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window).work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window).work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  ui::ScopedAnimationDurationScaleMode animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  ShelfWidget* shelf_widget = GetShelfWidget();
  EXPECT_TRUE(shelf_widget->status_area_widget()->IsVisible());
  StepWidgetLayerAnimatorToEnd(shelf_widget);
  StepWidgetLayerAnimatorToEnd(shelf_widget->status_area_widget());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  EXPECT_EQ(Shell::GetScreen()->GetDisplayNearestWindow(
                window).work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());
}

// Basic assertions around the dimming of the shelf.
TEST_F(ShelfLayoutManagerTest, DimmingBehavior) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->shelf_widget()->DisableDimmingAnimationsForTest();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect display_bounds(
      Shell::GetScreen()->GetDisplayNearestWindow(window).bounds());

  gfx::Point off_shelf = display_bounds.CenterPoint();
  gfx::Point on_shelf =
      shelf->shelf_widget()->GetWindowBoundsInScreen().CenterPoint();

  // Test there is no dimming object active at this point.
  generator.MoveMouseTo(on_shelf.x(), on_shelf.y());
  EXPECT_EQ(-1, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveMouseTo(off_shelf.x(), off_shelf.y());
  EXPECT_EQ(-1, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // After maximization, the shelf should be visible and the dimmer created.
  widget->Maximize();

  on_shelf = shelf->shelf_widget()->GetWindowBoundsInScreen().CenterPoint();
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Moving the mouse off the shelf should dim the bar.
  generator.MoveMouseTo(off_shelf.x(), off_shelf.y());
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Adding touch events outside the shelf should still keep the shelf in
  // dimmed state.
  generator.PressTouch();
  generator.MoveTouch(off_shelf);
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  // Move the touch into the shelf area should undim.
  generator.MoveTouch(on_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.ReleaseTouch();
  // And a release dims again.
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Moving the mouse on the shelf should undim the bar.
  generator.MoveMouseTo(on_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // No matter what the touch events do, the shelf should stay undimmed.
  generator.PressTouch();
  generator.MoveTouch(off_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(on_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(off_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(on_shelf);
  generator.ReleaseTouch();

  // After restore, the dimming object should be deleted again.
  widget->Restore();
  EXPECT_EQ(-1, shelf->shelf_widget()->GetDimmingAlphaForTest());
}

// Test that dimming works correctly with multiple displays.
TEST_F(ShelfLayoutManagerTest, DimmingBehaviorDualDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  // Create two displays.
  Shell* shell = Shell::GetInstance();
  UpdateDisplay("0+0-200x200,+200+0-100x100");
  EXPECT_EQ(2U, shell->display_manager()->GetNumDisplays());

  DisplayController* display_controller = shell->display_controller();
  aura::Window::Windows root_windows = display_controller->GetAllRootWindows();
  EXPECT_EQ(root_windows.size(), 2U);

  std::vector<ShelfWidget*> shelf_widgets;
  for (auto& root_window : root_windows) {
    ShelfLayoutManager* shelf =
        GetRootWindowController(root_window)->GetShelfLayoutManager();
    shelf_widgets.push_back(shelf->shelf_widget());

    // For disabling the dimming animation to work, the animation must be
    // disabled prior to creating the dimmer.
    shelf_widgets.back()->DisableDimmingAnimationsForTest();

    // Create a maximized window to create the dimmer.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.context = root_window;
    params.bounds = root_window->GetBoundsInScreen();
    params.show_state = ui::SHOW_STATE_MAXIMIZED;
    widget->Init(params);
    widget->Show();
  }

  ui::test::EventGenerator& generator(GetEventGenerator());

  generator.MoveMouseTo(root_windows[0]->GetBoundsInScreen().CenterPoint());
  EXPECT_LT(0, shelf_widgets[0]->GetDimmingAlphaForTest());
  EXPECT_LT(0, shelf_widgets[1]->GetDimmingAlphaForTest());

  generator.MoveMouseTo(
      shelf_widgets[0]->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_EQ(0, shelf_widgets[0]->GetDimmingAlphaForTest());
  EXPECT_LT(0, shelf_widgets[1]->GetDimmingAlphaForTest());

  generator.MoveMouseTo(
      shelf_widgets[1]->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_LT(0, shelf_widgets[0]->GetDimmingAlphaForTest());
  EXPECT_EQ(0, shelf_widgets[1]->GetDimmingAlphaForTest());

  generator.MoveMouseTo(root_windows[1]->GetBoundsInScreen().CenterPoint());
  EXPECT_LT(0, shelf_widgets[0]->GetDimmingAlphaForTest());
  EXPECT_LT(0, shelf_widgets[1]->GetDimmingAlphaForTest());
}

// Assertions around the dimming of the shelf in conjunction with menus.
TEST_F(ShelfLayoutManagerTest, DimmingBehaviorWithMenus) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->shelf_widget()->DisableDimmingAnimationsForTest();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect display_bounds(
      Shell::GetScreen()->GetDisplayNearestWindow(window).bounds());

  // After maximization, the shelf should be visible and the dimmer created.
  widget->Maximize();

  gfx::Point off_shelf = display_bounds.CenterPoint();
  gfx::Point on_shelf =
      shelf->shelf_widget()->GetWindowBoundsInScreen().CenterPoint();

  // Moving the mouse on the shelf should undim the bar.
  generator.MoveMouseTo(on_shelf.x(), on_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Simulate a menu opening.
  shelf->shelf_widget()->ForceUndimming(true);

  // Moving the mouse off the shelf should not dim the bar.
  generator.MoveMouseTo(off_shelf.x(), off_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // No matter what the touch events do, the shelf should stay undimmed.
  generator.PressTouch();
  generator.MoveTouch(off_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(on_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveTouch(off_shelf);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.ReleaseTouch();
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // "Closing the menu" should now turn off the menu since no event is inside
  // the shelf any longer.
  shelf->shelf_widget()->ForceUndimming(false);
  EXPECT_LT(0, shelf->shelf_widget()->GetDimmingAlphaForTest());

  // Moving the mouse again on the shelf which should undim the bar again.
  // This time we check that the bar stays undimmed when the mouse remains on
  // the bar and the "menu gets closed".
  generator.MoveMouseTo(on_shelf.x(), on_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  shelf->shelf_widget()->ForceUndimming(true);
  generator.MoveMouseTo(off_shelf.x(), off_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  generator.MoveMouseTo(on_shelf.x(), on_shelf.y());
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
  shelf->shelf_widget()->ForceUndimming(true);
  EXPECT_EQ(0, shelf->shelf_widget()->GetDimmingAlphaForTest());
}

// Verifies the shelf is visible when status/shelf is focused.
TEST_F(ShelfLayoutManagerTest, VisibleWhenStatusOrShelfFocused) {
  // Since ShelfLayoutManager queries for mouse location, move the mouse so
  // it isn't over the shelf.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  generator.MoveMouseTo(0, 0);

  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  // Widget is now owned by the parent window.
  widget->Init(params);
  widget->Show();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Focus the shelf. Have to go through the focus cycler as normal focus
  // requests to it do nothing.
  GetShelfWidget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  widget->Activate();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Trying to activate the status should fail, since we only allow activating
  // it when the user is using the keyboard (i.e. through FocusCycler).
  GetShelfWidget()->status_area_widget()->Activate();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  GetShelfWidget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
}

// Makes sure shelf will be visible when app list opens as shelf is in
// SHELF_VISIBLE state,and toggling app list won't change shelf
// visibility state.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfVisibleState) {
  Shell* shell = Shell::GetInstance();
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Create a normal unmaximized windowm shelf should be visible.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  // Show app list and the shelf stays visible.
  shell->ShowAppList(NULL);
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  // Hide app list and the shelf stays visible.
  shell->DismissAppList();
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
}

// Makes sure shelf will be shown with SHELF_AUTO_HIDE_SHOWN state
// when app list opens as shelf is in SHELF_AUTO_HIDE state, and
// toggling app list won't change shelf visibility state.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfAutoHideState) {
  Shell* shell = Shell::GetInstance();
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a window and show it in maximized state.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window->Show();
  wm::ActivateWindow(window);

  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  // Show app list.
  shell->ShowAppList(NULL);
  // The shelf's auto hide state won't be changed until the timer fires, so
  // calling shell->UpdateShelfVisibility() is kind of manually helping it to
  // update the state.
  shell->UpdateShelfVisibility();
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Hide app list.
  shell->DismissAppList();
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
}

// Makes sure that when we have dual displays, with one or both shelves are set
// to AutoHide, viewing the AppList on one of them doesn't unhide the other
// hidden shelf.
TEST_F(ShelfLayoutManagerTest, DualDisplayOpenAppListWithShelfAutoHideState) {
  if (!SupportsMultipleDisplays())
    return;

  // Create two displays.
  Shell* shell = Shell::GetInstance();
  DisplayManager* display_manager = shell->display_manager();
  EXPECT_EQ(1U, display_manager->GetNumDisplays());
  UpdateDisplay("0+0-200x200,+200+0-100x100");
  EXPECT_EQ(2U, display_manager->GetNumDisplays());

  DisplayController* display_controller = shell->display_controller();
  aura::Window::Windows root_windows = display_controller->GetAllRootWindows();
  EXPECT_EQ(root_windows.size(), 2U);

  // Get the shelves in both displays and set them to be 'AutoHide'.
  ShelfLayoutManager* shelf_1 =
      GetRootWindowController(root_windows[0])->GetShelfLayoutManager();
  ShelfLayoutManager* shelf_2 =
      GetRootWindowController(root_windows[1])->GetShelfLayoutManager();
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->shelf_widget()->GetNativeWindow()->GetRootWindow(),
            shelf_2->shelf_widget()->GetNativeWindow()->
            GetRootWindow());
  shelf_1->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_1->LayoutShelf();
  shelf_2->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_2->LayoutShelf();

  // Create a window in each display and show them in maximized state.
  aura::Window* window_1 =
      CreateTestWindowInParent(root_windows[0]);
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey,
                              ui::SHOW_STATE_MAXIMIZED);
  window_1->Show();
  aura::Window* window_2 =
      CreateTestWindowInParent(root_windows[1]);
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey,
                                ui::SHOW_STATE_MAXIMIZED);
  window_2->Show();

  EXPECT_EQ(shelf_1->shelf_widget()->GetNativeWindow()->GetRootWindow(),
            window_1->GetRootWindow());
  EXPECT_EQ(shelf_2->shelf_widget()->GetNativeWindow()->GetRootWindow(),
            window_2->GetRootWindow());

  // Activate one window in one display and manually trigger the update of shelf
  // visibility.
  wm::ActivateWindow(window_1);
  shell->UpdateShelfVisibility();

  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->auto_hide_state());

  // Show app list.
  shell->ShowAppList(NULL);
  shell->UpdateShelfVisibility();

  // Only the shelf in the active display should be shown, the other is hidden.
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf_1->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->auto_hide_state());

  // Hide app list, both shelves should be hidden.
  shell->DismissAppList();
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->auto_hide_state());
}

// Makes sure the shelf will be hidden when we have a fullscreen window, and it
// will unhide when we open the app list.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfHiddenState) {
  Shell* shell = Shell::GetInstance();
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  // For shelf to be visible, app list is not open in initial state.
  shelf->LayoutShelf();

  // Create a window and make it full screen.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window->Show();
  wm::ActivateWindow(window);

  // App list and shelf is not shown.
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  // Show app list.
  shell->ShowAppList(NULL);
  EXPECT_TRUE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  // Hide app list.
  shell->DismissAppList();
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
}

// Tests the correct behavior of the shelf when there is a system modal window
// open when we have a single display.
TEST_F(ShelfLayoutManagerTest, ShelfWithSystemModalWindowSingleDisplay) {
  Shell* shell = Shell::GetInstance();
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window->Show();
  wm::ActivateWindow(window);

  // Enable system modal dialog, and make sure shelf is still hidden.
  shell->SimulateModalWindowOpenForTesting(true);
  EXPECT_TRUE(shell->IsSystemModalWindowOpen());
  EXPECT_FALSE(wm::CanActivateWindow(window));
  shell->UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
}

// Tests the correct behavior of the shelf when there is a system modal window
// open when we have dual display.
TEST_F(ShelfLayoutManagerTest, ShelfWithSystemModalWindowDualDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  // Create two displays.
  Shell* shell = Shell::GetInstance();
  DisplayManager* display_manager = shell->display_manager();
  UpdateDisplay("200x200,100x100");
  EXPECT_EQ(2U, display_manager->GetNumDisplays());

  DisplayController* display_controller = shell->display_controller();
  aura::Window::Windows root_windows = display_controller->GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  // Get the shelves in both displays and set them to be 'AutoHide'.
  ShelfLayoutManager* shelf_1 =
      GetRootWindowController(root_windows[0])->GetShelfLayoutManager();
  ShelfLayoutManager* shelf_2 =
      GetRootWindowController(root_windows[1])->GetShelfLayoutManager();
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->shelf_widget()->GetNativeWindow()->GetRootWindow(),
            shelf_2->shelf_widget()->GetNativeWindow()->GetRootWindow());
  shelf_1->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_1->LayoutShelf();
  shelf_2->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_2->LayoutShelf();

  // Create a window in each display and show them in maximized state.
  aura::Window* window_1 = CreateTestWindowInParent(root_windows[0]);
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_1->Show();
  aura::Window* window_2 = CreateTestWindowInParent(root_windows[1]);
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_2->Show();

  EXPECT_EQ(shelf_1->shelf_widget()->GetNativeWindow()->GetRootWindow(),
            window_1->GetRootWindow());
  EXPECT_EQ(shelf_2->shelf_widget()->GetNativeWindow()->GetRootWindow(),
            window_2->GetRootWindow());
  EXPECT_TRUE(window_1->IsVisible());
  EXPECT_TRUE(window_2->IsVisible());

  // Enable system modal dialog, and make sure both shelves are still hidden.
  shell->SimulateModalWindowOpenForTesting(true);
  EXPECT_TRUE(shell->IsSystemModalWindowOpen());
  EXPECT_FALSE(wm::CanActivateWindow(window_1));
  EXPECT_FALSE(wm::CanActivateWindow(window_2));
  shell->UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->auto_hide_state());
}

// Tests that the shelf is only hidden for a fullscreen window at the front and
// toggles visibility when another window is activated.
TEST_F(ShelfLayoutManagerTest, FullscreenWindowInFrontHidesShelf) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();

  // Create a window and make it full screen.
  aura::Window* window1 = CreateTestWindow();
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window1->Show();

  aura::Window* window2 = CreateTestWindow();
  window2->SetBounds(gfx::Rect(0, 0, 100, 100));
  window2->Show();

  wm::GetWindowState(window1)->Activate();
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  wm::GetWindowState(window2)->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  wm::GetWindowState(window1)->Activate();
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
}

// Test the behavior of the shelf when a window on one display is fullscreen
// but the other display has the active window.
TEST_F(ShelfLayoutManagerTest, FullscreenWindowOnSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  Shell::RootWindowControllerList root_window_controllers =
      Shell::GetAllRootWindowControllers();

  // Create windows on either display.
  aura::Window* window1 = CreateTestWindow();
  window1->SetBoundsInScreen(
      gfx::Rect(0, 0, 100, 100),
      display_manager->GetDisplayAt(0));
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window1->Show();

  aura::Window* window2 = CreateTestWindow();
  window2->SetBoundsInScreen(
      gfx::Rect(800, 0, 100, 100),
      display_manager->GetDisplayAt(1));
  window2->Show();

  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  wm::GetWindowState(window2)->Activate();
  EXPECT_EQ(SHELF_HIDDEN,
      root_window_controllers[0]->GetShelfLayoutManager()->visibility_state());
  EXPECT_EQ(SHELF_VISIBLE,
      root_window_controllers[1]->GetShelfLayoutManager()->visibility_state());
}


#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_SetAlignment DISABLED_SetAlignment
#else
#define MAYBE_SetAlignment SetAlignment
#endif

// Tests SHELF_ALIGNMENT_(LEFT, RIGHT, TOP).
TEST_F(ShelfLayoutManagerTest, MAYBE_SetAlignment) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  // Force an initial layout.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  gfx::Rect shelf_bounds(
      GetShelfWidget()->GetWindowBoundsInScreen());
  const gfx::Screen* screen = Shell::GetScreen();
  gfx::Display display =
      screen->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(shelf->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_GE(
      shelf_bounds.width(),
      GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, GetSystemTray()->shelf_alignment());
  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  gfx::Rect status_bounds(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_GE(status_bounds.width(),
            status_area_widget->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(shelf->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_EQ(0, display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(display.bounds().x(), shelf_bounds.x());
  EXPECT_EQ(display.bounds().y(), shelf_bounds.y());
  EXPECT_EQ(display.bounds().height(), shelf_bounds.height());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = screen->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
      display.GetWorkAreaInsets().left());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize, display.work_area().x());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  display = screen->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  display = screen->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(shelf->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_GE(shelf_bounds.width(),
            GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, GetSystemTray()->shelf_alignment());
  status_bounds = gfx::Rect(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_GE(status_bounds.width(),
            status_area_widget->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(shelf->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(display.work_area().right(), shelf_bounds.x());
  EXPECT_EQ(display.bounds().y(), shelf_bounds.y());
  EXPECT_EQ(display.bounds().height(), shelf_bounds.height());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = screen->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
      display.GetWorkAreaInsets().right());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
      display.bounds().right() - display.work_area().right());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->SetAlignment(SHELF_ALIGNMENT_TOP);
  display = screen->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  display = screen->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(shelf->GetIdealBounds().height(),
            display.GetWorkAreaInsets().top());
  EXPECT_GE(shelf_bounds.height(),
            GetShelfWidget()->GetContentsView()->GetPreferredSize().height());
  EXPECT_EQ(SHELF_ALIGNMENT_TOP, GetSystemTray()->shelf_alignment());
  status_bounds = gfx::Rect(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_GE(status_bounds.height(),
            status_area_widget->GetContentsView()->GetPreferredSize().height());
  EXPECT_EQ(shelf->GetIdealBounds().height(),
            display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(display.work_area().y(), shelf_bounds.bottom());
  EXPECT_EQ(display.bounds().x(), shelf_bounds.x());
  EXPECT_EQ(display.bounds().width(), shelf_bounds.width());
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = screen->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
      display.GetWorkAreaInsets().top());
  EXPECT_EQ(ShelfLayoutManager::kAutoHideSize,
            display.work_area().y() - display.bounds().y());
}

TEST_F(ShelfLayoutManagerTest, GestureEdgeSwipe) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  widget->Init(params);
  widget->Show();
  widget->Maximize();

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  aura::Window* window = widget->GetNativeWindow();
  shelf->LayoutShelf();

  gfx::Rect shelf_shown = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Rect bounds_shelf = window->bounds();

  // Edge swipe when SHELF_VISIBLE should not change visibility state.
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
  generator.GestureEdgeSwipe();
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  // Edge swipe when AUTO_HIDE_HIDDEN should change to AUTO_HIDE_SHOWN.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  generator.GestureEdgeSwipe();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  widget->SetFullscreen(true);
  wm::GetWindowState(window)->set_hide_shelf_when_fullscreen(false);
  shelf->UpdateVisibilityState();

  // Edge swipe in fullscreen + AUTO_HIDE_HIDDEN should show the shelf and
  // remain fullscreen.
  EXPECT_TRUE(widget->IsFullscreen());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  generator.GestureEdgeSwipe();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_TRUE(widget->IsFullscreen());
}

// Tests that gesture edge swipe events are forwarded to the right shelf on the
// right monitor (crbug.com/449851).
TEST_F(ShelfLayoutManagerTest, GestureEdgeSwipeMultiMonitor) {
  if (!SupportsMultipleDisplays())
    return;

  // Create two displays.
  Shell* shell = Shell::GetInstance();
  DisplayManager* display_manager = shell->display_manager();
  UpdateDisplay("200x200,100x100");
  ASSERT_EQ(2U, display_manager->GetNumDisplays());

  auto root_window_controllers = Shell::GetAllRootWindowControllers();
  ASSERT_EQ(2U, root_window_controllers.size());
  ShelfLayoutManager* shelf_1 =
      root_window_controllers[0]->GetShelfLayoutManager();
  ShelfLayoutManager* shelf_2 =
      root_window_controllers[1]->GetShelfLayoutManager();

  // Create two maximized windows, one in each display.
  aura::Window* window_1 =
      CreateTestWindowInParent(root_window_controllers[0]->GetRootWindow());
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_1->Show();
  aura::Window* window_2 =
      CreateTestWindowInParent(root_window_controllers[1]->GetRootWindow());
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_2->Show();

  // Make sure both are set to auto-hide and both are hidden.
  shelf_1->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_2->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_1->LayoutShelf();
  shelf_2->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->auto_hide_state());

  ui::test::EventGenerator monitor_1_generator(
      root_window_controllers[0]->GetRootWindow());
  ui::test::EventGenerator monitor_2_generator(
      root_window_controllers[1]->GetRootWindow());

  // An edge swipe in one display should only affect the shelf in that display.
  monitor_1_generator.GestureEdgeSwipe();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf_1->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->auto_hide_state());

  // Back to normal after an update.
  shell->UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->auto_hide_state());

  monitor_2_generator.GestureEdgeSwipe();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->auto_hide_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf_2->auto_hide_state());
}

#if defined(OS_WIN)
// RootWindow and Display can't resize on Windows Ash. http://crbug.com/165962
#define MAYBE_GestureDrag DISABLED_GestureDrag
#else
#define MAYBE_GestureDrag GestureDrag
#endif

TEST_F(ShelfLayoutManagerTest, MAYBE_GestureDrag) {
  // Slop is an implementation detail of gesture recognition, and complicates
  // these tests. Ignore it.
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(0);
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  {
    SCOPED_TRACE("BOTTOM");
    RunGestureDragTests(gfx::Vector2d(0, 120));
  }

  {
    SCOPED_TRACE("LEFT");
    shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
    RunGestureDragTests(gfx::Vector2d(-120, 0));
  }

  {
    SCOPED_TRACE("RIGHT");
    shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
    RunGestureDragTests(gfx::Vector2d(120, 0));
  }
}

TEST_F(ShelfLayoutManagerTest, WindowVisibilityDisablesAutoHide) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a visible window so auto-hide behavior is enforced
  views::Widget* dummy = CreateTestWidget();

  // Window visible => auto hide behaves normally.
  shelf->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Window minimized => auto hide disabled.
  dummy->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Window closed => auto hide disabled.
  dummy->CloseNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Multiple window test
  views::Widget* window1 = CreateTestWidget();
  views::Widget* window2 = CreateTestWidget();

  // both visible => normal autohide
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // either minimzed => normal autohide
  window2->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  window2->Restore();
  window1->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // both minimized => disable auto hide
  window2->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

  // Test moving windows to/from other display.
  window2->Restore();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  // Move to second display.
  window2->SetBounds(gfx::Rect(850, 50, 50, 50));
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  // Move back to primary display.
  window2->SetBounds(gfx::Rect(50, 50, 50, 50));
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
}

// Test that the shelf animates back to its normal position upon a user
// completing a gesture drag.
TEST_F(ShelfLayoutManagerTest, ShelfAnimatesWhenGestureComplete) {
  if (!SupportsHostWindowResize())
    return;

  // Test the shelf animates back to its original visible bounds when it is
  // dragged when there are no visible windows.
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  gfx::Rect visible_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  {
    // Enable animations so that we can make sure that they occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    gfx::Rect shelf_bounds_in_screen =
        GetShelfWidget()->GetWindowBoundsInScreen();
    gfx::Point start(shelf_bounds_in_screen.CenterPoint());
    gfx::Point end(start.x(), shelf_bounds_in_screen.bottom());
    generator.GestureScrollSequence(start, end,
        base::TimeDelta::FromMilliseconds(10), 5);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());

    ShelfAnimationWaiter waiter(visible_bounds);
    // Wait till the animation completes and check that it occurred.
    waiter.WaitTillDoneAnimating();
    EXPECT_TRUE(waiter.WasValidAnimation());
  }

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  // Get the bounds of the shelf when it is hidden.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
  gfx::Rect auto_hidden_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  {
    // Enable the animations so that we can make sure they do occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    gfx::Point start =
        GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
    gfx::Point end(start.x(), start.y() - 100);
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

    // Test that the shelf animates to the visible bounds after a swipe up on
    // the auto hidden shelf.
    generator.GestureScrollSequence(start, end,
        base::TimeDelta::FromMilliseconds(10), 1);
    EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
    ShelfAnimationWaiter waiter1(visible_bounds);
    waiter1.WaitTillDoneAnimating();
    EXPECT_TRUE(waiter1.WasValidAnimation());

    // Test that the shelf animates to the auto hidden bounds after a swipe up
    // on the visible shelf.
    EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());
    generator.GestureScrollSequence(start, end,
        base::TimeDelta::FromMilliseconds(10), 1);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());
    ShelfAnimationWaiter waiter2(auto_hidden_bounds);
    waiter2.WaitTillDoneAnimating();
    EXPECT_TRUE(waiter2.WasValidAnimation());
  }
}

TEST_F(ShelfLayoutManagerTest, ShelfFlickerOnTrayActivation) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  // Turn on auto-hide for the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->auto_hide_state());

  // Show the status menu. That should make the shelf visible again.
  Shell::GetInstance()->accelerator_controller()->PerformActionIfEnabled(
      SHOW_SYSTEM_TRAY_BUBBLE);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->auto_hide_state());
  EXPECT_TRUE(GetSystemTray()->HasSystemBubble());
}

TEST_F(ShelfLayoutManagerTest, WorkAreaChangeWorkspace) {
  // Make sure the shelf is always visible.
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->LayoutShelf();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  views::Widget* widget_one = CreateTestWidgetWithParams(params);
  widget_one->Maximize();

  views::Widget* widget_two = CreateTestWidgetWithParams(params);
  widget_two->Maximize();
  widget_two->Activate();

  // Both windows are maximized. They should be of the same size.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  int area_when_shelf_shown =
      widget_one->GetNativeWindow()->bounds().size().GetArea();

  // Now hide the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Both windows should be resized according to the shelf status.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  // Resized to small.
  EXPECT_LT(area_when_shelf_shown,
            widget_one->GetNativeWindow()->bounds().size().GetArea());

  // Now show the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  // Again both windows should be of the same size.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  EXPECT_EQ(area_when_shelf_shown,
            widget_one->GetNativeWindow()->bounds().size().GetArea());
}

// Confirm that the shelf is dimmed only when content is maximized and
// shelf is not autohidden.
TEST_F(ShelfLayoutManagerTest, Dimming) {
  GetShelfLayoutManager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  scoped_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());

  // Normal window doesn't dim shelf.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  ShelfWidget* shelf = GetShelfWidget();
  EXPECT_FALSE(shelf->GetDimsShelf());

  // Maximized window does.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(shelf->GetDimsShelf());

  // Change back to normal stops dimming.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_FALSE(shelf->GetDimsShelf());

  // Changing back to maximized dims again.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_TRUE(shelf->GetDimsShelf());

  // Changing shelf to autohide stops dimming.
  GetShelfLayoutManager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_FALSE(shelf->GetDimsShelf());
}

// Make sure that the shelf will not hide if the mouse is between a bubble and
// the shelf.
TEST_F(ShelfLayoutManagerTest, BubbleEnlargesShelfMouseHitArea) {
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  StatusAreaWidget* status_area_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->status_area_widget();
  SystemTray* tray = GetSystemTray();

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  shelf->LayoutShelf();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  // Make two iterations - first without a message bubble which should make
  // the shelf disappear and then with a message bubble which should keep it
  // visible.
  for (int i = 0; i < 2; i++) {
    // Make sure the shelf is visible and position the mouse over it. Then
    // allow auto hide.
    shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
    EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
    gfx::Point center =
        status_area_widget->GetWindowBoundsInScreen().CenterPoint();
    generator.MoveMouseTo(center.x(), center.y());
    shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
    EXPECT_TRUE(shelf->IsVisible());
    if (!i) {
      // In our first iteration we make sure there is no bubble.
      tray->CloseSystemBubble();
      EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
    } else {
      // In our second iteration we show a bubble.
      TestItem *item = new TestItem;
      tray->AddTrayItem(item);
      tray->ShowNotificationView(item);
      EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
    }
    // Move the pointer over the edge of the shelf.
    generator.MoveMouseTo(
        center.x(), status_area_widget->GetWindowBoundsInScreen().y() - 8);
    shelf->UpdateVisibilityState();
    if (i) {
      EXPECT_TRUE(shelf->IsVisible());
      EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
    } else {
      EXPECT_FALSE(shelf->IsVisible());
      EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
    }
  }
}

TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColor) {
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  scoped_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  scoped_ptr<aura::Window> w2(CreateTestWindow());
  w2->Show();
  wm::ActivateWindow(w2.get());
  // Overlaps with shelf.
  w2->SetBounds(GetShelfLayoutManager()->GetIdealBounds());

  // Still background is 'maximized'.
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, GetShelfWidget()->GetBackgroundType());
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());
  w1.reset();
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
}

// Verify that the shelf doesn't have the opaque background if it's auto-hide
// status.
TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColorAutoHide) {
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget ()->GetBackgroundType());

  GetShelfLayoutManager()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  scoped_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, GetShelfWidget()->GetBackgroundType());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, GetShelfWidget()->GetBackgroundType());
}

#if defined(OS_CHROMEOS)
#define MAYBE_StatusAreaHitBoxCoversEdge StatusAreaHitBoxCoversEdge
#else
#define MAYBE_StatusAreaHitBoxCoversEdge DISABLED_StatusAreaHitBoxCoversEdge
#endif

// Verify the hit bounds of the status area extend to the edge of the shelf.
TEST_F(ShelfLayoutManagerTest, MAYBE_StatusAreaHitBoxCoversEdge) {
  UpdateDisplay("400x400");
  ShelfLayoutManager* shelf = GetShelfLayoutManager();
  StatusAreaWidget* status_area_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->status_area_widget();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseTo(399,399);

  // Test bottom right pixel for bottom alignment.
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator.ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator.ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());

  // Test bottom right pixel for right alignment.
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator.ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator.ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());

  // Test bottom left pixel for left alignment.
  generator.MoveMouseTo(0, 399);
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator.ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator.ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
}

// Tests that when the auto-hide behaviour is changed during an animation the
// target bounds are updated to reflect the new state.
TEST_F(ShelfLayoutManagerTest,
       ShelfAutoHideToggleDuringAnimationUpdatesBounds) {
  ShelfLayoutManager* shelf_manager = GetShelfLayoutManager();
  aura::Window* status_window = GetShelfWidget()->status_area_widget()->
      GetNativeView();
  gfx::Rect initial_bounds = status_window->bounds();

  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  shelf_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  gfx::Rect hide_target_bounds =  status_window->GetTargetBounds();
  EXPECT_GT(hide_target_bounds.y(), initial_bounds.y());

  shelf_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  gfx::Rect reshow_target_bounds = status_window->GetTargetBounds();
  EXPECT_EQ(initial_bounds, reshow_target_bounds);
}

// Tests that during shutdown, that window activation changes are properly
// handled, and do not crash (crbug.com/458768)
TEST_F(ShelfLayoutManagerTest, ShutdownHandlesWindowActivation) {
  ShelfLayoutManager* shelf_manager = GetShelfLayoutManager();
  ShelfWidget* shelf = GetShelfWidget();
  shelf_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  aura::Window* window1 = CreateTestWindowInShellWithId(0);
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window1->Show();
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(0));
  window2->SetBounds(gfx::Rect(0, 0, 100, 100));
  window2->Show();
  wm::ActivateWindow(window1);

  shelf->ShutdownStatusAreaWidget();
  shelf_manager->PrepareForShutdown();

  // Deleting a focused maximized window will switch focus to |window2|. This
  // would normally cause the ShelfLayoutManager to update its state. However
  // during shutdown we want to handle this without crashing.
  delete window1;
}

TEST_F(ShelfLayoutManagerTest, ShelfLayoutInUnifiedDesktop) {
  if (!SupportsMultipleDisplays())
    return;
  test::DisplayManagerTestApi::EnableUnifiedDesktopForTest();

  UpdateDisplay("500x500, 500x500");

  StatusAreaWidget* status_area_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->status_area_widget();
  EXPECT_TRUE(status_area_widget->IsVisible());
  // Shelf should be in the first display's area.
  EXPECT_EQ("348,453 152x47",
            status_area_widget->GetWindowBoundsInScreen().ToString());
}

}  // namespace ash
