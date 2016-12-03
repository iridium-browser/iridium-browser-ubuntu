// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CRNET_SDCH_OWNER_PREF_STORAGE_H_
#define IOS_CRNET_SDCH_OWNER_PREF_STORAGE_H_

#include <memory>

#include "base/macros.h"
#include "components/prefs/pref_store.h"
#include "net/sdch/sdch_owner.h"

class PersistentPrefStore;

// Provides an implementation of SdchOwner::PrefStorage that maps to
// Chrome's preferences system.
class SdchOwnerPrefStorage
    : public net::SdchOwner::PrefStorage,
      public PrefStore::Observer {
 public:
  // The storage must outlive this class.
  explicit SdchOwnerPrefStorage(PersistentPrefStore* storage);
  ~SdchOwnerPrefStorage() override;

  ReadError GetReadError() const override;
  bool GetValue(const base::DictionaryValue** result) const override;
  bool GetMutableValue(base::DictionaryValue** result) override;
  void SetValue(std::unique_ptr<base::DictionaryValue> value) override;
  void ReportValueChanged() override;
  bool IsInitializationComplete() override;
  void StartObservingInit(net::SdchOwner* observer) override;
  void StopObservingInit() override;

 private:
  // PrefStore::Observer implementation.
  void OnPrefValueChanged(const std::string& key) override;
  void OnInitializationCompleted(bool succeeded) override;

  PersistentPrefStore* storage_;  // Non-owning.
  const std::string storage_key_;

  net::SdchOwner* init_observer_;  // Non-owning.

  DISALLOW_COPY_AND_ASSIGN(SdchOwnerPrefStorage);
};

#endif  // IOS_CRNET_SDCH_OWNER_PREF_STORAGE_H_
