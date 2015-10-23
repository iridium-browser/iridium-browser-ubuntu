// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/metrics/field_trial.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxySettingsTest
    : public ConcreteDataReductionProxySettingsTest<
          DataReductionProxySettings> {
 public:
  void CheckMaybeActivateDataReductionProxy(bool initially_enabled,
                                            bool request_succeeded,
                                            bool expected_enabled,
                                            bool expected_restricted,
                                            bool expected_fallback_restricted) {
    test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                              initially_enabled);
    test_context_->config()->SetStateForTest(initially_enabled,
                                             request_succeeded);
    ExpectSetProxyPrefs(expected_enabled, false);
    settings_->MaybeActivateDataReductionProxy(false);
    test_context_->RunUntilIdle();
  }
};

TEST_F(DataReductionProxySettingsTest, TestIsProxyEnabledOrManaged) {
  settings_->InitPrefMembers();
  // The proxy is disabled initially.
  test_context_->config()->SetStateForTest(false, true);

  EXPECT_FALSE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  CheckOnPrefChange(true, true, false);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  CheckOnPrefChange(true, true, true);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_TRUE(settings_->IsDataReductionProxyManaged());

  test_context_->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, TestCanUseDataReductionProxy) {
  settings_->InitPrefMembers();
  // The proxy is disabled initially.
  test_context_->config()->SetStateForTest(false, true);

  GURL http_gurl("http://url.com/");
  EXPECT_FALSE(settings_->CanUseDataReductionProxy(http_gurl));

  CheckOnPrefChange(true, true, false);
  EXPECT_TRUE(settings_->CanUseDataReductionProxy(http_gurl));

  GURL https_gurl("https://url.com/");
  EXPECT_FALSE(settings_->CanUseDataReductionProxy(https_gurl));

  test_context_->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, TestResetDataReductionStatistics) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_time;
  settings_->ResetDataReductionStatistics();
  settings_->GetContentLengths(kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  EXPECT_EQ(0L, original_content_length);
  EXPECT_EQ(0L, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);
}

