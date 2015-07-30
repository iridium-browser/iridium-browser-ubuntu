// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
#define CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_

#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "ui/gfx/native_widget_types.h"

class NativePanelTesting;

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}

namespace gfx {
class Rect;
}  // namespace gfx

// An interface for a class that implements platform-specific behavior for panel
// windows to provide additional methods not found in ui::BaseWindow.
class NativePanel {
  friend class BasePanelBrowserTest;  // for CreateNativePanelTesting
  friend class Panel;
  friend class PanelBrowserWindow;
  friend class PanelBrowserTest;
  friend class PanelExtensionBrowserTest;

 protected:
  virtual ~NativePanel() {}

  virtual void ShowPanel() = 0;
  virtual void ShowPanelInactive() = 0;
  virtual gfx::Rect GetPanelBounds() const = 0;
  virtual void SetPanelBounds(const gfx::Rect& bounds) = 0;
  virtual void SetPanelBoundsInstantly(const gfx::Rect& bounds) = 0;
  virtual void ClosePanel() = 0;
  virtual void ActivatePanel() = 0;
  virtual void DeactivatePanel() = 0;
  virtual bool IsPanelActive() const = 0;
  virtual void PreventActivationByOS(bool prevent_activation) = 0;
  virtual gfx::NativeWindow GetNativePanelWindow() = 0;
  virtual void UpdatePanelTitleBar() = 0;
  virtual void UpdatePanelLoadingAnimations(bool should_animate) = 0;
  virtual void PanelCut() = 0;
  virtual void PanelCopy() = 0;
  virtual void PanelPaste() = 0;
  virtual void DrawAttention(bool draw_attention) = 0;
  virtual bool IsDrawingAttention() const = 0;
  virtual void HandlePanelKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;
  virtual void FullScreenModeChanged(bool is_full_screen) = 0;
  virtual void PanelExpansionStateChanging(Panel::ExpansionState old_state,
                                           Panel::ExpansionState new_state) = 0;
  virtual void AttachWebContents(content::WebContents* contents) = 0;
  virtual void DetachWebContents(content::WebContents* contents) = 0;

  // Returns the exterior size of the panel window given the client content
  // size and vice versa.
  virtual gfx::Size WindowSizeFromContentSize(
      const gfx::Size& content_size) const = 0;
  virtual gfx::Size ContentSizeFromWindowSize(
      const gfx::Size& window_size) const = 0;

  virtual int TitleOnlyHeight() const = 0;

  // Gets or sets whether the panel window is always on top.
  virtual bool IsPanelAlwaysOnTop() const = 0;
  virtual void SetPanelAlwaysOnTop(bool on_top) = 0;

  // Updates the visibility of the minimize and restore buttons.
  virtual void UpdatePanelMinimizeRestoreButtonVisibility() = 0;

  // Sets how the panel window displays its 4 corners, rounded or not.
  virtual void SetWindowCornerStyle(panel::CornerStyle corner_style) = 0;

  // Performs the system minimize for the panel, i.e. becoming iconic.
  virtual void MinimizePanelBySystem() = 0;

  // Returns true if the panel has been minimized by the system, i.e. becoming
  // iconic.
  virtual bool IsPanelMinimizedBySystem() const = 0;

  // Returns true if the panel is shown in the active desktop. The user could
  // create and use multiple virtual desktops or workspaces.
  virtual bool IsPanelShownOnActiveDesktop() const = 0;

  // Turns on/off the shadow effect around the window shape.
  virtual void ShowShadow(bool show) = 0;

  // Create testing interface for native panel. (Keep this last to separate
  // it from regular API.)
  virtual NativePanelTesting* CreateNativePanelTesting() = 0;
};

// A NativePanel utility interface used for accessing elements of the
// native panel used only by test automation.
class NativePanelTesting {
 public:
  virtual ~NativePanelTesting() {}

  // Wrappers for the common cases when no modifier is needed.
  void PressLeftMouseButtonTitlebar(const gfx::Point& mouse_location) {
    PressLeftMouseButtonTitlebar(mouse_location, panel::NO_MODIFIER);
  }
  void ReleaseMouseButtonTitlebar() {
    ReleaseMouseButtonTitlebar(panel::NO_MODIFIER);
  }

  // |mouse_location| is in screen coordinates.
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& mouse_location, panel::ClickModifier modifier) = 0;
  virtual void ReleaseMouseButtonTitlebar(panel::ClickModifier modifier) = 0;
  virtual void DragTitlebar(const gfx::Point& mouse_location) = 0;
  virtual void CancelDragTitlebar() = 0;
  virtual void FinishDragTitlebar() = 0;

  // Verifies, on a deepest possible level, if the Panel is showing the "Draw
  // Attention" effects to the user. May include checking colors etc.
  virtual bool VerifyDrawingAttention() const = 0;
  // Verifies, on a deepest possible level, if the native panel is really
  // active, i.e. the titlebar is painted per its active state.
  virtual bool VerifyActiveState(bool is_active) = 0;
  // Verifies, on a deepest possible level, if the native panel is really
  // showing a correct app icon (taskbar icon).
  virtual bool VerifyAppIcon() const = 0;
  // Verifies, on a deepest possible level, if the native panel is really
  // minimized by the system.
  virtual bool VerifySystemMinimizeState() const = 0;

  virtual bool IsWindowVisible() const = 0;
  virtual bool IsWindowSizeKnown() const = 0;
  virtual bool IsAnimatingBounds() const = 0;
  virtual bool IsButtonVisible(panel::TitlebarButtonType button_type) const = 0;

  virtual panel::CornerStyle GetWindowCornerStyle() const = 0;

  // Makes sure that the application is running on foreground. Returns false
  // if the effort fails.
  virtual bool EnsureApplicationRunOnForeground() = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
