// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/display/display_info.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/touch/touchscreen_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"

namespace ash {

class TouchscreenUtilTest : public test::AshTestBase {
 public:
  TouchscreenUtilTest() {}
  ~TouchscreenUtilTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    // Internal display will always match to internal touchscreen. If internal
    // touchscreen can't be detected, it is then associated to a touch screen
    // with matching size.
    {
      DisplayInfo display(1, std::string(), false);
      DisplayMode mode(gfx::Size(1920, 1080), 60.0, false, true);
      mode.native = true;
      std::vector<DisplayMode> modes(1, mode);
      display.SetDisplayModes(modes);
      displays_.push_back(display);
    }

    {
      DisplayInfo display(2, std::string(), false);
      DisplayMode mode(gfx::Size(800, 600), 60.0, false, true);
      mode.native = true;
      std::vector<DisplayMode> modes(1, mode);
      display.SetDisplayModes(modes);
      displays_.push_back(display);
    }

    // Display without native mode. Must not be matched to any touch screen.
    {
      DisplayInfo display(3, std::string(), false);
      displays_.push_back(display);
    }

    {
      DisplayInfo display(4, std::string(), false);
      DisplayMode mode(gfx::Size(1024, 768), 60.0, false, true);
      mode.native = true;
      std::vector<DisplayMode> modes(1, mode);
      display.SetDisplayModes(modes);
      displays_.push_back(display);
    }
  }

  void TearDown() override {
    displays_.clear();
    test::AshTestBase::TearDown();
  }

 protected:
  std::vector<DisplayInfo> displays_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchscreenUtilTest);
};

TEST_F(TouchscreenUtilTest, NoTouchscreens) {
  std::vector<ui::TouchscreenDevice> devices;

  test::ScopedSetInternalDisplayId set_internal(displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  for (size_t i = 0; i < displays_.size(); ++i)
    EXPECT_EQ(0u, displays_[i].input_devices().size());
}

TEST_F(TouchscreenUtilTest, OneToOneMapping) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(800, 600), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(1024, 768), 0));

  test::ScopedSetInternalDisplayId set_internal(displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].input_devices().size());
  EXPECT_EQ(1u, displays_[1].input_devices().size());
  EXPECT_EQ(1, displays_[1].input_devices()[0]);
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
  EXPECT_EQ(2, displays_[3].input_devices()[0]);
}

TEST_F(TouchscreenUtilTest, MapToCorrectDisplaySize) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(1024, 768), 0));

  test::ScopedSetInternalDisplayId set_internal(displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].input_devices().size());
  EXPECT_EQ(0u, displays_[1].input_devices().size());
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
  EXPECT_EQ(2, displays_[3].input_devices()[0]);
}

TEST_F(TouchscreenUtilTest, MapWhenSizeDiffersByOne) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(801, 600), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(1023, 768), 0));

  test::ScopedSetInternalDisplayId set_internal(displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].input_devices().size());
  EXPECT_EQ(1u, displays_[1].input_devices().size());
  EXPECT_EQ(1, displays_[1].input_devices()[0]);
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
  EXPECT_EQ(2, displays_[3].input_devices()[0]);
}

TEST_F(TouchscreenUtilTest, MapWhenSizesDoNotMatch) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(1022, 768), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(802, 600), 0));

  test::ScopedSetInternalDisplayId set_internal(displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(0u, displays_[0].input_devices().size());
  EXPECT_EQ(1u, displays_[1].input_devices().size());
  EXPECT_EQ(1, displays_[1].input_devices()[0]);
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
  EXPECT_EQ(2, displays_[3].input_devices()[0]);
}

TEST_F(TouchscreenUtilTest, MapInternalTouchscreen) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "",
                            gfx::Size(9999, 888), 0));

  test::ScopedSetInternalDisplayId set_internal(displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  // Internal touchscreen is always mapped to internal display.
  EXPECT_EQ(1u, displays_[0].input_devices().size());
  EXPECT_EQ(2, displays_[0].input_devices()[0]);
  EXPECT_EQ(1u, displays_[1].input_devices().size());
  EXPECT_EQ(1, displays_[1].input_devices()[0]);
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(0u, displays_[3].input_devices().size());
}

TEST_F(TouchscreenUtilTest, MultipleInternal) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "",
                            gfx::Size(1920, 1080), 0));

  test::ScopedSetInternalDisplayId set_internal(displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(2u, displays_[0].input_devices().size());
  EXPECT_EQ(0u, displays_[1].input_devices().size());
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(0u, displays_[3].input_devices().size());
}

TEST_F(TouchscreenUtilTest, MultipleInternalAndExternal) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(1024, 768), 0));

  test::ScopedSetInternalDisplayId set_internal(displays_[0].id());
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(2u, displays_[0].input_devices().size());
  EXPECT_EQ(0u, displays_[1].input_devices().size());
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(1u, displays_[3].input_devices().size());
}

// crbug.com/515201
TEST_F(TouchscreenUtilTest, TestWithNoInternalDisplay) {
  std::vector<ui::TouchscreenDevice> devices;
  devices.push_back(
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_EXTERNAL, "",
                            gfx::Size(1920, 1080), 0));
  devices.push_back(
      ui::TouchscreenDevice(2, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "",
                            gfx::Size(9999, 888), 0));

  // Internal touchscreen should not be associated with any display
  AssociateTouchscreens(&displays_, devices);

  EXPECT_EQ(1u, displays_[0].input_devices().size());
  EXPECT_EQ(1, displays_[0].input_devices()[0]);
  EXPECT_EQ(0u, displays_[1].input_devices().size());
  EXPECT_EQ(0u, displays_[2].input_devices().size());
  EXPECT_EQ(0u, displays_[3].input_devices().size());
}

}  // namespace ash
