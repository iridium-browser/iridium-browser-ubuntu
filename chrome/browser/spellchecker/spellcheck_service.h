// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/spellchecker/feedback_sender.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "chrome/common/spellcheck_common.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;
class SpellCheckHostMetrics;

namespace base {
class WaitableEvent;
}

namespace content {
class RenderProcessHost;
class BrowserContext;
}

// Encapsulates the browser side spellcheck service. There is one of these per
// profile and each is created by the SpellCheckServiceFactory.  The
// SpellcheckService maintains any per-profile information about spellcheck.
class SpellcheckService : public KeyedService,
                          public content::NotificationObserver,
                          public SpellcheckCustomDictionary::Observer,
                          public SpellcheckHunspellDictionary::Observer {
 public:
  // Event types used for reporting the status of this class and its derived
  // classes to browser tests.
  enum EventType {
    BDICT_NOTINITIALIZED,
    BDICT_CORRUPTED,
  };

  // Dictionary format used for loading an external dictionary.
  enum DictionaryFormat {
    DICT_HUNSPELL,
    DICT_TEXT,
    DICT_UNKNOWN,
  };

  explicit SpellcheckService(content::BrowserContext* context);
  ~SpellcheckService() override;

  base::WeakPtr<SpellcheckService> GetWeakPtr();

  // This function computes a vector of strings which are to be displayed in
  // the context menu over a text area for changing spell check languages. It
  // returns the index of the current spell check language in the vector.
  // TODO(port): this should take a vector of base::string16, but the
  // implementation has some dependencies in l10n util that need porting first.
  static int GetSpellCheckLanguages(content::BrowserContext* context,
                                    std::vector<std::string>* languages);

  // Computes a vector of strings which are to be displayed in the context
  // menu from |accept_languages| and |dictionary_language|.
  static void GetSpellCheckLanguagesFromAcceptLanguages(
      const std::vector<std::string>& accept_languages,
      const std::string& dictionary_language,
      std::vector<std::string>* languages);

  // Signals the event attached by AttachTestEvent() to report the specified
  // event to browser tests. This function is called by this class and its
  // derived classes to report their status. This function does not do anything
  // when we do not set an event to |status_event_|.
  static bool SignalStatusEvent(EventType type);

  // Instantiates SpellCheckHostMetrics object and makes it ready for recording
  // metrics. This should be called only if the metrics recording is active.
  void StartRecordingMetrics(bool spellcheck_enabled);

  // Pass the renderer some basic initialization information. Note that the
  // renderer will not load Hunspell until it needs to.
  void InitForRenderer(content::RenderProcessHost* process);

  // Returns a metrics counter associated with this object,
  // or null when metrics recording is disabled.
  SpellCheckHostMetrics* GetMetrics() const;

  // Returns the instance of the custom dictionary.
  SpellcheckCustomDictionary* GetCustomDictionary();

  // Returns the instance of the Hunspell dictionary.
  SpellcheckHunspellDictionary* GetHunspellDictionary();

  // Returns the instance of the spelling service feedback sender.
  spellcheck::FeedbackSender* GetFeedbackSender();

  // Load a dictionary from a given path. Format specifies how the dictionary
  // is stored. Return value is true if successful.
  bool LoadExternalDictionary(std::string language,
                              std::string locale,
                              std::string path,
                              DictionaryFormat format);

  // Unload a dictionary. The path is given to identify the dictionary.
  // Return value is true if successful.
  bool UnloadExternalDictionary(std::string path);

  // NotificationProfile implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // SpellcheckCustomDictionary::Observer implementation.
  void OnCustomDictionaryLoaded() override;
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& dictionary_change) override;

  // SpellcheckHunspellDictionary::Observer implementation.
  void OnHunspellDictionaryInitialized() override;
  void OnHunspellDictionaryDownloadBegin() override;
  void OnHunspellDictionaryDownloadSuccess() override;
  void OnHunspellDictionaryDownloadFailure() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT);

  // Attaches an event so browser tests can listen the status events.
  static void AttachStatusEvent(base::WaitableEvent* status_event);

  // Returns the status event type.
  static EventType GetStatusEvent();

  // Pass all renderers some basic initialization information.
  void InitForAllRenderers();

  // Reacts to a change in user preferences on whether auto-spell-correct should
  // be enabled.
  void OnEnableAutoSpellCorrectChanged();

  // Reacts to a change in user preference on which language should be used for
  // spellchecking.
  void OnSpellCheckDictionaryChanged();

  // Notification handler for changes to prefs::kSpellCheckUseSpellingService.
  void OnUseSpellingServiceChanged();

  // Enables the feedback sender if spelling server is available and enabled.
  // Otherwise disables the feedback sender.
  void UpdateFeedbackSenderState();

  PrefChangeRegistrar pref_change_registrar_;
  content::NotificationRegistrar registrar_;

  // A pointer to the BrowserContext which this service refers to.
  content::BrowserContext* context_;

  scoped_ptr<SpellCheckHostMetrics> metrics_;

  scoped_ptr<SpellcheckCustomDictionary> custom_dictionary_;

  scoped_ptr<SpellcheckHunspellDictionary> hunspell_dictionary_;

  scoped_ptr<spellcheck::FeedbackSender> feedback_sender_;

  base::WeakPtrFactory<SpellcheckService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckService);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_
