// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"
#include "chrome/browser/metrics/drive_metrics_provider.h"
#include "chrome/browser/metrics/omnibox_metrics_provider.h"
#include "chrome/browser/metrics/time_ticks_experiment_win.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/metrics/version_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/metrics/url_constants.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

#if defined(OS_ANDROID)
#include "chrome/browser/metrics/android_metrics_provider.h"
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
#include "chrome/browser/metrics/signin_status_metrics_provider_chromeos.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include "chrome/browser/metrics/google_update_metrics_provider_win.h"
#include "components/browser_watcher/watcher_metrics_provider_win.h"
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
#include "chrome/browser/metrics/signin_status_metrics_provider.h"
#endif  // !defined(OS_CHROMEOS) && !defined(OS_IOS)

namespace {

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

// Standard interval between log uploads, in seconds.
#if defined(OS_ANDROID) || defined(OS_IOS)
const int kStandardUploadIntervalSeconds = 5 * 60;  // Five minutes.
const int kStandardUploadIntervalCellularSeconds = 15 * 60;  // Fifteen minutes.
#else
const int kStandardUploadIntervalSeconds = 30 * 60;  // Thirty minutes.
#endif

// Returns true if current connection type is cellular and user is assigned to
// experimental group for enabled cellular uploads.
bool IsCellularLogicEnabled() {
  if (variations::GetVariationParamValue("UMA_EnableCellularLogUpload",
                                         "Enabled") != "true") {
    return false;
  }

  return net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
}

// Checks whether it is the first time that cellular uploads logic should be
// enabled based on whether the the preference for that logic is initialized.
// This should happen only once as the used preference will be initialized
// afterwards in |UmaSessionStats.java|.
bool ShouldClearSavedMetrics() {
#if defined(OS_ANDROID)
  PrefService* local_state = g_browser_process->local_state();
  return !local_state->HasPrefPath(prefs::kMetricsReportingEnabled) &&
         variations::GetVariationParamValue("UMA_EnableCellularLogUpload",
                                            "Enabled") == "true";
#else
  return false;
#endif
}

}  // namespace


ChromeMetricsServiceClient::ChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
      chromeos_metrics_provider_(nullptr),
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
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RecordCommandLineMetrics();
  RegisterForNotifications();
}

ChromeMetricsServiceClient::~ChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
scoped_ptr<ChromeMetricsServiceClient> ChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager,
    PrefService* local_state) {
  // Perform two-phase initialization so that |client->metrics_service_| only
  // receives pointers to fully constructed objects.
  scoped_ptr<ChromeMetricsServiceClient> client(
      new ChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client.Pass();
}

// static
void ChromeMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kUninstallLastLaunchTimeSec, 0);
  registry->RegisterInt64Pref(prefs::kUninstallLastObservedRunTimeSec, 0);

  metrics::MetricsService::RegisterPrefs(registry);
  ChromeStabilityMetricsProvider::RegisterPrefs(registry);

#if defined(OS_ANDROID)
  AndroidMetricsProvider::RegisterPrefs(registry);
#endif  // defined(OS_ANDROID)

#if defined(ENABLE_PLUGINS)
  PluginMetricsProvider::RegisterPrefs(registry);
#endif  // defined(ENABLE_PLUGINS)
}

void ChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
}

void ChromeMetricsServiceClient::OnRecordingDisabled() {
  crash_keys::ClearMetricsClientId();
}

bool ChromeMetricsServiceClient::IsOffTheRecordSessionActive() {
  return chrome::IsOffTheRecordSessionActive();
}

int32 ChromeMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string ChromeMetricsServiceClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return google_brand::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel ChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(chrome::VersionInfo::GetChannel());
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

void ChromeMetricsServiceClient::StartGatheringMetrics(
    const base::Closure& done_callback) {
  finished_gathering_initial_metrics_callback_ = done_callback;
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

void ChromeMetricsServiceClient::CollectFinalMetrics(
    const base::Closure& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  collect_final_metrics_done_callback_ = done_callback;

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
  details->StartFetch(MemoryDetails::FROM_CHROME_ONLY);

  // Collect WebCore cache information to put into a histogram.
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new ChromeViewMsg_GetCacheResourceStats());
  }
}

scoped_ptr<metrics::MetricsLogUploader>
ChromeMetricsServiceClient::CreateUploader(
    const base::Callback<void(int)>& on_upload_complete) {
  return scoped_ptr<metrics::MetricsLogUploader>(
      new metrics::NetMetricsLogUploader(
          g_browser_process->system_request_context(),
          metrics::kDefaultMetricsServerUrl,
          metrics::kDefaultMetricsMimeType,
          on_upload_complete));
}

