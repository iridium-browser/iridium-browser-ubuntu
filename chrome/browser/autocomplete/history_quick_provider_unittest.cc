// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_quick_provider.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/url_database.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/url_index_private_data.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::Time;
using base::TimeDelta;

using content::BrowserThread;

struct TestURLInfo {
  std::string url;
  std::string title;
  int visit_count;
  int typed_count;
  int days_from_now;
} quick_test_db[] = {
  {"http://www.google.com/", "Google", 3, 3, 0},
  {"http://slashdot.org/favorite_page.html", "Favorite page", 200, 100, 0},
  {"http://kerneltrap.org/not_very_popular.html", "Less popular", 4, 0, 0},
  {"http://freshmeat.net/unpopular.html", "Unpopular", 1, 1, 0},
  {"http://news.google.com/?ned=us&topic=n", "Google News - U.S.", 2, 2, 0},
  {"http://news.google.com/", "Google News", 1, 1, 0},
  {"http://foo.com/", "Dir", 200, 100, 0},
  {"http://foo.com/dir/", "Dir", 2, 1, 10},
  {"http://foo.com/dir/another/", "Dir", 10, 5, 0},
  {"http://foo.com/dir/another/again/", "Dir", 5, 1, 0},
  {"http://foo.com/dir/another/again/myfile.html", "File", 3, 1, 0},
  {"http://visitedest.com/y/a", "VA", 10, 1, 20},
  {"http://visitedest.com/y/b", "VB", 9, 1, 20},
  {"http://visitedest.com/x/c", "VC", 8, 1, 20},
  {"http://visitedest.com/x/d", "VD", 7, 1, 20},
  {"http://visitedest.com/y/e", "VE", 6, 1, 20},
  {"http://typeredest.com/y/a", "TA", 5, 5, 0},
  {"http://typeredest.com/y/b", "TB", 5, 4, 0},
  {"http://typeredest.com/x/c", "TC", 5, 3, 0},
  {"http://typeredest.com/x/d", "TD", 5, 2, 0},
  {"http://typeredest.com/y/e", "TE", 5, 1, 0},
  {"http://daysagoest.com/y/a", "DA", 1, 1, 0},
  {"http://daysagoest.com/y/b", "DB", 1, 1, 1},
  {"http://daysagoest.com/x/c", "DC", 1, 1, 2},
  {"http://daysagoest.com/x/d", "DD", 1, 1, 3},
  {"http://daysagoest.com/y/e", "DE", 1, 1, 4},
  {"http://abcdefghixyzjklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://spaces.com/path%20with%20spaces/foo.html", "Spaces", 2, 2, 0},
  {"http://abcdefghijklxyzmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://abcdefxyzghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://abcxyzdefghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://xyzabcdefghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice",
   "Dogs & Cats & Mice & Other Animals", 1, 1, 0},
  {"https://monkeytrap.org/", "", 3, 1, 0},
  {"http://popularsitewithpathonly.com/moo",
   "popularsitewithpathonly.com/moo", 50, 50, 0},
  {"http://popularsitewithroot.com/", "popularsitewithroot.com", 50, 50, 0},
  {"http://testsearch.com/?q=thequery", "Test Search Engine", 10, 10, 0},
  {"http://testsearch.com/", "Test Search Engine", 9, 9, 0},
  {"http://anotherengine.com/?q=thequery", "Another Search Engine", 8, 8, 0},
  // The encoded stuff between /wiki/ and the # is 第二次世界大戦
  {"http://ja.wikipedia.org/wiki/%E7%AC%AC%E4%BA%8C%E6%AC%A1%E4%B8%96%E7%95"
   "%8C%E5%A4%A7%E6%88%A6#.E3.83.B4.E3.82.A7.E3.83.AB.E3.82.B5.E3.82.A4.E3."
   "83.A6.E4.BD.93.E5.88.B6", "Title Unimportant", 2, 2, 0}
};

// Waits for OnURLsDeletedNotification and when run quits the supplied run loop.
class WaitForURLsDeletedObserver : public history::HistoryServiceObserver {
 public:
  explicit WaitForURLsDeletedObserver(base::RunLoop* runner);
  ~WaitForURLsDeletedObserver() override;

 private:
  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Weak. Owned by our owner.
  base::RunLoop* runner_;

  DISALLOW_COPY_AND_ASSIGN(WaitForURLsDeletedObserver);
};

