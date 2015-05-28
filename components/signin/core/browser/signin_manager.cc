// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_manager.h"

#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_account_id_helper.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager_cookie_helper.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

using namespace signin_internals_util;

SigninManager::SigninManager(SigninClient* client,
                             ProfileOAuth2TokenService* token_service,
                             AccountTrackerService* account_tracker_service,
                             GaiaCookieManagerService* cookie_manager_service)
    : SigninManagerBase(client),
      prohibit_signout_(false),
      type_(SIGNIN_TYPE_NONE),
      client_(client),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service),
      cookie_manager_service_(cookie_manager_service),
      signin_manager_signed_in_(false),
      user_info_fetched_by_account_tracker_(false),
      weak_pointer_factory_(this) {}

SigninManager::~SigninManager() {}

void SigninManager::InitTokenService() {
  if (token_service_ && IsAuthenticated())
    token_service_->LoadCredentials(GetAuthenticatedAccountId());
}

std::string SigninManager::SigninTypeToString(SigninManager::SigninType type) {
  switch (type) {
    case SIGNIN_TYPE_NONE:
      return "No Signin";
    case SIGNIN_TYPE_WITH_REFRESH_TOKEN:
      return "With refresh token";
  }

  NOTREACHED();
  return std::string();
}

bool SigninManager::PrepareForSignin(SigninType type,
                                     const std::string& username,
                                     const std::string& password) {
  DCHECK(possibly_invalid_username_.empty() ||
         possibly_invalid_username_ == username);
  DCHECK(!username.empty());

  if (!IsAllowedUsername(username)) {
    // Account is not allowed by admin policy.
    HandleAuthError(
        GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED));
    return false;
  }

  // This attempt is either 1) the user trying to establish initial sync, or
  // 2) trying to refresh credentials for an existing username.  If it is 2, we
  // need to try again, but take care to leave state around tracking that the
  // user has successfully signed in once before with this username, so that on
  // restart we don't think sync setup has never completed.
  ClearTransientSigninData();
  type_ = type;
  possibly_invalid_username_.assign(username);
  password_.assign(password);
  signin_manager_signed_in_ = false;
  user_info_fetched_by_account_tracker_ = false;
  NotifyDiagnosticsObservers(SIGNIN_STARTED, SigninTypeToString(type));
  return true;
}

void SigninManager::StartSignInWithRefreshToken(
    const std::string& refresh_token,
    const std::string& username,
    const std::string& password,
    const OAuthTokenFetchedCallback& callback) {
  DCHECK(!IsAuthenticated() ||
         gaia::AreEmailsSame(username, GetAuthenticatedUsername()));

  if (!PrepareForSignin(SIGNIN_TYPE_WITH_REFRESH_TOKEN, username, password))
    return;

  // Store our callback and token.
  temp_refresh_token_ = refresh_token;
  possibly_invalid_username_ = username;

  if (!callback.is_null() && !temp_refresh_token_.empty()) {
    callback.Run(temp_refresh_token_);
  } else {
    // No oauth token or callback, so just complete our pending signin.
    CompletePendingSignin();
  }
}

void SigninManager::CopyCredentialsFrom(const SigninManager& source) {
  DCHECK_NE(this, &source);
  possibly_invalid_username_ = source.possibly_invalid_username_;
  temp_refresh_token_ = source.temp_refresh_token_;
  password_ = source.password_;
}

void SigninManager::ClearTransientSigninData() {
  DCHECK(IsInitialized());

  possibly_invalid_username_.clear();
  password_.clear();
  type_ = SIGNIN_TYPE_NONE;
  temp_refresh_token_.clear();
}

void SigninManager::HandleAuthError(const GoogleServiceAuthError& error) {
  ClearTransientSigninData();

  FOR_EACH_OBSERVER(SigninManagerBase::Observer,
                    observer_list_,
                    GoogleSigninFailed(error));
}

