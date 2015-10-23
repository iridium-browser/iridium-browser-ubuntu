// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"

#include <keyhi.h>

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chrome/browser/chromeos/settings/session_manager_operation.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/tpm/tpm_token_loader.h"
#include "components/ownership/owner_key_util.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/content_switches.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/signature_creator.h"

namespace em = enterprise_management;

using content::BrowserThread;
using ownership::OwnerKeyUtil;
using ownership::PrivateKey;
using ownership::PublicKey;

namespace chromeos {

namespace {

bool IsOwnerInTests(const std::string& user_id) {
  if (user_id.empty() ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType) ||
      !CrosSettings::IsInitialized()) {
    return false;
  }
  const base::Value* value = CrosSettings::Get()->GetPref(kDeviceOwner);
  if (!value || value->GetType() != base::Value::TYPE_STRING)
    return false;
  return static_cast<const base::StringValue*>(value)->GetString() == user_id;
}

void LoadPrivateKeyByPublicKey(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    scoped_refptr<PublicKey> public_key,
    const std::string& username_hash,
    const base::Callback<void(const scoped_refptr<PublicKey>& public_key,
                              const scoped_refptr<PrivateKey>& private_key)>&
        callback) {
  crypto::EnsureNSSInit();
  crypto::ScopedPK11Slot public_slot =
      crypto::GetPublicSlotForChromeOSUser(username_hash);
  crypto::ScopedPK11Slot private_slot = crypto::GetPrivateSlotForChromeOSUser(
      username_hash, base::Callback<void(crypto::ScopedPK11Slot)>());

  // If private slot is already available, this will check it. If not, we'll get
  // called again later when the TPM Token is ready, and the slot will be
  // available then. FindPrivateKeyInSlot internally checks for a null slot if
  // needbe.
  //
  // TODO(davidben): The null check should be in the caller rather than
  // internally in the OwnerKeyUtil implementation. The tests currently get a
  // null private_slot and expect the mock OwnerKeyUtil to still be called.
  scoped_refptr<PrivateKey> private_key(
      new PrivateKey(owner_key_util->FindPrivateKeyInSlot(public_key->data(),
                                                          private_slot.get())));
  if (!private_key->key()) {
    private_key = new PrivateKey(owner_key_util->FindPrivateKeyInSlot(
        public_key->data(), public_slot.get()));
  }
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(callback, public_key, private_key));
}

void LoadPrivateKey(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const std::string username_hash,
    const base::Callback<void(const scoped_refptr<PublicKey>& public_key,
                              const scoped_refptr<PrivateKey>& private_key)>&
        callback) {
  std::vector<uint8> public_key_data;
  scoped_refptr<PublicKey> public_key;
  if (!owner_key_util->ImportPublicKey(&public_key_data)) {
    scoped_refptr<PrivateKey> private_key;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback, public_key, private_key));
    return;
  }
  public_key = new PublicKey();
  public_key->data().swap(public_key_data);
  bool rv = BrowserThread::PostTask(BrowserThread::IO,
                                    FROM_HERE,
                                    base::Bind(&LoadPrivateKeyByPublicKey,
                                               owner_key_util,
                                               public_key,
                                               username_hash,
                                               callback));
  if (!rv) {
    // IO thread doesn't exists in unit tests, but it's safe to use NSS from
    // BlockingPool in unit tests.
    LoadPrivateKeyByPublicKey(
        owner_key_util, public_key, username_hash, callback);
  }
}

bool DoesPrivateKeyExistAsyncHelper(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util) {
  std::vector<uint8> public_key;
  if (!owner_key_util->ImportPublicKey(&public_key))
    return false;
  crypto::ScopedSECKEYPrivateKey key =
      crypto::FindNSSKeyFromPublicKeyInfo(public_key);
  return key && SECKEY_GetPrivateKeyType(key.get()) == rsaKey;
}

