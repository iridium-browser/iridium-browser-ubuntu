// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/background_tracing_field_trial.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/background_tracing_preemptive_config.h"
#include "content/public/browser/background_tracing_reactive_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

namespace {

class ChromeTracingDelegateBrowserTest : public InProcessBrowserTest {
 public:
  ChromeTracingDelegateBrowserTest()
      : receive_count_(0),
        started_finalizations_count_(0),
        last_on_started_finalizing_success_(false) {}

  bool StartPreemptiveScenario(
      const base::Closure& on_upload_callback,
      content::BackgroundTracingManager::DataFiltering data_filtering) {
    on_upload_callback_ = on_upload_callback;

    scoped_ptr<content::BackgroundTracingPreemptiveConfig> config(
        new content::BackgroundTracingPreemptiveConfig());

    content::BackgroundTracingPreemptiveConfig::MonitoringRule rule;
    rule.type = content::BackgroundTracingPreemptiveConfig::
        MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED;
    rule.named_trigger_info.trigger_name = "test";

    config->configs.push_back(rule);

    content::BackgroundTracingManager::ReceiveCallback receive_callback =
        base::Bind(&ChromeTracingDelegateBrowserTest::OnUpload,
                   base::Unretained(this));

    return content::BackgroundTracingManager::GetInstance()->SetActiveScenario(
        config.Pass(), receive_callback, data_filtering);
  }

  void TriggerReactiveScenario(
      const base::Closure& on_started_finalization_callback) {
    on_started_finalization_callback_ = on_started_finalization_callback;
    trigger_handle_ =
        content::BackgroundTracingManager::GetInstance()->RegisterTriggerType(
            "test");

    content::BackgroundTracingManager::StartedFinalizingCallback
        started_finalizing_callback =
            base::Bind(&ChromeTracingDelegateBrowserTest::OnStartedFinalizing,
                       base::Unretained(this));
    content::BackgroundTracingManager::GetInstance()->TriggerNamedEvent(
        trigger_handle_, started_finalizing_callback);
  }

  int get_receive_count() const { return receive_count_; }
  bool get_started_finalizations() const {
    return started_finalizations_count_;
  }
  bool get_last_started_finalization_success() const {
    return last_on_started_finalizing_success_;
  }

