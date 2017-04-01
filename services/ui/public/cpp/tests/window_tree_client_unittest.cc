// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/window_tree_client.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "services/ui/common/util.h"
#include "services/ui/public/cpp/input_event_handler.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/tests/test_window.h"
#include "services/ui/public/cpp/tests/test_window_tree.h"
#include "services/ui/public/cpp/tests/window_tree_client_private.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_observer.h"
#include "services/ui/public/cpp/window_private.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tracker.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "services/ui/public/cpp/window_tree_client_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

namespace {

void DoNothingWithEventResult(mojom::EventResult result) {}

Id server_id(ui::Window* window) {
  return WindowPrivate(window).server_id();
}

}  // namespace

std::vector<uint8_t> Int32ToPropertyTransportValue(int32_t value) {
  return mojo::ConvertTo<std::vector<uint8_t>>(value);
}

class TestWindowTreeClientDelegate : public WindowTreeClientDelegate {
 public:
  TestWindowTreeClientDelegate() {}
  ~TestWindowTreeClientDelegate() override {}

  ui::PointerEvent* last_event_observed() { return last_event_observed_.get(); }

  void Reset() { last_event_observed_.reset(); }

  // WindowTreeClientDelegate:
  void OnEmbed(Window* root) override {}
  void OnLostConnection(WindowTreeClient* client) override {}
  void OnEmbedRootDestroyed(Window* root) override {}
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              Window* target) override {
    last_event_observed_.reset(new ui::PointerEvent(event));
  }

 private:
  std::unique_ptr<ui::PointerEvent> last_event_observed_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClientDelegate);
};

class WindowTreeSetup {
 public:
  WindowTreeSetup() : tree_client_(&window_tree_delegate_, nullptr, nullptr) {
    display::Screen::SetScreenInstance(&test_screen_);
    WindowTreeClientPrivate(&tree_client_).OnEmbed(&window_tree_);
    window_tree_.GetAndClearChangeId(nullptr);
  }

  ~WindowTreeSetup() { display::Screen::SetScreenInstance(nullptr); }

  WindowTreeClient* client() {
    return &tree_client_;
  }

  mojom::WindowTreeClient* window_tree_client() {
    return static_cast<mojom::WindowTreeClient*>(&tree_client_);
  }

  TestWindowTree* window_tree() { return &window_tree_; }

  TestWindowTreeClientDelegate* window_tree_delegate() {
    return &window_tree_delegate_;
  }

  Window* GetFirstRoot() {
    return client()->GetRoots().empty() ? nullptr
                                        : *client()->GetRoots().begin();
  }

 private:
  TestWindowTree window_tree_;
  TestWindowTreeClientDelegate window_tree_delegate_;
  WindowTreeClient tree_client_;

  // Dummy screen required to be the screen instance.
  display::test::TestScreen test_screen_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeSetup);
};

class TestInputEventHandler : public InputEventHandler {
 public:
  TestInputEventHandler()
      : received_event_(false), should_manually_ack_(false) {}
  ~TestInputEventHandler() override {}

  void set_should_manually_ack() { should_manually_ack_ = true; }

  void AckEvent() {
    DCHECK(should_manually_ack_);
    DCHECK(!ack_callback_.is_null());
    ack_callback_.Run(mojom::EventResult::HANDLED);
    ack_callback_ = base::Bind(&DoNothingWithEventResult);
  }

  void Reset() {
    received_event_ = false;
    ack_callback_ = base::Bind(&DoNothingWithEventResult);
  }
  bool received_event() const { return received_event_; }

 private:
  // InputEventHandler:
  void OnWindowInputEvent(
      Window* target,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(mojom::EventResult)>>* ack_callback)
      override {
    EXPECT_FALSE(received_event_)
        << "Observer was not reset after receiving event.";
    received_event_ = true;
    if (should_manually_ack_) {
      ack_callback_ = *ack_callback->get();
      ack_callback->reset();
    }
  }

  bool received_event_;
  bool should_manually_ack_;
  base::Callback<void(mojom::EventResult)> ack_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestInputEventHandler);
};

using WindowTreeClientTest = testing::Test;

// Verifies bounds are reverted if the server replied that the change failed.
TEST_F(WindowTreeClientTest, SetBoundsFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const gfx::Rect original_bounds(root->bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, root->bounds());
  root->SetBounds(new_bounds);
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(original_bounds, root->bounds());
}

