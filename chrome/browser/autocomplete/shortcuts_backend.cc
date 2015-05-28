// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/shortcuts_backend.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "chrome/browser/autocomplete/shortcuts_database.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/chrome_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_match_type.h"
#include "components/omnibox/autocomplete_result.h"
#include "components/omnibox/base_search_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#endif

using content::BrowserThread;

namespace {

// Takes Match classification vector and removes all matched positions,
// compacting repetitions if necessary.
std::string StripMatchMarkers(const ACMatchClassifications& matches) {
  ACMatchClassifications unmatched;
  for (ACMatchClassifications::const_iterator i(matches.begin());
       i != matches.end(); ++i) {
    AutocompleteMatch::AddLastClassificationIfNecessary(
        &unmatched, i->offset, i->style & ~ACMatchClassification::MATCH);
  }
  return AutocompleteMatch::ClassificationsToString(unmatched);
}

// Normally shortcuts have the same match type as the original match they were
// created from, but for certain match types, we should modify the shortcut's
// type slightly to reflect that the origin of the shortcut is historical.
AutocompleteMatch::Type GetTypeForShortcut(AutocompleteMatch::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
      return AutocompleteMatchType::HISTORY_URL;

    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
      return type;

    default:
      return AutocompleteMatch::IsSearchType(type) ?
          AutocompleteMatchType::SEARCH_HISTORY : type;
  }
}

}  // namespace


// ShortcutsBackend -----------------------------------------------------------

ShortcutsBackend::ShortcutsBackend(Profile* profile, bool suppress_db)
    : profile_(profile),
      current_state_(NOT_INITIALIZED),
      history_service_observer_(this),
      no_db_access_(suppress_db) {
  if (!suppress_db) {
    db_ = new ShortcutsDatabase(
        profile->GetPath().Append(chrome::kShortcutsDatabaseName));
  }
  // |profile| can be NULL in tests.
  if (profile) {
#if defined(ENABLE_EXTENSIONS)
    notification_registrar_.Add(
        this,
        extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
        content::Source<Profile>(profile));
#endif
    history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (hs)
      history_service_observer_.Add(hs);
  }
}

bool ShortcutsBackend::Init() {
  if (current_state_ != NOT_INITIALIZED)
    return false;

  if (no_db_access_) {
    current_state_ = INITIALIZED;
    return true;
  }

  current_state_ = INITIALIZING;
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&ShortcutsBackend::InitInternal, this));
}

bool ShortcutsBackend::DeleteShortcutsWithURL(const GURL& shortcut_url) {
  return initialized() && DeleteShortcutsWithURL(shortcut_url, true);
}

void ShortcutsBackend::AddObserver(ShortcutsBackendObserver* obs) {
  observer_list_.AddObserver(obs);
}

void ShortcutsBackend::RemoveObserver(ShortcutsBackendObserver* obs) {
  observer_list_.RemoveObserver(obs);
}

void ShortcutsBackend::AddOrUpdateShortcut(const base::string16& text,
                                           const AutocompleteMatch& match) {
  const base::string16 text_lowercase(base::i18n::ToLower(text));
  const base::Time now(base::Time::Now());
  for (ShortcutMap::const_iterator it(
       shortcuts_map_.lower_bound(text_lowercase));
       it != shortcuts_map_.end() &&
           StartsWith(it->first, text_lowercase, true); ++it) {
    if (match.destination_url == it->second.match_core.destination_url) {
      UpdateShortcut(ShortcutsDatabase::Shortcut(
          it->second.id, text, MatchToMatchCore(match, profile_), now,
          it->second.number_of_hits + 1));
      return;
    }
  }
  AddShortcut(ShortcutsDatabase::Shortcut(
      base::GenerateGUID(), text, MatchToMatchCore(match, profile_), now, 1));
}

ShortcutsBackend::~ShortcutsBackend() {
}

// static
ShortcutsDatabase::Shortcut::MatchCore ShortcutsBackend::MatchToMatchCore(
    const AutocompleteMatch& match,
    Profile* profile) {
  const AutocompleteMatch::Type match_type = GetTypeForShortcut(match.type);
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  const AutocompleteMatch& normalized_match =
      AutocompleteMatch::IsSpecializedSearchType(match.type) ?
          BaseSearchProvider::CreateSearchSuggestion(
              match.search_terms_args->search_terms, match_type,
              (match.transition == ui::PAGE_TRANSITION_KEYWORD),
              match.GetTemplateURL(service, false),
              UIThreadSearchTermsData(profile)) :
          match;
  return ShortcutsDatabase::Shortcut::MatchCore(
      normalized_match.fill_into_edit, normalized_match.destination_url,
      normalized_match.contents,
      StripMatchMarkers(normalized_match.contents_class),
      normalized_match.description,
      StripMatchMarkers(normalized_match.description_class),
      normalized_match.transition, match_type, normalized_match.keyword);
}

void ShortcutsBackend::ShutdownOnUIThread() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_registrar_.RemoveAll();
  history_service_observer_.RemoveAll();
}

void ShortcutsBackend::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
#if defined(ENABLE_EXTENSIONS)
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED, type);
  if (!initialized())
    return;

  // When an extension is unloaded, we want to remove any Shortcuts associated
  // with it.
  DeleteShortcutsWithURL(content::Details<extensions::UnloadedExtensionInfo>(
                             details)->extension->url(),
                         false);
