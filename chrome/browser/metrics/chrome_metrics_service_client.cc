// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"
#include "chrome/browser/metrics/https_engagement_metrics_provider.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/metrics/sampling_metrics_provider.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/metrics/time_ticks_experiment_win.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/features.h"
#include "chrome/installer/util/util_constants.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/net/version_utils.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/device_info/device_count_metrics_provider.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/notification_service.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/metrics/android_metrics_provider.h"
#include "chrome/browser/metrics/page_load_metrics_provider.h"
#endif

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/service_process/service_process_control.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/metrics/extensions_metrics_provider.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/metrics/plugin_metrics_provider.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/signin/signin_status_metrics_provider_chromeos.h"
#endif

#if defined(OS_WIN)
#include <windows.h>

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"
#include "chrome/browser/metrics/google_update_metrics_provider_win.h"
#include "chrome/common/metrics_constants_util_win.h"
#include "chrome/installer/util/browser_distribution.h"
#include "components/browser_watcher/watcher_metrics_provider_win.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/chrome_signin_status_metrics_provider_delegate.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#endif  // !defined(OS_CHROMEOS)

namespace {

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

// Checks whether it is the first time that cellular uploads logic should be
// enabled based on whether the the preference for that logic is initialized.
// This should happen only once as the used preference will be initialized
// afterwards in |UmaSessionStats.java|.
bool ShouldClearSavedMetrics() {
#if BUILDFLAG(ANDROID_JAVA_UI)
  PrefService* local_state = g_browser_process->local_state();
  return !local_state->HasPrefPath(metrics::prefs::kMetricsReportingEnabled) &&
         metrics::IsCellularLogicEnabled();
#else
  return false;
#endif
}

void RegisterInstallerFileMetricsPreferences(PrefRegistrySimple* registry) {
  metrics::FileMetricsProvider::RegisterPrefs(
      registry, ChromeMetricsServiceClient::kBrowserMetricsName);

#if defined(OS_WIN)
  metrics::FileMetricsProvider::RegisterPrefs(
      registry, installer::kSetupHistogramAllocatorName);
#endif
}

std::unique_ptr<metrics::FileMetricsProvider>
CreateInstallerFileMetricsProvider(bool metrics_reporting_enabled) {
  // Fetch a worker-pool for performing I/O tasks that are not allowed on
  // the main UI thread.
  scoped_refptr<base::TaskRunner> task_runner =
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  // Create an object to monitor files of metrics and include them in reports.
  std::unique_ptr<metrics::FileMetricsProvider> file_metrics_provider(
      new metrics::FileMetricsProvider(task_runner,
                                       g_browser_process->local_state()));

  // Create the full pathname of the file holding browser metrics.
  base::FilePath metrics_file;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &metrics_file)) {
    metrics_file =
        metrics_file
            .AppendASCII(ChromeMetricsServiceClient::kBrowserMetricsName)
            .AddExtension(base::PersistentMemoryAllocator::kFileExtension);

    if (metrics_reporting_enabled) {
      // Enable reading any existing saved metrics.
      file_metrics_provider->RegisterSource(
          metrics_file,
          metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          metrics::FileMetricsProvider::ASSOCIATE_PREVIOUS_RUN,
          ChromeMetricsServiceClient::kBrowserMetricsName);
    } else {
      // When metrics reporting is not enabled, any existing file should be
      // deleted in order to preserve user privacy.
      task_runner->PostTask(FROM_HERE,
                            base::Bind(base::IgnoreResult(&base::DeleteFile),
                                       metrics_file, /*recursive=*/false));
    }
  }

#if defined(OS_WIN)
  // Read metrics file from setup.exe.
  base::FilePath program_dir;
  base::PathService::Get(base::DIR_EXE, &program_dir);
  file_metrics_provider->RegisterSource(
      program_dir.AppendASCII(installer::kSetupHistogramAllocatorName),
      metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
      installer::kSetupHistogramAllocatorName);
#endif

  return file_metrics_provider;
}

