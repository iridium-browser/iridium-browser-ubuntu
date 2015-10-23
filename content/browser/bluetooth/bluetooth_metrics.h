// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_

#include <string>
#include <vector>

namespace base {
class TimeDelta;
}

namespace device {
class BluetoothUUID;
}

namespace content {
struct BluetoothScanFilter;

// General Metrics

// Enumaration of each Web Bluetooth API entry point.
enum class UMAWebBluetoothFunction {
  REQUEST_DEVICE = 0,
  CONNECT_GATT = 1,
  GET_PRIMARY_SERVICE = 2,
  GET_CHARACTERISTIC = 3,
  CHARACTERISTIC_READ_VALUE = 4,
  CHARACTERISTIC_WRITE_VALUE = 5,
  // NOTE: Add new actions immediately above this line. Make sure to update
  // the enum list in tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};

// There should be a call to this function for every call to the Web Bluetooth
// API.
void RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction function);

// requestDevice() Metrics
enum class UMARequestDeviceOutcome {
  SUCCESS = 0,
  NO_BLUETOOTH_ADAPTER = 1,
  NO_RENDER_FRAME = 2,
  DISCOVERY_START_FAILED = 3,
  DISCOVERY_STOP_FAILED = 4,
  NO_MATCHING_DEVICES_FOUND = 5,
  BLUETOOTH_ADAPTER_NOT_PRESENT = 6,
  BLUETOOTH_ADAPTER_OFF = 7,
  // NOTE: Add new requestDevice() outcomes immediately above this line. Make
  // sure to update the enum list in
  // tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};
// There should be a call to this function before every
// Send(BluetoothMsg_RequestDeviceSuccess...) or
// Send(BluetoothMsg_RequestDeviceError...).
void RecordRequestDeviceOutcome(UMARequestDeviceOutcome outcome);

// Records stats about the arguments used when calling requestDevice.
//  - The number of filters used.
//  - The size of each filter.
//  - UUID of the services used in filters.
//  - Number of optional services used.
//  - UUID of the optional services.
//  - Size of the union of all services.
void RecordRequestDeviceArguments(
    const std::vector<content::BluetoothScanFilter>& filters,
    const std::vector<device::BluetoothUUID>& optional_services);

// connectGATT() Metrics
enum class UMAConnectGATTOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  UNKNOWN = 2,
  IN_PROGRESS = 3,
  FAILED = 4,
  AUTH_FAILED = 5,
  AUTH_CANCELED = 6,
  AUTH_REJECTED = 7,
  AUTH_TIMEOUT = 8,
  UNSUPPORTED_DEVICE = 9,
  // Note: Add new ConnectGATT outcomes immediately above this line. Make sure
  // to update the enum list in tools/metrics/histograms/histograms.xml
  // accordingly.
  COUNT
};
// There should be a call to this function before every
// Send(BluetoothMsg_ConnectGATTSuccess) and
// Send(BluetoothMsg_ConnectGATTError).
void RecordConnectGATTOutcome(UMAConnectGATTOutcome outcome);
// Records how long it took for the connection to succeed.
void RecordConnectGATTTimeSuccess(const base::TimeDelta& duration);
// Records how long it took for the connection to fail.
void RecordConnectGATTTimeFailed(const base::TimeDelta& duration);

// getPrimaryService() Metrics
enum class UMAGetPrimaryServiceOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NOT_FOUND = 2,
  // Note: Add new GetPrimaryService outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};
// Record the service uuid used when calling getPrimaryService.
void RecordGetPrimaryServiceService(const device::BluetoothUUID& service);
// There should be a call to this function for every call to
// Send(BluetoothMsg_GetPrimaryServiceSuccess) and
// Send(BluetoothMsg_GetPrimaryServiceError).
void RecordGetPrimaryServiceOutcome(UMAGetPrimaryServiceOutcome outcome);

// getCharacteristic() Metrics
enum class UMAGetCharacteristicOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NO_SERVICE = 2,
  NOT_FOUND = 3,
  // Note: Add new outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrisc/histogram/histograms.xml accordingly.
  COUNT
};
// There should be a call to this function for every call to
// Send(BluetoothMsg_GetCharacteristicSuccess) and
// Send(BluetoothMsg_GetCharacteristicError).
void RecordGetCharacteristicOutcome(UMAGetCharacteristicOutcome outcome);
// Records the UUID of the characteristic used when calling getCharacteristic.
void RecordGetCharacteristicCharacteristic(const std::string& characteristic);

// GATT Operations Metrics

// These are the possible outcomes when performing GATT operations i.e.
// characteristic.readValue/writeValue descriptor.readValue/writeValue.
enum UMAGATTOperationOutcome {
  SUCCESS = 0,
  NO_DEVICE = 1,
  NO_SERVICE = 2,
  NO_CHARACTERISTIC = 3,
  NO_DESCRIPTOR = 4,
  UNKNOWN = 5,
  FAILED = 6,
  IN_PROGRESS = 7,
  INVALID_LENGTH = 8,
  NOT_PERMITTED = 9,
  NOT_AUTHORIZED = 10,
  NOT_PAIRED = 11,
  NOT_SUPPORTED = 12,
  // Note: Add new GATT Outcomes immediately above this line.
  // Make sure to update the enum list in
  // tools/metrics/histograms/histograms.xml accordingly.
  COUNT
};

enum class UMAGATTOperation {
  CHARACTERISTIC_READ,
  CHARACTERISTIC_WRITE,
  // Note: Add new GATT Operations immediately above this line.
  COUNT
};

// Records the outcome of a GATT operation.
// There should be a call to this function whenever the corresponding operation
// doesn't have a call to Record[Operation]Outcome.
void RecordGATTOperationOutcome(UMAGATTOperation operation,
                                UMAGATTOperationOutcome outcome);

// Characteristic.readValue() Metrics
// There should be a call to this function for every call to
// Send(BluetoothMsg_ReadCharacteristicValueSuccess) and
// Send(BluetoothMsg_ReadCharacteristicValueError).
void RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome error);

// Characteristic.writeValue() Metrics
// There should be a call to this function for every call to
// Send(BluetoothMsg_WriteCharacteristicValueSuccess) and
// Send(BluetoothMsg_WriteCharacteristicValueError).
void RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome error);

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_METRICS_H_
