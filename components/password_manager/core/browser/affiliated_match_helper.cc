// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliated_match_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_service.h"

namespace password_manager {

namespace {

// Dummy Android facet URIs for which affiliations will be fetched as part of an
// experiment to exercise the AffiliationService code in the wild, before users
// would get a chance to have real Android credentials saved.
// Note: although somewhat redundant, the URLs are listed explicitly so that
// they are easy to find in code search if someone wonders why they are fetched.
const char* kDummyAndroidFacetURIs[] = {
    "android://oEOFeXmqYvBlkpl3gJlItdIzb59KFnmFGuc1eHFQcIKpEWQuV2X4L7GYkRtdqTi_"
    "g9YvgKFAXew3rMDjeAkWVA==@com.example.one",
    "android://oEOFeXmqYvBlkpl3gJlItdIzb59KFnmFGuc1eHFQcIKpEWQuV2X4L7GYkRtdqTi_"
    "g9YvgKFAXew3rMDjeAkWVA==@com.example.two",
    "android://oEOFeXmqYvBlkpl3gJlItdIzb59KFnmFGuc1eHFQcIKpEWQuV2X4L7GYkRtdqTi_"
    "g9YvgKFAXew3rMDjeAkWVA==@com.example.twoprime",
    "android://oEOFeXmqYvBlkpl3gJlItdIzb59KFnmFGuc1eHFQcIKpEWQuV2X4L7GYkRtdqTi_"
    "g9YvgKFAXew3rMDjeAkWVA==@com.example.three",
    "android://oEOFeXmqYvBlkpl3gJlItdIzb59KFnmFGuc1eHFQcIKpEWQuV2X4L7GYkRtdqTi_"
    "g9YvgKFAXew3rMDjeAkWVA==@com.example.four",
    "android://oEOFeXmqYvBlkpl3gJlItdIzb59KFnmFGuc1eHFQcIKpEWQuV2X4L7GYkRtdqTi_"
    "g9YvgKFAXew3rMDjeAkWVA==@com.example.fourprime"};

// Dummy Web facet URIs for the same purpose. The URIs with the same numbers are
// in the same equivalence class.
const char* kDummyWebFacetURIs[] = {"https://one.example.com",
                                    "https://two.example.com",
                                    "https://three.example.com",
                                    "https://threeprime.example.com",
                                    "https://four.example.com",
                                    "https://fourprime.example.com"};

// Returns whether or not |form| represents a credential for an Android
// application, and if so, returns the |facet_uri| of that application.
bool IsAndroidApplicationCredential(const autofill::PasswordForm& form,
                                    FacetURI* facet_uri) {
  DCHECK(facet_uri);
  if (form.scheme != autofill::PasswordForm::SCHEME_HTML)
    return false;

  *facet_uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
  return facet_uri->IsValidAndroidFacetURI();
}

}  // namespace

// static
const int64 AffiliatedMatchHelper::kInitializationDelayOnStartupInSeconds;

AffiliatedMatchHelper::AffiliatedMatchHelper(
    PasswordStore* password_store,
    scoped_ptr<AffiliationService> affiliation_service)
    : password_store_(password_store),
      task_runner_for_waiting_(base::ThreadTaskRunnerHandle::Get()),
      affiliation_service_(affiliation_service.Pass()),
      weak_ptr_factory_(this) {
}

AffiliatedMatchHelper::~AffiliatedMatchHelper() {
  if (password_store_)
    password_store_->RemoveObserver(this);
}

void AffiliatedMatchHelper::Initialize() {
  DCHECK(password_store_);
  DCHECK(affiliation_service_);
  task_runner_for_waiting_->PostDelayedTask(
      FROM_HERE, base::Bind(&AffiliatedMatchHelper::DoDeferredInitialization,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kInitializationDelayOnStartupInSeconds));
}

void AffiliatedMatchHelper::GetAffiliatedAndroidRealms(
    const autofill::PasswordForm& observed_form,
    const AffiliatedRealmsCallback& result_callback) {
  if (IsValidWebCredential(observed_form)) {
    FacetURI facet_uri(
        FacetURI::FromPotentiallyInvalidSpec(observed_form.signon_realm));
    affiliation_service_->GetAffiliations(
        facet_uri, AffiliationService::StrategyOnCacheMiss::FAIL,
        base::Bind(&AffiliatedMatchHelper::CompleteGetAffiliatedAndroidRealms,
                   weak_ptr_factory_.GetWeakPtr(), facet_uri, result_callback));
  } else {
    result_callback.Run(std::vector<std::string>());
  }
}

void AffiliatedMatchHelper::GetAffiliatedWebRealms(
    const autofill::PasswordForm& android_form,
    const AffiliatedRealmsCallback& result_callback) {
  if (IsValidAndroidCredential(android_form)) {
    affiliation_service_->GetAffiliations(
        FacetURI::FromPotentiallyInvalidSpec(android_form.signon_realm),
        AffiliationService::StrategyOnCacheMiss::FETCH_OVER_NETWORK,
        base::Bind(&AffiliatedMatchHelper::CompleteGetAffiliatedWebRealms,
                   weak_ptr_factory_.GetWeakPtr(), result_callback));
  } else {
    result_callback.Run(std::vector<std::string>());
  }
}

void AffiliatedMatchHelper::TrimAffiliationCache() {
  affiliation_service_->TrimCache();
}

// static
bool AffiliatedMatchHelper::IsValidAndroidCredential(
    const autofill::PasswordForm& form) {
  return form.scheme == autofill::PasswordForm::SCHEME_HTML &&
         IsValidAndroidFacetURI(form.signon_realm);
}

// static
bool AffiliatedMatchHelper::IsValidWebCredential(
    const autofill::PasswordForm& form) {
  FacetURI facet_uri(FacetURI::FromPotentiallyInvalidSpec(form.signon_realm));
  return form.scheme == autofill::PasswordForm::SCHEME_HTML && form.ssl_valid &&
         facet_uri.IsValidWebFacetURI();
}

// static
ScopedVector<autofill::PasswordForm>
AffiliatedMatchHelper::TransformAffiliatedAndroidCredentials(
    const autofill::PasswordForm& observed_form,
    ScopedVector<autofill::PasswordForm> android_credentials) {
  for (autofill::PasswordForm* form : android_credentials) {
    DCHECK_EQ(form->scheme, autofill::PasswordForm::SCHEME_HTML);
    form->origin = observed_form.origin;
    form->original_signon_realm = form->signon_realm;
    form->signon_realm = observed_form.signon_realm;
  }
  return android_credentials.Pass();
}

void AffiliatedMatchHelper::SetTaskRunnerUsedForWaitingForTesting(
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_for_waiting_ = task_runner;
}

void AffiliatedMatchHelper::DoDeferredInitialization() {
  // Must start observing for changes at the same time as when the snapshot is
  // taken to avoid inconsistencies due to any changes taking place in-between.
  password_store_->AddObserver(this);
  password_store_->GetAutofillableLogins(this);
}

void AffiliatedMatchHelper::CompleteGetAffiliatedAndroidRealms(
    const FacetURI& original_facet_uri,
    const AffiliatedRealmsCallback& result_callback,
    const AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> affiliated_realms;
  if (success) {
    for (const FacetURI& affiliated_facet : results) {
      if (affiliated_facet != original_facet_uri &&
          affiliated_facet.IsValidAndroidFacetURI())
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.canonical_spec() + "/");
    }
  }
  result_callback.Run(affiliated_realms);
}

