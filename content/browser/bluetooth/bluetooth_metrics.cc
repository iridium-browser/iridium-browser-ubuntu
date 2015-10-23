// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_metrics.h"

#include <map>
#include <set>
#include "base/hash.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "content/common/bluetooth/bluetooth_scan_filter.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothUUID;

namespace {
// TODO(ortuno): Remove once we have a macro to histogram strings.
// http://crbug.com/520284
int HashUUID(const std::string& uuid) {
  uint32 data = base::SuperFastHash(uuid.data(), uuid.size());

  // Strip off the signed bit because UMA doesn't support negative values,
  // but takes a signed int as input.
  return static_cast<int>(data & 0x7fffffff);
}
}  // namespace

namespace content {

// General

void RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction function) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.FunctionCall.Count",
                            static_cast<int>(function),
                            static_cast<int>(UMAWebBluetoothFunction::COUNT));
}

// requestDevice()

void RecordRequestDeviceOutcome(UMARequestDeviceOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.RequestDevice.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMARequestDeviceOutcome::COUNT));
}

static void RecordRequestDeviceFilters(
    const std::vector<content::BluetoothScanFilter>& filters) {
  UMA_HISTOGRAM_COUNTS_100("Bluetooth.Web.RequestDevice.Filters.Count",
                           filters.size());
  for (const content::BluetoothScanFilter& filter : filters) {
    UMA_HISTOGRAM_COUNTS_100("Bluetooth.Web.RequestDevice.FilterSize",
                             filter.services.size());
    for (const BluetoothUUID& service : filter.services) {
      // TODO(ortuno): Use a macro to histogram strings.
      // http://crbug.com/520284
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Bluetooth.Web.RequestDevice.Filters.Services",
          HashUUID(service.canonical_value()));
    }
  }
}

static void RecordRequestDeviceOptionalServices(
    const std::vector<BluetoothUUID>& optional_services) {
  UMA_HISTOGRAM_COUNTS_100("Bluetooth.Web.RequestDevice.OptionalServices.Count",
                           optional_services.size());
  for (const BluetoothUUID& service : optional_services) {
    // TODO(ortuno): Use a macro to histogram strings.
    // http://crbug.com/520284
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Bluetooth.Web.RequestDevice.OptionalServices.Services",
        HashUUID(service.canonical_value()));
  }
}

static void RecordUnionOfServices(
    const std::vector<content::BluetoothScanFilter>& filters,
    const std::vector<BluetoothUUID>& optional_services) {
  std::set<BluetoothUUID> union_of_services(optional_services.begin(),
                                            optional_services.end());

  for (const content::BluetoothScanFilter& filter : filters)
    union_of_services.insert(filter.services.begin(), filter.services.end());

  UMA_HISTOGRAM_COUNTS_100("Bluetooth.Web.RequestDevice.UnionOfServices.Count",
                           union_of_services.size());
}

void RecordRequestDeviceArguments(
    const std::vector<content::BluetoothScanFilter>& filters,
    const std::vector<device::BluetoothUUID>& optional_services) {
  RecordRequestDeviceFilters(filters);
  RecordRequestDeviceOptionalServices(optional_services);
  RecordUnionOfServices(filters, optional_services);
}

// connectGATT

void RecordConnectGATTOutcome(UMAConnectGATTOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.ConnectGATT.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMAConnectGATTOutcome::COUNT));
}

void RecordConnectGATTTimeSuccess(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Bluetooth.Web.ConnectGATT.TimeSuccess", duration);
}

void RecordConnectGATTTimeFailed(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Bluetooth.Web.ConnectGATT.TimeFailed", duration);
}

// getPrimaryService

void RecordGetPrimaryServiceService(const BluetoothUUID& service) {
  // TODO(ortuno): Use a macro to histogram strings.
  // http://crbug.com/520284
  UMA_HISTOGRAM_SPARSE_SLOWLY("Bluetooth.Web.GetPrimaryService.Services",
                              HashUUID(service.canonical_value()));
}

void RecordGetPrimaryServiceOutcome(UMAGetPrimaryServiceOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.Web.GetPrimaryService.Outcome", static_cast<int>(outcome),
      static_cast<int>(UMAGetPrimaryServiceOutcome::COUNT));
}

// getCharacteristic

void RecordGetCharacteristicOutcome(UMAGetCharacteristicOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(
      "Bluetooth.Web.GetCharacteristic.Outcome", static_cast<int>(outcome),
      static_cast<int>(UMAGetCharacteristicOutcome::COUNT));
}

void RecordGetCharacteristicCharacteristic(const std::string& characteristic) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Bluetooth.Web.GetCharacteristic.Characteristic",
                              HashUUID(characteristic));
}

// GATT Operations

void RecordGATTOperationOutcome(UMAGATTOperation operation,
                                UMAGATTOperationOutcome outcome) {
  switch (operation) {
    case UMAGATTOperation::CHARACTERISTIC_READ:
      RecordCharacteristicReadValueOutcome(outcome);
      return;
    case UMAGATTOperation::CHARACTERISTIC_WRITE:
      RecordCharacteristicWriteValueOutcome(outcome);
      return;
    case UMAGATTOperation::COUNT:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

// Characteristic.readValue

// static
void RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.Characteristic.ReadValue.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMAGATTOperationOutcome::COUNT));
}

// Characteristic.writeValue

void RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Web.Characteristic.WriteValue.Outcome",
                            static_cast<int>(outcome),
                            static_cast<int>(UMAGATTOperationOutcome::COUNT));
}

}  // namespace content
