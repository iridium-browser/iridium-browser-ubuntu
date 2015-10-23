// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"

#include <Foundation/Foundation.h>

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_provider.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace {

// Match the way Chromium handles authentication errors in
// google_apis/gaia/oauth2_access_token_fetcher.cc:
GoogleServiceAuthError GetGoogleServiceAuthErrorFromNSError(
    ProfileOAuth2TokenServiceIOSProvider* provider,
    NSError* error) {
  if (!error)
    return GoogleServiceAuthError::AuthErrorNone();

  AuthenticationErrorCategory errorCategory =
      provider->GetAuthenticationErrorCategory(error);
  switch (errorCategory) {
    case kAuthenticationErrorCategoryUnknownErrors:
      // Treat all unknown error as unexpected service response errors.
      // This may be too general and may require a finer grain filtering.
      return GoogleServiceAuthError(
          GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
    case kAuthenticationErrorCategoryAuthorizationErrors:
      return GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
    case kAuthenticationErrorCategoryAuthorizationForbiddenErrors:
      // HTTP_FORBIDDEN (403) is treated as temporary error, because it may be
      // '403 Rate Limit Exceeded.' (for more details, see
      // google_apis/gaia/oauth2_access_token_fetcher.cc).
      return GoogleServiceAuthError(
          GoogleServiceAuthError::SERVICE_UNAVAILABLE);
    case kAuthenticationErrorCategoryNetworkServerErrors:
      // Just set the connection error state to FAILED.
      return GoogleServiceAuthError::FromConnectionError(
          net::URLRequestStatus::FAILED);
    case kAuthenticationErrorCategoryUserCancellationErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    case kAuthenticationErrorCategoryUnknownIdentityErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  }
}

class SSOAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  SSOAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                        ProfileOAuth2TokenServiceIOSProvider* provider,
                        const AccountTrackerService::AccountInfo& account);
  ~SSOAccessTokenFetcher() override;

  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;

  void CancelRequest() override;

  // Handles an access token response.
  void OnAccessTokenResponse(NSString* token,
                             NSDate* expiration,
                             NSError* error);

 private:
  ProfileOAuth2TokenServiceIOSProvider* provider_;  // weak
  AccountTrackerService::AccountInfo account_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<SSOAccessTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SSOAccessTokenFetcher);
};

SSOAccessTokenFetcher::SSOAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    ProfileOAuth2TokenServiceIOSProvider* provider,
    const AccountTrackerService::AccountInfo& account)
    : OAuth2AccessTokenFetcher(consumer),
      provider_(provider),
      account_(account),
      request_was_cancelled_(false),
      weak_factory_(this) {
  DCHECK(provider_);
}

SSOAccessTokenFetcher::~SSOAccessTokenFetcher() {
}

void SSOAccessTokenFetcher::Start(const std::string& client_id,
                                  const std::string& client_secret,
                                  const std::vector<std::string>& scopes) {
  std::set<std::string> scopes_set(scopes.begin(), scopes.end());
  provider_->GetAccessToken(
      account_.gaia, client_id, client_secret, scopes_set,
      base::Bind(&SSOAccessTokenFetcher::OnAccessTokenResponse,
                 weak_factory_.GetWeakPtr()));
}

void SSOAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

void SSOAccessTokenFetcher::OnAccessTokenResponse(NSString* token,
                                                  NSDate* expiration,
                                                  NSError* error) {
  if (request_was_cancelled_) {
    // Ignore the callback if the request was cancelled.
    return;
  }
  GoogleServiceAuthError auth_error =
      GetGoogleServiceAuthErrorFromNSError(provider_, error);
  if (auth_error.state() == GoogleServiceAuthError::NONE) {
    base::Time expiration_date =
        base::Time::FromDoubleT([expiration timeIntervalSince1970]);
    FireOnGetTokenSuccess(base::SysNSStringToUTF8(token), expiration_date);
  } else {
    FireOnGetTokenFailure(auth_error);
  }
}

}  // namespace

ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::AccountInfo(
    SigninErrorController* signin_error_controller,
    const std::string& account_id)
    : signin_error_controller_(signin_error_controller),
      account_id_(account_id),
      last_auth_error_(GoogleServiceAuthError::NONE) {
  DCHECK(signin_error_controller_);
  DCHECK(!account_id_.empty());
  signin_error_controller_->AddProvider(this);
}

ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::~AccountInfo() {
  signin_error_controller_->RemoveProvider(this);
}

void ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::SetLastAuthError(
    const GoogleServiceAuthError& error) {
  if (error.state() != last_auth_error_.state()) {
    last_auth_error_ = error;
    signin_error_controller_->AuthStatusChanged();
  }
}

std::string ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::GetAccountId()
    const {
  return account_id_;
}

GoogleServiceAuthError
ProfileOAuth2TokenServiceIOSDelegate::AccountInfo::GetAuthStatus() const {
  return last_auth_error_;
}

ProfileOAuth2TokenServiceIOSDelegate::ProfileOAuth2TokenServiceIOSDelegate(
    SigninClient* client,
    ProfileOAuth2TokenServiceIOSProvider* provider,
    AccountTrackerService* account_tracker_service,
    SigninErrorController* signin_error_controller)
    : client_(client),
      provider_(provider),
      account_tracker_service_(account_tracker_service),
      signin_error_controller_(signin_error_controller) {
  DCHECK(client_);
  DCHECK(provider_);
  DCHECK(account_tracker_service_);
  DCHECK(signin_error_controller_);
}

ProfileOAuth2TokenServiceIOSDelegate::~ProfileOAuth2TokenServiceIOSDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ProfileOAuth2TokenServiceIOSDelegate::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  accounts_.clear();
}

void ProfileOAuth2TokenServiceIOSDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (account_tracker_service_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    MigrateExcludedSecondaryAccountIds();
  }

  // LoadCredentials() is called iff the user is signed in to Chrome, so the
  // primary account id must not be empty.
  DCHECK(!primary_account_id.empty());

  ReloadCredentials(primary_account_id);
  FireRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadCredentials(
    const std::string& primary_account_id) {
  DCHECK(!primary_account_id.empty());
  DCHECK(primary_account_id_.empty() ||
         primary_account_id_ == primary_account_id);
  primary_account_id_ = primary_account_id;
  ReloadCredentials();
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (primary_account_id_.empty()) {
    // Avoid loading the credentials if there is no primary account id.
    return;
  }

  // Get the list of new account ids.
  std::set<std::string> excluded_account_ids = GetExcludedSecondaryAccounts();
  std::set<std::string> new_account_ids;
  for (const auto& new_account : provider_->GetAllAccounts()) {
    DCHECK(!new_account.gaia.empty());
    DCHECK(!new_account.email.empty());
    if (!IsAccountExcluded(new_account.gaia, new_account.email,
                           excluded_account_ids)) {
      // Account must to be seeded before adding an account to ensure that
      // the GAIA ID is available if any client of this token service starts
      // a fetch access token operation when it receives a
      // |OnRefreshTokenAvailable| notification.
      std::string account_id = account_tracker_service_->SeedAccountInfo(
          new_account.gaia, new_account.email);
      new_account_ids.insert(account_id);
    }
  }

  // Get the list of existing account ids.
  std::vector<std::string> old_account_ids = GetAccounts();
  std::sort(old_account_ids.begin(), old_account_ids.end());

  std::set<std::string> accounts_to_add =
      base::STLSetDifference<std::set<std::string>>(new_account_ids,
                                                    old_account_ids);
  std::set<std::string> accounts_to_remove =
      base::STLSetDifference<std::set<std::string>>(old_account_ids,
                                                    new_account_ids);
  if (accounts_to_add.empty() && accounts_to_remove.empty())
    return;

  // Remove all old accounts that do not appear in |new_accounts| and then
  // load |new_accounts|.
  ScopedBatchChange batch(this);
  for (const auto& account_to_remove : accounts_to_remove) {
    RemoveAccount(account_to_remove);
  }

  // Load all new_accounts.
  for (const auto& account_to_add : accounts_to_add) {
    AddOrUpdateAccount(account_to_add);
  }
}

void ProfileOAuth2TokenServiceIOSDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTREACHED() << "Unexpected call to UpdateCredentials when using shared "
                  "authentication.";
}

void ProfileOAuth2TokenServiceIOSDelegate::RevokeAllCredentials() {
  DCHECK(thread_checker_.CalledOnValidThread());

  ScopedBatchChange batch(this);
  AccountInfoMap toRemove = accounts_;
  for (AccountInfoMap::iterator i = toRemove.begin(); i != toRemove.end(); ++i)
    RemoveAccount(i->first);

  DCHECK_EQ(0u, accounts_.size());
  primary_account_id_.clear();
  ClearExcludedSecondaryAccounts();
}

