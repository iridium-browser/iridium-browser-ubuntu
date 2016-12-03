// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_fetcher.h"

#include <map>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/ntp_snippets/category_factory.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace {

using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;
using testing::PrintToString;
using testing::SizeIs;
using testing::StartsWith;
using testing::WithArg;

const char kTestChromeReaderUrlFormat[] =
    "https://chromereader-pa.googleapis.com/v1/fetch?key=%s";
const char kTestChromeContentSuggestionsUrlFormat[] =
    "https://chromecontentsuggestions-pa.googleapis.com/v1/suggestions/"
    "fetch?key=%s";

// Artificial time delay for JSON parsing.
const int64_t kTestJsonParsingLatencyMs = 20;

ACTION_P(MovePointeeTo, ptr) {
  *ptr = std::move(*arg0);
}

MATCHER(HasValue, "") {
  return static_cast<bool>(*arg);
}

MATCHER(IsEmptyArticleList, "is an empty list of articles") {
  NTPSnippetsFetcher::OptionalSnippets& snippets = *arg;
  return snippets && snippets->size() == 1 &&
         snippets->begin()->snippets.empty();
}

MATCHER_P(IsSingleArticle, url, "is a list with the single article %(url)s") {
  NTPSnippetsFetcher::OptionalSnippets& snippets = *arg;
  return snippets && snippets->size() == 1 &&
         snippets->begin()->snippets.size() == 1 &&
         snippets->begin()->snippets[0]->best_source().url.spec() == url;
}

MATCHER_P(EqualsJSON, json, "equals JSON") {
  std::unique_ptr<base::Value> expected = base::JSONReader::Read(json);
  if (!expected) {
    *result_listener << "INTERNAL ERROR: couldn't parse expected JSON";
    return false;
  }

  std::string err_msg;
  int err_line, err_col;
  std::unique_ptr<base::Value> actual = base::JSONReader::ReadAndReturnError(
      arg, base::JSON_PARSE_RFC, nullptr, &err_msg, &err_line, &err_col);
  if (!actual) {
    *result_listener << "input:" << err_line << ":" << err_col << ": "
                     << "parse error: " << err_msg;
    return false;
  }
  return base::Value::Equals(actual.get(), expected.get());
}

class MockSnippetsAvailableCallback {
 public:
  // Workaround for gMock's lack of support for movable arguments.
  void WrappedRun(NTPSnippetsFetcher::OptionalSnippets snippets) {
    Run(&snippets);
  }

  MOCK_METHOD1(Run, void(NTPSnippetsFetcher::OptionalSnippets* snippets));
};

// Factory for FakeURLFetcher objects that always generate errors.
class FailingFakeURLFetcherFactory : public net::URLFetcherFactory {
 public:
  std::unique_ptr<net::URLFetcher> CreateURLFetcher(
      int id, const GURL& url, net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* d) override {
    return base::WrapUnique(new net::FakeURLFetcher(
        url, d, /*response_data=*/std::string(), net::HTTP_NOT_FOUND,
        net::URLRequestStatus::FAILED));
  }
};

void ParseJson(
    const std::string& json,
    const ntp_snippets::NTPSnippetsFetcher::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsFetcher::ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value)
    success_callback.Run(std::move(value));
  else
    error_callback.Run(json_reader.GetErrorMessage());
}

void ParseJsonDelayed(
    const std::string& json,
    const ntp_snippets::NTPSnippetsFetcher::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsFetcher::ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ParseJson, json, success_callback, error_callback),
      base::TimeDelta::FromMilliseconds(kTestJsonParsingLatencyMs));
}

GURL GetFetcherUrl(const char* url_format) {
  return GURL(base::StringPrintf(url_format, google_apis::GetAPIKey().c_str()));
}

}  // namespace

class NTPSnippetsFetcherTest : public testing::Test {
 public:
  NTPSnippetsFetcherTest()
      : NTPSnippetsFetcherTest(GetFetcherUrl(kTestChromeReaderUrlFormat),
                               std::map<std::string, std::string>()) {}

