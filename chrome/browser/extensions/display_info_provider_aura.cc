// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/display_info_provider_aura.h"

#include "ui/gfx/screen.h"

namespace extensions {

DisplayInfoProviderAura::DisplayInfoProviderAura() {
}

DisplayInfoProviderAura::~DisplayInfoProviderAura() {
}

bool DisplayInfoProviderAura::SetInfo(
    const std::string& display_id,
    const api::system_display::DisplayProperties& info,
    std::string* error) {
  *error = "Not implemented";
  return false;
}

void DisplayInfoProviderAura::UpdateDisplayUnitInfoForPlatform(
    const gfx::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  static bool logged_once = false;
  if (!logged_once) {
    NOTIMPLEMENTED();
    logged_once = true;
  }
}

gfx::Screen* DisplayInfoProviderAura::GetActiveScreen() {
  return gfx::Screen::GetNativeScreen();
}

// static
DisplayInfoProvider* DisplayInfoProvider::Create() {
  return new DisplayInfoProviderAura();
}

}  // namespace extensions
