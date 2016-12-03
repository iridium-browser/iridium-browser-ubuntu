// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_HELPER_H_

#include <stddef.h>

#include <string>

namespace dictionary_helper {

// Synchronously loads the dictionaries across all profiles. Also loads the
// dictionary for the verifier if DisableVerifier() hasn't been called. Returns
// only after the dictionaries have finished to load.
void LoadDictionaries();

// Used to check the size of the dictionary within a particular sync profile.
size_t GetDictionarySize(int index);

// Used to check the size of the dictionary within the verifier sync profile.
size_t GetVerifierDictionarySize();

// Used to verify that dictionaries match across all profiles. Also checks
// verifier if DisableVerifier() hasn't been called.
bool DictionariesMatch();

// Waits until dictionaries match across all platforms.  Returns false
// if this operation times out.
bool AwaitDictionariesMatch();

// Waits until the profile at the given index contains exactly |num| dictionary
// entries.  Returns false if this operation times out.
bool AwaitNumDictionaryEntries(int index, size_t num_words);

// Used to verify that the dictionary within a particular sync profile matches
// the dictionary within the verifier sync profile.
bool DictionaryMatchesVerifier(int index);

// Adds |word| to the dictionary for profile with index |index|. Also adds
// |word| to the verifier if DisableVerifier() hasn't been called. Returns true
// if |word| is valid and not a duplicate. Otherwise returns false.
bool AddWord(int index, const std::string& word);

// Add |n| words with the given |prefix| to the specified client |index|. Also
// adds to the verifier if not disAbled. Return value is true iff all words are
// not duplicates and valid.
bool AddWords(int index, int n, const std::string& prefix);

// Removes |word| from the dictionary for profile with index |index|. Also
// removes |word| from the verifier if DisableVerifier() hasn't been called.
// Returns true if |word| was found. Otherwise returns false.
bool RemoveWord(int index, const std::string& word);

}  // namespace dictionary_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_HELPER_H_