// Simulates a bounds change, and while the bounds change is in flight the
// server replies with a new bounds and the original bounds change fails.
TEST_F(WindowTreeClientTest, SetBoundsFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const gfx::Rect original_bounds(root->bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, root->bounds());
  root->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, root->bounds());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with a bounds change.
  const gfx::Rect server_changed_bounds(gfx::Rect(0, 0, 101, 102));
  setup.window_tree_client()->OnWindowBoundsChanged(
      server_id(root), original_bounds, server_changed_bounds);

  // This shouldn't trigger the bounds changing yet.
  EXPECT_EQ(new_bounds, root->bounds());

  // Tell the client the change failed, which should trigger failing to the
  // most recent bounds from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(server_changed_bounds, root->bounds());

  // Simulate server changing back to original bounds. Should take immediately.
  setup.window_tree_client()->OnWindowBoundsChanged(
      server_id(root), server_changed_bounds, original_bounds);
  EXPECT_EQ(original_bounds, root->bounds());
}

TEST_F(WindowTreeClientTest, TwoInFlightBoundsChangesBothCanceled) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const gfx::Rect original_bounds(root->bounds());
  const gfx::Rect bounds1(gfx::Rect(0, 0, 100, 100));
  const gfx::Rect bounds2(gfx::Rect(0, 0, 100, 102));
  root->SetBounds(bounds1);
  EXPECT_EQ(bounds1, root->bounds());
  uint32_t change_id1;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id1));

  root->SetBounds(bounds2);
  EXPECT_EQ(bounds2, root->bounds());
  uint32_t change_id2;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id2));

  // Tell the client change 1 failed. As there is a still a change in flight
  // nothing should happen.
  setup.window_tree_client()->OnChangeCompleted(change_id1, false);
  EXPECT_EQ(bounds2, root->bounds());

  // And tell the client change 2 failed too. Should now fallback to original
  // bounds.
  setup.window_tree_client()->OnChangeCompleted(change_id2, false);
  EXPECT_EQ(original_bounds, root->bounds());
}

// Verifies properties are reverted if the server replied that the change
// failed.
TEST_F(WindowTreeClientTest, SetPropertyFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  ASSERT_FALSE(root->HasSharedProperty("foo"));
  const int32_t new_value = 11;
  root->SetSharedProperty("foo", new_value);
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(new_value, root->GetSharedProperty<int32_t>("foo"));
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_FALSE(root->HasSharedProperty("foo"));
}

// Simulates a property change, and while the property change is in flight the
// server replies with a new property and the original property change fails.
TEST_F(WindowTreeClientTest, SetPropertyFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const int32_t value1 = 11;
  root->SetSharedProperty("foo", value1);
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(value1, root->GetSharedProperty<int32_t>("foo"));
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with a different value.
  const int32_t server_value = 12;
  setup.window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root), "foo", Int32ToPropertyTransportValue(server_value));

  // This shouldn't trigger the property changing yet.
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(value1, root->GetSharedProperty<int32_t>("foo"));

  // Tell the client the change failed, which should trigger failing to the
  // most recent value from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(server_value, root->GetSharedProperty<int32_t>("foo"));

  // Simulate server changing back to value1. Should take immediately.
  setup.window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root), "foo", Int32ToPropertyTransportValue(value1));
  ASSERT_TRUE(root->HasSharedProperty("foo"));
  EXPECT_EQ(value1, root->GetSharedProperty<int32_t>("foo"));
}

// Verifies visible is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientTest, SetVisibleFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const bool original_visible = root->visible();
  const bool new_visible = !original_visible;
  ASSERT_NE(new_visible, root->visible());
  root->SetVisible(new_visible);
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(original_visible, root->visible());
}

// Simulates a visible change, and while the visible change is in flight the
// server replies with a new visible and the original visible change fails.
TEST_F(WindowTreeClientTest, SetVisibleFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const bool original_visible = root->visible();
  const bool new_visible = !original_visible;
  ASSERT_NE(new_visible, root->visible());
  root->SetVisible(new_visible);
  EXPECT_EQ(new_visible, root->visible());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with a visible change.
  const bool server_changed_visible = !new_visible;
  setup.window_tree_client()->OnWindowVisibilityChanged(server_id(root),
                                                        server_changed_visible);

  // This shouldn't trigger visible changing yet.
  EXPECT_EQ(new_visible, root->visible());

  // Tell the client the change failed, which should trigger failing to the
  // most recent visible from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(server_changed_visible, root->visible());

  // Simulate server changing back to original visible. Should take immediately.
  setup.window_tree_client()->OnWindowVisibilityChanged(server_id(root),
                                                        original_visible);
  EXPECT_EQ(original_visible, root->visible());
}

