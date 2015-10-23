// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NETWORK_ERROR Note:
// When a device can't be found in the BluetoothAdapter, that generally
// indicates that it's gone out of range. We reject with a NetworkError in that
// case.
// https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothdevice-connectgatt

#include "content/browser/bluetooth/bluetooth_dispatcher_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/bad_message.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/bluetooth/bluetooth_messages.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

using blink::WebBluetoothError;
using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothGattCharacteristic;
using device::BluetoothGattService;
using device::BluetoothUUID;

namespace content {

namespace {

// TODO(ortuno): Once we have a chooser for scanning and the right
// callback for discovered services we should delete these constants.
// https://crbug.com/436280 and https://crbug.com/484504
const int kDelayTime = 5;         // 5 seconds for scanning and discovering
const int kTestingDelayTime = 0;  // No need to wait during tests

// Defined at
// https://webbluetoothchrome.github.io/web-bluetooth/#dfn-matches-a-filter
bool MatchesFilter(const std::set<BluetoothUUID>& device_uuids,
                   const content::BluetoothScanFilter& filter) {
  if (filter.services.empty())
    return false;
  for (const BluetoothUUID& service : filter.services) {
    if (!ContainsKey(device_uuids, service)) {
      return false;
    }
  }
  return true;
}

bool MatchesFilters(const device::BluetoothDevice& device,
                    const std::vector<content::BluetoothScanFilter>& filters) {
  const std::vector<BluetoothUUID>& device_uuid_list = device.GetUUIDs();
  const std::set<BluetoothUUID> device_uuids(device_uuid_list.begin(),
                                             device_uuid_list.end());
  for (const content::BluetoothScanFilter& filter : filters) {
    if (MatchesFilter(device_uuids, filter)) {
      return true;
    }
  }
  return false;
}

WebBluetoothError TranslateConnectError(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ERROR_UNKNOWN:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::UNKNOWN);
      return WebBluetoothError::ConnectUnknownError;
    case device::BluetoothDevice::ERROR_INPROGRESS:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::IN_PROGRESS);
      return WebBluetoothError::ConnectAlreadyInProgress;
    case device::BluetoothDevice::ERROR_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::FAILED);
      return WebBluetoothError::ConnectUnknownFailure;
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_FAILED);
      return WebBluetoothError::ConnectAuthFailed;
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_CANCELED);
      return WebBluetoothError::ConnectAuthCanceled;
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_REJECTED);
      return WebBluetoothError::ConnectAuthRejected;
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_TIMEOUT);
      return WebBluetoothError::ConnectAuthTimeout;
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::UNSUPPORTED_DEVICE);
      return WebBluetoothError::ConnectUnsupportedDevice;
  }
  NOTREACHED();
  return WebBluetoothError::UntranslatedConnectErrorCode;
}

blink::WebBluetoothError TranslateGATTError(
    BluetoothGattService::GattErrorCode error_code,
    UMAGATTOperation operation) {
  switch (error_code) {
    case BluetoothGattService::GATT_ERROR_UNKNOWN:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::UNKNOWN);
      return blink::WebBluetoothError::GATTUnknownError;
    case BluetoothGattService::GATT_ERROR_FAILED:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::FAILED);
      return blink::WebBluetoothError::GATTUnknownFailure;
    case BluetoothGattService::GATT_ERROR_IN_PROGRESS:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::IN_PROGRESS);
      return blink::WebBluetoothError::GATTOperationInProgress;
    case BluetoothGattService::GATT_ERROR_INVALID_LENGTH:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::INVALID_LENGTH);
      return blink::WebBluetoothError::GATTInvalidAttributeLength;
    case BluetoothGattService::GATT_ERROR_NOT_PERMITTED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_PERMITTED);
      return blink::WebBluetoothError::GATTNotPermitted;
    case BluetoothGattService::GATT_ERROR_NOT_AUTHORIZED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_AUTHORIZED);
      return blink::WebBluetoothError::GATTNotAuthorized;
    case BluetoothGattService::GATT_ERROR_NOT_PAIRED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_PAIRED);
      return blink::WebBluetoothError::GATTNotPaired;
    case BluetoothGattService::GATT_ERROR_NOT_SUPPORTED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_SUPPORTED);
      return blink::WebBluetoothError::GATTNotSupported;
  }
  NOTREACHED();
  return blink::WebBluetoothError::GATTUntranslatedErrorCode;
}

}  //  namespace

