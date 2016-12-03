// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/root_window_controller.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ash/common/root_window_controller_common.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/always_on_top_controller.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/dock/docked_window_layout_manager.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/root_window_layout_manager.h"
#include "ash/common/wm/workspace/workspace_layout_manager.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_shelf_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/container_ids.h"
#include "ash/mus/non_client_frame_controller.h"
#include "ash/mus/property_util.h"
#include "ash/mus/screenlock_layout.h"
#include "ash/mus/shelf_layout_manager.h"
#include "ash/mus/window_manager.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/shell/public/cpp/connector.h"
#include "services/ui/common/switches.h"
#include "services/ui/common/util.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/cpp/window_tree_host_factory.h"

using ash::mojom::Container;

namespace ash {
namespace mus {

RootWindowController::RootWindowController(WindowManager* window_manager,
                                           ui::Window* root,
                                           const display::Display& display)
    : window_manager_(window_manager),
      root_(root),
      window_count_(0),
      display_(display) {
  wm_root_window_controller_.reset(
      new WmRootWindowControllerMus(window_manager_->shell(), this));

  root_window_controller_common_.reset(
      new RootWindowControllerCommon(WmWindowMus::Get(root_)));
  root_window_controller_common_->CreateContainers();
  root_window_controller_common_->CreateLayoutManagers();
  CreateLayoutManagers();

  disconnected_app_handler_.reset(new DisconnectedAppHandler(root));

  // Force a layout of the root, and its children, RootWindowLayout handles
  // both.
  root_window_controller_common_->root_window_layout()->OnWindowResized();

  for (size_t i = 0; i < kNumActivatableShellWindowIds; ++i) {
    window_manager_->window_manager_client()->AddActivationParent(
        GetWindowByShellWindowId(kActivatableShellWindowIds[i])->mus_window());
  }

  WmWindowMus* always_on_top_container =
      GetWindowByShellWindowId(kShellWindowId_AlwaysOnTopContainer);
  always_on_top_controller_.reset(
      new AlwaysOnTopController(always_on_top_container));
}

RootWindowController::~RootWindowController() {}

shell::Connector* RootWindowController::GetConnector() {
  return window_manager_->connector();
}

ui::Window* RootWindowController::NewTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  // TODO(sky): panels need a different frame, http:://crbug.com/614362.
  const bool provide_non_client_frame =
      GetWindowType(*properties) == ui::mojom::WindowType::WINDOW ||
      GetWindowType(*properties) == ui::mojom::WindowType::PANEL;
  if (provide_non_client_frame)
    (*properties)[ui::mojom::kWaitForUnderlay_Property].clear();

  // TODO(sky): constrain and validate properties before passing to server.
  ui::Window* window = root_->window_tree()->NewWindow(properties);
  window->SetBounds(CalculateDefaultBounds(window));

  ui::Window* container_window = nullptr;
  mojom::Container container;
  if (GetRequestedContainer(window, &container)) {
    container_window = GetWindowForContainer(container);
  } else {
    // TODO(sky): window->bounds() isn't quite right.
    container_window = WmWindowMus::GetMusWindow(wm::GetDefaultParent(
        WmWindowMus::Get(root_), WmWindowMus::Get(window), window->bounds()));
  }
  DCHECK(WmWindowMus::Get(container_window)->IsContainer());

  if (provide_non_client_frame) {
    NonClientFrameController::Create(GetConnector(), container_window, window,
                                     window_manager_->window_manager_client());
  } else {
    container_window->AddChild(window);
  }

  window_count_++;

  return window;
}

ui::Window* RootWindowController::GetWindowForContainer(Container container) {
  WmWindowMus* wm_window =
      GetWindowByShellWindowId(MashContainerToAshShellWindowId(container));
  DCHECK(wm_window);
  return wm_window->mus_window();
}

WmWindowMus* RootWindowController::GetWindowByShellWindowId(int id) {
  return WmWindowMus::AsWmWindowMus(
      WmWindowMus::Get(root_)->GetChildByShellWindowId(id));
}

gfx::Rect RootWindowController::CalculateDefaultBounds(
    ui::Window* window) const {
  if (window->HasSharedProperty(
          ui::mojom::WindowManager::kInitialBounds_Property)) {
    return window->GetSharedProperty<gfx::Rect>(
        ui::mojom::WindowManager::kInitialBounds_Property);
  }

  if (GetWindowShowState(window) == ui::mojom::ShowState::FULLSCREEN) {
    return gfx::Rect(0, 0, root_->bounds().width(), root_->bounds().height());
  }

  int width, height;
  const gfx::Size pref = GetWindowPreferredSize(window);
  if (pref.IsEmpty()) {
    width = root_->bounds().width() - 240;
    height = root_->bounds().height() - 240;
  } else {
    // TODO(sky): likely want to constrain more than root size.
    const gfx::Size max_size = root_->bounds().size();
    width = std::max(0, std::min(max_size.width(), pref.width()));
    height = std::max(0, std::min(max_size.height(), pref.height()));
  }
  return gfx::Rect(40 + (window_count_ % 4) * 40, 40 + (window_count_ % 4) * 40,
                   width, height);
}

void RootWindowController::OnShelfWindowAvailable() {
  DockedWindowLayoutManager* docked_window_layout_manager =
      DockedWindowLayoutManager::Get(WmWindowMus::Get(root_));

  DCHECK(!docked_window_layout_manager->shelf());
  docked_window_layout_manager->SetShelf(wm_shelf_.get());

  PanelLayoutManager::Get(WmWindowMus::Get(root_))->SetShelf(wm_shelf_.get());

  // TODO: http://crbug.com/614182 Ash's ShelfLayoutManager implements
  // DockedWindowLayoutManagerObserver so that it can inset by the docked
  // windows.
  // docked_layout_manager_->AddObserver(shelf_->shelf_layout_manager());
}

void RootWindowController::CreateLayoutManagers() {
  // Override the default layout managers for certain containers.
  WmWindowMus* lock_screen_container =
      GetWindowByShellWindowId(kShellWindowId_LockScreenContainer);
  layout_managers_[lock_screen_container->mus_window()].reset(
      new ScreenlockLayout(lock_screen_container->mus_window()));

  WmWindowMus* shelf_container =
      GetWindowByShellWindowId(kShellWindowId_ShelfContainer);
  ShelfLayoutManager* shelf_layout_manager =
      new ShelfLayoutManager(shelf_container->mus_window(), this);
  layout_managers_[shelf_container->mus_window()].reset(shelf_layout_manager);

  wm_shelf_.reset(new WmShelfMus(wm_root_window_controller_.get()));

  WmWindowMus* default_container =
      GetWindowByShellWindowId(kShellWindowId_DefaultContainer);
  // WorkspaceLayoutManager is not a mash::wm::LayoutManager (it's a
  // wm::LayoutManager), so it can't be in |layout_managers_|.
  workspace_layout_manager_ = new WorkspaceLayoutManager(default_container);
  default_container->SetLayoutManager(
      base::WrapUnique(workspace_layout_manager_));

  WmWindowMus* docked_container =
      GetWindowByShellWindowId(kShellWindowId_DockedContainer);
  docked_container->SetLayoutManager(
      base::MakeUnique<DockedWindowLayoutManager>(docked_container));

  WmWindowMus* panel_container =
      GetWindowByShellWindowId(kShellWindowId_PanelContainer);
  panel_container->SetLayoutManager(
      base::MakeUnique<PanelLayoutManager>(panel_container));
}

}  // namespace mus
}  // namespace ash
