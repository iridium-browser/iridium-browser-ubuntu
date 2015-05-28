// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_messages.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_platform_file.h"

using content::BrowserThread;
using chrome::spellcheck_common::WordList;

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  pref_change_registrar_.Init(prefs);

  std::string language_code;
  std::string country_code;
  chrome::spellcheck_common::GetISOLanguageCountryCodeFromLocale(
      prefs->GetString(prefs::kSpellCheckDictionary),
      &language_code,
      &country_code);
  feedback_sender_.reset(new spellcheck::FeedbackSender(
      context->GetRequestContext(), language_code, country_code));

  pref_change_registrar_.Add(
      prefs::kEnableAutoSpellCorrect,
      base::Bind(&SpellcheckService::OnEnableAutoSpellCorrectChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSpellCheckDictionary,
      base::Bind(&SpellcheckService::OnSpellCheckDictionaryChanged,
                 base::Unretained(this)));
 pref_change_registrar_.Add(
     prefs::kSpellCheckUseSpellingService,
     base::Bind(&SpellcheckService::OnUseSpellingServiceChanged,
                base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kEnableContinuousSpellcheck,
      base::Bind(&SpellcheckService::InitForAllRenderers,
                 base::Unretained(this)));

  OnSpellCheckDictionaryChanged();

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

// static
int SpellcheckService::GetSpellCheckLanguages(
    content::BrowserContext* context,
    std::vector<std::string>* languages) {
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  StringPrefMember accept_languages_pref;
  StringPrefMember dictionary_language_pref;
  accept_languages_pref.Init(prefs::kAcceptLanguages, prefs);
  dictionary_language_pref.Init(prefs::kSpellCheckDictionary, prefs);
  std::string dictionary_language = dictionary_language_pref.GetValue();

  // Now scan through the list of accept languages, and find possible mappings
  // from this list to the existing list of spell check languages.
  std::vector<std::string> accept_languages;

#if defined(OS_MACOSX)
  if (spellcheck_mac::SpellCheckerAvailable())
    spellcheck_mac::GetAvailableLanguages(&accept_languages);
  else
    base::SplitString(accept_languages_pref.GetValue(), ',', &accept_languages);
#else
  base::SplitString(accept_languages_pref.GetValue(), ',', &accept_languages);
#endif  // !OS_MACOSX

  GetSpellCheckLanguagesFromAcceptLanguages(
      accept_languages, dictionary_language, languages);

  for (size_t i = 0; i < languages->size(); ++i) {
    if ((*languages)[i] == dictionary_language)
      return i;
  }
  return -1;
}

// static
void SpellcheckService::GetSpellCheckLanguagesFromAcceptLanguages(
    const std::vector<std::string>& accept_languages,
    const std::string& dictionary_language,
    std::vector<std::string>* languages) {
  // The current dictionary language should be there.
  languages->push_back(dictionary_language);

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
}

// static
bool SpellcheckService::SignalStatusEvent(
    SpellcheckService::EventType status_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::BrowserContext* context = process->GetBrowserContext();
  if (SpellcheckServiceFactory::GetForContext(context) != this)
    return;

  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  IPC::PlatformFileForTransit file = IPC::InvalidPlatformFileForTransit();

  if (hunspell_dictionary_->GetDictionaryFile().IsValid()) {
    file = IPC::GetFileHandleForProcess(
        hunspell_dictionary_->GetDictionaryFile().GetPlatformFile(),
        process->GetHandle(), false);
  }

  process->Send(new SpellCheckMsg_Init(
      file,
      custom_dictionary_->GetWords(),
      hunspell_dictionary_->GetLanguage(),
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

SpellcheckHunspellDictionary* SpellcheckService::GetHunspellDictionary() {
  return hunspell_dictionary_.get();
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  g_status_event = status_event;
}

// static
SpellcheckService::EventType SpellcheckService::GetStatusEvent() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_status_type;
}

void SpellcheckService::InitForAllRenderers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

void SpellcheckService::OnSpellCheckDictionaryChanged() {
  if (hunspell_dictionary_.get())
    hunspell_dictionary_->RemoveObserver(this);
  PrefService* prefs = user_prefs::UserPrefs::Get(context_);
  DCHECK(prefs);

  std::string dictionary =
      prefs->GetString(prefs::kSpellCheckDictionary);
  hunspell_dictionary_.reset(new SpellcheckHunspellDictionary(
      dictionary, context_->GetRequestContext(), this));
  hunspell_dictionary_->AddObserver(this);
  hunspell_dictionary_->Load();
  std::string language_code;
  std::string country_code;
  chrome::spellcheck_common::GetISOLanguageCountryCodeFromLocale(
      dictionary, &language_code, &country_code);
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
