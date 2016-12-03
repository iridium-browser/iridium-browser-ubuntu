// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_delegate.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/ash_switches.h"
#include "ash/common/shelf/app_list_button.h"
#include "ash/common/shelf/shelf.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/shelf/shelf_types.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/wm_screen_util.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/presenter/app_list_presenter.h"
#include "ui/app_list/presenter/app_list_view_delegate_factory.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// The minimal anchor position offset to make sure that the bubble is still on
// the screen with 8 pixels spacing on the left / right. This constant is a
// result of minimal bubble arrow sizes and offsets.
const int kMinimalAnchorPositionOffset = 57;

// Gets arrow location based on shelf alignment.
views::BubbleBorder::Arrow GetBubbleArrow(aura::Window* window) {
  DCHECK(Shell::HasInstance());
  WmShelf* shelf = Shelf::ForWindow(WmWindowAura::Get(window))->wm_shelf();
  switch (shelf->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return views::BubbleBorder::BOTTOM_CENTER;
    case SHELF_ALIGNMENT_LEFT:
      return views::BubbleBorder::LEFT_CENTER;
    case SHELF_ALIGNMENT_RIGHT:
      return views::BubbleBorder::RIGHT_CENTER;
  }
  NOTREACHED();
  return views::BubbleBorder::BOTTOM_CENTER;
}

// Using |button_bounds|, determine the anchor offset so that the bubble gets
// shown above the shelf (used for the alternate shelf theme).
gfx::Vector2d GetAnchorPositionOffsetToShelf(const gfx::Rect& button_bounds,
                                             views::Widget* widget) {
  DCHECK(Shell::HasInstance());
  ShelfAlignment shelf_alignment =
      Shelf::ForWindow(WmLookup::Get()->GetWindowForWidget(widget))
          ->wm_shelf()
          ->alignment();
  gfx::Point anchor(button_bounds.CenterPoint());
  switch (shelf_alignment) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      if (base::i18n::IsRTL()) {
        int screen_width = widget->GetWorkAreaBoundsInScreen().width();
        return gfx::Vector2d(
            std::min(screen_width - kMinimalAnchorPositionOffset - anchor.x(),
                     0),
            0);
      }
      return gfx::Vector2d(
          std::max(kMinimalAnchorPositionOffset - anchor.x(), 0), 0);
    case SHELF_ALIGNMENT_LEFT:
      return gfx::Vector2d(
          0, std::max(kMinimalAnchorPositionOffset - anchor.y(), 0));
    case SHELF_ALIGNMENT_RIGHT:
      return gfx::Vector2d(
          0, std::max(kMinimalAnchorPositionOffset - anchor.y(), 0));
  }
  NOTREACHED();
  return gfx::Vector2d();
}

// Gets the point at the center of the display that a particular view is on.
// This calculation excludes the virtual keyboard area. If the height of the
// display area is less than |minimum_height|, its bottom will be extended to
// that height (so that the app list never starts above the top of the screen).
gfx::Point GetCenterOfDisplayForView(views::View* view, int minimum_height) {
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(view->GetWidget());
  gfx::Rect bounds = wm::GetDisplayBoundsWithShelf(window);
  bounds = window->GetRootWindow()->ConvertRectToScreen(bounds);

  // If the virtual keyboard is active, subtract it from the display bounds, so
  // that the app list is centered in the non-keyboard area of the display.
  // (Note that work_area excludes the keyboard, but it doesn't get updated
  // until after this function is called.)
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller->keyboard_visible())
    bounds.Subtract(keyboard_controller->current_keyboard_bounds());

  // Apply the |minimum_height|.
  if (bounds.height() < minimum_height)
    bounds.set_height(minimum_height);

  return bounds.CenterPoint();
}

