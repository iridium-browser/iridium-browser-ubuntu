// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_media_endpoint_service_provider.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_media_client.h"
#include "chromeos/dbus/fake_bluetooth_media_transport_client.h"

using dbus::ObjectPath;

namespace chromeos {

FakeBluetoothMediaEndpointServiceProvider::
    FakeBluetoothMediaEndpointServiceProvider(const ObjectPath& object_path,
                                              Delegate* delegate)
    : visible_(false), object_path_(object_path), delegate_(delegate) {
  VLOG(1) << "Create Bluetooth Media Endpoint: " << object_path_.value();
}

FakeBluetoothMediaEndpointServiceProvider::
    ~FakeBluetoothMediaEndpointServiceProvider() {
  VLOG(1) << "Cleaning up Bluetooth Media Endpoint: " << object_path_.value();
}

void FakeBluetoothMediaEndpointServiceProvider::SetConfiguration(
    const ObjectPath& transport_path,
    const Delegate::TransportProperties& properties) {
  VLOG(1) << object_path_.value() << ": SetConfiguration for "
          << transport_path.value();

  delegate_->SetConfiguration(transport_path, properties);
}

void FakeBluetoothMediaEndpointServiceProvider::SelectConfiguration(
    const std::vector<uint8_t>& capabilities,
    const Delegate::SelectConfigurationCallback& callback) {
  VLOG(1) << object_path_.value() << ": SelectConfiguration";

  delegate_->SelectConfiguration(capabilities, callback);

  // Makes the transport object valid for the given endpoint path.
  FakeBluetoothMediaTransportClient* transport =
      static_cast<FakeBluetoothMediaTransportClient*>(
          DBusThreadManager::Get()->GetBluetoothMediaTransportClient());
  DCHECK(transport);
  transport->SetValid(this, true);
}

void FakeBluetoothMediaEndpointServiceProvider::ClearConfiguration(
    const ObjectPath& transport_path) {
  VLOG(1) << object_path_.value() << ": ClearConfiguration on "
          << transport_path.value();

  delegate_->ClearConfiguration(transport_path);
}

void FakeBluetoothMediaEndpointServiceProvider::Released() {
  VLOG(1) << object_path_.value() << ": Released";

  delegate_->Released();
}

}  // namespace chromeos
