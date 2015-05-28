// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OVERRIDE_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OVERRIDE_PROVIDER_H_

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/content_settings/core/browser/content_settings_binary_value_map.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/common/content_settings_types.h"

class ContentSettingsPattern;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

// OverrideProvider contains if certain content settings are enabled or
// globally disabled. It may only be written to using the UI thread, but may be
// read on any thread.
class OverrideProvider : public ProviderInterface {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  OverrideProvider(PrefService* prefs, bool incognito);
  ~OverrideProvider() override;

  // ProviderInterface implementations.
  RuleIterator* GetRuleIterator(ContentSettingsType content_type,
                                const ResourceIdentifier& resource_identifier,
                                bool incognito) const override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;

  bool SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const ResourceIdentifier& resource_identifier,
                         base::Value* value) override;

  void ShutdownOnUIThread() override;

  // Sets if a given |content_type| is |enabled|.
  void SetOverrideSetting(ContentSettingsType content_type, bool enabled);

  // Returns if |content_type| is enabled. If it is not enabled, the content
  // setting type is considered globally disabled and acts as though it is
  // blocked. If it is enabled, the content setting type's permission is granted
  // by the other providers.
  bool IsEnabled(ContentSettingsType content_type) const;

 private:
  // Reads the override settings from the preferences service.
  void ReadOverrideSettings();

  // Copies of the pref data, so that we can read it on the IO thread.
  BinaryValueMap allowed_settings_;

  PrefService* prefs_;

  bool is_incognito_;

  // Used around accesses to the |override_content_settings_| object to
  // guarantee thread safety.
  mutable base::Lock lock_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(OverrideProvider);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_OVERRIDE_PROVIDER_H_
