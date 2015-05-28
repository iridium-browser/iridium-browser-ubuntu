// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android_profile_oauth2_token_service.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "jni/OAuth2TokenService_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

// Callback from FetchOAuth2TokenWithUsername().
// Arguments:
// - the error, or NONE if the token fetch was successful.
// - the OAuth2 access token.
// - the expiry time of the token (may be null, indicating that the expiry
//   time is unknown.
typedef base::Callback<void(
    const GoogleServiceAuthError&, const std::string&, const base::Time&)>
        FetchOAuth2TokenCallback;

class AndroidAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  AndroidAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                            const std::string& account_id);
  ~AndroidAccessTokenFetcher() override;

  // Overrides from OAuth2AccessTokenFetcher:
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;
  void CancelRequest() override;

  // Handles an access token response.
  void OnAccessTokenResponse(const GoogleServiceAuthError& error,
                             const std::string& access_token,
                             const base::Time& expiration_time);

 private:
  std::string CombineScopes(const std::vector<std::string>& scopes);

  std::string account_id_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<AndroidAccessTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidAccessTokenFetcher);
};

AndroidAccessTokenFetcher::AndroidAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    const std::string& account_id)
    : OAuth2AccessTokenFetcher(consumer),
      account_id_(account_id),
      request_was_cancelled_(false),
      weak_factory_(this) {
}

AndroidAccessTokenFetcher::~AndroidAccessTokenFetcher() {}

void AndroidAccessTokenFetcher::Start(const std::string& client_id,
                                      const std::string& client_secret,
                                      const std::vector<std::string>& scopes) {
  JNIEnv* env = AttachCurrentThread();
  std::string scope = CombineScopes(scopes);
  ScopedJavaLocalRef<jstring> j_username =
      ConvertUTF8ToJavaString(env, account_id_);
  ScopedJavaLocalRef<jstring> j_scope =
      ConvertUTF8ToJavaString(env, scope);
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      new FetchOAuth2TokenCallback(
          base::Bind(&AndroidAccessTokenFetcher::OnAccessTokenResponse,
                     weak_factory_.GetWeakPtr())));

  // Call into Java to get a new token.
  Java_OAuth2TokenService_getOAuth2AuthToken(
      env, base::android::GetApplicationContext(),
      j_username.obj(),
      j_scope.obj(),
      reinterpret_cast<intptr_t>(heap_callback.release()));
}

void AndroidAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

void AndroidAccessTokenFetcher::OnAccessTokenResponse(
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration_time) {
  if (request_was_cancelled_) {
    // Ignore the callback if the request was cancelled.
    return;
  }
  if (error.state() == GoogleServiceAuthError::NONE) {
    FireOnGetTokenSuccess(access_token, expiration_time);
  } else {
    FireOnGetTokenFailure(error);
  }
}

// static
std::string AndroidAccessTokenFetcher::CombineScopes(
    const std::vector<std::string>& scopes) {
  // The Android AccountManager supports multiple scopes separated by a space:
  // https://code.google.com/p/google-api-java-client/wiki/OAuth2#Android
  std::string scope;
  for (std::vector<std::string>::const_iterator it = scopes.begin();
       it != scopes.end(); ++it) {
    if (!scope.empty())
      scope += " ";
    scope += *it;
  }
  return scope;
}

}  // namespace

bool AndroidProfileOAuth2TokenService::is_testing_profile_ = false;

AndroidProfileOAuth2TokenService::AndroidProfileOAuth2TokenService() {
  DVLOG(1) << "AndroidProfileOAuth2TokenService::ctor";
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_OAuth2TokenService_create(env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, local_java_ref.obj());
}

AndroidProfileOAuth2TokenService::~AndroidProfileOAuth2TokenService() {}

// static
jobject AndroidProfileOAuth2TokenService::GetForProfile(
    JNIEnv* env, jclass clazz, jobject j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  AndroidProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetPlatformSpecificForProfile(profile);
  return service->java_ref_.obj();
}