void AffiliatedMatchHelper::CompleteGetAffiliatedWebRealms(
    const AffiliatedRealmsCallback& result_callback,
    const AffiliatedFacets& results,
    bool success) {
  std::vector<std::string> affiliated_realms;
  if (success) {
    for (const FacetURI& affiliated_facet : results) {
      if (affiliated_facet.IsValidWebFacetURI())
        // Facet URIs have no trailing slash, whereas realms do.
        affiliated_realms.push_back(affiliated_facet.canonical_spec() + "/");
    }
  }
  result_callback.Run(affiliated_realms);
}

void AffiliatedMatchHelper::VerifyAffiliationsForDummyFacets(
    VerificationTiming timing) {
  DCHECK(affiliation_service_);
  for (const char* web_facet_uri : kDummyWebFacetURIs) {
    // If affiliation for the Android facets has successfully been prefetched,
    // then cache-restricted queries into affiliated Web facets should succeed.
    affiliation_service_->GetAffiliations(
        FacetURI::FromCanonicalSpec(web_facet_uri),
        AffiliationService::StrategyOnCacheMiss::FAIL,
        base::Bind(&OnRetrievedAffiliationResultsForDummyWebFacets, timing));
  }
}

void AffiliatedMatchHelper::ScheduleVerifyAffiliationsForDummyFacets(
    base::Timer* timer,
    base::TimeDelta delay,
    VerificationTiming timing) {
  timer->Start(
      FROM_HERE, delay,
      base::Bind(&AffiliatedMatchHelper::VerifyAffiliationsForDummyFacets,
                 base::Unretained(this), timing));
}

