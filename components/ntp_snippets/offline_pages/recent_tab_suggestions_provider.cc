// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/offline_pages/recent_tab_suggestions_provider.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/pref_util.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model_query.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

using offline_pages::ClientId;
using offline_pages::OfflinePageItem;
using offline_pages::OfflinePageModelQuery;
using offline_pages::OfflinePageModelQueryBuilder;

namespace ntp_snippets {

namespace {

const int kDefaultMaxSuggestionsCount = 5;

const char* kMaxSuggestionsCountParamName = "recent_tabs_max_count";

int GetMaxSuggestionsCount() {
  return variations::GetVariationParamByFeatureAsInt(
      kRecentOfflineTabSuggestionsFeature, kMaxSuggestionsCountParamName,
      kDefaultMaxSuggestionsCount);
}

struct OrderOfflinePagesByMostRecentlyCreatedFirst {
  bool operator()(const OfflinePageItem* left,
                  const OfflinePageItem* right) const {
    return left->creation_time > right->creation_time;
  }
};

struct OrderOfflinePagesByUrlAndThenMostRecentlyCreatedFirst {
  bool operator()(const OfflinePageItem* left,
                  const OfflinePageItem* right) const {
    if (left->url != right->url) {
      return left->url < right->url;
    }
    return left->creation_time > right->creation_time;
  }
};

std::unique_ptr<OfflinePageModelQuery> BuildRecentTabsQuery(
    offline_pages::OfflinePageModel* model) {
  OfflinePageModelQueryBuilder builder;
  builder.RequireShownAsRecentlyVisitedSite(
      OfflinePageModelQuery::Requirement::INCLUDE_MATCHING);
  return builder.Build(model->GetPolicyController());
}

bool IsRecentTab(offline_pages::ClientPolicyController* policy_controller,
                 const OfflinePageItem& offline_page) {
  return policy_controller->IsShownAsRecentlyVisitedSite(
      offline_page.client_id.name_space);
}

}  // namespace

RecentTabSuggestionsProvider::RecentTabSuggestionsProvider(
    ContentSuggestionsProvider::Observer* observer,
    offline_pages::OfflinePageModel* offline_page_model,
    PrefService* pref_service)
    : ContentSuggestionsProvider(observer),
      category_status_(CategoryStatus::AVAILABLE_LOADING),
      provided_category_(
          Category::FromKnownCategory(KnownCategories::RECENT_TABS)),
      offline_page_model_(offline_page_model),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {
  observer->OnCategoryStatusChanged(this, provided_category_, category_status_);
  offline_page_model_->AddObserver(this);
  FetchRecentTabs();
}

RecentTabSuggestionsProvider::~RecentTabSuggestionsProvider() {
  offline_page_model_->RemoveObserver(this);
}

CategoryStatus RecentTabSuggestionsProvider::GetCategoryStatus(
    Category category) {
  if (category == provided_category_) {
    return category_status_;
  }
  NOTREACHED() << "Unknown category " << category.id();
  return CategoryStatus::NOT_PROVIDED;
}

CategoryInfo RecentTabSuggestionsProvider::GetCategoryInfo(Category category) {
  DCHECK_EQ(provided_category_, category);
  return CategoryInfo(
      l10n_util::GetStringUTF16(IDS_NTP_RECENT_TAB_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::MINIMAL_CARD,
      /*has_more_action=*/false,
      /*has_reload_action=*/false,
      /*has_view_all_action=*/false,
      /*show_if_empty=*/false,
      l10n_util::GetStringUTF16(IDS_NTP_RECENT_TAB_SUGGESTIONS_SECTION_EMPTY));
}

void RecentTabSuggestionsProvider::DismissSuggestion(
    const ContentSuggestion::ID& suggestion_id) {
  DCHECK_EQ(provided_category_, suggestion_id.category());
  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  dismissed_ids.insert(suggestion_id.id_within_category());
  StoreDismissedIDsToPrefs(dismissed_ids);
}

void RecentTabSuggestionsProvider::FetchSuggestionImage(
    const ContentSuggestion::ID& suggestion_id,
    const ImageFetchedCallback& callback) {
  // TODO(vitaliii): Fetch proper thumbnail from OfflinePageModel once it's
  // available there.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, gfx::Image()));
}

void RecentTabSuggestionsProvider::Fetch(
    const Category& category,
    const std::set<std::string>& known_suggestion_ids,
    const FetchDoneCallback& callback) {
  LOG(DFATAL) << "RecentTabSuggestionsProvider has no |Fetch| functionality!";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          callback,
          Status(StatusCode::PERMANENT_ERROR,
                 "RecentTabSuggestionsProvider has no |Fetch| functionality!"),
          base::Passed(std::vector<ContentSuggestion>())));
}