// Verifies that local opacity is not changed if the server replied that the
// change succeeded.
TEST_F(WindowTreeClientTest, SetOpacitySucceeds) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const float original_opacity = root->opacity();
  const float new_opacity = 0.5f;
  ASSERT_NE(new_opacity, original_opacity);
  ASSERT_NE(new_opacity, root->opacity());
  root->SetOpacity(new_opacity);
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, true);
  EXPECT_EQ(new_opacity, root->opacity());
}

// Verifies that opacity is reverted if the server replied that the change
// failed.
TEST_F(WindowTreeClientTest, SetOpacityFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const float original_opacity = root->opacity();
  const float new_opacity = 0.5f;
  ASSERT_NE(new_opacity, root->opacity());
  root->SetOpacity(new_opacity);
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(original_opacity, root->opacity());
}

// Simulates the server changing the opacitry while there is an opacity change
// in flight, causing the requested change to fail.
TEST_F(WindowTreeClientTest, SetOpacityFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const float original_opacity = root->opacity();
  const float new_opacity = 0.5f;
  ASSERT_NE(new_opacity, root->opacity());
  root->SetOpacity(new_opacity);
  EXPECT_EQ(new_opacity, root->opacity());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with an opacity change.
  const float server_changed_opacity = 0.75f;
  setup.window_tree_client()->OnWindowOpacityChanged(
      server_id(root), original_opacity, server_changed_opacity);

  // This shouldn't trigger opacity changing yet.
  EXPECT_EQ(new_opacity, root->opacity());

  // Tell the client the change failed, which should trigger failing to the
  // most recent opacity from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(server_changed_opacity, root->opacity());

  // Simulate server changing back to original opacity. Should take immediately.
  setup.window_tree_client()->OnWindowOpacityChanged(
      server_id(root), server_changed_opacity, original_opacity);
  EXPECT_EQ(original_opacity, root->opacity());
}

// Tests that when there are multiple changes in flight, that failing changes
// update the revert state of subsequent changes.
TEST_F(WindowTreeClientTest, SetOpacityFailedWithMultiplePendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  const float original_opacity = root->opacity();
  const float new_opacity = 0.5f;
  ASSERT_NE(new_opacity, root->opacity());
  root->SetOpacity(new_opacity);
  uint32_t change_id1;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id1));

  const float second_new_opacity = 0.75f;
  ASSERT_NE(second_new_opacity, root->opacity());
  root->SetOpacity(second_new_opacity);
  uint32_t change_id2;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id2));

  // Canceling the first one, while there is another in flight, should not
  // change the local opacity.
  setup.window_tree_client()->OnChangeCompleted(change_id1, false);
  EXPECT_EQ(second_new_opacity, root->opacity());

  // The previous cancelation should have updated the revert value of the in
  // flight change.
  setup.window_tree_client()->OnChangeCompleted(change_id2, false);
  EXPECT_EQ(original_opacity, root->opacity());
}

// Verifies |is_modal| is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientTest, SetModalFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  EXPECT_FALSE(root->is_modal());
  root->SetModal();
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  EXPECT_TRUE(root->is_modal());
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_FALSE(root->is_modal());
}

