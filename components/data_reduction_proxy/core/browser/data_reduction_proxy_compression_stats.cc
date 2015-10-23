// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_usage_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/mime_util.h"

namespace data_reduction_proxy {

namespace {

#define CONCAT(a, b) a##b
// CONCAT1 provides extra level of indirection so that __LINE__ macro expands.
#define CONCAT1(a, b) CONCAT(a, b)
#define UNIQUE_VARNAME CONCAT1(var_, __LINE__)
// We need to use a macro instead of a method because UMA_HISTOGRAM_COUNTS
// requires its first argument to be an inline string and not a variable.
#define RECORD_INT64PREF_TO_HISTOGRAM(pref, uma)     \
  int64 UNIQUE_VARNAME = GetInt64(pref);             \
  if (UNIQUE_VARNAME > 0) {                          \
    UMA_HISTOGRAM_COUNTS(uma, UNIQUE_VARNAME >> 10); \
  }

// Returns the value at |index| of |list_value| as an int64.
int64 GetInt64PrefValue(const base::ListValue& list_value, size_t index) {
  int64 val = 0;
  std::string pref_value;
  bool rv = list_value.GetString(index, &pref_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(pref_value, &val);
    DCHECK(rv);
  }
  return val;
}

// Ensure list has exactly |length| elements, either by truncating at the
// front, or appending "0"'s to the back.
void MaintainContentLengthPrefsWindow(base::ListValue* list, size_t length) {
  // Remove data for old days from the front.
  while (list->GetSize() > length)
    list->Remove(0, NULL);
  // Newly added lists are empty. Add entries to back to fill the window,
  // each initialized to zero.
  while (list->GetSize() < length)
    list->AppendString(base::Int64ToString(0));
  DCHECK_EQ(length, list->GetSize());
}

// Increments an int64, stored as a string, in a ListPref at the specified
// index.  The value must already exist and be a string representation of a
// number.
void AddInt64ToListPref(size_t index,
                        int64 length,
                        base::ListValue* list_update) {
  int64 value = 0;
  std::string old_string_value;
  bool rv = list_update->GetString(index, &old_string_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(old_string_value, &value);
    DCHECK(rv);
  }
  value += length;
  list_update->Set(index, new base::StringValue(base::Int64ToString(value)));
}

int64 ListPrefInt64Value(const base::ListValue& list_update, size_t index) {
  std::string string_value;
  if (!list_update.GetString(index, &string_value)) {
    NOTREACHED();
    return 0;
  }

  int64 value = 0;
  bool rv = base::StringToInt64(string_value, &value);
  DCHECK(rv);
  return value;
}

// DailyContentLengthUpdate maintains a data saving pref. The pref is a list
// of |kNumDaysInHistory| elements of daily total content lengths for the past
// |kNumDaysInHistory| days.
void RecordDailyContentLengthHistograms(
    int64 original_length,
    int64 received_length,
    int64 original_length_with_data_reduction_enabled,
    int64 received_length_with_data_reduction_enabled,
    int64 original_length_via_data_reduction_proxy,
    int64 received_length_via_data_reduction_proxy,
    int64 https_length_with_data_reduction_enabled,
    int64 short_bypass_length_with_data_reduction_enabled,
    int64 long_bypass_length_with_data_reduction_enabled,
    int64 unknown_length_with_data_reduction_enabled) {
  // Report daily UMA only for days having received content.
  if (original_length <= 0 || received_length <= 0)
    return;

  // Record metrics in KB.
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength", original_length >> 10);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength", received_length >> 10);

  int percent = 0;
  // UMA percentage cannot be negative.
  if (original_length > received_length) {
    percent = (100 * (original_length - received_length)) / original_length;
  }
  UMA_HISTOGRAM_PERCENTAGE("Net.DailyContentSavingPercent", percent);

  if (original_length_with_data_reduction_enabled <= 0 ||
      received_length_with_data_reduction_enabled <= 0) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength_DataReductionProxyEnabled",
      original_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled",
      received_length_with_data_reduction_enabled >> 10);