// If there is a global metrics file being updated on disk, mark it to be
// deleted when the process exits. A normal shutdown is almost complete
// so there is no benefit in keeping a file with no new data to be processed
// during the next startup sequence. Deleting the file during shutdown adds
// an extra disk-access or two to shutdown but eliminates the unnecessary
// processing of the contents during startup only to find nothing.
void CleanUpGlobalPersistentHistogramStorage() {
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (!allocator)
    return;

  const base::FilePath& path = allocator->GetPersistentLocation();
  if (path.empty())
    return;

  // Open (with delete) and then immediately close the file by going out of
  // scope. This is the only cross-platform safe way to delete a file that may
  // be open elsewhere. Open handles will continue to operate normally but
  // new opens will not be possible.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                        base::File::FLAG_DELETE_ON_CLOSE);
}

}  // namespace


const char ChromeMetricsServiceClient::kBrowserMetricsName[] = "BrowserMetrics";

ChromeMetricsServiceClient::ChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
#if defined(OS_CHROMEOS)
      chromeos_metrics_provider_(nullptr),
#endif
      waiting_for_collect_final_metrics_step_(false),
      num_async_histogram_fetches_in_progress_(0),
      profiler_metrics_provider_(nullptr),
#if defined(ENABLE_PLUGINS)
      plugin_metrics_provider_(nullptr),
#endif
#if defined(OS_WIN)
      google_update_metrics_provider_(nullptr),
#endif
      drive_metrics_provider_(nullptr),
      start_time_(base::TimeTicks::Now()),
      has_uploaded_profiler_data_(false),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RecordCommandLineMetrics();
  RegisterForNotifications();
}

ChromeMetricsServiceClient::~ChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CleanUpGlobalPersistentHistogramStorage();
}

// static
std::unique_ptr<ChromeMetricsServiceClient> ChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager) {
  // Perform two-phase initialization so that |client->metrics_service_| only
  // receives pointers to fully constructed objects.
  std::unique_ptr<ChromeMetricsServiceClient> client(
      new ChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client;
}

// static
void ChromeMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);

  RegisterInstallerFileMetricsPreferences(registry);

  metrics::RegisterMetricsReportingStatePrefs(registry);

#if BUILDFLAG(ANDROID_JAVA_UI)
  AndroidMetricsProvider::RegisterPrefs(registry);
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

#if defined(ENABLE_PLUGINS)
  PluginMetricsProvider::RegisterPrefs(registry);
#endif  // defined(ENABLE_PLUGINS)
}

metrics::MetricsService* ChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

void ChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
}

bool ChromeMetricsServiceClient::IsOffTheRecordSessionActive() {
  return chrome::IsIncognitoSessionActive();
}

int32_t ChromeMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string ChromeMetricsServiceClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return google_brand::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel ChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(chrome::GetChannel());
}

std::string ChromeMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void ChromeMetricsServiceClient::OnLogUploadComplete() {
  // Collect time ticks stats after each UMA upload.
#if defined(OS_WIN)
  chrome::CollectTimeTicksStats();
#endif
}

void ChromeMetricsServiceClient::InitializeSystemProfileMetrics(
    const base::Closure& done_callback) {
  finished_init_task_callback_ = done_callback;
  base::Closure got_hardware_class_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotHardwareClass,
                 weak_ptr_factory_.GetWeakPtr());
#if defined(OS_CHROMEOS)
  chromeos_metrics_provider_->InitTaskGetHardwareClass(
      got_hardware_class_callback);
#else
  got_hardware_class_callback.Run();
#endif  // defined(OS_CHROMEOS)
}

void ChromeMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  collect_final_metrics_done_callback_ = done_callback;

  if (ShouldIncludeProfilerDataInLog()) {
    // Fetch profiler data. This will call into
    // |FinishedReceivingProfilerData()| when the task completes.
    metrics::TrackingSynchronizer::FetchProfilerDataAsynchronously(
        weak_ptr_factory_.GetWeakPtr());
  } else {
    CollectFinalHistograms();
  }
}

std::unique_ptr<metrics::MetricsLogUploader>
ChromeMetricsServiceClient::CreateUploader(
    const base::Callback<void(int)>& on_upload_complete) {
  return std::unique_ptr<metrics::MetricsLogUploader>(
      new metrics::NetMetricsLogUploader(
          g_browser_process->system_request_context(),
          metrics::kDefaultMetricsServerUrl, metrics::kDefaultMetricsMimeType,
          on_upload_complete));
}

base::TimeDelta ChromeMetricsServiceClient::GetStandardUploadInterval() {
  return metrics::GetUploadInterval();
}

