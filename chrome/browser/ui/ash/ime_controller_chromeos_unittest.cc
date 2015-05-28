// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ime_controller_chromeos.h"

#include "base/basictypes.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

class ImeControllerTest : public testing::Test {
 public:
  ImeControllerTest() : mock_input_method_manager_(NULL) {}
  ~ImeControllerTest() override {}

  void SetUp() override {
    mock_input_method_manager_ =
        new chromeos::input_method::MockInputMethodManager;
    chromeos::input_method::InitializeForTesting(
        mock_input_method_manager_);
  }

  void TearDown() override {
    chromeos::input_method::Shutdown();
    mock_input_method_manager_ = NULL;
  }

 protected:
  ImeController controller_;
  chromeos::input_method::MockInputMethodManager* mock_input_method_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeControllerTest);
};

TEST_F(ImeControllerTest, TestRemapAccelerator) {
  mock_input_method_manager_->SetCurrentInputMethodId("xkb:us::eng");
  {
    ui::Accelerator accel(ui::VKEY_A, ui::EF_CONTROL_DOWN);
    ui::Accelerator remapped = controller_.RemapAccelerator(accel);
    EXPECT_EQ(accel, remapped);
    accel.set_type(ui::ET_KEY_RELEASED);
    remapped = controller_.RemapAccelerator(accel);
    EXPECT_EQ(accel, remapped);  // crbug.com/130519
  }
  mock_input_method_manager_->SetCurrentInputMethodId("xkb:fr::fra");
  {
    // Control+A shouldn't be remapped even when the current layout is FR.
    ui::Accelerator accel(ui::VKEY_A, ui::EF_CONTROL_DOWN);
    ui::Accelerator remapped = controller_.RemapAccelerator(accel);
    EXPECT_EQ(accel, remapped);
    accel.set_type(ui::ET_KEY_RELEASED);
    remapped = controller_.RemapAccelerator(accel);
    EXPECT_EQ(accel, remapped);
  }
  {
    // Shift+A shouldn't be remapped even when the current layout is FR.
    ui::Accelerator accel(ui::VKEY_A, ui::EF_SHIFT_DOWN);
    ui::Accelerator remapped = controller_.RemapAccelerator(accel);
    EXPECT_EQ(accel, remapped);
    accel.set_type(ui::ET_KEY_RELEASED);
    remapped = controller_.RemapAccelerator(accel);
    EXPECT_EQ(accel, remapped);
  }
  {
    // Shift+1 should be remapped when the current layout is FR.
    ui::Accelerator accel(ui::VKEY_1, ui::EF_SHIFT_DOWN);
    ui::Accelerator remapped = controller_.RemapAccelerator(accel);
    ui::Accelerator expected(ui::VKEY_1, 0);
    EXPECT_EQ(expected, remapped);
    accel.set_type(ui::ET_KEY_RELEASED);
    remapped = controller_.RemapAccelerator(accel);
    expected.set_type(ui::ET_KEY_RELEASED);
    EXPECT_EQ(expected, remapped);
  }
}

// Test that remapping the accelerator preserves the accelerator's IsRepeat
// property.
TEST_F(ImeControllerTest, TestRemapAcceleratorPreservesIsRepeat) {
  mock_input_method_manager_->SetCurrentInputMethodId("xkb:us::eng");
  {
    ui::Accelerator accel(ui::VKEY_A, ui::EF_CONTROL_DOWN);
    accel.set_is_repeat(true);
    ui::Accelerator remapped = controller_.RemapAccelerator(accel);
    EXPECT_EQ(accel, remapped);
    // The Accelerator's equality operator ignores whether an accelerator is
    // repeated.
    EXPECT_TRUE(remapped.IsRepeat());
  }
  {
    ui::Accelerator accel(ui::VKEY_A, ui::EF_CONTROL_DOWN);
    EXPECT_FALSE(accel.IsRepeat());
    ui::Accelerator remapped = controller_.RemapAccelerator(accel);
    EXPECT_EQ(accel, remapped);
    EXPECT_FALSE(remapped.IsRepeat());
  }
}
