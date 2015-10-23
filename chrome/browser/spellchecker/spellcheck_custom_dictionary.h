// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/spellchecker/spellcheck_dictionary.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_merge_result.h"
#include "sync/api/syncable_service.h"

namespace syncer {
class SyncErrorFactory;
class SyncChangeProcessor;
}

namespace tracked_objects {
class Location;
}

// Defines a custom dictionary where users can add their own words. All words
// must be UTF8, between 1 and 99 bytes long, and without leading or trailing
// ASCII whitespace. The dictionary contains its own checksum when saved on
// disk. Example dictionary file contents:
//
//   bar
//   foo
//   checksum_v1 = ec3df4034567e59e119fcf87f2d9bad4
//
class SpellcheckCustomDictionary : public SpellcheckDictionary,
                                   public syncer::SyncableService {
 public:
  // A change to the dictionary.
  class Change {
   public:
    Change();
    ~Change();

    // Adds |word| in this change.
    void AddWord(const std::string& word);

    // Adds |words| in this change.
    void AddWords(const std::set<std::string>& words);

    // Removes |word| in this change.
    void RemoveWord(const std::string& word);

    // Prepares this change to be applied to |words| by removing duplicate and
    // invalid words from words to be added and removing missing words from
    // words to be removed. Returns a bitmap of |ChangeSanitationResult| values.
    int Sanitize(const std::set<std::string>& words);

    // Returns the words to be added in this change.
    const std::set<std::string>& to_add() const { return to_add_; }

    // Returns the words to be removed in this change.
    const std::set<std::string>& to_remove() const {
      return to_remove_;
    }

    // Returns true if there are no changes to be made. Otherwise returns false.
    bool empty() const { return to_add_.empty() && to_remove_.empty(); }

   private:
    // The words to be added.
    std::set<std::string> to_add_;

    // The words to be removed.
    std::set<std::string> to_remove_;

    DISALLOW_COPY_AND_ASSIGN(Change);
  };

  // Interface to implement for dictionary load and change observers.
  class Observer {
   public:
    // Called when the custom dictionary has been loaded.
    virtual void OnCustomDictionaryLoaded() = 0;

    // Called when the custom dictionary has been changed.
    virtual void OnCustomDictionaryChanged(const Change& dictionary_change) = 0;
  };

  // The dictionary will be saved in |dictionary_directory_name|.
  explicit SpellcheckCustomDictionary(
      const base::FilePath& dictionary_directory_name);
  ~SpellcheckCustomDictionary() override;

  // Returns the in-memory cache of words in the custom dictionary.
  const std::set<std::string>& GetWords() const;

  // Adds |word| to the dictionary, schedules a write to disk, and notifies
  // observers of the change. Returns true if |word| is valid and not a
  // duplicate. Otherwise returns false.
  bool AddWord(const std::string& word);

  // Removes |word| from the dictionary, schedules a write to disk, and notifies
  // observers of the change. Returns true if |word| was found. Otherwise
  // returns false.
  bool RemoveWord(const std::string& word);

  // Returns true if the dictionary contains |word|. Otherwise returns false.
  bool HasWord(const std::string& word) const;

  // Adds |observer| to be notified of dictionary events and changes.
  void AddObserver(Observer* observer);

  // Removes |observer| to stop notifications of dictionary events and changes.
  void RemoveObserver(Observer* observer);

  // Returns true if the dictionary has been loaded. Otherwise returns false.
  bool IsLoaded();

  // Returns true if the dictionary is being synced. Otherwise returns false.
  bool IsSyncing();

  // Overridden from SpellcheckDictionary:
  void Load() override;

  // Overridden from syncer::SyncableService:
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

 private:
  friend class DictionarySyncIntegrationTestHelper;
  friend class SpellcheckCustomDictionaryTest;

  // Returns the list of words in the custom spellcheck dictionary at |path|.
  // Makes sure that the custom dictionary file does not have duplicates and
  // contains only valid words. Must be called on the FILE thread. The caller
  // owns the result.
  static scoped_ptr<std::set<std::string>> LoadDictionaryFile(
      const base::FilePath& path);

  // Applies the change in |dictionary_change| to the custom spellcheck
  // dictionary. Assumes that |dictionary_change| has been sanitized. Must be
  // called on the FILE thread. Takes ownership of |dictionary_change|.
  static void UpdateDictionaryFile(scoped_ptr<Change> dictionary_change,
                                   const base::FilePath& path);

  // The reply point for PostTaskAndReplyWithResult, called when
  // LoadDictionaryFile finishes reading the dictionary file. Takes ownership of
  // |custom_words|.
  void OnLoaded(scoped_ptr<std::set<std::string>> custom_words);

  // Applies the |dictionary_change| to the in-memory copy of the dictionary.
  void Apply(const Change& dictionary_change);

  // Schedules a write of |dictionary_change| to disk. Takes ownership of
  // |dictionary_change| to pass it to the FILE thread.
  void Save(scoped_ptr<Change> dictionary_change);

  // Notifies the sync service of the |dictionary_change|. Syncs up to the
  // maximum syncable words on the server. Disables syncing of this dictionary
  // if the server contains the maximum number of syncable words.
  syncer::SyncError Sync(const Change& dictionary_change);

  // Notifies observers of the dictionary change if the dictionary has been
  // changed.
  void Notify(const Change& dictionary_change);

  // In-memory cache of the custom words file.
  std::set<std::string> words_;

  // The path to the custom dictionary file.
  base::FilePath custom_dictionary_path_;

  // Observers for dictionary load and content changes.
  base::ObserverList<Observer> observers_;

  // Used to send local changes to the sync infrastructure.
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Used to send sync-related errors to the sync infrastructure.
  scoped_ptr<syncer::SyncErrorFactory> sync_error_handler_;

  // True if the dictionary has been loaded. Otherwise false.
  bool is_loaded_;

  // Used to create weak pointers for an instance of this class.
  base::WeakPtrFactory<SpellcheckCustomDictionary> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckCustomDictionary);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_
