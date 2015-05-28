// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/widget.h"

class AvatarMenuButton;
class BrowserRootView;
class BrowserView;
class NativeBrowserFrame;
class NewAvatarButton;
class NonClientFrameView;
class SystemMenuModelBuilder;

namespace gfx {
class FontList;
class Rect;
}

namespace ui {
class EventHandler;
class MenuModel;
class ThemeProvider;
}

namespace views {
class MenuRunner;
class View;
}

// This is a virtual interface that allows system specific browser frames.
class BrowserFrame
    : public views::Widget,
      public views::ContextMenuController {
 public:
  explicit BrowserFrame(BrowserView* browser_view);
  ~BrowserFrame() override;

  static const gfx::FontList& GetTitleFontList();

  // Initialize the frame (creates the underlying native window).
  void InitBrowserFrame();

  // Sets the ThemeProvider returned from GetThemeProvider().
  void SetThemeProvider(scoped_ptr<ui::ThemeProvider> provider);

  // Determine the distance of the left edge of the minimize button from the
  // left edge of the window. Used in our Non-Client View's Layout.
  int GetMinimizeButtonOffset() const;

  // Retrieves the bounds, in non-client view coordinates for the specified
  // TabStrip view.
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const;

  // Returns the inset of the topmost view in the client view from the top of
  // the non-client view. The topmost view depends on the window type. The
  // topmost view is the tab strip for tabbed browser windows, the toolbar for
  // popups, the web contents for app windows and varies for fullscreen windows
  int GetTopInset() const;

  // Returns the amount that the theme background should be inset.
  int GetThemeBackgroundXInset() const;

  // Tells the frame to update the throbber.
  void UpdateThrobber(bool running);

  // Tells the frame to update any toolbar elements it has.
  void UpdateToolbar();

  // Returns the location icon, if there is a location icon embedded into the
  // frame. This is the case for web app frames, which do not have a visible
  // toolbar. Instead of using the normal location icon from the location bar
  // in the toolbar, these windows have a location icon in the frame.
  views::View* GetLocationIconView() const;

  // Returns the NonClientFrameView of this frame.
  views::View* GetFrameView() const;

  // Returns |true| if we should use the custom frame.
  bool UseCustomFrame() const;

  // Returns true when the window placement should be saved.
  bool ShouldSaveWindowPlacement() const;

  // Retrieves the window placement (show state and bounds) for restoring.
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const;

  // Overridden from views::Widget:
  views::internal::RootView* CreateRootView() override;
  views::NonClientFrameView* CreateNonClientFrameView() override;
  bool GetAccelerator(int command_id,
                      ui::Accelerator* accelerator) const override;
  ui::ThemeProvider* GetThemeProvider() const override;
  void SchedulePaintInRect(const gfx::Rect& rect) override;
  void OnNativeWidgetActivationChanged(bool active) override;

  // Overridden from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& p,
                              ui::MenuSourceType source_type) override;

  // Returns true if we should leave any offset at the frame caption. Typically
  // when the frame is maximized/full screen we want to leave no offset at the
  // top.
  bool ShouldLeaveOffsetNearTopBorder();

  AvatarMenuButton* GetAvatarMenuButton();

  NewAvatarButton* GetNewAvatarMenuButton();

  // Returns the menu model. BrowserFrame owns the returned model.
  // Note that in multi user mode this will upon each call create a new model.
  ui::MenuModel* GetSystemMenuModel();

 private:
  NativeBrowserFrame* native_browser_frame_;

  // A weak reference to the root view associated with the window. We save a
  // copy as a BrowserRootView to avoid evil casting later, when we need to call
  // functions that only exist on BrowserRootView (versus RootView).
  BrowserRootView* root_view_;

  // A pointer to our NonClientFrameView as a BrowserNonClientFrameView.
  BrowserNonClientFrameView* browser_frame_view_;

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  scoped_ptr<SystemMenuModelBuilder> menu_model_builder_;

  // Used to show the system menu. Only used if
  // NativeBrowserFrame::UsesNativeSystemMenu() returns false.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // SetThemeProvider() triggers setting both |owned_theme_provider_| and
  // |theme_provider_|. Initially |theme_provider_| is set to the ThemeService
  // and |owned_theme_provider_| is null (as ThemeServices lifetime is managed
  // externally).
  scoped_ptr<ui::ThemeProvider> owned_theme_provider_;
  ui::ThemeProvider* theme_provider_;

  scoped_ptr<ui::EventHandler> browser_command_handler_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrame);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
