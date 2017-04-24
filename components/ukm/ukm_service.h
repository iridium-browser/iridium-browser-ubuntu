// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_SERVICE_H_
#define COMPONENTS_UKM_UKM_SERVICE_H_

#include <stddef.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_reporting_scheduler.h"
#include "components/metrics/persisted_logs.h"
#include "url/gurl.h"

class PluginInfoMessageFilter;
class PrefRegistrySimple;
class PrefService;
class UkmPageLoadMetricsObserver;

namespace autofill {
class AutofillMetrics;
}  // namespace autofill

namespace metrics {
class MetricsLogUploader;
class MetricsServiceClient;
}

namespace ukm {

class UkmEntry;
class UkmEntryBuilder;
class UkmSource;

// This feature controls whether UkmService should be created.
extern const base::Feature kUkmFeature;

// The URL-Keyed Metrics (UKM) service is responsible for gathering and
// uploading reports that contain fine grained performance metrics including
// URLs for top-level navigations.
class UkmService {
 public:
  // Constructs a UkmService.
  // Calling code is responsible for ensuring that the lifetime of
  // |pref_service| is longer than the lifetime of UkmService.
  UkmService(PrefService* pref_service, metrics::MetricsServiceClient* client);
  virtual ~UkmService();

  // Get the new source ID, which is unique for the duration of a browser
  // session.
  static int32_t GetNewSourceID();

  // Update the URL on the source keyed to the given source ID. If the source
  // does not exist, it will create a new UkmSource object.
  void UpdateSourceURL(int32_t source_id, const GURL& url);

  // Initializes the UKM service.
  void Initialize();

  // Enables/disables recording control if data is allowed to be collected.
  void EnableRecording();
  void DisableRecording();

  // Enables/disables transmission of accumulated logs. Logs that have already
  // been created will remain persisted to disk.
  void EnableReporting();
  void DisableReporting();

#if defined(OS_ANDROID) || defined(OS_IOS)
  void OnAppEnterBackground();
  void OnAppEnterForeground();
#endif

  // Records any collected data into logs, and writes to disk.
  void Flush();

  // Deletes any unsent local data.
  void Purge();

  // Resets the client id stored in prefs.
  void ResetClientId();

  // Registers the specified |provider| to provide additional metrics into the
  // UKM log. Should be called during MetricsService initialization only.
  void RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider> provider);

  // Registers the names of all of the preferences used by UkmService in
  // the provided PrefRegistry.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  using AddEntryCallback = base::Callback<void(std::unique_ptr<UkmEntry>)>;

 protected:
  const std::vector<std::unique_ptr<UkmSource>>& sources_for_testing() const {
    return sources_;
  }

  const std::vector<std::unique_ptr<UkmEntry>>& entries_for_testing() const {
    return entries_;
  }

 private:
  friend autofill::AutofillMetrics;
  friend PluginInfoMessageFilter;
  friend UkmPageLoadMetricsObserver;
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, AddEntryOnlyWithNonEmptyMetrics);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, EntryBuilderAndSerialization);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest,
                           LogsUploadedOnlyWhenHavingSourcesOrEntries);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, MetricsProviderTest);
  FRIEND_TEST_ALL_PREFIXES(UkmServiceTest, PersistAndPurge);

  // Get a new UkmEntryBuilder object for the specified source ID and event,
  // which can get metrics added to.
  //
  // This API being private is intentional. Any client using UKM needs to
  // declare itself to be a friend of UkmService and go through code review
  // process.
  std::unique_ptr<UkmEntryBuilder> GetEntryBuilder(int32_t source_id,
                                                   const char* event_name);

  // Adds a new source of UKM metrics, which will be stored until periodically
  // serialized for upload, and then deleted. This method is deprecated. Please
  // use GetEntryBuilder and UpdateSourceURL above.
  void RecordSource(std::unique_ptr<UkmSource> source);

  // Starts metrics client initialization.
  void StartInitTask();

  // Called when initialization tasks are complete, to notify the scheduler
  // that it can begin calling RotateLog.
  void FinishedInitTask();

  // Periodically called by scheduler_ to advance processing of logs.
  void RotateLog();

  // Constructs a new Report from available data and stores it in
  // persisted_logs_.
  void BuildAndStoreLog();

  // Starts an upload of the next log from persisted_logs_.
  void StartScheduledUpload();

  // Called by log_uploader_ when the an upload is completed.
  void OnLogUploadComplete(int response_code);

  // Add an entry to the UkmEntry list.
  void AddEntry(std::unique_ptr<UkmEntry> entry);

  // A weak pointer to the PrefService used to read and write preferences.
  PrefService* pref_service_;

  // Whether recording new data is currently allowed.
  bool recording_enabled_;

  // The UKM client id stored in prefs.
  uint64_t client_id_;

  // The UKM session id stored in prefs.
  int32_t session_id_;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  metrics::MetricsServiceClient* const client_;

  // Registered metrics providers.
  std::vector<std::unique_ptr<metrics::MetricsProvider>> metrics_providers_;

  // Logs that have not yet been sent.
  metrics::PersistedLogs persisted_logs_;

  // The scheduler for determining when uploads should happen.
  std::unique_ptr<metrics::MetricsReportingScheduler> scheduler_;

  base::ThreadChecker thread_checker_;

  // Instance of the helper class for uploading logs.
  std::unique_ptr<metrics::MetricsLogUploader> log_uploader_;

  bool initialize_started_;
  bool initialize_complete_;
  bool log_upload_in_progress_;

  // Contains newly added sources and entries of UKM metrics which periodically
  // get serialized and cleared by BuildAndStoreLog().
  // TODO(zhenw): update sources to a map keyed by source ID.
  std::vector<std::unique_ptr<UkmSource>> sources_;
  std::vector<std::unique_ptr<UkmEntry>> entries_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as UkmService.
  base::WeakPtrFactory<UkmService> self_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UkmService);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_SERVICE_H_
