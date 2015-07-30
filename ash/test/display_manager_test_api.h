// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_DISPLAY_MANAGER_TEST_API_H_
#define ASH_TEST_DISPLAY_MANAGER_TEST_API_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "ui/display/types/display_constants.h"

namespace gfx {
class Point;
}

namespace ui {
namespace test {
class EventGenerator;
}
}

namespace ash {
class DisplayManager;

namespace test {

class DisplayManagerTestApi {
 public:
  // Test if moving a mouse to |point_in_screen| warps it to another
  // display.
  static bool TestIfMouseWarpsAt(ui::test::EventGenerator& event_generator,
                                 const gfx::Point& point_in_screen);

  static void EnableUnifiedDesktopForTest();

  explicit DisplayManagerTestApi(DisplayManager* display_manager);
  virtual ~DisplayManagerTestApi();

  // Update the display configuration as given in |display_specs|. The format of
  // |display_spec| is a list of comma separated spec for each displays. Please
  // refer to the comment in |ash::DisplayInfo::CreateFromSpec| for the format
  // of the display spec.
  void UpdateDisplay(const std::string& display_specs);

  // Set the 1st display as an internal display and returns the display Id for
  // the internal display.
  int64 SetFirstDisplayAsInternalDisplay();

  // Sets the display id for internal display and
  // update the display mode list if necessary.
  void SetInternalDisplayId(int64 id);

  // Don't update the display when the root window's size was changed.
  void DisableChangeDisplayUponHostResize();

  // Sets the available color profiles for |display_id|.
  void SetAvailableColorProfiles(
      int64 display_id,
      const std::vector<ui::ColorCalibrationProfile>& profiles);

 private:
  DisplayManager* display_manager_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(DisplayManagerTestApi);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_DISPLAY_MANAGER_TEST_API_H_