static jobject GetForProfile(JNIEnv* env,
                             jclass clazz,
                             jobject j_profile_android) {
  return AndroidProfileOAuth2TokenService::GetForProfile(
      env, clazz, j_profile_android);
}

void AndroidProfileOAuth2TokenService::Initialize(
    SigninClient* client,
    SigninErrorController* signin_error_controller) {
  DVLOG(1) << "AndroidProfileOAuth2TokenService::Initialize";
  ProfileOAuth2TokenService::Initialize(client, signin_error_controller);

  if (!is_testing_profile_) {
    Java_OAuth2TokenService_validateAccounts(
        AttachCurrentThread(), java_ref_.obj(),
        base::android::GetApplicationContext(), JNI_TRUE);
  }
}

bool AndroidProfileOAuth2TokenService::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_account_id =
      ConvertUTF8ToJavaString(env, account_id);
  jboolean refresh_token_is_available =
      Java_OAuth2TokenService_hasOAuth2RefreshToken(
          env, base::android::GetApplicationContext(),
          j_account_id.obj());
  return refresh_token_is_available == JNI_TRUE;
}

void AndroidProfileOAuth2TokenService::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // TODO(rogerta): do we need to update anything, or does the system handle it?
}

std::vector<std::string> AndroidProfileOAuth2TokenService::GetAccounts() {
  std::vector<std::string> accounts;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_OAuth2TokenService_getAccounts(
          env, base::android::GetApplicationContext());
  // TODO(fgorski): We may decide to filter out some of the accounts.
  base::android::AppendJavaStringArrayToStringVector(env,
                                                     j_accounts.obj(),
                                                     &accounts);
  return accounts;
}

std::vector<std::string> AndroidProfileOAuth2TokenService::GetSystemAccounts() {
  std::vector<std::string> accounts;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_OAuth2TokenService_getSystemAccounts(
          env, base::android::GetApplicationContext());
  base::android::AppendJavaStringArrayToStringVector(env,
                                                     j_accounts.obj(),
                                                     &accounts);
  return accounts;
}

OAuth2AccessTokenFetcher*
AndroidProfileOAuth2TokenService::CreateAccessTokenFetcher(
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    OAuth2AccessTokenConsumer* consumer) {
  ValidateAccountId(account_id);
  return new AndroidAccessTokenFetcher(consumer, account_id);
}

void AndroidProfileOAuth2TokenService::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  ValidateAccountId(account_id);
  OAuth2TokenService::InvalidateOAuth2Token(account_id,
                                            client_id,
                                            scopes,
                                            access_token);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_access_token =
      ConvertUTF8ToJavaString(env, access_token);
  Java_OAuth2TokenService_invalidateOAuth2AuthToken(
      env, base::android::GetApplicationContext(),
      j_access_token.obj());
}

void AndroidProfileOAuth2TokenService::ValidateAccounts(
    JNIEnv* env,
    jobject obj,
    jstring j_current_acc,
    jboolean j_force_notifications) {
  DVLOG(1) << "AndroidProfileOAuth2TokenService::ValidateAccounts from java";
  std::string signed_in_account;
  if (j_current_acc)
    signed_in_account = ConvertJavaStringToUTF8(env, j_current_acc);
  if (!signed_in_account.empty())
    signed_in_account = gaia::CanonicalizeEmail(signed_in_account);
  ValidateAccounts(signed_in_account, j_force_notifications != JNI_FALSE);
}

