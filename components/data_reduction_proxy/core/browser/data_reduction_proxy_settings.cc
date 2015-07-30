// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace {
// Key of the UMA DataReductionProxy.StartupState histogram.
const char kUMAProxyStartupStateHistogram[] =
    "DataReductionProxy.StartupState";

bool IsLoFiEnabledOnCommandLine() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyLoFi);
}

}  // namespace

namespace data_reduction_proxy {

const char kDataReductionPassThroughHeader[] =
    "X-PSA-Client-Options: v=1,m=1\nCache-Control: no-cache";

DataReductionProxySettings::DataReductionProxySettings()
    : unreachable_(false),
      deferred_initialization_(false),
      allowed_(false),
      alternative_allowed_(false),
      promo_allowed_(false),
      prefs_(NULL),
      config_(nullptr) {
}

DataReductionProxySettings::~DataReductionProxySettings() {
  if (allowed_)
    spdy_proxy_auth_enabled_.Destroy();
}

void DataReductionProxySettings::InitPrefMembers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(&DataReductionProxySettings::OnProxyEnabledPrefChange,
                 base::Unretained(this)));
  data_reduction_proxy_alternative_enabled_.Init(
      prefs::kDataReductionProxyAltEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(
          &DataReductionProxySettings::OnProxyAlternativeEnabledPrefChange,
          base::Unretained(this)));
}

void DataReductionProxySettings::UpdateConfigValues() {
  DCHECK(config_);
  allowed_ = config_->allowed();
  alternative_allowed_ = config_->alternative_allowed();
  promo_allowed_ = config_->promo_allowed();
}

void DataReductionProxySettings::InitDataReductionProxySettings(
    PrefService* prefs,
    DataReductionProxyIOData* io_data,
    scoped_ptr<DataReductionProxyService> data_reduction_proxy_service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs);
  DCHECK(io_data);
  DCHECK(io_data->config());
  DCHECK(data_reduction_proxy_service.get());
  prefs_ = prefs;
  config_ = io_data->config();
  data_reduction_proxy_service_ = data_reduction_proxy_service.Pass();
  data_reduction_proxy_service_->AddObserver(this);
  InitPrefMembers();
  UpdateConfigValues();
  RecordDataReductionInit();
}

void DataReductionProxySettings::OnServiceInitialized() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!deferred_initialization_)
    return;
  deferred_initialization_ = false;
  // Technically, this is not "at startup", but this is the first chance that
  // IO data objects can be called.
  UpdateIOData(true);
}

void DataReductionProxySettings::SetCallbackToRegisterSyntheticFieldTrial(
    const SyntheticFieldTrialRegistrationCallback&
        on_data_reduction_proxy_enabled) {
  register_synthetic_field_trial_ = on_data_reduction_proxy_enabled;
  RegisterDataReductionProxyFieldTrial();
  RegisterLoFiFieldTrial();
}

bool DataReductionProxySettings::IsDataReductionProxyEnabled() const {
  return spdy_proxy_auth_enabled_.GetValue() ||
         DataReductionProxyParams::ShouldForceEnableDataReductionProxy();
}

bool DataReductionProxySettings::CanUseDataReductionProxy(
    const GURL& url) const {
  return url.is_valid() && url.scheme() == url::kHttpScheme &&
      IsDataReductionProxyEnabled();
}

bool
DataReductionProxySettings::IsDataReductionProxyAlternativeEnabled() const {
  return data_reduction_proxy_alternative_enabled_.GetValue();
}

bool DataReductionProxySettings::IsDataReductionProxyManaged() {
  return spdy_proxy_auth_enabled_.IsManaged();
}

void DataReductionProxySettings::SetDataReductionProxyEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!allowed_)
    return;

  if (spdy_proxy_auth_enabled_.GetValue() != enabled) {
    spdy_proxy_auth_enabled_.SetValue(enabled);
    OnProxyEnabledPrefChange();
  }
}

void DataReductionProxySettings::SetDataReductionProxyAlternativeEnabled(
    bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!alternative_allowed_)
    return;
  if (data_reduction_proxy_alternative_enabled_.GetValue() != enabled) {
    data_reduction_proxy_alternative_enabled_.SetValue(enabled);
    OnProxyAlternativeEnabledPrefChange();
  }
}