BluetoothDispatcherHost::BluetoothDispatcherHost(int render_process_id)
    : BrowserMessageFilter(BluetoothMsgStart),
      render_process_id_(render_process_id),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  current_delay_time_ = kDelayTime;
  if (BluetoothAdapterFactory::IsBluetoothAdapterAvailable())
    BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothDispatcherHost::set_adapter,
                   weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDispatcherHost::OnDestruct() const {
  // See class comment: UI Thread Note.
  BrowserThread::DeleteOnUIThread::Destruct(this);
}

void BluetoothDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  // See class comment: UI Thread Note.
  *thread = BrowserThread::UI;
}

bool BluetoothDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BluetoothDispatcherHost, message)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_RequestDevice, OnRequestDevice)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_ConnectGATT, OnConnectGATT)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_GetPrimaryService, OnGetPrimaryService)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_GetCharacteristic, OnGetCharacteristic)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_ReadValue, OnReadValue)
  IPC_MESSAGE_HANDLER(BluetoothHostMsg_WriteValue, OnWriteValue)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BluetoothDispatcherHost::SetBluetoothAdapterForTesting(
    scoped_refptr<device::BluetoothAdapter> mock_adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  current_delay_time_ = kTestingDelayTime;
  set_adapter(mock_adapter.Pass());
}

BluetoothDispatcherHost::~BluetoothDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Clear adapter, releasing observer references.
  set_adapter(scoped_refptr<device::BluetoothAdapter>());
}

// Stores information associated with an in-progress requestDevice call. This
// will include the state of the active chooser dialog in a future patch.
struct BluetoothDispatcherHost::RequestDeviceSession {
  RequestDeviceSession(const std::vector<BluetoothScanFilter>& filters,
                       const std::vector<BluetoothUUID>& optional_services)
      : filters(filters), optional_services(optional_services) {}

  std::vector<BluetoothScanFilter> filters;
  std::vector<BluetoothUUID> optional_services;
};

void BluetoothDispatcherHost::set_adapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (adapter_.get())
    adapter_->RemoveObserver(this);
  adapter_ = adapter;
  if (adapter_.get())
    adapter_->AddObserver(this);
}

static scoped_ptr<device::BluetoothDiscoveryFilter> ComputeScanFilter(
    const std::vector<BluetoothScanFilter>& filters) {
  std::set<BluetoothUUID> services;
  for (const BluetoothScanFilter& filter : filters) {
    services.insert(filter.services.begin(), filter.services.end());
  }
  scoped_ptr<device::BluetoothDiscoveryFilter> discovery_filter(
      new device::BluetoothDiscoveryFilter(
          device::BluetoothDiscoveryFilter::TRANSPORT_DUAL));
  for (const BluetoothUUID& service : services) {
    discovery_filter->AddUUID(service);
  }
  return discovery_filter.Pass();
}