base::string16 ChromeMetricsServiceClient::GetRegistryBackupKey() {
#if defined(OS_WIN)
  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  return distribution->GetRegistryPath().append(L"\\StabilityMetrics");
#else
  return base::string16();
#endif
}

void ChromeMetricsServiceClient::OnPluginLoadingError(
    const base::FilePath& plugin_path) {
#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_->LogPluginLoadingError(plugin_path);
#else
  NOTREACHED();
#endif  // defined(ENABLE_PLUGINS)
}

bool ChromeMetricsServiceClient::IsReportingPolicyManaged() {
  return IsMetricsReportingPolicyManaged();
}

metrics::EnableMetricsDefault
ChromeMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::GetMetricsReportingDefaultState(
      g_browser_process->local_state());
}

void ChromeMetricsServiceClient::Initialize() {
  // Clear metrics reports if it is the first time cellular upload logic should
  // apply to avoid sudden bulk uploads. It needs to be done before initializing
  // metrics service so that metrics log manager is initialized correctly.
  if (ShouldClearSavedMetrics()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->ClearPref(metrics::prefs::kMetricsInitialLogs);
    local_state->ClearPref(metrics::prefs::kMetricsOngoingLogs);
  }

  metrics_service_.reset(new metrics::MetricsService(
      metrics_state_manager_, this, g_browser_process->local_state()));

  // Gets access to persistent metrics shared by sub-processes.
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new SubprocessMetricsProvider()));

  // Register metrics providers.
#if defined(ENABLE_EXTENSIONS)
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new ExtensionsMetricsProvider(metrics_state_manager_)));
#endif
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::NetworkMetricsProvider(
              content::BrowserThread::GetBlockingPool())));

  // Currently, we configure OmniboxMetricsProvider to not log events to UMA
  // if there is a single incognito session visible. In the future, it may
  // be worth revisiting this to still log events from non-incognito sessions.
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(new OmniboxMetricsProvider(
          base::Bind(&chrome::IsIncognitoSessionActive))));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new ChromeStabilityMetricsProvider(
              g_browser_process->local_state())));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::GPUMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::ScreenInfoMetricsProvider));

  metrics_service_->RegisterMetricsProvider(CreateInstallerFileMetricsProvider(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()));

  drive_metrics_provider_ = new metrics::DriveMetricsProvider(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::FILE),
      chrome::FILE_LOCAL_STATE);
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(drive_metrics_provider_));

  profiler_metrics_provider_ = new metrics::ProfilerMetricsProvider(
      base::Bind(&metrics::IsCellularLogicEnabled));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(profiler_metrics_provider_));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::CallStackProfileMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::SamplingMetricsProvider));

#if BUILDFLAG(ANDROID_JAVA_UI)
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new AndroidMetricsProvider(g_browser_process->local_state())));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(new PageLoadMetricsProvider()));
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

#if defined(OS_WIN)
  google_update_metrics_provider_ = new GoogleUpdateMetricsProviderWin;
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          google_update_metrics_provider_));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new browser_watcher::WatcherMetricsProviderWin(
              chrome::GetBrowserExitCodesRegistryPath(),
              content::BrowserThread::GetBlockingPool())));

  antivirus_metrics_provider_ = new AntiVirusMetricsProvider(
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(antivirus_metrics_provider_));
#endif  // defined(OS_WIN)

#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_ =
      new PluginMetricsProvider(g_browser_process->local_state());
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(plugin_metrics_provider_));
#endif  // defined(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)
  ChromeOSMetricsProvider* chromeos_metrics_provider =
      new ChromeOSMetricsProvider;
  chromeos_metrics_provider_ = chromeos_metrics_provider;
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(chromeos_metrics_provider));

  SigninStatusMetricsProviderChromeOS* signin_metrics_provider_cros =
      new SigninStatusMetricsProviderChromeOS;
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(signin_metrics_provider_cros));

  // Record default UMA state as opt-out for all Chrome OS users, if not
  // recorded yet.
  PrefService* local_state = g_browser_process->local_state();
  if (metrics::GetMetricsReportingDefaultState(local_state) ==
      metrics::EnableMetricsDefault::DEFAULT_UNKNOWN) {
    metrics::RecordMetricsReportingDefaultState(
        local_state, metrics::EnableMetricsDefault::OPT_OUT);
  }
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          SigninStatusMetricsProvider::CreateInstance(base::WrapUnique(
              new ChromeSigninStatusMetricsProviderDelegate))));
