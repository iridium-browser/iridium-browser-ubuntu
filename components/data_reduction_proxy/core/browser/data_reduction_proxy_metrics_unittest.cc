// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "net/base/load_flags.h"
#include "net/log/net_log.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::MockRead;

namespace {

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

// Test UpdateContentLengthPrefs.
class ChromeNetworkDataSavingMetricsTest : public testing::Test {
 protected:
  ChromeNetworkDataSavingMetricsTest() {}

  void SetUp() override {
    compression_stats_.reset(new DataReductionProxyCompressionStats(
        &pref_service_,
        scoped_refptr<base::TestSimpleTaskRunner>(
            new base::TestSimpleTaskRunner()),
        base::TimeDelta()));
    PrefRegistrySimple* registry = pref_service_.registry();
    registry->RegisterInt64Pref(
        data_reduction_proxy::prefs::kHttpReceivedContentLength, 0);
    registry->RegisterInt64Pref(
        data_reduction_proxy::prefs::kHttpOriginalContentLength, 0);

    registry->RegisterListPref(data_reduction_proxy::prefs::
                                   kDailyHttpOriginalContentLength);
    registry->RegisterListPref(data_reduction_proxy::prefs::
                                   kDailyHttpReceivedContentLength);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthHttpsWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthUnknownWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxy);
    registry->RegisterListPref(
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxy);
    registry->RegisterInt64Pref(
        data_reduction_proxy::prefs::
            kDailyHttpContentLengthLastUpdateDate, 0L);
    registry->RegisterBooleanPref(
        data_reduction_proxy::prefs::kDataReductionProxyEnabled, false);
  }

  TestingPrefServiceSimple pref_service_;
  scoped_ptr<DataReductionProxyCompressionStats> compression_stats_;
};

// The initial last update time used in test. There is no leap second a few
// days around this time used in the test.
// Note: No time zone is specified. Local time will be assumed by
// base::Time::FromString below.
const char kLastUpdateTime[] = "Wed, 18 Sep 2013 03:45:26";