OAuth2AccessTokenFetcher*
ProfileOAuth2TokenServiceIOSDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  AccountTrackerService::AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  return new SSOAccessTokenFetcher(consumer, provider_, account_info);
}

std::vector<std::string> ProfileOAuth2TokenServiceIOSDelegate::GetAccounts() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<std::string> account_ids;
  for (auto i = accounts_.begin(); i != accounts_.end(); ++i)
    account_ids.push_back(i->first);
  return account_ids;
}

bool ProfileOAuth2TokenServiceIOSDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return accounts_.count(account_id) > 0;
}

bool ProfileOAuth2TokenServiceIOSDelegate::RefreshTokenHasError(
    const std::string& account_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = accounts_.find(account_id);
  // TODO(rogerta): should we distinguish between transient and persistent?
  return it == accounts_.end() ? false : IsError(it->second->GetAuthStatus());
}

void ProfileOAuth2TokenServiceIOSDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.
  if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
      error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
    return;
  }

  if (accounts_.count(account_id) == 0) {
    // Nothing to update as the account has already been removed.
    return;
  }
  accounts_[account_id]->SetLastAuthError(error);
}

// Clear the authentication error state and notify all observers that a new
// refresh token is available so that they request new access tokens.
void ProfileOAuth2TokenServiceIOSDelegate::AddOrUpdateAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Account must have been seeded before attempting to add it.
  DCHECK(!account_tracker_service_->GetAccountInfo(account_id).gaia.empty());
  DCHECK(!account_tracker_service_->GetAccountInfo(account_id).email.empty());

  bool account_present = accounts_.count(account_id) > 0;
  if (account_present &&
      accounts_[account_id]->GetAuthStatus().state() ==
          GoogleServiceAuthError::NONE) {
    // No need to update the account if it is already a known account and if
    // there is no auth error.
    return;
  }

  if (!account_present) {
    accounts_[account_id].reset(
        new AccountInfo(signin_error_controller_, account_id));
  }

  UpdateAuthError(account_id, GoogleServiceAuthError::AuthErrorNone());
  FireRefreshTokenAvailable(account_id);
}