  int percent_data_reduction_proxy_enabled = 0;
  // UMA percentage cannot be negative.
  if (original_length_with_data_reduction_enabled >
      received_length_with_data_reduction_enabled) {
    percent_data_reduction_proxy_enabled =
        100 * (original_length_with_data_reduction_enabled -
               received_length_with_data_reduction_enabled) /
        original_length_with_data_reduction_enabled;
  }
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentSavingPercent_DataReductionProxyEnabled",
      percent_data_reduction_proxy_enabled);

  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled",
      (100 * received_length_with_data_reduction_enabled) / received_length);

  DCHECK_GE(https_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_Https",
      https_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_Https",
      (100 * https_length_with_data_reduction_enabled) / received_length);

  DCHECK_GE(short_bypass_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_ShortBypass",
      short_bypass_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_ShortBypass",
      ((100 * short_bypass_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(long_bypass_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_LongBypass",
      long_bypass_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_LongBypass",
      ((100 * long_bypass_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(unknown_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_UnknownBypass",
      unknown_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_Unknown",
      ((100 * unknown_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(original_length_via_data_reduction_proxy, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength_ViaDataReductionProxy",
      original_length_via_data_reduction_proxy >> 10);
  DCHECK_GE(received_length_via_data_reduction_proxy, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_ViaDataReductionProxy",
      received_length_via_data_reduction_proxy >> 10);

  int percent_via_data_reduction_proxy = 0;
  if (original_length_via_data_reduction_proxy >
      received_length_via_data_reduction_proxy) {
    percent_via_data_reduction_proxy =
        100 * (original_length_via_data_reduction_proxy -
               received_length_via_data_reduction_proxy) /
        original_length_via_data_reduction_proxy;
  }
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentSavingPercent_ViaDataReductionProxy",
      percent_via_data_reduction_proxy);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_ViaDataReductionProxy",
      (100 * received_length_via_data_reduction_proxy) / received_length);
}

// Given a |net::NetworkChangeNotifier::ConnectionType|, returns the
// corresponding |data_reduction_proxy::ConnectionType|.
ConnectionType StoredConnectionType(
    net::NetworkChangeNotifier::ConnectionType networkType) {
  switch (networkType) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return ConnectionType::CONNECTION_UNKNOWN;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return ConnectionType::CONNECTION_ETHERNET;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return ConnectionType::CONNECTION_WIFI;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return ConnectionType::CONNECTION_2G;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return ConnectionType::CONNECTION_3G;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return ConnectionType::CONNECTION_4G;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return ConnectionType::CONNECTION_BLUETOOTH;
    default:
      NOTREACHED();
      return ConnectionType::CONNECTION_UNKNOWN;
  }
}

class DailyContentLengthUpdate {
 public:
  DailyContentLengthUpdate(base::ListValue* update)
      : update_(update) {}

  void UpdateForDataChange(int days_since_last_update) {
    // New empty lists may have been created. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_, kNumDaysInHistory);
    if (days_since_last_update) {
      MaintainContentLengthPrefForDateChange(days_since_last_update);
    }
  }

  // Update the lengths for the current day.
  void Add(int64 content_length) {
    AddInt64ToListPref(kNumDaysInHistory - 1, content_length, update_);
  }

  int64 GetListPrefValue(size_t index) {
    return ListPrefInt64Value(*update_, index);
  }

 private:
  // Update the list for date change and ensure the list has exactly |length|
  // elements. The last entry in the list will be for the current day after
  // the update.
  void MaintainContentLengthPrefForDateChange(int days_since_last_update) {
    if (days_since_last_update == -1) {
      // The system may go backwards in time by up to a day for legitimate
      // reasons, such as with changes to the time zone. In such cases, we
      // keep adding to the current day.
      // Note: we accept the fact that some reported data is shifted to
      // the adjacent day if users travel back and forth across time zones.
      days_since_last_update = 0;
    } else if (days_since_last_update < -1) {
      // Erase all entries if the system went backwards in time by more than
      // a day.
      update_->Clear();

      days_since_last_update = kNumDaysInHistory;
    }
    DCHECK_GE(days_since_last_update, 0);

    // Add entries for days since last update event. This will make the
    // lists longer than kNumDaysInHistory. The additional items will be cut off
    // from the head of the lists by |MaintainContentLengthPrefsWindow|, below.
    for (int i = 0;
         i < days_since_last_update && i < static_cast<int>(kNumDaysInHistory);
         ++i) {
      update_->AppendString(base::Int64ToString(0));
    }

    // Entries for new days may have been appended. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_, kNumDaysInHistory);
  }

  base::ListValue* update_;
};

// DailyDataSavingUpdate maintains a pair of data saving prefs, original_update_
// and received_update_. pref_original is a list of |kNumDaysInHistory| elements
// of daily total original content lengths for the past |kNumDaysInHistory|
// days. pref_received is the corresponding list of the daily total received
// content lengths.
class DailyDataSavingUpdate {
 public:
  DailyDataSavingUpdate(base::ListValue* original,
                        base::ListValue* received)
      : original_(original),
        received_(received) {}

  void UpdateForDataChange(int days_since_last_update) {
    original_.UpdateForDataChange(days_since_last_update);
    received_.UpdateForDataChange(days_since_last_update);
  }

  // Update the lengths for the current day.
  void Add(int64 original_content_length, int64 received_content_length) {
    original_.Add(original_content_length);
    received_.Add(received_content_length);
  }

  int64 GetOriginalListPrefValue(size_t index) {
    return original_.GetListPrefValue(index);
  }
  int64 GetReceivedListPrefValue(size_t index) {
    return received_.GetListPrefValue(index);
  }

 private:
  DailyContentLengthUpdate original_;
  DailyContentLengthUpdate received_;
};

// Report UMA metrics for daily data reductions.
}  // namespace

DataReductionProxyCompressionStats::DataReductionProxyCompressionStats(
    DataReductionProxyService* service,
    PrefService* prefs,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::TimeDelta delay)
    : service_(service),
      pref_service_(prefs),
      task_runner_(task_runner),
      delay_(delay),
      delayed_task_posted_(false),
      pref_change_registrar_(new PrefChangeRegistrar()),
      data_usage_map_is_dirty_(false),
      data_usage_loaded_(false),
      weak_factory_(this) {
  DCHECK(service);
  DCHECK(prefs);
  DCHECK_GE(delay.InMilliseconds(), 0);
  Init();
}

DataReductionProxyCompressionStats::~DataReductionProxyCompressionStats() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (data_usage_map_is_dirty_)
    PersistDataUsage();
  ClearInMemoryDataUsage();
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);

  WritePrefs();
  pref_change_registrar_->RemoveAll();
}

void DataReductionProxyCompressionStats::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());

  service_->LoadCurrentDataUsageBucket(
      base::Bind(&DataReductionProxyCompressionStats::OnDataUsageLoaded,
                 weak_factory_.GetWeakPtr()));

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  connection_type_ =
      StoredConnectionType(net::NetworkChangeNotifier::GetConnectionType());

  if (delay_ == base::TimeDelta())
    return;

  // Init all int64 prefs.
  InitInt64Pref(prefs::kDailyHttpContentLengthLastUpdateDate);
  InitInt64Pref(prefs::kHttpReceivedContentLength);
  InitInt64Pref(prefs::kHttpOriginalContentLength);

  InitInt64Pref(prefs::kDailyHttpOriginalContentLengthApplication);
  InitInt64Pref(prefs::kDailyHttpOriginalContentLengthVideo);
  InitInt64Pref(prefs::kDailyHttpOriginalContentLengthUnknown);
  InitInt64Pref(prefs::kDailyHttpReceivedContentLengthApplication);
  InitInt64Pref(prefs::kDailyHttpReceivedContentLengthVideo);
  InitInt64Pref(prefs::kDailyHttpReceivedContentLengthUnknown);

  InitInt64Pref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyApplication);
  InitInt64Pref(prefs::kDailyOriginalContentLengthViaDataReductionProxyVideo);
  InitInt64Pref(prefs::kDailyOriginalContentLengthViaDataReductionProxyUnknown);
  InitInt64Pref(prefs::kDailyContentLengthViaDataReductionProxyApplication);
  InitInt64Pref(prefs::kDailyContentLengthViaDataReductionProxyVideo);
  InitInt64Pref(prefs::kDailyContentLengthViaDataReductionProxyUnknown);

  InitInt64Pref(
      prefs::
          kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication);
  InitInt64Pref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo);
  InitInt64Pref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown);
  InitInt64Pref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledApplication);
  InitInt64Pref(prefs::kDailyContentLengthWithDataReductionProxyEnabledVideo);
  InitInt64Pref(prefs::kDailyContentLengthWithDataReductionProxyEnabledUnknown);

  // Init all list prefs.
  InitListPref(prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled);
  InitListPref(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
  InitListPref(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
  InitListPref(prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled);
  InitListPref(prefs::kDailyContentLengthViaDataReductionProxy);
  InitListPref(prefs::kDailyContentLengthWithDataReductionProxyEnabled);
  InitListPref(prefs::kDailyHttpOriginalContentLength);
  InitListPref(prefs::kDailyHttpReceivedContentLength);
  InitListPref(prefs::kDailyOriginalContentLengthViaDataReductionProxy);
  InitListPref(prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kClearDataReductionProxyDataSavings)) {
    ClearDataSavingStatistics();
  }

  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(prefs::kUpdateDailyReceivedContentLengths,
      base::Bind(&DataReductionProxyCompressionStats::OnUpdateContentLengths,
                 weak_factory_.GetWeakPtr()));
}