// Checks whether NSS slots with private key are mounted or
// not. Responds via |callback|.
void DoesPrivateKeyExistAsync(
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const OwnerSettingsServiceChromeOS::IsOwnerCallback& callback) {
  if (!owner_key_util.get()) {
    callback.Run(false);
    return;
  }
  scoped_refptr<base::TaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  base::PostTaskAndReplyWithResult(
      task_runner.get(),
      FROM_HERE,
      base::Bind(&DoesPrivateKeyExistAsyncHelper, owner_key_util),
      callback);
}

// Returns true if it is okay to transfer from the current mode to the new
// mode. This function should be called in SetManagementMode().
bool CheckManagementModeTransition(policy::ManagementMode current_mode,
                                   policy::ManagementMode new_mode) {
  // Mode is not changed.
  if (current_mode == new_mode)
    return true;

  switch (current_mode) {
    case policy::MANAGEMENT_MODE_LOCAL_OWNER:
      // For consumer management enrollment.
      return new_mode == policy::MANAGEMENT_MODE_CONSUMER_MANAGED;

    case policy::MANAGEMENT_MODE_ENTERPRISE_MANAGED:
      // Management mode cannot be set when it is currently ENTERPRISE_MANAGED.
      return false;

    case policy::MANAGEMENT_MODE_CONSUMER_MANAGED:
      // For consumer management unenrollment.
      return new_mode == policy::MANAGEMENT_MODE_LOCAL_OWNER;
  }

  NOTREACHED();
  return false;
}

}  // namespace

OwnerSettingsServiceChromeOS::ManagementSettings::ManagementSettings() {
}

OwnerSettingsServiceChromeOS::ManagementSettings::~ManagementSettings() {
}

OwnerSettingsServiceChromeOS::OwnerSettingsServiceChromeOS(
    DeviceSettingsService* device_settings_service,
    Profile* profile,
    const scoped_refptr<OwnerKeyUtil>& owner_key_util)
    : ownership::OwnerSettingsService(owner_key_util),
      device_settings_service_(device_settings_service),
      profile_(profile),
      waiting_for_profile_creation_(true),
      waiting_for_tpm_token_(true),
      has_pending_fixups_(false),
      has_pending_management_settings_(false),
      weak_factory_(this),
      store_settings_factory_(this) {
  if (TPMTokenLoader::IsInitialized()) {
    TPMTokenLoader::TPMTokenStatus tpm_token_status =
        TPMTokenLoader::Get()->IsTPMTokenEnabled(
            base::Bind(&OwnerSettingsServiceChromeOS::OnTPMTokenReady,
                       weak_factory_.GetWeakPtr()));
    waiting_for_tpm_token_ =
        tpm_token_status == TPMTokenLoader::TPM_TOKEN_STATUS_UNDETERMINED;
  }

  if (DBusThreadManager::IsInitialized() &&
      DBusThreadManager::Get()->GetSessionManagerClient()) {
    DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(this);
  }

  if (device_settings_service_)
    device_settings_service_->AddObserver(this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::Source<Profile>(profile_));
}

OwnerSettingsServiceChromeOS::~OwnerSettingsServiceChromeOS() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (device_settings_service_)
    device_settings_service_->RemoveObserver(this);

  if (DBusThreadManager::IsInitialized() &&
      DBusThreadManager::Get()->GetSessionManagerClient()) {
    DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(this);
  }
}

OwnerSettingsServiceChromeOS* OwnerSettingsServiceChromeOS::FromWebUI(
    content::WebUI* web_ui) {
  if (!web_ui)
    return nullptr;
  Profile* profile = Profile::FromWebUI(web_ui);
  if (!profile)
    return nullptr;
  return OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile);
}

void OwnerSettingsServiceChromeOS::OnTPMTokenReady(
    bool /* tpm_token_enabled */) {
  DCHECK(thread_checker_.CalledOnValidThread());
  waiting_for_tpm_token_ = false;

  // TPMTokenLoader initializes the TPM and NSS database which is necessary to
  // determine ownership. Force a reload once we know these are initialized.
  ReloadKeypair();
}