void ProfileOAuth2TokenServiceIOSDelegate::RemoveAccount(
    const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!account_id.empty());

  if (accounts_.count(account_id) > 0) {
    accounts_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

std::set<std::string>
ProfileOAuth2TokenServiceIOSDelegate::GetExcludedSecondaryAccounts() {
  const base::ListValue* excluded_secondary_accounts_pref =
      client_->GetPrefs()->GetList(
          prefs::kTokenServiceExcludedSecondaryAccounts);
  std::set<std::string> excluded_secondary_accounts;
  for (base::Value* pref_value : *excluded_secondary_accounts_pref) {
    std::string value;
    if (pref_value->GetAsString(&value))
      excluded_secondary_accounts.insert(value);
  }
  return excluded_secondary_accounts;
}

void ProfileOAuth2TokenServiceIOSDelegate::ExcludeSecondaryAccounts(
    const std::vector<std::string>& account_ids) {
  for (const auto& account_id : account_ids)
    ExcludeSecondaryAccount(account_id);
}

void ProfileOAuth2TokenServiceIOSDelegate::ExcludeSecondaryAccount(
    const std::string& account_id) {
  if (GetExcludeAllSecondaryAccounts()) {
    // Avoid excluding individual secondary accounts when all secondary
    // accounts are excluded.
    return;
  }

  DCHECK(!account_id.empty());
  ListPrefUpdate update(client_->GetPrefs(),
                        prefs::kTokenServiceExcludedSecondaryAccounts);
  base::ListValue* excluded_secondary_accounts = update.Get();
  for (base::Value* pref_value : *excluded_secondary_accounts) {
    std::string value_at_it;
    if (pref_value->GetAsString(&value_at_it) && (value_at_it == account_id)) {
      // |account_id| is already excluded.
      return;
    }
  }
  excluded_secondary_accounts->AppendString(account_id);
}

void ProfileOAuth2TokenServiceIOSDelegate::IncludeSecondaryAccount(
    const std::string& account_id) {
  if (GetExcludeAllSecondaryAccounts()) {
    // Avoid including individual secondary accounts when all secondary
    // accounts are excluded.
    return;
  }

  DCHECK_NE(account_id, primary_account_id_);
  DCHECK(!primary_account_id_.empty());

  // Excluded secondary account ids is a logical set (not a list) of accounts.
  // As the value stored in the excluded account ids preference is a list,
  // the code below removes all occurences of |account_id| from this list. This
  // ensures that |account_id| is actually included even in cases when the
  // preference value was corrupted (see bug http://crbug.com/453470 as
  // example).
  ListPrefUpdate update(client_->GetPrefs(),
                        prefs::kTokenServiceExcludedSecondaryAccounts);
  base::ListValue* excluded_secondary_accounts = update.Get();
  base::ListValue::iterator it = excluded_secondary_accounts->begin();
  while (it != excluded_secondary_accounts->end()) {
    base::Value* pref_value = *it;
    std::string value_at_it;
    if (pref_value->GetAsString(&value_at_it) && (value_at_it == account_id)) {
      it = excluded_secondary_accounts->Erase(it, nullptr);
      continue;
    }
    ++it;
  }
}

bool ProfileOAuth2TokenServiceIOSDelegate::GetExcludeAllSecondaryAccounts() {
  return client_->GetPrefs()->GetBoolean(
      prefs::kTokenServiceExcludeAllSecondaryAccounts);
}

void ProfileOAuth2TokenServiceIOSDelegate::ExcludeAllSecondaryAccounts() {
  client_->GetPrefs()->SetBoolean(
      prefs::kTokenServiceExcludeAllSecondaryAccounts, true);
}

void ProfileOAuth2TokenServiceIOSDelegate::ClearExcludedSecondaryAccounts() {
  client_->GetPrefs()->ClearPref(
      prefs::kTokenServiceExcludeAllSecondaryAccounts);
  client_->GetPrefs()->ClearPref(prefs::kTokenServiceExcludedSecondaryAccounts);
}

bool ProfileOAuth2TokenServiceIOSDelegate::IsAccountExcluded(
    const std::string& gaia,
    const std::string& email,
    const std::set<std::string>& excluded_account_ids) {
  std::string account_id =
      account_tracker_service_->PickAccountIdForAccount(gaia, email);
  if (account_id == primary_account_id_) {
    // Only secondary account ids are excluded.
    return false;
  }

  if (GetExcludeAllSecondaryAccounts())
    return true;
  return excluded_account_ids.count(account_id) > 0;
}

void ProfileOAuth2TokenServiceIOSDelegate::
    MigrateExcludedSecondaryAccountIds() {
  DCHECK_EQ(AccountTrackerService::MIGRATION_IN_PROGRESS,
            account_tracker_service_->GetMigrationState());

  // Before the account id migration, emails were used as account identifiers.
  // Thus the pref |prefs::kTokenServiceExcludedSecondaryAccounts| holds the
  // emails of the excluded secondary accounts.
  std::set<std::string> excluded_emails = GetExcludedSecondaryAccounts();
  if (excluded_emails.empty())
    return;

  std::vector<std::string> excluded_account_ids;
  for (const std::string& excluded_email : excluded_emails) {
    ProfileOAuth2TokenServiceIOSProvider::AccountInfo account_info =
        provider_->GetAccountInfoForEmail(excluded_email);
    if (account_info.gaia.empty()) {
      // The provider no longer has an account with email |excluded_email|.
      // This can occur for 2 reasons:
      // 1. The account with email |excluded_email| was removed before being
      //   migrated. It may simply be ignored in this case (no need to exclude
      //   an account that is no longer available).
      // 2. The migration of the excluded account ids was already done before,
      //   but the entire migration of the accounts did not end for whatever
      //   reason (e.g. the app crashed during the previous attempt to migrate
      //   the accounts). The entire migration should be ignored in this case.
      if (provider_->GetAccountInfoForGaia(excluded_email).gaia.empty()) {
        // Case 1 above (account was removed).
        DVLOG(1) << "Excluded secondary account with email " << excluded_email
                 << " was removed before migration.";
      } else {
        // Case 2 above (migration already done).
        DVLOG(1) << "Excluded secondary account ids were already migrated.";
        return;
      }
    } else {
      std::string excluded_account_id =
          account_tracker_service_->PickAccountIdForAccount(account_info.gaia,
                                                            account_info.email);
      excluded_account_ids.push_back(excluded_account_id);
    }
  }
  ClearExcludedSecondaryAccounts();
  ExcludeSecondaryAccounts(excluded_account_ids);
}
