// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/panels/panel_cocoa.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/panels/panel_titlebar_view_cocoa.h"
#import "chrome/browser/ui/cocoa/panels/panel_utils_cocoa.h"
#import "chrome/browser/ui/cocoa/panels/panel_window_controller_cocoa.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "content/public/browser/native_web_keyboard_event.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;

// This creates a shim window class, which in turn creates a Cocoa window
// controller which in turn creates actual NSWindow by loading a nib.
// Overall chain of ownership is:
// PanelWindowControllerCocoa -> PanelCocoa -> Panel.
// static
NativePanel* Panel::CreateNativePanel(Panel* panel,
                                      const gfx::Rect& bounds,
                                      bool always_on_top) {
  return new PanelCocoa(panel, bounds, always_on_top);
}

PanelCocoa::PanelCocoa(Panel* panel,
                       const gfx::Rect& bounds,
                       bool always_on_top)
    : panel_(panel),
      bounds_(bounds),
      always_on_top_(always_on_top),
      is_shown_(false),
      attention_request_id_(0),
      corner_style_(panel::ALL_ROUNDED) {
  controller_ = [[PanelWindowControllerCocoa alloc] initWithPanel:this];
}

PanelCocoa::~PanelCocoa() {
}

bool PanelCocoa::IsClosed() const {
  return !controller_;
}

void PanelCocoa::ShowPanel() {
  ShowPanelInactive();
  ActivatePanel();

  // |-makeKeyAndOrderFront:| won't send |-windowDidBecomeKey:| until we
  // return to the runloop. This causes extension tests that wait for the
  // active status change notification to fail, so we send an active status
  // notification here.
  panel_->OnActiveStateChanged(true);
}

void PanelCocoa::ShowPanelInactive() {
  if (IsClosed())
    return;

  // This method may be called several times, meaning 'ensure it's shown'.
  // Animations don't look good when repeated - hence this flag is needed.
  if (is_shown_) {
    return;
  }
  // A call to SetPanelBounds() before showing it is required to set
  // the panel frame properly.
  SetPanelBoundsInstantly(bounds_);
  is_shown_ = true;

  NSRect finalFrame = cocoa_utils::ConvertRectToCocoaCoordinates(bounds_);
  [controller_ revealAnimatedWithFrame:finalFrame];
}

gfx::Rect PanelCocoa::GetPanelBounds() const {
  return bounds_;
}

// |bounds| is the platform-independent screen coordinates, with (0,0) at
// top-left of the primary screen.
void PanelCocoa::SetPanelBounds(const gfx::Rect& bounds) {
  setBoundsInternal(bounds, true);
}

void PanelCocoa::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  setBoundsInternal(bounds, false);
}

void PanelCocoa::setBoundsInternal(const gfx::Rect& bounds, bool animate) {
  // We will call SetPanelBoundsInstantly() once before showing the panel
  // and it should set the panel frame correctly.
  if (bounds_ == bounds && is_shown_)
    return;

  bounds_ = bounds;

  // Safely ignore calls to animate bounds before the panel is shown to
  // prevent the window from loading prematurely.
  if (animate && !is_shown_)
    return;

  NSRect frame = cocoa_utils::ConvertRectToCocoaCoordinates(bounds);
  [controller_ setPanelFrame:frame animate:animate];
}

void PanelCocoa::ClosePanel() {
  if (IsClosed())
      return;

  NSWindow* window = [controller_ window];
  // performClose: contains a nested message loop which can cause reentrancy
  // if the browser is terminating and closing all the windows.
  // Use this version that corresponds to protocol of performClose: but does not
  // spin a nested loop.
  // TODO(dimich): refactor similar method from BWC and reuse here.
  if ([controller_ windowShouldClose:window]) {
    // Make sure that the panel window is not associated with the underlying
    // stack window because otherwise hiding the panel window could cause all
    // other panel windows in the same stack to disappear.
    NSWindow* stackWindow = [window parentWindow];
    if (stackWindow)
      [stackWindow removeChildWindow:window];

    [window orderOut:nil];
    [window close];
  }
}

