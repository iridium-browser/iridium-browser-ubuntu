// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_focus_rules.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window.h"

namespace ash {
namespace wm {
namespace {

// These are the list of container ids of containers which may contain windows
// that need to be activated in the order that they should be activated.
const int kWindowContainerIds[] = {
    kShellWindowId_OverlayContainer,
    kShellWindowId_LockSystemModalContainer,
    kShellWindowId_SettingBubbleContainer,
    kShellWindowId_LockScreenContainer,
    kShellWindowId_SystemModalContainer,
    kShellWindowId_AlwaysOnTopContainer,
    kShellWindowId_AppListContainer,
    kShellWindowId_DefaultContainer,

    // Docked, panel, launcher and status are intentionally checked after other
    // containers even though these layers are higher. The user expects their
    // windows to be focused before these elements.
    kShellWindowId_DockedContainer,
    kShellWindowId_PanelContainer,
    kShellWindowId_ShelfContainer,
    kShellWindowId_StatusContainer, };

bool BelongsToContainerWithEqualOrGreaterId(const aura::Window* window,
                                            int container_id) {
  for (; window; window = window->parent()) {
    if (window->id() >= container_id)
      return true;
  }
  return false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, public:

AshFocusRules::AshFocusRules() {
}

AshFocusRules::~AshFocusRules() {
}

bool AshFocusRules::IsWindowConsideredActivatable(aura::Window* window) const {
  // Only toplevel windows can be activated.
  if (!IsToplevelWindow(window))
    return false;

  // The window must be visible.
  if (!IsWindowConsideredVisibleForActivation(window))
    return false;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, ::wm::FocusRules:

bool AshFocusRules::SupportsChildActivation(aura::Window* window) const {
  if (window->id() == kShellWindowId_DefaultContainer)
    return true;

  for (size_t i = 0; i < arraysize(kWindowContainerIds); i++) {
    if (window->id() == kWindowContainerIds[i])
      return true;
  }
  return false;
}

bool AshFocusRules::IsWindowConsideredVisibleForActivation(
    aura::Window* window) const {
  // If the |window| doesn't belong to the current active user and also doesn't
  // show for the current active user, then it should not be activated.
  SessionStateDelegate* delegate =
      Shell::GetInstance()->session_state_delegate();
  if (delegate->NumberOfLoggedInUsers() > 1) {
    content::BrowserContext* active_browser_context =
        Shell::GetInstance()->delegate()->GetActiveBrowserContext();
    content::BrowserContext* owner_browser_context =
        delegate->GetBrowserContextForWindow(window);
    content::BrowserContext* shown_browser_context =
        delegate->GetUserPresentingBrowserContextForWindow(window);

    if (owner_browser_context && active_browser_context &&
        owner_browser_context != active_browser_context &&
        shown_browser_context != active_browser_context) {
      return false;
    }
  }

  if (BaseFocusRules::IsWindowConsideredVisibleForActivation(window))
    return true;

  // Minimized windows are hidden in their minimized state, but they can always
  // be activated.
  if (wm::GetWindowState(window)->IsMinimized())
    return true;

  return window->TargetVisibility() &&
         (window->parent()->id() == kShellWindowId_DefaultContainer ||
          window->parent()->id() == kShellWindowId_LockScreenContainer);
}

bool AshFocusRules::CanActivateWindow(aura::Window* window) const {
  // Clearing activation is always permissible.
  if (!window)
    return true;

  if (!BaseFocusRules::CanActivateWindow(window))
    return false;

  if (Shell::GetInstance()->IsSystemModalWindowOpen()) {
    return BelongsToContainerWithEqualOrGreaterId(
        window, kShellWindowId_SystemModalContainer);
  }

  return true;
}

aura::Window* AshFocusRules::GetNextActivatableWindow(
    aura::Window* ignore) const {
  DCHECK(ignore);

  // Start from the container of the most-recently-used window. If the list of
  // MRU windows is empty, then start from the container of the window that just
  // lost focus |ignore|.
  ash::MruWindowTracker* mru = ash::Shell::GetInstance()->mru_window_tracker();
  std::vector<aura::Window*> windows = mru->BuildMruWindowList();
  aura::Window* starting_window = windows.empty() ? ignore : windows[0];

  // Look for windows to focus in |starting_window|'s container. If none are
  // found, we look in all the containers in front of |starting_window|'s
  // container, then all behind.
  int starting_container_index = 0;
  aura::Window* root = starting_window->GetRootWindow();
  if (!root)
    root = Shell::GetTargetRootWindow();
  int container_count = static_cast<int>(arraysize(kWindowContainerIds));
  for (int i = 0; i < container_count; i++) {
    aura::Window* container = Shell::GetContainer(root, kWindowContainerIds[i]);
    if (container && container->Contains(starting_window)) {
      starting_container_index = i;
      break;
    }
  }

  aura::Window* window = nullptr;
  for (int i = starting_container_index; !window && i < container_count; i++)
    window = GetTopmostWindowToActivateForContainerIndex(i, ignore);
  if (!window && starting_container_index > 0) {
    for (int i = starting_container_index - 1; !window && i >= 0; i--)
      window = GetTopmostWindowToActivateForContainerIndex(i, ignore);
  }
  return window;
}

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, private:

aura::Window* AshFocusRules::GetTopmostWindowToActivateForContainerIndex(
    int index,
    aura::Window* ignore) const {
  aura::Window* window = NULL;
  aura::Window* root = ignore ? ignore->GetRootWindow() : NULL;
  aura::Window::Windows containers = Shell::GetContainersFromAllRootWindows(
      kWindowContainerIds[index], root);
  for (aura::Window::Windows::const_iterator iter = containers.begin();
        iter != containers.end() && !window; ++iter) {
    window = GetTopmostWindowToActivateInContainer((*iter), ignore);
  }
  return window;
}

aura::Window* AshFocusRules::GetTopmostWindowToActivateInContainer(
    aura::Window* container,
    aura::Window* ignore) const {
  for (aura::Window::Windows::const_reverse_iterator i =
           container->children().rbegin();
       i != container->children().rend();
       ++i) {
    WindowState* window_state = GetWindowState(*i);
    if (*i != ignore &&
        window_state->CanActivate() &&
        !window_state->IsMinimized())
      return *i;
  }
  return NULL;
}

}  // namespace wm
}  // namespace ash
