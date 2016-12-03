// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include <iterator>
#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/string_util_icu.h"
#include "grit/bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {

BluetoothDevice::DeviceUUIDs::DeviceUUIDs() {}

BluetoothDevice::DeviceUUIDs::~DeviceUUIDs() {}

void BluetoothDevice::DeviceUUIDs::ReplaceAdvertisedUUIDs(
    UUIDList new_advertised_uuids) {
  advertised_uuids_.clear();
  for (auto& it : new_advertised_uuids) {
    advertised_uuids_.insert(std::move(it));
  }
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ClearAdvertisedUUIDs() {
  advertised_uuids_.clear();
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ReplaceServiceUUIDs(
    const GattServiceMap& gatt_services) {
  service_uuids_.clear();
  for (const auto& gatt_service_pair : gatt_services) {
    service_uuids_.insert(gatt_service_pair.second->GetUUID());
  }
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ClearServiceUUIDs() {
  service_uuids_.clear();
  UpdateDeviceUUIDs();
}

const BluetoothDevice::UUIDSet& BluetoothDevice::DeviceUUIDs::GetUUIDs() const {
  return device_uuids_;
}

void BluetoothDevice::DeviceUUIDs::UpdateDeviceUUIDs() {
  device_uuids_.clear();
  std::set_union(advertised_uuids_.begin(), advertised_uuids_.end(),
                 service_uuids_.begin(), service_uuids_.end(),
                 std::inserter(device_uuids_, device_uuids_.begin()));
}

BluetoothDevice::BluetoothDevice(BluetoothAdapter* adapter)
    : adapter_(adapter),
      gatt_services_discovery_complete_(false),
      last_update_time_(base::Time()) {}

BluetoothDevice::~BluetoothDevice() {
  for (BluetoothGattConnection* connection : gatt_connections_) {
    connection->InvalidateConnectionReference();
  }
}

BluetoothDevice::ConnectionInfo::ConnectionInfo()
    : rssi(kUnknownPower),
      transmit_power(kUnknownPower),
      max_transmit_power(kUnknownPower) {}

BluetoothDevice::ConnectionInfo::ConnectionInfo(
    int rssi, int transmit_power, int max_transmit_power)
    : rssi(rssi),
      transmit_power(transmit_power),
      max_transmit_power(max_transmit_power) {}

BluetoothDevice::ConnectionInfo::~ConnectionInfo() {}

base::string16 BluetoothDevice::GetNameForDisplay() const {
  base::Optional<std::string> name = GetName();
  if (name && HasGraphicCharacter(name.value())) {
    return base::UTF8ToUTF16(name.value());
  } else {
    return GetAddressWithLocalizedDeviceTypeName();
  }
}

base::string16 BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() const {
  base::string16 address_utf16 = base::UTF8ToUTF16(GetAddress());
  BluetoothDevice::DeviceType device_type = GetDeviceType();
  switch (device_type) {
    case DEVICE_COMPUTER:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address_utf16);
    case DEVICE_PHONE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                        address_utf16);
    case DEVICE_MODEM:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address_utf16);
    case DEVICE_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_AUDIO,
                                        address_utf16);
    case DEVICE_CAR_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CAR_AUDIO,
                                        address_utf16);
    case DEVICE_VIDEO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_VIDEO,
                                        address_utf16);
    case DEVICE_JOYSTICK:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address_utf16);
    case DEVICE_GAMEPAD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
                                        address_utf16);
    case DEVICE_KEYBOARD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address_utf16);
    case DEVICE_MOUSE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address_utf16);
    case DEVICE_TABLET:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address_utf16);
    case DEVICE_KEYBOARD_MOUSE_COMBO:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address_utf16);
    default:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN,
                                        address_utf16);
  }
}

