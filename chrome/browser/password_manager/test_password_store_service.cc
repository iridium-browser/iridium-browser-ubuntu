// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/test_password_store_service.h"

#include "components/password_manager/core/browser/test_password_store.h"

// static
scoped_ptr<KeyedService> TestPasswordStoreService::Build(
    content::BrowserContext* /*profile*/) {
  scoped_refptr<password_manager::PasswordStore> store(
      new password_manager::TestPasswordStore);
  if (!store.get() || !store->Init(syncer::SyncableService::StartSyncFlare()))
    return nullptr;
  return scoped_ptr<KeyedService>(new TestPasswordStoreService(store));
}

TestPasswordStoreService::TestPasswordStoreService(
    scoped_refptr<password_manager::PasswordStore> password_store)
    : PasswordStoreService(password_store) {}

TestPasswordStoreService::~TestPasswordStoreService() {}
