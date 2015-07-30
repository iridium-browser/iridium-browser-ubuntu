// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_shill_device_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

std::string kSimPin = "1111";
std::string kFailedMessage = "Failed";

void ErrorFunction(const std::string& device_path,
                   const std::string& error_name,
                   const std::string& error_message) {
  LOG(ERROR) << "Shill Error for: " << device_path
             << ": " << error_name << " : " << error_message;
}

void PostError(const std::string& error,
               const ShillDeviceClient::ErrorCallback& error_callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(error_callback, error, kFailedMessage));
}

void PostNotFoundError(const ShillDeviceClient::ErrorCallback& error_callback) {
  PostError(shill::kErrorResultNotFound, error_callback);
}

bool IsReadOnlyProperty(const std::string& name) {
  if (name == shill::kCarrierProperty)
    return true;
  return false;
}

}  // namespace

FakeShillDeviceClient::FakeShillDeviceClient()
    : initial_tdls_busy_count_(0),
      tdls_busy_count_(0),
      weak_ptr_factory_(this) {
}

FakeShillDeviceClient::~FakeShillDeviceClient() {
  STLDeleteContainerPairSecondPointers(
      observer_list_.begin(), observer_list_.end());
}

// ShillDeviceClient overrides.

void FakeShillDeviceClient::Init(dbus::Bus* bus) {}

void FakeShillDeviceClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(device_path).AddObserver(observer);
}

void FakeShillDeviceClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& device_path,
    ShillPropertyChangedObserver* observer) {
  GetObserverList(device_path).RemoveObserver(observer);
}

void FakeShillDeviceClient::GetProperties(
    const dbus::ObjectPath& device_path,
    const DictionaryValueCallback& callback) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeShillDeviceClient::PassStubDeviceProperties,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_path, callback));
}

void FakeShillDeviceClient::ProposeScan(
    const dbus::ObjectPath& device_path,
    const VoidDBusMethodCallback& callback) {
  PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
}

void FakeShillDeviceClient::SetProperty(const dbus::ObjectPath& device_path,
                                        const std::string& name,
                                        const base::Value& value,
                                        const base::Closure& callback,
                                        const ErrorCallback& error_callback) {
  if (IsReadOnlyProperty(name))
    PostError(shill::kErrorResultInvalidArguments, error_callback);
  SetPropertyInternal(device_path, name, value, callback, error_callback);
}

void FakeShillDeviceClient::SetPropertyInternal(
    const dbus::ObjectPath& device_path,
    const std::string& name,
    const base::Value& value,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  base::DictionaryValue* device_properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(device_path.value(),
                                                       &device_properties)) {
    PostNotFoundError(error_callback);
    return;
  }
  device_properties->SetWithoutPathExpansion(name, value.DeepCopy());
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&FakeShillDeviceClient::NotifyObserversPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr(), device_path, name));
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::ClearProperty(
    const dbus::ObjectPath& device_path,
    const std::string& name,
    const VoidDBusMethodCallback& callback) {
  base::DictionaryValue* device_properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(device_path.value(),
                                                       &device_properties)) {
    PostVoidCallback(callback, DBUS_METHOD_CALL_FAILURE);
    return;
  }
  device_properties->RemoveWithoutPathExpansion(name, NULL);
  PostVoidCallback(callback, DBUS_METHOD_CALL_SUCCESS);
}

void FakeShillDeviceClient::AddIPConfig(
    const dbus::ObjectPath& device_path,
    const std::string& method,
    const ObjectPathDBusMethodCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback,
                                                    DBUS_METHOD_CALL_SUCCESS,
                                                    dbus::ObjectPath()));
}

