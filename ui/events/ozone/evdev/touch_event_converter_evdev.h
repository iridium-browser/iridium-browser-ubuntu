// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_

#include <bitset>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"

namespace ui {

class DeviceEventDispatcherEvdev;
class TouchEvent;
class TouchNoiseFinder;
struct InProgressTouchEvdev;

class EVENTS_OZONE_EVDEV_EXPORT TouchEventConverterEvdev
    : public EventConverterEvdev {
 public:
  TouchEventConverterEvdev(int fd,
                           base::FilePath path,
                           int id,
                           InputDeviceType type,
                           const EventDeviceInfo& devinfo,
                           DeviceEventDispatcherEvdev* dispatcher);
  ~TouchEventConverterEvdev() override;

  // EventConverterEvdev:
  bool HasTouchscreen() const override;
  gfx::Size GetTouchscreenSize() const override;
  int GetTouchPoints() const override;
  void OnStopped() override;

  // Unsafe part of initialization.
  virtual void Initialize(const EventDeviceInfo& info);

 private:
  friend class MockTouchEventConverterEvdev;

  // Overidden from base::MessagePumpLibevent::Watcher.
  void OnFileCanReadWithoutBlocking(int fd) override;

  virtual bool Reinitialize();

  void ProcessMultitouchEvent(const input_event& input);
  void EmulateMultitouchEvent(const input_event& input);
  void ProcessKey(const input_event& input);
  void ProcessAbs(const input_event& input);
  void ProcessSyn(const input_event& input);

  // Returns an EventType to dispatch for |touch|. Returns ET_UNKNOWN if an
  // event should not be dispatched.
  EventType GetEventTypeForTouch(const InProgressTouchEvdev& touch);

  void ReportEvent(const InProgressTouchEvdev& event,
                   EventType event_type,
                   const base::TimeDelta& delta);
  void ReportEvents(base::TimeDelta delta);

  void UpdateTrackingId(int slot, int tracking_id);
  void ReleaseTouches();

  // Normalize pressure value to [0, 1].
  float ScalePressure(int32_t value);

  int NextTrackingId();

  // Dispatcher for events.
  DeviceEventDispatcherEvdev* dispatcher_;

  // Set if we have seen a SYN_DROPPED and not yet re-synced with the device.
  bool syn_dropped_;

  // Device has multitouch capability.
  bool has_mt_;

  // Use BTN_LEFT instead of BT_TOUCH.
  bool quirk_left_mouse_button_ = false;

  // Pressure values.
  int pressure_min_;
  int pressure_max_;  // Used to normalize pressure values.

  // Input range for x-axis.
  float x_min_tuxels_;
  float x_num_tuxels_;

  // Input range for y-axis.
  float y_min_tuxels_;
  float y_num_tuxels_;

  // Number of touch points reported by driver
  int touch_points_;

  // Tracking id counter.
  int next_tracking_id_;

  // Touch point currently being updated from the /dev/input/event* stream.
  size_t current_slot_;

  // In-progress touch points.
  std::vector<InProgressTouchEvdev> events_;

  // Finds touch noise.
  scoped_ptr<TouchNoiseFinder> touch_noise_finder_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventConverterEvdev);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_TOUCH_EVENT_CONVERTER_EVDEV_H_
