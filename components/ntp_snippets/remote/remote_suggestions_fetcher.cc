// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_fetcher.h"

#include <cstdlib>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "net/url_request/url_fetcher.h"
#include "ui/base/l10n/l10n_util.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::HttpRequestHeaders;
using net::URLRequestStatus;
using translate::LanguageModel;

namespace ntp_snippets {

using internal::JsonRequest;
using internal::FetchAPI;
using internal::FetchResult;

namespace {

const char kChromeReaderApiScope[] =
    "https://www.googleapis.com/auth/webhistory";
const char kContentSuggestionsApiScope[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";
const char kSnippetsServerNonAuthorizedFormat[] = "%s?key=%s";
const char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

// Variation parameter for personalizing fetching of snippets.
const char kPersonalizationName[] = "fetching_personalization";

// Variation parameter for chrome-content-suggestions backend.
const char kContentSuggestionsBackend[] = "content_suggestions_backend";

// Constants for possible values of the "fetching_personalization" parameter.
const char kPersonalizationPersonalString[] = "personal";
const char kPersonalizationNonPersonalString[] = "non_personal";
const char kPersonalizationBothString[] = "both";  // the default value

const int kFetchTimeHistogramResolution = 5;

std::string FetchResultToString(FetchResult result) {
  switch (result) {
    case FetchResult::SUCCESS:
      return "OK";
    case FetchResult::DEPRECATED_EMPTY_HOSTS:
      return "Cannot fetch for empty hosts list.";
    case FetchResult::URL_REQUEST_STATUS_ERROR:
      return "URLRequestStatus error";
    case FetchResult::HTTP_ERROR:
      return "HTTP error";
    case FetchResult::JSON_PARSE_ERROR:
      return "Received invalid JSON";
    case FetchResult::INVALID_SNIPPET_CONTENT_ERROR:
      return "Invalid / empty list.";
    case FetchResult::OAUTH_TOKEN_ERROR:
      return "Error in obtaining an OAuth2 access token.";
    case FetchResult::INTERACTIVE_QUOTA_ERROR:
      return "Out of interactive quota.";
    case FetchResult::NON_INTERACTIVE_QUOTA_ERROR:
      return "Out of non-interactive quota.";
    case FetchResult::RESULT_MAX:
      break;
  }
  NOTREACHED();
  return "Unknown error";
}

Status FetchResultToStatus(FetchResult result) {
  switch (result) {
    case FetchResult::SUCCESS:
      return Status::Success();
    // Permanent errors occur if it is more likely that the error originated
    // from the client.
    case FetchResult::DEPRECATED_EMPTY_HOSTS:
    case FetchResult::OAUTH_TOKEN_ERROR:
      return Status(StatusCode::PERMANENT_ERROR, FetchResultToString(result));
    // Temporary errors occur if it's more likely that the client behaved
    // correctly but the server failed to respond as expected.
    // TODO(fhorschig): Revisit HTTP_ERROR once the rescheduling was reworked.
    case FetchResult::HTTP_ERROR:
    case FetchResult::INTERACTIVE_QUOTA_ERROR:
    case FetchResult::NON_INTERACTIVE_QUOTA_ERROR:
    case FetchResult::URL_REQUEST_STATUS_ERROR:
    case FetchResult::INVALID_SNIPPET_CONTENT_ERROR:
    case FetchResult::JSON_PARSE_ERROR:
      return Status(StatusCode::TEMPORARY_ERROR, FetchResultToString(result));
    case FetchResult::RESULT_MAX:
      break;
  }
  NOTREACHED();
  return Status(StatusCode::PERMANENT_ERROR, std::string());
}

std::string GetFetchEndpoint() {
  std::string endpoint = variations::GetVariationParamValue(
      ntp_snippets::kStudyName, kContentSuggestionsBackend);
  return endpoint.empty() ? kContentSuggestionsServer : endpoint;
}

bool UsesChromeContentSuggestionsAPI(const GURL& endpoint) {
  if (endpoint == kChromeReaderServer) {
    return false;
  }

  if (endpoint != kContentSuggestionsServer &&
      endpoint != kContentSuggestionsStagingServer &&
      endpoint != kContentSuggestionsAlphaServer) {
    LOG(WARNING) << "Unknown value for " << kContentSuggestionsBackend << ": "
                 << "assuming chromecontentsuggestions-style API";
  }
  return true;
}

// Creates snippets from dictionary values in |list| and adds them to
// |snippets|. Returns true on success, false if anything went wrong.
// |remote_category_id| is only used if |content_suggestions_api| is true.
bool AddSnippetsFromListValue(bool content_suggestions_api,
                              int remote_category_id,
                              const base::ListValue& list,
                              NTPSnippet::PtrVector* snippets) {
  for (const auto& value : list) {
    const base::DictionaryValue* dict = nullptr;
    if (!value->GetAsDictionary(&dict)) {
      return false;
    }

    std::unique_ptr<NTPSnippet> snippet;
    if (content_suggestions_api) {
      snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(
          *dict, remote_category_id);
    } else {
      snippet = NTPSnippet::CreateFromChromeReaderDictionary(*dict);
    }
    if (!snippet) {
      return false;
    }

    snippets->push_back(std::move(snippet));
  }
  return true;
}

int GetMinuteOfTheDay(bool local_time, bool reduced_resolution) {
  base::Time now(base::Time::Now());
  base::Time::Exploded now_exploded{};
  local_time ? now.LocalExplode(&now_exploded) : now.UTCExplode(&now_exploded);
  int now_minute = reduced_resolution
                       ? now_exploded.minute / kFetchTimeHistogramResolution *
                             kFetchTimeHistogramResolution
                       : now_exploded.minute;
  return now_exploded.hour * 60 + now_minute;
}

// The response from the backend might include suggestions from multiple
// categories. If only a single category was requested, this function filters
// all other categories out.
void FilterCategories(
    RemoteSuggestionsFetcher::FetchedCategoriesVector* categories,
    base::Optional<Category> exclusive_category) {
  if (!exclusive_category.has_value()) {
    return;
  }
  Category exclusive = exclusive_category.value();
  auto category_it = std::find_if(
      categories->begin(), categories->end(),
      [&exclusive](const RemoteSuggestionsFetcher::FetchedCategory& c) -> bool {
        return c.category == exclusive;
      });
  if (category_it == categories->end()) {
    categories->clear();
    return;
  }
  RemoteSuggestionsFetcher::FetchedCategory category = std::move(*category_it);
  categories->clear();
  categories->push_back(std::move(category));
}

}  // namespace

CategoryInfo BuildArticleCategoryInfo(
    const base::Optional<base::string16>& title) {
  return CategoryInfo(
      title.has_value() ? title.value()
                        : l10n_util::GetStringUTF16(
                              IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER),
      ContentSuggestionsCardLayout::FULL_CARD,
      // TODO(dgn): merge has_more_action and has_reload_action when we remove
      // the kFetchMoreFeature flag. See https://crbug.com/667752
      /*has_more_action=*/base::FeatureList::IsEnabled(kFetchMoreFeature),
      /*has_reload_action=*/true,
      /*has_view_all_action=*/false,
      /*show_if_empty=*/true,
      l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_EMPTY));
}

CategoryInfo BuildRemoteCategoryInfo(const base::string16& title,
                                     bool allow_fetching_more_results) {
  return CategoryInfo(
      title, ContentSuggestionsCardLayout::FULL_CARD,
      // TODO(dgn): merge has_more_action and has_reload_action when we remove
      // the kFetchMoreFeature flag. See https://crbug.com/667752
      /*has_more_action=*/allow_fetching_more_results &&
          base::FeatureList::IsEnabled(kFetchMoreFeature),
      /*has_reload_action=*/allow_fetching_more_results,
      /*has_view_all_action=*/false,
      /*show_if_empty=*/false,
      // TODO(tschumann): The message for no-articles is likely wrong
      // and needs to be added to the stubby protocol if we want to
      // support it.
      l10n_util::GetStringUTF16(IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_EMPTY));
}

RemoteSuggestionsFetcher::FetchedCategory::FetchedCategory(Category c,
                                                           CategoryInfo&& info)
    : category(c), info(info) {}

RemoteSuggestionsFetcher::FetchedCategory::FetchedCategory(FetchedCategory&&) =
    default;

RemoteSuggestionsFetcher::FetchedCategory::~FetchedCategory() = default;

RemoteSuggestionsFetcher::FetchedCategory&
RemoteSuggestionsFetcher::FetchedCategory::operator=(FetchedCategory&&) =
    default;

RemoteSuggestionsFetcher::RemoteSuggestionsFetcher(
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    scoped_refptr<URLRequestContextGetter> url_request_context_getter,
    PrefService* pref_service,
    LanguageModel* language_model,
    const ParseJSONCallback& parse_json_callback,
    const std::string& api_key,
    const UserClassifier* user_classifier)
    : OAuth2TokenService::Consumer("ntp_snippets"),
      signin_manager_(signin_manager),
      token_service_(token_service),
      url_request_context_getter_(std::move(url_request_context_getter)),
      language_model_(language_model),
      parse_json_callback_(parse_json_callback),
      fetch_url_(GetFetchEndpoint()),
      fetch_api_(UsesChromeContentSuggestionsAPI(fetch_url_)
                     ? FetchAPI::CHROME_CONTENT_SUGGESTIONS_API
                     : FetchAPI::CHROME_READER_API),
      api_key_(api_key),
      tick_clock_(new base::DefaultTickClock()),
      user_classifier_(user_classifier),
      request_throttler_rare_ntp_user_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_RARE_NTP_USER),
      request_throttler_active_ntp_user_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_NTP_USER),
      request_throttler_active_suggestions_consumer_(
          pref_service,
          RequestThrottler::RequestType::
              CONTENT_SUGGESTION_FETCHER_ACTIVE_SUGGESTIONS_CONSUMER),
      weak_ptr_factory_(this) {
  std::string personalization = variations::GetVariationParamValue(
      ntp_snippets::kStudyName, kPersonalizationName);
  if (personalization == kPersonalizationNonPersonalString) {
    personalization_ = Personalization::kNonPersonal;
  } else if (personalization == kPersonalizationPersonalString) {
    personalization_ = Personalization::kPersonal;
  } else {
    personalization_ = Personalization::kBoth;
    LOG_IF(WARNING, !personalization.empty() &&
                        personalization != kPersonalizationBothString)
        << "Unknown value for " << kPersonalizationName << ": "
        << personalization;
  }
}

