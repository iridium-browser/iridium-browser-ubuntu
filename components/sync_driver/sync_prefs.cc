// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/sync_prefs.h"

#include "base/logging.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_driver/pref_names.h"

namespace sync_driver {

SyncPrefObserver::~SyncPrefObserver() {}

SyncPrefs::SyncPrefs(PrefService* pref_service) : pref_service_(pref_service) {
  DCHECK(pref_service);
  RegisterPrefGroups();
  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(
      prefs::kSyncManaged,
      pref_service_,
      base::Bind(&SyncPrefs::OnSyncManagedPrefChanged, base::Unretained(this)));
}

SyncPrefs::SyncPrefs() : pref_service_(NULL) {}

SyncPrefs::~SyncPrefs() { DCHECK(CalledOnValidThread()); }

// static
void SyncPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kSyncHasSetupCompleted, false);
  registry->RegisterBooleanPref(prefs::kSyncSuppressStart, false);
  registry->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncLastPollTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncFirstSyncTime, 0);

  // All datatypes are on by default, but this gets set explicitly
  // when you configure sync (when turning it on), in
  // ProfileSyncService::OnUserChoseDatatypes.
  registry->RegisterBooleanPref(prefs::kSyncKeepEverythingSynced, true);

  syncer::ModelTypeSet user_types = syncer::UserTypes();

  // Include proxy types as well, as they can be individually selected,
  // although they don't have sync representations.
  user_types.PutAll(syncer::ProxyTypes());

  // Treat bookmarks and device info specially.
  RegisterDataTypePreferredPref(registry, syncer::BOOKMARKS, true);
  RegisterDataTypePreferredPref(registry, syncer::DEVICE_INFO, true);
  user_types.Remove(syncer::BOOKMARKS);
  user_types.Remove(syncer::DEVICE_INFO);

  // All types are set to off by default, which forces a configuration to
  // explicitly enable them. GetPreferredTypes() will ensure that any new
  // implicit types are enabled when their pref group is, or via
  // KeepEverythingSynced.
  for (syncer::ModelTypeSet::Iterator it = user_types.First(); it.Good();
       it.Inc()) {
    RegisterDataTypePreferredPref(registry, it.Get(), false);
  }

  registry->RegisterBooleanPref(prefs::kSyncManaged, false);
  registry->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                               std::string());
  registry->RegisterStringPref(prefs::kSyncKeystoreEncryptionBootstrapToken,
                               std::string());
#if defined(OS_CHROMEOS)
  registry->RegisterStringPref(prefs::kSyncSpareBootstrapToken, "");
#endif

  registry->RegisterBooleanPref(prefs::kSyncHasAuthError, false);

  registry->RegisterStringPref(prefs::kSyncSessionsGUID, std::string());

  registry->RegisterIntegerPref(prefs::kSyncRemainingRollbackTries, 0);

  registry->RegisterBooleanPref(prefs::kSyncPassphrasePrompted, false);

  registry->RegisterIntegerPref(prefs::kSyncMemoryPressureWarningCount, -1);

  registry->RegisterBooleanPref(prefs::kSyncShutdownCleanly, false);
}

void SyncPrefs::AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK(CalledOnValidThread());
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK(CalledOnValidThread());
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

void SyncPrefs::ClearPreferences() {
  DCHECK(CalledOnValidThread());
  pref_service_->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service_->ClearPref(prefs::kSyncLastPollTime);
  pref_service_->ClearPref(prefs::kSyncHasSetupCompleted);
  pref_service_->ClearPref(prefs::kSyncEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncKeystoreEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncPassphrasePrompted);

  // TODO(nick): The current behavior does not clear
  // e.g. prefs::kSyncBookmarks.  Is that really what we want?
}

bool SyncPrefs::HasSyncSetupCompleted() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncHasSetupCompleted);
}

void SyncPrefs::SetSyncSetupCompleted() {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncHasSetupCompleted, true);
  SetStartSuppressed(false);
}

bool SyncPrefs::SyncHasAuthError() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncHasAuthError);
}

