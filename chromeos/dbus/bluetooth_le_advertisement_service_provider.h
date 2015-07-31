// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_
#define CHROMEOS_DBUS_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "dbus/bus.h"
#include "dbus/file_descriptor.h"
#include "dbus/object_path.h"

namespace chromeos {

// BluetoothAdvertisementServiceProvider is used to provide a D-Bus object that
// the Bluetooth daemon can communicate with to advertise data.
class CHROMEOS_EXPORT BluetoothLEAdvertisementServiceProvider {
 public:
  using UUIDList = std::vector<std::string>;
  using ManufacturerData = std::map<uint16_t, std::vector<uint8_t>>;
  using ServiceData = std::map<std::string, std::vector<uint8_t>>;

  // Type of advertisement.
  enum AdvertisementType {
    ADVERTISEMENT_TYPE_BROADCAST,
    ADVERTISEMENT_TYPE_PERIPHERAL
  };

  // Interface for reacting to advertisement changes.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // This method will be called when the advertisement is unregistered from
    // the Bluetooth daemon, generally at shutdown or if the adapter goes away.
    // It may be used to perform cleanup tasks. This corresponds to the
    // org.bluez.LEAdvertisement1.Release method and is renamed to avoid a
    // conflict with base::Refcounted<T>.
    virtual void Released() = 0;
  };

  virtual ~BluetoothLEAdvertisementServiceProvider();

  const dbus::ObjectPath& object_path() { return object_path_; }

  // Creates the instance where |bus| is the D-Bus bus connection to export
  // the object onto, |object_path| is the object path that it should have
  // and |delegate| is the object to which all method calls will be passed
  // and responses generated from.
  static scoped_ptr<BluetoothLEAdvertisementServiceProvider> Create(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate,
      AdvertisementType type,
      scoped_ptr<UUIDList> service_uuids,
      scoped_ptr<ManufacturerData> manufacturer_data,
      scoped_ptr<UUIDList> solicit_uuids,
      scoped_ptr<ServiceData> service_data);

 protected:
  BluetoothLEAdvertisementServiceProvider();

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  dbus::ObjectPath object_path_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothLEAdvertisementServiceProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_
