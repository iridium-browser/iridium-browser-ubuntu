// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

// BluetoothDeviceAndroid along with the Java class
// org.chromium.device.bluetooth.BluetoothDevice implement BluetoothDevice.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceAndroid final
    : public BluetoothDevice {
 public:
  // Create a BluetoothDeviceAndroid instance and associated Java
  // ChromeBluetoothDevice using the provided |java_bluetooth_device_wrapper|.
  //
  // The ChromeBluetoothDevice instance will hold a Java reference
  // to |bluetooth_device_wrapper|.
  //
  // TODO(scheib): Return a scoped_ptr<>, but then adapter will need to handle
  // this correctly. http://crbug.com/506416
  static BluetoothDeviceAndroid* Create(
      jobject bluetooth_device_wrapper);  // Java Type: bluetoothDeviceWrapper

  ~BluetoothDeviceAndroid() override;

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJNI(JNIEnv* env);

  // Updates cached copy of advertised UUIDs discovered during a scan.
  // Returns true if new UUIDs differed from cached values.
  bool UpdateAdvertisedUUIDs(
      jobject advertised_uuids);  // Java Type: List<ParcelUuid>

  // BluetoothDevice:
  uint32 GetBluetoothClass() const override;
  std::string GetAddress() const override;
  VendorIDSource GetVendorIDSource() const override;
  uint16 GetVendorID() const override;
  uint16 GetProductID() const override;
  uint16 GetDeviceID() const override;
  bool IsPaired() const override;
  bool IsConnected() const override;
  bool IsConnectable() const override;
  bool IsConnecting() const override;
  UUIDList GetUUIDs() const override;
  int16 GetInquiryRSSI() const override;
  int16 GetInquiryTxPower() const override;
  bool ExpectingPinCode() const override;
  bool ExpectingPasskey() const override;
  bool ExpectingConfirmation() const override;
  void GetConnectionInfo(const ConnectionInfoCallback& callback) override;
  void Connect(device::BluetoothDevice::PairingDelegate* pairing_delegate,
               const base::Closure& callback,
               const ConnectErrorCallback& error_callback) override;
  void SetPinCode(const std::string& pincode) override;
  void SetPasskey(uint32 passkey) override;
  void ConfirmPairing() override;
  void RejectPairing() override;
  void CancelPairing() override;
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Forget(const ErrorCallback& error_callback) override;
  void ConnectToService(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void ConnectToServiceInsecurely(
      const device::BluetoothUUID& uuid,
      const ConnectToServiceCallback& callback,
      const ConnectToServiceErrorCallback& error_callback) override;
  void CreateGattConnection(
      const GattConnectionCallback& callback,
      const ConnectErrorCallback& error_callback) override;

 protected:
  BluetoothDeviceAndroid();

  // BluetoothDevice:
  std::string GetDeviceName() const override;

  // Java object org.chromium.device.bluetooth.ChromeBluetoothDevice.
  base::android::ScopedJavaGlobalRef<jobject> j_device_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceAndroid);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_ANDROID_H_
