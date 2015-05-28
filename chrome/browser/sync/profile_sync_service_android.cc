// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/pref_names.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "google/cacheinvalidation/types.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "jni/ProfileSyncService_jni.h"
#include "sync/internal_api/public/network_resources.h"
#include "sync/internal_api/public/read_transaction.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

// This enum contains the list of sync ModelTypes that Android can register for
// invalidations for.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.sync
enum ModelTypeSelection {
  AUTOFILL = 1 << 0,
  BOOKMARK = 1 << 1,
  PASSWORD = 1 << 2,
  SESSION = 1 << 3,
  TYPED_URL = 1 << 4,
  AUTOFILL_PROFILE = 1 << 5,
  HISTORY_DELETE_DIRECTIVE = 1 << 6,
  PROXY_TABS = 1 << 7,
  FAVICON_IMAGE = 1 << 8,
  FAVICON_TRACKING = 1 << 9,
  NIGORI = 1 << 10,
  DEVICE_INFO = 1 << 11,
  EXPERIMENTS = 1 << 12,
  SUPERVISED_USER_SETTING = 1 << 13,
  SUPERVISED_USER_WHITELIST = 1 << 14,
  AUTOFILL_WALLET = 1 << 15,
};

}  // namespace

ProfileSyncServiceAndroid::ProfileSyncServiceAndroid(JNIEnv* env, jobject obj)
    : profile_(NULL),
      sync_service_(NULL),
      weak_java_profile_sync_service_(env, obj) {
  if (g_browser_process == NULL ||
      g_browser_process->profile_manager() == NULL) {
    NOTREACHED() << "Browser process or profile manager not initialized";
    return;
  }

  profile_ = ProfileManager::GetActiveUserProfile();
  if (profile_ == NULL) {
    NOTREACHED() << "Sync Init: Profile not found.";
    return;
  }

  sync_prefs_.reset(new sync_driver::SyncPrefs(profile_->GetPrefs()));

  sync_service_ =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  DCHECK(sync_service_);
}

void ProfileSyncServiceAndroid::Init() {
  sync_service_->AddObserver(this);
}

void ProfileSyncServiceAndroid::RemoveObserver() {
  if (sync_service_->HasObserver(this)) {
    sync_service_->RemoveObserver(this);
  }
}

ProfileSyncServiceAndroid::~ProfileSyncServiceAndroid() {
  RemoveObserver();
}

void ProfileSyncServiceAndroid::OnStateChanged() {
  // Notify the java world that our sync state has changed.
  JNIEnv* env = AttachCurrentThread();
  Java_ProfileSyncService_syncStateChanged(
      env, weak_java_profile_sync_service_.get(env).obj());
}

void ProfileSyncServiceAndroid::EnableSync(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Don't need to do anything if we're already enabled.
  if (sync_prefs_->IsStartSuppressed())
    sync_service_->UnsuppressAndStart();
  else
    DVLOG(2) << "Ignoring call to EnableSync() because sync is already enabled";
}

void ProfileSyncServiceAndroid::DisableSync(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Don't need to do anything if we're already disabled.
  if (!sync_prefs_->IsStartSuppressed()) {
    sync_service_->StopAndSuppress();
  } else {
    DVLOG(2)
        << "Ignoring call to DisableSync() because sync is already disabled";
  }
}

void ProfileSyncServiceAndroid::SignInSync(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Just return if sync already has everything it needs to start up (sync
  // should start up automatically as long as it has credentials). This can
  // happen normally if (for example) the user closes and reopens the sync
  // settings window quickly during initial startup.
  if (sync_service_->IsSyncEnabledAndLoggedIn() &&
      sync_service_->IsOAuthRefreshTokenAvailable() &&
      sync_service_->HasSyncSetupCompleted()) {
    return;
  }

  // Enable sync (if we don't have credentials yet, this will enable sync but
  // will not start it up - sync will start once credentials arrive).
  sync_service_->UnsuppressAndStart();
}

void ProfileSyncServiceAndroid::SignOutSync(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  sync_service_->DisableForUser();

  // Need to clear suppress start flag manually
  sync_prefs_->SetStartSuppressed(false);
}

void ProfileSyncServiceAndroid::FlushDirectory(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->FlushDirectory();
}

ScopedJavaLocalRef<jstring> ProfileSyncServiceAndroid::QuerySyncStatusSummary(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  std::string status(sync_service_->QuerySyncStatusSummaryString());
  return ConvertUTF8ToJavaString(env, status);
}

jboolean ProfileSyncServiceAndroid::SetSyncSessionsId(
    JNIEnv* env, jobject obj, jstring tag) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  std::string machine_tag = ConvertJavaStringToUTF8(env, tag);
  sync_prefs_->SetSyncSessionsGUID(machine_tag);
  return true;
}