 private:
  void OnUpload(const scoped_refptr<base::RefCountedString>& file_contents,
                scoped_ptr<base::DictionaryValue> metadata,
                base::Callback<void()> done_callback) {
    receive_count_ += 1;

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(done_callback));
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(on_upload_callback_));
  }

  void OnStartedFinalizing(bool success) {
    started_finalizations_count_++;
    last_on_started_finalizing_success_ = success;

    if (!on_started_finalization_callback_.is_null()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(on_started_finalization_callback_));
    }
  }

  base::Closure on_upload_callback_;
  base::Closure on_started_finalization_callback_;
  int receive_count_;
  int started_finalizations_count_;
  content::BackgroundTracingManager::TriggerHandle trigger_handle_;
  bool last_on_started_finalizing_success_;
};

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingTimeThrottled) {
  base::RunLoop wait_for_upload;

  EXPECT_TRUE(StartPreemptiveScenario(
      wait_for_upload.QuitClosure(),
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  TriggerReactiveScenario(base::Closure());

  wait_for_upload.Run();

  EXPECT_TRUE(get_receive_count() == 1);

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::Time last_upload_time = base::Time::FromInternalValue(
      local_state->GetInt64(prefs::kBackgroundTracingLastUpload));
  EXPECT_FALSE(last_upload_time.is_null());

  // We should not be able to start a new reactive scenario immediately after
  // a previous one gets uploaded.
  EXPECT_FALSE(StartPreemptiveScenario(
      base::Closure(), content::BackgroundTracingManager::NO_DATA_FILTERING));
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       BackgroundTracingThrottleTimeElapsed) {
  base::RunLoop wait_for_upload;

  EXPECT_TRUE(StartPreemptiveScenario(
      wait_for_upload.QuitClosure(),
      content::BackgroundTracingManager::NO_DATA_FILTERING));

  TriggerReactiveScenario(base::Closure());

  wait_for_upload.Run();

  EXPECT_TRUE(get_receive_count() == 1);

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::Time last_upload_time = base::Time::FromInternalValue(
      local_state->GetInt64(prefs::kBackgroundTracingLastUpload));
  EXPECT_FALSE(last_upload_time.is_null());

  // We move the last upload time to eight days in the past,
  // and at that point should be able to start a scenario again.
  base::Time new_upload_time = last_upload_time - base::TimeDelta::FromDays(8);
  local_state->SetInt64(prefs::kBackgroundTracingLastUpload,
                        new_upload_time.ToInternalValue());
  EXPECT_TRUE(StartPreemptiveScenario(
      base::Closure(), content::BackgroundTracingManager::NO_DATA_FILTERING));
}

// If we need a PII-stripped trace, any existing OTR session should block the
// trace.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       ExistingIncognitoSessionBlockingTraceStart) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(BrowserList::IsOffTheRecordSessionActive());
  EXPECT_FALSE(StartPreemptiveScenario(
      base::Closure(), content::BackgroundTracingManager::ANONYMIZE_DATA));
}

// If we need a PII-stripped trace, any new OTR session during tracing should
// block the finalization of the trace.
IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTest,
                       NewIncognitoSessionBlockingTraceFinalization) {
  EXPECT_TRUE(StartPreemptiveScenario(
      base::Closure(), content::BackgroundTracingManager::ANONYMIZE_DATA));

  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(BrowserList::IsOffTheRecordSessionActive());

  base::RunLoop wait_for_finalization_start;
  TriggerReactiveScenario(wait_for_finalization_start.QuitClosure());
  wait_for_finalization_start.Run();

  EXPECT_TRUE(get_started_finalizations() == 1);
  EXPECT_FALSE(get_last_started_finalization_success());
}

class ChromeTracingDelegateBrowserTestOnStartup
    : public ChromeTracingDelegateBrowserTest {
 protected:
  ChromeTracingDelegateBrowserTestOnStartup() {}

  static void FieldTrialConfigTextFilter(std::string* config_text) {
    ASSERT_TRUE(config_text);
    // We need to replace the config JSON with the full one here, as we can't
    // pass JSON through the fieldtrial switch parsing.
    if (*config_text == "default_config_for_testing") {
      *config_text =
          "{\"mode\":\"PREEMPTIVE_TRACING_MODE\", \"category\": "
          "\"BENCHMARK\",\"configs\": [{\"rule\": "
          "\"MONITOR_AND_DUMP_WHEN_TRIGGER_NAMED\",\"trigger_name\":"
          "\"test\"}]}";
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceFieldTrials, "BackgroundTracing/TestGroup/");
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceFieldTrialParams,
        "BackgroundTracing.TestGroup:config/default_config_for_testing");

    tracing::SetConfigTextFilterForTesting(&FieldTrialConfigTextFilter);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       ScenarioSetFromFieldtrial) {
  // We should reach this point without crashing.
  EXPECT_TRUE(content::BackgroundTracingManager::GetInstance()
                  ->HasActiveScenarioForTesting());
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       PRE_StartupTracingThrottle) {
  EXPECT_TRUE(content::BackgroundTracingManager::GetInstance()
                  ->HasActiveScenarioForTesting());

  // Simulate a trace upload.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state->SetInt64(prefs::kBackgroundTracingLastUpload,
                        base::Time::Now().ToInternalValue());
}

IN_PROC_BROWSER_TEST_F(ChromeTracingDelegateBrowserTestOnStartup,
                       StartupTracingThrottle) {
  // The startup scenario should *not* be started, since not enough
  // time has elapsed since the last upload (set in the PRE_ above).
  EXPECT_FALSE(content::BackgroundTracingManager::GetInstance()
                   ->HasActiveScenarioForTesting());
}

}  // namespace