  NTPSnippetsFetcherTest(const GURL& gurl,
                         const std::map<std::string, std::string>& params)
      : params_manager_(ntp_snippets::kStudyName, params),
        mock_task_runner_(new base::TestMockTimeTaskRunner()),
        mock_task_runner_handle_(mock_task_runner_),
        signin_client_(new TestSigninClient(nullptr)),
        account_tracker_(new AccountTrackerService()),
        fake_signin_manager_(new FakeSigninManagerBase(signin_client_.get(),
                                                       account_tracker_.get())),
        fake_token_service_(new FakeProfileOAuth2TokenService()),
        pref_service_(new TestingPrefServiceSimple()),
        test_lang_("en-US"),
        test_url_(gurl) {
    RequestThrottler::RegisterProfilePrefs(pref_service_->registry());

    snippets_fetcher_ = base::MakeUnique<NTPSnippetsFetcher>(
        fake_signin_manager_.get(), fake_token_service_.get(),
        scoped_refptr<net::TestURLRequestContextGetter>(
            new net::TestURLRequestContextGetter(mock_task_runner_.get())),
        pref_service_.get(), &category_factory_, base::Bind(&ParseJsonDelayed),
        /*is_stable_channel=*/true);

    snippets_fetcher_->SetCallback(
        base::Bind(&MockSnippetsAvailableCallback::WrappedRun,
                   base::Unretained(&mock_callback_)));
    snippets_fetcher_->SetTickClockForTesting(
        mock_task_runner_->GetMockTickClock());
    test_hosts_.insert("www.somehost.com");
    test_excluded_.insert("1234567890");
    // Increase initial time such that ticks are non-zero.
    mock_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(1234));
  }

  NTPSnippetsFetcher& snippets_fetcher() { return *snippets_fetcher_; }
  MockSnippetsAvailableCallback& mock_callback() { return mock_callback_; }
  void FastForwardUntilNoTasksRemain() {
    mock_task_runner_->FastForwardUntilNoTasksRemain();
  }
  const std::string& test_lang() const { return test_lang_; }
  const GURL& test_url() { return test_url_; }
  const std::set<std::string>& test_hosts() const { return test_hosts_; }
  const std::set<std::string>& test_excluded() const { return test_excluded_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void InitFakeURLFetcherFactory() {
    if (fake_url_fetcher_factory_)
      return;
    // Instantiation of factory automatically sets itself as URLFetcher's
    // factory.
    fake_url_fetcher_factory_.reset(new net::FakeURLFetcherFactory(
        /*default_factory=*/&failing_url_fetcher_factory_));
  }

  void SetFakeResponse(const std::string& response_data,
                       net::HttpStatusCode response_code,
                       net::URLRequestStatus::Status status) {
    InitFakeURLFetcherFactory();
    fake_url_fetcher_factory_->SetFakeResponse(test_url_, response_data,
                                               response_code, status);
  }

 private:
  variations::testing::VariationParamsManager params_manager_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  base::ThreadTaskRunnerHandle mock_task_runner_handle_;
  FailingFakeURLFetcherFactory failing_url_fetcher_factory_;
  // Initialized lazily in SetFakeResponse().
  std::unique_ptr<net::FakeURLFetcherFactory> fake_url_fetcher_factory_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<AccountTrackerService> account_tracker_;
  std::unique_ptr<SigninManagerBase> fake_signin_manager_;
  std::unique_ptr<OAuth2TokenService> fake_token_service_;
  std::unique_ptr<NTPSnippetsFetcher> snippets_fetcher_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  CategoryFactory category_factory_;
  MockSnippetsAvailableCallback mock_callback_;
  const std::string test_lang_;
  const GURL test_url_;
  std::set<std::string> test_hosts_;
  std::set<std::string> test_excluded_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsFetcherTest);
};