void PanelCocoa::ActivatePanel() {
  if (!is_shown_)
    return;

  [controller_ activate];
}

void PanelCocoa::DeactivatePanel() {
  [controller_ deactivate];
}

bool PanelCocoa::IsPanelActive() const {
  // TODO(dcheng): It seems like a lot of these methods can be called before
  // our NSWindow is created. Do we really need to check in every one of these
  // methods if the NSWindow is created, or is there a better way to
  // gracefully handle this?
  if (!is_shown_)
    return false;
  return [[controller_ window] isMainWindow];
}

void PanelCocoa::PreventActivationByOS(bool prevent_activation) {
  [controller_ preventBecomingKeyWindow:prevent_activation];
  return;
}

gfx::NativeWindow PanelCocoa::GetNativePanelWindow() {
  return [controller_ window];
}

void PanelCocoa::UpdatePanelTitleBar() {
  if (!is_shown_)
    return;
  [controller_ updateTitleBar];
}

void PanelCocoa::UpdatePanelLoadingAnimations(bool should_animate) {
  [controller_ updateThrobber:should_animate];
}

void PanelCocoa::PanelCut() {
  // Nothing to do since we do not have panel-specific system menu on Mac.
}

void PanelCocoa::PanelCopy() {
  // Nothing to do since we do not have panel-specific system menu on Mac.
}

void PanelCocoa::PanelPaste() {
  // Nothing to do since we do not have panel-specific system menu on Mac.
}

void PanelCocoa::DrawAttention(bool draw_attention) {
  DCHECK((panel_->attention_mode() & Panel::USE_PANEL_ATTENTION) != 0);

  PanelTitlebarViewCocoa* titlebar = [controller_ titlebarView];
  if (draw_attention)
    [titlebar drawAttention];
  else
    [titlebar stopDrawingAttention];

  if ((panel_->attention_mode() & Panel::USE_SYSTEM_ATTENTION) != 0) {
    if (draw_attention) {
      DCHECK(!attention_request_id_);
      attention_request_id_ = [NSApp requestUserAttention:NSCriticalRequest];
    } else {
      [NSApp cancelUserAttentionRequest:attention_request_id_];
      attention_request_id_ = 0;
    }
  }
}

bool PanelCocoa::IsDrawingAttention() const {
  PanelTitlebarViewCocoa* titlebar = [controller_ titlebarView];
  return [titlebar isDrawingAttention];
}

void PanelCocoa::HandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || event.type == NativeWebKeyboardEvent::Char)
    return;

  ChromeEventProcessingWindow* event_window =
      static_cast<ChromeEventProcessingWindow*>([controller_ window]);
  DCHECK([event_window isKindOfClass:[ChromeEventProcessingWindow class]]);
  [event_window redispatchKeyEvent:event.os_event];
}

void PanelCocoa::FullScreenModeChanged(bool is_full_screen) {
  if (!is_shown_) {
    // If the panel window is not shown due to that a Chrome tab window is in
    // fullscreen mode when the panel is being created, we need to show the
    // panel window now. In addition, its titlebar needs to be updated since it
    // is not done at the panel creation time.
    if (!is_full_screen) {
      ShowPanelInactive();
      UpdatePanelTitleBar();
    }

    // No need to proceed when the panel window was not shown previously.
    // We either show the panel window or do not show it depending on current
    // full screen state.
    return;
  }
  [controller_ fullScreenModeChanged:is_full_screen];
}

bool PanelCocoa::IsPanelAlwaysOnTop() const {
  return always_on_top_;
}

void PanelCocoa::SetPanelAlwaysOnTop(bool on_top) {
  if (always_on_top_ == on_top)
    return;
  always_on_top_ = on_top;
  [controller_ updateWindowLevel];
  [controller_ updateWindowCollectionBehavior];
}