WaitForURLsDeletedObserver::WaitForURLsDeletedObserver(base::RunLoop* runner)
    : runner_(runner) {
}

WaitForURLsDeletedObserver::~WaitForURLsDeletedObserver() {
}

void WaitForURLsDeletedObserver::OnURLsDeleted(
    history::HistoryService* service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  runner_->Quit();
}

void WaitForURLsDeletedNotification(history::HistoryService* history_service) {
  base::RunLoop runner;
  WaitForURLsDeletedObserver observer(&runner);
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      scoped_observer(&observer);
  scoped_observer.Add(history_service);
  runner.Run();
}

class HistoryQuickProviderTest : public testing::Test {
 public:
  HistoryQuickProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}

 protected:
  class SetShouldContain : public std::unary_function<const std::string&,
                                                      std::set<std::string> > {
   public:
    explicit SetShouldContain(const ACMatches& matched_urls);

    void operator()(const std::string& expected);

    std::set<std::string> LeftOvers() const { return matches_; }

   private:
    std::set<std::string> matches_;
  };

  static scoped_ptr<KeyedService> CreateTemplateURLService(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    return make_scoped_ptr(new TemplateURLService(
        profile->GetPrefs(), make_scoped_ptr(new SearchTermsData), NULL,
        scoped_ptr<TemplateURLServiceClient>(new ChromeTemplateURLServiceClient(
            HistoryServiceFactory::GetForProfile(
                profile, ServiceAccessType::EXPLICIT_ACCESS))),
        NULL, NULL, base::Closure()));
  }

  void SetUp() override;
  void TearDown() override;

  virtual void GetTestData(size_t* data_count, TestURLInfo** test_data);

  // Fills test data into the history system.
  void FillData();

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided. |expected_urls| does not
  // need to be in sorted order.
  void RunTest(const base::string16 text,
               bool prevent_inline_autocomplete,
               std::vector<std::string> expected_urls,
               bool can_inline_top_result,
               base::string16 expected_fill_into_edit,
               base::string16 autocompletion);

  // As above, simply with a cursor position specified.
  void RunTestWithCursor(const base::string16 text,
                         const size_t cursor_position,
                         bool prevent_inline_autocomplete,
                         std::vector<std::string> expected_urls,
                         bool can_inline_top_result,
                         base::string16 expected_fill_into_edit,
                         base::string16 autocompletion);

  history::HistoryBackend* history_backend() {
    return history_service_->history_backend_.get();
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<ChromeAutocompleteProviderClient> client_;
  history::HistoryService* history_service_;

  ACMatches ac_matches_;  // The resulting matches after running RunTest.

  scoped_refptr<HistoryQuickProvider> provider_;
};

void HistoryQuickProviderTest::SetUp() {
  profile_.reset(new TestingProfile());
  client_.reset(new ChromeAutocompleteProviderClient(profile_.get()));
  ASSERT_TRUE(profile_->CreateHistoryService(true, false));
  profile_->CreateBookmarkModel(true);
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(profile_.get()));
  profile_->BlockUntilHistoryIndexIsRefreshed();
  history_service_ = HistoryServiceFactory::GetForProfile(
      profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
  EXPECT_TRUE(history_service_);
  provider_ = new HistoryQuickProvider(client_.get());
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(), &HistoryQuickProviderTest::CreateTemplateURLService);
  FillData();
  InMemoryURLIndex* index =
      InMemoryURLIndexFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(index);
  index->RebuildFromHistory(history_backend()->db());
}

void HistoryQuickProviderTest::TearDown() {
  provider_ = NULL;
}

void HistoryQuickProviderTest::GetTestData(size_t* data_count,
                                           TestURLInfo** test_data) {
  DCHECK(data_count);
  DCHECK(test_data);
  *data_count = arraysize(quick_test_db);
  *test_data = &quick_test_db[0];
}

