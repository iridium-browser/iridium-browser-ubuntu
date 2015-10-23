// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include <functional>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/md5.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/spellcheck_common.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

using content::BrowserThread;

namespace {

// Filename extension for backup dictionary file.
const base::FilePath::CharType BACKUP_EXTENSION[] = FILE_PATH_LITERAL("backup");

// Prefix for the checksum in the dictionary file.
const char CHECKSUM_PREFIX[] = "checksum_v1 = ";

// The status of the checksum in a custom spellcheck dictionary.
enum ChecksumStatus {
  VALID_CHECKSUM,
  INVALID_CHECKSUM,
};

// The result of a dictionary sanitation. Can be used as a bitmap.
enum ChangeSanitationResult {
  // The change is valid and can be applied as-is.
  VALID_CHANGE = 0,

  // The change contained words to be added that are not valid.
  DETECTED_INVALID_WORDS = 1,

  // The change contained words to be added that are already in the dictionary.
  DETECTED_DUPLICATE_WORDS = 2,

  // The change contained words to be removed that are not in the dictionary.
  DETECTED_MISSING_WORDS = 4,
};

// Loads the file at |file_path| into the |words| container. If the file has a
// valid checksum, then returns ChecksumStatus::VALID. If the file has an
// invalid checksum, then returns ChecksumStatus::INVALID and clears |words|.
ChecksumStatus LoadFile(const base::FilePath& file_path,
                        std::set<std::string>* words) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(words);
  words->clear();
  std::string contents;
  base::ReadFileToString(file_path, &contents);
  size_t pos = contents.rfind(CHECKSUM_PREFIX);
  if (pos != std::string::npos) {
    std::string checksum = contents.substr(pos + strlen(CHECKSUM_PREFIX));
    contents = contents.substr(0, pos);
    if (checksum != base::MD5String(contents))
      return INVALID_CHECKSUM;
  }

  std::vector<std::string> word_list = base::SplitString(
      base::TrimWhitespaceASCII(contents, base::TRIM_ALL), "\n",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  words->insert(word_list.begin(), word_list.end());
  return VALID_CHECKSUM;
}

// Returns true for valid custom dictionary words.
bool IsValidWord(const std::string& word) {
  std::string tmp;
  return !word.empty() &&
         word.size() <=
             chrome::spellcheck_common::MAX_CUSTOM_DICTIONARY_WORD_BYTES &&
         base::IsStringUTF8(word) &&
         base::TRIM_NONE ==
             base::TrimWhitespaceASCII(word, base::TRIM_ALL, &tmp);
}

// Loads the custom spellcheck dictionary from |path| into |custom_words|. If
// the dictionary checksum is not valid, but backup checksum is valid, then
// restores the backup and loads that into |custom_words| instead. If the backup
// is invalid too, then clears |custom_words|. Must be called on the file
// thread.
void LoadDictionaryFileReliably(const base::FilePath& path,
                                std::set<std::string>* custom_words) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(custom_words);
  // Load the contents and verify the checksum.
  if (LoadFile(path, custom_words) == VALID_CHECKSUM)
    return;
  // Checksum is not valid. See if there's a backup.
  base::FilePath backup = path.AddExtension(BACKUP_EXTENSION);
  if (!base::PathExists(backup))
    return;
  // Load the backup and verify its checksum.
  if (LoadFile(backup, custom_words) != VALID_CHECKSUM)
    return;
  // Backup checksum is valid. Restore the backup.
  base::CopyFile(backup, path);
}

// Backs up the original dictionary, saves |custom_words| and its checksum into
// the custom spellcheck dictionary at |path|.
void SaveDictionaryFileReliably(const base::FilePath& path,
                                const std::set<std::string>& custom_words) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  std::stringstream content;
  for (const std::string& word : custom_words)
    content << word << '\n';

  std::string checksum = base::MD5String(content.str());
  content << CHECKSUM_PREFIX << checksum;
  base::CopyFile(path, path.AddExtension(BACKUP_EXTENSION));
  base::ImportantFileWriter::WriteFileAtomically(path, content.str());
}

// Removes duplicate and invalid words from |to_add| word list. Looks for
// duplicates in both |to_add| and |existing| word lists. Returns a bitmap of
// |ChangeSanitationResult| values.
int SanitizeWordsToAdd(const std::set<std::string>& existing,
                       std::set<std::string>* to_add) {
  DCHECK(to_add);
  // Do not add duplicate words.
  std::set<std::string> new_words =
      base::STLSetDifference<std::set<std::string>>(*to_add, existing);
  int result = VALID_CHANGE;
  if (to_add->size() != new_words.size())
    result |= DETECTED_DUPLICATE_WORDS;
  // Do not add invalid words.
  std::set<std::string> valid_new_words;
  for (const std::string& word : new_words) {
    if (IsValidWord(word))
      valid_new_words.insert(valid_new_words.end(), word);
  }
  if (valid_new_words.size() != new_words.size())
    result |= DETECTED_INVALID_WORDS;
  // Save the sanitized words to be added.
  std::swap(*to_add, valid_new_words);
  return result;
}