void DataReductionProxyCompressionStats::OnUpdateContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!pref_service_->GetBoolean(prefs::kUpdateDailyReceivedContentLengths))
    return;

  WritePrefs();
  pref_service_->SetBoolean(prefs::kUpdateDailyReceivedContentLengths, false);
}

void DataReductionProxyCompressionStats::UpdateContentLengths(
    int64 data_used,
    int64 original_size,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& data_usage_host,
    const std::string& mime_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("loader",
               "DataReductionProxyCompressionStats::UpdateContentLengths")
  int64 total_received = GetInt64(
      data_reduction_proxy::prefs::kHttpReceivedContentLength);
  int64 total_original = GetInt64(
      data_reduction_proxy::prefs::kHttpOriginalContentLength);
  total_received += data_used;
  total_original += original_size;
  SetInt64(data_reduction_proxy::prefs::kHttpReceivedContentLength,
           total_received);
  SetInt64(data_reduction_proxy::prefs::kHttpOriginalContentLength,
           total_original);

  RecordDataUsage(data_usage_host, data_used, original_size);
  RecordRequestSizePrefs(data_used, original_size, data_reduction_proxy_enabled,
                         request_type, mime_type, base::Time::Now());
}

void DataReductionProxyCompressionStats::InitInt64Pref(const char* pref) {
  int64 pref_value = pref_service_->GetInt64(pref);
  pref_map_[pref] = pref_value;
}

