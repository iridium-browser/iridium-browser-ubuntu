// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_source.h"

namespace extensions {

namespace language_settings_private = api::language_settings_private;

LanguageSettingsPrivateDelegate::LanguageSettingsPrivateDelegate(
    content::BrowserContext* context)
    : custom_dictionary_(nullptr),
      context_(context),
      listening_spellcheck_(false),
      profile_added_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router)
    return;

  event_router->RegisterObserver(this,
      language_settings_private::OnSpellcheckDictionariesChanged::kEventName);
  event_router->RegisterObserver(this,
      language_settings_private::OnCustomDictionaryChanged::kEventName);

  // SpellcheckService cannot be created until Profile::DoFinalInit() has been
  // called. http://crbug.com/171406
  notification_registrar_.Add(this,
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(Profile::FromBrowserContext(context_)));

  pref_change_registrar_.Init(Profile::FromBrowserContext(context_)->
      GetPrefs());

  StartOrStopListeningForSpellcheckChanges();
}

LanguageSettingsPrivateDelegate::~LanguageSettingsPrivateDelegate() {
  DCHECK(!listening_spellcheck_);
  pref_change_registrar_.RemoveAll();
  notification_registrar_.RemoveAll();
}

LanguageSettingsPrivateDelegate* LanguageSettingsPrivateDelegate::Create(
    content::BrowserContext* context) {
  return new LanguageSettingsPrivateDelegate(context);
}

ScopedVector<language_settings_private::SpellcheckDictionaryStatus>
LanguageSettingsPrivateDelegate::GetHunspellDictionaryStatuses() {
  ScopedVector<language_settings_private::SpellcheckDictionaryStatus> statuses;
  for (const auto& dictionary : GetHunspellDictionaries()) {
    if (!dictionary)
      continue;
    scoped_ptr<language_settings_private::SpellcheckDictionaryStatus> status(
        new language_settings_private::SpellcheckDictionaryStatus());
    status->language_code = dictionary->GetLanguage();
    status->is_ready = dictionary->IsReady();
    if (!status->is_ready) {
      if (dictionary->IsDownloadInProgress())
        status->is_downloading.reset(new bool(true));
      if (dictionary->IsDownloadFailure())
        status->download_failed.reset(new bool(true));
    }
    statuses.push_back(status.Pass());
  }
  return statuses.Pass();
}

void LanguageSettingsPrivateDelegate::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all context services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_spellcheck_) {
    RemoveDictionaryObservers();
    listening_spellcheck_ = false;
  }
}

void LanguageSettingsPrivateDelegate::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to spellcheck change events.
  if (details.event_name ==
      language_settings_private::OnSpellcheckDictionariesChanged::kEventName ||
      details.event_name ==
      language_settings_private::OnCustomDictionaryChanged::kEventName) {
    StartOrStopListeningForSpellcheckChanges();
  }
}

void LanguageSettingsPrivateDelegate::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events if there are no more listeners.
  StartOrStopListeningForSpellcheckChanges();
}

void LanguageSettingsPrivateDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  profile_added_ = true;
  StartOrStopListeningForSpellcheckChanges();
}