TEST_F(WindowTreeClientTest, InputEventBasic) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);

  TestInputEventHandler event_handler;
  root->set_input_event_handler(&event_handler);

  std::unique_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_NONE, 0));
  setup.window_tree_client()->OnWindowInputEvent(
      1, server_id(root), ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(event_handler.received_event());
  EXPECT_TRUE(setup.window_tree()->WasEventAcked(1));
  event_handler.Reset();

  event_handler.set_should_manually_ack();
  setup.window_tree_client()->OnWindowInputEvent(
      33, server_id(root), ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(event_handler.received_event());
  EXPECT_FALSE(setup.window_tree()->WasEventAcked(33));

  event_handler.AckEvent();
  EXPECT_TRUE(setup.window_tree()->WasEventAcked(33));
}

// Tests pointer watchers triggered by events that did not hit a target in this
// window tree.
TEST_F(WindowTreeClientTest, OnPointerEventObserved) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);

  // Start a pointer watcher for all events excluding move events.
  setup.client()->StartPointerWatcher(false /* want_moves */);

  // Simulate the server sending an observed event.
  std::unique_ptr<ui::PointerEvent> pointer_event_down(new ui::PointerEvent(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), ui::EF_CONTROL_DOWN, 1,
      0, ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks()));
  setup.window_tree_client()->OnPointerEventObserved(
      std::move(pointer_event_down), 0u);

  // Delegate sensed the event.
  ui::Event* last_event = setup.window_tree_delegate()->last_event_observed();
  EXPECT_EQ(ui::ET_POINTER_DOWN, last_event->type());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, last_event->flags());
  setup.window_tree_delegate()->Reset();

  // Stop the pointer watcher.
  setup.client()->StopPointerWatcher();

  // Simulate another event from the server.
  std::unique_ptr<ui::PointerEvent> pointer_event_up(new ui::PointerEvent(
      ui::ET_POINTER_UP, gfx::Point(), gfx::Point(), ui::EF_CONTROL_DOWN, 1, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks()));
  setup.window_tree_client()->OnPointerEventObserved(
      std::move(pointer_event_up), 0u);

  // No event was sensed.
  EXPECT_FALSE(setup.window_tree_delegate()->last_event_observed());
}

// Tests pointer watchers triggered by events that hit this window tree.
TEST_F(WindowTreeClientTest, OnWindowInputEventWithPointerWatcher) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);

  // Start a pointer watcher for all events excluding move events.
  setup.client()->StartPointerWatcher(false /* want_moves */);

  // Simulate the server dispatching an event that also matched the observer.
  std::unique_ptr<ui::PointerEvent> pointer_event_down(new ui::PointerEvent(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), ui::EF_CONTROL_DOWN, 1,
      0, ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks()));
  setup.window_tree_client()->OnWindowInputEvent(
      1, server_id(root), std::move(pointer_event_down), true);

  // Delegate sensed the event.
  ui::Event* last_event = setup.window_tree_delegate()->last_event_observed();
  EXPECT_EQ(ui::ET_POINTER_DOWN, last_event->type());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, last_event->flags());
}

// Verifies focus is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientTest, SetFocusFailed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  root->SetVisible(true);
  Window* child = setup.client()->NewWindow();
  child->SetVisible(true);
  root->AddChild(child);
  Window* original_focus = setup.client()->GetFocusedWindow();
  Window* new_focus = child;
  ASSERT_NE(new_focus, original_focus);
  new_focus->SetFocus();
  ASSERT_TRUE(new_focus->HasFocus());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_EQ(original_focus, setup.client()->GetFocusedWindow());
}

// Simulates a focus change, and while the focus change is in flight the server
// replies with a new focus and the original focus change fails.
TEST_F(WindowTreeClientTest, SetFocusFailedWithPendingChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  root->SetVisible(true);
  Window* child1 = setup.client()->NewWindow();
  child1->SetVisible(true);
  root->AddChild(child1);
  Window* child2 = setup.client()->NewWindow();
  child2->SetVisible(true);
  root->AddChild(child2);
  Window* original_focus = setup.client()->GetFocusedWindow();
  Window* new_focus = child1;
  ASSERT_NE(new_focus, original_focus);
  new_focus->SetFocus();
  ASSERT_TRUE(new_focus->HasFocus());
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Simulate the server responding with a focus change.
  setup.window_tree_client()->OnWindowFocused(server_id(child2));

  // This shouldn't trigger focus changing yet.
  EXPECT_TRUE(child1->HasFocus());

  // Tell the client the change failed, which should trigger failing to the
  // most recent focus from server.
  setup.window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_FALSE(child1->HasFocus());
  EXPECT_TRUE(child2->HasFocus());
  EXPECT_EQ(child2, setup.client()->GetFocusedWindow());

  // Simulate server changing focus to child1. Should take immediately.
  setup.window_tree_client()->OnWindowFocused(server_id(child1));
  EXPECT_TRUE(child1->HasFocus());
}

