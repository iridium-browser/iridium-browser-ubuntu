// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "components/sync/protocol/proto_value_conversions.h"

#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/base64.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/app_notification_specifics.pb.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/dictionary_specifics.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/experiments_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/sync/protocol/favicon_image_specifics.pb.h"
#include "components/sync/protocol/favicon_tracking_specifics.pb.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/priority_preference_specifics.pb.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/typed_url_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace syncer {

namespace {

// Basic Type -> Value functions.

std::unique_ptr<base::StringValue> MakeInt64Value(int64_t x) {
  return base::MakeUnique<base::StringValue>(base::Int64ToString(x));
}

// TODO(akalin): Perhaps make JSONWriter support BinaryValue and use
// that instead of a StringValue.
std::string Base64EncodeString(const std::string& bytes) {
  std::string bytes_base64;
  base::Base64Encode(bytes, &bytes_base64);
  return bytes_base64;
}

std::unique_ptr<base::StringValue> MakeStringValue(const std::string& str) {
  return base::MakeUnique<base::StringValue>(str);
}

// T is the field type, F is either RepeatedField or RepeatedPtrField,
// and V is a subclass of Value.
template <class T, class F, class V>
std::unique_ptr<base::ListValue> MakeRepeatedValue(const F& fields,
                                                   V (*converter_fn)(T)) {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (typename F::const_iterator it = fields.begin(); it != fields.end();
       ++it) {
    list->Append(converter_fn(*it));
  }
  return list;
}

}  // namespace

// Helper macros to reduce the amount of boilerplate.

