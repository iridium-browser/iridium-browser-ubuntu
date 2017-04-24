// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SENSORS_SENSOR_MANAGER_CHROMEOS_H_
#define DEVICE_SENSORS_SENSOR_MANAGER_CHROMEOS_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "device/sensors/device_sensor_export.h"
#include "device/sensors/public/cpp/device_motion_hardware_buffer.h"
#include "device/sensors/public/cpp/device_orientation_hardware_buffer.h"

namespace device {

// Observes Chrome OS accelerometer sensors, and provides updated device
// orientation information.
class DEVICE_SENSOR_EXPORT SensorManagerChromeOS
    : public chromeos::AccelerometerReader::Observer {
 public:
  SensorManagerChromeOS();
  ~SensorManagerChromeOS() override;

  // Begins monitoring of motion events, the shared memory of |buffer| will be
  // updated upon subsequent events.
  void StartFetchingDeviceMotionData(DeviceMotionHardwareBuffer* buffer);

  // Stops monitoring motion events. Returns true if there is an active
  // |motion_buffer_| and fetching stops. Otherwise returns false.
  bool StopFetchingDeviceMotionData();

  // Begins monitoring of orientation events, the shared memory of |buffer| will
  // be updated upon subsequent events.
  void StartFetchingDeviceOrientationData(
      DeviceOrientationHardwareBuffer* buffer);

  // Stops monitoring orientation events. Returns true if there is an active
  // |orientation_buffer_| and fetching stops. Otherwise returns false.
  bool StopFetchingDeviceOrientationData();

  // chromeos::AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      scoped_refptr<const chromeos::AccelerometerUpdate> update) override;

 protected:
  // Begins/ends the observation of accelerometer events.
  virtual void StartObservingAccelerometer();
  virtual void StopObservingAccelerometer();

 private:
  // Updates |motion_buffer_| or |orientation_buffer_| accordingly.
  void GenerateMotionEvent(double x, double y, double z);
  void GenerateOrientationEvent(double x, double y, double z);

  // Shared memory to update.
  DeviceMotionHardwareBuffer* motion_buffer_;
  DeviceOrientationHardwareBuffer* orientation_buffer_;

  // Verify all work is done on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SensorManagerChromeOS);
};

}  // namespace device

#endif  // DEVICE_SENSORS_SENSOR_MANAGER_CHROMEOS_H_