void SigninManager::SignOut(
    signin_metrics::ProfileSignout signout_source_metric) {
  DCHECK(IsInitialized());

  signin_metrics::LogSignout(signout_source_metric);
  if (!IsAuthenticated()) {
    if (AuthInProgress()) {
      // If the user is in the process of signing in, then treat a call to
      // SignOut as a cancellation request.
      GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
      HandleAuthError(error);
    } else {
      // Clean up our transient data and exit if we aren't signed in.
      // This avoids a perf regression from clearing out the TokenDB if
      // SignOut() is invoked on startup to clean up any incomplete previous
      // signin attempts.
      ClearTransientSigninData();
    }
    return;
  }

  if (prohibit_signout_) {
    DVLOG(1) << "Ignoring attempt to sign out while signout is prohibited";
    return;
  }

  ClearTransientSigninData();

  const std::string account_id = GetAuthenticatedAccountId();
  const std::string username = GetAuthenticatedUsername();
  const base::Time signin_time =
      base::Time::FromInternalValue(
          client_->GetPrefs()->GetInt64(prefs::kSignedInTime));
  ClearAuthenticatedUsername();
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesHostedDomain);
  client_->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
  client_->GetPrefs()->ClearPref(prefs::kSignedInTime);
  client_->OnSignedOut();

  // Determine the duration the user was logged in and log that to UMA.
  if (!signin_time.is_null()) {
    base::TimeDelta signed_in_duration = base::Time::Now() - signin_time;
    UMA_HISTOGRAM_COUNTS("Signin.SignedInDurationBeforeSignout",
                         signed_in_duration.InMinutes());
  }

  // Revoke all tokens before sending signed_out notification, because there
  // may be components that don't listen for token service events when the
  // profile is not connected to an account.
  LOG(WARNING) << "Revoking refresh token on server. Reason: sign out, "
               << "IsSigninAllowed: " << IsSigninAllowed();
  token_service_->RevokeAllCredentials();

  FOR_EACH_OBSERVER(SigninManagerBase::Observer,
                    observer_list_,
                    GoogleSignedOut(account_id, username));
}

void SigninManager::Initialize(PrefService* local_state) {
  SigninManagerBase::Initialize(local_state);

  // local_state can be null during unit tests.
  if (local_state) {
    local_state_pref_registrar_.Init(local_state);
    local_state_pref_registrar_.Add(
        prefs::kGoogleServicesUsernamePattern,
        base::Bind(&SigninManager::OnGoogleServicesUsernamePatternChanged,
                   weak_pointer_factory_.GetWeakPtr()));
  }
  signin_allowed_.Init(prefs::kSigninAllowed,
                       client_->GetPrefs(),
                       base::Bind(&SigninManager::OnSigninAllowedPrefChanged,
                                  base::Unretained(this)));

  std::string user =
      client_->GetPrefs()->GetString(prefs::kGoogleServicesUsername);
  if ((!user.empty() && !IsAllowedUsername(user)) || !IsSigninAllowed()) {
    // User is signed in, but the username is invalid - the administrator must
    // have changed the policy since the last signin, so sign out the user.
    SignOut(signin_metrics::SIGNIN_PREF_CHANGED_DURING_SIGNIN);
  }

  InitTokenService();
  account_id_helper_.reset(
      new SigninAccountIdHelper(client_, token_service_, this));

  account_tracker_service_->AddObserver(this);
}

void SigninManager::Shutdown() {
  account_tracker_service_->RemoveObserver(this);
  local_state_pref_registrar_.RemoveAll();
  account_id_helper_.reset();
  SigninManagerBase::Shutdown();
}

void SigninManager::OnGoogleServicesUsernamePatternChanged() {
  if (IsAuthenticated() &&
      !IsAllowedUsername(GetAuthenticatedUsername())) {
    // Signed in user is invalid according to the current policy so sign
    // the user out.
    SignOut(signin_metrics::GOOGLE_SERVICE_NAME_PATTERN_CHANGED);
  }
}

bool SigninManager::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

void SigninManager::OnSigninAllowedPrefChanged() {
  if (!IsSigninAllowed())
    SignOut(signin_metrics::SIGNOUT_PREF_CHANGED);
}