void HistoryQuickProviderTest::FillData() {
  sql::Connection& db(history_backend()->db()->GetDB());
  ASSERT_TRUE(db.is_open());

  size_t data_count = 0;
  TestURLInfo* test_data = NULL;
  GetTestData(&data_count, &test_data);
  size_t visit_id = 1;
  for (size_t i = 0; i < data_count; ++i) {
    const TestURLInfo& cur(test_data[i]);
    Time visit_time = Time::Now() - TimeDelta::FromDays(cur.days_from_now);
    sql::Transaction transaction(&db);

    // Add URL.
    transaction.Begin();
    std::string sql_cmd_line = base::StringPrintf(
        "INSERT INTO \"urls\" VALUES(%" PRIuS ", \'%s\', \'%s\', %d, %d, %"
        PRId64 ", 0, 0)",
        i + 1, cur.url.c_str(), cur.title.c_str(), cur.visit_count,
        cur.typed_count, visit_time.ToInternalValue());
    sql::Statement sql_stmt(db.GetUniqueStatement(sql_cmd_line.c_str()));
    EXPECT_TRUE(sql_stmt.Run());
    transaction.Commit();

    // Add visits.
    for (int j = 0; j < cur.visit_count; ++j) {
      // Assume earlier visits are at one-day intervals.
      visit_time -= TimeDelta::FromDays(1);
      transaction.Begin();
      // Mark the most recent |cur.typed_count| visits as typed.
      std::string sql_cmd_line = base::StringPrintf(
          "INSERT INTO \"visits\" VALUES(%" PRIuS ", %" PRIuS ", %" PRId64
          ", 0, %d, 0, 1)",
          visit_id++, i + 1, visit_time.ToInternalValue(),
          (j < cur.typed_count) ? ui::PAGE_TRANSITION_TYPED :
                                  ui::PAGE_TRANSITION_LINK);

      sql::Statement sql_stmt(db.GetUniqueStatement(sql_cmd_line.c_str()));
      EXPECT_TRUE(sql_stmt.Run());
      transaction.Commit();
    }
  }
}

HistoryQuickProviderTest::SetShouldContain::SetShouldContain(
    const ACMatches& matched_urls) {
  for (ACMatches::const_iterator iter = matched_urls.begin();
       iter != matched_urls.end(); ++iter)
    matches_.insert(iter->destination_url.spec());
}

void HistoryQuickProviderTest::SetShouldContain::operator()(
    const std::string& expected) {
  EXPECT_EQ(1U, matches_.erase(expected))
      << "Results did not contain '" << expected << "' but should have.";
}

void HistoryQuickProviderTest::RunTest(
    const base::string16 text,
    bool prevent_inline_autocomplete,
    std::vector<std::string> expected_urls,
    bool can_inline_top_result,
    base::string16 expected_fill_into_edit,
    base::string16 expected_autocompletion) {
  RunTestWithCursor(text, base::string16::npos, prevent_inline_autocomplete,
                    expected_urls, can_inline_top_result,
                    expected_fill_into_edit, expected_autocompletion);
}

void HistoryQuickProviderTest::RunTestWithCursor(
    const base::string16 text,
    const size_t cursor_position,
    bool prevent_inline_autocomplete,
    std::vector<std::string> expected_urls,
    bool can_inline_top_result,
    base::string16 expected_fill_into_edit,
    base::string16 expected_autocompletion) {
  SCOPED_TRACE(text);  // Minimal hint to query being run.
  base::MessageLoop::current()->RunUntilIdle();
  AutocompleteInput input(text, cursor_position, std::string(), GURL(),
                          metrics::OmniboxEventProto::INVALID_SPEC,
                          prevent_inline_autocomplete, false, true, true, false,
                          ChromeAutocompleteSchemeClassifier(profile_.get()));
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->done());

  ac_matches_ = provider_->matches();

  // We should have gotten back at most AutocompleteProvider::kMaxMatches.
  EXPECT_LE(ac_matches_.size(), AutocompleteProvider::kMaxMatches);

  // If the number of expected and actual matches aren't equal then we need
  // test no further, but let's do anyway so that we know which URLs failed.
  EXPECT_EQ(expected_urls.size(), ac_matches_.size());

  // Verify that all expected URLs were found and that all found URLs
  // were expected.
  std::set<std::string> leftovers =
      for_each(expected_urls.begin(), expected_urls.end(),
               SetShouldContain(ac_matches_)).LeftOvers();
  EXPECT_EQ(0U, leftovers.size()) << "There were " << leftovers.size()
      << " unexpected results, one of which was: '"
      << *(leftovers.begin()) << "'.";

  if (expected_urls.empty())
    return;

  // Verify that we got the results in the order expected.
  int best_score = ac_matches_.begin()->relevance + 1;
  int i = 0;
  std::vector<std::string>::const_iterator expected = expected_urls.begin();
  for (ACMatches::const_iterator actual = ac_matches_.begin();
       actual != ac_matches_.end() && expected != expected_urls.end();
       ++actual, ++expected, ++i) {
    EXPECT_EQ(*expected, actual->destination_url.spec())
        << "For result #" << i << " we got '" << actual->destination_url.spec()
        << "' but expected '" << *expected << "'.";
    EXPECT_LT(actual->relevance, best_score)
      << "At result #" << i << " (url=" << actual->destination_url.spec()
      << "), we noticed scores are not monotonically decreasing.";
    best_score = actual->relevance;
  }

  EXPECT_EQ(can_inline_top_result, ac_matches_[0].allowed_to_be_default_match);
  if (can_inline_top_result)
    EXPECT_EQ(expected_autocompletion, ac_matches_[0].inline_autocompletion);
  EXPECT_EQ(expected_fill_into_edit, ac_matches_[0].fill_into_edit);
}