void SyncPrefs::SetSyncAuthError(bool error) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncHasAuthError, error);
}

bool SyncPrefs::IsStartSuppressed() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncSuppressStart);
}

void SyncPrefs::SetStartSuppressed(bool is_suppressed) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncSuppressStart, is_suppressed);
}

base::Time SyncPrefs::GetLastSyncedTime() const {
  DCHECK(CalledOnValidThread());
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastSyncedTime));
}

void SyncPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetInt64(prefs::kSyncLastSyncedTime, time.ToInternalValue());
}

base::Time SyncPrefs::GetLastPollTime() const {
  DCHECK(CalledOnValidThread());
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastSyncedTime));
}

void SyncPrefs::SetLastPollTime(base::Time time) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetInt64(prefs::kSyncLastPollTime, time.ToInternalValue());
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);
}

void SyncPrefs::SetKeepEverythingSynced(bool keep_everything_synced) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncKeepEverythingSynced,
                            keep_everything_synced);
}

syncer::ModelTypeSet SyncPrefs::GetPreferredDataTypes(
    syncer::ModelTypeSet registered_types) const {
  DCHECK(CalledOnValidThread());

  if (pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return registered_types;
  }

  syncer::ModelTypeSet preferred_types;
  for (syncer::ModelTypeSet::Iterator it = registered_types.First(); it.Good();
       it.Inc()) {
    if (GetDataTypePreferred(it.Get())) {
      preferred_types.Put(it.Get());
    }
  }
  return ResolvePrefGroups(registered_types, preferred_types);
}

void SyncPrefs::SetPreferredDataTypes(syncer::ModelTypeSet registered_types,
                                      syncer::ModelTypeSet preferred_types) {
  DCHECK(CalledOnValidThread());
  DCHECK(registered_types.HasAll(preferred_types));
  preferred_types = ResolvePrefGroups(registered_types, preferred_types);
  for (syncer::ModelTypeSet::Iterator i = registered_types.First(); i.Good();
       i.Inc()) {
    SetDataTypePreferred(i.Get(), preferred_types.Has(i.Get()));
  }
}

bool SyncPrefs::IsManaged() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(prefs::kSyncManaged);
}

std::string SyncPrefs::GetEncryptionBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetString(prefs::kSyncEncryptionBootstrapToken);
}

void SyncPrefs::SetEncryptionBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncEncryptionBootstrapToken, token);
}

std::string SyncPrefs::GetKeystoreEncryptionBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetString(prefs::kSyncKeystoreEncryptionBootstrapToken);
}

void SyncPrefs::SetKeystoreEncryptionBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncKeystoreEncryptionBootstrapToken, token);
}

std::string SyncPrefs::GetSyncSessionsGUID() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetString(prefs::kSyncSessionsGUID);
}

void SyncPrefs::SetSyncSessionsGUID(const std::string& guid) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncSessionsGUID, guid);
}

