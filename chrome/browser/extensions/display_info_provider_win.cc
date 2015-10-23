// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/display_info_provider_win.h"

#include <windows.h>

#include "base/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "extensions/common/api/system_display.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/win/dpi.h"

namespace extensions {

using api::system_display::DisplayUnitInfo;

namespace {

BOOL CALLBACK
EnumMonitorCallback(HMONITOR monitor, HDC hdc, LPRECT rect, LPARAM data) {
  DisplayInfo* all_displays = reinterpret_cast<DisplayInfo*>(data);
  DCHECK(all_displays);

  linked_ptr<DisplayUnitInfo> unit(new DisplayUnitInfo);

  MONITORINFOEX monitor_info;
  ZeroMemory(&monitor_info, sizeof(MONITORINFOEX));
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(monitor, &monitor_info);

  DISPLAY_DEVICE device;
  device.cb = sizeof(device);
  if (!EnumDisplayDevices(monitor_info.szDevice, 0, &device, 0))
    return FALSE;

  gfx::Size dpi(gfx::GetDPI());
  unit->id =
      base::Int64ToString(base::Hash(base::WideToUTF8(monitor_info.szDevice)));
  unit->name = base::WideToUTF8(device.DeviceString);
  unit->dpi_x = dpi.width();
  unit->dpi_y = dpi.height();
  all_displays->push_back(unit);

  return TRUE;
}

}  // namespace

DisplayInfoProviderWin::DisplayInfoProviderWin() {
}

DisplayInfoProviderWin::~DisplayInfoProviderWin() {
}

bool DisplayInfoProviderWin::SetInfo(
    const std::string& display_id,
    const api::system_display::DisplayProperties& info,
    std::string* error) {
  *error = "Not implemented";
  return false;
}

void DisplayInfoProviderWin::UpdateDisplayUnitInfoForPlatform(
    const gfx::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  DisplayInfo all_displays;
  EnumDisplayMonitors(
      NULL, NULL, EnumMonitorCallback, reinterpret_cast<LPARAM>(&all_displays));
  for (size_t i = 0; i < all_displays.size(); ++i) {
    if (unit->id == all_displays[i]->id) {
      unit->name = all_displays[i]->name;
      unit->dpi_x = all_displays[i]->dpi_x;
      unit->dpi_y = all_displays[i]->dpi_y;
      break;
    }
  }
}

gfx::Screen* DisplayInfoProviderWin::GetActiveScreen() {
  // TODO(scottmg): native screen is wrong http://crbug.com/133312
  return gfx::Screen::GetNativeScreen();
}

// static
DisplayInfoProvider* DisplayInfoProvider::Create() {
  return new DisplayInfoProviderWin();
}

}  // namespace extensions