void BluetoothDispatcherHost::OnRequestDevice(
    int thread_id,
    int request_id,
    int frame_routing_id,
    const std::vector<BluetoothScanFilter>& filters,
    const std::vector<BluetoothUUID>& optional_services) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::REQUEST_DEVICE);
  RecordRequestDeviceArguments(filters, optional_services);

  VLOG(1) << "requestDevice called with the following filters: ";
  for (const BluetoothScanFilter& filter : filters) {
    VLOG(1) << "\t[";
    for (const BluetoothUUID& service : filter.services)
      VLOG(1) << "\t\t" << service.value();
    VLOG(1) << "\t]";
  }

  VLOG(1) << "requestDevice called with the following optional services: ";
  for (const BluetoothUUID& service : optional_services)
    VLOG(1) << "\t" << service.value();

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_process_id_, frame_routing_id);

  if (!render_frame_host) {
    DLOG(WARNING)
        << "Got a requestDevice IPC without a matching RenderFrameHost: "
        << render_process_id_ << ", " << frame_routing_id;
    RecordRequestDeviceOutcome(UMARequestDeviceOutcome::NO_RENDER_FRAME);
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id, WebBluetoothError::RequestDeviceWithoutFrame));
    return;
  }

  // TODO(scheib): Device selection UI: crbug.com/436280
  // TODO(scheib): Utilize BluetoothAdapter::Observer::DeviceAdded/Removed.
  if (adapter_.get()) {
    if (!request_device_sessions_
             .insert(std::make_pair(
                 std::make_pair(thread_id, request_id),
                 RequestDeviceSession(filters, optional_services)))
             .second) {
      LOG(ERROR) << "2 requestDevice() calls with the same thread_id ("
                 << thread_id << ") and request_id (" << request_id
                 << ") shouldn't arrive at the same BluetoothDispatcherHost.";
      bad_message::ReceivedBadMessage(
          this, bad_message::BDH_DUPLICATE_REQUEST_DEVICE_ID);
    }
    if (!adapter_->IsPresent()) {
      VLOG(1) << "Bluetooth Adapter not present. Can't serve requestDevice.";
      RecordRequestDeviceOutcome(
          UMARequestDeviceOutcome::BLUETOOTH_ADAPTER_NOT_PRESENT);
      Send(new BluetoothMsg_RequestDeviceError(
          thread_id, request_id, WebBluetoothError::NoBluetoothAdapter));
      request_device_sessions_.erase(std::make_pair(thread_id, request_id));
      return;
    }
    // TODO(jyasskin): Once the dialog is available, the dialog should check for
    // the status of the adapter, i.e. check IsPowered() and
    // BluetoothAdapter::Observer::PoweredChanged, and inform the user. But
    // until the dialog is available we log/histogram the status and return
    // with a message.
    // https://crbug.com/517237
    if (!adapter_->IsPowered()) {
      RecordRequestDeviceOutcome(
          UMARequestDeviceOutcome::BLUETOOTH_ADAPTER_OFF);
      Send(new BluetoothMsg_RequestDeviceError(
          thread_id, request_id, WebBluetoothError::BluetoothAdapterOff));
      request_device_sessions_.erase(std::make_pair(thread_id, request_id));
      return;
    }
    adapter_->StartDiscoverySessionWithFilter(
        ComputeScanFilter(filters),
        base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStarted,
                   weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
        base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStartedError,
                   weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
  } else {
    VLOG(1) << "No BluetoothAdapter. Can't serve requestDevice.";
    RecordRequestDeviceOutcome(UMARequestDeviceOutcome::NO_BLUETOOTH_ADAPTER);
    Send(new BluetoothMsg_RequestDeviceError(
        thread_id, request_id, WebBluetoothError::NoBluetoothAdapter));
  }
  return;
}

void BluetoothDispatcherHost::OnConnectGATT(
    int thread_id,
    int request_id,
    const std::string& device_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::CONNECT_GATT);
  const base::TimeTicks start_time = base::TimeTicks::Now();

  // TODO(ortuno): Right now it's pointless to check if the domain has access to
  // the device, because any domain can connect to any device. But once
  // permissions are implemented we should check that the domain has access to
  // the device. https://crbug.com/484745
  device::BluetoothDevice* device = adapter_->GetDevice(device_instance_id);
  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    RecordConnectGATTOutcome(UMAConnectGATTOutcome::NO_DEVICE);
    Send(new BluetoothMsg_ConnectGATTError(
        thread_id, request_id, WebBluetoothError::DeviceNoLongerInRange));
    return;
  }
  device->CreateGattConnection(
      base::Bind(&BluetoothDispatcherHost::OnGATTConnectionCreated,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 device_instance_id, start_time),
      base::Bind(&BluetoothDispatcherHost::OnCreateGATTConnectionError,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 device_instance_id, start_time));
}