void RecentTabSuggestionsProvider::ClearHistory(
    base::Time begin,
    base::Time end,
    const base::Callback<bool(const GURL& url)>& filter) {
  ClearDismissedSuggestionsForDebugging(provided_category_);
  FetchRecentTabs();
}

void RecentTabSuggestionsProvider::ClearCachedSuggestions(Category category) {
  // Ignored.
}

void RecentTabSuggestionsProvider::GetDismissedSuggestionsForDebugging(
    Category category,
    const DismissedSuggestionsCallback& callback) {
  DCHECK_EQ(provided_category_, category);

  // Offline pages which are not related to recent tabs are also queried here,
  // so that they can be returned if they happen to be dismissed (e.g. due to a
  // bug).
  OfflinePageModelQueryBuilder query_builder;
  offline_page_model_->GetPagesMatchingQuery(
      query_builder.Build(offline_page_model_->GetPolicyController()),
      base::Bind(&RecentTabSuggestionsProvider::
                     GetPagesMatchingQueryCallbackForGetDismissedSuggestions,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void RecentTabSuggestionsProvider::ClearDismissedSuggestionsForDebugging(
    Category category) {
  DCHECK_EQ(provided_category_, category);
  StoreDismissedIDsToPrefs(std::set<std::string>());
  FetchRecentTabs();
}

// static
void RecentTabSuggestionsProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDismissedRecentOfflineTabSuggestions);
}

////////////////////////////////////////////////////////////////////////////////
// Private methods

void RecentTabSuggestionsProvider::
    GetPagesMatchingQueryCallbackForGetDismissedSuggestions(
        const DismissedSuggestionsCallback& callback,
        const std::vector<OfflinePageItem>& offline_pages) const {
  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem& item : offline_pages) {
    if (!dismissed_ids.count(base::IntToString(item.offline_id))) {
      continue;
    }

    suggestions.push_back(ConvertOfflinePage(item));
  }
  callback.Run(std::move(suggestions));
}

void RecentTabSuggestionsProvider::OfflinePageModelLoaded(
    offline_pages::OfflinePageModel* model) {}

void RecentTabSuggestionsProvider::OfflinePageAdded(
    offline_pages::OfflinePageModel* model,
    const offline_pages::OfflinePageItem& added_page) {
  DCHECK_EQ(offline_page_model_, model);
  if (IsRecentTab(model->GetPolicyController(), added_page)) {
    FetchRecentTabs();
  }
}

void RecentTabSuggestionsProvider::
    GetPagesMatchingQueryCallbackForFetchRecentTabs(
        const std::vector<OfflinePageItem>& offline_pages) {
  NotifyStatusChanged(CategoryStatus::AVAILABLE);
  std::set<std::string> old_dismissed_ids = ReadDismissedIDsFromPrefs();
  std::set<std::string> new_dismissed_ids;
  std::vector<const OfflinePageItem*> recent_tab_items;
  for (const OfflinePageItem& item : offline_pages) {
    std::string offline_page_id = base::IntToString(item.offline_id);
    if (old_dismissed_ids.count(offline_page_id)) {
      new_dismissed_ids.insert(offline_page_id);
    } else {
      recent_tab_items.push_back(&item);
    }
  }

  observer()->OnNewSuggestions(
      this, provided_category_,
      GetMostRecentlyCreatedWithoutDuplicates(std::move(recent_tab_items)));
  if (new_dismissed_ids.size() != old_dismissed_ids.size()) {
    StoreDismissedIDsToPrefs(new_dismissed_ids);
  }
}

void RecentTabSuggestionsProvider::OfflinePageDeleted(
    int64_t offline_id,
    const ClientId& client_id) {
  // Because we never switch to NOT_PROVIDED dynamically, there can be no open
  // UI containing an invalidated suggestion unless the status is something
  // other than NOT_PROVIDED, so only notify invalidation in that case.
  if (category_status_ != CategoryStatus::NOT_PROVIDED) {
    InvalidateSuggestion(offline_id);
  }
}

