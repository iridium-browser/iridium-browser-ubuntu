// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/test/content_settings_test_utils.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

// static
base::Value* TestUtils::GetContentSettingValue(
    const ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool include_incognito) {
  return HostContentSettingsMap::GetContentSettingValueAndPatterns(
      provider, primary_url, secondary_url, content_type, resource_identifier,
      include_incognito, NULL, NULL).release();
}

// static
ContentSetting TestUtils::GetContentSetting(
    const ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool include_incognito) {
  std::unique_ptr<base::Value> value(
      GetContentSettingValue(provider, primary_url, secondary_url, content_type,
                             resource_identifier, include_incognito));
  return ValueToContentSetting(value.get());
}

// static
std::unique_ptr<base::Value> TestUtils::GetContentSettingValueAndPatterns(
    content_settings::RuleIterator* rule_iterator,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern) {
  return HostContentSettingsMap::GetContentSettingValueAndPatterns(
      rule_iterator, primary_url, secondary_url, primary_pattern,
      secondary_pattern);
}

}  // namespace content_settings
