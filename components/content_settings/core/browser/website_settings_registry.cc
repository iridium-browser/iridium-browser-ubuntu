// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_registry.h"

#include "base/logging.h"
#include "base/values.h"
#include "components/content_settings/core/browser/plugins_field_trial.h"
#include "components/content_settings/core/common/content_settings.h"

namespace {

base::LazyInstance<content_settings::WebsiteSettingsRegistry>
    g_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content_settings {

// static
WebsiteSettingsRegistry* WebsiteSettingsRegistry::GetInstance() {
  return g_instance.Pointer();
}

WebsiteSettingsRegistry::WebsiteSettingsRegistry() {
  website_settings_info_.resize(CONTENT_SETTINGS_NUM_TYPES);

  // TODO(raymes): This registration code should not have to be in a single
  // location. It should be possible to register a setting from the code
  // associated with it.

  // WARNING: The string names of the permissions passed in below are used to
  // generate preference names and should never be changed!

  // Content settings (those with allow/block/ask/etc. values).
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_COOKIES, "cookies",
                         CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_IMAGES, "images",
                         CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript",
                         CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS, "plugins",
                         PluginsFieldTrial::GetDefaultPluginsContentSetting(),
                         WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, "popups",
                         CONTENT_SETTING_BLOCK, WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION, "geolocation",
                         CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications",
                         CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_FULLSCREEN, "fullscreen",
                         CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_MOUSELOCK, "mouselock",
                         CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, "mixed-script",
                         CONTENT_SETTING_DEFAULT,
                         WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
                         "media-stream-mic", CONTENT_SETTING_ASK,
                         WebsiteSettingsInfo::UNSYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                         "media-stream-camera", CONTENT_SETTING_ASK,
                         WebsiteSettingsInfo::UNSYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS,
                         "protocol-handler", CONTENT_SETTING_DEFAULT,
                         WebsiteSettingsInfo::UNSYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker",
                         CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
                         "automatic-downloads", CONTENT_SETTING_ASK,
                         WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex",
                         CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE);
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, "push-messaging",
                         CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                         "protected-media-identifier", CONTENT_SETTING_ASK,
                         WebsiteSettingsInfo::UNSYNCABLE);
#endif
  RegisterContentSetting(CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
                         "durable-storage", CONTENT_SETTING_ASK,
                         WebsiteSettingsInfo::UNSYNCABLE);

  // Website settings.
  RegisterWebsiteSetting(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                         "auto-select-certificate",
                         WebsiteSettingsInfo::NOT_LOSSY);
  RegisterWebsiteSetting(CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS,
                         "ssl-cert-decisions", WebsiteSettingsInfo::NOT_LOSSY);
  RegisterWebsiteSetting(CONTENT_SETTINGS_TYPE_APP_BANNER, "app-banner",
                         WebsiteSettingsInfo::LOSSY);
  RegisterWebsiteSetting(CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
                         "site-engagement", WebsiteSettingsInfo::LOSSY);

  // Deprecated.
  RegisterWebsiteSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM, "media-stream",
                         WebsiteSettingsInfo::NOT_LOSSY);
}

WebsiteSettingsRegistry::~WebsiteSettingsRegistry() {}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::Get(
    ContentSettingsType type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, static_cast<int>(website_settings_info_.size()));
  return website_settings_info_[type];
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::GetByName(
    const std::string& name) const {
  for (const auto& info : website_settings_info_) {
    if (info && info->name() == name)
      return info;
  }
  return nullptr;
}

void WebsiteSettingsRegistry::RegisterWebsiteSetting(
    ContentSettingsType type,
    const std::string& name,
    WebsiteSettingsInfo::LossyStatus lossy_status) {
  StoreWebsiteSettingsInfo(new WebsiteSettingsInfo(
      type, name, nullptr, WebsiteSettingsInfo::UNSYNCABLE, lossy_status));
}

void WebsiteSettingsRegistry::RegisterContentSetting(
    ContentSettingsType type,
    const std::string& name,
    ContentSetting initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status) {
  scoped_ptr<base::Value> default_value(
      new base::FundamentalValue(static_cast<int>(initial_default_value)));
  StoreWebsiteSettingsInfo(
      new WebsiteSettingsInfo(type, name, default_value.Pass(), sync_status,
                              WebsiteSettingsInfo::NOT_LOSSY));
}

void WebsiteSettingsRegistry::StoreWebsiteSettingsInfo(
    WebsiteSettingsInfo* info) {
  DCHECK_GE(info->type(), 0);
  DCHECK_LT(info->type(), static_cast<int>(website_settings_info_.size()));
  website_settings_info_[info->type()] = info;
}

}  // namespace content_settings