TEST_F(DataReductionProxySettingsTest, TestContentLengths) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_time;

  // Request |kNumDaysInHistory| days.
  settings_->GetContentLengths(kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  const unsigned int days = kNumDaysInHistory;
  // Received content length history values are 0 to |kNumDaysInHistory - 1|.
  int64 expected_total_received_content_length = (days - 1L) * days / 2;
  // Original content length history values are 0 to
  // |2 * (kNumDaysInHistory - 1)|.
  long expected_total_original_content_length = (days - 1L) * days;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);

  // Request |kNumDaysInHistory - 1| days.
  settings_->GetContentLengths(kNumDaysInHistory - 1,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  expected_total_received_content_length -= (days - 1);
  expected_total_original_content_length -= 2 * (days - 1);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 0 days.
  settings_->GetContentLengths(0,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  expected_total_received_content_length = 0;
  expected_total_original_content_length = 0;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 1 day. First day had 0 bytes so should be same as 0 days.
  settings_->GetContentLengths(1,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
}

TEST(DataReductionProxySettingsStandaloneTest, TestEndToEndSecureProxyCheck) {
  base::MessageLoopForIO message_loop;
  struct TestCase {
    const char* response_headers;
    const char* response_body;
    net::Error net_error_code;
    bool expected_restricted;
  };
  const TestCase kTestCases[] {
    { "HTTP/1.1 200 OK\r\n\r\n",
      "OK", net::OK, false,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "Bad", net::OK, true,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "", net::ERR_FAILED, true,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "", net::ERR_ABORTED, true,
    },
    // The secure proxy check shouldn't attempt to follow the redirect.
    { "HTTP/1.1 302 Found\r\nLocation: http://www.google.com/\r\n\r\n",
      "", net::OK, true,
    },
  };

  for (const TestCase& test_case : kTestCases) {
    net::TestURLRequestContext context(true);

    scoped_ptr<DataReductionProxyTestContext> drp_test_context =
        DataReductionProxyTestContext::Builder()
            .WithURLRequestContext(&context)
            .WithTestConfigurator()
            .SkipSettingsInitialization()
            .Build();

    context.set_net_log(drp_test_context->net_log());
    net::MockClientSocketFactory mock_socket_factory;
    context.set_client_socket_factory(&mock_socket_factory);
    context.Init();

    // Start with the Data Reduction Proxy disabled.
    drp_test_context->pref_service()->SetBoolean(
        prefs::kDataReductionProxyEnabled, false);
    drp_test_context->InitSettings();

    net::MockRead mock_reads[] = {
        net::MockRead(test_case.response_headers),
        net::MockRead(test_case.response_body),
        net::MockRead(net::SYNCHRONOUS, test_case.net_error_code),
    };
    net::StaticSocketDataProvider socket_data_provider(
        mock_reads, arraysize(mock_reads), nullptr, 0);
    mock_socket_factory.AddSocketDataProvider(&socket_data_provider);

    // Toggle the pref to trigger the secure proxy check.
    drp_test_context->pref_service()->SetBoolean(
            prefs::kDataReductionProxyEnabled, true);
    drp_test_context->RunUntilIdle();

    EXPECT_EQ(test_case.expected_restricted,
              drp_test_context->test_configurator()->restricted());
  }
}

TEST(DataReductionProxySettingsStandaloneTest, TestOnProxyEnabledPrefChange) {
  base::MessageLoopForIO message_loop;
  scoped_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .WithMockConfig()
          .WithTestConfigurator()
          .WithMockDataReductionProxyService()
          .SkipSettingsInitialization()
          .Build();

  // The proxy is enabled initially.
  drp_test_context->config()->SetStateForTest(true, true);
  drp_test_context->InitSettings();

  MockDataReductionProxyService* mock_service =
      static_cast<MockDataReductionProxyService*>(
          drp_test_context->data_reduction_proxy_service());

  // The pref is disabled, so correspondingly should be the proxy.
  EXPECT_CALL(*mock_service, SetProxyPrefs(false, false));
  drp_test_context->pref_service()->SetBoolean(
      prefs::kDataReductionProxyEnabled, false);

  // The pref is enabled, so correspondingly should be the proxy.
  EXPECT_CALL(*mock_service, SetProxyPrefs(true, false));
  drp_test_context->pref_service()->SetBoolean(
      prefs::kDataReductionProxyEnabled, true);
}

TEST_F(DataReductionProxySettingsTest, TestMaybeActivateDataReductionProxy) {
  // Initialize the pref member in |settings_| without the usual callback
  // so it won't trigger MaybeActivateDataReductionProxy when the pref value
  // is set.
  settings_->spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      settings_->GetOriginalProfilePrefs());

  // TODO(bengr): Test enabling/disabling while a secure proxy check is
  // outstanding.
  // The proxy is enabled and unrestricted initially.
  // Request succeeded but with bad response, expect proxy to be restricted.
  CheckMaybeActivateDataReductionProxy(true, true, true, true, false);
  // Request succeeded with valid response, expect proxy to be unrestricted.
  CheckMaybeActivateDataReductionProxy(true, true, true, false, false);
  // Request failed, expect proxy to be enabled but restricted.
  CheckMaybeActivateDataReductionProxy(true, false, true, true, false);
  // The proxy is disabled initially. No secure proxy checks should take place,
  // and so the state should not change.
  CheckMaybeActivateDataReductionProxy(false, true, false, false, false);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOn) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            true);
  InitDataReductionProxy(true);
  CheckDataReductionProxySyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOff) {
  // InitDataReductionProxySettings with the preference off will directly call
  // LogProxyState.
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_DISABLED));

  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            false);
  InitDataReductionProxy(false);
  CheckDataReductionProxySyntheticTrial(false);
}

