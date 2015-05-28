// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"

#include <vector>

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/pref_registry/pref_registry_syncable.h"

#if defined(OS_ANDROID)
#include "components/gcm_driver/gcm_driver_android.h"
#else
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/services/gcm/gcm_account_tracker.h"
#include "chrome/browser/services/gcm/gcm_desktop_utils.h"
#include "chrome/browser/signin/profile_identity_provider.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/account_tracker.h"
#include "google_apis/gaia/identity_provider.h"
#include "net/url_request/url_request_context_getter.h"
#endif

namespace gcm {

#if !defined(OS_ANDROID)
// Identity observer only has actual work to do when the user is actually signed
// in. It ensures that account tracker is taking
class GCMProfileService::IdentityObserver : public IdentityProvider::Observer {
 public:
  IdentityObserver(Profile* profile, GCMDriver* driver);
  ~IdentityObserver() override;

  // IdentityProvider::Observer:
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

 private:
  void StartAccountTracker();

  Profile* profile_;
  GCMDriver* driver_;
  scoped_ptr<IdentityProvider> identity_provider_;
  scoped_ptr<GCMAccountTracker> gcm_account_tracker_;

  // The account ID that this service is responsible for. Empty when the service
  // is not running.
  std::string account_id_;

  base::WeakPtrFactory<GCMProfileService::IdentityObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdentityObserver);
};

GCMProfileService::IdentityObserver::IdentityObserver(Profile* profile,
                                                      GCMDriver* driver)
    : profile_(profile), driver_(driver), weak_ptr_factory_(this) {
  identity_provider_.reset(new ProfileIdentityProvider(
      SigninManagerFactory::GetForProfile(profile),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      LoginUIServiceFactory::GetForProfile(profile)));
  identity_provider_->AddObserver(this);

  OnActiveAccountLogin();
  StartAccountTracker();
}

GCMProfileService::IdentityObserver::~IdentityObserver() {
  if (gcm_account_tracker_)
    gcm_account_tracker_->Shutdown();
  identity_provider_->RemoveObserver(this);
}

void GCMProfileService::IdentityObserver::OnActiveAccountLogin() {
  // This might be called multiple times when the password changes.
  const std::string account_id = identity_provider_->GetActiveAccountId();
  if (account_id == account_id_)
    return;
  account_id_ = account_id;

  // Still need to notify GCMDriver for UMA purpose.
  driver_->OnSignedIn();
}

void GCMProfileService::IdentityObserver::OnActiveAccountLogout() {
  account_id_.clear();

  // Still need to notify GCMDriver for UMA purpose.
  driver_->OnSignedOut();
}

void GCMProfileService::IdentityObserver::StartAccountTracker() {
  if (gcm_account_tracker_)
    return;

  scoped_ptr<gaia::AccountTracker> gaia_account_tracker(
      new gaia::AccountTracker(identity_provider_.get(),
                               profile_->GetRequestContext()));

  gcm_account_tracker_.reset(
      new GCMAccountTracker(gaia_account_tracker.Pass(), driver_));

  gcm_account_tracker_->Start();
}

#endif  // !defined(OS_ANDROID)

// static
bool GCMProfileService::IsGCMEnabled(Profile* profile) {
#if defined(OS_ANDROID)
  return true;
#else
  return profile->GetPrefs()->GetBoolean(gcm::prefs::kGCMChannelStatus);
#endif  // defined(OS_ANDROID)
}

#if defined(OS_ANDROID)
static GCMProfileService* debug_instance = nullptr;

GCMProfileService::GCMProfileService(Profile* profile)
    : profile_(profile) {
  CHECK(!profile->IsOffTheRecord());

  // TODO(johnme): Remove debug_instance and this logging code once
  // crbug.com/437827 is fixed.
  if (debug_instance != nullptr) {
    LOG(FATAL) << "An instance of GCMProfileService already exists!"
               << " Old profile: " << debug_instance->profile_ << " "
               << debug_instance->profile_->GetDebugName() << " "
               << debug_instance->profile_->GetProfileType() << " "
               << debug_instance->profile_->IsSupervised() << " "
               << debug_instance->profile_->IsNewProfile() << " "
               << debug_instance->profile_->GetStartTime().ToInternalValue()
               << ", new profile: " << profile << " "
               << profile->GetDebugName() << " "
               << profile->GetProfileType() << " "
               << profile->IsSupervised() << " "
               << profile->IsNewProfile() << " "
               << profile->GetStartTime().ToInternalValue();
  }
  debug_instance = this;

  driver_.reset(new GCMDriverAndroid);
}
#else
GCMProfileService::GCMProfileService(
    Profile* profile,
    scoped_ptr<GCMClientFactory> gcm_client_factory)
    : profile_(profile) {
  DCHECK(!profile->IsOffTheRecord());

  driver_ = CreateGCMDriverDesktop(
      gcm_client_factory.Pass(),
      profile_->GetPrefs(),
      profile_->GetPath().Append(chrome::kGCMStoreDirname),
      profile_->GetRequestContext());

  identity_observer_.reset(new IdentityObserver(profile, driver_.get()));
}
#endif  // defined(OS_ANDROID)

GCMProfileService::GCMProfileService()
    : profile_(NULL) {
}

GCMProfileService::~GCMProfileService() {
#if defined(OS_ANDROID)
  debug_instance = nullptr;
#endif
}

void GCMProfileService::Shutdown() {
#if !defined(OS_ANDROID)
  identity_observer_.reset();
#endif  // !defined(OS_ANDROID)
  if (driver_) {
    driver_->Shutdown();
    driver_.reset();
  }
}

void GCMProfileService::SetDriverForTesting(GCMDriver* driver) {
  driver_.reset(driver);
#if !defined(OS_ANDROID)
  if (identity_observer_)
    identity_observer_.reset(new IdentityObserver(profile_, driver));
#endif  // !defined(OS_ANDROID)
}

}  // namespace gcm