void DataReductionProxyCompressionStats::InitListPref(const char* pref) {
  scoped_ptr<base::ListValue> pref_value = scoped_ptr<base::ListValue>(
      pref_service_->GetList(pref)->DeepCopy());
  list_pref_map_.add(pref, pref_value.Pass());
}

int64 DataReductionProxyCompressionStats::GetInt64(const char* pref_path) {
  if (delay_ == base::TimeDelta())
    return pref_service_->GetInt64(pref_path);

  DataReductionProxyPrefMap::iterator iter = pref_map_.find(pref_path);
  return iter->second;
}

void DataReductionProxyCompressionStats::SetInt64(const char* pref_path,
                                                  int64 pref_value) {
  if (delay_ == base::TimeDelta()) {
    pref_service_->SetInt64(pref_path, pref_value);
    return;
  }

  DelayedWritePrefs();
  pref_map_[pref_path] = pref_value;
}

void DataReductionProxyCompressionStats::IncrementInt64Pref(
    const char* pref_path,
    int64_t pref_increment) {
  SetInt64(pref_path, GetInt64(pref_path) + pref_increment);
}

base::ListValue* DataReductionProxyCompressionStats::GetList(
    const char* pref_path) {
  if (delay_ == base::TimeDelta())
    return ListPrefUpdate(pref_service_, pref_path).Get();

  DelayedWritePrefs();
  return list_pref_map_.get(pref_path);
}

void DataReductionProxyCompressionStats::WritePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delay_ == base::TimeDelta())
    return;

  for (DataReductionProxyPrefMap::iterator iter = pref_map_.begin();
       iter != pref_map_.end(); ++iter) {
    pref_service_->SetInt64(iter->first, iter->second);
  }

  for (DataReductionProxyListPrefMap::iterator iter = list_pref_map_.begin();
       iter != list_pref_map_.end(); ++iter) {
    TransferList(*(iter->second),
                 ListPrefUpdate(pref_service_, iter->first).Get());
  }

  delayed_task_posted_ = false;
}

base::Value*
DataReductionProxyCompressionStats::HistoricNetworkStatsInfoToValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64 total_received = GetInt64(prefs::kHttpReceivedContentLength);
  int64 total_original = GetInt64(prefs::kHttpOriginalContentLength);

  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("historic_received_content_length",
                  base::Int64ToString(total_received));
  dict->SetString("historic_original_content_length",
                  base::Int64ToString(total_original));
  return dict;
}

int64 DataReductionProxyCompressionStats::GetLastUpdateTime() {
  int64 last_update_internal = GetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time last_update = base::Time::FromInternalValue(last_update_internal);
  return static_cast<int64>(last_update.ToJsTime());
}

void DataReductionProxyCompressionStats::ResetStatistics() {
  base::ListValue* original_update =
      GetList(prefs::kDailyHttpOriginalContentLength);
  base::ListValue* received_update =
      GetList(prefs::kDailyHttpReceivedContentLength);
  original_update->Clear();
  received_update->Clear();
  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    original_update->AppendString(base::Int64ToString(0));
    received_update->AppendString(base::Int64ToString(0));
  }
}