TEST_F(DataReductionProxySettingsTest, TestEnableProxyFromCommandLine) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDataReductionProxy);
  InitDataReductionProxy(true);
  CheckDataReductionProxySyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestSetDataReductionProxyEnabled) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));
  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            true);
  settings->SetLoFiModeActiveOnMainFrame(true);
  InitDataReductionProxy(true);

  ExpectSetProxyPrefs(false, false);
  settings_->SetDataReductionProxyEnabled(false);
  test_context_->RunUntilIdle();
  CheckDataReductionProxySyntheticTrial(false);
  CheckDataReductionProxyLoFiSyntheticTrial(false);

  ExpectSetProxyPrefs(true, false);
  settings->SetDataReductionProxyEnabled(true);
  CheckDataReductionProxySyntheticTrial(true);
  CheckDataReductionProxyLoFiSyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestEnableLoFiSyntheticTrial) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));
  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            true);
  InitDataReductionProxy(true);

  // The Lo-Fi field trial will be set to "Disabled" until the first main frame
  // request with Lo-Fi active.
  CheckDataReductionProxyLoFiSyntheticTrial(false);

  // Turn Lo-Fi on.
  settings->SetLoFiModeActiveOnMainFrame(true);
  CheckDataReductionProxyLoFiSyntheticTrial(true);

  // Now turn it off.
  settings->SetLoFiModeActiveOnMainFrame(false);
  CheckDataReductionProxyLoFiSyntheticTrial(false);
}

TEST_F(DataReductionProxySettingsTest, TestLoFiImplicitOptOutClicksPerSession) {
  test_context_->config()->ResetLoFiStatusForTest();
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiLoadImagesPerSession));
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiSnackbarsShownPerSession));
  EXPECT_EQ(LoFiStatus::LOFI_STATUS_TEMPORARILY_OFF,
            test_context_->config()->GetLoFiStatus());

  // Click "Load images" |lo_fi_user_requests_for_images_per_session_| times.
  for (int i = 1; i <= settings_->lo_fi_user_requests_for_images_per_session_;
       ++i) {
    settings_->IncrementLoFiSnackbarShown();
    settings_->SetLoFiModeActiveOnMainFrame(true);
    settings_->IncrementLoFiUserRequestsForImages();
    EXPECT_EQ(i, test_context_->pref_service()->GetInteger(
                     prefs::kLoFiLoadImagesPerSession));
    EXPECT_EQ(i, test_context_->pref_service()->GetInteger(
                     prefs::kLoFiSnackbarsShownPerSession));
  }

  test_context_->RunUntilIdle();
  EXPECT_EQ(1, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiConsecutiveSessionDisables));
  EXPECT_EQ(LoFiStatus::LOFI_STATUS_OFF,
            test_context_->config()->GetLoFiStatus());

  // Reset the opt out pref values and config Lo-Fi status as if we're starting
  // a new session.
  test_context_->config()->ResetLoFiStatusForTest();
  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiLoadImagesPerSession));
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiSnackbarsShownPerSession));
  EXPECT_EQ(1, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiConsecutiveSessionDisables));
  EXPECT_EQ(LoFiStatus::LOFI_STATUS_TEMPORARILY_OFF,
            test_context_->config()->GetLoFiStatus());

  // Don't show any snackbars or have any "Load images" requests, but start
  // a new session. kLoFiConsecutiveSessionDisables should not reset since
  // the minimum number of snackbars were not shown.
  test_context_->config()->ResetLoFiStatusForTest();
  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiLoadImagesPerSession));
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiSnackbarsShownPerSession));
  EXPECT_EQ(1, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiConsecutiveSessionDisables));
  EXPECT_EQ(LoFiStatus::LOFI_STATUS_TEMPORARILY_OFF,
            test_context_->config()->GetLoFiStatus());

  // Have a session that doesn't have
  // |lo_fi_user_requests_for_images_per_session_|, but has that number of
  // snackbars shown so kLoFiConsecutiveSessionDisables resets.
  for (int i = 1;
       i <= settings_->lo_fi_user_requests_for_images_per_session_ - 1; ++i) {
    settings_->IncrementLoFiSnackbarShown();
    settings_->SetLoFiModeActiveOnMainFrame(true);
    settings_->IncrementLoFiUserRequestsForImages();
    EXPECT_EQ(i, test_context_->pref_service()->GetInteger(
                     prefs::kLoFiLoadImagesPerSession));
    EXPECT_EQ(i, test_context_->pref_service()->GetInteger(
                     prefs::kLoFiSnackbarsShownPerSession));
  }
  settings_->IncrementLoFiSnackbarShown();
  EXPECT_EQ(settings_->lo_fi_user_requests_for_images_per_session_,
            test_context_->pref_service()->GetInteger(
                prefs::kLoFiSnackbarsShownPerSession));

  test_context_->RunUntilIdle();
  // Still should have only one consecutive session disable and Lo-Fi status
  // should have been set to off.
  EXPECT_EQ(1, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiConsecutiveSessionDisables));
  EXPECT_EQ(LoFiStatus::LOFI_STATUS_TEMPORARILY_OFF,
            test_context_->config()->GetLoFiStatus());

  // Start a new session. The consecutive session count should now be reset to
  // zero.
  test_context_->config()->ResetLoFiStatusForTest();
  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiConsecutiveSessionDisables));
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiSnackbarsShownPerSession));
}