bool OwnerSettingsServiceChromeOS::HasPendingChanges() const {
  return !pending_changes_.empty() || tentative_settings_.get() ||
         has_pending_management_settings_ || has_pending_fixups_;
}

bool OwnerSettingsServiceChromeOS::HandlesSetting(const std::string& setting) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStubCrosSettings)) {
    return false;
  }
  return DeviceSettingsProvider::IsDeviceSetting(setting);
}

bool OwnerSettingsServiceChromeOS::Set(const std::string& setting,
                                       const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsOwner() && !IsOwnerInTests(user_id_))
    return false;

  pending_changes_.add(setting, make_scoped_ptr(value.DeepCopy()));

  em::ChromeDeviceSettingsProto settings;
  if (tentative_settings_.get()) {
    settings = *tentative_settings_;
  } else if (device_settings_service_->status() ==
                 DeviceSettingsService::STORE_SUCCESS &&
             device_settings_service_->device_settings()) {
    settings = *device_settings_service_->device_settings();
  }
  UpdateDeviceSettings(setting, value, settings);
  em::PolicyData policy_data;
  policy_data.set_username(user_id_);
  CHECK(settings.SerializeToString(policy_data.mutable_policy_value()));
  FOR_EACH_OBSERVER(OwnerSettingsService::Observer, observers_,
                    OnTentativeChangesInPolicy(policy_data));
  StorePendingChanges();
  return true;
}

bool OwnerSettingsServiceChromeOS::AppendToList(const std::string& setting,
                                                const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::Value* old_value = CrosSettings::Get()->GetPref(setting);
  if (old_value && !old_value->IsType(base::Value::TYPE_LIST))
    return false;
  scoped_ptr<base::ListValue> new_value(
      old_value ? static_cast<const base::ListValue*>(old_value)->DeepCopy()
                : new base::ListValue());
  new_value->Append(value.DeepCopy());
  return Set(setting, *new_value);
}

bool OwnerSettingsServiceChromeOS::RemoveFromList(const std::string& setting,
                                                  const base::Value& value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::Value* old_value = CrosSettings::Get()->GetPref(setting);
  if (old_value && !old_value->IsType(base::Value::TYPE_LIST))
    return false;
  scoped_ptr<base::ListValue> new_value(
      old_value ? static_cast<const base::ListValue*>(old_value)->DeepCopy()
                : new base::ListValue());
  new_value->Remove(value, nullptr);
  return Set(setting, *new_value);
}

bool OwnerSettingsServiceChromeOS::CommitTentativeDeviceSettings(
    scoped_ptr<enterprise_management::PolicyData> policy) {
  if (!IsOwner() && !IsOwnerInTests(user_id_))
    return false;
  if (policy->username() != user_id_) {
    LOG(ERROR) << "Username mismatch: " << policy->username() << " vs. "
               << user_id_;
    return false;
  }
  tentative_settings_.reset(new em::ChromeDeviceSettingsProto);
  CHECK(tentative_settings_->ParseFromString(policy->policy_value()));
  StorePendingChanges();
  return true;
}

void OwnerSettingsServiceChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (type != chrome::NOTIFICATION_PROFILE_CREATED) {
    NOTREACHED();
    return;
  }

  Profile* profile = content::Source<Profile>(source).ptr();
  if (profile != profile_) {
    NOTREACHED();
    return;
  }

  waiting_for_profile_creation_ = false;
  ReloadKeypair();
}

void OwnerSettingsServiceChromeOS::OwnerKeySet(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (success)
    ReloadKeypair();
}

void OwnerSettingsServiceChromeOS::OwnershipStatusChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StorePendingChanges();
}

void OwnerSettingsServiceChromeOS::DeviceSettingsUpdated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StorePendingChanges();
}

void OwnerSettingsServiceChromeOS::OnDeviceSettingsServiceShutdown() {
  device_settings_service_ = nullptr;
}