// Removes word from |to_remove| that are missing from |existing| word list and
// sorts |to_remove|. Returns a bitmap of |ChangeSanitationResult| values.
int SanitizeWordsToRemove(const std::set<std::string>& existing,
                          std::set<std::string>* to_remove) {
  DCHECK(to_remove);
  // Do not remove words that are missing from the dictionary.
  std::set<std::string> found_words =
      base::STLSetIntersection<std::set<std::string>>(existing, *to_remove);
  int result = VALID_CHANGE;
  if (to_remove->size() > found_words.size())
    result |= DETECTED_MISSING_WORDS;
  // Save the sanitized words to be removed.
  std::swap(*to_remove, found_words);
  return result;
}

}  // namespace

SpellcheckCustomDictionary::Change::Change() {
}

SpellcheckCustomDictionary::Change::~Change() {
}

void SpellcheckCustomDictionary::Change::AddWord(const std::string& word) {
  to_add_.insert(word);
}

void SpellcheckCustomDictionary::Change::AddWords(
    const std::set<std::string>& words) {
  to_add_.insert(words.begin(), words.end());
}

void SpellcheckCustomDictionary::Change::RemoveWord(const std::string& word) {
  to_remove_.insert(word);
}

int SpellcheckCustomDictionary::Change::Sanitize(
    const std::set<std::string>& words) {
  int result = VALID_CHANGE;
  if (!to_add_.empty())
    result |= SanitizeWordsToAdd(words, &to_add_);
  if (!to_remove_.empty())
    result |= SanitizeWordsToRemove(words, &to_remove_);
  return result;
}

SpellcheckCustomDictionary::SpellcheckCustomDictionary(
    const base::FilePath& dictionary_directory_name)
    : custom_dictionary_path_(
          dictionary_directory_name.Append(chrome::kCustomDictionaryFileName)),
      is_loaded_(false),
      weak_ptr_factory_(this) {
}

SpellcheckCustomDictionary::~SpellcheckCustomDictionary() {
}

const std::set<std::string>& SpellcheckCustomDictionary::GetWords() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return words_;
}

bool SpellcheckCustomDictionary::AddWord(const std::string& word) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_ptr<Change> dictionary_change(new Change);
  dictionary_change->AddWord(word);
  int result = dictionary_change->Sanitize(GetWords());
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Sync(*dictionary_change);
  Save(dictionary_change.Pass());
  return result == VALID_CHANGE;
}

bool SpellcheckCustomDictionary::RemoveWord(const std::string& word) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_ptr<Change> dictionary_change(new Change);
  dictionary_change->RemoveWord(word);
  int result = dictionary_change->Sanitize(GetWords());
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Sync(*dictionary_change);
  Save(dictionary_change.Pass());
  return result == VALID_CHANGE;
}

bool SpellcheckCustomDictionary::HasWord(const std::string& word) const {
  return !!words_.count(word);
}

void SpellcheckCustomDictionary::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void SpellcheckCustomDictionary::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool SpellcheckCustomDictionary::IsLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_loaded_;
}

bool SpellcheckCustomDictionary::IsSyncing() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !!sync_processor_.get();
}

void SpellcheckCustomDictionary::Load() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SpellcheckCustomDictionary::LoadDictionaryFile,
                 custom_dictionary_path_),
      base::Bind(&SpellcheckCustomDictionary::OnLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

syncer::SyncMergeResult SpellcheckCustomDictionary::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!sync_processor_.get());
  DCHECK(!sync_error_handler_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_handler.get());
  DCHECK_EQ(syncer::DICTIONARY, type);
  sync_processor_ = sync_processor.Pass();
  sync_error_handler_ = sync_error_handler.Pass();

  // Build a list of words to add locally.
  scoped_ptr<Change> to_change_locally(new Change);
  for (const syncer::SyncData& data : initial_sync_data) {
    DCHECK_EQ(syncer::DICTIONARY, data.GetDataType());
    to_change_locally->AddWord(data.GetSpecifics().dictionary().word());
  }

  // Add as many as possible local words remotely.
  to_change_locally->Sanitize(GetWords());
  Change to_change_remotely;
  to_change_remotely.AddWords(base::STLSetDifference<std::set<std::string>>(
      words_, to_change_locally->to_add()));

  // Add remote words locally.
  Apply(*to_change_locally);
  Notify(*to_change_locally);
  Save(to_change_locally.Pass());

  // Send local changes to the sync server.
  syncer::SyncMergeResult result(type);
  result.set_error(Sync(to_change_remotely));
  return result;
}

void SpellcheckCustomDictionary::StopSyncing(syncer::ModelType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(syncer::DICTIONARY, type);
  sync_processor_.reset();
  sync_error_handler_.reset();
}

