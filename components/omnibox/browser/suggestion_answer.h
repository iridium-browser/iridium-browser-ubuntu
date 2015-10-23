// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

// Structured representation of the JSON payload of a suggestion with an answer.
// An answer has exactly two image lines, so called because they are a
// combination of text and an optional image URL.  Each image line has 1 or more
// text fields, each of which is required to contain a string and an integer
// type.  The text fields are contained in a non-empty vector and two optional
// named properties, referred to as "additional text" and "status text".
//
// When represented in the UI, these elements should be styled and laid out
// according to the specification at https://goto.google.com/ais_api.
//
// Each of the three classes has either an explicit or implicity copy
// constructor to support copying answer values (via SuggestionAnswer::copy) as
// members of SuggestResult and AutocompleteMatch.
class SuggestionAnswer {
 public:
  class TextField;
  typedef std::vector<TextField> TextFields;
  typedef std::vector<GURL> URLs;

  // These values are named and numbered to match a specification at go/ais_api.
  // The values are only used for answer results.
  enum TextType {
    ANSWER = 1,
    HEADLINE = 2,
    TOP_ALIGNED = 3,
    DESCRIPTION = 4,
    DESCRIPTION_NEGATIVE = 5,
    DESCRIPTION_POSITIVE = 6,
    MORE_INFO = 7,
    SUGGESTION = 8,
    SUGGESTION_POSITIVE = 9,
    SUGGESTION_NEGATIVE = 10,
    SUGGESTION_LINK = 11,
    STATUS = 12,
    PERSONALIZED_SUGGESTION = 13,
  };

  class TextField {
   public:
    TextField();
    ~TextField();

    // Parses |field_json| and populates |text_field| with the contents.  If any
    // of the required elements is missing, returns false and leaves text_field
    // in a partially populated state.
    static bool ParseTextField(const base::DictionaryValue* field_json,
                               TextField* text_field);

    const base::string16& text() const { return text_; }
    int type() const { return type_; }

    bool Equals(const TextField& field) const;

   private:
    base::string16 text_;
    int type_;

    FRIEND_TEST_ALL_PREFIXES(SuggestionAnswerTest, DifferentValuesAreUnequal);

    // No DISALLOW_COPY_AND_ASSIGN since we depend on the copy constructor in
    // SuggestionAnswer::copy and the assigment operator as values in vector.
  };

  class ImageLine {
   public:
    ImageLine();
    explicit ImageLine(const ImageLine& line);
    ~ImageLine();

    // Parses |line_json| and populates |image_line| with the contents.  If any
    // of the required elements is missing, returns false and leaves text_field
    // in a partially populated state.
    static bool ParseImageLine(const base::DictionaryValue* line_json,
                               ImageLine* image_line);

    const TextFields& text_fields() const { return text_fields_; }
    const TextField* additional_text() const { return additional_text_.get(); }
    const TextField* status_text() const { return status_text_.get(); }
    const GURL& image_url() const { return image_url_; }

    bool Equals(const ImageLine& line) const;

   private:
    // Forbid assignment.
    ImageLine& operator=(const ImageLine&);

    TextFields text_fields_;
    scoped_ptr<TextField> additional_text_;
    scoped_ptr<TextField> status_text_;
    GURL image_url_;

    FRIEND_TEST_ALL_PREFIXES(SuggestionAnswerTest, DifferentValuesAreUnequal);
  };

  SuggestionAnswer();
  SuggestionAnswer(const SuggestionAnswer& answer);
  ~SuggestionAnswer();

  // Parses |answer_json| and returns a SuggestionAnswer containing the
  // contents.  If the supplied data is not well formed or is missing required
  // elements, returns nullptr instead.
  static scoped_ptr<SuggestionAnswer> ParseAnswer(
        const base::DictionaryValue* answer_json);

  // TODO(jdonnelly): Once something like std::optional<T> is available in base/
  // (see discussion at http://goo.gl/zN2GNy) remove this in favor of having
  // SuggestResult and AutocompleteMatch use optional<SuggestionAnswer>.
  static scoped_ptr<SuggestionAnswer> copy(const SuggestionAnswer* source) {
    return make_scoped_ptr(source ? new SuggestionAnswer(*source) : nullptr);
  }

  const ImageLine& first_line() const { return first_line_; }
  const ImageLine& second_line() const { return second_line_; }

  // Answer type accessors.  Valid types are non-negative and defined at
  // https://goto.google.com/visual_element_configuration.
  int type() const { return type_; }
  void set_type(int type) { type_ = type; }

  bool Equals(const SuggestionAnswer& answer) const;

  // Retrieves any image URLs appearing in this answer and adds them to |urls|.
  void AddImageURLsTo(URLs* urls) const;

 private:
  // Forbid assignment.
  SuggestionAnswer& operator=(const SuggestionAnswer&);

  ImageLine first_line_;
  ImageLine second_line_;
  int type_;

  FRIEND_TEST_ALL_PREFIXES(SuggestionAnswerTest, DifferentValuesAreUnequal);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_
