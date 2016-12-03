// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/tracing/background_tracing_field_trial.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"

#if defined(OS_ANDROID)
#include "chrome/browser/chrome_browser_field_trials_mobile.h"
#else
#include "chrome/browser/chrome_browser_field_trials_desktop.h"
#endif

namespace {

// Check for feature enabling the use of persistent histogram storage and
// enable the global allocator if so.
// TODO(bcwhite): Move this and CreateInstallerFileMetricsProvider into a new
// file and make kBrowserMetricsName local to that file.
void InstantiatePersistentHistograms() {
  base::FilePath metrics_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &metrics_dir))
    return;

  base::FilePath metrics_file =
      metrics_dir
          .AppendASCII(ChromeMetricsServiceClient::kBrowserMetricsName)
          .AddExtension(base::PersistentMemoryAllocator::kFileExtension);
  base::FilePath active_file =
      metrics_dir
          .AppendASCII(
              std::string(ChromeMetricsServiceClient::kBrowserMetricsName) +
              "-active")
          .AddExtension(base::PersistentMemoryAllocator::kFileExtension);

  // Move any existing "active" file to the final name from which it will be
  // read when reporting initial stability metrics. If there is no file to
  // move, remove any old, existing file from before the previous session.
  if (!base::ReplaceFile(active_file, metrics_file, nullptr))
    base::DeleteFile(metrics_file, /*recursive=*/false);

  // This is used to report results to an UMA histogram. It's an int because
  // arithmetic is done on the value. The corresponding "failed" case must
  // always appear directly after the "success" case.
  enum : int {
    LOCAL_MEMORY_SUCCESS,
    LOCAL_MEMORY_FAILED,
    MAPPED_FILE_SUCCESS,
    MAPPED_FILE_FAILED,
    CREATE_ALLOCATOR_RESULTS
  };
  int result;

  // Create persistent/shared memory and allow histograms to be stored in
  // it. Memory that is not actualy used won't be physically mapped by the
  // system. BrowserMetrics usage, as reported in UMA, peaked around 1.9MiB
  // as of 2016-02-20.
  const size_t kAllocSize = 3 << 20;     // 3 MiB
  const uint32_t kAllocId = 0x935DDD43;  // SHA1(BrowserMetrics)
  std::string storage = variations::GetVariationParamValueByFeature(
      base::kPersistentHistogramsFeature, "storage");
  if (storage == "MappedFile") {
    // Create global allocator with the "active" file.
    base::GlobalHistogramAllocator::CreateWithFile(
        active_file, kAllocSize, kAllocId,
        ChromeMetricsServiceClient::kBrowserMetricsName);
    result = MAPPED_FILE_SUCCESS;
  } else if (storage == "LocalMemory") {
    // Use local memory for storage even though it will not persist across
    // an unclean shutdown.
    base::GlobalHistogramAllocator::CreateWithLocalMemory(
        kAllocSize, kAllocId, ChromeMetricsServiceClient::kBrowserMetricsName);
    result = LOCAL_MEMORY_SUCCESS;
  } else {
    // Persistent metric storage is disabled.
    return;
  }

  // Get the allocator that was just created and report result. Exit if the
  // allocator could not be created.
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  UMA_HISTOGRAM_ENUMERATION("UMA.PersistentHistograms.InitResult",
                            result + (allocator ? 0 : 1),
                            CREATE_ALLOCATOR_RESULTS);
  if (!allocator)
    return;

  // Create tracking histograms for the allocator and record storage file.
  allocator->CreateTrackingHistograms(
      ChromeMetricsServiceClient::kBrowserMetricsName);
  allocator->SetPersistentLocation(active_file);
}

// Create a field trial to control metrics/crash sampling for Stable on
// Windows/Android if no variations seed was applied.
void CreateFallbackSamplingTrialIfNeeded(bool has_seed,
                                         base::FeatureList* feature_list) {
#if defined(OS_WIN) || defined(OS_ANDROID)
  // Only create the fallback trial if there isn't already a variations seed
  // being applied. This should occur during first run when first-run variations
  // isn't supported. It's assumed that, if there is a seed, then it either
  // contains the relavent study, or is intentionally omitted, so no fallback is
  // needed.
  if (has_seed)
    return;

  // Sampling is only supported on Stable.
  if (chrome::GetChannel() != version_info::Channel::STABLE)
    return;

  ChromeMetricsServicesManagerClient::CreateFallbackSamplingTrial(feature_list);
#endif  // defined(OS_WIN) || defined(OS_ANDROID)
}

}  // namespace

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(
    const base::CommandLine& parsed_command_line)
    : parsed_command_line_(parsed_command_line) {
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::SetupFieldTrials() {
  // Field trials that are shared by all platforms.
  InstantiateDynamicTrials();

#if defined(OS_ANDROID)
  chrome::SetupMobileFieldTrials(parsed_command_line_);
#else
  chrome::SetupDesktopFieldTrials(parsed_command_line_);
#endif
}

void ChromeBrowserFieldTrials::SetupFeatureControllingFieldTrials(
    bool has_seed,
    base::FeatureList* feature_list) {
  CreateFallbackSamplingTrialIfNeeded(has_seed, feature_list);
}

void ChromeBrowserFieldTrials::InstantiateDynamicTrials() {
  // Persistent histograms must be enabled as soon as possible.
  InstantiatePersistentHistograms();
  tracing::SetupBackgroundTracingFieldTrial();
}