base::TimeDelta ChromeMetricsServiceClient::GetStandardUploadInterval() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  if (IsCellularLogicEnabled())
    return base::TimeDelta::FromSeconds(kStandardUploadIntervalCellularSeconds);
#endif
  return base::TimeDelta::FromSeconds(kStandardUploadIntervalSeconds);
}

base::string16 ChromeMetricsServiceClient::GetRegistryBackupKey() {
#if defined(OS_WIN)
  return L"Software\\" PRODUCT_STRING_PATH L"\\StabilityMetrics";
#else
  return base::string16();
#endif
}

void ChromeMetricsServiceClient::LogPluginLoadingError(
    const base::FilePath& plugin_path) {
#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_->LogPluginLoadingError(plugin_path);
#else
  NOTREACHED();
#endif  // defined(ENABLE_PLUGINS)
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

  // Register metrics providers.
#if defined(ENABLE_EXTENSIONS)
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new ExtensionsMetricsProvider(metrics_state_manager_)));
#endif
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new metrics::NetworkMetricsProvider(
          content::BrowserThread::GetBlockingPool())));

  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new OmniboxMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new ChromeStabilityMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new metrics::GPUMetricsProvider));

  drive_metrics_provider_ = new DriveMetricsProvider;
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(drive_metrics_provider_));

  profiler_metrics_provider_ =
      new metrics::ProfilerMetricsProvider(base::Bind(&IsCellularLogicEnabled));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(profiler_metrics_provider_));

  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new metrics::CallStackProfileMetricsProvider));

#if defined(OS_ANDROID)
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new AndroidMetricsProvider(g_browser_process->local_state())));
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  google_update_metrics_provider_ = new GoogleUpdateMetricsProviderWin;
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(google_update_metrics_provider_));

  // Report exit funnels for canary and dev only.
  bool report_exit_funnels = false;
  switch (chrome::VersionInfo::GetChannel()) {
    case chrome::VersionInfo::CHANNEL_CANARY:
    case chrome::VersionInfo::CHANNEL_DEV:
      report_exit_funnels = true;
      break;
  }

  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new browser_watcher::WatcherMetricsProviderWin(
              chrome::kBrowserExitCodesRegistryPath, report_exit_funnels)));
#endif  // defined(OS_WIN)

#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_ =
      new PluginMetricsProvider(g_browser_process->local_state());
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(plugin_metrics_provider_));
#endif  // defined(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)
  ChromeOSMetricsProvider* chromeos_metrics_provider =
      new ChromeOSMetricsProvider;
  chromeos_metrics_provider_ = chromeos_metrics_provider;
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(chromeos_metrics_provider));

  SigninStatusMetricsProviderChromeOS* signin_metrics_provider_cros =
      new SigninStatusMetricsProviderChromeOS;
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(signin_metrics_provider_cros));
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          SigninStatusMetricsProvider::CreateInstance()));
#endif  // !defined(OS_CHROMEOS) && !defined(OS_IOS)

  // Clear stability metrics if it is the first time cellular upload logic
  // should apply to avoid sudden bulk uploads. It needs to be done after all
  // providers are registered.
  if (ShouldClearSavedMetrics())
    metrics_service_->ClearSavedStabilityMetrics();
}

void ChromeMetricsServiceClient::OnInitTaskGotHardwareClass() {
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
  // Start the next part of the init task: fetching performance data.  This will
  // call into |FinishedReceivingProfilerData()| when the task completes.
  metrics::TrackingSynchronizer::FetchProfilerDataAsynchronously(
      weak_ptr_factory_.GetWeakPtr());
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
  drive_metrics_provider_->GetDriveMetrics(
      finished_gathering_initial_metrics_callback_);
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
  num_async_histogram_fetches_in_progress_ = 1;
#else   // !ENABLE_PRINT_PREVIEW
  num_async_histogram_fetches_in_progress_ = 2;
  // Run requests to service and content in parallel.
  if (!ServiceProcessControl::GetInstance()->GetHistograms(callback, timeout)) {
    // Assume |num_async_histogram_fetches_in_progress_| is not changed by
    // |GetHistograms()|.
    DCHECK_EQ(num_async_histogram_fetches_in_progress_, 2);
    // Assign |num_async_histogram_fetches_in_progress_| above and decrement it
    // here to make code work even if |GetHistograms()| fired |callback|.
    --num_async_histogram_fetches_in_progress_;
  }
#endif  // !ENABLE_PRINT_PREVIEW

  // Set up the callback to task to call after we receive histograms from all
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
  registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                 content::NotificationService::AllSources());
}

void ChromeMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED:
    case chrome::NOTIFICATION_BROWSER_CLOSED:
    case chrome::NOTIFICATION_OMNIBOX_OPENED_URL:
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
