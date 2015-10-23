// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

DOMException* BluetoothError::take(ScriptPromiseResolver*, const WebBluetoothError& webError)
{
    switch (webError) {
#define MAP_ERROR(enumeration, name, message) \
    case WebBluetoothError::enumeration:      \
        return DOMException::create(name, message)

        // InvalidModificationErrors:
        MAP_ERROR(GATTInvalidAttributeLength, InvalidModificationError, "GATT Error: invalid attribute length.");

        // InvalidStateErrors:
        MAP_ERROR(ServiceNoLongerExists, InvalidStateError, "GATT Service no longer exists.");
        MAP_ERROR(CharacteristicNoLongerExists, InvalidStateError, "GATT Characteristic no longer exists.");

        // NetworkErrors:
        MAP_ERROR(GATTOperationInProgress, NetworkError, "GATT operation already in progress.");
        MAP_ERROR(GATTNotPaired, NetworkError, "GATT Error: Not paired.");
        MAP_ERROR(DeviceNoLongerInRange, NetworkError, "Bluetooth Device is no longer in range.");
        MAP_ERROR(ConnectUnknownError, NetworkError, "Unknown error when connecting to the device.");
        MAP_ERROR(ConnectAlreadyInProgress, NetworkError, "Connection already in progress.");
        MAP_ERROR(ConnectUnknownFailure, NetworkError, "Connection failed for unknown reason.");
        MAP_ERROR(ConnectAuthFailed, NetworkError, "Authentication failed.");
        MAP_ERROR(ConnectAuthCanceled, NetworkError, "Authentication canceled.");
        MAP_ERROR(ConnectAuthRejected, NetworkError, "Authentication rejected.");
        MAP_ERROR(ConnectAuthTimeout, NetworkError, "Authentication timeout.");
        MAP_ERROR(ConnectUnsupportedDevice, NetworkError, "Unsupported device.");
        MAP_ERROR(UntranslatedConnectErrorCode, NetworkError, "Unknown ConnectErrorCode.");

        // NotFoundErrors:
        MAP_ERROR(BluetoothAdapterOff, NotFoundError, "Bluetooth adapter is off.");
        MAP_ERROR(NoBluetoothAdapter, NotFoundError, "Bluetooth adapter not available.");
        MAP_ERROR(DiscoverySessionStartFailed, NotFoundError, "Couldn't start discovery session.");
        MAP_ERROR(DiscoverySessionStopFailed, NotFoundError, "Failed to stop discovery session.");
        MAP_ERROR(NoDevicesFound, NotFoundError, "No Bluetooth devices in range.");
        MAP_ERROR(ServiceNotFound, NotFoundError, "Service not found in device.");
        MAP_ERROR(CharacteristicNotFound, NotFoundError, "Characteristic not found in device.");

        // NotSupportedErrors:
        MAP_ERROR(GATTUnknownError, NotSupportedError, "GATT Error Unknown.");
        MAP_ERROR(GATTUnknownFailure, NotSupportedError, "GATT operation failed for unknown reason.");
        MAP_ERROR(GATTNotPermitted, NotSupportedError, "GATT operation not permitted.");
        MAP_ERROR(GATTNotSupported, NotSupportedError, "GATT Error: Not supported.");
        MAP_ERROR(GATTUntranslatedErrorCode, NotSupportedError, "GATT Error: Unknown GattErrorCode.");

        // SecurityErrors:
        MAP_ERROR(GATTNotAuthorized, SecurityError, "GATT operation not authorized.");
        MAP_ERROR(RequestDeviceWithoutFrame, SecurityError, "No window to show the requestDevice() dialog.");

#undef MAP_ERROR
    }

    ASSERT_NOT_REACHED();
    return DOMException::create(UnknownError);
}

} // namespace blink
