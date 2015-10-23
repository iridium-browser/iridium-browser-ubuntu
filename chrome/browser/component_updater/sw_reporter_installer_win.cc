// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/strings/string_tokenizer.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/safe_browsing/srt_fetcher_win.h"
#include "chrome/browser/safe_browsing/srt_field_trial_win.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/component_updater/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

namespace {

// These two sets of values are used to send UMA information and are replicated
// in the histograms.xml file, so the order MUST NOT CHANGE.
enum SRTCompleted {
  SRT_COMPLETED_NOT_YET = 0,
  SRT_COMPLETED_YES = 1,
  SRT_COMPLETED_LATER = 2,
  SRT_COMPLETED_MAX,
};

// CRX hash. The extension id is: gkmgaooipdjhmangpemjhigmamcehddo. The hash was
// generated in Python with something like this:
// hashlib.sha256().update(open("<file>.crx").read()[16:16+294]).digest().
const uint8_t kSha256Hash[] = {0x6a, 0xc6, 0x0e, 0xe8, 0xf3, 0x97, 0xc0, 0xd6,
                               0xf4, 0xc9, 0x78, 0x6c, 0x0c, 0x24, 0x73, 0x3e,
                               0x05, 0xa5, 0x62, 0x4b, 0x2e, 0xc7, 0xb7, 0x1c,
                               0x5f, 0xea, 0xf0, 0x88, 0xf6, 0x97, 0x9b, 0xc7};

const base::FilePath::CharType kSwReporterExeName[] =
    FILE_PATH_LITERAL("software_reporter_tool.exe");

// Where to fetch the reporter's list of found uws in the registry.
const wchar_t kCleanerSuffixRegistryKey[] = L"Cleaner";
const wchar_t kEndTimeValueName[] = L"EndTime";
const wchar_t kExitCodeValueName[] = L"ExitCode";
const wchar_t kStartTimeValueName[] = L"StartTime";
const wchar_t kUploadResultsValueName[] = L"UploadResults";
const wchar_t kVersionValueName[] = L"Version";

void SRTHasCompleted(SRTCompleted value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Cleaner.HasCompleted", value,
                            SRT_COMPLETED_MAX);
}

void ReportVersionWithUma(const base::Version& version) {
  DCHECK(!version.components().empty());
  // The minor version is the 2nd last component of the version,
  // or just the first component if there is only 1.
  uint32_t minor_version = 0;
  if (version.components().size() > 1)
    minor_version = version.components()[version.components().size() - 2];
  else
    minor_version = version.components()[0];
  UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.MinorVersion", minor_version);

  // The major version for X.Y.Z is X*256^3+Y*256+Z. If there are additional
  // components, only the first three count, and if there are less than 3, the
  // missing values are just replaced by zero. So 1 is equivalent 1.0.0.
  DCHECK_LT(version.components()[0], 0x100U);
  uint32_t major_version = 0x1000000 * version.components()[0];
  if (version.components().size() >= 2) {
    DCHECK_LT(version.components()[1], 0x10000U);
    major_version += 0x100 * version.components()[1];
  }
  if (version.components().size() >= 3) {
    DCHECK_LT(version.components()[2], 0x100U);
    major_version += version.components()[2];
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.MajorVersion", major_version);
}

void ReportUploadsWithUma(const base::string16& upload_results) {
  base::WStringTokenizer tokenizer(upload_results, L";");
  int failure_count = 0;
  int success_count = 0;
  int longest_failure_run = 0;
  int current_failure_run = 0;
  bool last_result = false;
  while (tokenizer.GetNext()) {
    if (tokenizer.token() == L"0") {
      ++failure_count;
      ++current_failure_run;
      last_result = false;
    } else {
      ++success_count;
      current_failure_run = 0;
      last_result = true;
    }

    if (current_failure_run > longest_failure_run)
      longest_failure_run = current_failure_run;
  }

  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadFailureCount",
                           failure_count);
  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadSuccessCount",
                           success_count);
  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadLongestFailureRun",
                           longest_failure_run);
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.LastUploadResult", last_result);
}

class SwReporterInstallerTraits : public ComponentInstallerTraits {
 public:
  SwReporterInstallerTraits() {}

  ~SwReporterInstallerTraits() override {}

  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& dir) const override {
    return base::PathExists(dir.Append(kSwReporterExeName));
  }

  bool CanAutoUpdate() const override { return true; }

  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override {
    return true;
  }

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      scoped_ptr<base::DictionaryValue> manifest) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    ReportVersionWithUma(version);
    safe_browsing::RunSwReporter(install_dir.Append(kSwReporterExeName),
                                 version.GetString(),
                                 base::ThreadTaskRunnerHandle::Get(),
                                 base::WorkerPool::GetTaskRunner(true));
  }

  base::FilePath GetBaseDirectory() const override { return install_dir(); }

  void GetHash(std::vector<uint8_t>* hash) const override { GetPkHash(hash); }

  std::string GetName() const override { return "Software Reporter Tool"; }

  static base::FilePath install_dir() {
    // The base directory on windows looks like:
    // <profile>\AppData\Local\Google\Chrome\User Data\SwReporter\.
    base::FilePath result;
    PathService::Get(DIR_SW_REPORTER, &result);
    return result;
  }

  static std::string ID() {
    update_client::CrxComponent component;
    component.version = Version("0.0.0.0");
    GetPkHash(&component.pk_hash);
    return update_client::GetCrxComponentID(component);
  }

 private:
  static void GetPkHash(std::vector<uint8_t>* hash) {
    DCHECK(hash);
    hash->assign(kSha256Hash, kSha256Hash + sizeof(kSha256Hash));
  }
};

}  // namespace