void BluetoothDispatcherHost::OnGetPrimaryService(
    int thread_id,
    int request_id,
    const std::string& device_instance_id,
    const std::string& service_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::GET_PRIMARY_SERVICE);
  RecordGetPrimaryServiceService(BluetoothUUID(service_uuid));

  // TODO(ortuno): Check if device_instance_id is in "allowed devices"
  // https://crbug.com/493459
  // TODO(ortuno): Check if service_uuid is in "allowed services"
  // https://crbug.com/493460
  // For now just wait a fixed time and call OnServiceDiscovered.
  // TODO(ortuno): Use callback once it's implemented http://crbug.com/484504
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BluetoothDispatcherHost::OnServicesDiscovered,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 device_instance_id, service_uuid),
      base::TimeDelta::FromSeconds(current_delay_time_));
}

void BluetoothDispatcherHost::OnGetCharacteristic(
    int thread_id,
    int request_id,
    const std::string& service_instance_id,
    const std::string& characteristic_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(UMAWebBluetoothFunction::GET_CHARACTERISTIC);
  RecordGetCharacteristicCharacteristic(characteristic_uuid);

  auto device_iter = service_to_device_.find(service_instance_id);
  // A service_instance_id not in the map implies a hostile renderer
  // because a renderer obtains the service id from this class and
  // it will be added to the map at that time.
  if (device_iter == service_to_device_.end()) {
    // Kill the renderer
    bad_message::ReceivedBadMessage(this, bad_message::BDH_INVALID_SERVICE_ID);
    return;
  }

  // TODO(ortuno): Check if domain has access to device.
  // https://crbug.com/493459
  device::BluetoothDevice* device =
      adapter_->GetDevice(device_iter->second /* device_instance_id */);

  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    RecordGetCharacteristicOutcome(UMAGetCharacteristicOutcome::NO_DEVICE);
    Send(new BluetoothMsg_GetCharacteristicError(
        thread_id, request_id, WebBluetoothError::DeviceNoLongerInRange));
    return;
  }

  // TODO(ortuno): Check if domain has access to service
  // http://crbug.com/493460
  device::BluetoothGattService* service =
      device->GetGattService(service_instance_id);
  if (!service) {
    RecordGetCharacteristicOutcome(UMAGetCharacteristicOutcome::NO_SERVICE);
    Send(new BluetoothMsg_GetCharacteristicError(
        thread_id, request_id, WebBluetoothError::ServiceNoLongerExists));
    return;
  }

  for (BluetoothGattCharacteristic* characteristic :
       service->GetCharacteristics()) {
    if (characteristic->GetUUID().canonical_value() == characteristic_uuid) {
      const std::string& characteristic_instance_id =
          characteristic->GetIdentifier();

      auto insert_result = characteristic_to_service_.insert(
          make_pair(characteristic_instance_id, service_instance_id));

      // If  value is already in map, DCHECK it's valid.
      if (!insert_result.second)
        DCHECK(insert_result.first->second == service_instance_id);

      RecordGetCharacteristicOutcome(UMAGetCharacteristicOutcome::SUCCESS);
      // TODO(ortuno): Use generated instance ID instead.
      // https://crbug.com/495379
      Send(new BluetoothMsg_GetCharacteristicSuccess(
          thread_id, request_id, characteristic_instance_id));
      return;
    }
  }
  RecordGetCharacteristicOutcome(UMAGetCharacteristicOutcome::NOT_FOUND);
  Send(new BluetoothMsg_GetCharacteristicError(
      thread_id, request_id, WebBluetoothError::CharacteristicNotFound));
}