void AffiliatedMatchHelper::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  std::vector<FacetURI> facet_uris_to_trim;
  for (const PasswordStoreChange& change : changes) {
    FacetURI facet_uri;
    if (!IsAndroidApplicationCredential(change.form(), &facet_uri))
      continue;

    if (change.type() == PasswordStoreChange::ADD) {
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
    } else if (change.type() == PasswordStoreChange::REMOVE) {
      // Stop keeping affiliation information fresh for deleted Android logins,
      // and make a note to potentially remove any unneeded cached data later.
      facet_uris_to_trim.push_back(facet_uri);
      affiliation_service_->CancelPrefetch(facet_uri, base::Time::Max());
    }
  }

  // When the primary key for a login is updated, |changes| will contain both a
  // REMOVE and ADD change for that login. Cached affiliation data should not be
  // deleted in this case. A simple solution is to call TrimCacheForFacet()
  // always after Prefetch() calls -- the trimming logic will detect that there
  // is an active prefetch and not delete the corresponding data.
  for (const FacetURI& facet_uri : facet_uris_to_trim)
    affiliation_service_->TrimCacheForFacet(facet_uri);
}

void AffiliatedMatchHelper::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  for (autofill::PasswordForm* form : results) {
    FacetURI facet_uri;
    if (IsAndroidApplicationCredential(*form, &facet_uri))
      affiliation_service_->Prefetch(facet_uri, base::Time::Max());
  }

  // If the respective experiment is enabled, test prefetching affiliation data
  // for dummy Android facet URIs to discover potenial issues in the wild, even
  // before users would get a chance to have real Android credentials saved.
  if (password_manager::IsAffiliationRequestsForDummyFacetsEnabled(
          *base::CommandLine::ForCurrentProcess())) {
    for (const char* android_facet_uri : kDummyAndroidFacetURIs) {
      affiliation_service_->Prefetch(
          FacetURI::FromCanonicalSpec(android_facet_uri), base::Time::Max());
    }
    ScheduleVerifyAffiliationsForDummyFacets(&on_startup_verification_timer_,
                                             base::TimeDelta::FromMinutes(1),
                                             VerificationTiming::ON_STARTUP);
    ScheduleVerifyAffiliationsForDummyFacets(&repeated_verification_timer_,
                                             base::TimeDelta::FromHours(1),
                                             VerificationTiming::PERIODIC);
  }
}

// static
void AffiliatedMatchHelper::OnRetrievedAffiliationResultsForDummyWebFacets(
    VerificationTiming timing,
    const AffiliatedFacets& results,
    bool success) {
  if (timing == AffiliatedMatchHelper::VerificationTiming::ON_STARTUP) {
    UMA_HISTOGRAM_BOOLEAN(
        "PasswordManager.AffiliationDummyData.RequestSuccess.OnStartup",
        success);
    if (success) {
      UMA_HISTOGRAM_COUNTS_100(
          "PasswordManager.AffiliationDummyData.RequestResultCount.OnStartup",
          results.size());
    }
  } else if (timing == AffiliatedMatchHelper::VerificationTiming::PERIODIC) {
    UMA_HISTOGRAM_BOOLEAN(
        "PasswordManager.AffiliationDummyData.RequestSuccess.Periodic",
        success);
    if (success) {
      UMA_HISTOGRAM_COUNTS_100(
          "PasswordManager.AffiliationDummyData.RequestResultCount.Periodic",
          results.size());
    }
  } else {
    NOTREACHED();
  }
}

}  // namespace password_manager