RemoteSuggestionsFetcher::~RemoteSuggestionsFetcher() {
  if (waiting_for_refresh_token_) {
    token_service_->RemoveObserver(this);
  }
}

void RemoteSuggestionsFetcher::FetchSnippets(
    const RequestParams& params,
    SnippetsAvailableCallback callback) {
  if (!DemandQuotaForRequest(params.interactive_request)) {
    FetchFinished(OptionalFetchedCategories(), std::move(callback),
                  params.interactive_request
                      ? FetchResult::INTERACTIVE_QUOTA_ERROR
                      : FetchResult::NON_INTERACTIVE_QUOTA_ERROR,
                  /*error_details=*/std::string());
    return;
  }

  if (!params.interactive_request) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.FetchTimeLocal",
                                GetMinuteOfTheDay(/*local_time=*/true,
                                                  /*reduced_resolution=*/true));
    UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.Snippets.FetchTimeUTC",
                                GetMinuteOfTheDay(/*local_time=*/false,
                                                  /*reduced_resolution=*/true));
  }

  JsonRequest::Builder builder;
  builder.SetFetchAPI(fetch_api_)
      .SetFetchAPI(fetch_api_)
      .SetLanguageModel(language_model_)
      .SetParams(params)
      .SetParseJsonCallback(parse_json_callback_)
      .SetPersonalization(personalization_)
      .SetTickClock(tick_clock_.get())
      .SetUrlRequestContextGetter(url_request_context_getter_)
      .SetUserClassifier(*user_classifier_);

  if (NeedsAuthentication() && signin_manager_->IsAuthenticated()) {
    // Signed-in: get OAuth token --> fetch snippets.
    oauth_token_retried_ = false;
    pending_requests_.emplace(std::move(builder), std::move(callback));
    StartTokenRequest();
  } else if (NeedsAuthentication() && signin_manager_->AuthInProgress()) {
    // Currently signing in: wait for auth to finish (the refresh token) -->
    //     get OAuth token --> fetch snippets.
    pending_requests_.emplace(std::move(builder), std::move(callback));
    if (!waiting_for_refresh_token_) {
      // Wait until we get a refresh token.
      waiting_for_refresh_token_ = true;
      token_service_->AddObserver(this);
    }
  } else {
    // Not signed in: fetch snippets (without authentication).
    FetchSnippetsNonAuthenticated(std::move(builder), std::move(callback));
  }
}

