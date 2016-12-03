// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wm_helper_mus.h"

#include "ash/common/display/display_info.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

aura::Window* GetToplevelAuraWindow(ui::Window* window) {
  DCHECK(window);
  // We never create child ui::Window, so window->parent() should be null.
  DCHECK(!window->parent());
  views::Widget* widget = views::NativeWidgetMus::GetWidgetForWindow(window);
  if (!widget)
    return nullptr;
  return widget->GetNativeWindow();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WMHelperMus::EventForwarder:

class WMHelperMus::EventForwarder : public ui::EventHandler {
 public:
  explicit EventForwarder(ui::EventHandlerList* event_handlers)
      : event_handlers_(event_handlers) {}
  ~EventForwarder() override {}

  // Overriden from ui::EventHandler:
  void OnEvent(ui::Event* event) override;

 private:
  const ui::EventHandlerList* event_handlers_;

  DISALLOW_COPY_AND_ASSIGN(EventForwarder);
};

void WMHelperMus::EventForwarder::OnEvent(ui::Event* event) {
  for (ui::EventHandler* handler : *event_handlers_) {
    if (event->stopped_propagation())
      break;
    handler->OnEvent(event);
  }
}
////////////////////////////////////////////////////////////////////////////////
// WMHelperMus, public:

WMHelperMus::WMHelperMus()
    : pre_target_event_forwarder_(new EventForwarder(&pre_target_list_)),
      post_target_event_forwarder_(new EventForwarder(&post_target_list_)),
      active_window_(WMHelperMus::GetActiveWindow()),
      focused_window_(WMHelperMus::GetFocusedWindow()) {
  views::WindowManagerConnection::Get()->client()->AddObserver(this);
  aura::Env::GetInstance()->AddObserver(this);
}

WMHelperMus::~WMHelperMus() {
  views::WindowManagerConnection::Get()->client()->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WMHelperMus, private:

const ash::DisplayInfo WMHelperMus::GetDisplayInfo(int64_t display_id) const {
  // TODO(penghuang): Return real display info when it is supported in mus.
  return ash::DisplayInfo(display_id, "", false);
}

aura::Window* WMHelperMus::GetContainer(int container_id) {
  NOTIMPLEMENTED();
  return nullptr;
}

aura::Window* WMHelperMus::GetActiveWindow() const {
  ui::Window* window =
      views::WindowManagerConnection::Get()->client()->GetFocusedWindow();
  return window ? GetToplevelAuraWindow(window) : nullptr;
}

aura::Window* WMHelperMus::GetFocusedWindow() const {
  aura::Window* active_window = GetActiveWindow();
  if (!active_window)
    return nullptr;
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(active_window);
  return focus_client->GetFocusedWindow();
}

ui::CursorSetType WMHelperMus::GetCursorSet() const {
  NOTIMPLEMENTED();
  return ui::CursorSetType::CURSOR_SET_NORMAL;
}

void WMHelperMus::AddPreTargetHandler(ui::EventHandler* handler) {
  pre_target_list_.push_back(handler);
}

void WMHelperMus::PrependPreTargetHandler(ui::EventHandler* handler) {
  pre_target_list_.insert(pre_target_list_.begin(), handler);
}

void WMHelperMus::RemovePreTargetHandler(ui::EventHandler* handler) {
  auto it =
      std::find(pre_target_list_.begin(), pre_target_list_.end(), handler);
  if (it != pre_target_list_.end())
    pre_target_list_.erase(it);
}

void WMHelperMus::AddPostTargetHandler(ui::EventHandler* handler) {
  post_target_list_.push_back(handler);
}

void WMHelperMus::RemovePostTargetHandler(ui::EventHandler* handler) {
  auto it =
      std::find(post_target_list_.begin(), post_target_list_.end(), handler);
  if (it != post_target_list_.end())
    post_target_list_.erase(it);
}

bool WMHelperMus::IsMaximizeModeWindowManagerEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

void WMHelperMus::OnHostInitialized(aura::WindowTreeHost* host) {
  aura::Window* root_window = host->window();
  root_window->AddPreTargetHandler(pre_target_event_forwarder_.get());
  root_window->AddPostTargetHandler(post_target_event_forwarder_.get());
}

void WMHelperMus::OnWindowTreeFocusChanged(ui::Window* gained_focus,
                                           ui::Window* lost_focus) {
  aura::Window* gained_active =
      gained_focus ? GetToplevelAuraWindow(gained_focus) : nullptr;
  aura::Window* lost_active =
      lost_focus ? GetToplevelAuraWindow(lost_focus) : nullptr;

  // Because NativeWidgetMus uses separate FocusClient for every toplevel
  // window, we have to stop observering the FocusClient of the |lost_active|
  // and start observering the FocusClient of the |gained_active|.
  if (active_window_) {
    aura::client::FocusClient* focus_client =
        aura::client::GetFocusClient(active_window_);
    focus_client->RemoveObserver(this);
  }

  active_window_ = gained_active;
  NotifyWindowActivated(gained_active, lost_active);

  aura::Window* focused_window = nullptr;
  if (active_window_) {
    aura::client::FocusClient* focus_client =
        aura::client::GetFocusClient(active_window_);
    focus_client->AddObserver(this);
    focused_window = focus_client->GetFocusedWindow();
  }

  // OnWindowFocused() will update |focused_window_|.
  OnWindowFocused(focused_window, focused_window_);
}

void WMHelperMus::OnWindowFocused(aura::Window* gained_focus,
                                  aura::Window* lost_focus) {
  if (focused_window_ != gained_focus) {
    focused_window_ = gained_focus;
    NotifyWindowFocused(gained_focus, lost_focus);
  }
}

}  // namespace exo