class NTPSnippetsContentSuggestionsFetcherTest : public NTPSnippetsFetcherTest {
 public:
  NTPSnippetsContentSuggestionsFetcherTest()
      : NTPSnippetsFetcherTest(
            GetFetcherUrl(kTestChromeContentSuggestionsUrlFormat),
            {{"content_suggestions_backend", kContentSuggestionsServer}}) {}
};

class NTPSnippetsFetcherHostRestrictedTest : public NTPSnippetsFetcherTest {
 public:
  NTPSnippetsFetcherHostRestrictedTest()
      : NTPSnippetsFetcherTest(GetFetcherUrl(kTestChromeReaderUrlFormat),
                               {{"fetching_host_restrict", "on"}}) {}
};

TEST_F(NTPSnippetsFetcherTest, BuildRequestAuthenticated) {
  NTPSnippetsFetcher::RequestParams params;
  params.obfuscated_gaia_id = "0BFUSGAIA";
  params.only_return_personalized_results = true;
  params.user_locale = "en";
  params.host_restricts = {"chromium.org"};
  params.excluded_ids = {"1234567890"};
  params.count_to_fetch = 25;
  params.interactive_request = false;

  params.fetch_api = NTPSnippetsFetcher::CHROME_READER_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"response_detail_level\": \"STANDARD\","
                         "  \"obfuscated_gaia_id\": \"0BFUSGAIA\","
                         "  \"user_locale\": \"en\","
                         "  \"advanced_options\": {"
                         "    \"local_scoring_params\": {"
                         "      \"content_params\": {"
                         "        \"only_return_personalized_results\": true"
                         "      },"
                         "      \"content_restricts\": ["
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"TITLE\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"SNIPPET\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"THUMBNAIL\""
                         "        }"
                         "      ],"
                         "      \"content_selectors\": ["
                         "        {"
                         "          \"type\": \"HOST_RESTRICT\","
                         "          \"value\": \"chromium.org\""
                         "        }"
                         "      ]"
                         "    },"
                         "    \"global_scoring_params\": {"
                         "      \"num_to_return\": 25,"
                         "      \"sort_type\": 1"
                         "    }"
                         "  }"
                         "}"));

  params.fetch_api = NTPSnippetsFetcher::CHROME_CONTENT_SUGGESTIONS_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"uiLanguage\": \"en\","
                         "  \"priority\": \"BACKGROUND_PREFETCH\","
                         "  \"regularlyVisitedHostNames\": ["
                         "    \"chromium.org\""
                         "  ],"
                         "  \"excludedSuggestionIds\": ["
                         "    \"1234567890\""
                         "  ]"
                         "}"));
}

TEST_F(NTPSnippetsFetcherTest, BuildRequestUnauthenticated) {
  NTPSnippetsFetcher::RequestParams params;
  params.only_return_personalized_results = false;
  params.host_restricts = {};
  params.count_to_fetch = 10;
  params.excluded_ids = {};
  params.interactive_request = true;


  params.fetch_api = NTPSnippetsFetcher::CHROME_READER_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"response_detail_level\": \"STANDARD\","
                         "  \"advanced_options\": {"
                         "    \"local_scoring_params\": {"
                         "      \"content_params\": {"
                         "        \"only_return_personalized_results\": false"
                         "      },"
                         "      \"content_restricts\": ["
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"TITLE\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"SNIPPET\""
                         "        },"
                         "        {"
                         "          \"type\": \"METADATA\","
                         "          \"value\": \"THUMBNAIL\""
                         "        }"
                         "      ],"
                         "      \"content_selectors\": []"
                         "    },"
                         "    \"global_scoring_params\": {"
                         "      \"num_to_return\": 10,"
                         "      \"sort_type\": 1"
                         "    }"
                         "  }"
                         "}"));

  params.fetch_api = NTPSnippetsFetcher::CHROME_CONTENT_SUGGESTIONS_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"regularlyVisitedHostNames\": [],"
                         "  \"priority\": \"USER_ACTION\","
                         "  \"excludedSuggestionIds\": []"
                         "}"));
}