void RecentTabSuggestionsProvider::FetchRecentTabs() {
  offline_page_model_->GetPagesMatchingQuery(
      BuildRecentTabsQuery(offline_page_model_),
      base::Bind(&RecentTabSuggestionsProvider::
                     GetPagesMatchingQueryCallbackForFetchRecentTabs,
                 weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabSuggestionsProvider::NotifyStatusChanged(
    CategoryStatus new_status) {
  DCHECK_NE(CategoryStatus::NOT_PROVIDED, category_status_);
  if (category_status_ == new_status) {
    return;
  }
  category_status_ = new_status;
  observer()->OnCategoryStatusChanged(this, provided_category_, new_status);
}

ContentSuggestion RecentTabSuggestionsProvider::ConvertOfflinePage(
    const OfflinePageItem& offline_page) const {
  // TODO(vitaliii): Make sure the URL is opened in the existing tab.
  ContentSuggestion suggestion(provided_category_,
                               base::IntToString(offline_page.offline_id),
                               offline_page.url);

  if (offline_page.title.empty()) {
    // TODO(vitaliii): Remove this fallback once the OfflinePageModel provides
    // titles for all (relevant) OfflinePageItems.
    suggestion.set_title(base::UTF8ToUTF16(offline_page.url.spec()));
  } else {
    suggestion.set_title(offline_page.title);
  }
  suggestion.set_publish_date(offline_page.creation_time);
  suggestion.set_publisher_name(base::UTF8ToUTF16(offline_page.url.host()));
  auto extra = base::MakeUnique<RecentTabSuggestionExtra>();
  extra->tab_id = offline_page.client_id.id;
  extra->offline_page_id = offline_page.offline_id;
  suggestion.set_recent_tab_suggestion_extra(std::move(extra));
  return suggestion;
}

std::vector<ContentSuggestion>
RecentTabSuggestionsProvider::GetMostRecentlyCreatedWithoutDuplicates(
    std::vector<const OfflinePageItem*> offline_page_items) const {
  // |std::unique| only removes duplicates that immediately follow each other.
  // Thus, first, we have to sort by URL and creation time and only then remove
  // duplicates and sort the remaining items by creation time.
  std::sort(offline_page_items.begin(), offline_page_items.end(),
            OrderOfflinePagesByUrlAndThenMostRecentlyCreatedFirst());
  std::vector<const OfflinePageItem*>::iterator new_end = std::unique(
      offline_page_items.begin(), offline_page_items.end(),
      [](const OfflinePageItem* left, const OfflinePageItem* right) {
        return left->url == right->url;
      });
  offline_page_items.erase(new_end, offline_page_items.end());
  std::sort(offline_page_items.begin(), offline_page_items.end(),
            OrderOfflinePagesByMostRecentlyCreatedFirst());
  std::vector<ContentSuggestion> suggestions;
  for (const OfflinePageItem* offline_page_item : offline_page_items) {
    suggestions.push_back(ConvertOfflinePage(*offline_page_item));
    if (static_cast<int>(suggestions.size()) == GetMaxSuggestionsCount()) {
      break;
    }
  }
  return suggestions;
}

void RecentTabSuggestionsProvider::InvalidateSuggestion(int64_t offline_id) {
  std::string offline_page_id = base::IntToString(offline_id);
  observer()->OnSuggestionInvalidated(
      this, ContentSuggestion::ID(provided_category_, offline_page_id));

  std::set<std::string> dismissed_ids = ReadDismissedIDsFromPrefs();
  auto it = dismissed_ids.find(offline_page_id);
  if (it != dismissed_ids.end()) {
    dismissed_ids.erase(it);
    StoreDismissedIDsToPrefs(dismissed_ids);
  }
}

std::set<std::string> RecentTabSuggestionsProvider::ReadDismissedIDsFromPrefs()
    const {
  return prefs::ReadDismissedIDsFromPrefs(
      *pref_service_, prefs::kDismissedRecentOfflineTabSuggestions);
}

void RecentTabSuggestionsProvider::StoreDismissedIDsToPrefs(
    const std::set<std::string>& dismissed_ids) {
  prefs::StoreDismissedIDsToPrefs(pref_service_,
                                  prefs::kDismissedRecentOfflineTabSuggestions,
                                  dismissed_ids);
}

}  // namespace ntp_snippets