TEST_F(DataReductionProxySettingsTest,
       TestLoFiImplicitOptOutConsecutiveSessions) {
  test_context_->config()->ResetLoFiStatusForTest();
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiLoadImagesPerSession));
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiConsecutiveSessionDisables));
  EXPECT_EQ(LoFiStatus::LOFI_STATUS_TEMPORARILY_OFF,
            test_context_->config()->GetLoFiStatus());

  // Disable Lo-Fi for |lo_fi_consecutive_session_disables_|.
  for (int i = 1; i <= settings_->lo_fi_consecutive_session_disables_; ++i) {
    // Start a new session.
    test_context_->config()->ResetLoFiStatusForTest();
    settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
    EXPECT_EQ(LoFiStatus::LOFI_STATUS_TEMPORARILY_OFF,
              test_context_->config()->GetLoFiStatus());

    // Click "Load images" |lo_fi_user_requests_for_images_per_session_| times
    // for each session.
    for (int j = 1; j <= settings_->lo_fi_user_requests_for_images_per_session_;
         ++j) {
      settings_->SetLoFiModeActiveOnMainFrame(true);
      settings_->IncrementLoFiUserRequestsForImages();
      settings_->IncrementLoFiSnackbarShown();
      EXPECT_EQ(j, test_context_->pref_service()->GetInteger(
                       prefs::kLoFiLoadImagesPerSession));
      EXPECT_EQ(j, test_context_->pref_service()->GetInteger(
                       prefs::kLoFiSnackbarsShownPerSession));
    }

    test_context_->RunUntilIdle();
    EXPECT_EQ(i, test_context_->pref_service()->GetInteger(
                     prefs::kLoFiConsecutiveSessionDisables));
    EXPECT_EQ(LoFiStatus::LOFI_STATUS_OFF,
              test_context_->config()->GetLoFiStatus());
  }

  // Start a new session. Lo-Fi should be set off.
  test_context_->config()->ResetLoFiStatusForTest();
  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  test_context_->RunUntilIdle();
  EXPECT_EQ(3, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiConsecutiveSessionDisables));
  EXPECT_EQ(LoFiStatus::LOFI_STATUS_OFF,
            test_context_->config()->GetLoFiStatus());

  // Set the implicit opt out epoch to -1 so that the default value of zero will
  // be an increase and the opt out status will be reset.
  test_context_->pref_service()->SetInteger(prefs::kLoFiImplicitOptOutEpoch,
                                            -1);

  // Start a new session. Lo-Fi should be set on again.
  test_context_->config()->ResetLoFiStatusForTest();
  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  test_context_->RunUntilIdle();
  EXPECT_EQ(0, test_context_->pref_service()->GetInteger(
                   prefs::kLoFiConsecutiveSessionDisables));
  EXPECT_EQ(LoFiStatus::LOFI_STATUS_TEMPORARILY_OFF,
            test_context_->config()->GetLoFiStatus());
}

