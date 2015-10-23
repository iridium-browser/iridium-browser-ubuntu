// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <map>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_registry.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {

// Obsolete prefs.
// TODO(msramek): Remove the cleanup code after two releases (i.e. in M48).
const char kObsoleteContentSettingsPatternPairs[] =
    "profile.content_settings.pattern_pairs";
const char kObsoleteMigratedContentSettingsPatternPairs[] =
    "profile.migrated_content_settings_exceptions";

}  // namespace

namespace content_settings {

// ////////////////////////////////////////////////////////////////////////////
// PrefProvider:
//

// static
void PrefProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kContentSettingsVersion,
      ContentSettingsPattern::kContentSettingsPatternVersion);

  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    const WebsiteSettingsInfo* info =
        website_settings->Get(static_cast<ContentSettingsType>(i));
    registry->RegisterDictionaryPref(info->pref_name(),
                                     info->GetPrefRegistrationFlags());
  }

  // Obsolete prefs ----------------------------------------------------------

  registry->RegisterDictionaryPref(
      kObsoleteContentSettingsPatternPairs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(kObsoleteMigratedContentSettingsPatternPairs,
                                false);
}

PrefProvider::PrefProvider(PrefService* prefs, bool incognito)
    : prefs_(prefs),
      clock_(new base::DefaultClock()),
      is_incognito_(incognito) {
  DCHECK(prefs_);
  // Verify preferences version.
  if (!prefs_->HasPrefPath(prefs::kContentSettingsVersion)) {
    prefs_->SetInteger(prefs::kContentSettingsVersion,
                       ContentSettingsPattern::kContentSettingsPatternVersion);
  }
  if (prefs_->GetInteger(prefs::kContentSettingsVersion) >
      ContentSettingsPattern::kContentSettingsPatternVersion) {
    return;
  }

  pref_change_registrar_.Init(prefs_);

  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    content_settings_prefs_.push_back(new ContentSettingsPref(
        ContentSettingsType(i), prefs_, &pref_change_registrar_,
        website_settings->Get(static_cast<ContentSettingsType>(i))->pref_name(),
        is_incognito_,
        base::Bind(&PrefProvider::Notify, base::Unretained(this))));
  }

  if (!is_incognito_) {
    size_t num_exceptions = 0;
    for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
      num_exceptions += content_settings_prefs_[i]->GetNumExceptions();

    UMA_HISTOGRAM_COUNTS("ContentSettings.NumberOfExceptions",
                         num_exceptions);
  }

  DiscardObsoletePreferences();
}

PrefProvider::~PrefProvider() {
  DCHECK(!prefs_);
}

RuleIterator* PrefProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return content_settings_prefs_[content_type]->GetRuleIterator(
      resource_identifier,
      incognito);
}

bool PrefProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* in_value) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  // Default settings are set using a wildcard pattern for both
  // |primary_pattern| and |secondary_pattern|. Don't store default settings in
  // the |PrefProvider|. The |PrefProvider| handles settings for specific
  // sites/origins defined by the |primary_pattern| and the |secondary_pattern|.
  // Default settings are handled by the |DefaultProvider|.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard() &&
      resource_identifier.empty()) {
    return false;
  }

  return content_settings_prefs_[content_type]->SetWebsiteSetting(
      primary_pattern,
      secondary_pattern,
      resource_identifier,
      in_value);
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  content_settings_prefs_[content_type]->ClearAllContentSettingsRules();
}

void PrefProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void PrefProvider::UpdateLastUsage(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  content_settings_prefs_[content_type]->UpdateLastUsage(primary_pattern,
                                                         secondary_pattern,
                                                         clock_.get());
}

base::Time PrefProvider::GetLastUsage(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  return content_settings_prefs_[content_type]->GetLastUsage(primary_pattern,
                                                             secondary_pattern);
}

// ////////////////////////////////////////////////////////////////////////////
// Private

void PrefProvider::Notify(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  NotifyObservers(primary_pattern,
                  secondary_pattern,
                  content_type,
                  resource_identifier);
}

void PrefProvider::SetClockForTesting(scoped_ptr<base::Clock> clock) {
  clock_ = clock.Pass();
}

void PrefProvider::DiscardObsoletePreferences() {
  prefs_->ClearPref(kObsoleteContentSettingsPatternPairs);
  prefs_->ClearPref(kObsoleteMigratedContentSettingsPatternPairs);
}

}  // namespace content_settings