void PanelCocoa::UpdatePanelMinimizeRestoreButtonVisibility() {
  [controller_ updateTitleBarMinimizeRestoreButtonVisibility];
}

void PanelCocoa::SetWindowCornerStyle(panel::CornerStyle corner_style) {
  corner_style_ = corner_style;

  // TODO(dimich): investigate how to support it on Mac.
}

void PanelCocoa::MinimizePanelBySystem() {
  [controller_ miniaturize];
}

bool PanelCocoa::IsPanelMinimizedBySystem() const {
  return [controller_ isMiniaturized];
}

bool PanelCocoa::IsPanelShownOnActiveDesktop() const {
  return [[controller_ window] isOnActiveSpace];
}

void PanelCocoa::ShowShadow(bool show) {
  [controller_ showShadow:show];
}

void PanelCocoa::PanelExpansionStateChanging(
    Panel::ExpansionState old_state, Panel::ExpansionState new_state) {
  [controller_ updateWindowLevel:(new_state != Panel::EXPANDED)];
}

void PanelCocoa::AttachWebContents(content::WebContents* contents) {
  [controller_ webContentsInserted:contents];
}

void PanelCocoa::DetachWebContents(content::WebContents* contents) {
  [controller_ webContentsDetached:contents];
}

gfx::Size PanelCocoa::WindowSizeFromContentSize(
    const gfx::Size& content_size) const {
  NSRect content = NSMakeRect(0, 0,
                              content_size.width(), content_size.height());
  NSRect frame = [controller_ frameRectForContentRect:content];
  return gfx::Size(NSWidth(frame), NSHeight(frame));
}

gfx::Size PanelCocoa::ContentSizeFromWindowSize(
    const gfx::Size& window_size) const {
  NSRect frame = NSMakeRect(0, 0, window_size.width(), window_size.height());
  NSRect content = [controller_ contentRectForFrameRect:frame];
  return gfx::Size(NSWidth(content), NSHeight(content));
}

int PanelCocoa::TitleOnlyHeight() const {
  return [controller_ titlebarHeightInScreenCoordinates];
}

Panel* PanelCocoa::panel() const {
  return panel_.get();
}

void PanelCocoa::DidCloseNativeWindow() {
  DCHECK(!IsClosed());
  controller_ = NULL;
  panel_->OnNativePanelClosed();
}

// NativePanelTesting implementation.
class CocoaNativePanelTesting : public NativePanelTesting {
 public:
  CocoaNativePanelTesting(NativePanel* native_panel);
  ~CocoaNativePanelTesting() override {}
  // Overridden from NativePanelTesting
  void PressLeftMouseButtonTitlebar(const gfx::Point& mouse_location,
                                    panel::ClickModifier modifier) override;
  void ReleaseMouseButtonTitlebar(panel::ClickModifier modifier) override;
  void DragTitlebar(const gfx::Point& mouse_location) override;
  void CancelDragTitlebar() override;
  void FinishDragTitlebar() override;
  bool VerifyDrawingAttention() const override;
  bool VerifyActiveState(bool is_active) override;
  bool VerifyAppIcon() const override;
  bool VerifySystemMinimizeState() const override;
  bool IsWindowVisible() const override;
  bool IsWindowSizeKnown() const override;
  bool IsAnimatingBounds() const override;
  bool IsButtonVisible(panel::TitlebarButtonType button_type) const override;
  panel::CornerStyle GetWindowCornerStyle() const override;
  bool EnsureApplicationRunOnForeground() override;

 private:
  PanelTitlebarViewCocoa* titlebar() const;
  // Weak, assumed always to outlive this test API object.
  PanelCocoa* native_panel_window_;
};

NativePanelTesting* PanelCocoa::CreateNativePanelTesting() {
  return new CocoaNativePanelTesting(this);
}

