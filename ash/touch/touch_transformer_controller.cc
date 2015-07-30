// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_transformer_controller.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/devices/device_data_manager.h"

namespace ash {

namespace {

DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

ui::TouchscreenDevice FindTouchscreenById(int id) {
  const std::vector<ui::TouchscreenDevice>& touchscreens =
      ui::DeviceDataManager::GetInstance()->touchscreen_devices();
  for (const auto& touchscreen : touchscreens) {
    if (touchscreen.id == id)
      return touchscreen;
  }

  return ui::TouchscreenDevice();
}

}  // namespace

// This is to compute the scale ratio for the TouchEvent's radius. The
// configured resolution of the display is not always the same as the touch
// screen's reporting resolution, e.g. the display could be set as
// 1920x1080 while the touchscreen is reporting touch position range at
// 32767x32767. Touch radius is reported in the units the same as touch position
// so we need to scale the touch radius to be compatible with the display's
// resolution. We compute the scale as
// sqrt of (display_area / touchscreen_area)
double TouchTransformerController::GetTouchResolutionScale(
    const DisplayInfo& touch_display,
    const ui::TouchscreenDevice& touch_device) const {
  if (touch_device.id == ui::InputDevice::kInvalidId ||
      touch_device.size.IsEmpty() ||
      touch_display.bounds_in_native().size().IsEmpty())
    return 1.0;

  double display_area = touch_display.bounds_in_native().size().GetArea();
  double touch_area = touch_device.size.GetArea();
  double ratio = std::sqrt(display_area / touch_area);

  VLOG(2) << "Display size: "
          << touch_display.bounds_in_native().size().ToString()
          << ", Touchscreen size: " << touch_device.size.ToString()
          << ", Touch radius scale ratio: " << ratio;
  return ratio;
}

gfx::Transform TouchTransformerController::GetTouchTransform(
    const DisplayInfo& display,
    const DisplayInfo& touch_display,
    const ui::TouchscreenDevice& touchscreen,
    const gfx::Size& framebuffer_size) const {
  gfx::SizeF current_size = display.bounds_in_native().size();
  gfx::SizeF touch_native_size = touch_display.GetNativeModeSize();
#if defined(USE_OZONE)
  gfx::SizeF touch_area = touchscreen.size;
#elif defined(USE_X11)
  // On X11 touches are reported in the framebuffer coordinate space.
  gfx::SizeF touch_area = framebuffer_size;
#endif

  gfx::Transform ctm;

  if (current_size.IsEmpty() || touch_native_size.IsEmpty() ||
      touch_area.IsEmpty() || touchscreen.id == ui::InputDevice::kInvalidId)
    return ctm;

#if defined(USE_OZONE)
  // Translate the touch so that it falls within the display bounds.
  ctm.Translate(display.bounds_in_native().x(),
                display.bounds_in_native().y());
#endif

  // Take care of panel fitting only if supported. Panel fitting is emulated in
  // software mirroring mode (display != touch_display).
  // If panel fitting is enabled then the aspect ratio is preserved and the
  // display is scaled acordingly. In this case blank regions would be present
  // in order to center the displayed area.
  if (display.is_aspect_preserving_scaling() ||
      display.id() != touch_display.id()) {
    float touch_native_ar =
        touch_native_size.width() / touch_native_size.height();
    float current_ar = current_size.width() / current_size.height();

    if (current_ar > touch_native_ar) {  // Letterboxing
      ctm.Translate(
          0, (1 - current_ar / touch_native_ar) * 0.5 * current_size.height());
      ctm.Scale(1, current_ar / touch_native_ar);
    } else if (touch_native_ar > current_ar) {  // Pillarboxing
      ctm.Translate(
          (1 - touch_native_ar / current_ar) * 0.5 * current_size.width(), 0);
      ctm.Scale(touch_native_ar / current_ar, 1);
    }
  }

  // Take care of scaling between touchscreen area and display resolution.
  ctm.Scale(current_size.width() / touch_area.width(),
            current_size.height() / touch_area.height());
  return ctm;
}

TouchTransformerController::TouchTransformerController() {
  Shell::GetInstance()->display_controller()->AddObserver(this);
}

TouchTransformerController::~TouchTransformerController() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

void TouchTransformerController::UpdateTouchRadius(
    const DisplayInfo& display) const {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  device_manager->UpdateTouchRadiusScale(
      display.touch_device_id(),
      GetTouchResolutionScale(display,
                              FindTouchscreenById(display.touch_device_id())));
}

void TouchTransformerController::UpdateTouchTransform(
    int64_t target_display_id,
    const DisplayInfo& touch_display,
    const DisplayInfo& target_display) const {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  gfx::Size fb_size =
      Shell::GetInstance()->display_configurator()->framebuffer_size();
  device_manager->UpdateTouchInfoForDisplay(
      target_display_id, touch_display.touch_device_id(),
      GetTouchTransform(target_display, touch_display,
                        FindTouchscreenById(touch_display.touch_device_id()),
                        fb_size));
}

void TouchTransformerController::UpdateTouchTransformer() const {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  device_manager->ClearTouchDeviceAssociations();

  // Display IDs and DisplayInfo for mirror or extended mode.
  int64 display1_id = gfx::Display::kInvalidDisplayID;
  int64 display2_id = gfx::Display::kInvalidDisplayID;
  DisplayInfo display1;
  DisplayInfo display2;
  // Display ID and DisplayInfo for single display mode.
  int64 single_display_id = gfx::Display::kInvalidDisplayID;
  DisplayInfo single_display;

  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->num_connected_displays() == 0) {
    return;
  } else if (display_manager->num_connected_displays() == 1 ||
             display_manager->IsInUnifiedMode()) {
    single_display_id = display_manager->first_display_id();
    DCHECK(single_display_id != gfx::Display::kInvalidDisplayID);
    single_display = display_manager->GetDisplayInfo(single_display_id);
    UpdateTouchRadius(single_display);
  } else {
    DisplayIdPair id_pair = display_manager->GetCurrentDisplayIdPair();
    display1_id = id_pair.first;
    display2_id = id_pair.second;
    DCHECK(display1_id != gfx::Display::kInvalidDisplayID &&
           display2_id != gfx::Display::kInvalidDisplayID);
    display1 = display_manager->GetDisplayInfo(display1_id);
    display2 = display_manager->GetDisplayInfo(display2_id);
    UpdateTouchRadius(display1);
    UpdateTouchRadius(display2);
  }

