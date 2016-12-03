// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/material_design_controller_test_api.h"

namespace ash {
namespace test {

MaterialDesignControllerTestAPI::MaterialDesignControllerTestAPI(
    MaterialDesignController::Mode mode)
    : previous_mode_(MaterialDesignController::mode()) {
  MaterialDesignController::SetMode(mode);
}

MaterialDesignControllerTestAPI::~MaterialDesignControllerTestAPI() {
  MaterialDesignController::SetMode(previous_mode_);
}

// static
bool MaterialDesignControllerTestAPI::IsMaterial() {
  return MaterialDesignController::IsMaterial();
}

// static
bool MaterialDesignControllerTestAPI::IsMaterialNormal() {
  return MaterialDesignController::IsMaterialNormal();
}

// static
bool MaterialDesignControllerTestAPI::IsMaterialExperimental() {
  return MaterialDesignController::IsMaterialExperimental();
}

// static
MaterialDesignController::Mode MaterialDesignControllerTestAPI::DefaultMode() {
  return MaterialDesignController::DefaultMode();
}

// static
void MaterialDesignControllerTestAPI::Uninitialize() {
  MaterialDesignController::Uninitialize();
}

}  // namespace test
}  // namespace ash