BluetoothDevice::DeviceType BluetoothDevice::GetDeviceType() const {
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32_t bluetooth_class = GetBluetoothClass();
  switch ((bluetooth_class & 0x1f00) >> 8) {
    case 0x01:
      // Computer major device class.
      return DEVICE_COMPUTER;
    case 0x02:
      // Phone major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
        case 0x01:
        case 0x02:
        case 0x03:
          // Cellular, cordless and smart phones.
          return DEVICE_PHONE;
        case 0x04:
        case 0x05:
          // Modems: wired or voice gateway and common ISDN access.
          return DEVICE_MODEM;
      }
      break;
    case 0x04:
      // Audio major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
        case 0x08:
          // Car audio.
          return DEVICE_CAR_AUDIO;
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x010:
          // Video devices.
          return DEVICE_VIDEO;
        default:
          return DEVICE_AUDIO;
      }
      break;
    case 0x05:
      // Peripheral major device class.
      switch ((bluetooth_class & 0xc0) >> 6) {
        case 0x00:
          // "Not a keyboard or pointing device."
          switch ((bluetooth_class & 0x01e) >> 2) {
            case 0x01:
              // Joystick.
              return DEVICE_JOYSTICK;
            case 0x02:
              // Gamepad.
              return DEVICE_GAMEPAD;
            default:
              return DEVICE_PERIPHERAL;
          }
          break;
        case 0x01:
          // Keyboard.
          return DEVICE_KEYBOARD;
        case 0x02:
          // Pointing device.
          switch ((bluetooth_class & 0x01e) >> 2) {
            case 0x05:
              // Digitizer tablet.
              return DEVICE_TABLET;
            default:
              // Mouse.
              return DEVICE_MOUSE;
          }
          break;
        case 0x03:
          // Combo device.
          return DEVICE_KEYBOARD_MOUSE_COMBO;
      }
      break;
  }

  // Some bluetooth devices, e.g., Microsoft Universal Foldable Keyboard,
  // do not expose its bluetooth class. Use its appearance as a work-around.
  // https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.appearance.xml
  uint16_t appearance = GetAppearance();
  // appearance: 10-bit category and 6-bit sub-category
  switch ((appearance & 0xffc0) >> 6) {
    case 0x01:
      // Generic phone
      return DEVICE_PHONE;
    case 0x02:
      // Generic computer
      return DEVICE_COMPUTER;
    case 0x0f:
      // HID subtype
      switch (appearance & 0x3f) {
        case 0x01:
          // Keyboard.
          return DEVICE_KEYBOARD;
        case 0x02:
          // Mouse
          return DEVICE_MOUSE;
        case 0x03:
          // Joystick
          return DEVICE_JOYSTICK;
        case 0x04:
          // Gamepad
          return DEVICE_GAMEPAD;
        case 0x05:
          // Digitizer tablet
          return DEVICE_TABLET;
      }
  }

  return DEVICE_UNKNOWN;
}

bool BluetoothDevice::IsPairable() const {
  DeviceType type = GetDeviceType();

  // Get the vendor part of the address: "00:11:22" for "00:11:22:33:44:55"
  std::string vendor = GetAddress().substr(0, 8);

  // Verbatim "Bluetooth Mouse", model 96674
  if (type == DEVICE_MOUSE && vendor == "00:12:A1")
    return false;
  // Microsoft "Microsoft Bluetooth Notebook Mouse 5000", model X807028-001
  if (type == DEVICE_MOUSE && vendor == "7C:ED:8D")
    return false;
  // Sony PlayStation Dualshock3
  if (IsTrustable())
    return false;

  // TODO: Move this database into a config file.

  return true;
}

bool BluetoothDevice::IsTrustable() const {
  // Sony PlayStation Dualshock3
  if ((GetVendorID() == 0x054c && GetProductID() == 0x0268 &&
       GetName() == std::string("PLAYSTATION(R)3 Controller")))
    return true;

  return false;
}

BluetoothDevice::UUIDSet BluetoothDevice::GetUUIDs() const {
  return device_uuids_.GetUUIDs();
}

const BluetoothDevice::ServiceDataMap& BluetoothDevice::GetServiceData() const {
  return service_data_;
}

BluetoothDevice::UUIDSet BluetoothDevice::GetServiceDataUUIDs() const {
  UUIDSet service_data_uuids;
  for (const auto& uuid_service_data_pair : service_data_) {
    service_data_uuids.insert(uuid_service_data_pair.first);
  }
  return service_data_uuids;
}

const std::vector<uint8_t>* BluetoothDevice::GetServiceDataForUUID(
    const BluetoothUUID& uuid) const {
  auto it = service_data_.find(uuid);
  if (it != service_data_.end()) {
    return &it->second;
  }
  return nullptr;
}

base::Optional<int8_t> BluetoothDevice::GetInquiryRSSI() const {
  return inquiry_rssi_;
}

base::Optional<int8_t> BluetoothDevice::GetInquiryTxPower() const {
  return inquiry_tx_power_;
}

void BluetoothDevice::CreateGattConnection(
    const GattConnectionCallback& callback,
    const ConnectErrorCallback& error_callback) {
  create_gatt_connection_success_callbacks_.push_back(callback);
  create_gatt_connection_error_callbacks_.push_back(error_callback);

  if (IsGattConnected())
    return DidConnectGatt();

  CreateGattConnectionImpl();
}

void BluetoothDevice::SetGattServicesDiscoveryComplete(bool complete) {
  gatt_services_discovery_complete_ = complete;
}

bool BluetoothDevice::IsGattServicesDiscoveryComplete() const {
  return gatt_services_discovery_complete_;
}

std::vector<BluetoothRemoteGattService*> BluetoothDevice::GetGattServices()
    const {
  std::vector<BluetoothRemoteGattService*> services;
  for (const auto& iter : gatt_services_)
    services.push_back(iter.second);
  return services;
}