  gfx::Size fb_size =
      Shell::GetInstance()->display_configurator()->framebuffer_size();

  if (display_manager->IsInMirrorMode()) {
    int64_t primary_display_id = display_controller->GetPrimaryDisplayId();
    if (GetDisplayManager()->SoftwareMirroringEnabled()) {
      // In extended but software mirroring mode, there is a WindowTreeHost for
      // each display, but all touches are forwarded to the primary root
      // window's WindowTreeHost.
      DisplayInfo target_display =
          primary_display_id == display1_id ? display1 : display2;
      UpdateTouchTransform(target_display.id(), display1, target_display);
      UpdateTouchTransform(target_display.id(), display2, target_display);
    } else {
      // In mirror mode, there is just one WindowTreeHost and two displays. Make
      // the WindowTreeHost accept touch events from both displays.
      UpdateTouchTransform(primary_display_id, display1, display1);
      UpdateTouchTransform(primary_display_id, display2, display2);
    }
    return;
  }

  if (display_manager->num_connected_displays() > 1) {
    // In actual extended mode, each display is associated with one
    // WindowTreeHost.
    UpdateTouchTransform(display1_id, display1, display1);
    UpdateTouchTransform(display2_id, display2, display2);
    return;
  }

  // Single display mode. The WindowTreeHost has one associated display id.
  UpdateTouchTransform(single_display_id, single_display, single_display);
}

void TouchTransformerController::OnDisplaysInitialized() {
  UpdateTouchTransformer();
}

void TouchTransformerController::OnDisplayConfigurationChanged() {
  UpdateTouchTransformer();
}

}  // namespace ash
