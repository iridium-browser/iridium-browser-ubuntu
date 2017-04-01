// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/json_request.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/remote/request_params.h"
#include "components/ntp_snippets/user_classifier.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "grit/components_strings.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "ui/base/l10n/l10n_util.h"

using net::URLFetcher;
using net::URLRequestContextGetter;
using net::HttpRequestHeaders;
using net::URLRequestStatus;
using translate::LanguageModel;

namespace ntp_snippets {

namespace internal {

namespace {

// Variation parameter for disabling the retry.
const char kBackground5xxRetriesName[] = "background_5xx_retries_count";

const int kMaxExcludedIds = 100;

// Variation parameter for sending LanguageModel info to the server.
const char kSendTopLanguagesName[] = "send_top_languages";

// Variation parameter for sending UserClassifier info to the server.
const char kSendUserClassName[] = "send_user_class";

int Get5xxRetryCount(bool interactive_request) {
  if (interactive_request) {
    return 2;
  }
  return std::max(0, variations::GetVariationParamByFeatureAsInt(
                         ntp_snippets::kArticleSuggestionsFeature,
                         kBackground5xxRetriesName, 0));
}

bool IsSendingTopLanguagesEnabled() {
  return variations::GetVariationParamByFeatureAsBool(
      ntp_snippets::kArticleSuggestionsFeature, kSendTopLanguagesName,
      /*default_value=*/true);
}

bool IsSendingUserClassEnabled() {
  return variations::GetVariationParamByFeatureAsBool(
      ntp_snippets::kArticleSuggestionsFeature, kSendUserClassName,
      /*default_value=*/false);
}

// Translate the BCP 47 |language_code| into a posix locale string.
std::string PosixLocaleFromBCP47Language(const std::string& language_code) {
  char locale[ULOC_FULLNAME_CAPACITY];
  UErrorCode error = U_ZERO_ERROR;
  // Translate the input to a posix locale.
  uloc_forLanguageTag(language_code.c_str(), locale, ULOC_FULLNAME_CAPACITY,
                      nullptr, &error);
  if (error != U_ZERO_ERROR) {
    DLOG(WARNING) << "Error in translating language code to a locale string: "
                  << error;
    return std::string();
  }
  return locale;
}

std::string ISO639FromPosixLocale(const std::string& locale) {
  char language[ULOC_LANG_CAPACITY];
  UErrorCode error = U_ZERO_ERROR;
  uloc_getLanguage(locale.c_str(), language, ULOC_LANG_CAPACITY, &error);
  if (error != U_ZERO_ERROR) {
    DLOG(WARNING)
        << "Error in translating locale string to a ISO639 language code: "
        << error;
    return std::string();
  }
  return language;
}

void AppendLanguageInfoToList(base::ListValue* list,
                              const LanguageModel::LanguageInfo& info) {
  auto lang = base::MakeUnique<base::DictionaryValue>();
  lang->SetString("language", info.language_code);
  lang->SetDouble("frequency", info.frequency);
  list->Append(std::move(lang));
}

std::string GetUserClassString(UserClassifier::UserClass user_class) {
  switch (user_class) {
    case UserClassifier::UserClass::RARE_NTP_USER:
      return "RARE_NTP_USER";
    case UserClassifier::UserClass::ACTIVE_NTP_USER:
      return "ACTIVE_NTP_USER";
    case UserClassifier::UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      return "ACTIVE_SUGGESTIONS_CONSUMER";
  }
  NOTREACHED();
  return std::string();
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

JsonRequest::JsonRequest(
    base::Optional<Category> exclusive_category,
    base::TickClock* tick_clock,  // Needed until destruction of the request.
    const ParseJSONCallback& callback)
    : exclusive_category_(exclusive_category),
      tick_clock_(tick_clock),
      parse_json_callback_(callback),
      weak_ptr_factory_(this) {
  creation_time_ = tick_clock_->NowTicks();
}

JsonRequest::~JsonRequest() {
  LOG_IF(DFATAL, !request_completed_callback_.is_null())
      << "The CompletionCallback was never called!";
}

void JsonRequest::Start(CompletedCallback callback) {
  request_completed_callback_ = std::move(callback);
  url_fetcher_->Start();
}

base::TimeDelta JsonRequest::GetFetchDuration() const {
  return tick_clock_->NowTicks() - creation_time_;
}

std::string JsonRequest::GetResponseString() const {
  std::string response;
  url_fetcher_->GetResponseAsString(&response);
  return response;
}

////////////////////////////////////////////////////////////////////////////////
// URLFetcherDelegate overrides
void JsonRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);
  const URLRequestStatus& status = url_fetcher_->GetStatus();
  int response = url_fetcher_->GetResponseCode();
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "NewTabPage.Snippets.FetchHttpResponseOrErrorCode",
      status.is_success() ? response : status.error());

