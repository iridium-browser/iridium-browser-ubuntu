// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/container_finder.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/always_on_top_controller.h"
#include "ash/common/wm/root_window_finder.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace wm {
namespace {

WmWindow* FindContainerRoot(WmShell* shell, const gfx::Rect& bounds) {
  if (bounds == gfx::Rect())
    return shell->GetRootWindowForNewWindows();
  return GetRootWindowMatching(bounds);
}

bool HasTransientParentWindow(const WmWindow* window) {
  return window->GetTransientParent() &&
         window->GetTransientParent()->GetType() != ui::wm::WINDOW_TYPE_UNKNOWN;
}

WmWindow* GetSystemModalContainer(WmWindow* root, WmWindow* window) {
  DCHECK(window->IsSystemModal());

  // If screen lock is not active and user session is active,
  // all modal windows are placed into the normal modal container.
  // In case of missing transient parent (it could happen for alerts from
  // background pages) assume that the window belongs to user session.
  if (!window->GetShell()->GetSessionStateDelegate()->IsUserSessionBlocked() ||
      !window->GetTransientParent()) {
    return root->GetChildByShellWindowId(kShellWindowId_SystemModalContainer);
  }

  // Otherwise those that originate from LockScreen container and above are
  // placed in the screen lock modal container.
  int window_container_id =
      window->GetTransientParent()->GetParent()->GetShellWindowId();
  if (window_container_id < kShellWindowId_LockScreenContainer)
    return root->GetChildByShellWindowId(kShellWindowId_SystemModalContainer);
  return root->GetChildByShellWindowId(kShellWindowId_LockSystemModalContainer);
}

WmWindow* GetContainerFromAlwaysOnTopController(WmWindow* root,
                                                WmWindow* window) {
  return root->GetRootWindowController()
      ->GetAlwaysOnTopController()
      ->GetContainer(window);
}

}  // namespace

WmWindow* GetContainerForWindow(WmWindow* window) {
  WmWindow* container = window->GetParent();
  while (container && container->GetType() != ui::wm::WINDOW_TYPE_UNKNOWN)
    container = container->GetParent();
  return container;
}

WmWindow* GetDefaultParent(WmWindow* context,
                           WmWindow* window,
                           const gfx::Rect& bounds) {
  WmWindow* target_root = nullptr;
  WmWindow* transient_parent = window->GetTransientParent();
  if (transient_parent) {
    // Transient window should use the same root as its transient parent.
    target_root = transient_parent->GetRootWindow();
  } else {
    target_root = FindContainerRoot(context->GetShell(), bounds);
  }

  switch (window->GetType()) {
    case ui::wm::WINDOW_TYPE_NORMAL:
    case ui::wm::WINDOW_TYPE_POPUP:
      if (window->IsSystemModal())
        return GetSystemModalContainer(target_root, window);
      if (HasTransientParentWindow(window))
        return GetContainerForWindow(window->GetTransientParent());
      return GetContainerFromAlwaysOnTopController(target_root, window);
    case ui::wm::WINDOW_TYPE_CONTROL:
      return target_root->GetChildByShellWindowId(
          kShellWindowId_UnparentedControlContainer);
    case ui::wm::WINDOW_TYPE_PANEL:
      if (window->GetWindowState()->panel_attached())
        return target_root->GetChildByShellWindowId(
            kShellWindowId_PanelContainer);
      return GetContainerFromAlwaysOnTopController(target_root, window);
    case ui::wm::WINDOW_TYPE_MENU:
      return target_root->GetChildByShellWindowId(kShellWindowId_MenuContainer);
    case ui::wm::WINDOW_TYPE_TOOLTIP:
      return target_root->GetChildByShellWindowId(
          kShellWindowId_DragImageAndTooltipContainer);
    default:
      NOTREACHED() << "Window " << window->GetShellWindowId()
                   << " has unhandled type " << window->GetType();
      break;
  }
  return nullptr;
}

}  // namespace wm
}  // namespace ash