TEST_F(NTPSnippetsFetcherTest, BuildRequestExcludedIds) {
  NTPSnippetsFetcher::RequestParams params;
  params.only_return_personalized_results = false;
  params.host_restricts = {};
  params.count_to_fetch = 10;
  params.interactive_request = false;
  for (int i = 0; i < 200; ++i) {
    params.excluded_ids.insert(base::StringPrintf("%03d", i));
  }

  params.fetch_api = NTPSnippetsFetcher::CHROME_CONTENT_SUGGESTIONS_API;
  EXPECT_THAT(params.BuildRequest(),
              EqualsJSON("{"
                         "  \"regularlyVisitedHostNames\": [],"
                         "  \"priority\": \"BACKGROUND_PREFETCH\","
                         "  \"excludedSuggestionIds\": ["
                         "    \"000\", \"001\", \"002\", \"003\", \"004\","
                         "    \"005\", \"006\", \"007\", \"008\", \"009\","
                         "    \"010\", \"011\", \"012\", \"013\", \"014\","
                         "    \"015\", \"016\", \"017\", \"018\", \"019\","
                         "    \"020\", \"021\", \"022\", \"023\", \"024\","
                         "    \"025\", \"026\", \"027\", \"028\", \"029\","
                         "    \"030\", \"031\", \"032\", \"033\", \"034\","
                         "    \"035\", \"036\", \"037\", \"038\", \"039\","
                         "    \"040\", \"041\", \"042\", \"043\", \"044\","
                         "    \"045\", \"046\", \"047\", \"048\", \"049\","
                         "    \"050\", \"051\", \"052\", \"053\", \"054\","
                         "    \"055\", \"056\", \"057\", \"058\", \"059\","
                         "    \"060\", \"061\", \"062\", \"063\", \"064\","
                         "    \"065\", \"066\", \"067\", \"068\", \"069\","
                         "    \"070\", \"071\", \"072\", \"073\", \"074\","
                         "    \"075\", \"076\", \"077\", \"078\", \"079\","
                         "    \"080\", \"081\", \"082\", \"083\", \"084\","
                         "    \"085\", \"086\", \"087\", \"088\", \"089\","
                         "    \"090\", \"091\", \"092\", \"093\", \"094\","
                         "    \"095\", \"096\", \"097\", \"098\", \"099\""
                         // Truncated to 100 entries. Currently, they happen to
                         // be those lexically first.
                         "  ]"
                         "}"));
}

TEST_F(NTPSnippetsFetcherTest, ShouldNotFetchOnCreation) {
  // The lack of registered baked in responses would cause any fetch to fail.
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              IsEmpty());
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              IsEmpty());
  EXPECT_THAT(snippets_fetcher().last_status(), IsEmpty());
}