void OwnerSettingsServiceChromeOS::SetManagementSettings(
    const ManagementSettings& settings,
    const OnManagementSettingsSetCallback& callback) {
  if ((!IsOwner() && !IsOwnerInTests(user_id_))) {
    if (!callback.is_null())
      callback.Run(false /* success */);
    return;
  }

  policy::ManagementMode current_mode = policy::MANAGEMENT_MODE_LOCAL_OWNER;
  if (has_pending_management_settings_) {
    current_mode = pending_management_settings_.management_mode;
  } else if (device_settings_service_ &&
             device_settings_service_->policy_data()) {
    current_mode =
        policy::GetManagementMode(*device_settings_service_->policy_data());
  }

  if (!CheckManagementModeTransition(current_mode, settings.management_mode)) {
    LOG(ERROR) << "Invalid management mode transition: current mode = "
               << current_mode << ", new mode = " << settings.management_mode;
    if (!callback.is_null())
      callback.Run(false /* success */);
    return;
  }

  pending_management_settings_ = settings;
  has_pending_management_settings_ = true;
  pending_management_settings_callbacks_.push_back(callback);
  StorePendingChanges();
}

// static
void OwnerSettingsServiceChromeOS::IsOwnerForSafeModeAsync(
    const std::string& user_hash,
    const scoped_refptr<OwnerKeyUtil>& owner_key_util,
    const IsOwnerCallback& callback) {
  CHECK(chromeos::LoginState::Get()->IsInSafeMode());

  // Make sure NSS is initialized and NSS DB is loaded for the user before
  // searching for the owner key.
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&crypto::InitializeNSSForChromeOSUser),
                 user_hash,
                 ProfileHelper::GetProfilePathByUserIdHash(user_hash)),
      base::Bind(&DoesPrivateKeyExistAsync, owner_key_util, callback));
}