void BluetoothDispatcherHost::OnReadValue(
    int thread_id,
    int request_id,
    const std::string& characteristic_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_READ_VALUE);

  auto characteristic_iter =
      characteristic_to_service_.find(characteristic_instance_id);
  // A characteristic_instance_id not in the map implies a hostile renderer
  // because a renderer obtains the characteristic id from this class and
  // it will be added to the map at that time.
  if (characteristic_iter == characteristic_to_service_.end()) {
    // Kill the renderer
    bad_message::ReceivedBadMessage(this,
                                    bad_message::BDH_INVALID_CHARACTERISTIC_ID);
    return;
  }
  const std::string& service_instance_id = characteristic_iter->second;

  auto device_iter = service_to_device_.find(service_instance_id);

  CHECK(device_iter != service_to_device_.end());

  device::BluetoothDevice* device =
      adapter_->GetDevice(device_iter->second /* device_instance_id */);
  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::NO_DEVICE);
    Send(new BluetoothMsg_ReadCharacteristicValueError(
        thread_id, request_id, WebBluetoothError::DeviceNoLongerInRange));
    return;
  }

  BluetoothGattService* service = device->GetGattService(service_instance_id);
  if (service == nullptr) {
    RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::NO_SERVICE);
    Send(new BluetoothMsg_ReadCharacteristicValueError(
        thread_id, request_id, WebBluetoothError::ServiceNoLongerExists));
    return;
  }

  BluetoothGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_instance_id);
  if (characteristic == nullptr) {
    RecordCharacteristicReadValueOutcome(
        UMAGATTOperationOutcome::NO_CHARACTERISTIC);
    Send(new BluetoothMsg_ReadCharacteristicValueError(
        thread_id, request_id,
        WebBluetoothError::CharacteristicNoLongerExists));
    return;
  }

  characteristic->ReadRemoteCharacteristic(
      base::Bind(&BluetoothDispatcherHost::OnCharacteristicValueRead,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
      base::Bind(&BluetoothDispatcherHost::OnCharacteristicReadValueError,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
}

void BluetoothDispatcherHost::OnWriteValue(
    int thread_id,
    int request_id,
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordWebBluetoothFunctionCall(
      UMAWebBluetoothFunction::CHARACTERISTIC_WRITE_VALUE);

  // Length check per step 3 of writeValue algorithm:
  // https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothgattcharacteristic-writevalue
  // We perform the length check on the renderer side. So if we
  // get a value with length > 512, we can assume it's a hostile
  // renderer and kill it.
  if (value.size() > 512) {
    bad_message::ReceivedBadMessage(
        this, bad_message::BDH_INVALID_WRITE_VALUE_LENGTH);
    return;
  }

  auto characteristic_iter =
      characteristic_to_service_.find(characteristic_instance_id);
  // A characteristic_instance_id not in the map implies a hostile renderer
  // because a renderer obtains the characteristic id from this class and
  // it will be added to the map at that time.
  if (characteristic_iter == characteristic_to_service_.end()) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::BDH_INVALID_CHARACTERISTIC_ID);
    return;
  }
  const std::string& service_instance_id = characteristic_iter->second;

  auto device_iter = service_to_device_.find(service_instance_id);

  CHECK(device_iter != service_to_device_.end());

  device::BluetoothDevice* device =
      adapter_->GetDevice(device_iter->second /* device_instance_id */);
  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::NO_DEVICE);
    Send(new BluetoothMsg_WriteCharacteristicValueError(
        thread_id, request_id, WebBluetoothError::DeviceNoLongerInRange));
    return;
  }

  BluetoothGattService* service = device->GetGattService(service_instance_id);
  if (service == nullptr) {
    RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::NO_SERVICE);
    Send(new BluetoothMsg_WriteCharacteristicValueError(
        thread_id, request_id, WebBluetoothError::ServiceNoLongerExists));
    return;
  }

  BluetoothGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_instance_id);
  if (characteristic == nullptr) {
    RecordCharacteristicWriteValueOutcome(
        UMAGATTOperationOutcome::NO_CHARACTERISTIC);
    Send(new BluetoothMsg_WriteCharacteristicValueError(
        thread_id, request_id,
        WebBluetoothError::CharacteristicNoLongerExists));
    return;
  }
  characteristic->WriteRemoteCharacteristic(
      value, base::Bind(&BluetoothDispatcherHost::OnWriteValueSuccess,
                        weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
      base::Bind(&BluetoothDispatcherHost::OnWriteValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
}

void BluetoothDispatcherHost::OnDiscoverySessionStarted(
    int thread_id,
    int request_id,
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BluetoothDispatcherHost::StopDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id,
                 base::Passed(&discovery_session)),
      base::TimeDelta::FromSeconds(current_delay_time_));
}