TEST_F(HistoryQuickProviderTest, SimpleSingleMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://slashdot.org/favorite_page.html");
  RunTest(ASCIIToUTF16("slashdot"), false, expected_urls, true,
          ASCIIToUTF16("slashdot.org/favorite_page.html"),
                  ASCIIToUTF16(".org/favorite_page.html"));
}

TEST_F(HistoryQuickProviderTest, SingleMatchWithCursor) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://slashdot.org/favorite_page.html");
  // With cursor after "slash", we should retrieve the desired result but it
  // should not be allowed to be the default match.
  RunTestWithCursor(ASCIIToUTF16("slashfavorite_page.html"), 5, false,
                    expected_urls, false,
                    ASCIIToUTF16("slashdot.org/favorite_page.html"),
                    base::string16());
  // If the cursor is in the middle of a valid URL suggestion, it should be
  // allowed to be the default match.  The inline completion will be empty
  // though as no completion is necessary.
  RunTestWithCursor(ASCIIToUTF16("slashdot.org/favorite_page.html"), 5, false,
                    expected_urls, true,
                    ASCIIToUTF16("slashdot.org/favorite_page.html"),
                    base::string16());
}

TEST_F(HistoryQuickProviderTest, WordBoundariesWithPunctuationMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://popularsitewithpathonly.com/moo");
  RunTest(ASCIIToUTF16("/moo"), false, expected_urls, false,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"), base::string16());
}

TEST_F(HistoryQuickProviderTest, MultiTermTitleMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back(
      "http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice");
  RunTest(ASCIIToUTF16("mice other animals"), false, expected_urls, false,
          ASCIIToUTF16("cda.com/Dogs Cats Gorillas Sea Slugs and Mice"),
          base::string16());
}

TEST_F(HistoryQuickProviderTest, NonWordLastCharacterMatch) {
  std::string expected_url("http://slashdot.org/favorite_page.html");
  std::vector<std::string> expected_urls;
  expected_urls.push_back(expected_url);
  RunTest(ASCIIToUTF16("slashdot.org/"), false, expected_urls, true,
          ASCIIToUTF16("slashdot.org/favorite_page.html"),
                       ASCIIToUTF16("favorite_page.html"));
}

TEST_F(HistoryQuickProviderTest, MultiMatch) {
  std::vector<std::string> expected_urls;
  // Scores high because of typed_count.
  expected_urls.push_back("http://foo.com/");
  // Scores high because of visit count.
  expected_urls.push_back("http://foo.com/dir/another/");
  // Scores high because of high visit count.
  expected_urls.push_back("http://foo.com/dir/another/again/");
  RunTest(ASCIIToUTF16("foo"), false, expected_urls, true,
          ASCIIToUTF16("foo.com"), ASCIIToUTF16(".com"));
}

TEST_F(HistoryQuickProviderTest, StartRelativeMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://xyzabcdefghijklmnopqrstuvw.com/a");
  RunTest(ASCIIToUTF16("xyza"), false, expected_urls, true,
          ASCIIToUTF16("xyzabcdefghijklmnopqrstuvw.com/a"),
              ASCIIToUTF16("bcdefghijklmnopqrstuvw.com/a"));
}

TEST_F(HistoryQuickProviderTest, EncodingMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://spaces.com/path%20with%20spaces/foo.html");
  RunTest(ASCIIToUTF16("path with spaces"), false, expected_urls, false,
          ASCIIToUTF16("spaces.com/path with spaces/foo.html"),
          base::string16());
}

TEST_F(HistoryQuickProviderTest, ContentsClass) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back(
      "http://ja.wikipedia.org/wiki/%E7%AC%AC%E4%BA%8C%E6%AC%A1%E4%B8%96%E7"
      "%95%8C%E5%A4%A7%E6%88%A6#.E3.83.B4.E3.82.A7.E3.83.AB.E3.82.B5.E3.82."
      "A4.E3.83.A6.E4.BD.93.E5.88.B6");
  RunTest(base::UTF8ToUTF16("第二 e3"), false, expected_urls, false,
          base::UTF8ToUTF16("ja.wikipedia.org/wiki/第二次世界大戦#.E3.83.B4.E3."
                            "82.A7.E3.83.AB.E3.82.B5.E3.82.A4.E3.83.A6.E4.BD."
                            "93.E5.88.B6"),
          base::string16());
#ifndef NDEBUG
  ac_matches_[0].Validate();
#endif
  // Verify that contents_class divides the string in the right places.
  // [22, 24) is the "第二".  All the other pairs are the "e3".
  ACMatchClassifications contents_class(ac_matches_[0].contents_class);
  size_t expected_offsets[] = { 0, 22, 24, 31, 33, 40, 42, 49, 51, 58, 60, 67,
                                69, 76, 78 };
  // ScoredHistoryMatch may not highlight all the occurrences of these terms
  // because it only highlights terms at word breaks, and it only stores word
  // breaks up to some specified number of characters (50 at the time of this
  // comment).  This test is written flexibly so it still will pass if we
  // increase that number in the future.  Regardless, we require the first
  // five offsets to be correct--in this example these cover at least one
  // occurrence of each term.
  EXPECT_LE(contents_class.size(), arraysize(expected_offsets));
  EXPECT_GE(contents_class.size(), 5u);
  for (size_t i = 0; i < contents_class.size(); ++i)
    EXPECT_EQ(expected_offsets[i], contents_class[i].offset);
}