bool IsFullscreenAppListEnabled() {
#if defined(OS_CHROMEOS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshEnableFullscreenAppList) &&
         app_list::switches::IsExperimentalAppListEnabled();
#else
  return false;
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, public:

AppListPresenterDelegate::AppListPresenterDelegate(
    app_list::AppListPresenter* presenter,
    app_list::AppListViewDelegateFactory* view_delegate_factory)
    : presenter_(presenter), view_delegate_factory_(view_delegate_factory) {
  WmShell::Get()->AddShellObserver(this);
}

AppListPresenterDelegate::~AppListPresenterDelegate() {
  DCHECK(view_);
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->RemoveObserver(this);
  Shell::GetInstance()->RemovePreTargetHandler(this);
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(view_->GetWidget());
  window->GetRootWindowController()->GetShelf()->RemoveObserver(this);
  WmShell::Get()->RemoveShellObserver(this);
}

app_list::AppListViewDelegate* AppListPresenterDelegate::GetViewDelegate() {
  return view_delegate_factory_->GetDelegate();
}

void AppListPresenterDelegate::Init(app_list::AppListView* view,
                                    int64_t display_id,
                                    int current_apps_page) {
  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  ash::Shell::GetPrimaryRootWindowController()
      ->GetShelfLayoutManager()
      ->UpdateAutoHideState();
  view_ = view;
  aura::Window* root_window = Shell::GetInstance()
                                  ->window_tree_host_manager()
                                  ->GetRootWindowForDisplayId(display_id);
  aura::Window* container = GetRootWindowController(root_window)
                                ->GetContainer(kShellWindowId_AppListContainer);
  Shelf* shelf = Shelf::ForWindow(WmWindowAura::Get(container));
  AppListButton* applist_button = shelf->GetAppListButton();
  is_centered_ = view->ShouldCenterWindow();
  bool is_fullscreen = IsFullscreenAppListEnabled() &&
                       WmShell::Get()
                           ->maximize_mode_controller()
                           ->IsMaximizeModeWindowManagerEnabled();
  if (is_fullscreen) {
    view->InitAsFramelessWindow(
        container, current_apps_page,
        ScreenUtil::GetDisplayWorkAreaBoundsInParent(container));
  } else if (is_centered_) {
    // Note: We can't center the app list until we have its dimensions, so we
    // init at (0, 0) and then reset its anchor point.
    view->InitAsBubbleAtFixedLocation(container, current_apps_page,
                                      gfx::Point(), views::BubbleBorder::FLOAT,
                                      true /* border_accepts_events */);
    // The experimental app list is centered over the display of the app list
    // button that was pressed (if triggered via keyboard, this is the display
    // with the currently focused window).
    view->SetAnchorPoint(GetCenterOfDisplayForView(
        applist_button, GetMinimumBoundsHeightForAppList(view)));
  } else {
    gfx::Rect applist_button_bounds = applist_button->GetBoundsInScreen();
    // We need the location of the button within the local screen.
    applist_button_bounds =
        ScreenUtil::ConvertRectFromScreen(root_window, applist_button_bounds);
    view->InitAsBubbleAttachedToAnchor(
        container, current_apps_page, applist_button,
        GetAnchorPositionOffsetToShelf(applist_button_bounds,
                                       applist_button->GetWidget()),
        GetBubbleArrow(container), true /* border_accepts_events */);
    view->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);
  }

  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller)
    keyboard_controller->AddObserver(this);
  Shell::GetInstance()->AddPreTargetHandler(this);
  WmWindow* window = WmShell::Get()->GetRootWindowForDisplayId(display_id);
  window->GetRootWindowController()->GetShelf()->AddObserver(this);

  // By setting us as DnD recipient, the app list knows that we can
  // handle items.
  view->SetDragAndDropHostOfCurrentAppList(
      shelf->GetDragAndDropHostForAppList());
}

void AppListPresenterDelegate::OnShown(int64_t display_id) {
  is_visible_ = true;
  // Update applist button status when app list visibility is changed.
  WmWindow* root_window = WmShell::Get()->GetRootWindowForDisplayId(display_id);
  Shelf::ForWindow(root_window)->GetAppListButton()->OnAppListShown();
}