void BluetoothDispatcherHost::OnDiscoverySessionStartedError(int thread_id,
                                                             int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DLOG(WARNING) << "BluetoothDispatcherHost::OnDiscoverySessionStartedError";
  RecordRequestDeviceOutcome(UMARequestDeviceOutcome::DISCOVERY_START_FAILED);
  Send(new BluetoothMsg_RequestDeviceError(
      thread_id, request_id, WebBluetoothError::DiscoverySessionStartFailed));
  request_device_sessions_.erase(std::make_pair(thread_id, request_id));
}

void BluetoothDispatcherHost::StopDiscoverySession(
    int thread_id,
    int request_id,
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  discovery_session->Stop(
      base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStopped,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id),
      base::Bind(&BluetoothDispatcherHost::OnDiscoverySessionStoppedError,
                 weak_ptr_factory_.GetWeakPtr(), thread_id, request_id));
}

void BluetoothDispatcherHost::OnDiscoverySessionStopped(int thread_id,
                                                        int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto session =
      request_device_sessions_.find(std::make_pair(thread_id, request_id));
  CHECK(session != request_device_sessions_.end());
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  for (device::BluetoothDevice* device : devices) {
    VLOG(1) << "Device: " << device->GetName();
    VLOG(1) << "UUIDs: ";
    for (BluetoothUUID uuid : device->GetUUIDs())
      VLOG(1) << "\t" << uuid.canonical_value();
    if (MatchesFilters(*device, session->second.filters)) {
      content::BluetoothDevice device_ipc(
          device->GetAddress(),         // instance_id
          device->GetName(),            // name
          device->GetBluetoothClass(),  // device_class
          device->GetVendorIDSource(),  // vendor_id_source
          device->GetVendorID(),        // vendor_id
          device->GetProductID(),       // product_id
          device->GetDeviceID(),        // product_version
          device->IsPaired(),           // paired
          content::BluetoothDevice::UUIDsFromBluetoothUUIDs(
              device->GetUUIDs()));  // uuids
      RecordRequestDeviceOutcome(UMARequestDeviceOutcome::SUCCESS);
      Send(new BluetoothMsg_RequestDeviceSuccess(thread_id, request_id,
                                                 device_ipc));
      request_device_sessions_.erase(session);
      return;
    }
  }
  RecordRequestDeviceOutcome(
      UMARequestDeviceOutcome::NO_MATCHING_DEVICES_FOUND);
  Send(new BluetoothMsg_RequestDeviceError(thread_id, request_id,
                                           WebBluetoothError::NoDevicesFound));
  request_device_sessions_.erase(session);
}

void BluetoothDispatcherHost::OnDiscoverySessionStoppedError(int thread_id,
                                                             int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DLOG(WARNING) << "BluetoothDispatcherHost::OnDiscoverySessionStoppedError";
  RecordRequestDeviceOutcome(UMARequestDeviceOutcome::DISCOVERY_STOP_FAILED);
  Send(new BluetoothMsg_RequestDeviceError(
      thread_id, request_id, WebBluetoothError::DiscoverySessionStopFailed));
  request_device_sessions_.erase(std::make_pair(thread_id, request_id));
}

