// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kWriteDelayMinutes = 60;

int64 GetListPrefInt64Value(
    const base::ListValue& list_update, size_t index) {
  std::string string_value;
  EXPECT_TRUE(list_update.GetString(index, &string_value));

  int64 value = 0;
  EXPECT_TRUE(base::StringToInt64(string_value, &value));
  return value;
}

}  // namespace

namespace data_reduction_proxy {

// The initial last update time used in test. There is no leap second a few
// days around this time used in the test.
// Note: No time zone is specified. Local time will be assumed by
// base::Time::FromString below.
const char kLastUpdateTime[] = "Wed, 18 Sep 2013 03:45:26";

class DataReductionProxyCompressionStatsTest : public testing::Test {
 protected:
  DataReductionProxyCompressionStatsTest()
      : task_runner_(scoped_refptr<base::TestSimpleTaskRunner>(
          new base::TestSimpleTaskRunner())) {
    base::Time::FromString(kLastUpdateTime, &now_);
  }

  void SetUp() override {
    compression_stats_.reset(new DataReductionProxyCompressionStats(
        pref_service(), task_runner_, base::TimeDelta()));
    RegisterSimpleProfilePrefs(pref_service()->registry());
  }

  void ResetCompressionStatsWithDelay(const base::TimeDelta& delay) {
    compression_stats_.reset(new DataReductionProxyCompressionStats(
        pref_service(), task_runner_, delay));
  }

  base::Time FakeNow() const {
    return now_ + now_delta_;
  }

  void SetFakeTimeDeltaInHours(int hours) {
    now_delta_ = base::TimeDelta::FromHours(hours);
  }

  void AddFakeTimeDeltaInHours(int hours) {
    now_delta_ += base::TimeDelta::FromHours(hours);
  }

  void SetUpPrefs() {
    CreatePrefList(prefs::kDailyHttpOriginalContentLength);
    CreatePrefList(prefs::kDailyHttpReceivedContentLength);

    const int64 kOriginalLength = 150;
    const int64 kReceivedLength = 100;

    compression_stats_->SetInt64(
        prefs::kHttpOriginalContentLength, kOriginalLength);
    compression_stats_->SetInt64(
        prefs::kHttpReceivedContentLength, kReceivedLength);

    base::ListValue* original_daily_content_length_list =
        compression_stats_->GetList(prefs::kDailyHttpOriginalContentLength);
    base::ListValue* received_daily_content_length_list =
        compression_stats_->GetList(prefs::kDailyHttpReceivedContentLength);

    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      original_daily_content_length_list->Set(
          i, new base::StringValue(base::Int64ToString(i)));
    }

    received_daily_content_length_list->Clear();
    for (size_t i = 0; i < kNumDaysInHistory / 2; ++i) {
      received_daily_content_length_list->Set(
          i, new base::StringValue(base::Int64ToString(i)));
    }
  }

