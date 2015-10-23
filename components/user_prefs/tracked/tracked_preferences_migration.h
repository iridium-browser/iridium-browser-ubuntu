// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_
#define COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_

#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "components/user_prefs/tracked/pref_hash_store.h"

class HashStoreContents;
class InterceptablePrefFilter;
class PrefHashStore;

// Sets up InterceptablePrefFilter::FilterOnLoadInterceptors on
// |unprotected_pref_filter| and |protected_pref_filter| which prevents each
// filter from running their on load operations until the interceptors decide to
// hand the prefs back to them (after migration is complete). |
// (un)protected_store_cleaner| and
// |register_on_successful_(un)protected_store_write_callback| are used to do
// post-migration cleanup tasks. Those should be bound to weak pointers to avoid
// blocking shutdown. |(un)protected_pref_hash_store| and
// |legacy_pref_hash_store| are used to migrate MACs along with their protected
// preferences and/or from the legacy location in Local State. Migrated MACs
// will only be cleared from their old location in a subsequent run. The
// migration framework is resilient to a failed cleanup (it will simply try
// again in the next Chrome run).
void SetupTrackedPreferencesMigration(
    const std::set<std::string>& unprotected_pref_names,
    const std::set<std::string>& protected_pref_names,
    const base::Callback<void(const std::string& key)>&
        unprotected_store_cleaner,
    const base::Callback<void(const std::string& key)>& protected_store_cleaner,
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_unprotected_store_write_callback,
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_protected_store_write_callback,
    scoped_ptr<PrefHashStore> unprotected_pref_hash_store,
    scoped_ptr<PrefHashStore> protected_pref_hash_store,
    scoped_ptr<HashStoreContents> legacy_pref_hash_store,
    InterceptablePrefFilter* unprotected_pref_filter,
    InterceptablePrefFilter* protected_pref_filter);

#endif  // COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_
