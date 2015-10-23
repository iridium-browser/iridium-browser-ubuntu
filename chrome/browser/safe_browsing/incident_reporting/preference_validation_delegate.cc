// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/preference_validation_delegate.h"

#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/tracked_preference_incident.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "components/user_prefs/tracked/pref_hash_store_transaction.h"
#include "components/user_prefs/tracked/tracked_preference_helper.h"

namespace safe_browsing {

namespace {

typedef ClientIncidentReport_IncidentData_TrackedPreferenceIncident TPIncident;
typedef ClientIncidentReport_IncidentData_TrackedPreferenceIncident_ValueState
    TPIncident_ValueState;

// Maps a PrefHashStoreTransaction::ValueState to a
// TrackedPreferenceIncident::ValueState.
TPIncident_ValueState MapValueState(
    PrefHashStoreTransaction::ValueState value_state) {
  switch (value_state) {
    case PrefHashStoreTransaction::CLEARED:
      return TPIncident::CLEARED;
    case PrefHashStoreTransaction::CHANGED:
      return TPIncident::CHANGED;
    case PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE:
      return TPIncident::UNTRUSTED_UNKNOWN_VALUE;
    default:
      return TPIncident::UNKNOWN;
  }
}

}  // namespace

PreferenceValidationDelegate::PreferenceValidationDelegate(
    Profile* profile,
    scoped_ptr<IncidentReceiver> incident_receiver)
    : profile_(profile),
      incident_receiver_(incident_receiver.Pass()) {
}

PreferenceValidationDelegate::~PreferenceValidationDelegate() {
}

void PreferenceValidationDelegate::OnAtomicPreferenceValidation(
    const std::string& pref_path,
    const base::Value* value,
    PrefHashStoreTransaction::ValueState value_state,
    bool is_personal) {
  TPIncident_ValueState proto_value_state = MapValueState(value_state);
  if (proto_value_state != TPIncident::UNKNOWN) {
    scoped_ptr<TPIncident> incident(
        new ClientIncidentReport_IncidentData_TrackedPreferenceIncident());
    incident->set_path(pref_path);
    if (!value ||
        (!value->GetAsString(incident->mutable_atomic_value()) &&
         !base::JSONWriter::Write(*value, incident->mutable_atomic_value()))) {
      incident->clear_atomic_value();
    }
    incident->set_value_state(proto_value_state);
    incident_receiver_->AddIncidentForProfile(
        profile_, make_scoped_ptr(new TrackedPreferenceIncident(incident.Pass(),
                                                                is_personal)));
  }
}

void PreferenceValidationDelegate::OnSplitPreferenceValidation(
    const std::string& pref_path,
    const base::DictionaryValue* /* dict_value */,
    const std::vector<std::string>& invalid_keys,
    PrefHashStoreTransaction::ValueState value_state,
    bool is_personal) {
  TPIncident_ValueState proto_value_state = MapValueState(value_state);
  if (proto_value_state != TPIncident::UNKNOWN) {
    scoped_ptr<ClientIncidentReport_IncidentData_TrackedPreferenceIncident>
        incident(
            new ClientIncidentReport_IncidentData_TrackedPreferenceIncident());
    incident->set_path(pref_path);
    for (std::vector<std::string>::const_iterator scan(invalid_keys.begin());
         scan != invalid_keys.end();
         ++scan) {
      incident->add_split_key(*scan);
    }
    incident->set_value_state(proto_value_state);
    incident_receiver_->AddIncidentForProfile(
        profile_, make_scoped_ptr(new TrackedPreferenceIncident(incident.Pass(),
                                                                is_personal)));
  }
}

}  // namespace safe_browsing
