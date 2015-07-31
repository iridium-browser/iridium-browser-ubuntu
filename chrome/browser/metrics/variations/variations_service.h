// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SERVICE_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/variations/variations_request_scheduler.h"
#include "chrome/browser/metrics/variations/variations_seed_store.h"
#include "chrome/common/chrome_version_info.h"
#include "components/variations/variations_seed_simulator.h"
#include "components/web_resource/resource_request_allowed_notifier.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/metrics/variations/variations_registry_syncer_win.h"
#endif

class PrefService;
class PrefRegistrySimple;

namespace base {
class Version;
}

namespace metrics {
class MetricsStateManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace variations {
class VariationsSeed;
}

namespace chrome_variations {

// Used to setup field trials based on stored variations seed data, and fetch
// new seed data from the variations server.
class VariationsService
    : public net::URLFetcherDelegate,
      public web_resource::ResourceRequestAllowedNotifier::Observer {
 public:
  class Observer {
   public:
    // How critical a detected experiment change is. Whether it should be
    // handled on a "best-effort" basis or, for a more critical change, if it
    // should be given higher priority.
    enum Severity {
      BEST_EFFORT,
      CRITICAL,
    };

    // Called when the VariationsService detects that there will be significant
    // experiment changes on a restart. This notification can then be used to
    // update UI (i.e. badging an icon).
    virtual void OnExperimentChangesDetected(Severity severity) = 0;

   protected:
    virtual ~Observer() {}
  };

  ~VariationsService() override;

  // Creates field trials based on Variations Seed loaded from local prefs. If
  // there is a problem loading the seed data, all trials specified by the seed
  // may not be created.
  bool CreateTrialsFromSeed();

  // Calls FetchVariationsSeed once and repeats this periodically. See
  // implementation for details on the period. Must be called after
  // |CreateTrialsFromSeed|.
  void StartRepeatedVariationsSeedFetch();

  // Adds an observer to listen for detected experiment changes.
  void AddObserver(Observer* observer);

  // Removes a previously-added observer.
  void RemoveObserver(Observer* observer);

  // Called when the application enters foreground. This may trigger a
  // FetchVariationsSeed call.
  // TODO(rkaplow): Handle this and the similar event in metrics_service by
  // observing an 'OnAppEnterForeground' event instead of requiring the frontend
  // code to notify each service individually.
  void OnAppEnterForeground();

#if defined(OS_WIN)
  // Starts syncing Google Update Variation IDs with the registry.
  void StartGoogleUpdateRegistrySync();
#endif

  // Sets the value of the "restrict" URL param to the variations service that
  // should be used for variation seed requests. This takes precedence over any
  // value coming from policy prefs. This should be called prior to any calls
  // to |StartRepeatedVariationsSeedFetch|.
  void SetRestrictMode(const std::string& restrict_mode);

  // Exposed for testing.
  void SetCreateTrialsFromSeedCalledForTesting(bool called);

  // Returns the variations server URL, which can vary if a command-line flag is
  // set and/or the variations restrict pref is set in |local_prefs|. Declared
  // static for test purposes.
  static GURL GetVariationsServerURL(PrefService* local_prefs,
                                     const std::string& restrict_mode_override);

  // Exposed for testing.
  static std::string GetDefaultVariationsServerURLForTesting();

  // Register Variations related prefs in Local State.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Register Variations related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Factory method for creating a VariationsService. Does not take ownership of
  // |state_manager|. Caller should ensure that |state_manager| is valid for the
  // lifetime of this class.
  static scoped_ptr<VariationsService> Create(
      PrefService* local_state,
      metrics::MetricsStateManager* state_manager);

  // Set the PrefService responsible for getting policy-related preferences,
  // such as the restrict parameter.
  void set_policy_pref_service(PrefService* service) {
    DCHECK(service);
    policy_pref_service_ = service;
  }

  // Returns the invalid variations seed signature in base64 format, or an empty
  // string if the signature was valid, missing, or if signature verification is
  // disabled.
  std::string GetInvalidVariationsSeedSignature() const;

 protected:
  // Starts the fetching process once, where |OnURLFetchComplete| is called with
  // the response.
  virtual void DoActualFetch();

  // Stores the seed to prefs. Set as virtual and protected so that it can be
  // overridden by tests.
  virtual void StoreSeed(const std::string& seed_data,
                         const std::string& seed_signature,
                         const base::Time& date_fetched);

  // Creates the VariationsService with the given |local_state| prefs service
  // and |state_manager|. This instance will take ownership of |notifier|.
  // Does not take ownership of |state_manager|. Caller should ensure that
  // |state_manager| is valid for the lifetime of this class. Use the |Create|
  // factory method to create a VariationsService.
  VariationsService(web_resource::ResourceRequestAllowedNotifier* notifier,
                    PrefService* local_state,
                    metrics::MetricsStateManager* state_manager);

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, Observer);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, SeedStoredWhenOKStatus);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, SeedNotStoredWhenNonOKStatus);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest, SeedDateUpdatedOn304Status);
  FRIEND_TEST_ALL_PREFIXES(VariationsServiceTest,
                           LoadPermanentConsistencyCountry);

  // Set of different possible values to report for the
  // Variations.LoadPermanentConsistencyCountryResult histogram. This enum must
  // be kept consistent with its counterpart in histograms.xml.
  enum LoadPermanentConsistencyCountryResult {
    LOAD_COUNTRY_NO_PREF_NO_SEED = 0,
    LOAD_COUNTRY_NO_PREF_HAS_SEED,
    LOAD_COUNTRY_INVALID_PREF_NO_SEED,
    LOAD_COUNTRY_INVALID_PREF_HAS_SEED,
    LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ,
    LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ,
    LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ,
    LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ,
    LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ,
    LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ,
    LOAD_COUNTRY_MAX,
  };

  // Checks if prerequisites for fetching the Variations seed are met, and if
  // so, performs the actual fetch using |DoActualFetch|.
  void FetchVariationsSeed();

  // Notify any observers of this service based on the simulation |result|.
  void NotifyObservers(
      const variations::VariationsSeedSimulator::Result& result);

  // net::URLFetcherDelegate implementation:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // ResourceRequestAllowedNotifier::Observer implementation:
  void OnResourceRequestsAllowed() override;

  // Performs a variations seed simulation with the given |seed| and |version|
  // and logs the simulation results as histograms.
  void PerformSimulationWithVersion(scoped_ptr<variations::VariationsSeed> seed,
                                    const base::Version& version);

  // Record the time of the most recent successful fetch.
  void RecordLastFetchTime();

  // Loads the country code to use for filtering permanent consistency studies,
  // updating the stored country code if the stored value was for a different
  // Chrome version. The country used for permanent consistency studies is kept
  // consistent between Chrome upgrades in order to avoid annoying the user due
  // to experiment churn while traveling.
  std::string LoadPermanentConsistencyCountry(
      const base::Version& version,
      const variations::VariationsSeed& seed);

  // The pref service used to store persist the variations seed.
  PrefService* local_state_;

  // Used for instantiating entropy providers for variations seed simulation.
  // Weak pointer.
  metrics::MetricsStateManager* state_manager_;

  // Used to obtain policy-related preferences. Depending on the platform, will
  // either be Local State or Profile prefs.
  PrefService* policy_pref_service_;

  VariationsSeedStore seed_store_;

  // Contains the scheduler instance that handles timing for requests to the
  // server. Initially NULL and instantiated when the initial fetch is
  // requested.
  scoped_ptr<VariationsRequestScheduler> request_scheduler_;

  // Contains the current seed request. Will only have a value while a request
  // is pending, and will be reset by |OnURLFetchComplete|.
  scoped_ptr<net::URLFetcher> pending_seed_request_;

  // The value of the "restrict" URL param to the variations server that has
  // been specified via |SetRestrictMode|. If empty, the URL param will be set
  // based on policy prefs.
  std::string restrict_mode_;

  // The URL to use for querying the variations server.
  GURL variations_server_url_;

  // Tracks whether |CreateTrialsFromSeed| has been called, to ensure that
  // it gets called prior to |StartRepeatedVariationsSeedFetch|.
  bool create_trials_from_seed_called_;

  // Tracks whether the initial request to the variations server had completed.
  bool initial_request_completed_;

  // Helper class used to tell this service if it's allowed to make network
  // resource requests.
  scoped_ptr<web_resource::ResourceRequestAllowedNotifier>
      resource_request_allowed_notifier_;

  // The start time of the last seed request. This is used to measure the
  // latency of seed requests. Initially zero.
  base::TimeTicks last_request_started_time_;

  // The number of requests to the variations server that have been performed.
  int request_count_;

  // List of observers of the VariationsService.
  ObserverList<Observer> observer_list_;

#if defined(OS_WIN)
  // Helper that handles synchronizing Variations with the Registry.
  VariationsRegistrySyncer registry_syncer_;
#endif

  base::WeakPtrFactory<VariationsService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VariationsService);
};

}  // namespace chrome_variations

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_VARIATIONS_SERVICE_H_