  // Create daily pref list of |kNumDaysInHistory| zero values.
  void CreatePrefList(const char* pref) {
    base::ListValue* update = compression_stats_->GetList(pref);
    update->Clear();
    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      update->Insert(0, new base::StringValue(base::Int64ToString(0)));
    }
  }

  // Verify the pref list values in |pref_service_| are equal to those in
  // |simple_pref_service| for |pref|.
  void VerifyPrefListWasWritten(const char* pref) {
    const base::ListValue* delayed_list = compression_stats_->GetList(pref);
    const base::ListValue* written_list = pref_service()->GetList(pref);
    ASSERT_EQ(delayed_list->GetSize(), written_list->GetSize());
    size_t count = delayed_list->GetSize();

    for (size_t i = 0; i < count; ++i) {
      EXPECT_EQ(GetListPrefInt64Value(*delayed_list, i),
                GetListPrefInt64Value(*written_list, i));
    }
  }

  // Verify the pref value in |pref_service_| are equal to that in
  // |simple_pref_service|.
  void VerifyPrefWasWritten(const char* pref) {
    int64 delayed_pref = compression_stats_->GetInt64(pref);
    int64 written_pref = pref_service()->GetInt64(pref);
    EXPECT_EQ(delayed_pref, written_pref);
  }

  // Verify the pref values in |dict| are equal to that in |compression_stats_|.
  void VerifyPrefs(base::DictionaryValue* dict) {
    base::string16 dict_pref_string;
    int64 dict_pref;
    int64 service_pref;

    dict->GetString("historic_original_content_length", &dict_pref_string);
    base::StringToInt64(dict_pref_string, &dict_pref);
    service_pref =
        compression_stats_->GetInt64(prefs::kHttpOriginalContentLength);
    EXPECT_EQ(service_pref, dict_pref);

    dict->GetString("historic_received_content_length", &dict_pref_string);
    base::StringToInt64(dict_pref_string, &dict_pref);
    service_pref =
        compression_stats_->GetInt64(prefs::kHttpReceivedContentLength);
    EXPECT_EQ(service_pref, dict_pref);
  }

  // Verify the pref list values are equal to the given values.
  // If the count of values is less than kNumDaysInHistory, zeros are assumed
  // at the beginning.
  void VerifyPrefList(const char* pref, const int64* values, size_t count) {
    ASSERT_GE(kNumDaysInHistory, count);
    base::ListValue* update = compression_stats_->GetList(pref);
    ASSERT_EQ(kNumDaysInHistory, update->GetSize()) << "Pref: " << pref;

    for (size_t i = 0; i < count; ++i) {
      EXPECT_EQ(
          values[i],
          GetListPrefInt64Value(*update, kNumDaysInHistory - count + i))
          << "index=" << (kNumDaysInHistory - count + i);
    }
    for (size_t i = 0; i < kNumDaysInHistory - count; ++i) {
      EXPECT_EQ(0, GetListPrefInt64Value(*update, i)) << "index=" << i;
    }
  }

  // Verify all daily data saving pref list values.
  void VerifyDailyDataSavingContentLengthPrefLists(
      const int64* original_values, size_t original_count,
      const int64* received_values, size_t received_count,
      const int64* original_with_data_reduction_proxy_enabled_values,
      size_t original_with_data_reduction_proxy_enabled_count,
      const int64* received_with_data_reduction_proxy_enabled_values,
      size_t received_with_data_reduction_proxy_count,
      const int64* original_via_data_reduction_proxy_values,
      size_t original_via_data_reduction_proxy_count,
      const int64* received_via_data_reduction_proxy_values,
      size_t received_via_data_reduction_proxy_count) {
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
                   original_values, original_count);
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
                   received_values, received_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabled,
        original_with_data_reduction_proxy_enabled_values,
        original_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabled,
        received_with_data_reduction_proxy_enabled_values,
        received_with_data_reduction_proxy_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxy,
        original_via_data_reduction_proxy_values,
        original_via_data_reduction_proxy_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxy,
        received_via_data_reduction_proxy_values,
        received_via_data_reduction_proxy_count);
  }

  // Verify daily data saving pref for request types.
  void VerifyDailyRequestTypeContentLengthPrefLists(
      const int64* original_values, size_t original_count,
      const int64* received_values, size_t received_count,
      const int64* original_with_data_reduction_proxy_enabled_values,
      size_t original_with_data_reduction_proxy_enabled_count,
      const int64* received_with_data_reduction_proxy_enabled_values,
      size_t received_with_data_reduction_proxy_count,
      const int64* https_with_data_reduction_proxy_enabled_values,
      size_t https_with_data_reduction_proxy_enabled_count,
      const int64* short_bypass_with_data_reduction_proxy_enabled_values,
      size_t short_bypass_with_data_reduction_proxy_enabled_count,
      const int64* long_bypass_with_data_reduction_proxy_enabled_values,
      size_t long_bypass_with_data_reduction_proxy_enabled_count,
      const int64* unknown_with_data_reduction_proxy_enabled_values,
      size_t unknown_with_data_reduction_proxy_enabled_count) {
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
                   original_values, original_count);
    VerifyPrefList(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength,
                   received_values, received_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabled,
        original_with_data_reduction_proxy_enabled_values,
        original_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabled,
        received_with_data_reduction_proxy_enabled_values,
        received_with_data_reduction_proxy_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthHttpsWithDataReductionProxyEnabled,
        https_with_data_reduction_proxy_enabled_values,
        https_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
        short_bypass_with_data_reduction_proxy_enabled_values,
        short_bypass_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
        long_bypass_with_data_reduction_proxy_enabled_values,
        long_bypass_with_data_reduction_proxy_enabled_count);
    VerifyPrefList(
        data_reduction_proxy::prefs::
            kDailyContentLengthUnknownWithDataReductionProxyEnabled,
        unknown_with_data_reduction_proxy_enabled_values,
        unknown_with_data_reduction_proxy_enabled_count);
  }

  int64 GetInt64(const char* pref_path) {
    return compression_stats_->GetInt64(pref_path);
  }

  void SetInt64(const char* pref_path, int64 pref_value) {
    compression_stats_->SetInt64(pref_path, pref_value);
  }

  void RecordContentLengthPrefs(int64 received_content_length,
                                int64 original_content_length,
                                bool with_data_reduction_proxy_enabled,
                                DataReductionProxyRequestType request_type,
                                base::Time now) {
    compression_stats_->RecordContentLengthPrefs(
        received_content_length, original_content_length,
        with_data_reduction_proxy_enabled, request_type, now);
  }

  DataReductionProxyCompressionStats* compression_stats() {
    return compression_stats_.get();
  }

  void RunPendingTasks() {
    task_runner_->RunPendingTasks();
  }

  TestingPrefServiceSimple* pref_service() {
    return &simple_pref_service_;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TestingPrefServiceSimple simple_pref_service_;
  scoped_ptr<DataReductionProxyCompressionStats> compression_stats_;
  base::Time now_;
  base::TimeDelta now_delta_;
};

