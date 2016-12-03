// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_

#include "base/macros.h"
#include "base/strings/string16.h"

namespace ntp_snippets {

// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum class ContentSuggestionsCardLayout {
  // Uses all fields.
  FULL_CARD,

  // No snippet_text and no thumbnail image.
  MINIMAL_CARD
};

// Contains static meta information about a Category.
class CategoryInfo {
 public:
  CategoryInfo(const base::string16& title,
               ContentSuggestionsCardLayout card_layout,
               bool has_more_button,
               bool show_if_empty);
  CategoryInfo(CategoryInfo&&) = default;
  CategoryInfo& operator=(CategoryInfo&&) = default;

  ~CategoryInfo();

  // Localized title of the category.
  const base::string16& title() const { return title_; }

  // Layout of the cards to be used to display suggestions in this category.
  ContentSuggestionsCardLayout card_layout() const { return card_layout_; }

  // Whether the category supports a "More" button. The button either triggers
  // a fixed action (like opening a native page) or, if there is no such fixed
  // action, it queries the provider for more suggestions.
  bool has_more_button() const { return has_more_button_; }

  // Whether this category should be shown if it offers no suggestions.
  bool show_if_empty() const { return show_if_empty_; }

 private:
  base::string16 title_;
  ContentSuggestionsCardLayout card_layout_;
  bool has_more_button_;
  bool show_if_empty_;

  DISALLOW_COPY_AND_ASSIGN(CategoryInfo);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_INFO_H_