void LanguageSettingsPrivateDelegate::OnHunspellDictionaryInitialized() {
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::OnHunspellDictionaryDownloadBegin() {
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::OnHunspellDictionaryDownloadSuccess() {
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::OnHunspellDictionaryDownloadFailure() {
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::OnCustomDictionaryLoaded() {
}

void LanguageSettingsPrivateDelegate::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& change) {
  std::vector<std::string> to_add(change.to_add().begin(),
                                  change.to_add().end());
  std::vector<std::string> to_remove(change.to_remove().begin(),
                                     change.to_remove().end());
  scoped_ptr<base::ListValue> args(
      language_settings_private::OnCustomDictionaryChanged::Create(
          to_add, to_remove));
  scoped_ptr<Event> extension_event(new Event(
      events::LANGUAGE_SETTINGS_PRIVATE_ON_CUSTOM_DICTIONARY_CHANGED,
      language_settings_private::OnCustomDictionaryChanged::kEventName,
      args.Pass()));
  EventRouter::Get(context_)->BroadcastEvent(extension_event.Pass());
}

void LanguageSettingsPrivateDelegate::RefreshDictionaries(
    bool was_listening, bool should_listen) {
  if (!profile_added_)
    return;
  if (was_listening)
    RemoveDictionaryObservers();
  hunspell_dictionaries_.clear();
  SpellcheckService* service = SpellcheckServiceFactory::GetForContext(
      context_);
  if (!custom_dictionary_)
    custom_dictionary_ = service->GetCustomDictionary();

  const ScopedVector<SpellcheckHunspellDictionary>& dictionaries(
      service->GetHunspellDictionaries());
  for (const auto& dictionary: dictionaries) {
    hunspell_dictionaries_.push_back(dictionary->AsWeakPtr());
    if (should_listen)
      dictionary->AddObserver(this);
  }
}

const LanguageSettingsPrivateDelegate::WeakDictionaries&
LanguageSettingsPrivateDelegate::GetHunspellDictionaries() {
  // If there are no hunspell dictionaries, or the first is invalid, refresh.
  if (!hunspell_dictionaries_.size() || !hunspell_dictionaries_.front())
    RefreshDictionaries(listening_spellcheck_, listening_spellcheck_);
  return hunspell_dictionaries_;
}

void LanguageSettingsPrivateDelegate::
    StartOrStopListeningForSpellcheckChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen =
      event_router->HasEventListener(language_settings_private::
          OnSpellcheckDictionariesChanged::kEventName) ||
      event_router->HasEventListener(language_settings_private::
          OnCustomDictionaryChanged::kEventName);

  if (should_listen && !listening_spellcheck_) {
    // Update and observe the hunspell dictionaries.
    RefreshDictionaries(listening_spellcheck_, should_listen);
    // Observe the dictionaries preference.
    pref_change_registrar_.Add(prefs::kSpellCheckDictionaries, base::Bind(
        &LanguageSettingsPrivateDelegate::OnSpellcheckDictionariesChanged,
        base::Unretained(this)));
    // Observe the dictionary of custom words.
    if (custom_dictionary_)
      custom_dictionary_->AddObserver(this);
  } else if (!should_listen && listening_spellcheck_) {
    // Stop observing any dictionaries that still exist.
    RemoveDictionaryObservers();
    hunspell_dictionaries_.clear();
    pref_change_registrar_.Remove(prefs::kSpellCheckDictionaries);
    if (custom_dictionary_)
      custom_dictionary_->RemoveObserver(this);
  }

  listening_spellcheck_ = should_listen;
}

void LanguageSettingsPrivateDelegate::OnSpellcheckDictionariesChanged() {
  RefreshDictionaries(listening_spellcheck_, listening_spellcheck_);
  BroadcastDictionariesChangedEvent();
}

void LanguageSettingsPrivateDelegate::BroadcastDictionariesChangedEvent() {
  std::vector<linked_ptr<language_settings_private::SpellcheckDictionaryStatus>>
      broadcast_statuses;
  ScopedVector<language_settings_private::SpellcheckDictionaryStatus> statuses =
      GetHunspellDictionaryStatuses();

  for (language_settings_private::SpellcheckDictionaryStatus* status : statuses)
    broadcast_statuses.push_back(make_linked_ptr(status));
  statuses.weak_clear();

  scoped_ptr<base::ListValue> args(
      language_settings_private::OnSpellcheckDictionariesChanged::Create(
          broadcast_statuses));
  scoped_ptr<extensions::Event> extension_event(new extensions::Event(
      events::LANGUAGE_SETTINGS_PRIVATE_ON_SPELLCHECK_DICTIONARIES_CHANGED,
      language_settings_private::OnSpellcheckDictionariesChanged::kEventName,
      args.Pass()));
  EventRouter::Get(context_)->BroadcastEvent(extension_event.Pass());
}

void LanguageSettingsPrivateDelegate::RemoveDictionaryObservers() {
  for (const auto& dictionary : hunspell_dictionaries_) {
    if (dictionary)
      dictionary->RemoveObserver(this);
  }
}

}  // namespace extensions