TEST_F(WindowTreeClientTest, FocusOnRemovedWindowWithInFlightFocusChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  root->SetVisible(true);
  Window* child1 = setup.client()->NewWindow();
  child1->SetVisible(true);
  root->AddChild(child1);
  Window* child2 = setup.client()->NewWindow();
  child2->SetVisible(true);
  root->AddChild(child2);

  child1->SetFocus();
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));

  // Destroy child1, which should set focus to null.
  child1->Destroy();
  EXPECT_EQ(nullptr, setup.client()->GetFocusedWindow());

  // Server changes focus to 2.
  setup.window_tree_client()->OnWindowFocused(server_id(child2));
  // Shouldn't take immediately.
  EXPECT_FALSE(child2->HasFocus());

  // Ack the change, focus should still be null.
  setup.window_tree_client()->OnChangeCompleted(change_id, true);
  EXPECT_EQ(nullptr, setup.client()->GetFocusedWindow());

  // Change to 2 again, this time it should take.
  setup.window_tree_client()->OnWindowFocused(server_id(child2));
  EXPECT_TRUE(child2->HasFocus());
}

class ToggleVisibilityFromDestroyedObserver : public WindowObserver {
 public:
  explicit ToggleVisibilityFromDestroyedObserver(Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }

  ToggleVisibilityFromDestroyedObserver() { EXPECT_FALSE(window_); }

  // WindowObserver:
  void OnWindowDestroyed(Window* window) override {
    window_->SetVisible(!window->visible());
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ToggleVisibilityFromDestroyedObserver);
};

TEST_F(WindowTreeClientTest, ToggleVisibilityFromWindowDestroyed) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);
  Window* child1 = setup.client()->NewWindow();
  root->AddChild(child1);
  ToggleVisibilityFromDestroyedObserver toggler(child1);
  // Destroying the window triggers
  // ToggleVisibilityFromDestroyedObserver::OnWindowDestroyed(), which toggles
  // the visibility of the window. Ack the change, which should not crash or
  // trigger DCHECKs.
  child1->Destroy();
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, true);
}

TEST_F(WindowTreeClientTest, NewTopLevelWindow) {
  WindowTreeSetup setup;
  Window* root1 = setup.GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup.client()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);
  EXPECT_TRUE(WindowPrivate(root2).parent_drawn());
  ASSERT_NE(root2, root1);
  EXPECT_NE(server_id(root2), server_id(root1));
  EXPECT_EQ(2u, setup.client()->GetRoots().size());
  EXPECT_TRUE(setup.client()->GetRoots().count(root1) > 0u);
  EXPECT_TRUE(setup.client()->GetRoots().count(root2) > 0u);

  // Ack the request to the windowtree to create the new window.
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  EXPECT_EQ(setup.window_tree()->window_id(), server_id(root2));

  mojom::WindowDataPtr data = mojom::WindowData::New();
  data->window_id = server_id(root2);
  const int64_t display_id = 1;
  setup.window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                                display_id, false);

  EXPECT_FALSE(WindowPrivate(root2).parent_drawn());

  // Should not be able to add a top level as a child of another window.
  root1->AddChild(root2);
  ASSERT_EQ(nullptr, root2->parent());

  // Destroy the first root, shouldn't initiate tear down.
  root1->Destroy();
  root1 = nullptr;
  EXPECT_EQ(1u, setup.client()->GetRoots().size());
  EXPECT_TRUE(setup.client()->GetRoots().count(root2) > 0u);
}

TEST_F(WindowTreeClientTest, NewTopLevelWindowGetsPropertiesFromData) {
  WindowTreeSetup setup;
  Window* root1 = setup.GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup.client()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);

  EXPECT_FALSE(root2->IsDrawn());
  EXPECT_FALSE(root2->visible());

  // Ack the request to the windowtree to create the new window.
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  EXPECT_EQ(setup.window_tree()->window_id(), server_id(root2));

  mojom::WindowDataPtr data = mojom::WindowData::New();
  data->window_id = server_id(root2);
  data->bounds.SetRect(1, 2, 3, 4);
  data->visible = true;
  const int64_t display_id = 1;
  setup.window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                                display_id, true);

  // Make sure all the properties took.
  EXPECT_TRUE(root2->IsDrawn());
  EXPECT_TRUE(root2->visible());
  EXPECT_EQ(1, root2->display_id());
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), root2->bounds());
}