void BluetoothDispatcherHost::OnGATTConnectionCreated(
    int thread_id,
    int request_id,
    const std::string& device_instance_id,
    base::TimeTicks start_time,
    scoped_ptr<device::BluetoothGattConnection> connection) {
  // TODO(ortuno): Save the BluetoothGattConnection so we can disconnect
  // from it.
  RecordConnectGATTTimeSuccess(base::TimeTicks::Now() - start_time);
  RecordConnectGATTOutcome(UMAConnectGATTOutcome::SUCCESS);
  Send(new BluetoothMsg_ConnectGATTSuccess(thread_id, request_id,
                                           device_instance_id));
}

void BluetoothDispatcherHost::OnCreateGATTConnectionError(
    int thread_id,
    int request_id,
    const std::string& device_instance_id,
    base::TimeTicks start_time,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  // There was an error creating the ATT Bearer so we reject with
  // NetworkError.
  // https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetoothdevice-connectgatt
  RecordConnectGATTTimeFailed(base::TimeTicks::Now() - start_time);
  // RecordConnectGATTOutcome is called by TranslateConnectError.
  Send(new BluetoothMsg_ConnectGATTError(thread_id, request_id,
                                         TranslateConnectError(error_code)));
}

void BluetoothDispatcherHost::OnServicesDiscovered(
    int thread_id,
    int request_id,
    const std::string& device_instance_id,
    const std::string& service_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  device::BluetoothDevice* device = adapter_->GetDevice(device_instance_id);
  if (device == nullptr) {  // See "NETWORK_ERROR Note" above.
    RecordGetPrimaryServiceOutcome(UMAGetPrimaryServiceOutcome::NO_DEVICE);
    Send(new BluetoothMsg_GetPrimaryServiceError(
        thread_id, request_id, WebBluetoothError::DeviceNoLongerInRange));
    return;
  }
  for (BluetoothGattService* service : device->GetGattServices()) {
    if (service->GetUUID().canonical_value() == service_uuid) {
      // TODO(ortuno): Use generated instance ID instead.
      // https://crbug.com/495379
      const std::string& service_identifier = service->GetIdentifier();
      auto insert_result = service_to_device_.insert(
          make_pair(service_identifier, device_instance_id));

      // If a value is already in map, DCHECK it's valid.
      if (!insert_result.second)
        DCHECK(insert_result.first->second == device_instance_id);

      RecordGetPrimaryServiceOutcome(UMAGetPrimaryServiceOutcome::SUCCESS);
      Send(new BluetoothMsg_GetPrimaryServiceSuccess(thread_id, request_id,
                                                     service_identifier));
      return;
    }
  }
  RecordGetPrimaryServiceOutcome(UMAGetPrimaryServiceOutcome::NOT_FOUND);
  Send(new BluetoothMsg_GetPrimaryServiceError(
      thread_id, request_id, WebBluetoothError::ServiceNotFound));
}

void BluetoothDispatcherHost::OnCharacteristicValueRead(
    int thread_id,
    int request_id,
    const std::vector<uint8>& value) {
  RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  Send(new BluetoothMsg_ReadCharacteristicValueSuccess(thread_id, request_id,
                                                       value));
}

void BluetoothDispatcherHost::OnCharacteristicReadValueError(
    int thread_id,
    int request_id,
    device::BluetoothGattService::GattErrorCode error_code) {
  // TranslateGATTError calls RecordGATTOperationOutcome.
  Send(new BluetoothMsg_ReadCharacteristicValueError(
      thread_id, request_id,
      TranslateGATTError(error_code, UMAGATTOperation::CHARACTERISTIC_READ)));
}

void BluetoothDispatcherHost::OnWriteValueSuccess(int thread_id,
                                                  int request_id) {
  RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  Send(new BluetoothMsg_WriteCharacteristicValueSuccess(thread_id, request_id));
}

void BluetoothDispatcherHost::OnWriteValueFailed(
    int thread_id,
    int request_id,
    device::BluetoothGattService::GattErrorCode error_code) {
  // TranslateGATTError calls RecordGATTOperationOutcome.
  Send(new BluetoothMsg_WriteCharacteristicValueError(
      thread_id, request_id,
      TranslateGATTError(error_code, UMAGATTOperation::CHARACTERISTIC_WRITE)));
}

}  // namespace content