TEST_F(DataReductionProxySettingsTest, TestLoFiImplicitOptOutHistograms) {
  const char kUMALoFiImplicitOptOutAction[] =
      "DataReductionProxy.LoFi.ImplicitOptOutAction.Unknown";
  base::HistogramTester histogram_tester;

  // Disable Lo-Fi for |lo_fi_consecutive_session_disables_|.
  for (int i = 1; i <= settings_->lo_fi_consecutive_session_disables_; ++i) {
    // Start a new session.
    test_context_->config()->ResetLoFiStatusForTest();
    settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();

    // Click "Show images" |lo_fi_show_images_clicks_per_session_| times for
    // each session.
    for (int j = 1; j <= settings_->lo_fi_user_requests_for_images_per_session_;
         ++j) {
      settings_->SetLoFiModeActiveOnMainFrame(true);
      settings_->IncrementLoFiUserRequestsForImages();
    }

    test_context_->RunUntilIdle();
    histogram_tester.ExpectBucketCount(
        kUMALoFiImplicitOptOutAction, LO_FI_OPT_OUT_ACTION_DISABLED_FOR_SESSION,
        i);
  }

  histogram_tester.ExpectBucketCount(
      kUMALoFiImplicitOptOutAction,
      LO_FI_OPT_OUT_ACTION_DISABLED_UNTIL_NEXT_EPOCH, 1);

  // Set the implicit opt out epoch to -1 so that the default value of zero
  // will be an increase and implicit opt out will be reset.
  test_context_->pref_service()->SetInteger(prefs::kLoFiImplicitOptOutEpoch,
                                            -1);
  test_context_->config()->ResetLoFiStatusForTest();
  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  test_context_->RunUntilIdle();
  histogram_tester.ExpectBucketCount(kUMALoFiImplicitOptOutAction,
                                     LO_FI_OPT_OUT_ACTION_NEXT_EPOCH, 1);
}

TEST_F(DataReductionProxySettingsTest, TestLoFiSessionStateHistograms) {
  const char kUMALoFiSessionState[] = "DataReductionProxy.LoFi.SessionState";
  base::HistogramTester histogram_tester;

  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  histogram_tester.ExpectBucketCount(
      kUMALoFiSessionState,
      DataReductionProxyService::LO_FI_SESSION_STATE_NOT_USED, 1);

  // Disable Lo-Fi for |lo_fi_consecutive_session_disables_|.
  for (int i = 1; i <= settings_->lo_fi_consecutive_session_disables_; ++i) {
    settings_->SetLoFiModeActiveOnMainFrame(true);

    // Click "Show images" |lo_fi_show_images_clicks_per_session_| times for
    // each session. This would put user in either the temporarary opt out
    // state or permanent opt out.
    for (int j = 1; j <= settings_->lo_fi_user_requests_for_images_per_session_;
         ++j) {
      settings_->IncrementLoFiUserRequestsForImages();
    }

    // Start a new session.
    test_context_->config()->ResetLoFiStatusForTest();
    settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
    test_context_->RunUntilIdle();
    histogram_tester.ExpectBucketCount(
        kUMALoFiSessionState,
        DataReductionProxyService::LO_FI_SESSION_STATE_USED, 0);

    histogram_tester.ExpectBucketCount(
        kUMALoFiSessionState,
        DataReductionProxyService::LO_FI_SESSION_STATE_NOT_USED, 1);

    if (i < settings_->lo_fi_consecutive_session_disables_) {
      histogram_tester.ExpectBucketCount(
          kUMALoFiSessionState,
          DataReductionProxyService::LO_FI_SESSION_STATE_TEMPORARILY_OPTED_OUT,
          i);
      // Still permanently not opted out.
      histogram_tester.ExpectBucketCount(
          kUMALoFiSessionState,
          DataReductionProxyService::LO_FI_SESSION_STATE_OPTED_OUT, 0);
    } else {
      // Permanently opted out.
      histogram_tester.ExpectBucketCount(
          kUMALoFiSessionState,
          DataReductionProxyService::LO_FI_SESSION_STATE_OPTED_OUT, 1);
    }
  }

  // Total count should be equal to the number of sessions.
  histogram_tester.ExpectTotalCount(
      kUMALoFiSessionState, settings_->lo_fi_consecutive_session_disables_ + 1);

  // Set the implicit opt out epoch to -1 so that the default value of zero
  // will be an increase and implicit opt out will be reset. This session
  // should count that the previous session was implicitly opted out.
  test_context_->pref_service()->SetInteger(prefs::kLoFiImplicitOptOutEpoch,
                                            -1);
  test_context_->config()->ResetLoFiStatusForTest();
  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  test_context_->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUMALoFiSessionState,
      DataReductionProxyService::LO_FI_SESSION_STATE_OPTED_OUT, 2);

  // The implicit opt out epoch should cause the state to no longer be opt out.
  test_context_->config()->ResetLoFiStatusForTest();
  settings_->data_reduction_proxy_service_->InitializeLoFiPrefs();
  test_context_->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUMALoFiSessionState,
      DataReductionProxyService::LO_FI_SESSION_STATE_NOT_USED, 2);

  // Total count should be equal to the number of sessions.
  histogram_tester.ExpectTotalCount(
      kUMALoFiSessionState, settings_->lo_fi_consecutive_session_disables_ + 3);
}