// static
scoped_ptr<em::PolicyData> OwnerSettingsServiceChromeOS::AssemblePolicy(
    const std::string& user_id,
    const em::PolicyData* policy_data,
    bool apply_pending_management_settings,
    const ManagementSettings& pending_management_settings,
    em::ChromeDeviceSettingsProto* settings) {
  scoped_ptr<em::PolicyData> policy(new em::PolicyData());
  if (policy_data) {
    // Preserve management settings.
    if (policy_data->has_management_mode())
      policy->set_management_mode(policy_data->management_mode());
    if (policy_data->has_request_token())
      policy->set_request_token(policy_data->request_token());
    if (policy_data->has_device_id())
      policy->set_device_id(policy_data->device_id());
  } else {
    // If there's no previous policy data, this is the first time the device
    // setting is set. We set the management mode to LOCAL_OWNER initially.
    policy->set_management_mode(em::PolicyData::LOCAL_OWNER);
  }
  if (apply_pending_management_settings) {
    policy::SetManagementMode(*policy,
                              pending_management_settings.management_mode);

    if (pending_management_settings.request_token.empty())
      policy->clear_request_token();
    else
      policy->set_request_token(pending_management_settings.request_token);

    if (pending_management_settings.device_id.empty())
      policy->clear_device_id();
    else
      policy->set_device_id(pending_management_settings.device_id);
  }
  policy->set_policy_type(policy::dm_protocol::kChromeDevicePolicyType);
  policy->set_timestamp(
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
  policy->set_username(user_id);
  if (policy_data->management_mode() == em::PolicyData::LOCAL_OWNER ||
      policy_data->management_mode() == em::PolicyData::CONSUMER_MANAGED) {
    FixupLocalOwnerPolicy(user_id, settings);
  }
  if (!settings->SerializeToString(policy->mutable_policy_value()))
    return scoped_ptr<em::PolicyData>();

  return policy.Pass();
}

// static
void OwnerSettingsServiceChromeOS::FixupLocalOwnerPolicy(
    const std::string& user_id,
    enterprise_management::ChromeDeviceSettingsProto* settings) {
  if (!settings->has_allow_new_users())
    settings->mutable_allow_new_users()->set_allow_new_users(true);

  em::UserWhitelistProto* whitelist_proto = settings->mutable_user_whitelist();
  if (whitelist_proto->user_whitelist().end() ==
      std::find(whitelist_proto->user_whitelist().begin(),
                whitelist_proto->user_whitelist().end(), user_id)) {
    whitelist_proto->add_user_whitelist(user_id);
  }
}

// static
void OwnerSettingsServiceChromeOS::UpdateDeviceSettings(
    const std::string& path,
    const base::Value& value,
    enterprise_management::ChromeDeviceSettingsProto& settings) {
  if (path == kAccountsPrefAllowNewUser) {
    em::AllowNewUsersProto* allow = settings.mutable_allow_new_users();
    bool allow_value;
    if (value.GetAsBoolean(&allow_value)) {
      allow->set_allow_new_users(allow_value);
    } else {
      NOTREACHED();
    }
  } else if (path == kAccountsPrefAllowGuest) {
    em::GuestModeEnabledProto* guest = settings.mutable_guest_mode_enabled();
    bool guest_value;
    if (value.GetAsBoolean(&guest_value))
      guest->set_guest_mode_enabled(guest_value);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefSupervisedUsersEnabled) {
    em::SupervisedUsersSettingsProto* supervised =
        settings.mutable_supervised_users_settings();
    bool supervised_value;
    if (value.GetAsBoolean(&supervised_value))
      supervised->set_supervised_users_enabled(supervised_value);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefShowUserNamesOnSignIn) {
    em::ShowUserNamesOnSigninProto* show = settings.mutable_show_user_names();
    bool show_value;
    if (value.GetAsBoolean(&show_value))
      show->set_show_user_names(show_value);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccounts) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    device_local_accounts->clear_account();
    const base::ListValue* accounts_list = NULL;
    if (value.GetAsList(&accounts_list)) {
      for (base::ListValue::const_iterator entry(accounts_list->begin());
           entry != accounts_list->end();
           ++entry) {
        const base::DictionaryValue* entry_dict = NULL;
        if ((*entry)->GetAsDictionary(&entry_dict)) {
          em::DeviceLocalAccountInfoProto* account =
              device_local_accounts->add_account();
          std::string account_id;
          if (entry_dict->GetStringWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyId, &account_id)) {
            account->set_account_id(account_id);
          }
          int type;
          if (entry_dict->GetIntegerWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyType, &type)) {
            account->set_type(
                static_cast<em::DeviceLocalAccountInfoProto::AccountType>(
                    type));
          }
          std::string kiosk_app_id;
          if (entry_dict->GetStringWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                  &kiosk_app_id)) {
            account->mutable_kiosk_app()->set_app_id(kiosk_app_id);
          }
          std::string kiosk_app_update_url;
          if (entry_dict->GetStringWithoutPathExpansion(
                  kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL,
                  &kiosk_app_update_url)) {
            account->mutable_kiosk_app()->set_update_url(kiosk_app_update_url);
          }
        } else {
          NOTREACHED();
        }
      }
    } else {
      NOTREACHED();
    }
  } else if (path == kAccountsPrefDeviceLocalAccountAutoLoginId) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    std::string id;
    if (value.GetAsString(&id))
      device_local_accounts->set_auto_login_id(id);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccountAutoLoginDelay) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    int delay;
    if (value.GetAsInteger(&delay))
      device_local_accounts->set_auto_login_delay(delay);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    bool enabled;
    if (value.GetAsBoolean(&enabled))
      device_local_accounts->set_enable_auto_login_bailout(enabled);
    else
      NOTREACHED();
  } else if (path ==
             kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline) {
    em::DeviceLocalAccountsProto* device_local_accounts =
        settings.mutable_device_local_accounts();
    bool should_prompt;
    if (value.GetAsBoolean(&should_prompt))
      device_local_accounts->set_prompt_for_network_when_offline(should_prompt);
    else
      NOTREACHED();
  } else if (path == kSignedDataRoamingEnabled) {
    em::DataRoamingEnabledProto* roam = settings.mutable_data_roaming_enabled();
    bool roaming_value = false;
    if (value.GetAsBoolean(&roaming_value))
      roam->set_data_roaming_enabled(roaming_value);
    else
      NOTREACHED();
  } else if (path == kReleaseChannel) {
    em::ReleaseChannelProto* release_channel =
        settings.mutable_release_channel();
    std::string channel_value;
    if (value.GetAsString(&channel_value))
      release_channel->set_release_channel(channel_value);
    else
      NOTREACHED();
  } else if (path == kStatsReportingPref) {
    em::MetricsEnabledProto* metrics = settings.mutable_metrics_enabled();
    bool metrics_value = false;
    if (value.GetAsBoolean(&metrics_value))
      metrics->set_metrics_enabled(metrics_value);
    else
      NOTREACHED();
  } else if (path == kAccountsPrefUsers) {
    em::UserWhitelistProto* whitelist_proto = settings.mutable_user_whitelist();
    whitelist_proto->clear_user_whitelist();
    const base::ListValue* users;
    if (value.GetAsList(&users)) {
      for (base::ListValue::const_iterator i = users->begin();
           i != users->end();
           ++i) {
        std::string email;
        if ((*i)->GetAsString(&email))
          whitelist_proto->add_user_whitelist(email);
      }
    }
  } else if (path == kAccountsPrefEphemeralUsersEnabled) {
    em::EphemeralUsersEnabledProto* ephemeral_users_enabled =
        settings.mutable_ephemeral_users_enabled();
    bool ephemeral_users_enabled_value = false;
    if (value.GetAsBoolean(&ephemeral_users_enabled_value)) {
      ephemeral_users_enabled->set_ephemeral_users_enabled(
          ephemeral_users_enabled_value);
    } else {
      NOTREACHED();
    }
  } else if (path == kAllowRedeemChromeOsRegistrationOffers) {
    em::AllowRedeemChromeOsRegistrationOffersProto* allow_redeem_offers =
        settings.mutable_allow_redeem_offers();
    bool allow_redeem_offers_value;
    if (value.GetAsBoolean(&allow_redeem_offers_value)) {
      allow_redeem_offers->set_allow_redeem_offers(allow_redeem_offers_value);
    } else {
      NOTREACHED();
    }
  } else if (path == kStartUpFlags) {
    em::StartUpFlagsProto* flags_proto = settings.mutable_start_up_flags();
    flags_proto->Clear();
    const base::ListValue* flags;
    if (value.GetAsList(&flags)) {
      for (base::ListValue::const_iterator i = flags->begin();
           i != flags->end();
           ++i) {
        std::string flag;
        if ((*i)->GetAsString(&flag))
          flags_proto->add_flags(flag);
      }
    }
  } else if (path == kSystemUse24HourClock) {
    em::SystemUse24HourClockProto* use_24hour_clock_proto =
        settings.mutable_use_24hour_clock();
    use_24hour_clock_proto->Clear();
    bool use_24hour_clock_value;
    if (value.GetAsBoolean(&use_24hour_clock_value)) {
      use_24hour_clock_proto->set_use_24hour_clock(use_24hour_clock_value);
    } else {
      NOTREACHED();
    }
  } else if (path == kAttestationForContentProtectionEnabled) {
    em::AttestationSettingsProto* attestation_settings =
        settings.mutable_attestation_settings();
    bool setting_enabled;
    if (value.GetAsBoolean(&setting_enabled)) {
      attestation_settings->set_content_protection_enabled(setting_enabled);
    } else {
      NOTREACHED();
    }
  } else {
    // The remaining settings don't support Set(), since they are not
    // intended to be customizable by the user:
    //   kAccountsPrefTransferSAMLCookies
    //   kDeviceAttestationEnabled
    //   kDeviceOwner
    //   kHeartbeatEnabled
    //   kHeartbeatFrequency
    //   kReleaseChannelDelegated
    //   kReportDeviceActivityTimes
    //   kReportDeviceBootMode
    //   kReportDeviceHardwareStatus
    //   kReportDeviceLocation
    //   kReportDeviceNetworkInterfaces
    //   kReportDeviceSessionStatus
    //   kReportDeviceVersionInfo
    //   kReportDeviceUsers
    //   kServiceAccountIdentity
    //   kSystemTimezonePolicy
    //   kVariationsRestrictParameter
    //   kDeviceDisabled
    //   kDeviceDisabledMessage

    LOG(FATAL) << "Device setting " << path << " is read-only.";
  }
}

