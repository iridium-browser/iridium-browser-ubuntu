// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_ash.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {

namespace {

DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

gfx::Display FindDisplayNearestPoint(const std::vector<gfx::Display>& displays,
                                     const gfx::Point& point) {
  int min_distance = INT_MAX;
  const gfx::Display* nearest_display = NULL;
  for (std::vector<gfx::Display>::const_iterator iter = displays.begin();
       iter != displays.end(); ++iter) {
    const gfx::Display& display = *iter;
    int distance = display.bounds().ManhattanDistanceToPoint(point);
    if (distance < min_distance) {
      min_distance = distance;
      nearest_display = &display;
    }
  }
  // There should always be at least one display that is less than INT_MAX away.
  DCHECK(nearest_display);
  return *nearest_display;
}

const gfx::Display* FindDisplayMatching(
    const std::vector<gfx::Display>& displays,
    const gfx::Rect& match_rect) {
  int max_area = 0;
  const gfx::Display* matching = NULL;
  for (std::vector<gfx::Display>::const_iterator iter = displays.begin();
       iter != displays.end(); ++iter) {
    const gfx::Display& display = *iter;
    gfx::Rect intersect = gfx::IntersectRects(display.bounds(), match_rect);
    int area = intersect.width() * intersect.height();
    if (area > max_area) {
      max_area = area;
      matching = &display;
    }
  }
  return matching;
}

class ScreenForShutdown : public gfx::Screen {
 public:
  explicit ScreenForShutdown(ScreenAsh* screen_ash)
      : display_list_(screen_ash->GetAllDisplays()),
        primary_display_(screen_ash->GetPrimaryDisplay()) {
  }

  // gfx::Screen overrides:
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  gfx::NativeWindow GetWindowUnderCursor() override { return NULL; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return NULL;
  }
  int GetNumDisplays() const override { return display_list_.size(); }
  std::vector<gfx::Display> GetAllDisplays() const override {
    return display_list_;
  }
  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override {
    return primary_display_;
  }
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override {
    return FindDisplayNearestPoint(display_list_, point);
  }
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override {
    const gfx::Display* matching =
        FindDisplayMatching(display_list_, match_rect);
    // Fallback to the primary display if there is no matching display.
    return matching ? *matching : GetPrimaryDisplay();
  }
  gfx::Display GetPrimaryDisplay() const override { return primary_display_; }
  void AddObserver(gfx::DisplayObserver* observer) override {
    NOTREACHED() << "Observer should not be added during shutdown";
  }
  void RemoveObserver(gfx::DisplayObserver* observer) override {}

 private:
  const std::vector<gfx::Display> display_list_;
  const gfx::Display primary_display_;

  DISALLOW_COPY_AND_ASSIGN(ScreenForShutdown);
};

}  // namespace

ScreenAsh::ScreenAsh() {
}

ScreenAsh::~ScreenAsh() {
}

void ScreenAsh::NotifyMetricsChanged(const gfx::Display& display,
                                     uint32_t metrics) {
  FOR_EACH_OBSERVER(gfx::DisplayObserver,
                    observers_,
                    OnDisplayMetricsChanged(display, metrics));
}

void ScreenAsh::NotifyDisplayAdded(const gfx::Display& display) {
  FOR_EACH_OBSERVER(gfx::DisplayObserver, observers_, OnDisplayAdded(display));
}

void ScreenAsh::NotifyDisplayRemoved(const gfx::Display& display) {
  FOR_EACH_OBSERVER(
      gfx::DisplayObserver, observers_, OnDisplayRemoved(display));
}

gfx::Point ScreenAsh::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

gfx::NativeWindow ScreenAsh::GetWindowUnderCursor() {
  return GetWindowAtScreenPoint(Shell::GetScreen()->GetCursorScreenPoint());
}

gfx::NativeWindow ScreenAsh::GetWindowAtScreenPoint(const gfx::Point& point) {
  return wm::GetRootWindowAt(point)->GetTopWindowContainingPoint(point);
}

int ScreenAsh::GetNumDisplays() const {
  return GetDisplayManager()->GetNumDisplays();
}

std::vector<gfx::Display> ScreenAsh::GetAllDisplays() const {
  return GetDisplayManager()->active_display_list();
}

gfx::Display ScreenAsh::GetDisplayNearestWindow(gfx::NativeView window) const {
  if (!window)
    return GetPrimaryDisplay();
  const aura::Window* root_window = window->GetRootWindow();
  if (!root_window)
    return GetPrimaryDisplay();
  const RootWindowSettings* rws = GetRootWindowSettings(root_window);
  int64 id = rws->display_id;
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != gfx::Display::kInvalidDisplayID);
  if (id == gfx::Display::kInvalidDisplayID)
    return GetPrimaryDisplay();

  DisplayManager* display_manager = GetDisplayManager();
  // RootWindow needs Display to determine its device scale factor
  // for non desktop display.
  gfx::Display mirroring_display = display_manager->GetMirroringDisplayById(id);
  if (mirroring_display.is_valid())
    return mirroring_display;
  return display_manager->GetDisplayForId(id);
}

gfx::Display ScreenAsh::GetDisplayNearestPoint(const gfx::Point& point) const {
  const gfx::Display& display =
      GetDisplayManager()->FindDisplayContainingPoint(point);
  if (display.is_valid())
    return display;
  // Fallback to the display that has the shortest Manhattan distance from
  // the |point|. This is correct in the only areas that matter, namely in the
  // corners between the physical screens.
  return FindDisplayNearestPoint(GetDisplayManager()->active_display_list(),
                                 point);
}

gfx::Display ScreenAsh::GetDisplayMatching(const gfx::Rect& match_rect) const {
  if (match_rect.IsEmpty())
    return GetDisplayNearestPoint(match_rect.origin());
  const gfx::Display* matching = FindDisplayMatching(
      GetDisplayManager()->active_display_list(), match_rect);
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : GetPrimaryDisplay();
}

gfx::Display ScreenAsh::GetPrimaryDisplay() const {
  return GetDisplayManager()->GetDisplayForId(
      DisplayController::GetPrimaryDisplayId());
}

void ScreenAsh::AddObserver(gfx::DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void ScreenAsh::RemoveObserver(gfx::DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::Screen* ScreenAsh::CloneForShutdown() {
  return new ScreenForShutdown(this);
}

}  // namespace ash