ContentLengthList DataReductionProxyCompressionStats::GetDailyContentLengths(
    const char* pref_name) {
  ContentLengthList content_lengths;
  const base::ListValue* list_value = GetList(pref_name);
  if (list_value->GetSize() == kNumDaysInHistory) {
    for (size_t i = 0; i < kNumDaysInHistory; ++i)
      content_lengths.push_back(GetInt64PrefValue(*list_value, i));
  }
  return content_lengths;
}

void DataReductionProxyCompressionStats::GetContentLengths(
    unsigned int days,
    int64* original_content_length,
    int64* received_content_length,
    int64* last_update_time) {
  DCHECK_LE(days, kNumDaysInHistory);

  const base::ListValue* original_list =
      GetList(prefs::kDailyHttpOriginalContentLength);
  const base::ListValue* received_list =
      GetList(prefs::kDailyHttpReceivedContentLength);

  if (original_list->GetSize() != kNumDaysInHistory ||
      received_list->GetSize() != kNumDaysInHistory) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  int64 orig = 0L;
  int64 recv = 0L;
  // Include days from the end of the list going backwards.
  for (size_t i = kNumDaysInHistory - days;
       i < kNumDaysInHistory; ++i) {
    orig += GetInt64PrefValue(*original_list, i);
    recv += GetInt64PrefValue(*received_list, i);
  }
  *original_content_length = orig;
  *received_content_length = recv;
  *last_update_time = GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
}

void DataReductionProxyCompressionStats::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  connection_type_ = StoredConnectionType(type);
}

void DataReductionProxyCompressionStats::OnDataUsageLoaded(
    scoped_ptr<DataUsageBucket> data_usage) {
  DCHECK(!data_usage_loaded_);
  DCHECK(data_usage_map_last_updated_.is_null());
  DCHECK(data_usage_map_.empty());

  for (auto connection_usage : data_usage->connection_usage()) {
    ConnectionType type = connection_usage.connection_type();
    scoped_ptr<SiteUsageMap> site_usage_map(new SiteUsageMap());

    for (auto site_usage : connection_usage.site_usage()) {
      site_usage_map->set(site_usage.site_url(),
                          make_scoped_ptr(new PerSiteDataUsage(site_usage)));
    }
    data_usage_map_.set(type, site_usage_map.Pass());
  }

  data_usage_map_last_updated_ =
      base::Time::FromInternalValue(data_usage->last_updated_timestamp());
  data_usage_loaded_ = true;
}

void DataReductionProxyCompressionStats::ClearDataSavingStatistics() {
  list_pref_map_.get(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled)->Clear();
  list_pref_map_
      .get(prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled)
      ->Clear();
  list_pref_map_
      .get(prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled)
      ->Clear();
  list_pref_map_
      .get(prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled)
      ->Clear();
  list_pref_map_.get(prefs::kDailyContentLengthViaDataReductionProxy)->Clear();
  list_pref_map_.get(prefs::kDailyContentLengthWithDataReductionProxyEnabled)
      ->Clear();
  list_pref_map_.get(prefs::kDailyHttpOriginalContentLength)->Clear();
  list_pref_map_.get(prefs::kDailyHttpReceivedContentLength)->Clear();
  list_pref_map_.get(prefs::kDailyOriginalContentLengthViaDataReductionProxy)
      ->Clear();
  list_pref_map_
      .get(prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled)
      ->Clear();

  WritePrefs();
}

void DataReductionProxyCompressionStats::DelayedWritePrefs() {
  // Only write after the first time posting the task.
  if (delayed_task_posted_)
    return;

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyCompressionStats::WritePrefs,
                 weak_factory_.GetWeakPtr()),
      delay_);

  delayed_task_posted_ = true;
}

void DataReductionProxyCompressionStats::TransferList(
    const base::ListValue& from_list,
    base::ListValue* to_list) {
  to_list->Clear();
  for (size_t i = 0; i < from_list.GetSize(); ++i) {
    to_list->Set(i, new base::StringValue(base::Int64ToString(
        GetListPrefInt64Value(from_list, i))));
  }
}

int64 DataReductionProxyCompressionStats::GetListPrefInt64Value(
    const base::ListValue& list,
    size_t index) {
  std::string string_value;
  if (!list.GetString(index, &string_value)) {
    NOTREACHED();
    return 0;
  }

  int64 value = 0;
  bool rv = base::StringToInt64(string_value, &value);
  DCHECK(rv);
  return value;
}