// static
bool SigninManager::IsUsernameAllowedByPolicy(const std::string& username,
                                              const std::string& policy) {
  if (policy.empty())
    return true;

  // Patterns like "*@foo.com" are not accepted by our regex engine (since they
  // are not valid regular expressions - they should instead be ".*@foo.com").
  // For convenience, detect these patterns and insert a "." character at the
  // front.
  base::string16 pattern = base::UTF8ToUTF16(policy);
  if (pattern[0] == L'*')
    pattern.insert(pattern.begin(), L'.');

  // See if the username matches the policy-provided pattern.
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(pattern.data(), pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    LOG(ERROR) << "Invalid login regex: " << pattern << ", status: " << status;
    // If an invalid pattern is provided, then prohibit *all* logins (better to
    // break signin than to quietly allow users to sign in).
    return false;
  }
  base::string16 username16 = base::UTF8ToUTF16(username);
  icu::UnicodeString icu_input(username16.data(), username16.length());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

bool SigninManager::IsAllowedUsername(const std::string& username) const {
  const PrefService* local_state = local_state_pref_registrar_.prefs();
  if (!local_state)
    return true;  // In a unit test with no local state - all names are allowed.

  std::string pattern =
      local_state->GetString(prefs::kGoogleServicesUsernamePattern);
  return IsUsernameAllowedByPolicy(username, pattern);
}

bool SigninManager::AuthInProgress() const {
  return !possibly_invalid_username_.empty();
}

const std::string& SigninManager::GetUsernameForAuthInProgress() const {
  return possibly_invalid_username_;
}

void SigninManager::DisableOneClickSignIn(PrefService* prefs) {
  prefs->SetBoolean(prefs::kReverseAutologinEnabled, false);
}

void SigninManager::MergeSigninCredentialIntoCookieJar() {
  if (!client_->ShouldMergeSigninCredentialsIntoCookieJar())
    return;

  if (!IsAuthenticated())
    return;

  cookie_manager_service_->AddAccountToCookie(GetAuthenticatedAccountId());
}

void SigninManager::CompletePendingSignin() {
  NotifyDiagnosticsObservers(SIGNIN_COMPLETED, "Successful");

  DCHECK(!possibly_invalid_username_.empty());
  OnSignedIn(possibly_invalid_username_);

  DCHECK(!temp_refresh_token_.empty());
  DCHECK(IsAuthenticated());
  token_service_->UpdateCredentials(GetAuthenticatedAccountId(),
                                    temp_refresh_token_);
  temp_refresh_token_.clear();

  MergeSigninCredentialIntoCookieJar();
}

void SigninManager::OnExternalSigninCompleted(const std::string& username) {
  OnSignedIn(username);
}

void SigninManager::OnSignedIn(const std::string& username) {
  client_->GetPrefs()->SetInt64(prefs::kSignedInTime,
                                base::Time::Now().ToInternalValue());
  SetAuthenticatedUsername(username);
  possibly_invalid_username_.clear();
  signin_manager_signed_in_ = true;

  FOR_EACH_OBSERVER(
      SigninManagerBase::Observer,
      observer_list_,
      GoogleSigninSucceeded(GetAuthenticatedAccountId(),
                            GetAuthenticatedUsername(),
                            password_));

  client_->OnSignedIn(GetAuthenticatedAccountId(),
                      GetAuthenticatedUsername(),
                      password_);

  signin_metrics::LogSigninProfile(client_->IsFirstRun(),
                                   client_->GetInstallDate());

  DisableOneClickSignIn(client_->GetPrefs());  // Don't ever offer again.

  PostSignedIn();
}

void SigninManager::PostSignedIn() {
  if (!signin_manager_signed_in_ || !user_info_fetched_by_account_tracker_)
    return;

  client_->PostSignedIn(GetAuthenticatedAccountId(),
                        GetAuthenticatedUsername(),
                        password_);
  password_.clear();
}

void SigninManager::OnAccountUpdated(
    const AccountTrackerService::AccountInfo& info) {
  user_info_fetched_by_account_tracker_ = true;
  PostSignedIn();
}

void SigninManager::OnAccountUpdateFailed(const std::string& account_id) {
  user_info_fetched_by_account_tracker_ = true;
  PostSignedIn();
}

void SigninManager::ProhibitSignout(bool prohibit_signout) {
  prohibit_signout_ = prohibit_signout;
}

bool SigninManager::IsSignoutProhibited() const { return prohibit_signout_; }
