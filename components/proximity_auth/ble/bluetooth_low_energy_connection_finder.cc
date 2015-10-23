// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_device_whitelist.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_uuid.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattConnection;
using device::BluetoothDiscoveryFilter;

namespace proximity_auth {
namespace {
const int kMinDiscoveryRSSI = -90;
}  // namespace

class BluetoothThrottler;

BluetoothLowEnergyConnectionFinder::BluetoothLowEnergyConnectionFinder(
    const std::string& remote_service_uuid,
    const std::string& to_peripheral_char_uuid,
    const std::string& from_peripheral_char_uuid,
    const BluetoothLowEnergyDeviceWhitelist* device_whitelist,
    BluetoothThrottler* bluetooth_throttler,
    int max_number_of_tries)
    : remote_service_uuid_(device::BluetoothUUID(remote_service_uuid)),
      to_peripheral_char_uuid_(device::BluetoothUUID(to_peripheral_char_uuid)),
      from_peripheral_char_uuid_(
          device::BluetoothUUID(from_peripheral_char_uuid)),
      device_whitelist_(device_whitelist),
      bluetooth_throttler_(bluetooth_throttler),
      max_number_of_tries_(max_number_of_tries),
      weak_ptr_factory_(this) {}

BluetoothLowEnergyConnectionFinder::~BluetoothLowEnergyConnectionFinder() {
  if (discovery_session_) {
    StopDiscoverySession();
  }

  if (connection_) {
    connection_->RemoveObserver(this);
    connection_.reset();
  }

  if (adapter_) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothLowEnergyConnectionFinder::Find(
    const ConnectionCallback& connection_callback) {
  if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    PA_LOG(WARNING) << "Bluetooth is unsupported on this platform. Aborting.";
    return;
  }
  PA_LOG(INFO) << "Finding connection";

  connection_callback_ = connection_callback;

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnAdapterInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

// It's not necessary to observe |AdapterPresentChanged| too. When |adapter_| is
// present, but not powered, it's not possible to scan for new devices.
void BluetoothLowEnergyConnectionFinder::AdapterPoweredChanged(
    BluetoothAdapter* adapter,
    bool powered) {
  DCHECK_EQ(adapter_.get(), adapter);
  PA_LOG(INFO) << "Adapter powered: " << powered;

  // Important: do not rely on |adapter->IsDiscoverying()| to verify if there is
  // an active discovery session. We need to create our own with an specific
  // filter.
  if (powered && (!discovery_session_ || !discovery_session_->IsActive()))
    StartDiscoverySession();
}

void BluetoothLowEnergyConnectionFinder::DeviceAdded(BluetoothAdapter* adapter,
                                                     BluetoothDevice* device) {
  DCHECK_EQ(adapter_.get(), adapter);
  DCHECK(device);

  // Note: Only consider |device| when it was actually added/updated during a
  // scanning, otherwise the device is stale and the GATT connection will fail.
  // For instance, when |adapter_| change status from unpowered to powered,
  // |DeviceAdded| is called for each paired |device|.
  if (adapter_->IsPowered() && discovery_session_ &&
      discovery_session_->IsActive())
    HandleDeviceUpdated(device);
}

void BluetoothLowEnergyConnectionFinder::DeviceChanged(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  DCHECK_EQ(adapter_.get(), adapter);
  DCHECK(device);

  // Note: Only consider |device| when it was actually added/updated during a
  // scanning, otherwise the device is stale and the GATT connection will fail.
  // For instance, when |adapter_| change status from unpowered to powered,
  // |DeviceAdded| is called for each paired |device|.
  if (adapter_->IsPowered() && discovery_session_ &&
      discovery_session_->IsActive())
    HandleDeviceUpdated(device);
}

void BluetoothLowEnergyConnectionFinder::HandleDeviceUpdated(
    BluetoothDevice* device) {
  // Ensuring only one call to |CreateConnection()| is made. A new |connection_|
  // can be created only when the previous one disconnects, triggering a call to
  // |OnConnectionStatusChanged|.
  if (connection_ || !device->IsPaired())
    return;

  if (HasService(device) ||
      device_whitelist_->HasDeviceWithAddress(device->GetAddress())) {
    PA_LOG(INFO) << "Connecting to paired device " << device->GetAddress()
                 << " with service (" << HasService(device)
                 << ") or is whitelisted ("
                 << device_whitelist_->HasDeviceWithAddress(
                        device->GetAddress()) << ")";

    connection_ = CreateConnection(device->GetAddress());
    connection_->AddObserver(this);
    connection_->Connect();

    StopDiscoverySession();
  }
}

void BluetoothLowEnergyConnectionFinder::OnAdapterInitialized(
    scoped_refptr<BluetoothAdapter> adapter) {
  PA_LOG(INFO) << "Adapter ready";

  adapter_ = adapter;
  adapter_->AddObserver(this);

  // Note: it's not possible to connect with the paired directly, as the
  // temporary MAC may not be resolved automatically (see crbug.com/495402). The
  // Bluetooth adapter will fire |OnDeviceChanged| notifications for all
  // Bluetooth Low Energy devices that are advertising.
  StartDiscoverySession();
}

void BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStarted(
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  PA_LOG(INFO) << "Discovery session started";
  discovery_session_ = discovery_session.Pass();
}

void BluetoothLowEnergyConnectionFinder::OnStartDiscoverySessionError() {
  PA_LOG(WARNING) << "Error starting discovery session";
}

void BluetoothLowEnergyConnectionFinder::StartDiscoverySession() {
  DCHECK(adapter_);
  if (discovery_session_ && discovery_session_->IsActive()) {
    PA_LOG(INFO) << "Discovery session already active";
    return;
  }

  // Discover only low energy (LE) devices with strong enough signal.
  scoped_ptr<BluetoothDiscoveryFilter> filter(new BluetoothDiscoveryFilter(
      BluetoothDiscoveryFilter::Transport::TRANSPORT_LE));
  filter->SetRSSI(kMinDiscoveryRSSI);

  adapter_->StartDiscoverySessionWithFilter(
      filter.Pass(),
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStarted,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothLowEnergyConnectionFinder::OnStartDiscoverySessionError,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStopped() {
  PA_LOG(INFO) << "Discovery session stopped";
  discovery_session_.reset();
}

void BluetoothLowEnergyConnectionFinder::OnStopDiscoverySessionError() {
  PA_LOG(WARNING) << "Error stopping discovery session";
}

void BluetoothLowEnergyConnectionFinder::StopDiscoverySession() {
  PA_LOG(INFO) << "Stopping discovery sesison";

  if (!adapter_) {
    PA_LOG(WARNING) << "Adapter not initialized";
    return;
  }
  if (!discovery_session_ || !discovery_session_->IsActive()) {
    PA_LOG(INFO) << "No Active discovery session";
    return;
  }

  discovery_session_->Stop(
      base::Bind(&BluetoothLowEnergyConnectionFinder::OnDiscoverySessionStopped,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothLowEnergyConnectionFinder::OnStopDiscoverySessionError,
          weak_ptr_factory_.GetWeakPtr()));
}

bool BluetoothLowEnergyConnectionFinder::HasService(
    BluetoothDevice* remote_device) {
  if (remote_device) {
    PA_LOG(INFO) << "Device " << remote_device->GetAddress() << " has "
                 << remote_device->GetUUIDs().size() << " services.";
    std::vector<device::BluetoothUUID> uuids = remote_device->GetUUIDs();
    for (const auto& service_uuid : uuids) {
      if (remote_service_uuid_ == service_uuid) {
        return true;
      }
    }
  }
  return false;
}

scoped_ptr<Connection> BluetoothLowEnergyConnectionFinder::CreateConnection(
    const std::string& device_address) {
  RemoteDevice remote_device(std::string(), std::string(), device_address,
                             std::string());

  return make_scoped_ptr(new BluetoothLowEnergyConnection(
      remote_device, adapter_, remote_service_uuid_, to_peripheral_char_uuid_,
      from_peripheral_char_uuid_, bluetooth_throttler_, max_number_of_tries_));
}

void BluetoothLowEnergyConnectionFinder::OnConnectionStatusChanged(
    Connection* connection,
    Connection::Status old_status,
    Connection::Status new_status) {
  DCHECK_EQ(connection, connection_.get());
  PA_LOG(INFO) << "OnConnectionStatusChanged: " << old_status << " -> "
               << new_status;

  if (!connection_callback_.is_null() && connection_->IsConnected()) {
    adapter_->RemoveObserver(this);
    connection_->RemoveObserver(this);

    // If we invoke the callback now, the callback function may install its own
    // observer to |connection_|. Because we are in the ConnectionObserver
    // callstack, this new observer will receive this connection event.
    // Therefore, we need to invoke the callback asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothLowEnergyConnectionFinder::InvokeCallbackAsync,
                   weak_ptr_factory_.GetWeakPtr()));
  } else if (old_status == Connection::IN_PROGRESS) {
    PA_LOG(WARNING) << "Connection failed. Retrying.";
    RestartDiscoverySessionWhenReady();
  }
}

void BluetoothLowEnergyConnectionFinder::RestartDiscoverySessionWhenReady() {
  PA_LOG(INFO) << "Trying to restart discovery.";

  // To restart scanning for devices, it's necessary to ensure that:
  // (i) the GATT connection to |remove_device_| is closed;
  // (ii) there is no pending call to
  // |device::BluetoothDiscoverySession::Stop()|.
  // The second condition is satisfied when |OnDiscoveryStopped| is called and
  // |discovery_session_| is reset.
  if (!discovery_session_) {
    PA_LOG(INFO) << "Ready to start discovery.";
    connection_.reset();
    StartDiscoverySession();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&BluetoothLowEnergyConnectionFinder::
                                  RestartDiscoverySessionWhenReady,
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

BluetoothDevice* BluetoothLowEnergyConnectionFinder::GetDevice(
    std::string device_address) {
  // It's not possible to simply use
  // |adapter_->GetDevice(GetRemoteDeviceAddress())| to find the device with MAC
  // address |GetRemoteDeviceAddress()|. For paired devices,
  // BluetoothAdapter::GetDevice(XXX) searches for the temporary MAC address
  // XXX, whereas |remote_device_.bluetooth_address| is the real MAC address.
  // This is a bug in the way device::BluetoothAdapter is storing the devices
  // (see crbug.com/497841).
  std::vector<BluetoothDevice*> devices = adapter_->GetDevices();
  for (const auto& device : devices) {
    if (device->GetAddress() == device_address)
      return device;
  }
  return nullptr;
}

void BluetoothLowEnergyConnectionFinder::InvokeCallbackAsync() {
  connection_callback_.Run(connection_.Pass());
}

}  // namespace proximity_auth