void OwnerSettingsServiceChromeOS::OnPostKeypairLoadedActions() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile_);
  user_id_ = user ? user->GetUserID() : std::string();

  const bool is_owner = IsOwner() || IsOwnerInTests(user_id_);
  if (is_owner && device_settings_service_)
    device_settings_service_->InitOwner(user_id_, weak_factory_.GetWeakPtr());

  has_pending_fixups_ = true;
}

void OwnerSettingsServiceChromeOS::ReloadKeypairImpl(const base::Callback<
    void(const scoped_refptr<PublicKey>& public_key,
         const scoped_refptr<PrivateKey>& private_key)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (waiting_for_profile_creation_ || waiting_for_tpm_token_)
    return;
  scoped_refptr<base::TaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(&LoadPrivateKey,
                 owner_key_util_,
                 ProfileHelper::GetUserIdHashFromProfile(profile_),
                 callback));
}

void OwnerSettingsServiceChromeOS::StorePendingChanges() {
  if (!HasPendingChanges() || store_settings_factory_.HasWeakPtrs() ||
      !device_settings_service_ || user_id_.empty()) {
    return;
  }

  em::ChromeDeviceSettingsProto settings;
  if (tentative_settings_.get()) {
    settings.Swap(tentative_settings_.get());
    tentative_settings_.reset();
  } else if (device_settings_service_->status() ==
                 DeviceSettingsService::STORE_SUCCESS &&
             device_settings_service_->device_settings()) {
    settings = *device_settings_service_->device_settings();
  } else {
    return;
  }

  for (const auto& change : pending_changes_)
    UpdateDeviceSettings(change.first, *change.second, settings);
  pending_changes_.clear();

  scoped_ptr<em::PolicyData> policy =
      AssemblePolicy(user_id_, device_settings_service_->policy_data(),
                     has_pending_management_settings_,
                     pending_management_settings_, &settings);
  has_pending_fixups_ = false;
  has_pending_management_settings_ = false;

  bool rv = AssembleAndSignPolicyAsync(
      content::BrowserThread::GetBlockingPool(), policy.Pass(),
      base::Bind(&OwnerSettingsServiceChromeOS::OnPolicyAssembledAndSigned,
                 store_settings_factory_.GetWeakPtr()));
  if (!rv)
    ReportStatusAndContinueStoring(false /* success */);
}

