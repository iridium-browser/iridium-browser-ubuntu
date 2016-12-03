// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <vector>

#include "ash/aura/wm_window_aura.h"
#include "ash/common/ash_constants.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm/wm_screen_util.h"
#include "ash/common/wm_window.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/snap_to_pixel_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_aura.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace wm {

// TODO(beng): replace many of these functions with the corewm versions.
void ActivateWindow(aura::Window* window) {
  ::wm::ActivateWindow(window);
}

void DeactivateWindow(aura::Window* window) {
  ::wm::DeactivateWindow(window);
}

bool IsActiveWindow(aura::Window* window) {
  return ::wm::IsActiveWindow(window);
}

aura::Window* GetActiveWindow() {
  return aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())
      ->GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return ::wm::GetActivatableWindow(window);
}

bool CanActivateWindow(aura::Window* window) {
  return ::wm::CanActivateWindow(window);
}

bool IsWindowUserPositionable(aura::Window* window) {
  return GetWindowState(window)->IsUserPositionable();
}

void PinWindow(aura::Window* window) {
  wm::WMEvent event(wm::WM_EVENT_PIN);
  wm::GetWindowState(window)->OnWMEvent(&event);
}

bool MoveWindowToEventRoot(aura::Window* window, const ui::Event& event) {
  views::View* target = static_cast<views::View*>(event.target());
  if (!target)
    return false;
  aura::Window* target_root =
      target->GetWidget()->GetNativeView()->GetRootWindow();
  if (!target_root || target_root == window->GetRootWindow())
    return false;
  aura::Window* window_container =
      ash::Shell::GetContainer(target_root, window->parent()->id());
  // Move the window to the target launcher.
  window_container->AddChild(window);
  return true;
}

void SnapWindowToPixelBoundary(aura::Window* window) {
  aura::Window* snapped_ancestor = window->parent();
  while (snapped_ancestor) {
    if (snapped_ancestor->GetProperty(kSnapChildrenToPixelBoundary)) {
      ui::SnapLayerToPhysicalPixelBoundary(snapped_ancestor->layer(),
                                           window->layer());
      return;
    }
    snapped_ancestor = snapped_ancestor->parent();
  }
}

void SetSnapsChildrenToPhysicalPixelBoundary(aura::Window* container) {
  DCHECK(!container->GetProperty(kSnapChildrenToPixelBoundary))
      << container->name();
  container->SetProperty(kSnapChildrenToPixelBoundary, true);
}

void InstallSnapLayoutManagerToContainers(aura::Window* parent) {
  aura::Window::Windows children = parent->children();
  for (aura::Window::Windows::iterator iter = children.begin();
       iter != children.end(); ++iter) {
    aura::Window* container = *iter;
    if (container->id() < 0)  // not a container
      continue;
    if (container->GetProperty(kSnapChildrenToPixelBoundary)) {
      if (!container->layout_manager())
        container->SetLayoutManager(new SnapToPixelLayoutManager(container));
    } else {
      InstallSnapLayoutManagerToContainers(container);
    }
  }
}

}  // namespace wm
}  // namespace ash
