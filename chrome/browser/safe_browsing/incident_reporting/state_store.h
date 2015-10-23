// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_STATE_STORE_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_STATE_STORE_H_

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/scoped_user_pref_update.h"

class Profile;

namespace safe_browsing {

enum class IncidentType : int32_t;

// The storage to track which incidents have been reported for a profile. Only
// usable on the UI thread.
class StateStore {
 public:
  using IncidentDigest = uint32_t;

  // An object through which modifications to a StateStore can be made. Changes
  // are visible to the StateStore immediately and are written to persistent
  // storage when the instance is destroyed (or shortly thereafter). Only one
  // transaction may be live for a given StateStore at a given time. Instances
  // are typically created on the stack for immediate use.
  class Transaction {
   public:
    explicit Transaction(StateStore* store);
    ~Transaction();

    // Marks the described incident as having been reported.
    void MarkAsReported(IncidentType type,
                        const std::string& key,
                        IncidentDigest digest);

    // Clears all data associated with an incident type.
    void ClearForType(IncidentType type);

   private:
    // Returns a writable view on the incidents_sent preference. The act of
    // obtaining this view will cause a serialize-and-write operation to be
    // scheduled when the transaction terminates. Use the store's
    // |incidents_sent_| member directly to simply query the preference.
    base::DictionaryValue* GetPrefDict();

    // The store corresponding to this transaction.
    StateStore* store_;

    // A ScopedUserPrefUpdate through which changes to the incidents_sent
    // preference are made.
    scoped_ptr<DictionaryPrefUpdate> pref_update_;

    DISALLOW_COPY_AND_ASSIGN(Transaction);
  };

  explicit StateStore(Profile* profile);
  ~StateStore();

  // Returns true if the described incident has already been reported.
  bool HasBeenReported(IncidentType type,
                       const std::string& key,
                       IncidentDigest digest);

 private:
  // Called on load to clear values that are no longer used.
  void CleanLegacyValues(Transaction* transaction);

  // The profile to which this state corresponds.
  Profile* profile_;

  // A read-only view on the profile's incidents_sent preference.
  const base::DictionaryValue* incidents_sent_;

#if DCHECK_IS_ON()
  // True when a Transaction instance is outstanding.
  bool has_transaction_;
#endif

  DISALLOW_COPY_AND_ASSIGN(StateStore);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_STATE_STORE_H_