TEST_F(DataReductionProxySettingsTest, TestGetDailyContentLengths) {
  ContentLengthList result =
      settings_->GetDailyContentLengths(prefs::kDailyHttpOriginalContentLength);

  ASSERT_FALSE(result.empty());
  ASSERT_EQ(kNumDaysInHistory, result.size());

  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    long expected_length =
        static_cast<long>((kNumDaysInHistory - 1 - i) * 2);
    ASSERT_EQ(expected_length, result[i]);
  }
}

TEST_F(DataReductionProxySettingsTest, CheckInitMetricsWhenNotAllowed) {
  // No call to |AddProxyToCommandLine()| was made, so the proxy feature
  // should be unavailable.
  // Clear the command line. Setting flags can force the proxy to be allowed.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);

  ResetSettings(false, false, false, false);
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_FALSE(settings->allowed_);
  EXPECT_CALL(*settings, RecordStartupState(PROXY_NOT_AVAILABLE));

  settings_->InitDataReductionProxySettings(
      test_context_->pref_service(), test_context_->io_data(),
      test_context_->CreateDataReductionProxyService());
  settings_->SetCallbackToRegisterSyntheticFieldTrial(
      base::Bind(&DataReductionProxySettingsTestBase::
                 SyntheticFieldTrialRegistrationCallback,
                 base::Unretained(this)));

  test_context_->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, CheckQUICFieldTrials) {
  const struct {
    bool enable_quic;
    std::string field_trial_group_name;
  } tests[] = {
      {
          false, std::string(),
      },
      {
          false, "NotEnabled",
      },
      {
          false, "Control",
      },
      {
          false, "Disabled",
      },
      {
          true, "EnabledControl",
      },
      {
          true, "Enabled",
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    // No call to |AddProxyToCommandLine()| was made, so the proxy feature
    // should be unavailable.
    // Clear the command line. Setting flags can force the proxy to be allowed.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);

    ResetSettings(false, false, false, false);
    MockSettings* settings = static_cast<MockSettings*>(settings_.get());
    EXPECT_FALSE(settings->Allowed());
    EXPECT_CALL(*settings, RecordStartupState(PROXY_NOT_AVAILABLE));

    settings_->InitDataReductionProxySettings(
         test_context_->pref_service(), test_context_->io_data(),
         test_context_->CreateDataReductionProxyService());

    base::FieldTrialList field_trial_list(new base::MockEntropyProvider());

    base::FieldTrialList::CreateFieldTrial(params::GetQuicFieldTrialName(),
                                           tests[i].field_trial_group_name);
    EXPECT_EQ(
        tests[i].field_trial_group_name,
        base::FieldTrialList::FindFullName(params::GetQuicFieldTrialName()));
    test_context_->config()->EnableQuic(tests[i].enable_quic);

    settings_->SetCallbackToRegisterSyntheticFieldTrial(
        base::Bind(&DataReductionProxySettingsTestBase::
                       SyntheticFieldTrialRegistrationCallback,
                   base::Unretained(this)));

    net::ProxyServer origin =
        test_context_->config()->test_params()->proxies_for_http().front();
    EXPECT_EQ(tests[i].enable_quic, origin.is_quic()) << i;
  }
}

}  // namespace data_reduction_proxy