TEST_F(DataReductionProxyCompressionStatsTest, WritePrefsDirect) {
  SetUpPrefs();

  VerifyPrefWasWritten(prefs::kHttpOriginalContentLength);
  VerifyPrefWasWritten(prefs::kHttpReceivedContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpOriginalContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpReceivedContentLength);
}

TEST_F(DataReductionProxyCompressionStatsTest, WritePrefsDelayed) {
  ResetCompressionStatsWithDelay(
      base::TimeDelta::FromMinutes(kWriteDelayMinutes));
  SetUpPrefs();

  RunPendingTasks();

  VerifyPrefWasWritten(prefs::kHttpOriginalContentLength);
  VerifyPrefWasWritten(prefs::kHttpReceivedContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpOriginalContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpReceivedContentLength);
}

TEST_F(DataReductionProxyCompressionStatsTest,
       WritePrefsOnUpdateDailyReceivedContentLengths) {
  ResetCompressionStatsWithDelay(
      base::TimeDelta::FromMinutes(kWriteDelayMinutes));
  SetUpPrefs();

  pref_service()->SetBoolean(
      prefs::kUpdateDailyReceivedContentLengths, true);

  VerifyPrefWasWritten(prefs::kHttpOriginalContentLength);
  VerifyPrefWasWritten(prefs::kHttpReceivedContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpOriginalContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpReceivedContentLength);
}

TEST_F(DataReductionProxyCompressionStatsTest,
       HistoricNetworkStatsInfoToValue) {
  const int64 kOriginalLength = 150;
  const int64 kReceivedLength = 100;
  ResetCompressionStatsWithDelay(
      base::TimeDelta::FromMinutes(kWriteDelayMinutes));

  base::DictionaryValue* dict = nullptr;
  scoped_ptr<base::Value> stats_value(
      compression_stats()->HistoricNetworkStatsInfoToValue());
  EXPECT_TRUE(stats_value->GetAsDictionary(&dict));
  VerifyPrefs(dict);

  SetInt64(prefs::kHttpOriginalContentLength, kOriginalLength);
  SetInt64(prefs::kHttpReceivedContentLength, kReceivedLength);

  stats_value.reset(compression_stats()->HistoricNetworkStatsInfoToValue());
  EXPECT_TRUE(stats_value->GetAsDictionary(&dict));
  VerifyPrefs(dict);
}

TEST_F(DataReductionProxyCompressionStatsTest,
       HistoricNetworkStatsInfoToValueDirect) {
  const int64 kOriginalLength = 150;
  const int64 kReceivedLength = 100;

  base::DictionaryValue* dict = nullptr;
  scoped_ptr<base::Value> stats_value(
      compression_stats()->HistoricNetworkStatsInfoToValue());
  EXPECT_TRUE(stats_value->GetAsDictionary(&dict));
  VerifyPrefs(dict);

  SetInt64(prefs::kHttpOriginalContentLength, kOriginalLength);
  SetInt64(prefs::kHttpReceivedContentLength, kReceivedLength);

  stats_value.reset(compression_stats()->HistoricNetworkStatsInfoToValue());
  EXPECT_TRUE(stats_value->GetAsDictionary(&dict));
  VerifyPrefs(dict);
}