// static
const char* SyncPrefs::GetPrefNameForDataType(syncer::ModelType data_type) {
  switch (data_type) {
    case syncer::BOOKMARKS:
      return prefs::kSyncBookmarks;
    case syncer::PASSWORDS:
      return prefs::kSyncPasswords;
    case syncer::PREFERENCES:
      return prefs::kSyncPreferences;
    case syncer::AUTOFILL:
      return prefs::kSyncAutofill;
    case syncer::AUTOFILL_PROFILE:
      return prefs::kSyncAutofillWallet;
    case syncer::AUTOFILL_WALLET_DATA:
      return prefs::kSyncAutofillProfile;
    case syncer::THEMES:
      return prefs::kSyncThemes;
    case syncer::TYPED_URLS:
      return prefs::kSyncTypedUrls;
    case syncer::EXTENSION_SETTINGS:
      return prefs::kSyncExtensionSettings;
    case syncer::EXTENSIONS:
      return prefs::kSyncExtensions;
    case syncer::APP_LIST:
      return prefs::kSyncAppList;
    case syncer::APP_SETTINGS:
      return prefs::kSyncAppSettings;
    case syncer::APPS:
      return prefs::kSyncApps;
    case syncer::SEARCH_ENGINES:
      return prefs::kSyncSearchEngines;
    case syncer::SESSIONS:
      return prefs::kSyncSessions;
    case syncer::APP_NOTIFICATIONS:
      return prefs::kSyncAppNotifications;
    case syncer::HISTORY_DELETE_DIRECTIVES:
      return prefs::kSyncHistoryDeleteDirectives;
    case syncer::SYNCED_NOTIFICATIONS:
      return prefs::kSyncSyncedNotifications;
    case syncer::SYNCED_NOTIFICATION_APP_INFO:
      return prefs::kSyncSyncedNotificationAppInfo;
    case syncer::DICTIONARY:
      return prefs::kSyncDictionary;
    case syncer::FAVICON_IMAGES:
      return prefs::kSyncFaviconImages;
    case syncer::FAVICON_TRACKING:
      return prefs::kSyncFaviconTracking;
    case syncer::SUPERVISED_USER_SETTINGS:
      return prefs::kSyncSupervisedUserSettings;
    case syncer::PROXY_TABS:
      return prefs::kSyncTabs;
    case syncer::PRIORITY_PREFERENCES:
      return prefs::kSyncPriorityPreferences;
    case syncer::SUPERVISED_USERS:
      return prefs::kSyncSupervisedUsers;
    case syncer::ARTICLES:
      return prefs::kSyncArticles;
    case syncer::SUPERVISED_USER_SHARED_SETTINGS:
      return prefs::kSyncSupervisedUserSharedSettings;
    case syncer::SUPERVISED_USER_WHITELISTS:
      return prefs::kSyncSupervisedUserWhitelists;
    case syncer::DEVICE_INFO:
      return prefs::kSyncDeviceInfo;
    case syncer::WIFI_CREDENTIALS:
      return prefs::kSyncWifiCredentials;
    default:
      break;
  }
  NOTREACHED() << "Type is " << data_type;
  return NULL;
}

#if defined(OS_CHROMEOS)
std::string SyncPrefs::GetSpareBootstrapToken() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetString(prefs::kSyncSpareBootstrapToken);
}

void SyncPrefs::SetSpareBootstrapToken(const std::string& token) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetString(prefs::kSyncSpareBootstrapToken, token);
}
#endif

int SyncPrefs::GetRemainingRollbackTries() const {
  return pref_service_->GetInteger(prefs::kSyncRemainingRollbackTries);
}

void SyncPrefs::SetRemainingRollbackTries(int times) {
  pref_service_->SetInteger(prefs::kSyncRemainingRollbackTries, times);
}

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(SyncPrefObserver,
                    sync_pref_observers_,
                    OnSyncManagedPrefChange(*pref_sync_managed_));
}

void SyncPrefs::SetManagedForTest(bool is_managed) {
  DCHECK(CalledOnValidThread());
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
}

void SyncPrefs::RegisterPrefGroups() {
  pref_groups_[syncer::APPS].Put(syncer::APP_NOTIFICATIONS);
  pref_groups_[syncer::APPS].Put(syncer::APP_SETTINGS);
  pref_groups_[syncer::APPS].Put(syncer::APP_LIST);

  pref_groups_[syncer::AUTOFILL].Put(syncer::AUTOFILL_PROFILE);
  pref_groups_[syncer::AUTOFILL].Put(syncer::AUTOFILL_WALLET_DATA);

  pref_groups_[syncer::EXTENSIONS].Put(syncer::EXTENSION_SETTINGS);

  pref_groups_[syncer::PREFERENCES].Put(syncer::DICTIONARY);
  pref_groups_[syncer::PREFERENCES].Put(syncer::PRIORITY_PREFERENCES);
  pref_groups_[syncer::PREFERENCES].Put(syncer::SEARCH_ENGINES);

  pref_groups_[syncer::TYPED_URLS].Put(syncer::HISTORY_DELETE_DIRECTIVES);
  pref_groups_[syncer::TYPED_URLS].Put(syncer::SESSIONS);
  pref_groups_[syncer::TYPED_URLS].Put(syncer::FAVICON_IMAGES);
  pref_groups_[syncer::TYPED_URLS].Put(syncer::FAVICON_TRACKING);

  pref_groups_[syncer::PROXY_TABS].Put(syncer::SESSIONS);
  pref_groups_[syncer::PROXY_TABS].Put(syncer::FAVICON_IMAGES);
  pref_groups_[syncer::PROXY_TABS].Put(syncer::FAVICON_TRACKING);

  // TODO(zea): put favicons in the bookmarks group as well once it handles
  // those favicons.
}

