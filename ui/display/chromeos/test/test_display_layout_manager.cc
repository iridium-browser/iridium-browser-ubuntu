// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/test/test_display_layout_manager.h"

#include "ui/display/types/display_snapshot.h"

namespace ui {
namespace test {

TestDisplayLayoutManager::TestDisplayLayoutManager(
    ScopedVector<DisplaySnapshot> displays,
    MultipleDisplayState display_state)
    : displays_(displays.Pass()), display_state_(display_state) {
}

TestDisplayLayoutManager::~TestDisplayLayoutManager() {
}

DisplayConfigurator::StateController*
TestDisplayLayoutManager::GetStateController() const {
  return nullptr;
}

DisplayConfigurator::SoftwareMirroringController*
TestDisplayLayoutManager::GetSoftwareMirroringController() const {
  return nullptr;
}

MultipleDisplayState TestDisplayLayoutManager::GetDisplayState() const {
  return display_state_;
}

chromeos::DisplayPowerState TestDisplayLayoutManager::GetPowerState() const {
  NOTREACHED();
  return chromeos::DISPLAY_POWER_ALL_ON;
}

bool TestDisplayLayoutManager::GetDisplayLayout(
    const std::vector<DisplaySnapshot*>& displays,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state,
    std::vector<DisplayConfigureRequest>* requests,
    gfx::Size* framebuffer_size) const {
  NOTREACHED();
  return false;
}

std::vector<DisplaySnapshot*> TestDisplayLayoutManager::GetDisplayStates()
    const {
  return displays_.get();
}

bool TestDisplayLayoutManager::IsMirroring() const {
  return display_state_ == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR;
}

}  // namespace test
}  // namespace ui
