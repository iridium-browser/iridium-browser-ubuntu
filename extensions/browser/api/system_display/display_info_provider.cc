// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/display_info_provider.h"

#include "base/strings/string_number_conversions.h"
#include "extensions/common/api/system_display.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace extensions {

namespace {

// Created on demand and will leak when the process exits.
DisplayInfoProvider* g_display_info_provider = NULL;

// Converts Rotation enum to integer.
int RotationToDegrees(gfx::Display::Rotation rotation) {
  switch (rotation) {
    case gfx::Display::ROTATE_0:
      return 0;
    case gfx::Display::ROTATE_90:
      return 90;
    case gfx::Display::ROTATE_180:
      return 180;
    case gfx::Display::ROTATE_270:
      return 270;
  }
  return 0;
}

// Creates new DisplayUnitInfo struct for |display|.
api::system_display::DisplayUnitInfo* CreateDisplayUnitInfo(
    const gfx::Display& display,
    int64 primary_display_id) {
  api::system_display::DisplayUnitInfo* unit =
      new api::system_display::DisplayUnitInfo();
  const gfx::Rect& bounds = display.bounds();
  const gfx::Rect& work_area = display.work_area();
  unit->id = base::Int64ToString(display.id());
  unit->is_primary = (display.id() == primary_display_id);
  unit->is_internal = display.IsInternal();
  unit->is_enabled = true;
  unit->rotation = RotationToDegrees(display.rotation());
  unit->bounds.left = bounds.x();
  unit->bounds.top = bounds.y();
  unit->bounds.width = bounds.width();
  unit->bounds.height = bounds.height();
  unit->work_area.left = work_area.x();
  unit->work_area.top = work_area.y();
  unit->work_area.width = work_area.width();
  unit->work_area.height = work_area.height();
  return unit;
}

}  // namespace

DisplayInfoProvider::~DisplayInfoProvider() {
}

// static
DisplayInfoProvider* DisplayInfoProvider::Get() {
  if (g_display_info_provider == NULL)
    g_display_info_provider = DisplayInfoProvider::Create();
  return g_display_info_provider;
}

// static
void DisplayInfoProvider::InitializeForTesting(
    DisplayInfoProvider* display_info_provider) {
  DCHECK(display_info_provider);
  g_display_info_provider = display_info_provider;
}

void DisplayInfoProvider::EnableUnifiedDesktop(bool enable) {}

DisplayInfo DisplayInfoProvider::GetAllDisplaysInfo() {
  // TODO(scottmg): Native is wrong http://crbug.com/133312
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  int64 primary_id = screen->GetPrimaryDisplay().id();
  std::vector<gfx::Display> displays = screen->GetAllDisplays();
  DisplayInfo all_displays;
  for (const gfx::Display& display : displays) {
    linked_ptr<api::system_display::DisplayUnitInfo> unit(
        CreateDisplayUnitInfo(display, primary_id));
    UpdateDisplayUnitInfoForPlatform(display, unit.get());
    all_displays.push_back(unit);
  }
  return all_displays;
}

DisplayInfoProvider::DisplayInfoProvider() {
}

}  // namespace extensions
