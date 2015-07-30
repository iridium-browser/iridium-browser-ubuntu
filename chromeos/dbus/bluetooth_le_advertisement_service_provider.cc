// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/bluetooth_le_advertisement_service_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/threading/platform_thread.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_le_advertisement_service_provider.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {
const char kErrorInvalidArgs[] = "org.freedesktop.DBus.Error.InvalidArgs";
}  // namespace

// The BluetoothAdvertisementServiceProvider implementation used in production.
class BluetoothAdvertisementServiceProviderImpl
    : public BluetoothLEAdvertisementServiceProvider {
 public:
  BluetoothAdvertisementServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      Delegate* delegate,
      AdvertisementType type,
      scoped_ptr<UUIDList> service_uuids,
      scoped_ptr<ManufacturerData> manufacturer_data,
      scoped_ptr<UUIDList> solicit_uuids,
      scoped_ptr<ServiceData> service_data)
      : origin_thread_id_(base::PlatformThread::CurrentId()),
        bus_(bus),
        delegate_(delegate),
        type_(type),
        service_uuids_(service_uuids.Pass()),
        manufacturer_data_(manufacturer_data.Pass()),
        solicit_uuids_(solicit_uuids.Pass()),
        service_data_(service_data.Pass()),
        weak_ptr_factory_(this) {
    DCHECK(bus);
    DCHECK(delegate);

    VLOG(1) << "Creating Bluetooth Advertisement: " << object_path_.value();

    object_path_ = object_path;
    exported_object_ = bus_->GetExportedObject(object_path_);

    // Export Bluetooth Advertisement interface methods.
    exported_object_->ExportMethod(
        bluetooth_advertisement::kBluetoothAdvertisementInterface,
        bluetooth_advertisement::kRelease,
        base::Bind(&BluetoothAdvertisementServiceProviderImpl::Release,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAdvertisementServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    // Export dbus property methods.
    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGet,
        base::Bind(&BluetoothAdvertisementServiceProviderImpl::Get,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAdvertisementServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));

    exported_object_->ExportMethod(
        dbus::kDBusPropertiesInterface, dbus::kDBusPropertiesGetAll,
        base::Bind(&BluetoothAdvertisementServiceProviderImpl::GetAll,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothAdvertisementServiceProviderImpl::OnExported,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  ~BluetoothAdvertisementServiceProviderImpl() override {
    VLOG(1) << "Cleaning up Bluetooth Advertisement: " << object_path_.value();

    // Unregister the object path so we can reuse with a new agent.
    bus_->UnregisterExportedObject(object_path_);
  }

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() {
    return base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Called by dbus:: when this advertisement is unregistered from the Bluetooth
  // daemon, generally by our request.
  void Release(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender) {
    DCHECK(OnOriginThread());
    DCHECK(delegate_);

    delegate_->Released();
  }

  // Called by dbus:: when the Bluetooth daemon fetches a single property of
  // the descriptor.
  void Get(dbus::MethodCall* method_call,
           dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "BluetoothAdvertisementServiceProvider::Get: "
            << object_path_.value();
    DCHECK(OnOriginThread());

    dbus::MessageReader reader(method_call);

    std::string interface_name;
    std::string property_name;
    if (!reader.PopString(&interface_name) ||
        !reader.PopString(&property_name) || reader.HasMoreData()) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(method_call, kErrorInvalidArgs,
                                              "Expected 'ss'.");
      response_sender.Run(error_response.Pass());
      return;
    }

    // Only the advertisement interface is supported.
    if (interface_name !=
        bluetooth_advertisement::kBluetoothAdvertisementInterface) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      response_sender.Run(error_response.Pass());
      return;
    }

    scoped_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter variant_writer(NULL);

    if (property_name == bluetooth_advertisement::kTypeProperty) {
      writer.OpenVariant("s", &variant_writer);
      if (type_ == ADVERTISEMENT_TYPE_BROADCAST) {
        variant_writer.AppendString("broadcast");
      } else {
        variant_writer.AppendString("peripheral");
      }
    } else if ((property_name ==
                bluetooth_advertisement::kServiceUUIDsProperty) &&
               service_uuids_) {
      writer.OpenVariant("as", &variant_writer);
      variant_writer.AppendArrayOfStrings(*service_uuids_);
    } else if ((property_name ==
                bluetooth_advertisement::kSolicitUUIDsProperty) &&
               solicit_uuids_) {
      writer.OpenVariant("as", &variant_writer);
      variant_writer.AppendArrayOfStrings(*solicit_uuids_);
    } else if ((property_name ==
                bluetooth_advertisement::kManufacturerDataProperty) &&
               manufacturer_data_) {
      writer.OpenVariant("o", &variant_writer);
      AppendManufacturerDataVariant(&variant_writer);
    } else if ((property_name ==
                bluetooth_advertisement::kServiceDataProperty) &&
               service_data_) {
      writer.OpenVariant("o", &variant_writer);
      AppendServiceDataVariant(&variant_writer);
    } else {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such property: '" + property_name + "'.");
      response_sender.Run(error_response.Pass());
    }

    writer.CloseContainer(&variant_writer);
    response_sender.Run(response.Pass());
  }

  // Called by dbus:: when the Bluetooth daemon fetches all properties of the
  // descriptor.
  void GetAll(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender) {
    VLOG(2) << "BluetoothAdvertisementServiceProvider::GetAll: "
            << object_path_.value();
    DCHECK(OnOriginThread());

    dbus::MessageReader reader(method_call);

    std::string interface_name;
    if (!reader.PopString(&interface_name) || reader.HasMoreData()) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(method_call, kErrorInvalidArgs,
                                              "Expected 's'.");
      response_sender.Run(error_response.Pass());
      return;
    }

    // Only the advertisement interface is supported.
    if (interface_name !=
        bluetooth_advertisement::kBluetoothAdvertisementInterface) {
      scoped_ptr<dbus::ErrorResponse> error_response =
          dbus::ErrorResponse::FromMethodCall(
              method_call, kErrorInvalidArgs,
              "No such interface: '" + interface_name + "'.");
      response_sender.Run(error_response.Pass());
      return;
    }

    response_sender.Run(CreateGetAllResponse(method_call).Pass());
  }

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success) {
    LOG_IF(WARNING, !success) << "Failed to export " << interface_name << "."
                              << method_name;
  }

  // Helper for populating the DBus response with the advertisement data.
  scoped_ptr<dbus::Response> CreateGetAllResponse(
      dbus::MethodCall* method_call) {
    VLOG(2) << "Descriptor value obtained from delegate. Responding to "
            << "GetAll.";

    scoped_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);

    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter array_writer(NULL);

    writer.OpenArray("{sv}", &array_writer);

    AppendType(&array_writer);
    AppendServiceUUIDs(&array_writer);
    AppendManufacturerData(&array_writer);
    AppendSolicitUUIDs(&array_writer);
    AppendServiceData(&array_writer);

    writer.CloseContainer(&array_writer);
    return response;
  }

  // Called by the Delegate in response to a successful method call to get the
  // descriptor value.
  void OnGet(dbus::MethodCall* method_call,
             dbus::ExportedObject::ResponseSender response_sender,
             const std::vector<uint8>& value) {
    VLOG(2) << "Returning descriptor value obtained from delegate.";
    scoped_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    dbus::MessageWriter variant_writer(NULL);

    writer.OpenVariant("ay", &variant_writer);
    variant_writer.AppendArrayOfBytes(value.data(), value.size());
    writer.CloseContainer(&variant_writer);

    response_sender.Run(response.Pass());
  }

  void AppendType(dbus::MessageWriter* array_writer) {
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(bluetooth_advertisement::kTypeProperty);
    if (type_ == ADVERTISEMENT_TYPE_BROADCAST) {
      dict_entry_writer.AppendString("broadcast");
    } else {
      dict_entry_writer.AppendString("peripheral");
    }
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendServiceUUIDs(dbus::MessageWriter* array_writer) {
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kServiceUUIDsProperty);
    dict_entry_writer.AppendArrayOfStrings(*service_uuids_);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendManufacturerData(dbus::MessageWriter* array_writer) {
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kManufacturerDataProperty);
    dbus::MessageWriter variant_writer(NULL);
    dict_entry_writer.OpenVariant("a{qay}", &variant_writer);
    AppendManufacturerDataVariant(&variant_writer);
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendSolicitUUIDs(dbus::MessageWriter* array_writer) {
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kSolicitUUIDsProperty);
    dict_entry_writer.AppendArrayOfStrings(*solicit_uuids_);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendServiceData(dbus::MessageWriter* array_writer) {
    dbus::MessageWriter dict_entry_writer(NULL);
    array_writer->OpenDictEntry(&dict_entry_writer);
    dict_entry_writer.AppendString(
        bluetooth_advertisement::kServiceDataProperty);
    dbus::MessageWriter variant_writer(NULL);
    dict_entry_writer.OpenVariant("a{say}", &variant_writer);
    AppendServiceDataVariant(&variant_writer);
    dict_entry_writer.CloseContainer(&variant_writer);
    array_writer->CloseContainer(&dict_entry_writer);
  }

  void AppendManufacturerDataVariant(dbus::MessageWriter* writer) {
    dbus::MessageWriter array_writer(NULL);
    writer->OpenArray("{qay}", &array_writer);
    for (const auto& m : *manufacturer_data_) {
      dbus::MessageWriter entry_writer(NULL);

      array_writer.OpenDictEntry(&entry_writer);

      entry_writer.AppendUint32(m.first);
      entry_writer.AppendArrayOfBytes(vector_as_array(&m.second),
                                      m.second.size());

      array_writer.CloseContainer(&entry_writer);
    }
    writer->CloseContainer(&array_writer);
  }

  void AppendServiceDataVariant(dbus::MessageWriter* writer) {
    dbus::MessageWriter array_writer(NULL);
    writer->OpenArray("{say}", &array_writer);
    for (const auto& m : *service_data_) {
      dbus::MessageWriter entry_writer(NULL);

      array_writer.OpenDictEntry(&entry_writer);

      entry_writer.AppendString(m.first);
      entry_writer.AppendArrayOfBytes(vector_as_array(&m.second),
                                      m.second.size());

      array_writer.CloseContainer(&entry_writer);
    }
    writer->CloseContainer(&array_writer);
  }

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  // D-Bus bus object is exported on, not owned by this object and must
  // outlive it.
  dbus::Bus* bus_;

  // All incoming method calls are passed on to the Delegate and a callback
  // passed to generate the reply. |delegate_| is generally the object that
  // owns this one, and must outlive it.
  Delegate* delegate_;

  // Advertisement data that needs to be provided to BlueZ when requested.
  AdvertisementType type_;
  scoped_ptr<UUIDList> service_uuids_;
  scoped_ptr<ManufacturerData> manufacturer_data_;
  scoped_ptr<UUIDList> solicit_uuids_;
  scoped_ptr<ServiceData> service_data_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdvertisementServiceProviderImpl>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdvertisementServiceProviderImpl);
};

BluetoothLEAdvertisementServiceProvider::
    BluetoothLEAdvertisementServiceProvider() {
}

BluetoothLEAdvertisementServiceProvider::
    ~BluetoothLEAdvertisementServiceProvider() {
}

// static
scoped_ptr<BluetoothLEAdvertisementServiceProvider>
BluetoothLEAdvertisementServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    Delegate* delegate,
    AdvertisementType type,
    scoped_ptr<UUIDList> service_uuids,
    scoped_ptr<ManufacturerData> manufacturer_data,
    scoped_ptr<UUIDList> solicit_uuids,
    scoped_ptr<ServiceData> service_data) {
  if (!DBusThreadManager::Get()->IsUsingStub(DBusClientBundle::BLUETOOTH)) {
    return make_scoped_ptr(new BluetoothAdvertisementServiceProviderImpl(
        bus, object_path, delegate, type, service_uuids.Pass(),
        manufacturer_data.Pass(), solicit_uuids.Pass(), service_data.Pass()));
  } else {
    return make_scoped_ptr(
        new FakeBluetoothLEAdvertisementServiceProvider(object_path, delegate));
  }
}

}  // namespace chromeos