TEST_F(HistoryQuickProviderTest, VisitCountMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://visitedest.com/y/a");
  expected_urls.push_back("http://visitedest.com/y/b");
  expected_urls.push_back("http://visitedest.com/x/c");
  RunTest(ASCIIToUTF16("visitedest"), false, expected_urls, true,
          ASCIIToUTF16("visitedest.com/y/a"),
                    ASCIIToUTF16(".com/y/a"));
}

TEST_F(HistoryQuickProviderTest, TypedCountMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://typeredest.com/y/a");
  expected_urls.push_back("http://typeredest.com/y/b");
  expected_urls.push_back("http://typeredest.com/x/c");
  RunTest(ASCIIToUTF16("typeredest"), false, expected_urls, true,
          ASCIIToUTF16("typeredest.com/y/a"),
                    ASCIIToUTF16(".com/y/a"));
}

TEST_F(HistoryQuickProviderTest, DaysAgoMatches) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://daysagoest.com/y/a");
  expected_urls.push_back("http://daysagoest.com/y/b");
  expected_urls.push_back("http://daysagoest.com/x/c");
  RunTest(ASCIIToUTF16("daysagoest"), false, expected_urls, true,
          ASCIIToUTF16("daysagoest.com/y/a"),
                    ASCIIToUTF16(".com/y/a"));
}

TEST_F(HistoryQuickProviderTest, EncodingLimitMatch) {
  std::vector<std::string> expected_urls;
  std::string url(
      "http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice");
  // First check that a mid-word match yield no results.
  RunTest(ASCIIToUTF16("ice"), false, expected_urls, false,
          ASCIIToUTF16("cda.com/Dogs Cats Gorillas Sea Slugs and Mice"),
          base::string16());
  // Then check that we get results when the match is at a word start
  // that is present because of an encoded separate (%20 = space).
  expected_urls.push_back(url);
  RunTest(ASCIIToUTF16("Mice"), false, expected_urls, false,
          ASCIIToUTF16("cda.com/Dogs Cats Gorillas Sea Slugs and Mice"),
          base::string16());
  // Verify that the matches' ACMatchClassifications offsets are in range.
  ACMatchClassifications content(ac_matches_[0].contents_class);
  // The max offset accounts for 6 occurrences of '%20' plus the 'http://'.
  const size_t max_offset = url.length() - ((6 * 2) + 7);
  for (ACMatchClassifications::const_iterator citer = content.begin();
       citer != content.end(); ++citer)
    EXPECT_LT(citer->offset, max_offset);
  ACMatchClassifications description(ac_matches_[0].description_class);
  std::string page_title("Dogs & Cats & Mice & Other Animals");
  for (ACMatchClassifications::const_iterator diter = description.begin();
       diter != description.end(); ++diter)
    EXPECT_LT(diter->offset, page_title.length());
}

