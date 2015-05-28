// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
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

// TODO Make kNumDaysInHistory accessible from DataReductionProxySettings.
const size_t kNumDaysInHistory = 60;
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

class DataReductionProxyCompressionStatsTest : public testing::Test {
 protected:
  DataReductionProxyCompressionStatsTest()
      : task_runner_(scoped_refptr<base::TestSimpleTaskRunner>(
            new base::TestSimpleTaskRunner())) {}

  void SetUp() override { RegisterPrefs(simple_pref_service_.registry()); }

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
    const base::ListValue* written_list = simple_pref_service_.GetList(pref);
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
    int64 written_pref = simple_pref_service_.GetInt64(pref);
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

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TestingPrefServiceSimple simple_pref_service_;
  scoped_ptr<DataReductionProxyCompressionStats> compression_stats_;
};

TEST_F(DataReductionProxyCompressionStatsTest, WritePrefsDirect) {
  compression_stats_.reset(new DataReductionProxyCompressionStats(
      &simple_pref_service_,
      task_runner_,
      base::TimeDelta()));
  SetUpPrefs();

  VerifyPrefWasWritten(prefs::kHttpOriginalContentLength);
  VerifyPrefWasWritten(prefs::kHttpReceivedContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpOriginalContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpReceivedContentLength);
}

TEST_F(DataReductionProxyCompressionStatsTest, WritePrefsDelayed) {
  compression_stats_.reset(new DataReductionProxyCompressionStats(
      &simple_pref_service_,
      task_runner_,
      base::TimeDelta::FromMinutes(kWriteDelayMinutes)));
  SetUpPrefs();

  task_runner_->RunPendingTasks();

  VerifyPrefWasWritten(prefs::kHttpOriginalContentLength);
  VerifyPrefWasWritten(prefs::kHttpReceivedContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpOriginalContentLength);
  VerifyPrefListWasWritten(prefs::kDailyHttpReceivedContentLength);
}

TEST_F(DataReductionProxyCompressionStatsTest,
       WritePrefsOnUpdateDailyReceivedContentLengths) {
  compression_stats_.reset(new DataReductionProxyCompressionStats(
      &simple_pref_service_,
      task_runner_,
      base::TimeDelta::FromMinutes(kWriteDelayMinutes)));
  SetUpPrefs();

  simple_pref_service_.SetBoolean(
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
  compression_stats_.reset(new DataReductionProxyCompressionStats(
      &simple_pref_service_,
      task_runner_,
      base::TimeDelta::FromMinutes(kWriteDelayMinutes)));

  base::DictionaryValue* dict = nullptr;
  scoped_ptr<base::Value> stats_value(
      compression_stats_->HistoricNetworkStatsInfoToValue());
  EXPECT_TRUE(stats_value->GetAsDictionary(&dict));
  VerifyPrefs(dict);

  compression_stats_->SetInt64(prefs::kHttpOriginalContentLength,
                               kOriginalLength);
  compression_stats_->SetInt64(prefs::kHttpReceivedContentLength,
                               kReceivedLength);

  stats_value.reset(compression_stats_->HistoricNetworkStatsInfoToValue());
  EXPECT_TRUE(stats_value->GetAsDictionary(&dict));
  VerifyPrefs(dict);
}

TEST_F(DataReductionProxyCompressionStatsTest,
       HistoricNetworkStatsInfoToValueDirect) {
  const int64 kOriginalLength = 150;
  const int64 kReceivedLength = 100;
  compression_stats_.reset(new DataReductionProxyCompressionStats(
      &simple_pref_service_,
      task_runner_,
      base::TimeDelta()));

  base::DictionaryValue* dict = nullptr;
  scoped_ptr<base::Value> stats_value(
      compression_stats_->HistoricNetworkStatsInfoToValue());
  EXPECT_TRUE(stats_value->GetAsDictionary(&dict));
  VerifyPrefs(dict);

  compression_stats_->SetInt64(prefs::kHttpOriginalContentLength,
                               kOriginalLength);
  compression_stats_->SetInt64(prefs::kHttpReceivedContentLength,
                               kReceivedLength);

  stats_value.reset(compression_stats_->HistoricNetworkStatsInfoToValue());
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
  simple_pref_service_.Set(prefs::kDailyHttpOriginalContentLength, list_value);

  compression_stats_.reset(new DataReductionProxyCompressionStats(
      &simple_pref_service_,
      task_runner_,
      base::TimeDelta::FromMinutes(kWriteDelayMinutes)));

  const base::ListValue* value = simple_pref_service_.GetList(
      prefs::kDailyHttpOriginalContentLength);
  EXPECT_EQ(0u, value->GetSize());
}

TEST_F(DataReductionProxyCompressionStatsTest,
       ClearPrefsOnRestartDisabled) {
  base::ListValue list_value;
  list_value.Insert(0, new base::StringValue(base::Int64ToString(1234)));
  simple_pref_service_.Set(prefs::kDailyHttpOriginalContentLength, list_value);

  compression_stats_.reset(new DataReductionProxyCompressionStats(
      &simple_pref_service_,
      task_runner_,
      base::TimeDelta::FromMinutes(kWriteDelayMinutes)));

  const base::ListValue* value = simple_pref_service_.GetList(
      prefs::kDailyHttpOriginalContentLength);
  std::string string_value;
  value->GetString(0, &string_value);
  EXPECT_EQ("1234", string_value);
}

}  // namespace data_reduction_proxy