  if (!status.is_success()) {
    std::move(request_completed_callback_)
        .Run(/*result=*/nullptr, FetchResult::URL_REQUEST_STATUS_ERROR,
             /*error_details=*/base::StringPrintf(" %d", status.error()));
  } else if (response != net::HTTP_OK) {
    // TODO(jkrcal): https://crbug.com/609084
    // We need to deal with the edge case again where the auth
    // token expires just before we send the request (in which case we need to
    // fetch a new auth token). We should extract that into a common class
    // instead of adding it to every single class that uses auth tokens.
    std::move(request_completed_callback_)
        .Run(/*result=*/nullptr, FetchResult::HTTP_ERROR,
             /*error_details=*/base::StringPrintf(" %d", response));
  } else {
    ParseJsonResponse();
  }
}

void JsonRequest::ParseJsonResponse() {
  std::string json_string;
  bool stores_result_to_string =
      url_fetcher_->GetResponseAsString(&json_string);
  DCHECK(stores_result_to_string);

  parse_json_callback_.Run(
      json_string,
      base::Bind(&JsonRequest::OnJsonParsed, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&JsonRequest::OnJsonError, weak_ptr_factory_.GetWeakPtr()));
}

void JsonRequest::OnJsonParsed(std::unique_ptr<base::Value> result) {
  std::move(request_completed_callback_)
      .Run(std::move(result), FetchResult::SUCCESS,
           /*error_details=*/std::string());
}

void JsonRequest::OnJsonError(const std::string& error) {
  std::string json_string;
  url_fetcher_->GetResponseAsString(&json_string);
  LOG(WARNING) << "Received invalid JSON (" << error << "): " << json_string;
  std::move(request_completed_callback_)
      .Run(/*result=*/nullptr, FetchResult::JSON_PARSE_ERROR,
           /*error_details=*/base::StringPrintf(" (error %s)", error.c_str()));
}

JsonRequest::Builder::Builder()
    : fetch_api_(CHROME_READER_API),
      personalization_(Personalization::kBoth),
      language_model_(nullptr) {}
JsonRequest::Builder::Builder(JsonRequest::Builder&&) = default;
JsonRequest::Builder::~Builder() = default;

std::unique_ptr<JsonRequest> JsonRequest::Builder::Build() const {
  DCHECK(!url_.is_empty());
  DCHECK(url_request_context_getter_);
  DCHECK(tick_clock_);
  auto request = base::MakeUnique<JsonRequest>(
      params_.exclusive_category, tick_clock_, parse_json_callback_);
  std::string body = BuildBody();
  std::string headers = BuildHeaders();
  request->url_fetcher_ = BuildURLFetcher(request.get(), headers, body);

  // Log the request for debugging network issues.
  VLOG(1) << "Sending a NTP snippets request to " << url_ << ":\n"
          << headers << "\n"
          << body;

  return request;
}

JsonRequest::Builder& JsonRequest::Builder::SetAuthentication(
    const std::string& account_id,
    const std::string& auth_header) {
  obfuscated_gaia_id_ = account_id;
  auth_header_ = auth_header;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetFetchAPI(FetchAPI fetch_api) {
  fetch_api_ = fetch_api;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetLanguageModel(
    const translate::LanguageModel* language_model) {
  language_model_ = language_model;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetParams(
    const RequestParams& params) {
  params_ = params;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetParseJsonCallback(
    ParseJSONCallback callback) {
  parse_json_callback_ = callback;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetPersonalization(
    Personalization personalization) {
  personalization_ = personalization;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetTickClock(
    base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetUrl(const GURL& url) {
  url_ = url;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetUrlRequestContextGetter(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter) {
  url_request_context_getter_ = context_getter;
  return *this;
}

JsonRequest::Builder& JsonRequest::Builder::SetUserClassifier(
    const UserClassifier& user_classifier) {
  if (IsSendingUserClassEnabled()) {
    user_class_ = GetUserClassString(user_classifier.GetUserClass());
  }
  return *this;
}

std::string JsonRequest::Builder::BuildHeaders() const {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Content-Type", "application/json; charset=UTF-8");
  if (!auth_header_.empty()) {
    headers.SetHeader("Authorization", auth_header_);
  }
  // Add X-Client-Data header with experiment IDs from field trials.
  // Note: It's OK to pass |is_signed_in| false if it's unknown, as it does
  // not affect transmission of experiments coming from the variations server.
  bool is_signed_in = false;
  variations::AppendVariationHeaders(url_,
                                     false,  // incognito
                                     false,  // uma_enabled
                                     is_signed_in, &headers);
  return headers.ToString();
}

std::string JsonRequest::Builder::BuildBody() const {
  auto request = base::MakeUnique<base::DictionaryValue>();
  std::string user_locale = PosixLocaleFromBCP47Language(params_.language_code);
  switch (fetch_api_) {
    case CHROME_READER_API: {
      auto content_params = base::MakeUnique<base::DictionaryValue>();
      content_params->SetBoolean("only_return_personalized_results",
                                 ReturnOnlyPersonalizedResults());

      auto content_restricts = base::MakeUnique<base::ListValue>();
      for (const auto* metadata : {"TITLE", "SNIPPET", "THUMBNAIL"}) {
        auto entry = base::MakeUnique<base::DictionaryValue>();
        entry->SetString("type", "METADATA");
        entry->SetString("value", metadata);
        content_restricts->Append(std::move(entry));
      }

      auto local_scoring_params = base::MakeUnique<base::DictionaryValue>();
      local_scoring_params->Set("content_params", std::move(content_params));
      local_scoring_params->Set("content_restricts",
                                std::move(content_restricts));

      auto global_scoring_params = base::MakeUnique<base::DictionaryValue>();
      global_scoring_params->SetInteger("num_to_return",
                                        params_.count_to_fetch);
      global_scoring_params->SetInteger("sort_type", 1);

      auto advanced = base::MakeUnique<base::DictionaryValue>();
      advanced->Set("local_scoring_params", std::move(local_scoring_params));
      advanced->Set("global_scoring_params", std::move(global_scoring_params));

      request->SetString("response_detail_level", "STANDARD");
      request->Set("advanced_options", std::move(advanced));
      if (!obfuscated_gaia_id_.empty()) {
        request->SetString("obfuscated_gaia_id", obfuscated_gaia_id_);
      }
      if (!user_locale.empty()) {
        request->SetString("user_locale", user_locale);
      }
      break;
    }

    case CHROME_CONTENT_SUGGESTIONS_API: {
      if (!user_locale.empty()) {
        request->SetString("uiLanguage", user_locale);
      }

      request->SetString("priority", params_.interactive_request
                                         ? "USER_ACTION"
                                         : "BACKGROUND_PREFETCH");

      auto excluded = base::MakeUnique<base::ListValue>();
      for (const auto& id : params_.excluded_ids) {
        excluded->AppendString(id);
        if (excluded->GetSize() >= kMaxExcludedIds) {
          break;
        }
      }
      request->Set("excludedSuggestionIds", std::move(excluded));

      if (!user_class_.empty()) {
        request->SetString("userActivenessClass", user_class_);
      }

      translate::LanguageModel::LanguageInfo ui_language;
      translate::LanguageModel::LanguageInfo other_top_language;
      PrepareLanguages(&ui_language, &other_top_language);

      if (ui_language.frequency == 0 && other_top_language.frequency == 0) {
        break;
      }

      auto language_list = base::MakeUnique<base::ListValue>();
      if (ui_language.frequency > 0) {
        AppendLanguageInfoToList(language_list.get(), ui_language);
      }
      if (other_top_language.frequency > 0) {
        AppendLanguageInfoToList(language_list.get(), other_top_language);
      }
      request->Set("topLanguages", std::move(language_list));

      // TODO(sfiera): Support only_return_personalized_results.
      // TODO(sfiera): Support count_to_fetch.
      break;
    }
  }

  std::string request_json;
  bool success = base::JSONWriter::WriteWithOptions(
      *request, base::JSONWriter::OPTIONS_PRETTY_PRINT, &request_json);
  DCHECK(success);
  return request_json;
}

std::unique_ptr<net::URLFetcher> JsonRequest::Builder::BuildURLFetcher(
    net::URLFetcherDelegate* delegate,
    const std::string& headers,
    const std::string& body) const {
  std::unique_ptr<net::URLFetcher> url_fetcher =
      net::URLFetcher::Create(url_, net::URLFetcher::POST, delegate);
  url_fetcher->SetRequestContext(url_request_context_getter_.get());
  url_fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                            net::LOAD_DO_NOT_SAVE_COOKIES);
  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher.get(), data_use_measurement::DataUseUserData::NTP_SNIPPETS);

  url_fetcher->SetExtraRequestHeaders(headers);
  url_fetcher->SetUploadData("application/json", body);

  // Fetchers are sometimes cancelled because a network change was detected.
  url_fetcher->SetAutomaticallyRetryOnNetworkChanges(3);
  url_fetcher->SetMaxRetriesOn5xx(
      Get5xxRetryCount(params_.interactive_request));
  return url_fetcher;
}

void JsonRequest::Builder::PrepareLanguages(
    translate::LanguageModel::LanguageInfo* ui_language,
    translate::LanguageModel::LanguageInfo* other_top_language) const {
  // TODO(jkrcal): Add language model factory for iOS and add fakes to tests so
  // that |language_model| is never nullptr. Remove this check and add a DCHECK
  // into the constructor.
  if (!language_model_ || !IsSendingTopLanguagesEnabled()) {
    return;
  }

  // TODO(jkrcal): Is this back-and-forth converting necessary?
  ui_language->language_code = ISO639FromPosixLocale(
      PosixLocaleFromBCP47Language(params_.language_code));
  ui_language->frequency =
      language_model_->GetLanguageFrequency(ui_language->language_code);

  std::vector<LanguageModel::LanguageInfo> top_languages =
      language_model_->GetTopLanguages();
  for (const LanguageModel::LanguageInfo& info : top_languages) {
    if (info.language_code != ui_language->language_code) {
      *other_top_language = info;

      // Report to UMA how important the UI language is.
      DCHECK_GT(other_top_language->frequency, 0)
          << "GetTopLanguages() should not return languages with 0 frequency";
      float ratio_ui_in_both_languages =
          ui_language->frequency /
          (ui_language->frequency + other_top_language->frequency);
      UMA_HISTOGRAM_PERCENTAGE(
          "NewTabPage.Languages.UILanguageRatioInTwoTopLanguages",
          ratio_ui_in_both_languages * 100);
      break;
    }
  }
}

}  // namespace internal

}  // namespace ntp_snippets