#define SET_TYPE(field, set_fn, transform)           \
  if (proto.has_##field()) {                         \
    value->set_fn(#field, transform(proto.field())); \
  }
#define SET(field, fn) SET_TYPE(field, Set, fn)
#define SET_REP(field, fn) \
  value->Set(#field, MakeRepeatedValue(proto.field(), fn))
#define SET_ENUM(field, fn) SET_TYPE(field, SetString, fn)

#define SET_BOOL(field) SET_TYPE(field, SetBoolean, )
#define SET_BYTES(field) SET_TYPE(field, SetString, Base64EncodeString)
#define SET_INT32(field) SET_TYPE(field, SetString, base::Int64ToString)
#define SET_INT32_REP(field) SET_REP(field, MakeInt64Value)
#define SET_INT64(field) SET_TYPE(field, SetString, base::Int64ToString)
#define SET_INT64_REP(field) SET_REP(field, MakeInt64Value)
#define SET_STR(field) SET_TYPE(field, SetString, )
#define SET_TIME_STR(field) SET_TYPE(field, SetString, TimestampToString)
#define SET_STR_REP(field)                                                 \
  value->Set(                                                              \
      #field,                                                              \
      MakeRepeatedValue<const std::string&,                                \
                        google::protobuf::RepeatedPtrField<std::string>,   \
                        std::unique_ptr<base::StringValue>>(proto.field(), \
                                                            MakeStringValue))
#define SET_EXPERIMENT_ENABLED_FIELD(field)                                    \
  do {                                                                         \
    if (proto.has_##field() && proto.field().has_enabled()) {                  \
      value->Set(#field, new base::FundamentalValue(proto.field().enabled())); \
    }                                                                          \
  } while (0)

#define SET_FIELD(field, fn)                     \
  do {                                           \
    if (specifics.has_##field()) {               \
      value->Set(#field, fn(specifics.field())); \
    }                                            \
  } while (0)

// If you add another macro, don't forget to add an #undef at the end
// of this file, too.

std::unique_ptr<base::DictionaryValue> EncryptedDataToValue(
    const sync_pb::EncryptedData& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(key_name);
  // TODO(akalin): Shouldn't blob be of type bytes instead of string?
  SET_BYTES(blob);
  return value;
}

std::unique_ptr<base::DictionaryValue> AppSettingsToValue(
    const sync_pb::AppNotificationSettings& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_BOOL(initial_setup_done);
  SET_BOOL(disabled);
  SET_STR(oauth_client_id);
  return value;
}

std::unique_ptr<base::DictionaryValue> SessionHeaderToValue(
    const sync_pb::SessionHeader& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_REP(window, SessionWindowToValue);
  SET_STR(client_name);
  SET_ENUM(device_type, GetDeviceTypeString);
  return value;
}

std::unique_ptr<base::DictionaryValue> SessionTabToValue(
    const sync_pb::SessionTab& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32(tab_id);
  SET_INT32(window_id);
  SET_INT32(tab_visual_index);
  SET_INT32(current_navigation_index);
  SET_BOOL(pinned);
  SET_STR(extension_app_id);
  SET_REP(navigation, TabNavigationToValue);
  SET_BYTES(favicon);
  SET_ENUM(favicon_type, GetFaviconTypeString);
  SET_STR(favicon_source);
  SET_REP(variation_id, MakeInt64Value);
  return value;
}

std::unique_ptr<base::DictionaryValue> SessionWindowToValue(
    const sync_pb::SessionWindow& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32(window_id);
  SET_INT32(selected_tab_index);
  SET_INT32_REP(tab);
  SET_ENUM(browser_type, GetBrowserTypeString);
  return value;
}

std::unique_ptr<base::DictionaryValue> TabNavigationToValue(
    const sync_pb::TabNavigation& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(virtual_url);
  SET_STR(referrer);
  SET_STR(title);
  SET_ENUM(page_transition, GetPageTransitionString);
  SET_ENUM(redirect_type, GetPageTransitionRedirectTypeString);
  SET_INT32(unique_id);
  SET_INT64(timestamp_msec);
  SET_BOOL(navigation_forward_back);
  SET_BOOL(navigation_from_address_bar);
  SET_BOOL(navigation_home_page);
  SET_BOOL(navigation_chain_start);
  SET_BOOL(navigation_chain_end);
  SET_INT64(global_id);
  SET_STR(search_terms);
  SET_STR(favicon_url);
  SET_ENUM(blocked_state, GetBlockedStateString);
  SET_STR_REP(content_pack_categories);
  SET_INT32(http_status_code);
  SET_INT32(obsolete_referrer_policy);
  SET_BOOL(is_restored);
  SET_REP(navigation_redirect, NavigationRedirectToValue);
  SET_STR(last_navigation_redirect_url);
  SET_INT32(correct_referrer_policy);
  SET_ENUM(password_state, GetPasswordStateString);
  return value;
}

std::unique_ptr<base::DictionaryValue> NavigationRedirectToValue(
    const sync_pb::NavigationRedirect& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(url);
  return value;
}

std::unique_ptr<base::DictionaryValue> PasswordSpecificsDataToValue(
    const sync_pb::PasswordSpecificsData& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32(scheme);
  SET_STR(signon_realm);
  SET_STR(origin);
  SET_STR(action);
  SET_STR(username_element);
  SET_STR(username_value);
  SET_STR(password_element);
  value->SetString("password_value", "<redacted>");
  SET_BOOL(preferred);
  SET_INT64(date_created);
  SET_BOOL(blacklisted);
  SET_INT32(type);
  SET_INT32(times_used);
  SET_STR(display_name);
  SET_STR(avatar_url);
  SET_STR(federation_url);
  return value;
}

std::unique_ptr<base::DictionaryValue> GlobalIdDirectiveToValue(
    const sync_pb::GlobalIdDirective& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT64_REP(global_id);
  SET_INT64(start_time_usec);
  SET_INT64(end_time_usec);
  return value;
}

std::unique_ptr<base::DictionaryValue> TimeRangeDirectiveToValue(
    const sync_pb::TimeRangeDirective& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT64(start_time_usec);
  SET_INT64(end_time_usec);
  return value;
}

std::unique_ptr<base::DictionaryValue> AppListSpecificsToValue(
    const sync_pb::AppListSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(item_id);
  SET_ENUM(item_type, GetAppListItemTypeString);
  SET_STR(item_name);
  SET_STR(parent_id);
  SET_STR(item_ordinal);
  SET_STR(item_pin_ordinal);

  return value;
}

std::unique_ptr<base::DictionaryValue> ArcPackageSpecificsToValue(
    const sync_pb::ArcPackageSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(package_name);
  SET_INT32(package_version);
  SET_INT64(last_backup_android_id);
  SET_INT64(last_backup_time);

  return value;
}

std::unique_ptr<base::DictionaryValue> AppNotificationToValue(
    const sync_pb::AppNotification& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(guid);
  SET_STR(app_id);
  SET_INT64(creation_timestamp_ms);
  SET_STR(title);
  SET_STR(body_text);
  SET_STR(link_url);
  SET_STR(link_text);
  return value;
}

std::unique_ptr<base::DictionaryValue> AppSettingSpecificsToValue(
    const sync_pb::AppSettingSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET(extension_setting, ExtensionSettingSpecificsToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> LinkedAppIconInfoToValue(
    const sync_pb::LinkedAppIconInfo& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(url);
  SET_INT32(size);
  return value;
}

std::unique_ptr<base::DictionaryValue> AppSpecificsToValue(
    const sync_pb::AppSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET(extension, ExtensionSpecificsToValue);
  SET(notification_settings, AppSettingsToValue);
  SET_STR(app_launch_ordinal);
  SET_STR(page_ordinal);
  SET_ENUM(launch_type, GetLaunchTypeString);
  SET_STR(bookmark_app_url);
  SET_STR(bookmark_app_description);
  SET_STR(bookmark_app_icon_color);
  SET_REP(linked_app_icons, LinkedAppIconInfoToValue);

  return value;
}

std::unique_ptr<base::DictionaryValue> AutofillSpecificsToValue(
    const sync_pb::AutofillSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(name);
  SET_STR(value);
  SET_INT64_REP(usage_timestamp);
  SET(profile, AutofillProfileSpecificsToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> AutofillProfileSpecificsToValue(
    const sync_pb::AutofillProfileSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(guid);
  SET_STR(origin);
  SET_INT64(use_count);
  SET_INT64(use_date);

  SET_STR_REP(name_first);
  SET_STR_REP(name_middle);
  SET_STR_REP(name_last);
  SET_STR_REP(name_full);
  SET_STR_REP(email_address);
  SET_STR(company_name);

  SET_STR(address_home_line1);
  SET_STR(address_home_line2);
  SET_STR(address_home_city);
  SET_STR(address_home_state);
  SET_STR(address_home_zip);
  SET_STR(address_home_country);

  SET_STR(address_home_street_address);
  SET_STR(address_home_sorting_code);
  SET_STR(address_home_dependent_locality);
  SET_STR(address_home_language_code);

  SET_STR_REP(phone_home_whole_number);
  return value;
}

std::unique_ptr<base::DictionaryValue> WalletMetadataSpecificsToValue(
    const sync_pb::WalletMetadataSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_ENUM(type, GetWalletMetadataTypeString);
  SET_STR(id);
  SET_INT64(use_count);
  SET_INT64(use_date);
  return value;
}

std::unique_ptr<base::DictionaryValue> AutofillWalletSpecificsToValue(
    const sync_pb::AutofillWalletSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  SET_ENUM(type, GetWalletInfoTypeString);
  if (proto.type() == sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD) {
    value->Set("masked_card",
               WalletMaskedCreditCardToValue(proto.masked_card()));
  } else if (proto.type() == sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS) {
    value->Set("address", WalletPostalAddressToValue(proto.address()));
  }
  return value;
}

std::unique_ptr<base::DictionaryValue> MetaInfoToValue(
    const sync_pb::MetaInfo& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(key);
  SET_STR(value);
  return value;
}

std::unique_ptr<base::DictionaryValue> BookmarkSpecificsToValue(
    const sync_pb::BookmarkSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(url);
  SET_BYTES(favicon);
  SET_STR(title);
  SET_INT64(creation_time_us);
  SET_STR(icon_url);
  SET_REP(meta_info, &MetaInfoToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> DeviceInfoSpecificsToValue(
    const sync_pb::DeviceInfoSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(cache_guid);
  SET_STR(client_name);
  SET_ENUM(device_type, GetDeviceTypeString);
  SET_STR(sync_user_agent);
  SET_STR(chrome_version);
  SET_STR(signin_scoped_device_id);
  return value;
}

std::unique_ptr<base::DictionaryValue> DictionarySpecificsToValue(
    const sync_pb::DictionarySpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(word);
  return value;
}

namespace {

std::unique_ptr<base::DictionaryValue> FaviconSyncFlagsToValue(
    const sync_pb::FaviconSyncFlags& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_BOOL(enabled);
  SET_INT32(favicon_sync_limit);
  return value;
}

}  // namespace

std::unique_ptr<base::DictionaryValue> ExperimentsSpecificsToValue(
    const sync_pb::ExperimentsSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_EXPERIMENT_ENABLED_FIELD(keystore_encryption);
  SET_EXPERIMENT_ENABLED_FIELD(history_delete_directives);
  SET_EXPERIMENT_ENABLED_FIELD(autofill_culling);
  SET_EXPERIMENT_ENABLED_FIELD(pre_commit_update_avoidance);
  SET(favicon_sync, FaviconSyncFlagsToValue);
  SET_EXPERIMENT_ENABLED_FIELD(gcm_channel);
  SET_EXPERIMENT_ENABLED_FIELD(gcm_invalidations);
  return value;
}

std::unique_ptr<base::DictionaryValue> ExtensionSettingSpecificsToValue(
    const sync_pb::ExtensionSettingSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(extension_id);
  SET_STR(key);
  SET_STR(value);
  return value;
}

std::unique_ptr<base::DictionaryValue> ExtensionSpecificsToValue(
    const sync_pb::ExtensionSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(id);
  SET_STR(version);
  SET_STR(update_url);
  SET_BOOL(enabled);
  SET_BOOL(incognito_enabled);
  SET_STR(name);
  SET_BOOL(remote_install);
  SET_BOOL(installed_by_custodian);
  SET_BOOL(all_urls_enabled);
  SET_INT32(disable_reasons);
  return value;
}

namespace {
std::unique_ptr<base::DictionaryValue> FaviconDataToValue(
    const sync_pb::FaviconData& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_BYTES(favicon);
  SET_INT32(width);
  SET_INT32(height);
  return value;
}
}  // namespace

std::unique_ptr<base::DictionaryValue> FaviconImageSpecificsToValue(
    const sync_pb::FaviconImageSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(favicon_url);
  SET(favicon_web, FaviconDataToValue);
  SET(favicon_web_32, FaviconDataToValue);
  SET(favicon_touch_64, FaviconDataToValue);
  SET(favicon_touch_precomposed_64, FaviconDataToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> FaviconTrackingSpecificsToValue(
    const sync_pb::FaviconTrackingSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(favicon_url);
  SET_INT64(last_visit_time_ms)
  SET_BOOL(is_bookmarked);
  return value;
}

std::unique_ptr<base::DictionaryValue> HistoryDeleteDirectiveSpecificsToValue(
    const sync_pb::HistoryDeleteDirectiveSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET(global_id_directive, GlobalIdDirectiveToValue);
  SET(time_range_directive, TimeRangeDirectiveToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> ManagedUserSettingSpecificsToValue(
    const sync_pb::ManagedUserSettingSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(name);
  SET_STR(value);
  return value;
}

std::unique_ptr<base::DictionaryValue> ManagedUserSpecificsToValue(
    const sync_pb::ManagedUserSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(id);
  SET_STR(name);
  SET_BOOL(acknowledged);
  SET_STR(master_key);
  SET_STR(chrome_avatar);
  SET_STR(chromeos_avatar);
  return value;
}

std::unique_ptr<base::DictionaryValue> ManagedUserSharedSettingSpecificsToValue(
    const sync_pb::ManagedUserSharedSettingSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(mu_id);
  SET_STR(key);
  SET_STR(value);
  SET_BOOL(acknowledged);
  return value;
}

std::unique_ptr<base::DictionaryValue> ManagedUserWhitelistSpecificsToValue(
    const sync_pb::ManagedUserWhitelistSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(id);
  SET_STR(name);
  return value;
}

std::unique_ptr<base::DictionaryValue> NigoriSpecificsToValue(
    const sync_pb::NigoriSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET(encryption_keybag, EncryptedDataToValue);
  SET_BOOL(keybag_is_frozen);
  SET_BOOL(encrypt_bookmarks);
  SET_BOOL(encrypt_preferences);
  SET_BOOL(encrypt_autofill_profile);
  SET_BOOL(encrypt_autofill);
  SET_BOOL(encrypt_themes);
  SET_BOOL(encrypt_typed_urls);
  SET_BOOL(encrypt_extension_settings);
  SET_BOOL(encrypt_extensions);
  SET_BOOL(encrypt_sessions);
  SET_BOOL(encrypt_app_settings);
  SET_BOOL(encrypt_apps);
  SET_BOOL(encrypt_search_engines);
  SET_BOOL(encrypt_dictionary);
  SET_BOOL(encrypt_articles);
  SET_BOOL(encrypt_app_list);
  SET_BOOL(encrypt_arc_package);
  SET_BOOL(encrypt_everything);
  SET_BOOL(server_only_was_missing_keystore_migration_time);
  SET_BOOL(sync_tab_favicons);
  SET_ENUM(passphrase_type, PassphraseTypeString);
  SET(keystore_decryptor_token, EncryptedDataToValue);
  SET_INT64(keystore_migration_time);
  SET_INT64(custom_passphrase_time);
  return value;
}

std::unique_ptr<base::DictionaryValue> ArticlePageToValue(
    const sync_pb::ArticlePage& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(url);
  return value;
}

std::unique_ptr<base::DictionaryValue> ArticleSpecificsToValue(
    const sync_pb::ArticleSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(entry_id);
  SET_STR(title);
  SET_REP(pages, ArticlePageToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> PasswordSpecificsToValue(
    const sync_pb::PasswordSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET(encrypted, EncryptedDataToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> PreferenceSpecificsToValue(
    const sync_pb::PreferenceSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(name);
  SET_STR(value);
  return value;
}

std::unique_ptr<base::DictionaryValue> PriorityPreferenceSpecificsToValue(
    const sync_pb::PriorityPreferenceSpecifics& specifics) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_FIELD(preference, PreferenceSpecificsToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue>
SyncedNotificationAppInfoSpecificsToValue(
    const sync_pb::SyncedNotificationAppInfoSpecifics& proto) {
  return base::MakeUnique<base::DictionaryValue>();
}

std::unique_ptr<base::DictionaryValue> SyncedNotificationSpecificsToValue(
    const sync_pb::SyncedNotificationSpecifics& proto) {
  return base::MakeUnique<base::DictionaryValue>();
}

std::unique_ptr<base::DictionaryValue> SearchEngineSpecificsToValue(
    const sync_pb::SearchEngineSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(short_name);
  SET_STR(keyword);
  SET_STR(favicon_url);
  SET_STR(url);
  SET_BOOL(safe_for_autoreplace);
  SET_STR(originating_url);
  SET_INT64(date_created);
  SET_STR(input_encodings);
  SET_BOOL(show_in_default_list);
  SET_STR(suggestions_url);
  SET_INT32(prepopulate_id);
  SET_BOOL(autogenerate_keyword);
  SET_STR(instant_url);
  SET_INT64(last_modified);
  SET_STR(sync_guid);
  SET_STR_REP(alternate_urls);
  SET_STR(search_terms_replacement_key);
  SET_STR(image_url);
  SET_STR(search_url_post_params);
  SET_STR(suggestions_url_post_params);
  SET_STR(instant_url_post_params);
  SET_STR(image_url_post_params);
  SET_STR(new_tab_url);
  return value;
}

std::unique_ptr<base::DictionaryValue> SessionSpecificsToValue(
    const sync_pb::SessionSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(session_tag);
  SET(header, SessionHeaderToValue);
  SET(tab, SessionTabToValue);
  SET_INT32(tab_node_id);
  return value;
}

std::unique_ptr<base::DictionaryValue> ThemeSpecificsToValue(
    const sync_pb::ThemeSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_BOOL(use_custom_theme);
  SET_BOOL(use_system_theme_by_default);
  SET_STR(custom_theme_name);
  SET_STR(custom_theme_id);
  SET_STR(custom_theme_update_url);
  return value;
}

std::unique_ptr<base::DictionaryValue> TypedUrlSpecificsToValue(
    const sync_pb::TypedUrlSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(url);
  SET_STR(title);
  SET_BOOL(hidden);
  SET_INT64_REP(visits);
  SET_INT32_REP(visit_transitions);
  return value;
}

std::unique_ptr<base::DictionaryValue> WalletMaskedCreditCardToValue(
    const sync_pb::WalletMaskedCreditCard& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(id);
  SET_ENUM(status, GetWalletCardStatusString);
  SET_STR(name_on_card);
  SET_ENUM(type, GetWalletCardTypeString);
  SET_STR(last_four);
  SET_INT32(exp_month);
  SET_INT32(exp_year);
  SET_STR(billing_address_id);
  return value;
}

std::unique_ptr<base::DictionaryValue> WalletPostalAddressToValue(
    const sync_pb::WalletPostalAddress& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(id);
  SET_STR(recipient_name);
  SET_STR(company_name);
  SET_STR_REP(street_address);
  SET_STR(address_1);
  SET_STR(address_2);
  SET_STR(address_3);
  SET_STR(address_4);
  SET_STR(postal_code);
  SET_STR(sorting_code);
  SET_STR(country_code);
  SET_STR(phone_number);
  SET_STR(language_code);
  return value;
}

std::unique_ptr<base::DictionaryValue> WifiCredentialSpecificsToValue(
    const sync_pb::WifiCredentialSpecifics& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_BYTES(ssid);
  SET_ENUM(security_class, GetWifiCredentialSecurityClassString);
  SET_BYTES(passphrase);
  return value;
}

std::unique_ptr<base::DictionaryValue> EntitySpecificsToValue(
    const sync_pb::EntitySpecifics& specifics) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_FIELD(app, AppSpecificsToValue);
  SET_FIELD(app_list, AppListSpecificsToValue);
  SET_FIELD(app_notification, AppNotificationToValue);
  SET_FIELD(app_setting, AppSettingSpecificsToValue);
  SET_FIELD(arc_package, ArcPackageSpecificsToValue);
  SET_FIELD(article, ArticleSpecificsToValue);
  SET_FIELD(autofill, AutofillSpecificsToValue);
  SET_FIELD(autofill_profile, AutofillProfileSpecificsToValue);
  SET_FIELD(autofill_wallet, AutofillWalletSpecificsToValue);
  SET_FIELD(wallet_metadata, WalletMetadataSpecificsToValue);
  SET_FIELD(bookmark, BookmarkSpecificsToValue);
  SET_FIELD(device_info, DeviceInfoSpecificsToValue);
  SET_FIELD(dictionary, DictionarySpecificsToValue);
  SET_FIELD(experiments, ExperimentsSpecificsToValue);
  SET_FIELD(extension, ExtensionSpecificsToValue);
  SET_FIELD(extension_setting, ExtensionSettingSpecificsToValue);
  SET_FIELD(favicon_image, FaviconImageSpecificsToValue);
  SET_FIELD(favicon_tracking, FaviconTrackingSpecificsToValue);
  SET_FIELD(history_delete_directive, HistoryDeleteDirectiveSpecificsToValue);
  SET_FIELD(managed_user_setting, ManagedUserSettingSpecificsToValue);
  SET_FIELD(managed_user_shared_setting,
            ManagedUserSharedSettingSpecificsToValue);
  SET_FIELD(managed_user, ManagedUserSpecificsToValue);
  SET_FIELD(managed_user_whitelist, ManagedUserWhitelistSpecificsToValue);
  SET_FIELD(nigori, NigoriSpecificsToValue);
  SET_FIELD(password, PasswordSpecificsToValue);
  SET_FIELD(preference, PreferenceSpecificsToValue);
  SET_FIELD(priority_preference, PriorityPreferenceSpecificsToValue);
  SET_FIELD(search_engine, SearchEngineSpecificsToValue);
  SET_FIELD(session, SessionSpecificsToValue);
  SET_FIELD(synced_notification, SyncedNotificationSpecificsToValue);
  SET_FIELD(synced_notification_app_info,
            SyncedNotificationAppInfoSpecificsToValue);
  SET_FIELD(theme, ThemeSpecificsToValue);
  SET_FIELD(typed_url, TypedUrlSpecificsToValue);
  SET_FIELD(wifi_credential, WifiCredentialSpecificsToValue);
  return value;
}

namespace {

base::StringValue* UniquePositionToStringValue(
    const sync_pb::UniquePosition& proto) {
  UniquePosition pos = UniquePosition::FromProto(proto);
  return new base::StringValue(pos.ToDebugString());
}

}  // namespace

std::unique_ptr<base::DictionaryValue> SyncEntityToValue(
    const sync_pb::SyncEntity& proto,
    bool include_specifics) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(id_string);
  SET_STR(parent_id_string);
  SET_STR(old_parent_id);
  SET_INT64(version);
  SET_INT64(mtime);
  SET_INT64(ctime);
  SET_STR(name);
  SET_STR(non_unique_name);
  SET_INT64(sync_timestamp);
  SET_STR(server_defined_unique_tag);
  SET_INT64(position_in_parent);
  SET(unique_position, UniquePositionToStringValue);
  SET_STR(insert_after_item_id);
  SET_BOOL(deleted);
  SET_STR(originator_cache_guid);
  SET_STR(originator_client_item_id);
  if (include_specifics)
    SET(specifics, EntitySpecificsToValue);
  SET_BOOL(folder);
  SET_STR(client_defined_unique_tag);
  SET_REP(attachment_id, AttachmentIdProtoToValue);
  return value;
}

namespace {

base::ListValue* SyncEntitiesToValue(
    const ::google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>& entities,
    bool include_specifics) {
  base::ListValue* list = new base::ListValue();
  ::google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>::const_iterator it;
  for (it = entities.begin(); it != entities.end(); ++it) {
    list->Append(SyncEntityToValue(*it, include_specifics));
  }

  return list;
}

std::unique_ptr<base::DictionaryValue> ChromiumExtensionActivityToValue(
    const sync_pb::ChromiumExtensionsActivity& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(extension_id);
  SET_INT32(bookmark_writes_since_last_commit);
  return value;
}

std::unique_ptr<base::DictionaryValue> CommitMessageToValue(
    const sync_pb::CommitMessage& proto,
    bool include_specifics) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set("entries",
             SyncEntitiesToValue(proto.entries(), include_specifics));
  SET_STR(cache_guid);
  SET_REP(extensions_activity, ChromiumExtensionActivityToValue);
  SET(config_params, ClientConfigParamsToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> GetUpdateTriggersToValue(
    const sync_pb::GetUpdateTriggers& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR_REP(notification_hint);
  SET_BOOL(client_dropped_hints);
  SET_BOOL(invalidations_out_of_sync);
  SET_INT64(local_modification_nudges);
  SET_INT64(datatype_refresh_nudges);
  return value;
}

std::unique_ptr<base::DictionaryValue> DataTypeProgressMarkerToValue(
    const sync_pb::DataTypeProgressMarker& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32(data_type_id);
  SET_BYTES(token);
  SET_INT64(timestamp_token_for_migration);
  SET_STR(notification_hint);
  SET(get_update_triggers, GetUpdateTriggersToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> DataTypeContextToValue(
    const sync_pb::DataTypeContext& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32(data_type_id);
  SET_STR(context);
  SET_INT64(version);
  return value;
}

std::unique_ptr<base::DictionaryValue> GetUpdatesCallerInfoToValue(
    const sync_pb::GetUpdatesCallerInfo& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_ENUM(source, GetUpdatesSourceString);
  SET_BOOL(notifications_enabled);
  return value;
}

std::unique_ptr<base::DictionaryValue> GetUpdatesMessageToValue(
    const sync_pb::GetUpdatesMessage& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET(caller_info, GetUpdatesCallerInfoToValue);
  SET_BOOL(fetch_folders);
  SET_INT32(batch_size);
  SET_REP(from_progress_marker, DataTypeProgressMarkerToValue);
  SET_BOOL(streaming);
  SET_BOOL(need_encryption_key);
  SET_BOOL(create_mobile_bookmarks_folder);
  SET_ENUM(get_updates_origin, GetUpdatesOriginString);
  SET_REP(client_contexts, DataTypeContextToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> ClientStatusToValue(
    const sync_pb::ClientStatus& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_BOOL(hierarchy_conflict_detected);
  return value;
}

std::unique_ptr<base::DictionaryValue> EntryResponseToValue(
    const sync_pb::CommitResponse::EntryResponse& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_ENUM(response_type, GetResponseTypeString);
  SET_STR(id_string);
  SET_STR(parent_id_string);
  SET_INT64(position_in_parent);
  SET_INT64(version);
  SET_STR(name);
  SET_STR(error_message);
  SET_INT64(mtime);
  return value;
}

std::unique_ptr<base::DictionaryValue> CommitResponseToValue(
    const sync_pb::CommitResponse& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_REP(entryresponse, EntryResponseToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> GetUpdatesResponseToValue(
    const sync_pb::GetUpdatesResponse& proto,
    bool include_specifics) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->Set("entries",
             SyncEntitiesToValue(proto.entries(), include_specifics));
  SET_INT64(changes_remaining);
  SET_REP(new_progress_marker, DataTypeProgressMarkerToValue);
  SET_REP(context_mutations, DataTypeContextToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> ClientCommandToValue(
    const sync_pb::ClientCommand& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32(set_sync_poll_interval);
  SET_INT32(set_sync_long_poll_interval);
  SET_INT32(max_commit_batch_size);
  SET_INT32(sessions_commit_delay_seconds);
  SET_INT32(throttle_delay_seconds);
  SET_INT32(client_invalidation_hint_buffer_size);
  return value;
}

std::unique_ptr<base::DictionaryValue> ErrorToValue(
    const sync_pb::ClientToServerResponse::Error& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_ENUM(error_type, GetErrorTypeString);
  SET_STR(error_description);
  SET_STR(url);
  SET_ENUM(action, GetActionString);
  return value;
}

}  // namespace

std::unique_ptr<base::DictionaryValue> ClientToServerResponseToValue(
    const sync_pb::ClientToServerResponse& proto,
    bool include_specifics) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET(commit, CommitResponseToValue);
  if (proto.has_get_updates()) {
    value->Set("get_updates", GetUpdatesResponseToValue(proto.get_updates(),
                                                        include_specifics));
  }

  SET(error, ErrorToValue);
  SET_ENUM(error_code, GetErrorTypeString);
  SET_STR(error_message);
  SET_STR(store_birthday);
  SET(client_command, ClientCommandToValue);
  SET_INT32_REP(migrated_data_type_id);
  return value;
}

std::unique_ptr<base::DictionaryValue> ClientToServerMessageToValue(
    const sync_pb::ClientToServerMessage& proto,
    bool include_specifics) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(share);
  SET_INT32(protocol_version);
  if (proto.has_commit()) {
    value->Set("commit",
               CommitMessageToValue(proto.commit(), include_specifics));
  }

  SET(get_updates, GetUpdatesMessageToValue);
  SET_STR(store_birthday);
  SET_BOOL(sync_problem_detected);
  SET(debug_info, DebugInfoToValue);
  SET(client_status, ClientStatusToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> DatatypeAssociationStatsToValue(
    const sync_pb::DatatypeAssociationStats& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32(data_type_id);
  SET_INT32(num_local_items_before_association);
  SET_INT32(num_sync_items_before_association);
  SET_INT32(num_local_items_after_association);
  SET_INT32(num_sync_items_after_association);
  SET_INT32(num_local_items_added);
  SET_INT32(num_local_items_deleted);
  SET_INT32(num_local_items_modified);
  SET_INT32(num_sync_items_added);
  SET_INT32(num_sync_items_deleted);
  SET_INT32(num_sync_items_modified);
  SET_INT64(local_version_pre_association);
  SET_INT64(sync_version_pre_association)
  SET_BOOL(had_error);
  SET_INT64(download_wait_time_us);
  SET_INT64(download_time_us);
  SET_INT64(association_wait_time_for_high_priority_us);
  SET_INT64(association_wait_time_for_same_priority_us);
  return value;
}

std::unique_ptr<base::DictionaryValue> DebugEventInfoToValue(
    const sync_pb::DebugEventInfo& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_ENUM(singleton_event, SingletonDebugEventTypeString);
  SET(sync_cycle_completed_event_info, SyncCycleCompletedEventInfoToValue);
  SET_INT32(nudging_datatype);
  SET_INT32_REP(datatypes_notified_from_server);
  SET(datatype_association_stats, DatatypeAssociationStatsToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> DebugInfoToValue(
    const sync_pb::DebugInfo& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_REP(events, DebugEventInfoToValue);
  SET_BOOL(cryptographer_ready);
  SET_BOOL(cryptographer_has_pending_keys);
  SET_BOOL(events_dropped);
  return value;
}

std::unique_ptr<base::DictionaryValue> SyncCycleCompletedEventInfoToValue(
    const sync_pb::SyncCycleCompletedEventInfo& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32(num_encryption_conflicts);
  SET_INT32(num_hierarchy_conflicts);
  SET_INT32(num_server_conflicts);
  SET_INT32(num_updates_downloaded);
  SET_INT32(num_reflected_updates_downloaded);
  SET(caller_info, GetUpdatesCallerInfoToValue);
  return value;
}

std::unique_ptr<base::DictionaryValue> ClientConfigParamsToValue(
    const sync_pb::ClientConfigParams& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_INT32_REP(enabled_type_ids);
  SET_BOOL(tabs_datatype_enabled);
  SET_BOOL(cookie_jar_mismatch);
  return value;
}

std::unique_ptr<base::DictionaryValue> AttachmentIdProtoToValue(
    const sync_pb::AttachmentIdProto& proto) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  SET_STR(unique_id);
  return value;
}

#undef SET_TYPE
#undef SET
#undef SET_REP

#undef SET_BOOL
#undef SET_BYTES
#undef SET_INT32
#undef SET_INT64
#undef SET_INT64_REP
#undef SET_STR
#undef SET_STR_REP

#undef SET_FIELD

}  // namespace syncer