void FakeShillDeviceClient::RequirePin(const dbus::ObjectPath& device_path,
                                       const std::string& pin,
                                       bool require,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
  VLOG(1) << "RequirePin: " << device_path.value();
  if (pin != kSimPin) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback, shill::kErrorResultIncorrectPin, ""));
    return;
  }
  base::DictionaryValue* device_properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(device_path.value(),
                                                       &device_properties)) {
    PostNotFoundError(error_callback);
    return;
  }
  base::DictionaryValue* simlock_dict = NULL;
  if (!device_properties->GetDictionaryWithoutPathExpansion(
          shill::kSIMLockStatusProperty, &simlock_dict)) {
    simlock_dict = new base::DictionaryValue;
    device_properties->SetWithoutPathExpansion(
        shill::kSIMLockStatusProperty, simlock_dict);
  }
  simlock_dict->Clear();
  simlock_dict->SetBoolean(shill::kSIMLockEnabledProperty, require);
  // TODO(stevenjb): Investigate why non-empty value breaks UI.
  std::string lock_type = "";  // shill::kSIMLockPin
  simlock_dict->SetString(shill::kSIMLockTypeProperty, lock_type);
  simlock_dict->SetInteger(shill::kSIMLockRetriesLeftProperty, 5);

  NotifyObserversPropertyChanged(device_path, shill::kSIMLockStatusProperty);
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::EnterPin(const dbus::ObjectPath& device_path,
                                     const std::string& pin,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  VLOG(1) << "EnterPin: " << device_path.value();
  if (pin != kSimPin) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback, shill::kErrorResultIncorrectPin, ""));
    return;
  }
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::UnblockPin(const dbus::ObjectPath& device_path,
                                       const std::string& puk,
                                       const std::string& pin,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
  VLOG(1) << "UnblockPin: " << device_path.value();
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::ChangePin(const dbus::ObjectPath& device_path,
                                      const std::string& old_pin,
                                      const std::string& new_pin,
                                      const base::Closure& callback,
                                      const ErrorCallback& error_callback) {
  VLOG(1) << "ChangePin: " << device_path.value();
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::Register(const dbus::ObjectPath& device_path,
                                     const std::string& network_id,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::SetCarrier(const dbus::ObjectPath& device_path,
                                       const std::string& carrier,
                                       const base::Closure& callback,
                                       const ErrorCallback& error_callback) {
  SetPropertyInternal(device_path, shill::kCarrierProperty,
                      base::StringValue(carrier), callback, error_callback);
}

void FakeShillDeviceClient::Reset(const dbus::ObjectPath& device_path,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::PerformTDLSOperation(
    const dbus::ObjectPath& device_path,
    const std::string& operation,
    const std::string& peer,
    const StringCallback& callback,
    const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }
  // Use -1 to emulate a TDLS failure.
  if (tdls_busy_count_ == -1) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(error_callback, shill::kErrorDhcpFailed, "Failed"));
    return;
  }
  if (operation != shill::kTDLSStatusOperation && tdls_busy_count_ > 0) {
    --tdls_busy_count_;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(error_callback, shill::kErrorResultInProgress,
                              "In-Progress"));
    return;
  }

  tdls_busy_count_ = initial_tdls_busy_count_;

  std::string result;
  if (operation == shill::kTDLSDiscoverOperation) {
    if (tdls_state_.empty())
      tdls_state_ = shill::kTDLSDisconnectedState;
  } else if (operation == shill::kTDLSSetupOperation) {
    if (tdls_state_.empty())
      tdls_state_ = shill::kTDLSConnectedState;
  } else if (operation == shill::kTDLSTeardownOperation) {
    if (tdls_state_.empty())
      tdls_state_ = shill::kTDLSDisconnectedState;
  } else if (operation == shill::kTDLSStatusOperation) {
    result = tdls_state_;
  }

  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, result));
}