TEST_F(DataReductionProxyCompressionStatsTest,
       ClearPrefsOnRestartEnabled) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(
      data_reduction_proxy::switches::kClearDataReductionProxyDataSavings);

  base::ListValue list_value;
  list_value.Insert(0, new base::StringValue(base::Int64ToString(1234)));
  pref_service()->Set(prefs::kDailyHttpOriginalContentLength, list_value);

  ResetCompressionStatsWithDelay(
      base::TimeDelta::FromMinutes(kWriteDelayMinutes));

  const base::ListValue* value = pref_service()->GetList(
      prefs::kDailyHttpOriginalContentLength);
  EXPECT_EQ(0u, value->GetSize());
}

TEST_F(DataReductionProxyCompressionStatsTest,
       ClearPrefsOnRestartDisabled) {
  base::ListValue list_value;
  list_value.Insert(0, new base::StringValue(base::Int64ToString(1234)));
  pref_service()->Set(prefs::kDailyHttpOriginalContentLength, list_value);

  ResetCompressionStatsWithDelay(
      base::TimeDelta::FromMinutes(kWriteDelayMinutes));

  const base::ListValue* value = pref_service()->GetList(
      prefs::kDailyHttpOriginalContentLength);
  std::string string_value;
  value->GetString(0, &string_value);
  EXPECT_EQ("1234", string_value);
}

TEST_F(DataReductionProxyCompressionStatsTest, TotalLengths) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;

  compression_stats()->UpdateContentLengths(
      kReceivedLength, kOriginalLength,
      pref_service()->GetBoolean(
          data_reduction_proxy::prefs::kDataReductionProxyEnabled),
      UNKNOWN_TYPE);

  EXPECT_EQ(kReceivedLength,
            GetInt64(data_reduction_proxy::prefs::kHttpReceivedContentLength));
  EXPECT_FALSE(pref_service()->GetBoolean(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled));
  EXPECT_EQ(kOriginalLength,
            GetInt64(data_reduction_proxy::prefs::kHttpOriginalContentLength));

  // Record the same numbers again, and total lengths should be doubled.
  compression_stats()->UpdateContentLengths(
      kReceivedLength, kOriginalLength,
      pref_service()->GetBoolean(
          data_reduction_proxy::prefs::kDataReductionProxyEnabled),
      UNKNOWN_TYPE);

  EXPECT_EQ(kReceivedLength * 2,
            GetInt64(data_reduction_proxy::prefs::kHttpReceivedContentLength));
  EXPECT_FALSE(pref_service()->GetBoolean(
      data_reduction_proxy::prefs::kDataReductionProxyEnabled));
  EXPECT_EQ(kOriginalLength * 2,
            GetInt64(data_reduction_proxy::prefs::kHttpOriginalContentLength));
}

TEST_F(DataReductionProxyCompressionStatsTest, OneResponse) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);
}

TEST_F(DataReductionProxyCompressionStatsTest, MultipleResponses) {
  const int64 kOriginalLength = 150;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, false, UNKNOWN_TYPE, FakeNow());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      NULL, 0, NULL, 0, NULL, 0, NULL, 0);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, UNKNOWN_TYPE, FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  int64 original_proxy_enabled[] = {kOriginalLength};
  int64 received_proxy_enabled[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      NULL, 0, NULL, 0);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  int64 original_via_proxy[] = {kOriginalLength};
  int64 received_via_proxy[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, UNKNOWN_TYPE, FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, false, UNKNOWN_TYPE, FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);
}

TEST_F(DataReductionProxyCompressionStatsTest, RequestType) {
  const int64 kContentLength = 200;
  int64 received[] = {0};
  int64 https_received[] = {0};
  int64 total_received[] = {0};
  int64 proxy_enabled_received[] = {0};

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, HTTPS, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  https_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  // Data reduction proxy is not enabled.
  RecordContentLengthPrefs(
      kContentLength, kContentLength, false, HTTPS, FakeNow());
  total_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, HTTPS, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  https_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, SHORT_BYPASS, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, LONG_BYPASS, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,  // total
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 1,  // long bypass
      received, 0);  // unknown

  RecordContentLengthPrefs(
      kContentLength, kContentLength, true, UNKNOWN_TYPE, FakeNow());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 1,  // long bypass
      received, 1);  // unknown
}