TEST_F(WindowTreeClientTest, NewTopLevelWindowGetsAllChangesInFlight) {
  WindowTreeSetup setup;
  Window* root1 = setup.GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup.client()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);

  EXPECT_FALSE(root2->IsDrawn());
  EXPECT_FALSE(root2->visible());

  // Get the id of the in flight change for creating the new window.
  uint32_t new_window_in_flight_change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(
      &new_window_in_flight_change_id));
  EXPECT_EQ(setup.window_tree()->window_id(), server_id(root2));

  // Make visibility go from false->true->false. Don't ack immediately.
  root2->SetVisible(true);
  uint32_t vis_in_flight_change_id1;
  ASSERT_TRUE(
      setup.window_tree()->GetAndClearChangeId(&vis_in_flight_change_id1));
  EXPECT_NE(new_window_in_flight_change_id, vis_in_flight_change_id1);
  root2->SetVisible(false);
  uint32_t vis_in_flight_change_id2;
  ASSERT_TRUE(
      setup.window_tree()->GetAndClearChangeId(&vis_in_flight_change_id2));
  EXPECT_NE(vis_in_flight_change_id1, vis_in_flight_change_id2);

  // Change bounds to 5, 6, 7, 8.
  root2->SetBounds(gfx::Rect(5, 6, 7, 8));
  uint32_t bounds_in_flight_change_id;
  ASSERT_TRUE(
      setup.window_tree()->GetAndClearChangeId(&bounds_in_flight_change_id));
  EXPECT_NE(vis_in_flight_change_id2, bounds_in_flight_change_id);

  root2->SetSharedProperty<std::string>("xx", "client_xx");
  uint32_t property_in_flight_change_id;
  ASSERT_TRUE(
      setup.window_tree()->GetAndClearChangeId(&property_in_flight_change_id));
  EXPECT_NE(bounds_in_flight_change_id, property_in_flight_change_id);

  // Ack the new window top level window. Vis and bounds shouldn't change.
  mojom::WindowDataPtr data = mojom::WindowData::New();
  data->window_id = server_id(root2);
  data->bounds.SetRect(1, 2, 3, 4);
  data->visible = true;
  constexpr char kXxName[] = "server_xx";
  data->properties["xx"] =
      std::vector<uint8_t>(kXxName, kXxName + strlen(kXxName));
  constexpr char kYyName[] = "server_yy";
  data->properties["yy"] =
      std::vector<uint8_t>(kYyName, kYyName + strlen(kYyName));
  const int64_t display_id = 1;
  setup.window_tree_client()->OnTopLevelCreated(
      new_window_in_flight_change_id, std::move(data), display_id, true);

  // The only value that should take effect is the property for 'yy' as it was
  // not in flight.
  EXPECT_TRUE(WindowPrivate(root2).parent_drawn());
  EXPECT_FALSE(root2->visible());
  EXPECT_EQ(1, root2->display_id());
  EXPECT_EQ(gfx::Rect(5, 6, 7, 8), root2->bounds());
  EXPECT_EQ(2u, root2->shared_properties().size());
  ASSERT_TRUE(root2->HasSharedProperty("yy"));
  EXPECT_EQ("server_yy", root2->GetSharedProperty<std::string>("yy"));
  ASSERT_TRUE(root2->HasSharedProperty("xx"));
  EXPECT_EQ("client_xx", root2->GetSharedProperty<std::string>("xx"));

  // Tell the client the changes failed. This should cause the values to change
  // to that of the server.
  setup.window_tree_client()->OnChangeCompleted(vis_in_flight_change_id1,
                                                false);
  EXPECT_FALSE(root2->visible());
  setup.window_tree_client()->OnChangeCompleted(vis_in_flight_change_id2,
                                                false);
  EXPECT_TRUE(root2->visible());
  setup.window_tree_client()->OnChangeCompleted(bounds_in_flight_change_id,
                                                false);
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), root2->bounds());
  setup.window_tree_client()->OnChangeCompleted(property_in_flight_change_id,
                                                false);
  EXPECT_EQ(2u, root2->shared_properties().size());
  ASSERT_TRUE(root2->HasSharedProperty("yy"));
  EXPECT_EQ("server_yy", root2->GetSharedProperty<std::string>("yy"));
  ASSERT_TRUE(root2->HasSharedProperty("xx"));
  EXPECT_EQ("server_xx", root2->GetSharedProperty<std::string>("xx"));
}