void FakeShillDeviceClient::AddWakeOnPacketConnection(
    const dbus::ObjectPath& device_path,
    const net::IPEndPoint& ip_endpoint,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (!stub_devices_.HasKey(device_path.value())) {
    PostNotFoundError(error_callback);
    return;
  }

  wake_on_packet_connections_[device_path].insert(ip_endpoint);

  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::RemoveWakeOnPacketConnection(
    const dbus::ObjectPath& device_path,
    const net::IPEndPoint& ip_endpoint,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  const auto device_iter = wake_on_packet_connections_.find(device_path);
  if (!stub_devices_.HasKey(device_path.value()) ||
      device_iter == wake_on_packet_connections_.end()) {
    PostNotFoundError(error_callback);
    return;
  }

  const auto endpoint_iter = device_iter->second.find(ip_endpoint);
  if (endpoint_iter == device_iter->second.end()) {
    PostNotFoundError(error_callback);
    return;
  }

  device_iter->second.erase(endpoint_iter);

  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

void FakeShillDeviceClient::RemoveAllWakeOnPacketConnections(
    const dbus::ObjectPath& device_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  const auto iter = wake_on_packet_connections_.find(device_path);
  if (!stub_devices_.HasKey(device_path.value()) ||
      iter == wake_on_packet_connections_.end()) {
    PostNotFoundError(error_callback);
    return;
  }

  wake_on_packet_connections_.erase(iter);

  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

ShillDeviceClient::TestInterface* FakeShillDeviceClient::GetTestInterface() {
  return this;
}

// ShillDeviceClient::TestInterface overrides.

void FakeShillDeviceClient::AddDevice(const std::string& device_path,
                                      const std::string& type,
                                      const std::string& name) {
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      AddDevice(device_path);

  base::DictionaryValue* properties = GetDeviceProperties(device_path);
  properties->SetStringWithoutPathExpansion(shill::kTypeProperty, type);
  properties->SetStringWithoutPathExpansion(shill::kNameProperty, name);
  properties->SetStringWithoutPathExpansion(shill::kDBusObjectProperty,
                                            device_path);
  properties->SetStringWithoutPathExpansion(
      shill::kDBusServiceProperty, modemmanager::kModemManager1ServiceName);
  if (type == shill::kTypeCellular) {
    properties->SetBooleanWithoutPathExpansion(
        shill::kCellularAllowRoamingProperty, false);
  }
}

void FakeShillDeviceClient::RemoveDevice(const std::string& device_path) {
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      RemoveDevice(device_path);

  stub_devices_.RemoveWithoutPathExpansion(device_path, NULL);
}

void FakeShillDeviceClient::ClearDevices() {
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      ClearDevices();

  stub_devices_.Clear();
}

void FakeShillDeviceClient::SetDeviceProperty(const std::string& device_path,
                                              const std::string& name,
                                              const base::Value& value) {
  VLOG(1) << "SetDeviceProperty: " << device_path
          << ": " << name << " = " << value;
  SetPropertyInternal(dbus::ObjectPath(device_path), name, value,
                      base::Bind(&base::DoNothing),
                      base::Bind(&ErrorFunction, device_path));
}

std::string FakeShillDeviceClient::GetDevicePathForType(
    const std::string& type) {
  for (base::DictionaryValue::Iterator iter(stub_devices_);
       !iter.IsAtEnd(); iter.Advance()) {
    const base::DictionaryValue* properties = NULL;
    if (!iter.value().GetAsDictionary(&properties))
      continue;
    std::string prop_type;
    if (!properties->GetStringWithoutPathExpansion(
            shill::kTypeProperty, &prop_type) ||
        prop_type != type)
      continue;
    return iter.key();
  }
  return std::string();
}

void FakeShillDeviceClient::SetTDLSBusyCount(int count) {
  tdls_busy_count_ = std::max(count, -1);
}

void FakeShillDeviceClient::SetTDLSState(const std::string& state) {
  tdls_state_ = state;
}

void FakeShillDeviceClient::PassStubDeviceProperties(
    const dbus::ObjectPath& device_path,
    const DictionaryValueCallback& callback) const {
  const base::DictionaryValue* device_properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(
      device_path.value(), &device_properties)) {
    base::DictionaryValue empty_dictionary;
    callback.Run(DBUS_METHOD_CALL_FAILURE, empty_dictionary);
    return;
  }
  callback.Run(DBUS_METHOD_CALL_SUCCESS, *device_properties);
}

// Posts a task to run a void callback with status code |status|.
void FakeShillDeviceClient::PostVoidCallback(
    const VoidDBusMethodCallback& callback,
    DBusMethodCallStatus status) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, status));
}

void FakeShillDeviceClient::NotifyObserversPropertyChanged(
    const dbus::ObjectPath& device_path,
    const std::string& property) {
  base::DictionaryValue* dict = NULL;
  std::string path = device_path.value();
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(path, &dict)) {
    LOG(ERROR) << "Notify for unknown service: " << path;
    return;
  }
  base::Value* value = NULL;
  if (!dict->GetWithoutPathExpansion(property, &value)) {
    LOG(ERROR) << "Notify for unknown property: "
        << path << " : " << property;
    return;
  }
  FOR_EACH_OBSERVER(ShillPropertyChangedObserver,
                    GetObserverList(device_path),
                    OnPropertyChanged(property, *value));
}

base::DictionaryValue* FakeShillDeviceClient::GetDeviceProperties(
    const std::string& device_path) {
  base::DictionaryValue* properties = NULL;
  if (!stub_devices_.GetDictionaryWithoutPathExpansion(
      device_path, &properties)) {
    properties = new base::DictionaryValue;
    stub_devices_.SetWithoutPathExpansion(device_path, properties);
  }
  return properties;
}

FakeShillDeviceClient::PropertyObserverList&
FakeShillDeviceClient::GetObserverList(const dbus::ObjectPath& device_path) {
  std::map<dbus::ObjectPath, PropertyObserverList*>::iterator iter =
      observer_list_.find(device_path);
  if (iter != observer_list_.end())
    return *(iter->second);
  PropertyObserverList* observer_list = new PropertyObserverList();
  observer_list_[device_path] = observer_list;
  return *observer_list;
}

}  // namespace chromeos