#endif  // !defined(OS_CHROMEOS)

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new sync_driver::DeviceCountMetricsProvider(base::Bind(
              &browser_sync::ChromeSyncClient::GetDeviceInfoTrackers))));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new HttpsEngagementMetricsProvider()));

  // Clear stability metrics if it is the first time cellular upload logic
  // should apply to avoid sudden bulk uploads. It needs to be done after all
  // providers are registered.
  if (ShouldClearSavedMetrics())
    metrics_service_->ClearSavedStabilityMetrics();
}

void ChromeMetricsServiceClient::OnInitTaskGotHardwareClass() {
  const base::Closure got_bluetooth_adapter_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotBluetoothAdapter,
                 weak_ptr_factory_.GetWeakPtr());
#if defined(OS_CHROMEOS)
  chromeos_metrics_provider_->InitTaskGetBluetoothAdapter(
      got_bluetooth_adapter_callback);
#else
  got_bluetooth_adapter_callback.Run();
#endif  // defined(OS_CHROMEOS)
}

void ChromeMetricsServiceClient::OnInitTaskGotBluetoothAdapter() {
  const base::Closure got_plugin_info_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotPluginInfo,
                 weak_ptr_factory_.GetWeakPtr());

#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_->GetPluginInformation(got_plugin_info_callback);
#else
  got_plugin_info_callback.Run();
#endif  // defined(ENABLE_PLUGINS)
}

void ChromeMetricsServiceClient::OnInitTaskGotPluginInfo() {
  const base::Closure got_metrics_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotGoogleUpdateData,
                 weak_ptr_factory_.GetWeakPtr());

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  google_update_metrics_provider_->GetGoogleUpdateData(got_metrics_callback);
#else
  got_metrics_callback.Run();
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
}

void ChromeMetricsServiceClient::OnInitTaskGotGoogleUpdateData() {
  const base::Closure got_metrics_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotAntiVirusData,
                 weak_ptr_factory_.GetWeakPtr());

#if defined(OS_WIN)
  antivirus_metrics_provider_->GetAntiVirusMetrics(got_metrics_callback);
#else
  got_metrics_callback.Run();
#endif  // defined(OS_WIN)
}

void ChromeMetricsServiceClient::OnInitTaskGotAntiVirusData() {
  drive_metrics_provider_->GetDriveMetrics(
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotDriveMetrics,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ChromeMetricsServiceClient::OnInitTaskGotDriveMetrics() {
  finished_init_task_callback_.Run();
}

bool ChromeMetricsServiceClient::ShouldIncludeProfilerDataInLog() {
  // Upload profiler data at most once per session.
  if (has_uploaded_profiler_data_)
    return false;

  // For each log, flip a fair coin. Thus, profiler data is sent with the first
  // log with probability 50%, with the second log with probability 25%, and so
  // on. As a result, uploaded data is biased toward earlier logs.
  // TODO(isherman): Explore other possible algorithms, and choose one that
  // might be more appropriate.  For example, it might be reasonable to include
  // profiler data with some fixed probability, so that a given client might
  // upload profiler data more than once; but on average, clients won't upload
  // too much data.
  if (base::RandDouble() < 0.5)
    return false;

  has_uploaded_profiler_data_ = true;
  return true;
}

void ChromeMetricsServiceClient::ReceivedProfilerData(
    const metrics::ProfilerDataAttributes& attributes,
    const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
    const metrics::ProfilerEvents& past_events) {
  profiler_metrics_provider_->RecordProfilerData(
      process_data_phase, attributes.process_id, attributes.process_type,
      attributes.profiling_phase, attributes.phase_start - start_time_,
      attributes.phase_finish - start_time_, past_events);
}

void ChromeMetricsServiceClient::FinishedReceivingProfilerData() {
  CollectFinalHistograms();
}

void ChromeMetricsServiceClient::CollectFinalHistograms() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Begin the multi-step process of collecting memory usage histograms:
  // First spawn a task to collect the memory details; when that task is
  // finished, it will call OnMemoryDetailCollectionDone. That will in turn
  // call HistogramSynchronization to collect histograms from all renderers and
  // then call OnHistogramSynchronizationDone to continue processing.
  DCHECK(!waiting_for_collect_final_metrics_step_);
  waiting_for_collect_final_metrics_step_ = true;

  base::Closure callback =
      base::Bind(&ChromeMetricsServiceClient::OnMemoryDetailCollectionDone,
                 weak_ptr_factory_.GetWeakPtr());

  scoped_refptr<MetricsMemoryDetails> details(
      new MetricsMemoryDetails(callback, &memory_growth_tracker_));
  details->StartFetch();
}

void ChromeMetricsServiceClient::MergeHistogramDeltas() {
  DCHECK(GetMetricsService());
  GetMetricsService()->MergeHistogramDeltas();
}

void ChromeMetricsServiceClient::OnMemoryDetailCollectionDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);

  // Create a callback_task for OnHistogramSynchronizationDone.
  base::Closure callback = base::Bind(
      &ChromeMetricsServiceClient::OnHistogramSynchronizationDone,
      weak_ptr_factory_.GetWeakPtr());

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kMaxHistogramGatheringWaitDuration);

  DCHECK_EQ(num_async_histogram_fetches_in_progress_, 0);