TEST_F(DataReductionProxyCompressionStatsTest, ForwardOneDay) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());

  // Forward one day.
  SetFakeTimeDeltaInHours(24);

  // Proxy not enabled. Not via proxy.
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, false, UNKNOWN_TYPE, FakeNow());

  int64 original[] = {kOriginalLength, kOriginalLength};
  int64 received[] = {kReceivedLength, kReceivedLength};
  int64 original_with_data_reduction_proxy_enabled[] = {kOriginalLength, 0};
  int64 received_with_data_reduction_proxy_enabled[] = {kReceivedLength, 0};
  int64 original_via_data_reduction_proxy[] = {kOriginalLength, 0};
  int64 received_via_data_reduction_proxy[] = {kReceivedLength, 0};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);

  // Proxy enabled. Not via proxy.
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, UNKNOWN_TYPE, FakeNow());
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);

  // Proxy enabled and via proxy.
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kReceivedLength;
  original_via_data_reduction_proxy[1] += kOriginalLength;
  received_via_data_reduction_proxy[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);

  // Proxy enabled and via proxy, with content length greater than max int32.
  const int64 kBigOriginalLength = 0x300000000LL;  // 12G.
  const int64 kBigReceivedLength = 0x200000000LL;  // 8G.
  RecordContentLengthPrefs(kBigReceivedLength, kBigOriginalLength, true,
                           VIA_DATA_REDUCTION_PROXY, FakeNow());
  original[1] += kBigOriginalLength;
  received[1] += kBigReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kBigOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kBigReceivedLength;
  original_via_data_reduction_proxy[1] += kBigOriginalLength;
  received_via_data_reduction_proxy[1] += kBigReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2, original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2, received_via_data_reduction_proxy,
      2);
}

TEST_F(DataReductionProxyCompressionStatsTest, PartialDayTimeChange) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {0, kOriginalLength};
  int64 received[] = {0, kReceivedLength};

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2,
      original, 2, received, 2,
      original, 2, received, 2);

  // Forward 10 hours, stay in the same day.
  // See kLastUpdateTime: "Now" in test is 03:45am.
  SetFakeTimeDeltaInHours(10);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2,
      original, 2, received, 2,
      original, 2, received, 2);

  // Forward 11 more hours, comes to tomorrow.
  AddFakeTimeDeltaInHours(11);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  int64 original2[] = {kOriginalLength * 2, kOriginalLength};
  int64 received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 2, received2, 2,
      original2, 2, received2, 2,
      original2, 2, received2, 2);
}

TEST_F(DataReductionProxyCompressionStatsTest, ForwardMultipleDays) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());

  // Forward three days.
  SetFakeTimeDeltaInHours(3 * 24);

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());

  int64 original[] = {kOriginalLength, 0, 0, kOriginalLength};
  int64 received[] = {kReceivedLength, 0, 0, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 4, received, 4,
      original, 4, received, 4,
      original, 4, received, 4);

  // Forward four more days.
  AddFakeTimeDeltaInHours(4 * 24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  int64 original2[] = {
    kOriginalLength, 0, 0, kOriginalLength, 0, 0, 0, kOriginalLength,
  };
  int64 received2[] = {
    kReceivedLength, 0, 0, kReceivedLength, 0, 0, 0, kReceivedLength,
  };
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 8, received2, 8,
      original2, 8, received2, 8,
      original2, 8, received2, 8);

  // Forward |kNumDaysInHistory| more days.
  AddFakeTimeDeltaInHours(kNumDaysInHistory * 24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  int64 original3[] = {kOriginalLength};
  int64 received3[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original3, 1, received3, 1,
      original3, 1, received3, 1,
      original3, 1, received3, 1);

  // Forward |kNumDaysInHistory| + 1 more days.
  AddFakeTimeDeltaInHours((kNumDaysInHistory + 1)* 24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  VerifyDailyDataSavingContentLengthPrefLists(
      original3, 1, received3, 1,
      original3, 1, received3, 1,
      original3, 1, received3, 1);
}

TEST_F(DataReductionProxyCompressionStatsTest, BackwardAndForwardOneDay) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());

  // Backward one day.
  SetFakeTimeDeltaInHours(-24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);

  // Then, Forward one day
  AddFakeTimeDeltaInHours(24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  int64 original2[] = {kOriginalLength * 2, kOriginalLength};
  int64 received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 2, received2, 2,
      original2, 2, received2, 2,
      original2, 2, received2, 2);
}

TEST_F(DataReductionProxyCompressionStatsTest, BackwardTwoDays) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  // Backward two days.
  SetFakeTimeDeltaInHours(-2 * 24);
  RecordContentLengthPrefs(
      kReceivedLength, kOriginalLength, true, VIA_DATA_REDUCTION_PROXY,
      FakeNow());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);
}

}  // namespace data_reduction_proxy