void RemoteSuggestionsFetcher::FetchSnippetsNonAuthenticated(
    JsonRequest::Builder builder,
    SnippetsAvailableCallback callback) {
  // When not providing OAuth token, we need to pass the Google API key.
  builder.SetUrl(
      GURL(base::StringPrintf(kSnippetsServerNonAuthorizedFormat,
                              fetch_url_.spec().c_str(), api_key_.c_str())));
  StartRequest(std::move(builder), std::move(callback));
}

void RemoteSuggestionsFetcher::FetchSnippetsAuthenticated(
    JsonRequest::Builder builder,
    SnippetsAvailableCallback callback,
    const std::string& account_id,
    const std::string& oauth_access_token) {
  // TODO(jkrcal, treib): Add unit-tests for authenticated fetches.
  builder.SetUrl(fetch_url_)
      .SetAuthentication(account_id,
                         base::StringPrintf(kAuthorizationRequestHeaderFormat,
                                            oauth_access_token.c_str()));
  StartRequest(std::move(builder), std::move(callback));
}

void RemoteSuggestionsFetcher::StartRequest(
    JsonRequest::Builder builder,
    SnippetsAvailableCallback callback) {
  std::unique_ptr<JsonRequest> request = builder.Build();
  JsonRequest* raw_request = request.get();
  raw_request->Start(base::BindOnce(&RemoteSuggestionsFetcher::JsonRequestDone,
                                    base::Unretained(this), std::move(request),
                                    std::move(callback)));
}

