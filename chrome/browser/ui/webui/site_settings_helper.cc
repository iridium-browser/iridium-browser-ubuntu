// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/site_settings_helper.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"

namespace site_settings {

const char kSetting[] = "setting";
const char kOrigin[] = "origin";
const char kPolicyProviderId[] = "policy";
const char kSource[] = "source";
const char kEmbeddingOrigin[] = "embeddingOrigin";
const char kPreferencesSource[] = "preference";
const char kObject[] = "object";
const char kObjectName[] = "objectName";

const char kGroupTypeUsb[] = "usb-devices";

ChooserContextBase* GetUsbChooserContext(Profile* profile) {
  return reinterpret_cast<ChooserContextBase*>(
      UsbChooserContextFactory::GetForProfile(profile));
}

namespace {

// Maps from the UI string to the object it represents (for sorting purposes).
typedef std::multimap<std::string, const base::DictionaryValue*> SortedObjects;

// Maps from a secondary URL to the set of objects it has permission to access.
typedef std::map<GURL, SortedObjects> OneOriginObjects;

// Maps from a primary URL/source pair to a OneOriginObjects. All the mappings
// in OneOriginObjects share the given primary URL and source.
typedef std::map<std::pair<GURL, std::string>, OneOriginObjects>
    AllOriginObjects;

}  // namespace

bool HasRegisteredGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type)
      return true;
  }
  return false;
}

ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (name == kContentSettingsTypeGroupNames[i].name)
      return kContentSettingsTypeGroupNames[i].type;
  }

  NOTREACHED() << name << " is not a recognized content settings type.";
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

std::string ContentSettingsTypeToGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type)
      return kContentSettingsTypeGroupNames[i].name;
  }

  NOTREACHED() << type << " is not a recognized content settings type.";
  return std::string();
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
std::unique_ptr<base::DictionaryValue> GetExceptionForPage(
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ContentSetting& setting,
    const std::string& provider_name) {
  base::DictionaryValue* exception = new base::DictionaryValue();
  exception->SetString(kOrigin, pattern.ToString());
  exception->SetString(kEmbeddingOrigin,
                       secondary_pattern == ContentSettingsPattern::Wildcard() ?
                           std::string() :
                           secondary_pattern.ToString());

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kSource, provider_name);
  return base::WrapUnique(exception);
}

void GetExceptionsFromHostContentSettingsMap(const HostContentSettingsMap* map,
                                             ContentSettingsType type,
                                             content::WebUI* web_ui,
                                             base::ListValue* exceptions) {
  ContentSettingsForOneType entries;
  map->GetSettingsForOneType(type, std::string(), &entries);
  // Group settings by primary_pattern.
  AllPatternsSettings all_patterns_settings;
  for (ContentSettingsForOneType::iterator i = entries.begin();
       i != entries.end(); ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i->source != kPreferencesSource) {
      continue;
    }

    // Off-the-record HostContentSettingsMap contains incognito content settings
    // as well as normal content settings. Here, we use the incongnito settings
    // only.
    if (map->is_off_the_record() && !i->incognito)
      continue;

    all_patterns_settings[std::make_pair(i->primary_pattern, i->source)]
        [i->secondary_pattern] = i->setting;
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<std::unique_ptr<base::DictionaryValue>>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  // |all_patterns_settings| is sorted from the lowest precedence pattern to
  // the highest (see operator< in ContentSettingsPattern), so traverse it in
  // reverse to show the patterns with the highest precedence (the more specific
  // ones) on the top.
  for (AllPatternsSettings::reverse_iterator i = all_patterns_settings.rbegin();
       i != all_patterns_settings.rend();
       ++i) {
    const ContentSettingsPattern& primary_pattern = i->first.first;
    const OnePatternSettings& one_settings = i->second;

    // The "parent" entry either has an identical primary and secondary pattern,
    // or has a wildcard secondary. The two cases are indistinguishable in the
    // UI.
    OnePatternSettings::const_iterator parent =
        one_settings.find(primary_pattern);
    if (parent == one_settings.end())
      parent = one_settings.find(ContentSettingsPattern::Wildcard());

    const std::string& source = i->first.second;
    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    // Add the "parent" entry for the non-embedded setting.
    ContentSetting parent_setting =
        parent == one_settings.end() ? CONTENT_SETTING_DEFAULT : parent->second;
    const ContentSettingsPattern& secondary_pattern =
        parent == one_settings.end() ? primary_pattern : parent->first;
    this_provider_exceptions.push_back(GetExceptionForPage(
        primary_pattern, secondary_pattern, parent_setting, source));

    // Add the "children" for any embedded settings.
    for (OnePatternSettings::const_iterator j = one_settings.begin();
         j != one_settings.end(); ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      ContentSetting content_setting = j->second;
      this_provider_exceptions.push_back(GetExceptionForPage(
          primary_pattern, j->first, content_setting, source));
    }
  }

  // For camera and microphone, we do not have policy exceptions, but we do have
  // the policy-set allowed URLs, which should be displayed in the same manner.
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    auto& policy_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(kPolicyProviderId)];
    DCHECK(policy_exceptions.empty());
    GetPolicyAllowedUrls(type, &policy_exceptions, web_ui);
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
}

void GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<std::unique_ptr<base::DictionaryValue>>* exceptions,
    content::WebUI* web_ui) {
  DCHECK(type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
         type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  PrefService* prefs = Profile::FromWebUI(web_ui)->GetPrefs();
  const base::ListValue* policy_urls = prefs->GetList(
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
          ? prefs::kAudioCaptureAllowedUrls
          : prefs::kVideoCaptureAllowedUrls);

  // Convert the URLs to |ContentSettingsPattern|s. Ignore any invalid ones.
  std::vector<ContentSettingsPattern> patterns;
  for (const auto& entry : *policy_urls) {
    std::string url;
    bool valid_string = entry->GetAsString(&url);
    if (!valid_string)
      continue;

    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(url);
    if (!pattern.IsValid())
      continue;

    patterns.push_back(pattern);
  }

  // The patterns are shown in the UI in a reverse order defined by
  // |ContentSettingsPattern::operator<|.
  std::sort(
      patterns.begin(), patterns.end(), std::greater<ContentSettingsPattern>());

  for (const ContentSettingsPattern& pattern : patterns) {
    exceptions->push_back(GetExceptionForPage(pattern, ContentSettingsPattern(),
                                              CONTENT_SETTING_ALLOW,
                                              kPolicyProviderId));
  }
}

const ChooserTypeNameEntry* ChooserTypeFromGroupName(const std::string& name) {
  for (const auto& chooser_type : kChooserTypeGroupNames) {
    if (chooser_type.name == name)
      return &chooser_type;
  }
  return nullptr;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a chooser permission exceptions table.
std::unique_ptr<base::DictionaryValue> GetChooserExceptionForPage(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const std::string& provider_name,
    const std::string& name,
    const base::DictionaryValue* object) {
  std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue());

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
  DCHECK(!setting_string.empty());

  exception->SetString(site_settings::kSetting, setting_string);
  exception->SetString(site_settings::kOrigin, requesting_origin.spec());
  exception->SetString(
      site_settings::kEmbeddingOrigin, embedding_origin.spec());
  exception->SetString(site_settings::kSource, provider_name);
  if (object) {
    exception->SetString(kObjectName, name);
    exception->Set(kObject, object->CreateDeepCopy());
  }
  return exception;
}

void GetChooserExceptionsFromProfile(
    Profile* profile,
    bool incognito,
    const ChooserTypeNameEntry& chooser_type,
    base::ListValue* exceptions) {
  if (incognito) {
    if (!profile->HasOffTheRecordProfile())
      return;
    profile = profile->GetOffTheRecordProfile();
  }

  ChooserContextBase* chooser_context = chooser_type.get_context(profile);
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      chooser_context->GetAllGrantedObjects();
  AllOriginObjects all_origin_objects;
  for (const auto& object : objects) {
    std::string name;
    bool found = object->object.GetString(chooser_type.ui_name_key, &name);
    DCHECK(found);
    // It is safe for this structure to hold references into |objects| because
    // they are both destroyed at the end of this function.
    all_origin_objects[make_pair(object->requesting_origin,
                                 object->source)][object->embedding_origin]
        .insert(make_pair(name, &object->object));
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<std::unique_ptr<base::DictionaryValue>>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  for (const auto& all_origin_objects_entry : all_origin_objects) {
    const GURL& requesting_origin = all_origin_objects_entry.first.first;
    const std::string& source = all_origin_objects_entry.first.second;
    const OneOriginObjects& one_origin_objects =
        all_origin_objects_entry.second;

    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    // Add entries for any non-embedded origins.
    bool has_embedded_entries = false;
    for (const auto& one_origin_objects_entry : one_origin_objects) {
      const GURL& embedding_origin = one_origin_objects_entry.first;
      const SortedObjects& sorted_objects = one_origin_objects_entry.second;

      // Skip the embedded settings which will be added below.
      if (requesting_origin != embedding_origin) {
        has_embedded_entries = true;
        continue;
      }

      for (const auto& sorted_objects_entry : sorted_objects) {
        this_provider_exceptions.push_back(GetChooserExceptionForPage(
            requesting_origin, embedding_origin, source,
            sorted_objects_entry.first, sorted_objects_entry.second));
      }
    }

    if (has_embedded_entries) {
      // Add a "parent" entry that simply acts as a heading for all entries
      // where |requesting_origin| has been embedded.
      this_provider_exceptions.push_back(
          GetChooserExceptionForPage(requesting_origin, requesting_origin,
                                     source, std::string(), nullptr));

      // Add the "children" for any embedded settings.
      for (const auto& one_origin_objects_entry : one_origin_objects) {
        const GURL& embedding_origin = one_origin_objects_entry.first;
        const SortedObjects& sorted_objects = one_origin_objects_entry.second;

        // Skip the non-embedded setting which we already added above.
        if (requesting_origin == embedding_origin)
          continue;

        for (const auto& sorted_objects_entry : sorted_objects) {
          this_provider_exceptions.push_back(GetChooserExceptionForPage(
              requesting_origin, embedding_origin, source,
              sorted_objects_entry.first, sorted_objects_entry.second));
        }
      }
    }
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
}

}  // namespace site_settings