#endif
}

void ShortcutsBackend::OnURLsDeleted(history::HistoryService* history_service,
                                     bool all_history,
                                     bool expired,
                                     const history::URLRows& deleted_rows,
                                     const std::set<GURL>& favicon_urls) {
  if (!initialized())
    return;

  if (all_history) {
    DeleteAllShortcuts();
    return;
  }

  ShortcutsDatabase::ShortcutIDs shortcut_ids;
  for (const auto& guid_pair : guid_map_) {
    if (std::find_if(
            deleted_rows.begin(), deleted_rows.end(),
            history::URLRow::URLRowHasURL(
                guid_pair.second->second.match_core.destination_url)) !=
        deleted_rows.end()) {
      shortcut_ids.push_back(guid_pair.first);
    }
  }
  DeleteShortcutsWithIDs(shortcut_ids);
}

void ShortcutsBackend::InitInternal() {
  DCHECK(current_state_ == INITIALIZING);
  db_->Init();
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);
  temp_shortcuts_map_.reset(new ShortcutMap);
  temp_guid_map_.reset(new GuidMap);
  for (ShortcutsDatabase::GuidToShortcutMap::const_iterator it(
           shortcuts.begin());
       it != shortcuts.end(); ++it) {
    (*temp_guid_map_)[it->first] = temp_shortcuts_map_->insert(
        std::make_pair(base::i18n::ToLower(it->second.text), it->second));
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&ShortcutsBackend::InitCompleted, this));
}

void ShortcutsBackend::InitCompleted() {
  temp_guid_map_->swap(guid_map_);
  temp_shortcuts_map_->swap(shortcuts_map_);
  temp_shortcuts_map_.reset(NULL);
  temp_guid_map_.reset(NULL);
  current_state_ = INITIALIZED;
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsLoaded());
}

bool ShortcutsBackend::AddShortcut(
    const ShortcutsDatabase::Shortcut& shortcut) {
  if (!initialized())
    return false;
  DCHECK(guid_map_.find(shortcut.id) == guid_map_.end());
  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ ||
         BrowserThread::PostTask(
             BrowserThread::DB, FROM_HERE,
             base::Bind(base::IgnoreResult(&ShortcutsDatabase::AddShortcut),
                        db_.get(), shortcut));
}

bool ShortcutsBackend::UpdateShortcut(
    const ShortcutsDatabase::Shortcut& shortcut) {
  if (!initialized())
    return false;
  GuidMap::iterator it(guid_map_.find(shortcut.id));
  if (it != guid_map_.end())
    shortcuts_map_.erase(it->second);
  guid_map_[shortcut.id] = shortcuts_map_.insert(
      std::make_pair(base::i18n::ToLower(shortcut.text), shortcut));
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ ||
         BrowserThread::PostTask(
             BrowserThread::DB, FROM_HERE,
             base::Bind(base::IgnoreResult(&ShortcutsDatabase::UpdateShortcut),
                        db_.get(), shortcut));
}

bool ShortcutsBackend::DeleteShortcutsWithIDs(
    const ShortcutsDatabase::ShortcutIDs& shortcut_ids) {
  if (!initialized())
    return false;
  for (size_t i = 0; i < shortcut_ids.size(); ++i) {
    GuidMap::iterator it(guid_map_.find(shortcut_ids[i]));
    if (it != guid_map_.end()) {
      shortcuts_map_.erase(it->second);
      guid_map_.erase(it);
    }
  }
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ ||
         BrowserThread::PostTask(
             BrowserThread::DB, FROM_HERE,
             base::Bind(
                 base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithIDs),
                 db_.get(), shortcut_ids));
}

bool ShortcutsBackend::DeleteShortcutsWithURL(const GURL& url,
                                              bool exact_match) {
  const std::string& url_spec = url.spec();
  ShortcutsDatabase::ShortcutIDs shortcut_ids;
  for (GuidMap::iterator it(guid_map_.begin()); it != guid_map_.end(); ) {
    if (exact_match ?
        (it->second->second.match_core.destination_url == url) :
        StartsWithASCII(it->second->second.match_core.destination_url.spec(),
                        url_spec, true)) {
      shortcut_ids.push_back(it->first);
      shortcuts_map_.erase(it->second);
      guid_map_.erase(it++);
    } else {
      ++it;
    }
  }
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ ||
         BrowserThread::PostTask(
             BrowserThread::DB, FROM_HERE,
             base::Bind(
                 base::IgnoreResult(&ShortcutsDatabase::DeleteShortcutsWithURL),
                 db_.get(), url_spec));
}

bool ShortcutsBackend::DeleteAllShortcuts() {
  if (!initialized())
    return false;
  shortcuts_map_.clear();
  guid_map_.clear();
  FOR_EACH_OBSERVER(ShortcutsBackendObserver, observer_list_,
                    OnShortcutsChanged());
  return no_db_access_ ||
         BrowserThread::PostTask(
             BrowserThread::DB, FROM_HERE,
             base::Bind(
                 base::IgnoreResult(&ShortcutsDatabase::DeleteAllShortcuts),
                 db_.get()));
}