void RemoteSuggestionsFetcher::StartTokenRequest() {
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(fetch_api_ == FetchAPI::CHROME_CONTENT_SUGGESTIONS_API
                    ? kContentSuggestionsApiScope
                    : kChromeReaderApiScope);
  oauth_request_ = token_service_->StartRequest(
      signin_manager_->GetAuthenticatedAccountId(), scopes, this);
}

////////////////////////////////////////////////////////////////////////////////
// OAuth2TokenService::Consumer overrides
void RemoteSuggestionsFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  // Delete the request after we leave this method.
  std::unique_ptr<OAuth2TokenService::Request> oauth_request(
      std::move(oauth_request_));
  DCHECK_EQ(oauth_request.get(), request)
      << "Got tokens from some previous request";

  while (!pending_requests_.empty()) {
    std::pair<JsonRequest::Builder, SnippetsAvailableCallback>
        builder_and_callback = std::move(pending_requests_.front());
    pending_requests_.pop();
    FetchSnippetsAuthenticated(std::move(builder_and_callback.first),
                               std::move(builder_and_callback.second),
                               oauth_request->GetAccountId(), access_token);
  }
}

void RemoteSuggestionsFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  oauth_request_.reset();

  if (!oauth_token_retried_ &&
      error.state() == GoogleServiceAuthError::State::REQUEST_CANCELED) {
    // The request (especially on startup) can get reset by loading the refresh
    // token - do it one more time.
    oauth_token_retried_ = true;
    StartTokenRequest();
    return;
  }

  DLOG(ERROR) << "Unable to get token: " << error.ToString();
  while (!pending_requests_.empty()) {
    std::pair<JsonRequest::Builder, SnippetsAvailableCallback>
        builder_and_callback = std::move(pending_requests_.front());

    FetchFinished(OptionalFetchedCategories(),
                  std::move(builder_and_callback.second),
                  FetchResult::OAUTH_TOKEN_ERROR,
                  /*error_details=*/base::StringPrintf(
                      " (%s)", error.ToString().c_str()));
    pending_requests_.pop();
  }
}

////////////////////////////////////////////////////////////////////////////////
// OAuth2TokenService::Observer overrides
void RemoteSuggestionsFetcher::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // Only react on tokens for the account the user has signed in with.
  if (account_id != signin_manager_->GetAuthenticatedAccountId()) {
    return;
  }

  token_service_->RemoveObserver(this);
  waiting_for_refresh_token_ = false;
  oauth_token_retried_ = false;
  StartTokenRequest();
}

void RemoteSuggestionsFetcher::JsonRequestDone(
    std::unique_ptr<JsonRequest> request,
    SnippetsAvailableCallback callback,
    std::unique_ptr<base::Value> result,
    FetchResult status_code,
    const std::string& error_details) {
  DCHECK(request);
  last_fetch_json_ = request->GetResponseString();

  UMA_HISTOGRAM_TIMES("NewTabPage.Snippets.FetchTime",
                      request->GetFetchDuration());

  if (!result) {
    FetchFinished(OptionalFetchedCategories(), std::move(callback), status_code,
                  error_details);
    return;
  }
  FetchedCategoriesVector categories;
  if (!JsonToSnippets(*result, &categories)) {
    LOG(WARNING) << "Received invalid snippets: " << last_fetch_json_;
    FetchFinished(OptionalFetchedCategories(), std::move(callback),
                  FetchResult::INVALID_SNIPPET_CONTENT_ERROR, std::string());
    return;
  }
  // Filter out unwanted categories if necessary.
  // TODO(fhorschig): As soon as the server supports filtering by category,
  // adjust the request instead of over-fetching and filtering here.
  FilterCategories(&categories, request->exclusive_category());

  FetchFinished(std::move(categories), std::move(callback),
                FetchResult::SUCCESS, std::string());
}

