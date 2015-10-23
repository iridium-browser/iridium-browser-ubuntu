// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
class TaskRunner;
}

class Browser;

namespace safe_browsing {

// The registry key for the Reporter and Cleaner.
extern const wchar_t kSoftwareRemovalToolRegistryKey[];

// Reporter exit codes.
const int kSwReporterCleanupNeeded = 0;
const int kSwReporterNothingFound = 2;
const int kSwReporterPostRebootCleanupNeeded = 4;
const int kSwReporterDelayedPostRebootCleanupNeeded = 15;

// A special exit code identifying a failure to run the reporter.
const int kReporterFailureExitCode = INT_MAX;

// The number of days to wait before triggering another reporter run.
const int kDaysBetweenSuccessfulSwReporterRuns = 7;
const int kDaysBetweenSwReporterRunsForPendingPrompt = 1;

// To test SRT Fetches.
const int kSRTFetcherID = 47;

// Tries to run the sw_reporter component, and then schedule the next try. If
// called multiple times, then multiple sequences of trying to run will happen,
// yet only one reporter will run per specified period (either
// |kDaysBetweenSuccessfulSwReporterRuns| or
// |kDaysBetweenSwReporterRunsForPendingPrompt|) will actually happen.
// |exe_path| is the full path to the SwReporter to execute and |version| is its
// version. The task runners are provided to allow tests to provide their own.
void RunSwReporter(
    const base::FilePath& exe_path,
    const std::string& version,
    const scoped_refptr<base::TaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner);

// Test mocks for launching the reporter and showing the prompt
typedef base::Callback<int(const base::FilePath& exe_path,
                           const std::string& version)> ReporterLauncher;
typedef base::Callback<void(Browser*, const std::string&)> PromptTrigger;
void SetReporterLauncherForTesting(const ReporterLauncher& reporter_launcher);
void SetPromptTriggerForTesting(const PromptTrigger& prompt_trigger);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_