// static
void SyncPrefs::RegisterDataTypePreferredPref(
    user_prefs::PrefRegistrySyncable* registry,
    syncer::ModelType type,
    bool is_preferred) {
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return;
  }
  registry->RegisterBooleanPref(pref_name, is_preferred);
}

bool SyncPrefs::GetDataTypePreferred(syncer::ModelType type) const {
  DCHECK(CalledOnValidThread());
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return false;
  }

  // Device info is always enabled.
  if (pref_name == prefs::kSyncDeviceInfo)
    return true;

  if (type == syncer::PROXY_TABS &&
      pref_service_->GetUserPrefValue(pref_name) == NULL &&
      pref_service_->IsUserModifiablePreference(pref_name)) {
    // If there is no tab sync preference yet (i.e. newly enabled type),
    // default to the session sync preference value.
    pref_name = GetPrefNameForDataType(syncer::SESSIONS);
  }

  return pref_service_->GetBoolean(pref_name);
}

void SyncPrefs::SetDataTypePreferred(syncer::ModelType type,
                                     bool is_preferred) {
  DCHECK(CalledOnValidThread());
  const char* pref_name = GetPrefNameForDataType(type);
  if (!pref_name) {
    NOTREACHED();
    return;
  }

  // Device info is always preferred.
  if (type == syncer::DEVICE_INFO)
    return;

  pref_service_->SetBoolean(pref_name, is_preferred);
}

syncer::ModelTypeSet SyncPrefs::ResolvePrefGroups(
    syncer::ModelTypeSet registered_types,
    syncer::ModelTypeSet types) const {
  DCHECK(registered_types.HasAll(types));
  syncer::ModelTypeSet types_with_groups = types;
  for (PrefGroupsMap::const_iterator i = pref_groups_.begin();
       i != pref_groups_.end();
       ++i) {
    if (types.Has(i->first))
      types_with_groups.PutAll(i->second);
  }
  types_with_groups.RetainAll(registered_types);
  return types_with_groups;
}

base::Time SyncPrefs::GetFirstSyncTime() const {
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncFirstSyncTime));
}

void SyncPrefs::SetFirstSyncTime(base::Time time) {
  pref_service_->SetInt64(prefs::kSyncFirstSyncTime, time.ToInternalValue());
}

void SyncPrefs::ClearFirstSyncTime() {
  pref_service_->ClearPref(prefs::kSyncFirstSyncTime);
}

bool SyncPrefs::IsPassphrasePrompted() const {
  return pref_service_->GetBoolean(prefs::kSyncPassphrasePrompted);
}

void SyncPrefs::SetPassphrasePrompted(bool value) {
  pref_service_->SetBoolean(prefs::kSyncPassphrasePrompted, value);
}

int SyncPrefs::GetMemoryPressureWarningCount() const {
  return pref_service_->GetInteger(prefs::kSyncMemoryPressureWarningCount);
}

void SyncPrefs::SetMemoryPressureWarningCount(int value) {
  pref_service_->SetInteger(prefs::kSyncMemoryPressureWarningCount, value);
}

bool SyncPrefs::DidSyncShutdownCleanly() const {
  return pref_service_->GetBoolean(prefs::kSyncShutdownCleanly);
}

void SyncPrefs::SetCleanShutdown(bool value) {
  pref_service_->SetBoolean(prefs::kSyncShutdownCleanly, value);
}

}  // namespace sync_driver