TEST_F(NTPSnippetsFetcherTest, ShouldFetchSuccessfully) {
  const std::string kJsonStr =
      "{\"recos\": [{"
      "  \"contentInfo\": {"
      "    \"url\" : \"http://localhost/foobar\","
      "    \"sourceCorpusInfo\" : [{"
      "      \"ampUrl\" : \"http://localhost/amp\","
      "      \"corpusId\" : \"http://localhost/foobar\","
      "      \"publisherData\": { \"sourceName\" : \"Foo News\" }"
      "    }]"
      "  }"
      "}]}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(IsSingleArticle("http://localhost/foobar")));
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(), Eq("OK"));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(NTPSnippetsContentSuggestionsFetcherTest, ShouldFetchSuccessfully) {
  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(IsSingleArticle("http://localhost/foobar")));
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(), Eq("OK"));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(NTPSnippetsContentSuggestionsFetcherTest, EmptyCategoryIsOK) {
  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\""
      "}]}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(IsEmptyArticleList()));
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(), Eq("OK"));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(NTPSnippetsContentSuggestionsFetcherTest, ServerCategories) {
  const std::string kJsonStr =
      "{\"categories\" : [{"
      "  \"id\": 1,"
      "  \"localizedTitle\": \"Articles for You\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foobar\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foobar\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}, {"
      "  \"id\": 2,"
      "  \"localizedTitle\": \"Articles for Me\","
      "  \"suggestions\" : [{"
      "    \"ids\" : [\"http://localhost/foo2\"],"
      "    \"title\" : \"Foo Barred from Baz\","
      "    \"snippet\" : \"...\","
      "    \"fullPageUrl\" : \"http://localhost/foo2\","
      "    \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "    \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "    \"attribution\" : \"Foo News\","
      "    \"imageUrl\" : \"http://localhost/foo2.jpg\","
      "    \"ampUrl\" : \"http://localhost/amp\","
      "    \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "  }]"
      "}]}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  NTPSnippetsFetcher::OptionalSnippets snippets;
  EXPECT_CALL(mock_callback(), Run(_))
      .WillOnce(WithArg<0>(MovePointeeTo(&snippets)));
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*force_request=*/true);
  FastForwardUntilNoTasksRemain();

  ASSERT_TRUE(snippets);
  ASSERT_THAT(snippets->size(), Eq(2u));
  for (const auto& category : *snippets) {
    const auto& articles = category.snippets;
    switch (category.category.id()) {
      case static_cast<int>(KnownCategories::ARTICLES):
        ASSERT_THAT(articles.size(), Eq(1u));
        EXPECT_THAT(articles[0]->best_source().url.spec(),
                    Eq("http://localhost/foobar"));
        break;
      case static_cast<int>(KnownCategories::ARTICLES) + 1:
        ASSERT_THAT(articles.size(), Eq(1u));
        EXPECT_THAT(articles[0]->best_source().url.spec(),
                    Eq("http://localhost/foo2"));
        break;
      default:
        FAIL() << "unknown category ID " << category.category.id();
    }
  }

  EXPECT_THAT(snippets_fetcher().last_status(), Eq("OK"));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherTest, ShouldFetchSuccessfullyEmptyList) {
  const std::string kJsonStr = "{\"recos\": []}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(IsEmptyArticleList()));
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(), Eq("OK"));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherHostRestrictedTest, ShouldReportEmptyHostsError) {
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(/*hosts=*/std::set<std::string>(),
                                            /*language_code=*/"en-US",
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(),
              Eq("Cannot fetch for empty hosts list."));
  EXPECT_THAT(snippets_fetcher().last_json(), IsEmpty());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/1, /*count=*/1)));
  // This particular error gets triggered prior to fetching, so no fetch time
  // or response should get recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              IsEmpty());
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              IsEmpty());
}

