// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include "base/logging.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/supports_user_data.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/spellchecker/feedback_sender.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_platform.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_bdict_language.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;

// TODO(rlp): I do not like globals, but keeping these for now during
// transition.
// An event used by browser tests to receive status events from this class and
// its derived classes.
base::WaitableEvent* g_status_event = NULL;
SpellcheckService::EventType g_status_type =
    SpellcheckService::BDICT_NOTINITIALIZED;

SpellcheckService::SpellcheckService(content::BrowserContext* context)
    : context_(context),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  pref_change_registrar_.Init(prefs);
  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(prefs::kSpellCheckDictionaries, prefs);
  std::string first_of_dictionaries;
  if (!dictionaries_pref.GetValue().empty())
    first_of_dictionaries = dictionaries_pref.GetValue().front();

  // For preference migration, set the new preference kSpellCheckDictionaries
  // to be the same as the old kSpellCheckDictionary.
  StringPrefMember single_dictionary_pref;
  single_dictionary_pref.Init(prefs::kSpellCheckDictionary, prefs);
  std::string single_dictionary = single_dictionary_pref.GetValue();

  if (first_of_dictionaries.empty() && !single_dictionary.empty()) {
    first_of_dictionaries = single_dictionary;
    dictionaries_pref.SetValue(
        std::vector<std::string>(1, first_of_dictionaries));
  }

  single_dictionary_pref.SetValue("");

  // If a user goes from single language to multi-language spellchecking with
  // spellchecking disabled the dictionaries preference should be blanked.
  if (!prefs->GetBoolean(prefs::kEnableContinuousSpellcheck) &&
      chrome::spellcheck_common::IsMultilingualSpellcheckEnabled()) {
    dictionaries_pref.SetValue(std::vector<std::string>());
    prefs->SetBoolean(prefs::kEnableContinuousSpellcheck, true);
  }

  // If a user goes back to single language spellchecking make sure there is
  // only one language in the dictionaries preference.
  if (!chrome::spellcheck_common::IsMultilingualSpellcheckEnabled() &&
      dictionaries_pref.GetValue().size() > 1) {
    dictionaries_pref.SetValue(
        std::vector<std::string>(1, first_of_dictionaries));
  }

  std::string language_code;
  std::string country_code;
  chrome::spellcheck_common::GetISOLanguageCountryCodeFromLocale(
      first_of_dictionaries,
      &language_code,
      &country_code);
  feedback_sender_.reset(new spellcheck::FeedbackSender(
      context->GetRequestContext(), language_code, country_code));

  pref_change_registrar_.Add(
      prefs::kEnableAutoSpellCorrect,
      base::Bind(&SpellcheckService::OnEnableAutoSpellCorrectChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSpellCheckDictionaries,
      base::Bind(&SpellcheckService::OnSpellCheckDictionariesChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSpellCheckUseSpellingService,
      base::Bind(&SpellcheckService::OnUseSpellingServiceChanged,
                 base::Unretained(this)));

  pref_change_registrar_.Add(
      prefs::kEnableContinuousSpellcheck,
      base::Bind(&SpellcheckService::InitForAllRenderers,
                 base::Unretained(this)));

  OnSpellCheckDictionariesChanged();

  custom_dictionary_.reset(new SpellcheckCustomDictionary(context_->GetPath()));
  custom_dictionary_->AddObserver(this);
  custom_dictionary_->Load();

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
}

SpellcheckService::~SpellcheckService() {
  // Remove pref observers
  pref_change_registrar_.RemoveAll();
}

base::WeakPtr<SpellcheckService> SpellcheckService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if !defined(OS_MACOSX)
// static
size_t SpellcheckService::GetSpellCheckLanguages(
    base::SupportsUserData* context,
    std::vector<std::string>* languages) {
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  StringPrefMember accept_languages_pref;
  accept_languages_pref.Init(prefs::kAcceptLanguages, prefs);

  std::vector<std::string> accept_languages = base::SplitString(
      accept_languages_pref.GetValue(), ",",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  StringListPrefMember dictionaries_pref;
  dictionaries_pref.Init(prefs::kSpellCheckDictionaries, prefs);
  *languages = dictionaries_pref.GetValue();
  size_t enabled_spellcheck_languages = languages->size();

  for (std::vector<std::string>::const_iterator i = accept_languages.begin();
       i != accept_languages.end(); ++i) {
    std::string language =
        chrome::spellcheck_common::GetCorrespondingSpellCheckLanguage(*i);
    if (!language.empty() &&
        std::find(languages->begin(), languages->end(), language) ==
            languages->end()) {
      languages->push_back(language);
    }
  }

  return enabled_spellcheck_languages;
}
#endif  // !OS_MACOSX

// static
bool SpellcheckService::SignalStatusEvent(
    SpellcheckService::EventType status_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_status_event)
    return false;
  g_status_type = status_type;
  g_status_event->Signal();
  return true;
}

void SpellcheckService::StartRecordingMetrics(bool spellcheck_enabled) {
  metrics_.reset(new SpellCheckHostMetrics());
  metrics_->RecordEnabledStats(spellcheck_enabled);
  OnUseSpellingServiceChanged();
}

void SpellcheckService::InitForRenderer(content::RenderProcessHost* process) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* context = process->GetBrowserContext();
  if (SpellcheckServiceFactory::GetForContext(context) != this)
    return;

  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  std::vector<SpellCheckBDictLanguage> bdict_languages;

  for (const auto& hunspell_dictionary : hunspell_dictionaries_) {
    bdict_languages.push_back(SpellCheckBDictLanguage());
    bdict_languages.back().language = hunspell_dictionary->GetLanguage();
    bdict_languages.back().file =
        hunspell_dictionary->GetDictionaryFile().IsValid()
            ? IPC::GetFileHandleForProcess(
                  hunspell_dictionary->GetDictionaryFile().GetPlatformFile(),
                  process->GetHandle(), false)
            : IPC::InvalidPlatformFileForTransit();
  }

  process->Send(new SpellCheckMsg_Init(
      bdict_languages, custom_dictionary_->GetWords(),
      prefs->GetBoolean(prefs::kEnableAutoSpellCorrect)));
  process->Send(new SpellCheckMsg_EnableSpellCheck(
      prefs->GetBoolean(prefs::kEnableContinuousSpellcheck)));
}