TEST_F(HistoryQuickProviderTest, Spans) {
  // Test SpansFromTermMatch
  TermMatches matches_a;
  // Simulates matches: '.xx.xxx..xx...xxxxx..' which will test no match at
  // either beginning or end as well as adjacent matches.
  matches_a.push_back(TermMatch(1, 1, 2));
  matches_a.push_back(TermMatch(2, 4, 3));
  matches_a.push_back(TermMatch(3, 9, 1));
  matches_a.push_back(TermMatch(3, 10, 1));
  matches_a.push_back(TermMatch(4, 14, 5));
  ACMatchClassifications spans_a =
      HistoryQuickProvider::SpansFromTermMatch(matches_a, 20, false);
  // ACMatch spans should be: 'NM-NM---N-M-N--M----N-'
  ASSERT_EQ(9U, spans_a.size());
  EXPECT_EQ(0U, spans_a[0].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[0].style);
  EXPECT_EQ(1U, spans_a[1].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[1].style);
  EXPECT_EQ(3U, spans_a[2].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[2].style);
  EXPECT_EQ(4U, spans_a[3].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[3].style);
  EXPECT_EQ(7U, spans_a[4].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[4].style);
  EXPECT_EQ(9U, spans_a[5].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[5].style);
  EXPECT_EQ(11U, spans_a[6].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[6].style);
  EXPECT_EQ(14U, spans_a[7].offset);
  EXPECT_EQ(ACMatchClassification::MATCH, spans_a[7].style);
  EXPECT_EQ(19U, spans_a[8].offset);
  EXPECT_EQ(ACMatchClassification::NONE, spans_a[8].style);
  // Simulates matches: 'xx.xx' which will test matches at both beginning and
  // end.
  TermMatches matches_b;
  matches_b.push_back(TermMatch(1, 0, 2));
  matches_b.push_back(TermMatch(2, 3, 2));
  ACMatchClassifications spans_b =
      HistoryQuickProvider::SpansFromTermMatch(matches_b, 5, true);
  // ACMatch spans should be: 'M-NM-'
  ASSERT_EQ(3U, spans_b.size());
  EXPECT_EQ(0U, spans_b[0].offset);
  EXPECT_EQ(ACMatchClassification::MATCH | ACMatchClassification::URL,
            spans_b[0].style);
  EXPECT_EQ(2U, spans_b[1].offset);
  EXPECT_EQ(ACMatchClassification::URL, spans_b[1].style);
  EXPECT_EQ(3U, spans_b[2].offset);
  EXPECT_EQ(ACMatchClassification::MATCH | ACMatchClassification::URL,
            spans_b[2].style);
}

TEST_F(HistoryQuickProviderTest, DeleteMatch) {
  GURL test_url("http://slashdot.org/favorite_page.html");
  std::vector<std::string> expected_urls;
  expected_urls.push_back(test_url.spec());
  // Fill up ac_matches_; we don't really care about the test yet.
  RunTest(ASCIIToUTF16("slashdot"), false, expected_urls, true,
          ASCIIToUTF16("slashdot.org/favorite_page.html"),
                  ASCIIToUTF16(".org/favorite_page.html"));
  EXPECT_EQ(1U, ac_matches_.size());
  EXPECT_TRUE(history_backend()->GetURL(test_url, NULL));
  provider_->DeleteMatch(ac_matches_[0]);

  // Check that the underlying URL is deleted from the history DB (this implies
  // that all visits are gone as well). Also verify that a deletion notification
  // is sent, in response to which the secondary data stores (InMemoryDatabase,
  // InMemoryURLIndex) will drop any data they might have pertaining to the URL.
  // To ensure that the deletion has been propagated everywhere before we start
  // verifying post-deletion states, first wait until we see the notification.
  WaitForURLsDeletedNotification(history_service_);
  EXPECT_FALSE(history_backend()->GetURL(test_url, NULL));

  // Just to be on the safe side, explicitly verify that we have deleted enough
  // data so that we will not be serving the same result again.
  expected_urls.clear();
  RunTest(ASCIIToUTF16("slashdot"), false, expected_urls, true,
          ASCIIToUTF16("NONE EXPECTED"), base::string16());
}