void AndroidProfileOAuth2TokenService::ValidateAccounts(
    const std::string& signed_in_account,
    bool force_notifications) {
  std::vector<std::string> prev_ids = GetAccounts();
  std::vector<std::string> curr_ids = GetSystemAccounts();
  std::vector<std::string> refreshed_ids;
  std::vector<std::string> revoked_ids;

  // Canonicalize system accounts.  |prev_ids| is already done.
  for (size_t i = 0; i < curr_ids.size(); ++i)
    curr_ids[i] = gaia::CanonicalizeEmail(curr_ids[i]);
  for (size_t i = 0; i < prev_ids.size(); ++i)
    ValidateAccountId(prev_ids[i]);

  DVLOG(1) << "AndroidProfileOAuth2TokenService::ValidateAccounts:"
           << " sigined_in_account=" << signed_in_account
           << " prev_ids=" << prev_ids.size()
           << " curr_ids=" << curr_ids.size()
           << " force=" << (force_notifications ? "true" : "false");

  if (!ValidateAccounts(signed_in_account, prev_ids, curr_ids, refreshed_ids,
                        revoked_ids, force_notifications)) {
    curr_ids.clear();
  }

  ScopedBatchChange batch(this);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> java_accounts(
      base::android::ToJavaArrayOfStrings(env, curr_ids));
  Java_OAuth2TokenService_saveStoredAccounts(
      env, base::android::GetApplicationContext(), java_accounts.obj());

  for (std::vector<std::string>::iterator it = refreshed_ids.begin();
       it != refreshed_ids.end(); it++) {
    FireRefreshTokenAvailable(*it);
  }

  for (std::vector<std::string>::iterator it = revoked_ids.begin();
       it != revoked_ids.end(); it++) {
    FireRefreshTokenRevoked(*it);
  }
}

bool AndroidProfileOAuth2TokenService::ValidateAccounts(
    const std::string& signed_in_account,
    const std::vector<std::string>& prev_account_ids,
    const std::vector<std::string>& curr_account_ids,
    std::vector<std::string>& refreshed_ids,
    std::vector<std::string>& revoked_ids,
    bool force_notifications) {
  if (std::find(curr_account_ids.begin(),
                curr_account_ids.end(),
                signed_in_account) != curr_account_ids.end()) {
    // Test to see if an account is removed from the Android AccountManager.
    // If so, invoke FireRefreshTokenRevoked to notify the reconcilor.
    for (std::vector<std::string>::const_iterator it = prev_account_ids.begin();
         it != prev_account_ids.end(); it++) {
      if (*it == signed_in_account)
        continue;

      if (std::find(curr_account_ids.begin(),
                    curr_account_ids.end(),
                    *it) == curr_account_ids.end()) {
        DVLOG(1) << "AndroidProfileOAuth2TokenService::ValidateAccounts:"
                 << "revoked=" << *it;
        revoked_ids.push_back(*it);
      }
    }

    if (force_notifications ||
        std::find(prev_account_ids.begin(), prev_account_ids.end(),
                  signed_in_account) == prev_account_ids.end()) {
      // Always fire the primary signed in account first.
      DVLOG(1) << "AndroidProfileOAuth2TokenService::ValidateAccounts:"
               << "refreshed=" << signed_in_account;
      refreshed_ids.push_back(signed_in_account);
    }

    for (std::vector<std::string>::const_iterator it = curr_account_ids.begin();
         it != curr_account_ids.end(); it++) {
      if (*it != signed_in_account) {
        if (force_notifications ||
            std::find(prev_account_ids.begin(),
                      prev_account_ids.end(),
                      *it) == prev_account_ids.end()) {
          DVLOG(1) << "AndroidProfileOAuth2TokenService::ValidateAccounts:"
                   << "refreshed=" << *it;
          refreshed_ids.push_back(*it);
        }
      }
    }
    return true;
  } else {
    // Currently signed in account does not any longer exist among accounts on
    // system together with all other accounts.
    if (std::find(prev_account_ids.begin(), prev_account_ids.end(),
                  signed_in_account) != prev_account_ids.end()) {
      DVLOG(1) << "AndroidProfileOAuth2TokenService::ValidateAccounts:"
               << "revoked=" << signed_in_account;
      revoked_ids.push_back(signed_in_account);
    }
    for (std::vector<std::string>::const_iterator it = prev_account_ids.begin();
         it != prev_account_ids.end(); it++) {
      if (*it == signed_in_account)
        continue;
      DVLOG(1) << "AndroidProfileOAuth2TokenService::ValidateAccounts:"
               << "revoked=" << *it;
      revoked_ids.push_back(*it);
    }
    return false;
  }
}