syncer::SyncDataList SpellcheckCustomDictionary::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(syncer::DICTIONARY, type);
  syncer::SyncDataList data;
  std::string word;
  size_t i = 0;
  for (auto it = words_.begin();
       it != words_.end() &&
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;
       ++it, ++i) {
    word = *it;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    data.push_back(syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  return data;
}

syncer::SyncError SpellcheckCustomDictionary::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_ptr<Change> dictionary_change(new Change);
  for (const syncer::SyncChange& change : change_list) {
    DCHECK(change.IsValid());
    const std::string& word =
        change.sync_data().GetSpecifics().dictionary().word();
    switch (change.change_type()) {
      case syncer::SyncChange::ACTION_ADD:
        dictionary_change->AddWord(word);
        break;
      case syncer::SyncChange::ACTION_DELETE:
        dictionary_change->RemoveWord(word);
        break;
      case syncer::SyncChange::ACTION_UPDATE:
        // Intentionally fall through.
      case syncer::SyncChange::ACTION_INVALID:
        return sync_error_handler_->CreateAndUploadError(
            FROM_HERE,
            "Processing sync changes failed on change type " +
                syncer::SyncChange::ChangeTypeToString(change.change_type()));
    }
  }

  dictionary_change->Sanitize(GetWords());
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Save(dictionary_change.Pass());

  return syncer::SyncError();
}

// static
scoped_ptr<std::set<std::string>>
SpellcheckCustomDictionary::LoadDictionaryFile(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  scoped_ptr<std::set<std::string>> words(new std::set<std::string>);
  LoadDictionaryFileReliably(path, words.get());
  if (!words->empty() &&
      VALID_CHANGE !=
          SanitizeWordsToAdd(std::set<std::string>(), words.get())) {
    SaveDictionaryFileReliably(path, *words);
  }
  SpellCheckHostMetrics::RecordCustomWordCountStats(words->size());
  return words;
}

// static
void SpellcheckCustomDictionary::UpdateDictionaryFile(
    scoped_ptr<Change> dictionary_change,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(dictionary_change);

  if (dictionary_change->empty())
    return;

  std::set<std::string> custom_words;
  LoadDictionaryFileReliably(path, &custom_words);

  // Add words.
  custom_words.insert(dictionary_change->to_add().begin(),
                      dictionary_change->to_add().end());

  // Remove words and save the remainder.
  SaveDictionaryFileReliably(path,
                             base::STLSetDifference<std::set<std::string>>(
                                 custom_words, dictionary_change->to_remove()));
}

void SpellcheckCustomDictionary::OnLoaded(
    scoped_ptr<std::set<std::string>> custom_words) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(custom_words);
  Change dictionary_change;
  dictionary_change.AddWords(*custom_words);
  dictionary_change.Sanitize(GetWords());
  Apply(dictionary_change);
  Sync(dictionary_change);
  is_loaded_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnCustomDictionaryLoaded());
}

void SpellcheckCustomDictionary::Apply(const Change& dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!dictionary_change.to_add().empty()) {
    words_.insert(dictionary_change.to_add().begin(),
                  dictionary_change.to_add().end());
  }
  if (!dictionary_change.to_remove().empty()) {
    std::set<std::string> updated_words =
        base::STLSetDifference<std::set<std::string>>(
            words_, dictionary_change.to_remove());
    std::swap(words_, updated_words);
  }
}

void SpellcheckCustomDictionary::Save(scoped_ptr<Change> dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellcheckCustomDictionary::UpdateDictionaryFile,
                 base::Passed(&dictionary_change), custom_dictionary_path_));
}

syncer::SyncError SpellcheckCustomDictionary::Sync(
    const Change& dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  syncer::SyncError error;
  if (!IsSyncing() || dictionary_change.empty())
    return error;

  // The number of words on the sync server should not exceed the limits.
  int server_size = static_cast<int>(words_.size()) -
      static_cast<int>(dictionary_change.to_add().size());
  int max_upload_size = std::max(
      0,
      static_cast<int>(
          chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS) -
          server_size);
  int upload_size = std::min(
      static_cast<int>(dictionary_change.to_add().size()),
      max_upload_size);

  syncer::SyncChangeList sync_change_list;
  int i = 0;

  for (auto it = dictionary_change.to_add().begin();
       it != dictionary_change.to_add().end() && i < upload_size; ++it, ++i) {
    const std::string& word = *it;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    sync_change_list.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }

  for (const std::string& word : dictionary_change.to_remove()) {
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    sync_change_list.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }

  // Send the changes to the sync processor.
  error = sync_processor_->ProcessSyncChanges(FROM_HERE, sync_change_list);
  if (error.IsSet())
    return error;

  // Turn off syncing of this dictionary if the server already has the maximum
  // number of words.
  if (words_.size() > chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS)
    StopSyncing(syncer::DICTIONARY);

  return error;
}

void SpellcheckCustomDictionary::Notify(const Change& dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsLoaded() || dictionary_change.empty())
    return;
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnCustomDictionaryChanged(dictionary_change));
}
