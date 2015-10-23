// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SPELLCHECKER_PLATFORM_SPELLING_ENGINE_H_
#define CHROME_RENDERER_SPELLCHECKER_PLATFORM_SPELLING_ENGINE_H_

#include "base/compiler_specific.h"
#include "chrome/renderer/spellchecker/spelling_engine.h"

class PlatformSpellingEngine : public SpellingEngine {
 public:
  void Init(base::File bdict_file) override;
  bool InitializeIfNeeded() override;
  bool IsEnabled() override;
  bool CheckSpelling(const base::string16& word_to_check, int tag) override;
  void FillSuggestionList(
      const base::string16& wrong_word,
      std::vector<base::string16>* optional_suggestions) override;
};

#endif  // CHROME_RENDERER_SPELLCHECKER_PLATFORM_SPELLING_ENGINE_H_

