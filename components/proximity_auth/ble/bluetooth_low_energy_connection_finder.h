// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_FINDER_H
#define COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_LOW_ENERGY_CONNECTION_FINDER_H

#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/connection.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/connection_observer.h"
#include "components/proximity_auth/remote_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"

namespace proximity_auth {

class BluetoothLowEnergyDeviceWhitelist;
class BluetoothThrottler;

// This ConnectionFinder implementation is specialized in finding a Bluetooth
// Low Energy remote device.
class BluetoothLowEnergyConnectionFinder
    : public ConnectionFinder,
      public ConnectionObserver,
      public device::BluetoothAdapter::Observer {
 public:
  BluetoothLowEnergyConnectionFinder(
      const std::string& remote_service_uuid,
      const std::string& to_peripheral_char_uuid,
      const std::string& from_peripheral_char_uuid,
      const BluetoothLowEnergyDeviceWhitelist* device_whitelist,
      BluetoothThrottler* bluetooth_throttler,
      int max_number_of_tries);
  ~BluetoothLowEnergyConnectionFinder() override;

  // Finds a connection to the remote device.
  void Find(const ConnectionCallback& connection_callback) override;

  // proximity_auth::ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;

  // device::BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 protected:
  // Creates a proximity_auth::Connection with the device given by
  // |device_address|. Exposed for testing.
  virtual scoped_ptr<Connection> CreateConnection(
      const std::string& device_address);

 private:
  // Callback to be called when the Bluetooth adapter is initialized.
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // Checks if |remote_device| contains |remote_service_uuid| and creates a
  // connection in that case.
  void HandleDeviceUpdated(device::BluetoothDevice* remote_device);

  // Callback called when a new discovery session is started.
  void OnDiscoverySessionStarted(
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

  // Callback called when there is an error starting a new discovery session.
  void OnStartDiscoverySessionError();

  // Starts a discovery session for |adapter_|.
  void StartDiscoverySession();

  // Callback called when |discovery_session_| is stopped.
  void OnDiscoverySessionStopped();

  // Callback called when there is an error stopping |discovery_session_|.
  void OnStopDiscoverySessionError();

  // Stops the discovery session given by |discovery_session_|.
  void StopDiscoverySession();

  // Checks if a service with |service_uuid| is offered by |remote_device|.
  bool HasService(device::BluetoothDevice* remote_device);

  // Restarts the discovery session after creating |connection_| fails.
  void RestartDiscoverySessionWhenReady();

  // Used to invoke |connection_callback_| asynchronously, decoupling the
  // callback invocation from the ConnectionObserver callstack.
  void InvokeCallbackAsync();

  // Returns the device with |device_address|.
  device::BluetoothDevice* GetDevice(std::string device_address);

  // The uuid of the service it looks for to establish a GattConnection.
  device::BluetoothUUID remote_service_uuid_;

  // Characteristic used to send data to the remote device.
  device::BluetoothUUID to_peripheral_char_uuid_;

  // Characteristic used to receive data from the remote device.
  device::BluetoothUUID from_peripheral_char_uuid_;

  // Devices in |device_whitelist_| don't need to have |remote_service_uuid_|
  // cached or advertised. Not owned, must outlive this instance.
  const BluetoothLowEnergyDeviceWhitelist* device_whitelist_;

  // Throttles repeated connection attempts to the same device. This is a
  // workaround for crbug.com/508919. Not owned, must outlive this instance.
  BluetoothThrottler* bluetooth_throttler_;

  // The Bluetooth adapter over which the Bluetooth connection will be made.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The discovery session associated to this object.
  scoped_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // The connection with |remote_device|.
  scoped_ptr<Connection> connection_;

  // Callback called when the connection is established.
  // device::BluetoothDevice::GattConnectionCallback connection_callback_;
  ConnectionCallback connection_callback_;

  // BluetoothLowEnergyConnection parameter.
  int max_number_of_tries_;

  base::WeakPtrFactory<BluetoothLowEnergyConnectionFinder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyConnectionFinder);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLE_BLUETOOTH_CONNECTION_FINDER_H
