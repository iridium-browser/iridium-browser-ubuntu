// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/physical_web_pages/physical_web_page_suggestions_provider.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace ntp_snippets {

namespace {

const size_t kMaxSuggestionsCount = 10;

}  // namespace

// TODO(vitaliii): remove when Physical Web C++ interface is provided.
UrlInfo::UrlInfo() {}
UrlInfo::~UrlInfo() {}
UrlInfo::UrlInfo(const UrlInfo& other) = default;

PhysicalWebPageSuggestionsProvider::PhysicalWebPageSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    CategoryFactory* category_factory)
    : ContentSuggestionsProvider(observer, category_factory),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(category_factory->FromKnownCategory(
          KnownCategories::PHYSICAL_WEB_PAGES)) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);
}

PhysicalWebPageSuggestionsProvider::~PhysicalWebPageSuggestionsProvider() {}

void PhysicalWebPageSuggestionsProvider::OnDisplayableUrlsChanged(
    const std::vector<UrlInfo>& urls) {
  NotifyStatusChanged(CategoryStatus::AVAILABLE);
  std::vector<ContentSuggestion> suggestions;

  for (const UrlInfo& url_info : urls) {
    if (suggestions.size() >= kMaxSuggestionsCount) break;

    ContentSuggestion suggestion(
        MakeUniqueID(provided_category_, url_info.site_url.spec()),
        url_info.site_url);

    suggestion.set_title(base::UTF8ToUTF16(url_info.title));
    suggestion.set_snippet_text(base::UTF8ToUTF16(url_info.description));
    suggestion.set_publish_date(url_info.scan_time);
    suggestion.set_publisher_name(base::UTF8ToUTF16(url_info.site_url.host()));
    suggestions.push_back(std::move(suggestion));
  }

  observer()->OnNewSuggestions(this, provided_category_,
                               std::move(suggestions));
}

CategoryStatus PhysicalWebPageSuggestionsProvider::GetCategoryStatus(
    Category category) {
  return category_status_;
}

CategoryInfo PhysicalWebPageSuggestionsProvider::GetCategoryInfo(
    Category category) {
  // TODO(vitaliii): Use a proper string once it's been agreed on.
  return CategoryInfo(base::ASCIIToUTF16("Physical web pages"),
                      ContentSuggestionsCardLayout::MINIMAL_CARD,
                      /* has_more_button */ true,
                      /* show_if_empty */ false);
}

void PhysicalWebPageSuggestionsProvider::DismissSuggestion(
    const std::string& suggestion_id) {
  // TODO(vitaliii): Implement this and then
  // ClearDismissedSuggestionsForDebugging.
}

void PhysicalWebPageSuggestionsProvider::FetchSuggestionImage(
    const std::string& suggestion_id, const ImageFetchedCallback& callback) {
  // TODO(vitaliii): Implement.
}

void PhysicalWebPageSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  ClearDismissedSuggestionsForDebugging(provided_category_);
}

void PhysicalWebPageSuggestionsProvider::ClearCachedSuggestions(
    Category category) {
  // Ignored
}

void PhysicalWebPageSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  // Not implemented.
  callback.Run(std::vector<ContentSuggestion>());
}

void PhysicalWebPageSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  // TODO(vitaliii): Implement when dismissed suggestions are supported.
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

// Updates the |category_status_| and notifies the |observer_|, if necessary.
void PhysicalWebPageSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  if (category_status_ == new_status) return;
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

}  // namespace ntp_snippets