TEST_F(HistoryQuickProviderTest, PreventBeatingURLWhatYouTypedMatch) {
  std::vector<std::string> expected_urls;

  expected_urls.clear();
  expected_urls.push_back("http://popularsitewithroot.com/");
  // If the user enters a hostname (no path) that they have visited
  // before, we should make sure that all HistoryQuickProvider results
  // have scores less than what HistoryURLProvider will assign the
  // URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithroot.com"), false, expected_urls, true,
          ASCIIToUTF16("popularsitewithroot.com"), base::string16());
  EXPECT_LT(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForBestInlineableResult);

  // Check that if the user didn't quite enter the full hostname, this
  // hostname would've normally scored above the URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithroot.c"), false, expected_urls, true,
          ASCIIToUTF16("popularsitewithroot.com"),
                               ASCIIToUTF16("om"));
  EXPECT_GE(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  expected_urls.clear();
  expected_urls.push_back("http://popularsitewithpathonly.com/moo");
  // If the user enters a hostname of a host that they have visited
  // but never visited the root page of, we should make sure that all
  // HistoryQuickProvider results have scores less than what the
  // HistoryURLProvider will assign the URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.com"), false, expected_urls,
          true,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"),
                                     ASCIIToUTF16("/moo"));
  EXPECT_LT(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForUnvisitedIntranetResult);

  // Verify the same thing happens if the user adds a / to end of the
  // hostname.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.com/"), false, expected_urls,
          true, ASCIIToUTF16("popularsitewithpathonly.com/moo"),
                                            ASCIIToUTF16("moo"));
  EXPECT_LT(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForUnvisitedIntranetResult);

  // Check that if the user didn't quite enter the full hostname, this
  // page would've normally scored above the URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.co"), false, expected_urls,
          true, ASCIIToUTF16("popularsitewithpathonly.com/moo"),
                                          ASCIIToUTF16("m/moo"));
  EXPECT_GE(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  // If the user enters a hostname + path that they have not visited
  // before (but visited other things on the host), we can allow
  // inline autocompletions.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.com/mo"), false, expected_urls,
          true,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"),
                                        ASCIIToUTF16("o"));
  EXPECT_GE(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForWhatYouTypedResult);

  // If the user enters a hostname + path that they have visited
  // before, we should make sure that all HistoryQuickProvider results
  // have scores less than what the HistoryURLProvider will assign
  // the URL-what-you-typed match.
  RunTest(ASCIIToUTF16("popularsitewithpathonly.com/moo"), false,
          expected_urls, true,
          ASCIIToUTF16("popularsitewithpathonly.com/moo"), base::string16());
  EXPECT_LT(ac_matches_[0].relevance,
            HistoryURLProvider::kScoreForBestInlineableResult);
}

TEST_F(HistoryQuickProviderTest, PreventInlineAutocomplete) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://popularsitewithroot.com/");

  // Check that the desired URL is normally allowed to be the default match
  // against input that is a prefex of the URL.
  RunTest(ASCIIToUTF16("popularsitewithr"), false, expected_urls, true,
          ASCIIToUTF16("popularsitewithroot.com"),
                          ASCIIToUTF16("oot.com"));

  // Check that it's not allowed to be the default match if
  // prevent_inline_autocomplete is true.
  RunTest(ASCIIToUTF16("popularsitewithr"), true, expected_urls, false,
          ASCIIToUTF16("popularsitewithroot.com"),
                          ASCIIToUTF16("oot.com"));

  // But the exact hostname can still match even if prevent inline autocomplete
  // is true.  i.e., there's no autocompletion necessary; this is effectively
  // URL-what-you-typed.
  RunTest(ASCIIToUTF16("popularsitewithroot.com"), true, expected_urls, true,
          ASCIIToUTF16("popularsitewithroot.com"), base::string16());

  // The above still holds even with an extra trailing slash.
  RunTest(ASCIIToUTF16("popularsitewithroot.com/"), true, expected_urls, true,
          ASCIIToUTF16("popularsitewithroot.com"), base::string16());
}

TEST_F(HistoryQuickProviderTest, CullSearchResults) {
  // Set up a default search engine.
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("TestEngine"));
  data.SetKeyword(ASCIIToUTF16("TestEngine"));
  data.SetURL("http://testsearch.com/?q={searchTerms}");
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_.get());
  TemplateURL* template_url = new TemplateURL(data);
  template_url_service->Add(template_url);
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  template_url_service->Load();

  // A search results page should not be returned when typing a query.
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://anotherengine.com/?q=thequery");
  RunTest(ASCIIToUTF16("thequery"), false, expected_urls, false,
          ASCIIToUTF16("anotherengine.com/?q=thequery"), base::string16());

  // A search results page should not be returned when typing the engine URL.
  expected_urls.clear();
  expected_urls.push_back("http://testsearch.com/");
  RunTest(ASCIIToUTF16("testsearch"), false, expected_urls, true,
          ASCIIToUTF16("testsearch.com"),
                    ASCIIToUTF16(".com"));
}