TEST_F(NTPSnippetsFetcherHostRestrictedTest, ShouldRestrictToHosts) {
  net::TestURLFetcherFactory test_url_fetcher_factory;
  snippets_fetcher().FetchSnippetsFromHosts(
      {"www.somehost1.com", "www.somehost2.com"}, test_lang(), test_excluded(),
      /*count=*/17,
      /*interactive_request=*/true);
  net::TestURLFetcher* fetcher = test_url_fetcher_factory.GetFetcherByID(0);
  ASSERT_THAT(fetcher, NotNull());
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(fetcher->upload_data());
  ASSERT_TRUE(value) << " failed to parse JSON: "
                     << PrintToString(fetcher->upload_data());
  const base::DictionaryValue* dict = nullptr;
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  const base::DictionaryValue* local_scoring_params = nullptr;
  ASSERT_TRUE(dict->GetDictionary("advanced_options.local_scoring_params",
                                  &local_scoring_params));
  const base::ListValue* content_selectors = nullptr;
  ASSERT_TRUE(
      local_scoring_params->GetList("content_selectors", &content_selectors));
  ASSERT_THAT(content_selectors->GetSize(), Eq(static_cast<size_t>(2)));
  const base::DictionaryValue* content_selector = nullptr;
  ASSERT_TRUE(content_selectors->GetDictionary(0, &content_selector));
  std::string content_selector_value;
  EXPECT_TRUE(content_selector->GetString("value", &content_selector_value));
  EXPECT_THAT(content_selector_value, Eq("www.somehost1.com"));
  ASSERT_TRUE(content_selectors->GetDictionary(1, &content_selector));
  EXPECT_TRUE(content_selector->GetString("value", &content_selector_value));
  EXPECT_THAT(content_selector_value, Eq("www.somehost2.com"));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportUrlStatusError) {
  SetFakeResponse(/*data=*/std::string(), net::HTTP_NOT_FOUND,
                  net::URLRequestStatus::FAILED);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(),
              Eq("URLRequestStatus error -2"));
  EXPECT_THAT(snippets_fetcher().last_json(), IsEmpty());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/-2, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportHttpError) {
  SetFakeResponse(/*data=*/std::string(), net::HTTP_NOT_FOUND,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_json(), IsEmpty());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/404, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportJsonError) {
  const std::string kInvalidJsonStr = "{ \"recos\": []";
  SetFakeResponse(/*data=*/kInvalidJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_status(),
              StartsWith("Received invalid JSON (error "));
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kInvalidJsonStr));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportJsonErrorForEmptyResponse) {
  SetFakeResponse(/*data=*/std::string(), net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_json(), std::string());
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/4, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
}

TEST_F(NTPSnippetsFetcherTest, ShouldReportInvalidListError) {
  const std::string kJsonStr =
      "{\"recos\": [{ \"contentInfo\": { \"foo\" : \"bar\" }}]}";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(snippets_fetcher().last_json(), Eq(kJsonStr));
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/5, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              Not(IsEmpty()));
}

// This test actually verifies that the test setup itself is sane, to prevent
// hard-to-reproduce test failures.
TEST_F(NTPSnippetsFetcherTest, ShouldReportHttpErrorForMissingBakedResponse) {
  InitFakeURLFetcherFactory();
  EXPECT_CALL(mock_callback(), Run(/*snippets=*/Not(HasValue()))).Times(1);
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
}

TEST_F(NTPSnippetsFetcherTest, ShouldCancelOngoingFetch) {
  const std::string kJsonStr = "{ \"recos\": [] }";
  SetFakeResponse(/*data=*/kJsonStr, net::HTTP_OK,
                  net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(mock_callback(), Run(IsEmptyArticleList()));
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  // Second call to FetchSnippetsFromHosts() overrides/cancels the previous.
  // Callback is expected to be called once.
  snippets_fetcher().FetchSnippetsFromHosts(test_hosts(), test_lang(),
                                            test_excluded(),
                                            /*count=*/1,
                                            /*interactive_request=*/true);
  FastForwardUntilNoTasksRemain();
  EXPECT_THAT(
      histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchResult"),
      ElementsAre(base::Bucket(/*min=*/0, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "NewTabPage.Snippets.FetchHttpResponseOrErrorCode"),
              ElementsAre(base::Bucket(/*min=*/200, /*count=*/1)));
  EXPECT_THAT(histogram_tester().GetAllSamples("NewTabPage.Snippets.FetchTime"),
              ElementsAre(base::Bucket(/*min=*/kTestJsonParsingLatencyMs,
                                       /*count=*/1)));
}

::std::ostream& operator<<(
    ::std::ostream& os,
    const NTPSnippetsFetcher::OptionalSnippets& snippets) {
  if (snippets) {
    // Matchers above aren't any more precise than this, so this is sufficient
    // for test-failure diagnostics.
    return os << "list with " << snippets->size() << " elements";
  } else {
    return os << "null";
  }
}

}  // namespace ntp_snippets