int64 DataReductionProxySettings::GetDataReductionLastUpdateTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  return
      data_reduction_proxy_service_->compression_stats()->GetLastUpdateTime();
}

void DataReductionProxySettings::SetUnreachable(bool unreachable) {
  unreachable_ = unreachable;
}

bool DataReductionProxySettings::IsDataReductionProxyUnreachable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return unreachable_;
}

PrefService* DataReductionProxySettings::GetOriginalProfilePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return prefs_;
}

bool DataReductionProxySettings::IsLoFiEnabled() const {
  return IsDataReductionProxyEnabled() && IsLoFiEnabledOnCommandLine();
}

void DataReductionProxySettings::RegisterDataReductionProxyFieldTrial() {
  register_synthetic_field_trial_.Run(
      "SyntheticDataReductionProxySetting",
      IsDataReductionProxyEnabled() ? "Enabled" : "Disabled");
}

void DataReductionProxySettings::RegisterLoFiFieldTrial() {
  register_synthetic_field_trial_.Run(
      "SyntheticDataReductionProxyLoFiSetting",
      IsLoFiEnabled() ? "Enabled" : "Disabled");
}

void DataReductionProxySettings::OnProxyEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!register_synthetic_field_trial_.is_null()) {
    RegisterDataReductionProxyFieldTrial();
    RegisterLoFiFieldTrial();
  }
  if (!allowed_)
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::OnProxyAlternativeEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!alternative_allowed_)
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::ResetDataReductionStatistics() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  data_reduction_proxy_service_->compression_stats()->ResetStatistics();
}

void DataReductionProxySettings::UpdateIOData(bool at_startup) {
  data_reduction_proxy_service_->SetProxyPrefs(
      IsDataReductionProxyEnabled(), IsDataReductionProxyAlternativeEnabled(),
      at_startup);
  data_reduction_proxy_service_->RetrieveConfig();
}

void DataReductionProxySettings::MaybeActivateDataReductionProxy(
    bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* prefs = GetOriginalProfilePrefs();
  // Do nothing if prefs have not been initialized. This allows unit testing
  // of profile related code without having to initialize data reduction proxy
  // related prefs.
  if (!prefs)
    return;
  // TODO(marq): Consider moving this so stats are wiped the first time the
  // proxy settings are actually (not maybe) turned on.
  if (spdy_proxy_auth_enabled_.GetValue() &&
      !prefs->GetBoolean(prefs::kDataReductionProxyWasEnabledBefore)) {
    prefs->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, true);
    ResetDataReductionStatistics();
  }
  // Configure use of the data reduction proxy if it is enabled.
  if (at_startup && !data_reduction_proxy_service_->Initialized())
    deferred_initialization_ = true;
  else
    UpdateIOData(at_startup);
}

DataReductionProxyEventStore* DataReductionProxySettings::GetEventStore()
    const {
  if (data_reduction_proxy_service_)
    return data_reduction_proxy_service_->event_store();

  return nullptr;
}

// Metrics methods
void DataReductionProxySettings::RecordDataReductionInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProxyStartupState state = PROXY_NOT_AVAILABLE;
  if (allowed_) {
    if (IsDataReductionProxyEnabled())
      state = PROXY_ENABLED;
    else
      state = PROXY_DISABLED;
  }

  RecordStartupState(state);
}

void DataReductionProxySettings::RecordStartupState(ProxyStartupState state) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyStartupStateHistogram,
                            state,
                            PROXY_STARTUP_STATE_COUNT);
}

ContentLengthList
DataReductionProxySettings::GetDailyContentLengths(const char* pref_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  return data_reduction_proxy_service_->compression_stats()->
      GetDailyContentLengths(pref_name);
}

void DataReductionProxySettings::GetContentLengths(
    unsigned int days,
    int64* original_content_length,
    int64* received_content_length,
    int64* last_update_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());

  data_reduction_proxy_service_->compression_stats()->GetContentLengths(
      days, original_content_length, received_content_length, last_update_time);
}

}  // namespace data_reduction_proxy
