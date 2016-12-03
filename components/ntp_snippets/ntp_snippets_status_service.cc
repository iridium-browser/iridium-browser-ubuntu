// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_status_service.h"

#include <string>

#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

namespace {

const char kFetchingRequiresSignin[] = "fetching_requires_signin";
const char kFetchingRequiresSigninEnabled[] = "true";
const char kFetchingRequiresSigninDisabled[] = "false";

}  // namespace

NTPSnippetsStatusService::NTPSnippetsStatusService(
    SigninManagerBase* signin_manager,
    PrefService* pref_service)
    : disabled_reason_(DisabledReason::EXPLICITLY_DISABLED),
      require_signin_(false),
      signin_manager_(signin_manager),
      pref_service_(pref_service),
      signin_observer_(this) {
  std::string param_value_str = variations::GetVariationParamValueByFeature(
      kArticleSuggestionsFeature, kFetchingRequiresSignin);
  if (param_value_str == kFetchingRequiresSigninEnabled) {
    require_signin_ = true;
  } else if (!param_value_str.empty() &&
             param_value_str != kFetchingRequiresSigninDisabled) {
    DLOG(WARNING) << "Unknow value for the variations parameter "
                  << kFetchingRequiresSignin << ": " << param_value_str;
  }
}

NTPSnippetsStatusService::~NTPSnippetsStatusService() {}

// static
void NTPSnippetsStatusService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEnableSnippets, true);
}

void NTPSnippetsStatusService::Init(
    const DisabledReasonChangeCallback& callback) {
  DCHECK(disabled_reason_change_callback_.is_null());

  disabled_reason_change_callback_ = callback;

  // Notify about the current state before registering the observer, to make
  // sure we don't get a double notification due to an undefined start state.
  disabled_reason_ = GetDisabledReasonFromDeps();
  disabled_reason_change_callback_.Run(disabled_reason_);

  signin_observer_.Add(signin_manager_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kEnableSnippets,
      base::Bind(&NTPSnippetsStatusService::OnStateChanged,
                 base::Unretained(this)));
}

void NTPSnippetsStatusService::OnStateChanged() {
  DisabledReason new_disabled_reason = GetDisabledReasonFromDeps();

  if (new_disabled_reason == disabled_reason_)
    return;

  disabled_reason_ = new_disabled_reason;
  disabled_reason_change_callback_.Run(disabled_reason_);
}

void NTPSnippetsStatusService::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username,
    const std::string& password) {
  OnStateChanged();
}

void NTPSnippetsStatusService::GoogleSignedOut(const std::string& account_id,
                                               const std::string& username) {
  OnStateChanged();
}

DisabledReason NTPSnippetsStatusService::GetDisabledReasonFromDeps() const {
  if (!pref_service_->GetBoolean(prefs::kEnableSnippets)) {
    DVLOG(1) << "[GetNewDisabledReason] Disabled via pref";
    return DisabledReason::EXPLICITLY_DISABLED;
  }

  if (require_signin_ &&
      (!signin_manager_ || !signin_manager_->IsAuthenticated())) {
    DVLOG(1) << "[GetNewDisabledReason] Signed out";
    return DisabledReason::SIGNED_OUT;
  }

  DVLOG(1) << "[GetNewDisabledReason] Enabled";
  return DisabledReason::NONE;
}

}  // namespace ntp_snippets
