// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_

// A content settings provider that takes its settings out of the pref service.

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_change_registrar.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_utils.h"

class PrefService;

namespace base {
class Clock;
class DictionaryValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content_settings {

class ContentSettingsPref;

// Content settings provider that provides content settings from the user
// preference.
class PrefProvider : public ObservableProvider {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  PrefProvider(PrefService* prefs, bool incognito);
  ~PrefProvider() override;

  // ProviderInterface implementations.
  RuleIterator* GetRuleIterator(ContentSettingsType content_type,
                                const ResourceIdentifier& resource_identifier,
                                bool incognito) const override;

  bool SetWebsiteSetting(const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern,
                         ContentSettingsType content_type,
                         const ResourceIdentifier& resource_identifier,
                         base::Value* value) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;

  void ShutdownOnUIThread() override;

  // Records the last time the given pattern has used a certain content setting.
  void UpdateLastUsage(const ContentSettingsPattern& primary_pattern,
                       const ContentSettingsPattern& secondary_pattern,
                       ContentSettingsType content_type);

  base::Time GetLastUsage(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          ContentSettingsType content_type);

  void Notify(const ContentSettingsPattern& primary_pattern,
              const ContentSettingsPattern& secondary_pattern,
              ContentSettingsType content_type,
              const std::string& resource_identifier);

  // Gains ownership of |clock|.
  void SetClockForTesting(scoped_ptr<base::Clock> clock);

 private:
  friend class DeadlockCheckerObserver;  // For testing.

  // Clean up the obsolete preferences from the user's profile.
  void DiscardObsoletePreferences();

  // Weak; owned by the Profile and reset in ShutdownOnUIThread.
  PrefService* prefs_;

  // Can be set for testing.
  scoped_ptr<base::Clock> clock_;

  bool is_incognito_;

  PrefChangeRegistrar pref_change_registrar_;

  ScopedVector<ContentSettingsPref> content_settings_prefs_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PrefProvider);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_PREF_PROVIDER_H_
