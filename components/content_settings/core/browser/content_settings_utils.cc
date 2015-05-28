// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_utils.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

namespace {

// The names of the ContentSettingsType values, for use with dictionary prefs.
const char* kTypeNames[] = {
  "cookies",
  "images",
  "javascript",
  "plugins",
  "popups",
  "geolocation",
  "notifications",
  "auto-select-certificate",
  "fullscreen",
  "mouselock",
  "mixed-script",
  "media-stream",
  "media-stream-mic",
  "media-stream-camera",
  "register-protocol-handler",
  "ppapi-broker",
  "multiple-automatic-downloads",
  "midi-sysex",
  "push-messaging",
  "ssl-cert-decisions",
#if defined(OS_WIN)
  "metro-switch-to-desktop",
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
  "protected-media-identifier",
#endif
  "app-banner",
};
static_assert(arraysize(kTypeNames) == CONTENT_SETTINGS_NUM_TYPES,
              "kTypeNames should have CONTENT_SETTINGS_NUM_TYPES elements");

const char kPatternSeparator[] = ",";

}  // namespace

namespace content_settings {

std::string GetTypeName(ContentSettingsType type) {
  return std::string(kTypeNames[type]);
}

bool GetTypeFromName(const std::string& name,
                     ContentSettingsType* return_setting) {
  for (size_t type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
    if (name.compare(kTypeNames[type]) == 0) {
      *return_setting = static_cast<ContentSettingsType>(type);
      return true;
    }
  }
  return false;
}

std::string ContentSettingToString(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return "allow";
    case CONTENT_SETTING_ASK:
      return "ask";
    case CONTENT_SETTING_BLOCK:
      return "block";
    case CONTENT_SETTING_SESSION_ONLY:
      return "session";
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
      return "detect";
    case CONTENT_SETTING_DEFAULT:
      return "default";
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
  }

  return std::string();
}

ContentSetting ContentSettingFromString(const std::string& name) {
  if (name == "allow")
    return CONTENT_SETTING_ALLOW;
  if (name == "ask")
    return CONTENT_SETTING_ASK;
  if (name == "block")
    return CONTENT_SETTING_BLOCK;
  if (name == "session")
    return CONTENT_SETTING_SESSION_ONLY;
  if (name == "detect")
    return CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;

  NOTREACHED() << name << " is not a recognized content setting.";
  return CONTENT_SETTING_DEFAULT;
}

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern) {
  return item_pattern.ToString()
         + std::string(kPatternSeparator)
         + top_level_frame_pattern.ToString();
}

PatternPair ParsePatternString(const std::string& pattern_str) {
  std::vector<std::string> pattern_str_list;
  base::SplitString(pattern_str, kPatternSeparator[0], &pattern_str_list);

  // If the |pattern_str| is an empty string then the |pattern_string_list|
  // contains a single empty string. In this case the empty string will be
  // removed to signal an invalid |pattern_str|. Invalid pattern strings are
  // handle by the "if"-statment below. So the order of the if statements here
  // must be preserved.
  if (pattern_str_list.size() == 1) {
    if (pattern_str_list[0].empty()) {
      pattern_str_list.pop_back();
    } else {
      pattern_str_list.push_back("*");
    }
  }

  if (pattern_str_list.size() > 2 ||
      pattern_str_list.size() == 0) {
    return PatternPair(ContentSettingsPattern(),
                       ContentSettingsPattern());
  }

  PatternPair pattern_pair;
  pattern_pair.first =
      ContentSettingsPattern::FromString(pattern_str_list[0]);
  pattern_pair.second =
      ContentSettingsPattern::FromString(pattern_str_list[1]);
  return pattern_pair;
}

ContentSetting ValueToContentSetting(const base::Value* value) {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  bool valid = ParseContentSettingValue(value, &setting);
  DCHECK(valid);
  return setting;
}

bool ParseContentSettingValue(const base::Value* value,
                              ContentSetting* setting) {
  if (!value) {
    *setting = CONTENT_SETTING_DEFAULT;
    return true;
  }
  int int_value = -1;
  if (!value->GetAsInteger(&int_value))
    return false;
  *setting = IntToContentSetting(int_value);
  return *setting != CONTENT_SETTING_DEFAULT;
}

base::Value* GetContentSettingValueAndPatterns(
    const ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool include_incognito,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  if (include_incognito) {
    // Check incognito-only specific settings. It's essential that the
    // |RuleIterator| gets out of scope before we get a rule iterator for the
    // normal mode.
    scoped_ptr<RuleIterator> incognito_rule_iterator(
        provider->GetRuleIterator(content_type, resource_identifier, true));
    base::Value* value = GetContentSettingValueAndPatterns(
        incognito_rule_iterator.get(), primary_url, secondary_url,
        primary_pattern, secondary_pattern);
    if (value)
      return value;
  }
  // No settings from the incognito; use the normal mode.
  scoped_ptr<RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, resource_identifier, false));
  return GetContentSettingValueAndPatterns(
      rule_iterator.get(), primary_url, secondary_url,
      primary_pattern, secondary_pattern);
}

base::Value* GetContentSettingValueAndPatterns(
    RuleIterator* rule_iterator,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  while (rule_iterator->HasNext()) {
    const Rule& rule = rule_iterator->Next();
    if (rule.primary_pattern.Matches(primary_url) &&
        rule.secondary_pattern.Matches(secondary_url)) {
      if (primary_pattern)
        *primary_pattern = rule.primary_pattern;
      if (secondary_pattern)
        *secondary_pattern = rule.secondary_pattern;
      return rule.value.get()->DeepCopy();
    }
  }
  return NULL;
}

void GetRendererContentSettingRules(const HostContentSettingsMap* map,
                                    RendererContentSettingRules* rules) {
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_IMAGES, std::string(), &(rules->image_rules));
  map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(), &(rules->script_rules));
}

}  // namespace content_settings