void DataReductionProxyCompressionStats::RecordRequestSizePrefs(
    int64 data_used,
    int64 original_size,
    bool with_data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    base::Time now) {
  // TODO(bengr): Remove this check once the underlying cause of
  // http://crbug.com/287821 is fixed. For now, only continue if the current
  // year is reported as being between 1972 and 2970.
  base::TimeDelta time_since_unix_epoch = now - base::Time::UnixEpoch();
  const int kMinDaysSinceUnixEpoch = 365 * 2;  // 2 years.
  const int kMaxDaysSinceUnixEpoch = 365 * 1000;  // 1000 years.
  if (time_since_unix_epoch.InDays() < kMinDaysSinceUnixEpoch ||
      time_since_unix_epoch.InDays() > kMaxDaysSinceUnixEpoch) {
    return;
  }

  // Determine how many days it has been since the last update.
  int64 then_internal = GetInt64(
      data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate);

  // Local midnight could have been shifted due to time zone change.
  // If time is null then don't care if midnight will be wrong shifted due to
  // time zone change because it's still too much time ago.
  base::Time then_midnight = base::Time::FromInternalValue(then_internal);
  if (!then_midnight.is_null()) {
    then_midnight = then_midnight.LocalMidnight();
  }
  base::Time midnight = now.LocalMidnight();

  DailyDataSavingUpdate total(
      GetList(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength),
      GetList(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength));

  DailyDataSavingUpdate proxy_enabled(
      GetList(data_reduction_proxy::prefs::
          kDailyOriginalContentLengthWithDataReductionProxyEnabled),
      GetList(data_reduction_proxy::prefs::
          kDailyContentLengthWithDataReductionProxyEnabled));

  DailyDataSavingUpdate via_proxy(
      GetList(data_reduction_proxy::prefs::
                  kDailyOriginalContentLengthViaDataReductionProxy),
      GetList(data_reduction_proxy::prefs::
                  kDailyContentLengthViaDataReductionProxy));

  DailyContentLengthUpdate https(
      GetList(data_reduction_proxy::prefs::
                  kDailyContentLengthHttpsWithDataReductionProxyEnabled));

  DailyContentLengthUpdate short_bypass(
      GetList(data_reduction_proxy::prefs::
                  kDailyContentLengthShortBypassWithDataReductionProxyEnabled));

  DailyContentLengthUpdate long_bypass(
      GetList(data_reduction_proxy::prefs::
                  kDailyContentLengthLongBypassWithDataReductionProxyEnabled));

  DailyContentLengthUpdate unknown(
      GetList(data_reduction_proxy::prefs::
                  kDailyContentLengthUnknownWithDataReductionProxyEnabled));

  int days_since_last_update = (midnight - then_midnight).InDays();
  if (days_since_last_update) {
    // Record the last update time in microseconds in UTC.
    SetInt64(data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate,
             midnight.ToInternalValue());

    // A new day. Report the previous day's data if exists. We'll lose usage
    // data if the last time Chrome was run was more than a day ago.
    // Here, we prefer collecting less data but the collected data is
    // associated with an accurate date.
    if (days_since_last_update == 1) {
      RecordDailyContentLengthHistograms(
          total.GetOriginalListPrefValue(kNumDaysInHistory - 1),
          total.GetReceivedListPrefValue(kNumDaysInHistory - 1),
          proxy_enabled.GetOriginalListPrefValue(kNumDaysInHistory - 1),
          proxy_enabled.GetReceivedListPrefValue(kNumDaysInHistory - 1),
          via_proxy.GetOriginalListPrefValue(kNumDaysInHistory - 1),
          via_proxy.GetReceivedListPrefValue(kNumDaysInHistory - 1),
          https.GetListPrefValue(kNumDaysInHistory - 1),
          short_bypass.GetListPrefValue(kNumDaysInHistory - 1),
          long_bypass.GetListPrefValue(kNumDaysInHistory - 1),
          unknown.GetListPrefValue(kNumDaysInHistory - 1));

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyHttpOriginalContentLengthApplication,
          "Net.DailyOriginalContentLength_Application");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyHttpReceivedContentLengthApplication,
          "Net.DailyReceivedContentLength_Application");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthVideo,
          "Net.DailyOriginalContentLength_Video");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthVideo,
          "Net.DailyContentLength_Video");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthUnknown,
          "Net.DailyOriginalContentLength_UnknownMime");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthUnknown,
          "Net.DailyContentLength_UnknownMime");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
          "Net.DailyOriginalContentLength_DataReductionProxyEnabled_"
          "Application");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthWithDataReductionProxyEnabledApplication,
          "Net.DailyContentLength_DataReductionProxyEnabled_Application");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo,
          "Net.DailyOriginalContentLength_DataReductionProxyEnabled_Video");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthWithDataReductionProxyEnabledVideo,
          "Net.DailyContentLength_DataReductionProxyEnabled_Video");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
          "Net.DailyOriginalContentLength_DataReductionProxyEnabled_"
          "UnknownMime");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthWithDataReductionProxyEnabledUnknown,
          "Net.DailyContentLength_DataReductionProxyEnabled_UnknownMime")

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthViaDataReductionProxyApplication,
          "Net.DailyOriginalContentLength_ViaDataReductionProxy_Application");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthViaDataReductionProxyApplication,
          "Net.DailyContentLength_ViaDataReductionProxy_Application");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthViaDataReductionProxyVideo,
          "Net.DailyOriginalContentLength_ViaDataReductionProxy_Video");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthViaDataReductionProxyVideo,
          "Net.DailyContentLength_ViaDataReductionProxy_Video");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthViaDataReductionProxyUnknown,
          "Net.DailyOriginalContentLength_ViaDataReductionProxy_UnknownMime");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthViaDataReductionProxyUnknown,
          "Net.DailyContentLength_ViaDataReductionProxy_UnknownMime");
    }

    // The system may go backwards in time by up to a day for legitimate
    // reasons, such as with changes to the time zone. In such cases, we
    // keep adding to the current day which is why we check for
    // |days_since_last_update != -1|.
    // Note: we accept the fact that some reported data is shifted to
    // the adjacent day if users travel back and forth across time zones.
    if (days_since_last_update && (days_since_last_update != -1)) {
      SetInt64(data_reduction_proxy::prefs::
                   kDailyHttpOriginalContentLengthApplication,
               0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyHttpReceivedContentLengthApplication,
               0);

      SetInt64(
          data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthVideo, 0);
      SetInt64(
          data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthVideo, 0);

      SetInt64(
          data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthUnknown,
          0);
      SetInt64(
          data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthUnknown,
          0);

      SetInt64(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
          0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthWithDataReductionProxyEnabledApplication,
               0);

      SetInt64(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo,
          0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthWithDataReductionProxyEnabledVideo,
               0);

      SetInt64(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
          0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthWithDataReductionProxyEnabledUnknown,
               0);

      SetInt64(data_reduction_proxy::prefs::
                   kDailyOriginalContentLengthViaDataReductionProxyApplication,
               0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthViaDataReductionProxyApplication,
               0);

      SetInt64(data_reduction_proxy::prefs::
                   kDailyOriginalContentLengthViaDataReductionProxyVideo,
               0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthViaDataReductionProxyVideo,
               0);

      SetInt64(data_reduction_proxy::prefs::
                   kDailyOriginalContentLengthViaDataReductionProxyUnknown,
               0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthViaDataReductionProxyUnknown,
               0);
    }
  }
  total.UpdateForDataChange(days_since_last_update);
  proxy_enabled.UpdateForDataChange(days_since_last_update);
  via_proxy.UpdateForDataChange(days_since_last_update);
  https.UpdateForDataChange(days_since_last_update);
  short_bypass.UpdateForDataChange(days_since_last_update);
  long_bypass.UpdateForDataChange(days_since_last_update);
  unknown.UpdateForDataChange(days_since_last_update);

  total.Add(original_size, data_used);
  if (with_data_reduction_proxy_enabled) {
    proxy_enabled.Add(original_size, data_used);
    // Ignore data source cases, if exist, when
    // "with_data_reduction_proxy_enabled == false"
    switch (request_type) {
      case VIA_DATA_REDUCTION_PROXY:
        via_proxy.Add(original_size, data_used);
        break;
      case HTTPS:
        https.Add(data_used);
        break;
      case SHORT_BYPASS:
        short_bypass.Add(data_used);
        break;
      case LONG_BYPASS:
        long_bypass.Add(data_used);
        break;
      case UNKNOWN_TYPE:
        unknown.Add(data_used);
        break;
      default:
        NOTREACHED();
    }
  }

  bool via_data_reduction_proxy = request_type == VIA_DATA_REDUCTION_PROXY;
  bool is_application = net::MatchesMimeType("application/*", mime_type);
  bool is_video = net::MatchesMimeType("video/*", mime_type);
  bool is_mime_type_empty = mime_type.empty();
  if (is_application) {
    IncrementDailyUmaPrefs(
        original_size, data_used,
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthApplication,
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthApplication,
        with_data_reduction_proxy_enabled,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabledApplication,
        via_data_reduction_proxy,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxyApplication,
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxyApplication);
  } else if (is_video) {
    IncrementDailyUmaPrefs(
        original_size, data_used,
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthVideo,
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthVideo,
        with_data_reduction_proxy_enabled,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo,
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabledVideo,
        via_data_reduction_proxy,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxyVideo,
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxyVideo);
  } else if (is_mime_type_empty) {
    IncrementDailyUmaPrefs(
        original_size, data_used,
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthUnknown,
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthUnknown,
        with_data_reduction_proxy_enabled,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabledUnknown,
        via_data_reduction_proxy,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxyUnknown,
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxyUnknown);
  }
}