BluetoothRemoteGattService* BluetoothDevice::GetGattService(
    const std::string& identifier) const {
  return gatt_services_.get(identifier);
}

// static
std::string BluetoothDevice::CanonicalizeAddress(const std::string& address) {
  std::string canonicalized = address;
  if (address.size() == 12) {
    // Might be an address in the format "1A2B3C4D5E6F". Add separators.
    for (size_t i = 2; i < canonicalized.size(); i += 3) {
      canonicalized.insert(i, ":");
    }
  }

  // Verify that the length matches the canonical format "1A:2B:3C:4D:5E:6F".
  const size_t kCanonicalAddressLength = 17;
  if (canonicalized.size() != kCanonicalAddressLength)
    return std::string();

  const char separator = canonicalized[2];
  for (size_t i = 0; i < canonicalized.size(); ++i) {
    bool is_separator = (i + 1) % 3 == 0;
    if (is_separator) {
      // All separators in the input |address| should be consistent.
      if (canonicalized[i] != separator)
        return std::string();

      canonicalized[i] = ':';
    } else {
      if (!base::IsHexDigit(canonicalized[i]))
        return std::string();

      canonicalized[i] = base::ToUpperASCII(canonicalized[i]);
    }
  }

  return canonicalized;
}

std::string BluetoothDevice::GetIdentifier() const { return GetAddress(); }

void BluetoothDevice::UpdateAdvertisementData(int8_t rssi,
                                              UUIDList advertised_uuids,
                                              ServiceDataMap service_data,
                                              const int8_t* tx_power) {
  UpdateTimestamp();

  inquiry_rssi_ = rssi;

  device_uuids_.ReplaceAdvertisedUUIDs(std::move(advertised_uuids));
  service_data_ = std::move(service_data);

  if (tx_power != nullptr) {
    inquiry_tx_power_ = *tx_power;
  } else {
    inquiry_tx_power_ = base::nullopt;
  }
}

void BluetoothDevice::ClearAdvertisementData() {
  inquiry_rssi_ = base::nullopt;
  device_uuids_.ClearAdvertisedUUIDs();
  service_data_.clear();
  inquiry_tx_power_ = base::nullopt;
  GetAdapter()->NotifyDeviceChanged(this);
}

void BluetoothDevice::DidConnectGatt() {
  for (const auto& callback : create_gatt_connection_success_callbacks_) {
    callback.Run(
        base::MakeUnique<BluetoothGattConnection>(adapter_, GetAddress()));
  }
  create_gatt_connection_success_callbacks_.clear();
  create_gatt_connection_error_callbacks_.clear();
  GetAdapter()->NotifyDeviceChanged(this);
}

void BluetoothDevice::DidFailToConnectGatt(ConnectErrorCode error) {
  // Connection request should only be made if there are no active
  // connections.
  DCHECK(gatt_connections_.empty());

  for (const auto& error_callback : create_gatt_connection_error_callbacks_)
    error_callback.Run(error);
  create_gatt_connection_success_callbacks_.clear();
  create_gatt_connection_error_callbacks_.clear();
}

void BluetoothDevice::DidDisconnectGatt() {
  // Pending calls to connect GATT are not expected, if they were then
  // DidFailToConnectGatt should have been called.
  DCHECK(create_gatt_connection_error_callbacks_.empty());

  // Invalidate all BluetoothGattConnection objects.
  for (BluetoothGattConnection* connection : gatt_connections_) {
    connection->InvalidateConnectionReference();
  }
  gatt_connections_.clear();
  GetAdapter()->NotifyDeviceChanged(this);
}

void BluetoothDevice::AddGattConnection(BluetoothGattConnection* connection) {
  auto result = gatt_connections_.insert(connection);
  DCHECK(result.second);  // Check insert happened; there was no duplicate.
}

void BluetoothDevice::RemoveGattConnection(
    BluetoothGattConnection* connection) {
  size_t erased_count = gatt_connections_.erase(connection);
  DCHECK(erased_count);
  if (gatt_connections_.size() == 0)
    DisconnectGatt();
}

void BluetoothDevice::SetAsExpiredForTesting() {
  last_update_time_ =
      base::Time::NowFromSystemTime() -
      (BluetoothAdapter::timeoutSec + base::TimeDelta::FromSeconds(1));
}

void BluetoothDevice::Pair(PairingDelegate* pairing_delegate,
                           const base::Closure& callback,
                           const ConnectErrorCallback& error_callback) {
  NOTREACHED();
}

void BluetoothDevice::UpdateTimestamp() {
  last_update_time_ = base::Time::NowFromSystemTime();
}

// static
int8_t BluetoothDevice::ClampPower(int power) {
  if (power < INT8_MIN) {
    return INT8_MIN;
  }
  if (power > INT8_MAX) {
    return INT8_MAX;
  }
  return static_cast<int8_t>(power);
}

}  // namespace device