jint ProfileSyncServiceAndroid::GetAuthError(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->GetAuthError().state();
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingEnabled(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->EncryptEverythingEnabled();
}

jboolean ProfileSyncServiceAndroid::IsSyncInitialized(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->backend_initialized();
}

jboolean ProfileSyncServiceAndroid::IsFirstSetupInProgress(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->FirstSetupInProgress();
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingAllowed(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->EncryptEverythingAllowed();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequired(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->IsPassphraseRequired();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForDecryption(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // In case of CUSTOM_PASSPHRASE we always sync passwords. Prompt the user for
  // a passphrase if cryptographer has any pending keys.
  if (sync_service_->GetPassphraseType() == syncer::CUSTOM_PASSPHRASE) {
    return !IsCryptographerReady(env, obj);
  }
  if (sync_service_->IsPassphraseRequiredForDecryption()) {
    // Passwords datatype should never prompt for a passphrase, except when
    // user is using a custom passphrase. Do not prompt for a passphrase if
    // passwords are the only encrypted datatype. This prevents a temporary
    // notification for passphrase  when PSS has not completed configuring
    // DataTypeManager, after configuration password datatype shall be disabled.
    const syncer::ModelTypeSet encrypted_types =
        sync_service_->GetEncryptedDataTypes();
    return !encrypted_types.Equals(syncer::ModelTypeSet(syncer::PASSWORDS));
  }
  return false;
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForExternalType(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return
      sync_service_->passphrase_required_reason() == syncer::REASON_DECRYPTION;
}

jboolean ProfileSyncServiceAndroid::IsUsingSecondaryPassphrase(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->IsUsingSecondaryPassphrase();
}

jboolean ProfileSyncServiceAndroid::SetDecryptionPassphrase(
    JNIEnv* env, jobject obj, jstring passphrase) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  return sync_service_->SetDecryptionPassphrase(key);
}

void ProfileSyncServiceAndroid::SetEncryptionPassphrase(
    JNIEnv* env, jobject obj, jstring passphrase, jboolean is_gaia) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  sync_service_->SetEncryptionPassphrase(
      key,
      is_gaia ? ProfileSyncService::IMPLICIT : ProfileSyncService::EXPLICIT);
}

jboolean ProfileSyncServiceAndroid::IsCryptographerReady(JNIEnv* env, jobject) {
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  return sync_service_->IsCryptographerReady(&trans);
}

jint ProfileSyncServiceAndroid::GetPassphraseType(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->GetPassphraseType();
}

jboolean ProfileSyncServiceAndroid::HasExplicitPassphraseTime(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return !passphrase_time.is_null();
}

jlong ProfileSyncServiceAndroid::GetExplicitPassphraseTime(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return passphrase_time.ToJavaTime();
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterGooglePassphraseBodyWithDateText(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  base::string16 passphrase_time_str =
      base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
        IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyWithDateText(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  base::string16 passphrase_time_str =
      base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetCurrentSignedInAccountText(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const std::string& sync_username =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER,
          base::ASCIIToUTF16(sync_username)));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyText(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_SYNC_ENTER_PASSPHRASE_BODY));
}

jboolean ProfileSyncServiceAndroid::IsSyncKeystoreMigrationDone(
      JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  syncer::SyncStatus status;
  bool is_status_valid = sync_service_->QueryDetailedSyncStatus(&status);
  return is_status_valid && !status.keystore_migration_time.is_null();
}

jlong ProfileSyncServiceAndroid::GetActiveDataTypes(
      JNIEnv* env, jobject obj) {
  syncer::ModelTypeSet types = sync_service_->GetActiveDataTypes();
  types.PutAll(syncer::ControlTypes());
  return ModelTypeSetToSelection(types);
}

jlong ProfileSyncServiceAndroid::GetPreferredDataTypes(
      JNIEnv* env, jobject obj) {
  syncer::ModelTypeSet types = sync_service_->GetPreferredDataTypes();
  types.PutAll(syncer::ControlTypes());
  return ModelTypeSetToSelection(types);
}

void ProfileSyncServiceAndroid::SetPreferredDataTypes(
    JNIEnv* env, jobject obj,
    jboolean sync_everything,
    jlong model_type_selection) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  syncer::ModelTypeSet types;
  // Note: only user selectable types should be included here.
  if (model_type_selection & AUTOFILL)
    types.Put(syncer::AUTOFILL);
  if (model_type_selection & BOOKMARK)
    types.Put(syncer::BOOKMARKS);
  if (model_type_selection & PASSWORD)
    types.Put(syncer::PASSWORDS);
  if (model_type_selection & PROXY_TABS)
    types.Put(syncer::PROXY_TABS);
  if (model_type_selection & TYPED_URL)
    types.Put(syncer::TYPED_URLS);
  DCHECK(syncer::UserSelectableTypes().HasAll(types));
  sync_service_->OnUserChoseDatatypes(sync_everything, types);
}

void ProfileSyncServiceAndroid::SetSetupInProgress(
    JNIEnv* env, jobject obj, jboolean in_progress) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->SetSetupInProgress(in_progress);
}

void ProfileSyncServiceAndroid::SetSyncSetupCompleted(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->SetSyncSetupCompleted();
}

jboolean ProfileSyncServiceAndroid::HasSyncSetupCompleted(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->HasSyncSetupCompleted();
}

jboolean ProfileSyncServiceAndroid::IsStartSuppressed(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_prefs_->IsStartSuppressed();
}

void ProfileSyncServiceAndroid::EnableEncryptEverything(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->EnableEncryptEverything();
}

jboolean ProfileSyncServiceAndroid::HasKeepEverythingSynced(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_prefs_->HasKeepEverythingSynced();
}

jboolean ProfileSyncServiceAndroid::HasUnrecoverableError(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->HasUnrecoverableError();
}

ScopedJavaLocalRef<jstring> ProfileSyncServiceAndroid::GetAboutInfoForTest(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<base::DictionaryValue> about_info =
      sync_ui_util::ConstructAboutInformation(sync_service_);
  std::string about_info_json;
  base::JSONWriter::Write(about_info.get(), &about_info_json);

  return ConvertUTF8ToJavaString(env, about_info_json);
}

jlong ProfileSyncServiceAndroid::GetLastSyncedTimeForTest(
    JNIEnv* env, jobject obj) {
  // Use profile preferences here instead of SyncPrefs to avoid an extra
  // conversion, since SyncPrefs::GetLastSyncedTime() converts the stored value
  // to to base::Time.
  return static_cast<jlong>(
      profile_->GetPrefs()->GetInt64(sync_driver::prefs::kSyncLastSyncedTime));
}

void ProfileSyncServiceAndroid::OverrideNetworkResourcesForTest(
    JNIEnv* env,
    jobject obj,
    jlong network_resources) {
  syncer::NetworkResources* resources =
      reinterpret_cast<syncer::NetworkResources*>(network_resources);
  sync_service_->OverrideNetworkResourcesForTest(
      make_scoped_ptr<syncer::NetworkResources>(resources));
}

// static
jlong ProfileSyncServiceAndroid::ModelTypeSetToSelection(
    syncer::ModelTypeSet types) {
  jlong model_type_selection = 0;
  if (types.Has(syncer::BOOKMARKS)) {
    model_type_selection |= BOOKMARK;
  }
  if (types.Has(syncer::AUTOFILL)) {
    model_type_selection |= AUTOFILL;
  }
  if (types.Has(syncer::AUTOFILL_PROFILE)) {
    model_type_selection |= AUTOFILL_PROFILE;
  }
  if (types.Has(syncer::AUTOFILL_WALLET_DATA)) {
    model_type_selection |= AUTOFILL_WALLET;
  }
  if (types.Has(syncer::PASSWORDS)) {
    model_type_selection |= PASSWORD;
  }
  if (types.Has(syncer::TYPED_URLS)) {
    model_type_selection |= TYPED_URL;
  }
  if (types.Has(syncer::SESSIONS)) {
    model_type_selection |= SESSION;
  }
  if (types.Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
    model_type_selection |= HISTORY_DELETE_DIRECTIVE;
  }
  if (types.Has(syncer::PROXY_TABS)) {
    model_type_selection |= PROXY_TABS;
  }
  if (types.Has(syncer::FAVICON_IMAGES)) {
    model_type_selection |= FAVICON_IMAGE;
  }
  if (types.Has(syncer::FAVICON_TRACKING)) {
    model_type_selection |= FAVICON_TRACKING;
  }
  if (types.Has(syncer::DEVICE_INFO)) {
    model_type_selection |= DEVICE_INFO;
  }
  if (types.Has(syncer::NIGORI)) {
    model_type_selection |= NIGORI;
  }
  if (types.Has(syncer::EXPERIMENTS)) {
    model_type_selection |= EXPERIMENTS;
  }
  if (types.Has(syncer::SUPERVISED_USER_SETTINGS)) {
    model_type_selection |= SUPERVISED_USER_SETTING;
  }
  if (types.Has(syncer::SUPERVISED_USER_WHITELISTS)) {
    model_type_selection |= SUPERVISED_USER_WHITELIST;
  }
  return model_type_selection;
}

// static
std::string ProfileSyncServiceAndroid::ModelTypeSelectionToStringForTest(
    jlong model_type_selection) {
  ScopedJavaLocalRef<jstring> string =
      Java_ProfileSyncService_modelTypeSelectionToStringForTest(
          AttachCurrentThread(), model_type_selection);
  return ConvertJavaStringToUTF8(string);
}

// static
ProfileSyncServiceAndroid*
    ProfileSyncServiceAndroid::GetProfileSyncServiceAndroid() {
  return reinterpret_cast<ProfileSyncServiceAndroid*>(
          Java_ProfileSyncService_getProfileSyncServiceAndroid(
      AttachCurrentThread(), base::android::GetApplicationContext()));
}

static jlong Init(JNIEnv* env, jobject obj) {
  ProfileSyncServiceAndroid* profile_sync_service_android =
      new ProfileSyncServiceAndroid(env, obj);
  profile_sync_service_android->Init();
  return reinterpret_cast<intptr_t>(profile_sync_service_android);
}

// static
bool ProfileSyncServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
