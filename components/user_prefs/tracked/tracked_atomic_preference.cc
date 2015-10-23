// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/tracked_atomic_preference.h"

#include "base/values.h"
#include "components/user_prefs/tracked/pref_hash_store_transaction.h"
#include "components/user_prefs/tracked/tracked_preference_validation_delegate.h"

TrackedAtomicPreference::TrackedAtomicPreference(
    const std::string& pref_path,
    size_t reporting_id,
    size_t reporting_ids_count,
    PrefHashFilter::EnforcementLevel enforcement_level,
    PrefHashFilter::ValueType value_type,
    TrackedPreferenceValidationDelegate* delegate)
    : pref_path_(pref_path),
      helper_(pref_path,
              reporting_id,
              reporting_ids_count,
              enforcement_level,
              value_type),
      delegate_(delegate) {
}

void TrackedAtomicPreference::OnNewValue(
    const base::Value* value,
    PrefHashStoreTransaction* transaction) const {
  transaction->StoreHash(pref_path_, value);
}

bool TrackedAtomicPreference::EnforceAndReport(
    base::DictionaryValue* pref_store_contents,
    PrefHashStoreTransaction* transaction) const {
  const base::Value* value = NULL;
  pref_store_contents->Get(pref_path_, &value);
  PrefHashStoreTransaction::ValueState value_state =
      transaction->CheckValue(pref_path_, value);

  helper_.ReportValidationResult(value_state);

  TrackedPreferenceHelper::ResetAction reset_action =
      helper_.GetAction(value_state);
  if (delegate_) {
    delegate_->OnAtomicPreferenceValidation(pref_path_, value, value_state,
                                            helper_.IsPersonal());
  }
  helper_.ReportAction(reset_action);

  bool was_reset = false;
  if (reset_action == TrackedPreferenceHelper::DO_RESET) {
    pref_store_contents->RemovePath(pref_path_, NULL);
    was_reset = true;
  }

  if (value_state != PrefHashStoreTransaction::UNCHANGED) {
    // Store the hash for the new value (whether it was reset or not).
    const base::Value* new_value = NULL;
    pref_store_contents->Get(pref_path_, &new_value);
    transaction->StoreHash(pref_path_, new_value);
  }

  return was_reset;
}