void RegisterSwReporterComponent(ComponentUpdateService* cus) {
  // The Sw reporter doesn't need to run if the user isn't reporting metrics and
  // isn't in the SRTPrompt field trial "On" group.
  if (!ChromeMetricsServiceAccessor::IsMetricsReportingEnabled() &&
      !safe_browsing::IsInSRTPromptFieldTrialGroups()) {
    return;
  }

  // Check if we have information from Cleaner and record UMA statistics.
  base::string16 cleaner_key_name(
      safe_browsing::kSoftwareRemovalToolRegistryKey);
  cleaner_key_name.append(1, L'\\').append(kCleanerSuffixRegistryKey);
  base::win::RegKey cleaner_key(
      HKEY_CURRENT_USER, cleaner_key_name.c_str(), KEY_ALL_ACCESS);
  // Cleaner is assumed to have run if we have a start time.
  if (cleaner_key.Valid()) {
    if (cleaner_key.HasValue(kStartTimeValueName)) {
      // Get version number.
      if (cleaner_key.HasValue(kVersionValueName)) {
        DWORD version;
        cleaner_key.ReadValueDW(kVersionValueName, &version);
        UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.Cleaner.Version",
                                    version);
        cleaner_key.DeleteValue(kVersionValueName);
      }
      // Get start & end time. If we don't have an end time, we can assume the
      // cleaner has not completed.
      int64 start_time_value;
      cleaner_key.ReadInt64(kStartTimeValueName, &start_time_value);

      bool completed = cleaner_key.HasValue(kEndTimeValueName);
      SRTHasCompleted(completed ? SRT_COMPLETED_YES : SRT_COMPLETED_NOT_YET);
      if (completed) {
        int64 end_time_value;
        cleaner_key.ReadInt64(kEndTimeValueName, &end_time_value);
        cleaner_key.DeleteValue(kEndTimeValueName);
        base::TimeDelta run_time(
            base::Time::FromInternalValue(end_time_value) -
            base::Time::FromInternalValue(start_time_value));
        UMA_HISTOGRAM_LONG_TIMES("SoftwareReporter.Cleaner.RunningTime",
                                 run_time);
      }
      // Get exit code. Assume nothing was found if we can't read the exit code.
      DWORD exit_code = safe_browsing::kSwReporterNothingFound;
      if (cleaner_key.HasValue(kExitCodeValueName)) {
        cleaner_key.ReadValueDW(kExitCodeValueName, &exit_code);
        UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.Cleaner.ExitCode",
                                    exit_code);
        cleaner_key.DeleteValue(kExitCodeValueName);
      }
      cleaner_key.DeleteValue(kStartTimeValueName);

      if (exit_code == safe_browsing::kSwReporterPostRebootCleanupNeeded ||
          exit_code ==
              safe_browsing::kSwReporterDelayedPostRebootCleanupNeeded) {
        // Check if we are running after the user has rebooted.
        base::TimeDelta elapsed(
            base::Time::Now() -
            base::Time::FromInternalValue(start_time_value));
        DCHECK_GT(elapsed.InMilliseconds(), 0);
        UMA_HISTOGRAM_BOOLEAN(
            "SoftwareReporter.Cleaner.HasRebooted",
            static_cast<uint64>(elapsed.InMilliseconds()) > ::GetTickCount());
      }

      if (cleaner_key.HasValue(kUploadResultsValueName)) {
        base::string16 upload_results;
        cleaner_key.ReadValue(kUploadResultsValueName, &upload_results);
        ReportUploadsWithUma(upload_results);
      }
    } else {
      if (cleaner_key.HasValue(kEndTimeValueName)) {
        SRTHasCompleted(SRT_COMPLETED_LATER);
        cleaner_key.DeleteValue(kEndTimeValueName);
      }
    }
  }

  // Install the component.
  scoped_ptr<ComponentInstallerTraits> traits(new SwReporterInstallerTraits());
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(traits.Pass());
  installer->Register(cus, base::Closure());
}

void RegisterPrefsForSwReporter(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kSwReporterLastTimeTriggered, 0);
  registry->RegisterIntegerPref(prefs::kSwReporterLastExitCode, -1);
  registry->RegisterBooleanPref(prefs::kSwReporterPendingPrompt, false);
}

void RegisterProfilePrefsForSwReporter(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSwReporterPromptVersion, "");

  registry->RegisterStringPref(prefs::kSwReporterPromptSeed, "");
}

}  // namespace component_updater