void AppListPresenterDelegate::OnDismissed() {
  DCHECK(is_visible_);
  DCHECK(view_);

  is_visible_ = false;

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shelf* shelf =
      Shelf::ForWindow(WmLookup::Get()->GetWindowForWidget(view_->GetWidget()));
  shelf->shelf_layout_manager()->UpdateAutoHideState();

  // Update applist button status when app list visibility is changed.
  shelf->GetAppListButton()->OnAppListDismissed();
}

void AppListPresenterDelegate::UpdateBounds() {
  if (!view_ || !is_visible_)
    return;

  view_->UpdateBounds();

  if (is_centered_) {
    view_->SetAnchorPoint(GetCenterOfDisplayForView(
        view_, GetMinimumBoundsHeightForAppList(view_)));
  }
}

gfx::Vector2d AppListPresenterDelegate::GetVisibilityAnimationOffset(
    aura::Window* root_window) {
  DCHECK(Shell::HasInstance());

  // App list needs to know the new shelf layout in order to calculate its
  // UI layout when AppListView visibility changes.
  Shelf* shelf = Shelf::ForWindow(WmWindowAura::Get(root_window));
  shelf->shelf_layout_manager()->UpdateAutoHideState();

  switch (shelf->wm_shelf()->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      return gfx::Vector2d(0, kAnimationOffset);
    case SHELF_ALIGNMENT_LEFT:
      return gfx::Vector2d(-kAnimationOffset, 0);
    case SHELF_ALIGNMENT_RIGHT:
      return gfx::Vector2d(kAnimationOffset, 0);
  }
  NOTREACHED();
  return gfx::Vector2d();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, private:

void AppListPresenterDelegate::ProcessLocatedEvent(ui::LocatedEvent* event) {
  if (!view_ || !is_visible_)
    return;

  // If the event happened on a menu, then the event should not close the app
  // list.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (target) {
    RootWindowController* root_controller =
        GetRootWindowController(target->GetRootWindow());
    if (root_controller) {
      aura::Window* menu_container =
          root_controller->GetContainer(kShellWindowId_MenuContainer);
      if (menu_container->Contains(target))
        return;
      aura::Window* keyboard_container = root_controller->GetContainer(
          kShellWindowId_VirtualKeyboardContainer);
      if (keyboard_container->Contains(target))
        return;
    }
  }

  aura::Window* window = view_->GetWidget()->GetNativeView()->parent();
  if (!window->Contains(target) &&
      !app_list::switches::ShouldNotDismissOnBlur()) {
    presenter_->Dismiss();
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, aura::EventFilter implementation:

void AppListPresenterDelegate::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessLocatedEvent(event);
}

void AppListPresenterDelegate::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    ProcessLocatedEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, keyboard::KeyboardControllerObserver
// implementation:

void AppListPresenterDelegate::OnKeyboardBoundsChanging(
    const gfx::Rect& new_bounds) {
  UpdateBounds();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, ShellObserver implementation:
void AppListPresenterDelegate::OnShelfAlignmentChanged(WmWindow* root_window) {
  if (view_)
    view_->SetBubbleArrow(GetBubbleArrow(view_->GetWidget()->GetNativeView()));
}

void AppListPresenterDelegate::OnOverviewModeStarting() {
  if (is_visible_)
    presenter_->Dismiss();
}

void AppListPresenterDelegate::OnMaximizeModeStarted() {
  // The "fullscreen" app-list is initialized as in a different type of window,
  // therefore we can't switch between the fullscreen status and the normal
  // app-list bubble. App-list should be dismissed for the transition between
  // maximize mode (touch-view mode) and non-maximize mode, otherwise the app
  // list tries to behave as a bubble which leads to a crash. crbug.com/510062
  if (IsFullscreenAppListEnabled() && is_visible_)
    presenter_->Dismiss();
}

void AppListPresenterDelegate::OnMaximizeModeEnded() {
  // See the comments of OnMaximizeModeStarted().
  if (IsFullscreenAppListEnabled() && is_visible_)
    presenter_->Dismiss();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterDelegate, WmShelfObserver implementation:

void AppListPresenterDelegate::OnShelfIconPositionsChanged() {
  UpdateBounds();
}

}  // namespace ash
