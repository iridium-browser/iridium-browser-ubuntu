// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothAdapter;

// A test fixture for Bluetooth that abstracts platform specifics for creating
// and controlling fake low level objects.
//
// Subclasses on each platform implement this, and are then typedef-ed to
// BluetoothTest.
class BluetoothTestBase : public testing::Test {
 public:
  static const std::string kTestAdapterName;
  static const std::string kTestAdapterAddress;

  static const std::string kTestDeviceName;
  static const std::string kTestDeviceNameEmpty;

  static const std::string kTestDeviceAddress1;
  static const std::string kTestDeviceAddress2;

  static const std::string kTestUUIDGenericAccess;
  static const std::string kTestUUIDGenericAttribute;
  static const std::string kTestUUIDImmediateAlert;
  static const std::string kTestUUIDLinkLoss;

  BluetoothTestBase();
  ~BluetoothTestBase() override;

  // Check if Low Energy is available. On Mac, we require OS X >= 10.10.
  virtual bool PlatformSupportsLowEnergy() = 0;

  // Initializes the BluetoothAdapter |adapter_| with the system adapter.
  virtual void InitWithDefaultAdapter(){};

  // Initializes the BluetoothAdapter |adapter_| with the system adapter forced
  // to be ignored as if it did not exist. This enables tests for when an
  // adapter is not present on the system.
  virtual void InitWithoutDefaultAdapter(){};

  // Initializes the BluetoothAdapter |adapter_| with a fake adapter that can be
  // controlled by this test fixture.
  virtual void InitWithFakeAdapter(){};

  // Create a fake Low Energy device and discover it.
  // |device_ordinal| selects between multiple fake device data sets to produce:
  //   1: kTestDeviceName with advertised UUIDs kTestUUIDGenericAccess,
  //      kTestUUIDGenericAttribute and address kTestDeviceAddress1.
  //   2: kTestDeviceName with advertised UUIDs kTestUUIDImmediateAlert,
  //      kTestUUIDLinkLoss and address kTestDeviceAddress1.
  //   3: kTestDeviceNameEmpty with no advertised UUIDs and address
  //      kTestDeviceAddress1.
  //   4: kTestDeviceNameEmpty with no advertised UUIDs and address
  //      kTestDeviceAddress2.
  virtual void DiscoverLowEnergyDevice(int device_ordinal){};

  // Callbacks that increment |callback_count_|, |error_callback_count_|:
  void Callback();
  void DiscoverySessionCallback(scoped_ptr<BluetoothDiscoverySession>);
  void ErrorCallback();

  // Accessors to get callbacks bound to this fixture:
  base::Closure GetCallback();
  BluetoothAdapter::DiscoverySessionCallback GetDiscoverySessionCallback();
  BluetoothAdapter::ErrorCallback GetErrorCallback();

  // A Message loop is required by some implementations that will PostTasks and
  // by base::RunLoop().RunUntilIdle() use in this fixuture.
  base::MessageLoop message_loop_;

  scoped_refptr<BluetoothAdapter> adapter_;
  ScopedVector<BluetoothDiscoverySession> discovery_sessions_;
  int callback_count_ = 0;
  int error_callback_count_ = 0;
  bool run_message_loop_to_wait_for_callbacks_ = true;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_