class ChromeNetworkDailyDataSavingMetricsTest
    : public ChromeNetworkDataSavingMetricsTest {
 protected:
  ChromeNetworkDailyDataSavingMetricsTest() {
    base::Time::FromString(kLastUpdateTime, &now_);
  }

  void SetUp() override {
    ChromeNetworkDataSavingMetricsTest::SetUp();

    // Only create two lists in Setup to test that adding new lists is fine.
    CreatePrefList(
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLength);
    CreatePrefList(
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLength);
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

  // Create daily pref list of |kNumDaysInHistory| zero values.
  void CreatePrefList(const char* pref) {
    base::ListValue* update = compression_stats_->GetList(pref);
    update->Clear();
    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      update->Insert(0, new base::StringValue(base::Int64ToString(0)));
    }
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

 private:
  base::Time now_;
  base::TimeDelta now_delta_;
};

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, OneResponse) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, MultipleResponses) {
  const int64 kOriginalLength = 150;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, UNKNOWN_TYPE,
      FakeNow(), compression_stats_.get());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      NULL, 0, NULL, 0, NULL, 0, NULL, 0);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, UNKNOWN_TYPE,
      FakeNow(), compression_stats_.get());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  int64 original_proxy_enabled[] = {kOriginalLength};
  int64 received_proxy_enabled[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      NULL, 0, NULL, 0);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
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

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, UNKNOWN_TYPE, FakeNow(), compression_stats_.get());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  original_proxy_enabled[0] += kOriginalLength;
  received_proxy_enabled[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, UNKNOWN_TYPE, FakeNow(), compression_stats_.get());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original_proxy_enabled, 1, received_proxy_enabled, 1,
      original_via_proxy, 1, received_via_proxy, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, RequestType) {
  const int64 kContentLength = 200;
  int64 received[] = {0};
  int64 https_received[] = {0};
  int64 total_received[] = {0};
  int64 proxy_enabled_received[] = {0};

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, HTTPS,
      FakeNow(), compression_stats_.get());
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
  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      false, HTTPS,
      FakeNow(), compression_stats_.get());
  total_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 0,  // short bypass
      received, 0,  // long bypass
      received, 0);  // unknown

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, HTTPS,
      FakeNow(), compression_stats_.get());
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

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, SHORT_BYPASS,
      FakeNow(), compression_stats_.get());
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

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, LONG_BYPASS,
      FakeNow(), compression_stats_.get());
  total_received[0] += kContentLength;
  proxy_enabled_received[0] += kContentLength;
  VerifyDailyRequestTypeContentLengthPrefLists(
      total_received, 1, total_received, 1,  // total
      proxy_enabled_received, 1, proxy_enabled_received, 1,
      https_received, 1,
      received, 1,  // short bypass
      received, 1,  // long bypass
      received, 0);  // unknown

  UpdateContentLengthPrefsForDataReductionProxy(
      kContentLength, kContentLength,
      true, UNKNOWN_TYPE,
      FakeNow(), compression_stats_.get());
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

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, ForwardOneDay) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());

  // Forward one day.
  SetFakeTimeDeltaInHours(24);

  // Proxy not enabled. Not via proxy.
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      false, UNKNOWN_TYPE, FakeNow(), compression_stats_.get());

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
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, UNKNOWN_TYPE, FakeNow(), compression_stats_.get());
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
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
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
  UpdateContentLengthPrefsForDataReductionProxy(
      kBigReceivedLength, kBigOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  original[1] += kBigOriginalLength;
  received[1] += kBigReceivedLength;
  original_with_data_reduction_proxy_enabled[1] += kBigOriginalLength;
  received_with_data_reduction_proxy_enabled[1] += kBigReceivedLength;
  original_via_data_reduction_proxy[1] += kBigOriginalLength;
  received_via_data_reduction_proxy[1] += kBigReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2,
      received, 2,
      original_with_data_reduction_proxy_enabled, 2,
      received_with_data_reduction_proxy_enabled, 2,
      original_via_data_reduction_proxy, 2,
      received_via_data_reduction_proxy, 2);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, PartialDayTimeChange) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {0, kOriginalLength};
  int64 received[] = {0, kReceivedLength};

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2,
      original, 2, received, 2,
      original, 2, received, 2);

  // Forward 10 hours, stay in the same day.
  // See kLastUpdateTime: "Now" in test is 03:45am.
  SetFakeTimeDeltaInHours(10);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  original[1] += kOriginalLength;
  received[1] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 2, received, 2,
      original, 2, received, 2,
      original, 2, received, 2);

  // Forward 11 more hours, comes to tomorrow.
  AddFakeTimeDeltaInHours(11);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  int64 original2[] = {kOriginalLength * 2, kOriginalLength};
  int64 received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 2, received2, 2,
      original2, 2, received2, 2,
      original2, 2, received2, 2);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, ForwardMultipleDays) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());

  // Forward three days.
  SetFakeTimeDeltaInHours(3 * 24);

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());

  int64 original[] = {kOriginalLength, 0, 0, kOriginalLength};
  int64 received[] = {kReceivedLength, 0, 0, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 4, received, 4,
      original, 4, received, 4,
      original, 4, received, 4);

  // Forward four more days.
  AddFakeTimeDeltaInHours(4 * 24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
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
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  int64 original3[] = {kOriginalLength};
  int64 received3[] = {kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original3, 1, received3, 1,
      original3, 1, received3, 1,
      original3, 1, received3, 1);

  // Forward |kNumDaysInHistory| + 1 more days.
  AddFakeTimeDeltaInHours((kNumDaysInHistory + 1)* 24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  VerifyDailyDataSavingContentLengthPrefLists(
      original3, 1, received3, 1,
      original3, 1, received3, 1,
      original3, 1, received3, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, BackwardAndForwardOneDay) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());

  // Backward one day.
  SetFakeTimeDeltaInHours(-24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  original[0] += kOriginalLength;
  received[0] += kReceivedLength;
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);

  // Then, Forward one day
  AddFakeTimeDeltaInHours(24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  int64 original2[] = {kOriginalLength * 2, kOriginalLength};
  int64 received2[] = {kReceivedLength * 2, kReceivedLength};
  VerifyDailyDataSavingContentLengthPrefLists(
      original2, 2, received2, 2,
      original2, 2, received2, 2,
      original2, 2, received2, 2);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest, BackwardTwoDays) {
  const int64 kOriginalLength = 200;
  const int64 kReceivedLength = 100;
  int64 original[] = {kOriginalLength};
  int64 received[] = {kReceivedLength};

  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  // Backward two days.
  SetFakeTimeDeltaInHours(-2 * 24);
  UpdateContentLengthPrefsForDataReductionProxy(
      kReceivedLength, kOriginalLength,
      true, VIA_DATA_REDUCTION_PROXY,
      FakeNow(), compression_stats_.get());
  VerifyDailyDataSavingContentLengthPrefLists(
      original, 1, received, 1,
      original, 1, received, 1,
      original, 1, received, 1);
}

TEST_F(ChromeNetworkDailyDataSavingMetricsTest,
       GetDataReductionProxyRequestType) {
  scoped_ptr<DataReductionProxyTestContext> test_context =
      DataReductionProxyTestContext::Builder()
          .WithParamsFlags(DataReductionProxyParams::kAllowed)
          .WithParamsDefinitions(TestDataReductionProxyParams::HAS_ORIGIN)
          .Build();
  TestDataReductionProxyConfig* config = test_context->config();

  net::ProxyConfig data_reduction_proxy_config;
  data_reduction_proxy_config.proxy_rules().ParseFromString(
      "http=" + config->test_params()->origin().host_port_pair().ToString() +
          ",direct://");
  data_reduction_proxy_config.proxy_rules().bypass_rules.ParseFromString(
      "localbypass.com");

  struct TestCase {
    GURL url;
    net::ProxyServer proxy_server;
    base::TimeDelta bypass_duration;  // 0 indicates not bypassed.
    int load_flags;
    const char* response_headers;
    DataReductionProxyRequestType expected_request_type;
  };
  const TestCase test_cases[] = {
    { GURL("http://foo.com"),
      config->test_params()->origin(),
      base::TimeDelta(),
      net::LOAD_NORMAL,
      "HTTP/1.1 200 OK\r\nVia: 1.1 Chrome-Compression-Proxy\r\n\r\n",
      VIA_DATA_REDUCTION_PROXY,
    },
    { GURL("https://foo.com"),
      net::ProxyServer::Direct(),
      base::TimeDelta(),
      net::LOAD_NORMAL,
      "HTTP/1.1 200 OK\r\n\r\n",
      HTTPS,
    },
    { GURL("http://foo.com"),
      net::ProxyServer::Direct(),
      base::TimeDelta::FromSeconds(1),
      net::LOAD_NORMAL,
      "HTTP/1.1 200 OK\r\n\r\n",
      SHORT_BYPASS,
    },
    { GURL("http://foo.com"),
      net::ProxyServer::Direct(),
      base::TimeDelta::FromMinutes(60),
      net::LOAD_NORMAL,
      "HTTP/1.1 200 OK\r\n\r\n",
      LONG_BYPASS,
    },
    // Requests with LOAD_BYPASS_PROXY (e.g. block-once) should be classified as
    // SHORT_BYPASS.
    { GURL("http://foo.com"),
      net::ProxyServer::Direct(),
      base::TimeDelta(),
      net::LOAD_BYPASS_PROXY,
      "HTTP/1.1 200 OK\r\n\r\n",
      SHORT_BYPASS,
    },
    // Another proxy overriding the Data Reduction Proxy should be classified as
    // SHORT_BYPASS.
    { GURL("http://foo.com"),
      net::ProxyServer::FromPacString("PROXY otherproxy.net:80"),
      base::TimeDelta(),
      net::LOAD_NORMAL,
      "HTTP/1.1 200 OK\r\n\r\n",
      SHORT_BYPASS,
    },
    // Bypasses due to local bypass rules should be classified as SHORT_BYPASS.
    { GURL("http://localbypass.com"),
      net::ProxyServer::Direct(),
      base::TimeDelta(),
      net::LOAD_NORMAL,
      "HTTP/1.1 200 OK\r\n\r\n",
      SHORT_BYPASS,
    },
    // Responses that seem like they should have come through the Data Reduction
    // Proxy, but did not, should be classified as UNKNOWN_TYPE.
    { GURL("http://foo.com"),
      net::ProxyServer::Direct(),
      base::TimeDelta(),
      net::LOAD_NORMAL,
      "HTTP/1.1 200 OK\r\n\r\n",
      UNKNOWN_TYPE,
    },
  };

  for (const TestCase& test_case : test_cases) {
    net::TestURLRequestContext context(true);
    net::MockClientSocketFactory mock_socket_factory;
    context.set_client_socket_factory(&mock_socket_factory);
    // Set the |proxy_service| to use |test_case.proxy_server| for requests.
    scoped_ptr<net::ProxyService> proxy_service(
        net::ProxyService::CreateFixedFromPacResult(
            test_case.proxy_server.ToPacString()));
    context.set_proxy_service(proxy_service.get());
    context.Init();

    // Create a fake URLRequest and fill it with the appropriate response
    // headers and proxy server by executing it with fake socket data.
    net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC, net::OK);
    if (test_case.url.SchemeIsSecure())
      mock_socket_factory.AddSSLSocketDataProvider(&ssl_socket_data_provider);
    MockRead mock_reads[] = {
        MockRead(test_case.response_headers),
        MockRead("hello world"),
        MockRead(net::SYNCHRONOUS, net::OK),
    };
    net::StaticSocketDataProvider socket_data_provider(
        mock_reads, arraysize(mock_reads), nullptr, 0);
    mock_socket_factory.AddSocketDataProvider(&socket_data_provider);

    net::TestDelegate delegate;
    scoped_ptr<net::URLRequest> request =
        context.CreateRequest(test_case.url, net::IDLE, &delegate);
    request->SetLoadFlags(test_case.load_flags);
    request->Start();
    test_context->RunUntilIdle();

    // Mark the Data Reduction Proxy as bad if the test specifies to.
    if (test_case.bypass_duration > base::TimeDelta()) {
      net::ProxyInfo proxy_info;
      proxy_info.UseProxyList(
          data_reduction_proxy_config.proxy_rules().proxies_for_http);
      EXPECT_TRUE(context.proxy_service()->MarkProxiesAsBadUntil(
          proxy_info, test_case.bypass_duration, net::ProxyServer(),
          net::BoundNetLog::Make(context.net_log(), net::NetLog::SOURCE_NONE)));
    }

    EXPECT_EQ(test_case.expected_request_type,
              GetDataReductionProxyRequestType(
                  *request, data_reduction_proxy_config,
                  *test_context->config()));
  }
}

}  // namespace data_reduction_proxy