void OwnerSettingsServiceChromeOS::OnPolicyAssembledAndSigned(
    scoped_ptr<em::PolicyFetchResponse> policy_response) {
  if (!policy_response.get() || !device_settings_service_) {
    ReportStatusAndContinueStoring(false /* success */);
    return;
  }
  device_settings_service_->Store(
      policy_response.Pass(),
      base::Bind(&OwnerSettingsServiceChromeOS::OnSignedPolicyStored,
                 store_settings_factory_.GetWeakPtr(),
                 true /* success */));
}

void OwnerSettingsServiceChromeOS::OnSignedPolicyStored(bool success) {
  CHECK(device_settings_service_);
  ReportStatusAndContinueStoring(success &&
                                 device_settings_service_->status() ==
                                     DeviceSettingsService::STORE_SUCCESS);
}

void OwnerSettingsServiceChromeOS::ReportStatusAndContinueStoring(
    bool success) {
  store_settings_factory_.InvalidateWeakPtrs();
  FOR_EACH_OBSERVER(OwnerSettingsService::Observer, observers_,
                    OnSignedPolicyStored(success));

  std::vector<OnManagementSettingsSetCallback> callbacks;
  pending_management_settings_callbacks_.swap(callbacks);
  for (const auto& callback : callbacks) {
    if (!callback.is_null())
      callback.Run(success);
  }
  StorePendingChanges();
}

}  // namespace chromeos