void DataReductionProxyCompressionStats::IncrementDailyUmaPrefs(
    int64_t original_size,
    int64_t received_size,
    const char* original_size_pref,
    const char* received_size_pref,
    bool data_reduction_proxy_enabled,
    const char* original_size_with_proxy_enabled_pref,
    const char* recevied_size_with_proxy_enabled_pref,
    bool via_data_reduction_proxy,
    const char* original_size_via_proxy_pref,
    const char* received_size_via_proxy_pref) {
  IncrementInt64Pref(original_size_pref, original_size);
  IncrementInt64Pref(received_size_pref, received_size);

  if (data_reduction_proxy_enabled) {
    IncrementInt64Pref(original_size_with_proxy_enabled_pref, original_size);
    IncrementInt64Pref(recevied_size_with_proxy_enabled_pref, received_size);
  }

  if (via_data_reduction_proxy) {
    IncrementInt64Pref(original_size_via_proxy_pref, original_size);
    IncrementInt64Pref(received_size_via_proxy_pref, received_size);
  }
}

void DataReductionProxyCompressionStats::RecordUserVisibleDataSavings() {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_internal;
  GetContentLengths(kNumDaysInHistorySummary, &original_content_length,
                    &received_content_length, &last_update_internal);

  if (original_content_length == 0)
    return;

  int64 user_visible_savings_bytes =
      original_content_length - received_content_length;
  int user_visible_savings_percent =
      user_visible_savings_bytes * 100 / original_content_length;
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyUserVisibleSavingsPercent_DataReductionProxyEnabled",
      user_visible_savings_percent);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyUserVisibleSavingsSize_DataReductionProxyEnabled",
      user_visible_savings_bytes >> 10);
}