void RemoteSuggestionsFetcher::FetchFinished(
    OptionalFetchedCategories categories,
    SnippetsAvailableCallback callback,
    FetchResult fetch_result,
    const std::string& error_details) {
  DCHECK(fetch_result == FetchResult::SUCCESS || !categories.has_value());

  last_status_ = FetchResultToString(fetch_result) + error_details;

  UMA_HISTOGRAM_ENUMERATION("NewTabPage.Snippets.FetchResult",
                            static_cast<int>(fetch_result),
                            static_cast<int>(FetchResult::RESULT_MAX));

  DVLOG(1) << "Fetch finished: " << last_status_;

  std::move(callback).Run(FetchResultToStatus(fetch_result),
                          std::move(categories));
}

bool RemoteSuggestionsFetcher::JsonToSnippets(
    const base::Value& parsed,
    FetchedCategoriesVector* categories) {
  const base::DictionaryValue* top_dict = nullptr;
  if (!parsed.GetAsDictionary(&top_dict)) {
    return false;
  }

  switch (fetch_api_) {
    case FetchAPI::CHROME_READER_API: {
      const int kUnusedRemoteCategoryId = -1;
      categories->push_back(FetchedCategory(
          Category::FromKnownCategory(KnownCategories::ARTICLES),
          BuildArticleCategoryInfo(base::nullopt)));

      const base::ListValue* recos = nullptr;
      return top_dict->GetList("recos", &recos) &&
             AddSnippetsFromListValue(/*content_suggestions_api=*/false,
                                      kUnusedRemoteCategoryId, *recos,
                                      &categories->back().snippets);
    }

    case FetchAPI::CHROME_CONTENT_SUGGESTIONS_API: {
      const base::ListValue* categories_value = nullptr;
      if (!top_dict->GetList("categories", &categories_value)) {
        return false;
      }

      for (const auto& v : *categories_value) {
        std::string utf8_title;
        int remote_category_id = -1;
        const base::DictionaryValue* category_value = nullptr;
        if (!(v->GetAsDictionary(&category_value) &&
              category_value->GetString("localizedTitle", &utf8_title) &&
              category_value->GetInteger("id", &remote_category_id) &&
              (remote_category_id > 0))) {
          return false;
        }

        NTPSnippet::PtrVector snippets;
        const base::ListValue* suggestions = nullptr;
        // Absence of a list of suggestions is treated as an empty list, which
        // is permissible.
        if (category_value->GetList("suggestions", &suggestions)) {
          if (!AddSnippetsFromListValue(
                  /*content_suggestions_api=*/true, remote_category_id,
                  *suggestions, &snippets)) {
            return false;
          }
        }
        Category category = Category::FromRemoteCategory(remote_category_id);
        if (category.IsKnownCategory(KnownCategories::ARTICLES)) {
          categories->push_back(FetchedCategory(
              category,
              BuildArticleCategoryInfo(base::UTF8ToUTF16(utf8_title))));
        } else {
          // TODO(tschumann): Right now, the backend does not yet populate this
          // field. Make it mandatory once the backends provide it.
          bool allow_fetching_more_results = false;
          category_value->GetBoolean("allowFetchingMoreResults",
                                     &allow_fetching_more_results);
          categories->push_back(FetchedCategory(
              category, BuildRemoteCategoryInfo(base::UTF8ToUTF16(utf8_title),
                                                allow_fetching_more_results)));
        }
        categories->back().snippets = std::move(snippets);
      }
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool RemoteSuggestionsFetcher::DemandQuotaForRequest(bool interactive_request) {
  switch (user_classifier_->GetUserClass()) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      return request_throttler_rare_ntp_user_.DemandQuotaForRequest(
          interactive_request);
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      return request_throttler_active_ntp_user_.DemandQuotaForRequest(
          interactive_request);
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      return request_throttler_active_suggestions_consumer_
          .DemandQuotaForRequest(interactive_request);
  }
  NOTREACHED();
  return false;
}

bool RemoteSuggestionsFetcher::NeedsAuthentication() const {
  return (personalization_ == Personalization::kPersonal ||
          personalization_ == Personalization::kBoth);
}

std::string RemoteSuggestionsFetcher::PersonalizationModeString() const {
  switch (personalization_) {
    case Personalization::kPersonal:
      return "Only personalized";
      break;
    case Personalization::kBoth:
      return "Both personalized and non-personalized";
      break;
    case Personalization::kNonPersonal:
      return "Only non-personalized";
      break;
  }
  NOTREACHED();
  return std::string();
}

}  // namespace ntp_snippets