// Tests that if the client has multiple unowned windows, and one of them is a
// transient child to another, the  teardown can happen cleanly.
TEST_F(WindowTreeClientTest, MultipleUnOwnedWindowsDuringDestruction) {
  std::unique_ptr<WindowTreeSetup> setup(new WindowTreeSetup());
  Window* root1 = setup->GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup->client()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);
  root1->AddTransientWindow(root2);

  WindowTracker tracker;
  tracker.Add(root1);
  tracker.Add(root2);
  setup.reset();
  EXPECT_TRUE(tracker.windows().empty());
}

TEST_F(WindowTreeClientTest, TopLevelWindowDestroyedBeforeCreateComplete) {
  WindowTreeSetup setup;
  Window* root1 = setup.GetFirstRoot();
  ASSERT_TRUE(root1);
  Window* root2 = setup.client()->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(root2);
  ASSERT_EQ(2u, setup.client()->GetRoots().size());

  // Get the id of the in flight change for creating the new window.
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  EXPECT_EQ(setup.window_tree()->window_id(), server_id(root2));

  mojom::WindowDataPtr data = mojom::WindowData::New();
  data->window_id = server_id(root2);

  // Destroy the window before the server has a chance to ack the window
  // creation.
  root2->Destroy();
  EXPECT_EQ(1u, setup.client()->GetRoots().size());

  const int64_t display_id = 1;
  setup.window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                                display_id, true);
  EXPECT_EQ(1u, setup.client()->GetRoots().size());
}

// Tests both SetCapture and ReleaseCapture, to ensure that Window is properly
// updated on failures.
TEST_F(WindowTreeClientTest, ExplicitCapture) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);

  root->SetCapture();
  EXPECT_TRUE(root->HasCapture());
  uint32_t change_id1;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id1));
  setup.window_tree_client()->OnChangeCompleted(change_id1, false);
  EXPECT_FALSE(root->HasCapture());

  root->SetCapture();
  EXPECT_TRUE(root->HasCapture());
  uint32_t change_id2;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id2));
  setup.window_tree_client()->OnChangeCompleted(change_id2, true);
  EXPECT_TRUE(root->HasCapture());

  root->ReleaseCapture();
  EXPECT_FALSE(root->HasCapture());
  uint32_t change_id3;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id3));
  setup.window_tree_client()->OnChangeCompleted(change_id3, false);
  EXPECT_TRUE(root->HasCapture());

  root->ReleaseCapture();
  uint32_t change_id4;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id4));
  setup.window_tree_client()->OnChangeCompleted(change_id4, true);
  EXPECT_FALSE(root->HasCapture());
}

// Tests that when capture is lost, that the window tree updates properly.
TEST_F(WindowTreeClientTest, LostCapture) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);

  root->SetCapture();
  EXPECT_TRUE(root->HasCapture());
  uint32_t change_id1;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id1));
  setup.window_tree_client()->OnChangeCompleted(change_id1, true);
  EXPECT_TRUE(root->HasCapture());

  // The second SetCapture should be ignored.
  root->SetCapture();
  uint32_t change_id2;
  ASSERT_FALSE(setup.window_tree()->GetAndClearChangeId(&change_id2));

  setup.window_tree_client()->OnCaptureChanged(0, server_id(root));
  EXPECT_FALSE(root->HasCapture());
}

// Tests that when capture is lost, while there is a release capture request
// inflight, that the revert value of that request is updated correctly.
TEST_F(WindowTreeClientTest, LostCaptureDifferentInFlightChange) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  ASSERT_TRUE(root);

  root->SetCapture();
  EXPECT_TRUE(root->HasCapture());
  uint32_t change_id1;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id1));
  setup.window_tree_client()->OnChangeCompleted(change_id1, true);
  EXPECT_TRUE(root->HasCapture());

  // The ReleaseCapture should be updated to the revert of the SetCapture.
  root->ReleaseCapture();
  uint32_t change_id2;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id2));

  setup.window_tree_client()->OnCaptureChanged(0, server_id(root));
  EXPECT_FALSE(root->HasCapture());

  setup.window_tree_client()->OnChangeCompleted(change_id2, false);
  EXPECT_FALSE(root->HasCapture());
}