CocoaNativePanelTesting::CocoaNativePanelTesting(NativePanel* native_panel)
  : native_panel_window_(static_cast<PanelCocoa*>(native_panel)) {
}

PanelTitlebarViewCocoa* CocoaNativePanelTesting::titlebar() const {
  return [native_panel_window_->controller_ titlebarView];
}

void CocoaNativePanelTesting::PressLeftMouseButtonTitlebar(
    const gfx::Point& mouse_location, panel::ClickModifier modifier) {
  // Convert from platform-indepedent screen coordinates to Cocoa's screen
  // coordinates because PanelTitlebarViewCocoa method takes Cocoa's screen
  // coordinates.
  int modifierFlags =
      (modifier == panel::APPLY_TO_ALL ? NSShiftKeyMask : 0);
  [titlebar() pressLeftMouseButtonTitlebar:
      cocoa_utils::ConvertPointToCocoaCoordinates(mouse_location)
           modifiers:modifierFlags];
}

void CocoaNativePanelTesting::ReleaseMouseButtonTitlebar(
    panel::ClickModifier modifier) {
  int modifierFlags =
      (modifier == panel::APPLY_TO_ALL ? NSShiftKeyMask : 0);
  [titlebar() releaseLeftMouseButtonTitlebar:modifierFlags];
}

void CocoaNativePanelTesting::DragTitlebar(const gfx::Point& mouse_location) {
  // Convert from platform-indepedent screen coordinates to Cocoa's screen
  // coordinates because PanelTitlebarViewCocoa method takes Cocoa's screen
  // coordinates.
  [titlebar() dragTitlebar:
      cocoa_utils::ConvertPointToCocoaCoordinates(mouse_location)];
}

void CocoaNativePanelTesting::CancelDragTitlebar() {
  [titlebar() cancelDragTitlebar];
}

void CocoaNativePanelTesting::FinishDragTitlebar() {
  [titlebar() finishDragTitlebar];
}

bool CocoaNativePanelTesting::VerifyDrawingAttention() const {
  return [titlebar() isDrawingAttention];
}

bool CocoaNativePanelTesting::VerifyActiveState(bool is_active) {
  // TODO(jianli): to be implemented.
  return false;
}

bool CocoaNativePanelTesting::VerifyAppIcon() const {
  // Nothing to do since panel does not show dock icon.
  return true;
}

bool CocoaNativePanelTesting::VerifySystemMinimizeState() const {
  // TODO(jianli): to be implemented.
  return true;
}

bool CocoaNativePanelTesting::IsWindowVisible() const {
  return [[native_panel_window_->controller_ window] isVisible];
}

bool CocoaNativePanelTesting::IsWindowSizeKnown() const {
  return true;
}

bool CocoaNativePanelTesting::IsAnimatingBounds() const {
  if ([native_panel_window_->controller_ isAnimatingBounds])
    return true;
  StackedPanelCollection* stack = native_panel_window_->panel()->stack();
  if (!stack)
    return false;
  return stack->IsAnimatingPanelBounds(native_panel_window_->panel());
}

bool CocoaNativePanelTesting::IsButtonVisible(
    panel::TitlebarButtonType button_type) const {
  switch (button_type) {
    case panel::CLOSE_BUTTON:
      return ![[titlebar() closeButton] isHidden];
    case panel::MINIMIZE_BUTTON:
      return ![[titlebar() minimizeButton] isHidden];
    case panel::RESTORE_BUTTON:
      return ![[titlebar() restoreButton] isHidden];
    default:
      NOTREACHED();
  }
  return false;
}

panel::CornerStyle CocoaNativePanelTesting::GetWindowCornerStyle() const {
  return native_panel_window_->corner_style_;
}

bool CocoaNativePanelTesting::EnsureApplicationRunOnForeground() {
  if ([NSApp isActive])
    return true;
  [NSApp activateIgnoringOtherApps:YES];
  return [NSApp isActive];
}