SpellCheckHostMetrics* SpellcheckService::GetMetrics() const {
  return metrics_.get();
}

SpellcheckCustomDictionary* SpellcheckService::GetCustomDictionary() {
  return custom_dictionary_.get();
}

const ScopedVector<SpellcheckHunspellDictionary>&
SpellcheckService::GetHunspellDictionaries() {
  return hunspell_dictionaries_;
}

spellcheck::FeedbackSender* SpellcheckService::GetFeedbackSender() {
  return feedback_sender_.get();
}

bool SpellcheckService::LoadExternalDictionary(std::string language,
                                               std::string locale,
                                               std::string path,
                                               DictionaryFormat format) {
  return false;
}

bool SpellcheckService::UnloadExternalDictionary(std::string path) {
  return false;
}

void SpellcheckService::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  InitForRenderer(process);
}

void SpellcheckService::OnCustomDictionaryLoaded() {
  InitForAllRenderers();
}

void SpellcheckService::OnCustomDictionaryChanged(
    const SpellcheckCustomDictionary::Change& dictionary_change) {
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new SpellCheckMsg_CustomDictionaryChanged(
        dictionary_change.to_add(),
        dictionary_change.to_remove()));
  }
}

void SpellcheckService::OnHunspellDictionaryInitialized() {
  InitForAllRenderers();
}

void SpellcheckService::OnHunspellDictionaryDownloadBegin() {
}

void SpellcheckService::OnHunspellDictionaryDownloadSuccess() {
}

void SpellcheckService::OnHunspellDictionaryDownloadFailure() {
}

// static
void SpellcheckService::AttachStatusEvent(base::WaitableEvent* status_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  g_status_event = status_event;
}

// static
SpellcheckService::EventType SpellcheckService::GetStatusEvent() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_status_type;
}

void SpellcheckService::InitForAllRenderers() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* process = i.GetCurrentValue();
    if (process && process->GetHandle())
      InitForRenderer(process);
  }
}

void SpellcheckService::OnEnableAutoSpellCorrectChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      prefs::kEnableAutoSpellCorrect);
  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* process = i.GetCurrentValue();
    process->Send(new SpellCheckMsg_EnableAutoSpellCorrect(enabled));
  }
}

void SpellcheckService::OnSpellCheckDictionariesChanged() {
  for (auto& hunspell_dictionary : hunspell_dictionaries_)
    hunspell_dictionary->RemoveObserver(this);

  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  DCHECK(prefs);

  const base::ListValue* dictionary_values =
      prefs->GetList(prefs::kSpellCheckDictionaries);

  hunspell_dictionaries_.clear();
  for (const base::Value* dictionary_value : *dictionary_values) {
    std::string dictionary;
    dictionary_value->GetAsString(&dictionary);
    hunspell_dictionaries_.push_back(new SpellcheckHunspellDictionary(
        dictionary, context_->GetRequestContext(), this));
    hunspell_dictionaries_.back()->AddObserver(this);
    hunspell_dictionaries_.back()->Load();
  }

  std::string feedback_language;
  dictionary_values->GetString(0, &feedback_language);
  std::string language_code;
  std::string country_code;
  chrome::spellcheck_common::GetISOLanguageCountryCodeFromLocale(
      feedback_language, &language_code, &country_code);
  feedback_sender_->OnLanguageCountryChange(language_code, country_code);
  UpdateFeedbackSenderState();
}

void SpellcheckService::OnUseSpellingServiceChanged() {
  bool enabled = pref_change_registrar_.prefs()->GetBoolean(
      prefs::kSpellCheckUseSpellingService);
  if (metrics_)
    metrics_->RecordSpellingServiceStats(enabled);
  UpdateFeedbackSenderState();
}

void SpellcheckService::UpdateFeedbackSenderState() {
  if (SpellingServiceClient::IsAvailable(
          context_, SpellingServiceClient::SPELLCHECK)) {
    feedback_sender_->StartFeedbackCollection();
  } else {
    feedback_sender_->StopFeedbackCollection();
  }
}
