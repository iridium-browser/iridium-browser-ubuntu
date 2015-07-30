// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATED_MATCH_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATED_MATCH_HELPER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/timer/timer.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace base {
struct SingleThreadedTaskRunner;
}  // namespace base

namespace password_manager {

class AffiliationService;

// Interacts with the AffiliationService on behalf of the PasswordStore.
// For each GetLogins() request, it provides the PasswordStore with a list of
// additional realms that are affiliation-based matches to the observed realm.
//
// Currently, the only supported use-case is obtaining Android applications
// affiliated with the web site containing the observed form. This is achieved
// by implementing the "proactive fetching" strategy for interacting with the
// AffiliationService (see affiliation_service.h for details), with Android
// applications playing the role of facet Y.
//
// More specifically, this class prefetches affiliation information on start-up
// for all Android applications that the PasswordStore has credentials stored
// for. Then, the actual GetLogins() can be restricted to the cache, so that
// realms of the observed web forms will never be looked up against the
// Affiliation API.
class AffiliatedMatchHelper : public PasswordStore::Observer,
                              public PasswordStoreConsumer {
 public:
  // Callback to returns the list of affiliated signon_realms (as per defined in
  // autofill::PasswordForm) to the caller.
  typedef base::Callback<void(const std::vector<std::string>&)>
      AffiliatedRealmsCallback;

  // The |password_store| must outlive |this|. Both arguments must be non-NULL,
  // except in tests which do not Initialize() the object.
  AffiliatedMatchHelper(PasswordStore* password_store,
                        scoped_ptr<AffiliationService> affiliation_service);
  ~AffiliatedMatchHelper() override;

  // Schedules deferred initialization.
  void Initialize();

  // Retrieves realms of Android applications affiliated with the realm of the
  // |observed_form| if it is web-based. Otherwise, yields the empty list. The
  // |result_callback| will be invoked in both cases, on the same thread.
  virtual void GetAffiliatedAndroidRealms(
      const autofill::PasswordForm& observed_form,
      const AffiliatedRealmsCallback& result_callback);

  // Retrieves realms of web sites affiliated with the Android application that
  // |android_form| belongs to and invokes |result_callback| on the same thread;
  // or yields the empty list if  |android_form| is not an Android credential.
  // NOTE: This will issue an on-demand network request against the Affiliation
  // API if affiliations of the Android application are not cached. However, as
  // long as the |android_form| is from the PasswordStore, this should rarely
  // happen as affiliation information for those applications are prefetched.
  virtual void GetAffiliatedWebRealms(
      const autofill::PasswordForm& android_form,
      const AffiliatedRealmsCallback& result_callback);

  // Removes cached affiliation data that is no longer needed.
  void TrimAffiliationCache();

  // Returns whether or not |form| represents an Android credential.
  static bool IsValidAndroidCredential(const autofill::PasswordForm& form);

  // Returns whether or not |form| represents a valid Web credential for the
  // purposes of affiliation-based matching.
  static bool IsValidWebCredential(const autofill::PasswordForm& form);

  // Consumes a list of |android_credentials| corresponding to applications that
  // are affiliated with the realm of |observed_form|, and transforms them so
  // as to make them fillable into |observed_form|. This can be called from any
  // thread.
  static ScopedVector<autofill::PasswordForm>
  TransformAffiliatedAndroidCredentials(
      const autofill::PasswordForm& observed_form,
      ScopedVector<autofill::PasswordForm> android_credentials);

  // Sets the task runner to be used to delay I/O heavy initialization. Should
  // be called before Initialize(). Used only for testing.
  void SetTaskRunnerUsedForWaitingForTesting(
      const scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // I/O heavy initialization on start-up will be delayed by this long.
  // This should be high enough not to exacerbate start-up I/O contention too
  // much, but also low enough that the user be able log-in shortly after
  // browser start-up into web sites using Android credentials.
  // TODO(engedy): See if we can tie this instead to some meaningful event.
  static const int64 kInitializationDelayOnStartupInSeconds = 8;

 private:
  // Indicates the time at which verifying that affiliation information has been
  // correctly prefetched for dummy Android applications takes place.
  enum class VerificationTiming { ON_STARTUP, PERIODIC };

  // Reads all autofillable credentials from the password store and starts
  // observing the store for future changes.
  void DoDeferredInitialization();

  // Called back by AffiliationService to supply the list of facets affiliated
  // with |original_facet_uri| so that a GetAffiliatedAndroidRealms() call can
  // be completed.
  void CompleteGetAffiliatedAndroidRealms(
      const FacetURI& original_facet_uri,
      const AffiliatedRealmsCallback& result_callback,
      const AffiliatedFacets& results,
      bool success);

  // Called back by AffiliationService to supply the list of facets affiliated
  // with the Android application that GetAffiliatedWebRealms() was called with,
  // so that the call can be completed.
  void CompleteGetAffiliatedWebRealms(
      const AffiliatedRealmsCallback& result_callback,
      const AffiliatedFacets& results,
      bool success);

  // Called either shortly after startup or periodically in the steady state (as
  // indicated by |timing|) to verify that affiliation information has been
  // correctly prefetched for the dummy Android applications. Will record UMA
  // histograms using a appropriate suffix based on |timing|.
  void VerifyAffiliationsForDummyFacets(VerificationTiming timing);

  // Sets up the given |timer| to call VerifyAffiliationsForDummyFacets() with
  // the specified |delay|, with the given |timing| designation.
  void ScheduleVerifyAffiliationsForDummyFacets(base::Timer* timer,
                                                base::TimeDelta delay,
                                                VerificationTiming timing);

  // Called back by the AffiliationService in response to requesting affiliation
  // information for the dummy Web facets. The |timing| indicates if the check
  // was performed shortly after startup or periodically in the steady state.
  static void OnRetrievedAffiliationResultsForDummyWebFacets(
      VerificationTiming timing,
      const AffiliatedFacets& results,
      bool success);

  // PasswordStore::Observer:
  void OnLoginsChanged(const PasswordStoreChangeList& changes) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

  PasswordStore* const password_store_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_waiting_;

  // Being the sole consumer of AffiliationService, |this| owns the service.
  scoped_ptr<AffiliationService> affiliation_service_;

  // Timers used to schedule VerifyAffiliationsPrefetchedForDummyFacets().
  base::OneShotTimer<AffiliatedMatchHelper> on_startup_verification_timer_;
  base::RepeatingTimer<AffiliatedMatchHelper> repeated_verification_timer_;

  base::WeakPtrFactory<AffiliatedMatchHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AffiliatedMatchHelper);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATED_MATCH_HELPER_H_
