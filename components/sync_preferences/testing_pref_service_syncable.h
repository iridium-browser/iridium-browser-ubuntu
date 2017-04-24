// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_TESTING_PREF_SERVICE_SYNCABLE_H_
#define COMPONENTS_SYNC_PREFERENCES_TESTING_PREF_SERVICE_SYNCABLE_H_

#include "base/macros.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace sync_preferences {

// Test version of PrefServiceSyncable.
class TestingPrefServiceSyncable
    : public TestingPrefServiceBase<PrefServiceSyncable,
                                    user_prefs::PrefRegistrySyncable> {
 public:
  TestingPrefServiceSyncable();
  TestingPrefServiceSyncable(TestingPrefStore* managed_prefs,
                             TestingPrefStore* extension_prefs,
                             TestingPrefStore* user_prefs,
                             TestingPrefStore* recommended_prefs,
                             user_prefs::PrefRegistrySyncable* pref_registry,
                             PrefNotifierImpl* pref_notifier);
  ~TestingPrefServiceSyncable() override;

  // This is provided as a convenience; on a production PrefService
  // you would do all registrations before constructing it, passing it
  // a PrefRegistry via its constructor (or via e.g. PrefServiceFactory).
  user_prefs::PrefRegistrySyncable* registry();

  using PrefServiceSyncable::SetPrefModelAssociatorClientForTesting;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingPrefServiceSyncable);
};

}  // namespace sync_preferences

template <>
TestingPrefServiceBase<sync_preferences::PrefServiceSyncable,
                       user_prefs::PrefRegistrySyncable>::
    TestingPrefServiceBase(TestingPrefStore* managed_prefs,
                           TestingPrefStore* extension_prefs,
                           TestingPrefStore* user_prefs,
                           TestingPrefStore* recommended_prefs,
                           user_prefs::PrefRegistrySyncable* pref_registry,
                           PrefNotifierImpl* pref_notifier);

#endif  // COMPONENTS_SYNC_PREFERENCES_TESTING_PREF_SERVICE_SYNCABLE_H_
