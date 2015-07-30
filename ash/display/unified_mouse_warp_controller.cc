// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/unified_mouse_warp_controller.h"

#include <cmath>

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/display_util.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

AshWindowTreeHost* GetMirroringAshWindowTreeHostForDisplayId(int64 display_id) {
  return Shell::GetInstance()
      ->display_controller()
      ->mirror_window_controller()
      ->GetAshWindowTreeHostForDisplayId(display_id);
}

// Find a WindowTreeHost used for mirroring displays that contains
// the |point_in_screen|. Returns nullptr if such WTH does not exist.
aura::WindowTreeHost* FindMirroringWindowTreeHostFromScreenPoint(
    const gfx::Point& point_in_screen) {
  DisplayManager::DisplayList mirroring_display_list =
      Shell::GetInstance()
          ->display_manager()
          ->software_mirroring_display_list();
  int index =
      FindDisplayIndexContainingPoint(mirroring_display_list, point_in_screen);
  if (index < 0)
    return nullptr;
  return GetMirroringAshWindowTreeHostForDisplayId(
             mirroring_display_list[index].id())->AsWindowTreeHost();
}

}  // namespace

UnifiedMouseWarpController::UnifiedMouseWarpController()
    : allow_non_native_event_(false) {
}

UnifiedMouseWarpController::~UnifiedMouseWarpController() {
}

bool UnifiedMouseWarpController::WarpMouseCursor(ui::MouseEvent* event) {
  // Mirroring windows are created asynchronously, so compute the edge
  // beounds when we received an event instead of in constructor.
  if (first_edge_bounds_in_native_.IsEmpty())
    ComputeBounds();

  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::Point point_in_screen = event->location();
  ::wm::ConvertPointToScreen(target, &point_in_screen);

  // A native event may not exist in unit test. Generate the native point
  // from the screen point instead.
  if (!event->HasNativeEvent()) {
    if (!allow_non_native_event_)
      return false;
    aura::Window* target_root = target->GetRootWindow();
    gfx::Point point_in_native = point_in_screen;
    ::wm::ConvertPointFromScreen(target_root, &point_in_native);
    aura::WindowTreeHost* host =
        FindMirroringWindowTreeHostFromScreenPoint(point_in_screen);
    DCHECK(host);
    host->ConvertPointToNativeScreen(&point_in_native);
    return WarpMouseCursorInNativeCoords(point_in_native, point_in_screen,
                                         true);
  }

  gfx::Point point_in_native =
      ui::EventSystemLocationFromNative(event->native_event());

#if defined(USE_OZONE)
  // TODO(dnicoara): crbug.com/415680 Move cursor warping into Ozone once Ozone
  // has access to the logical display layout.
  // Native events in Ozone are in the native window coordinate system. We need
  // to translate them to get the global position.
  aura::WindowTreeHost* host =
      FindMirroringWindowTreeHostFromScreenPoint(point_in_screen);
  if (!host)
    return false;
  point_in_native.Offset(host->GetBounds().x(), host->GetBounds().y());
#endif

  return WarpMouseCursorInNativeCoords(point_in_native, point_in_screen, false);
}

void UnifiedMouseWarpController::SetEnabled(bool enabled) {
  // Mouse warp shuld be always on in Unified mode.
}

void UnifiedMouseWarpController::ComputeBounds() {
  DisplayManager::DisplayList display_list =
      Shell::GetInstance()
          ->display_manager()
          ->software_mirroring_display_list();

  if (display_list.size() < 2) {
    LOG(ERROR) << "Mirroring Display lost during re-configuration";
    return;
  }
  LOG_IF(ERROR, display_list.size() > 2) << "Only two displays are supported";

  const gfx::Display& first = display_list[0];
  const gfx::Display& second = display_list[1];
  ComputeBoundary(first, second, DisplayLayout::RIGHT,
                  &first_edge_bounds_in_native_,
                  &second_edge_bounds_in_native_);

  first_edge_bounds_in_native_ =
      GetNativeEdgeBounds(GetMirroringAshWindowTreeHostForDisplayId(first.id()),
                          first_edge_bounds_in_native_);

  second_edge_bounds_in_native_ = GetNativeEdgeBounds(
      GetMirroringAshWindowTreeHostForDisplayId(second.id()),
      second_edge_bounds_in_native_);
}

bool UnifiedMouseWarpController::WarpMouseCursorInNativeCoords(
    const gfx::Point& point_in_native,
    const gfx::Point& point_in_screen,
    bool update_mouse_location_now) {
  bool in_first_edge = first_edge_bounds_in_native_.Contains(point_in_native);
  bool in_second_edge = second_edge_bounds_in_native_.Contains(point_in_native);
  if (!in_first_edge && !in_second_edge)
    return false;
  DisplayManager::DisplayList display_list =
      Shell::GetInstance()
          ->display_manager()
          ->software_mirroring_display_list();
  AshWindowTreeHost* target_ash_host =
      GetMirroringAshWindowTreeHostForDisplayId(
          in_first_edge ? display_list[1].id() : display_list[0].id());
  MoveCursorTo(target_ash_host, point_in_screen, update_mouse_location_now);
  return true;
}

}  // namespace ash