void DataReductionProxyCompressionStats::RecordDataUsage(
    const std::string& data_usage_host,
    int64 data_used,
    int64 original_size) {
  if (!data_usage_loaded_)
    return;

  if (!DataUsageStore::IsInCurrentInterval(data_usage_map_last_updated_)) {
    if (data_usage_map_is_dirty_)
      PersistDataUsage();
    ClearInMemoryDataUsage();
    DCHECK(data_usage_map_last_updated_.is_null());
    DCHECK(data_usage_map_.empty());
  }

  std::string normalized_host = NormalizeHostname(data_usage_host);

  auto i =
      data_usage_map_.add(connection_type_, make_scoped_ptr(new SiteUsageMap));
  SiteUsageMap* site_usage_map = i.first->second;

  auto j = site_usage_map->add(normalized_host,
                               make_scoped_ptr(new PerSiteDataUsage()));
  PerSiteDataUsage* per_site_usage = j.first->second;

  per_site_usage->set_site_url(normalized_host);
  data_usage_map_last_updated_ = base::Time::Now();
  per_site_usage->set_original_size(per_site_usage->original_size() +
                                    original_size);
  per_site_usage->set_data_used(per_site_usage->data_used() + data_used);

  data_usage_map_is_dirty_ = true;
}

void DataReductionProxyCompressionStats::PersistDataUsage() {
  scoped_ptr<DataUsageBucket> data_usage_bucket(new DataUsageBucket());
  for (auto i = data_usage_map_.begin(); i != data_usage_map_.end(); ++i) {
    SiteUsageMap* site_usage_map = i->second;
    PerConnectionDataUsage* connection_usage =
        data_usage_bucket->add_connection_usage();
    connection_usage->set_connection_type(connection_type_);
    for (auto j = site_usage_map->begin(); j != site_usage_map->end(); ++j) {
      PerSiteDataUsage* per_site_usage = connection_usage->add_site_usage();
      per_site_usage->CopyFrom(*(j->second));
    }
  }
  // TODO(kundaji): Persist |data_usage_bucket|.

  data_usage_map_is_dirty_ = false;
}

void DataReductionProxyCompressionStats::ClearInMemoryDataUsage() {
  DCHECK(!data_usage_map_is_dirty_);
  data_usage_map_.clear();
  data_usage_map_last_updated_ = base::Time();
}

// static
std::string DataReductionProxyCompressionStats::NormalizeHostname(
    const std::string& host) {
  size_t pos = host.find("://");
  if (pos != std::string::npos)
    return host.substr(pos + 3);

  return host;
}

}  // namespace data_reduction_proxy
