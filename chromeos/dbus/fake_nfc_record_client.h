// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_NFC_RECORD_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_NFC_RECORD_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/nfc_record_client.h"
#include "dbus/object_path.h"

namespace chromeos {

// FakeNfcRecordClient simulates the behavior of the NFC record objects and is
// used both in test cases in place of a mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeNfcRecordClient : public NfcRecordClient {
 public:
  // Paths of the records exposed.
  static const char kDeviceSmartPosterRecordPath[];
  static const char kDeviceTextRecordPath[];
  static const char kDeviceUriRecordPath[];
  static const char kTagRecordPath[];

  // Properties structure that provides fake behavior for D-Bus calls.
  struct Properties : public NfcRecordClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet overrides.
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeNfcRecordClient();
  ~FakeNfcRecordClient() override;

  // NfcTagClient overrides.
  void Init(dbus::Bus* bus) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::vector<dbus::ObjectPath> GetRecordsForDevice(
      const dbus::ObjectPath& device_path) override;
  std::vector<dbus::ObjectPath> GetRecordsForTag(
      const dbus::ObjectPath& tag_path) override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;

  // Adds or removes the fake record objects and notifies the observers.
  void SetDeviceRecordsVisible(bool visible);
  void SetTagRecordsVisible(bool visible);

  // Modifies the contents of the tag record. |attributes| should be the
  // same as the argument to NfcTagClient::Write. Each field will be directly
  // assigned to the underlying record based on the type property, with
  // no validity checking. Invalid tag content can be passed here to test
  // the case where the remote application returns an incorrectly formatted
  // record.
  bool WriteTagRecord(const base::DictionaryValue& attributes);

 private:
  // Property changed callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Called by Properties* structures when GetAll is called.
  void OnPropertiesReceived(const dbus::ObjectPath& object_path);

  // If true, the records are currently visible.
  bool device_records_visible_;
  bool tag_records_visible_;

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer> observers_;

  // Fake properties that are returned for the fake records.
  std::unique_ptr<Properties> device_smart_poster_record_properties_;
  std::unique_ptr<Properties> device_text_record_properties_;
  std::unique_ptr<Properties> device_uri_record_properties_;
  std::unique_ptr<Properties> tag_record_properties_;

  DISALLOW_COPY_AND_ASSIGN(FakeNfcRecordClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_NFC_RECORD_CLIENT_H_