TEST_F(HistoryQuickProviderTest, DoesNotProvideMatchesOnFocus) {
  AutocompleteInput input(
      ASCIIToUTF16("popularsite"), base::string16::npos, std::string(), GURL(),
      metrics::OmniboxEventProto::INVALID_SPEC, false, false, true, true, true,
      ChromeAutocompleteSchemeClassifier(profile_.get()));
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

// HQPOrderingTest -------------------------------------------------------------

TestURLInfo ordering_test_db[] = {
  {"http://www.teamliquid.net/tlpd/korean/games/21648_bisu_vs_iris", "", 6, 3,
      256},
  {"http://www.amazon.com/", "amazon.com: online shopping for electronics, "
      "apparel, computers, books, dvds & more", 20, 20, 10},
  {"http://www.teamliquid.net/forum/viewmessage.php?topic_id=52045&"
      "currentpage=83", "google images", 6, 6, 0},
  {"http://www.tempurpedic.com/", "tempur-pedic", 7, 7, 0},
  {"http://www.teamfortress.com/", "", 5, 5, 6},
  {"http://www.rottentomatoes.com/", "", 3, 3, 7},
  {"http://music.google.com/music/listen?u=0#start_pl", "", 3, 3, 9},
  {"https://www.emigrantdirect.com/", "high interest savings account, high "
      "yield savings - emigrantdirect", 5, 5, 3},
  {"http://store.steampowered.com/", "", 6, 6, 1},
  {"http://techmeme.com/", "techmeme", 111, 110, 4},
  {"http://www.teamliquid.net/tlpd", "team liquid progaming database", 15, 15,
      2},
  {"http://store.steampowered.com/", "the steam summer camp sale", 6, 6, 1},
  {"http://www.teamliquid.net/tlpd/korean/players", "tlpd - bw korean - player "
      "index", 25, 7, 219},
  {"http://slashdot.org/", "slashdot: news for nerds, stuff that matters", 3, 3,
      6},
  {"http://translate.google.com/", "google translate", 3, 3, 0},
  {"http://arstechnica.com/", "ars technica", 3, 3, 3},
  {"http://www.rottentomatoes.com/", "movies | movie trailers | reviews - "
      "rotten tomatoes", 3, 3, 7},
  {"http://www.teamliquid.net/", "team liquid - starcraft 2 and brood war pro "
      "gaming news", 26, 25, 3},
  {"http://metaleater.com/", "metaleater", 4, 3, 8},
  {"http://half.com/", "half.com: textbooks , books , music , movies , games , "
      "video games", 4, 4, 6},
  {"http://teamliquid.net/", "team liquid - starcraft 2 and brood war pro "
      "gaming news", 8, 5, 9},
};

class HQPOrderingTest : public HistoryQuickProviderTest {
 protected:
  void GetTestData(size_t* data_count, TestURLInfo** test_data) override;
};

void HQPOrderingTest::GetTestData(size_t* data_count, TestURLInfo** test_data) {
  DCHECK(data_count);
  DCHECK(test_data);
  *data_count = arraysize(ordering_test_db);
  *test_data = &ordering_test_db[0];
}

TEST_F(HQPOrderingTest, TEMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://techmeme.com/");
  expected_urls.push_back("http://www.teamliquid.net/");
  expected_urls.push_back("http://www.teamliquid.net/tlpd");
  RunTest(ASCIIToUTF16("te"), false, expected_urls, true,
          ASCIIToUTF16("techmeme.com"),
            ASCIIToUTF16("chmeme.com"));
}

TEST_F(HQPOrderingTest, TEAMatch) {
  std::vector<std::string> expected_urls;
  expected_urls.push_back("http://www.teamliquid.net/");
  expected_urls.push_back("http://www.teamliquid.net/tlpd");
  expected_urls.push_back("http://www.teamliquid.net/tlpd/korean/players");
  RunTest(ASCIIToUTF16("tea"), false, expected_urls, true,
          ASCIIToUTF16("www.teamliquid.net"),
                 ASCIIToUTF16("mliquid.net"));
}
