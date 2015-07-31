// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_

#include <deque>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

class GaiaAuthFetcher;
class ProfileOAuth2TokenService;
class SigninClient;

namespace net {
class CanonicalCookie;
}

class AccountReconcilor : public KeyedService,
                          public content_settings::Observer,
                          public GaiaCookieManagerService::Observer,
                          public OAuth2TokenService::Observer,
                          public SigninManagerBase::Observer {
 public:
  enum State {
      NOT_RECONCILING,
      NOT_RECONCILING_ERROR_OCCURED,
      GATHERING_INFORMATION,
      APPLYING_CHANGES
  };

  AccountReconcilor(ProfileOAuth2TokenService* token_service,
                    SigninManagerBase* signin_manager,
                    SigninClient* client,
                    GaiaCookieManagerService* cookie_manager_service);
  ~AccountReconcilor() override;

  void Initialize(bool start_reconcile_if_tokens_available);

  // Signal that the status of the new_profile_management flag has changed.
  // Pass the new status as an explicit parameter since disabling the flag
  // doesn't remove it from the CommandLine::ForCurrentProcess().
  void OnNewProfileManagementFlagChanged(bool new_flag_status);

  // KeyedService implementation.
  void Shutdown() override;

  // Determine what the reconcilor is currently doing.
  State GetState();

 private:
  bool IsRegisteredWithTokenService() const {
    return registered_with_token_service_;
  }

  const std::vector<std::pair<std::string, bool> >& GetGaiaAccountsForTesting()
      const {
    return gaia_accounts_;
  }

  friend class AccountReconcilorTest;
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, SigninManagerRegistration);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, Reauth);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, ProfileAlreadyConnected);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileCookiesDisabled);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileContentSettings);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileContentSettingsGaiaUrl);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileContentSettingsNonGaiaUrl);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileContentSettingsInvalidPattern);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, GetAccountsFromCookieSuccess);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, GetAccountsFromCookieFailure);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoop);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoopWithDots);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileNoopMultiple);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileAddToCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileRemoveFromCookie);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileAddToCookieTwice);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileBadPrimary);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest, StartReconcileOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           StartReconcileWithSessionInfoExpiredDefault);
  FRIEND_TEST_ALL_PREFIXES(AccountReconcilorTest,
                           AddAccountToCookieCompletedWithBogusAccount);

  // Register and unregister with dependent services.
  void RegisterWithSigninManager();
  void UnregisterWithSigninManager();
  void RegisterWithTokenService();
  void UnregisterWithTokenService();
  void RegisterWithCookieManagerService();
  void UnregisterWithCookieManagerService();
  void RegisterWithContentSettings();
  void UnregisterWithContentSettings();

  bool IsProfileConnected();

  // All actions with side effects.  Virtual so that they can be overridden
  // in tests.
  virtual void PerformMergeAction(const std::string& account_id);
  virtual void PerformLogoutAllAccountsAction();

  // Used during periodic reconciliation.
  void StartReconcile();
  void FinishReconcile();
  void AbortReconcile();
  void CalculateIfReconcileIsDone();
  void ScheduleStartReconcileIfChromeAccountsChanged();

  void ValidateAccountsFromTokenService();
  // Note internally that this |account_id| is added to the cookie jar.
  bool MarkAccountAsAddedToCookie(const std::string& account_id);

  // Overriden from content_settings::Observer.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      std::string resource_identifier) override;

  // Overriden from GaiaGookieManagerService::Observer.
  void OnAddAccountToCookieCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) override;
  void OnGaiaAccountsInCookieUpdated(
        const std::vector<std::pair<std::string, bool> >& accounts,
        const GoogleServiceAuthError& error) override;

  // Overriden from OAuth2TokenService::Observer.
  void OnEndBatchChanges() override;

  // Overriden from SigninManagerBase::Observer.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username,
                             const std::string& password) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  // The ProfileOAuth2TokenService associated with this reconcilor.
  ProfileOAuth2TokenService* token_service_;

  // The SigninManager associated with this reconcilor.
  SigninManagerBase* signin_manager_;

  // The SigninClient associated with this reconcilor.
  SigninClient* client_;

  // The GaiaCookieManagerService associated with this reconcilor.
  GaiaCookieManagerService* cookie_manager_service_;

  bool registered_with_token_service_;
  bool registered_with_cookie_manager_service_;
  bool registered_with_content_settings_;

  // True while the reconcilor is busy checking or managing the accounts in
  // this profile.
  bool is_reconcile_started_;
  base::Time m_reconcile_start_time_;

  // True iff this is the first time the reconcilor is executing.
  bool first_execution_;

  // True iff an error occured during the last attempt to reconcile.
  bool error_during_last_reconcile_;

  // Used during reconcile action.
  // These members are used to validate the gaia cookie.  |gaia_accounts_|
  // holds the state of google accounts in the gaia cookie.  Each element is
  // a pair that holds the email address of the account and a boolean that
  // indicates whether the account is valid or not.  The accounts in the vector
  // are ordered the in same way as the gaia cookie.
  std::vector<std::pair<std::string, bool> > gaia_accounts_;

  // Used during reconcile action.
  // These members are used to validate the tokens in OAuth2TokenService.
  std::string primary_account_;
  std::vector<std::string> chrome_accounts_;
  std::vector<std::string> add_to_cookie_;
  bool chrome_accounts_changed_;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilor);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_H_