// Tests that while two windows can inflight capture requests, that the
// WindowTreeClient only identifies one as having the current capture.
TEST_F(WindowTreeClientTest, TwoWindowsRequestCapture) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  Window* child = setup.client()->NewWindow();
  child->SetVisible(true);
  root->AddChild(child);

  root->SetCapture();
  EXPECT_TRUE(root->HasCapture());
  uint32_t change_id1;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id1));

  child->SetCapture();
  EXPECT_TRUE(child->HasCapture());
  EXPECT_FALSE(root->HasCapture());

  uint32_t change_id2;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id2));

  setup.window_tree_client()->OnChangeCompleted(change_id1, true);
  EXPECT_FALSE(root->HasCapture());
  EXPECT_TRUE(child->HasCapture());

  setup.window_tree_client()->OnChangeCompleted(change_id2, false);
  EXPECT_FALSE(child->HasCapture());
  EXPECT_TRUE(root->HasCapture());

  setup.window_tree_client()->OnCaptureChanged(0, server_id(root));
  EXPECT_FALSE(root->HasCapture());
}

TEST_F(WindowTreeClientTest, WindowDestroyedWhileTransientChildHasCapture) {
  WindowTreeSetup setup;
  Window* root = setup.GetFirstRoot();
  Window* transient_parent = setup.client()->NewWindow();
  Window* transient_child = setup.client()->NewWindow();
  transient_parent->SetVisible(true);
  transient_child->SetVisible(true);
  root->AddChild(transient_parent);
  root->AddChild(transient_child);

  transient_parent->AddTransientWindow(transient_child);

  WindowTracker tracker;
  tracker.Add(transient_parent);
  tracker.Add(transient_child);
  // Request a capture on the transient child, then destroy the transient
  // parent. That will destroy both windows, and should reset the capture window
  // correctly.
  transient_child->SetCapture();
  transient_parent->Destroy();
  EXPECT_TRUE(tracker.windows().empty());

  // Create a new Window, and attempt to place capture on that.
  Window* child = setup.client()->NewWindow();
  child->SetVisible(true);
  root->AddChild(child);
  child->SetCapture();
  EXPECT_TRUE(child->HasCapture());
}

namespace {

class CaptureRecorder : public WindowTreeClientObserver {
 public:
  explicit CaptureRecorder(WindowTreeClient* tree_client)
      : tree_client_(tree_client) {
    tree_client_->AddObserver(this);
  }

  ~CaptureRecorder() override { tree_client_->RemoveObserver(this); }

  void reset_capture_captured_count() { capture_changed_count_ = 0; }
  int capture_changed_count() const { return capture_changed_count_; }
  int last_gained_capture_window_id() const {
    return last_gained_capture_window_id_;
  }
  int last_lost_capture_window_id() const {
    return last_lost_capture_window_id_;
  }

  // WindowTreeClientObserver:
  void OnWindowTreeCaptureChanged(Window* gained_capture,
                                  Window* lost_capture) override {
    capture_changed_count_++;
    last_gained_capture_window_id_ =
        gained_capture ? gained_capture->local_id() : 0;
    last_lost_capture_window_id_ = lost_capture ? lost_capture->local_id() : 0;
  }

 private:
  WindowTreeClient* tree_client_;
  int capture_changed_count_ = 0;
  int last_gained_capture_window_id_ = 0;
  int last_lost_capture_window_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CaptureRecorder);
};

}  // namespace

TEST_F(WindowTreeClientTest, OnWindowTreeCaptureChanged) {
  WindowTreeSetup setup;
  CaptureRecorder capture_recorder(setup.client());
  Window* root = setup.GetFirstRoot();
  Window* child1 = setup.client()->NewWindow();
  const int child1_id = 1;
  child1->set_local_id(child1_id);
  child1->SetVisible(true);
  root->AddChild(child1);
  Window* child2 = setup.client()->NewWindow();
  const int child2_id = 2;
  child2->set_local_id(child2_id);
  child2->SetVisible(true);
  root->AddChild(child2);

  EXPECT_EQ(0, capture_recorder.capture_changed_count());
  // Give capture to child1 and ensure everyone is notified correctly.
  child1->SetCapture();
  uint32_t change_id;
  ASSERT_TRUE(setup.window_tree()->GetAndClearChangeId(&change_id));
  setup.window_tree_client()->OnChangeCompleted(change_id, true);
  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(child1_id, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();

  // Deleting a window with capture should notify observers as well.
  child1->Destroy();
  child1 = nullptr;
  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(0, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(child1_id, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();

  // Changes originating from server should notify observers too.
  WindowTreeClientPrivate(setup.client()).CallOnCaptureChanged(child2, nullptr);
  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(child2_id, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();
}

}  // namespace ui