void AndroidProfileOAuth2TokenService::FireRefreshTokenAvailableFromJava(
    JNIEnv* env,
    jobject obj,
    const jstring account_name) {
  std::string account_id = ConvertJavaStringToUTF8(env, account_name);
  AndroidProfileOAuth2TokenService::FireRefreshTokenAvailable(account_id);
}

void AndroidProfileOAuth2TokenService::FireRefreshTokenAvailable(
    const std::string& account_id) {
  DVLOG(1) << "AndroidProfileOAuth2TokenService::FireRefreshTokenAvailable id="
           << account_id;

  // Notify native observers.
  OAuth2TokenService::FireRefreshTokenAvailable(account_id);
  // Notify Java observers.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> account_name =
      ConvertUTF8ToJavaString(env, account_id);
  Java_OAuth2TokenService_notifyRefreshTokenAvailable(
      env, java_ref_.obj(), account_name.obj());
}

void AndroidProfileOAuth2TokenService::FireRefreshTokenRevokedFromJava(
    JNIEnv* env,
    jobject obj,
    const jstring account_name) {
  std::string account_id = ConvertJavaStringToUTF8(env, account_name);
  AndroidProfileOAuth2TokenService::FireRefreshTokenRevoked(account_id);
}

void AndroidProfileOAuth2TokenService::FireRefreshTokenRevoked(
    const std::string& account_id) {
  DVLOG(1) << "AndroidProfileOAuth2TokenService::FireRefreshTokenRevoked id="
           << account_id;

  // Notify native observers.
  OAuth2TokenService::FireRefreshTokenRevoked(account_id);
  // Notify Java observers.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> account_name =
      ConvertUTF8ToJavaString(env, account_id);
  Java_OAuth2TokenService_notifyRefreshTokenRevoked(
      env, java_ref_.obj(), account_name.obj());
}

void AndroidProfileOAuth2TokenService::FireRefreshTokensLoadedFromJava(
    JNIEnv* env,
    jobject obj) {
  AndroidProfileOAuth2TokenService::FireRefreshTokensLoaded();
}

void AndroidProfileOAuth2TokenService::FireRefreshTokensLoaded() {
  DVLOG(1) << "AndroidProfileOAuth2TokenService::FireRefreshTokensLoaded";
  // Notify native observers.
  OAuth2TokenService::FireRefreshTokensLoaded();
  // Notify Java observers.
  JNIEnv* env = AttachCurrentThread();
  Java_OAuth2TokenService_notifyRefreshTokensLoaded(
      env, java_ref_.obj());
}

void AndroidProfileOAuth2TokenService::RevokeAllCredentials() {
  DVLOG(1) << "AndroidProfileOAuth2TokenService::RevokeAllCredentials";
  ScopedBatchChange batch(this);
  std::vector<std::string> accounts = GetAccounts();
  for (std::vector<std::string>::iterator it = accounts.begin();
       it != accounts.end(); it++) {
    FireRefreshTokenRevoked(*it);
  }

  // Clear everything on the Java side as well.
  std::vector<std::string> empty;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> java_accounts(
      base::android::ToJavaArrayOfStrings(env, empty));
  Java_OAuth2TokenService_saveStoredAccounts(
      env, base::android::GetApplicationContext(), java_accounts.obj());
}

// Called from Java when fetching of an OAuth2 token is finished. The
// |authToken| param is only valid when |result| is true.
void OAuth2TokenFetched(
    JNIEnv* env,
    jclass clazz,
    jstring authToken,
    jboolean result,
    jlong nativeCallback) {
  std::string token;
  if (authToken)
    token = ConvertJavaStringToUTF8(env, authToken);
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      reinterpret_cast<FetchOAuth2TokenCallback*>(nativeCallback));
  // Android does not provide enough information to know if the credentials are
  // wrong, so assume any error is transient by using CONNECTION_FAILED.
  GoogleServiceAuthError err(result ?
                             GoogleServiceAuthError::NONE :
                             GoogleServiceAuthError::CONNECTION_FAILED);
  heap_callback->Run(err, token, base::Time());
}

// static
bool AndroidProfileOAuth2TokenService::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
