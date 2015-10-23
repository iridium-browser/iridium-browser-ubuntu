// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The |Misspelling| object stores the misspelling, a spellcheck suggestion for
// it, and user's action on it. The misspelling is stored as |context|,
// |location|, and |length| instead of only misspelled text, because the
// spellcheck algorithm uses the context.

#include "chrome/browser/spellchecker/misspelling.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace {

// Builds a value from a list of spellcheck suggestions. The caller owns the
// result.
base::Value* BuildSuggestionsValue(const std::vector<base::string16>& list) {
  base::ListValue* result = new base::ListValue;
  result->AppendStrings(list);
  return result;
}

// Builds a value from a spellcheck action. The caller owns the result.
base::Value* BuildUserActionValue(const SpellcheckAction& action) {
  base::ListValue* result = new base::ListValue;
  result->Append(action.Serialize());
  return result;
}

}  // namespace

Misspelling::Misspelling()
    : location(0), length(0), hash(0), timestamp(base::Time::Now()) {}

Misspelling::Misspelling(const base::string16& context,
                         size_t location,
                         size_t length,
                         const std::vector<base::string16>& suggestions,
                         uint32 hash)
    : context(context),
      location(location),
      length(length),
      suggestions(suggestions),
      hash(hash),
      timestamp(base::Time::Now()) {}

Misspelling::~Misspelling() {}

base::DictionaryValue* SerializeMisspelling(const Misspelling& misspelling) {
  base::DictionaryValue* result = new base::DictionaryValue;
  result->SetString(
      "timestamp",
      base::Int64ToString(static_cast<long>(misspelling.timestamp.ToJsTime())));
  result->SetInteger("misspelledLength", misspelling.length);
  result->SetInteger("misspelledStart", misspelling.location);
  result->SetString("originalText", misspelling.context);
  result->SetString("suggestionId", base::UintToString(misspelling.hash));
  result->Set("suggestions", BuildSuggestionsValue(misspelling.suggestions));
  result->Set("userActions", BuildUserActionValue(misspelling.action));
  return result;
}

base::string16 GetMisspelledString(const Misspelling& misspelling) {
  // Feedback sender does not create Misspelling objects for spellcheck results
  // that are out-of-bounds of checked text length.
  if (misspelling.location > misspelling.context.length())
    return base::string16();
  return misspelling.context.substr(misspelling.location, misspelling.length);
}