#if !defined(ENABLE_PRINT_PREVIEW)
  num_async_histogram_fetches_in_progress_ = 2;
#else   // !ENABLE_PRINT_PREVIEW
  num_async_histogram_fetches_in_progress_ = 3;
  // Run requests to service and content in parallel.
  if (!ServiceProcessControl::GetInstance()->GetHistograms(callback, timeout)) {
    // Assume |num_async_histogram_fetches_in_progress_| is not changed by
    // |GetHistograms()|.
    DCHECK_EQ(num_async_histogram_fetches_in_progress_, 3);
    // Assign |num_async_histogram_fetches_in_progress_| above and decrement it
    // here to make code work even if |GetHistograms()| fired |callback|.
    --num_async_histogram_fetches_in_progress_;
  }
#endif  // !ENABLE_PRINT_PREVIEW

  // Merge histograms from metrics providers into StatisticsRecorder.
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ChromeMetricsServiceClient::MergeHistogramDeltas,
                 weak_ptr_factory_.GetWeakPtr()),
      callback);

  // Set up the callback task to call after we receive histograms from all
  // child processes. |timeout| specifies how long to wait before absolutely
  // calling us back on the task.
  content::FetchHistogramsAsynchronously(base::MessageLoop::current(), callback,
                                         timeout);
}

void ChromeMetricsServiceClient::OnHistogramSynchronizationDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);
  DCHECK_GT(num_async_histogram_fetches_in_progress_, 0);

  // Check if all expected requests finished.
  if (--num_async_histogram_fetches_in_progress_ > 0)
    return;

  waiting_for_collect_final_metrics_step_ = false;
  collect_final_metrics_done_callback_.Run();
}

void ChromeMetricsServiceClient::RecordCommandLineMetrics() {
  // Get stats on use of command line.
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  size_t common_commands = 0;
  if (command_line->HasSwitch(switches::kUserDataDir)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineDatDirCount", 1);
  }

  if (command_line->HasSwitch(switches::kApp)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineAppModeCount", 1);
  }

  // TODO(rohitrao): Should these be logged on iOS as well?
  // http://crbug.com/375794
  size_t switch_count = command_line->GetSwitches().size();
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineFlagCount", switch_count);
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineUncommonFlagCount",
                           switch_count - common_commands);
}

void ChromeMetricsServiceClient::RegisterForNotifications() {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_PARENTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CLOSING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());

  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&ChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
                     base::Unretained(this)));
}

void ChromeMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED:
    case chrome::NOTIFICATION_BROWSER_CLOSED:
    case chrome::NOTIFICATION_TAB_PARENTED:
    case chrome::NOTIFICATION_TAB_CLOSING:
    case content::NOTIFICATION_LOAD_STOP:
    case content::NOTIFICATION_LOAD_START:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      metrics_service_->OnApplicationNotIdle();
      break;

    default:
      NOTREACHED();
  }
}

void ChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}

bool ChromeMetricsServiceClient::IsUMACellularUploadLogicEnabled() {
  return metrics::IsCellularLogicEnabled();
}
