// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace ntp_snippets {

namespace {

const char kBookmarkLastVisitDateKey[] = "last_visited";

std::unique_ptr<BookmarkModel> CreateModelWithRecentBookmarks(
    int number_of_bookmarks,
    int number_of_recent,
    const base::Time& threshold_time) {
  std::unique_ptr<BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();

  base::TimeDelta week = base::TimeDelta::FromDays(7);
  base::Time recent_time = threshold_time + week;
  std::string recent_time_string =
      base::Int64ToString(recent_time.ToInternalValue());
  base::Time nonrecent_time = threshold_time - week;
  std::string nonrecent_time_string =
      base::Int64ToString(nonrecent_time.ToInternalValue());

  for (int index = 0; index < number_of_bookmarks; ++index) {
    base::string16 title =
        base::ASCIIToUTF16(base::StringPrintf("title%d", index));
    GURL url(base::StringPrintf("http://url%d.com", index));
    const BookmarkNode* node =
        model->AddURL(model->bookmark_bar_node(), index, title, url);

    model->SetNodeMetaInfo(
        node, kBookmarkLastVisitDateKey,
        index < number_of_recent ? recent_time_string : nonrecent_time_string);
  }

  return model;
}

}  // namespace

class GetRecentlyVisitedBookmarksTest : public testing::Test {
 public:
  GetRecentlyVisitedBookmarksTest() {
    base::TimeDelta week = base::TimeDelta::FromDays(7);
    threshold_time_ = base::Time::UnixEpoch() + 52 * week;
  }

  const base::Time& threshold_time() const { return threshold_time_; }

 private:
  base::Time threshold_time_;

  DISALLOW_COPY_AND_ASSIGN(GetRecentlyVisitedBookmarksTest);
};

TEST_F(GetRecentlyVisitedBookmarksTest,
       WithoutDateFallbackShouldNotReturnNonRecent) {
  const int number_of_recent = 0;
  const int number_of_bookmarks = 3;
  std::unique_ptr<BookmarkModel> model = CreateModelWithRecentBookmarks(
      number_of_bookmarks, number_of_recent, threshold_time());

  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), 0, number_of_bookmarks,
                                  threshold_time(),
                                  /*creation_date_fallback=*/false);
  EXPECT_THAT(result, IsEmpty());
}

TEST_F(GetRecentlyVisitedBookmarksTest,
       WithDateFallbackShouldReturnNonRecentUpToMinCount) {
  const int number_of_recent = 0;
  const int number_of_bookmarks = 3;
  std::unique_ptr<BookmarkModel> model = CreateModelWithRecentBookmarks(
      number_of_bookmarks, number_of_recent, threshold_time());

  const int min_count = number_of_bookmarks - 1;
  const int max_count = min_count + 10;
  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), min_count, max_count,
                                  threshold_time(),
                                  /*creation_date_fallback=*/true);
  EXPECT_THAT(result, SizeIs(min_count));
}

TEST_F(GetRecentlyVisitedBookmarksTest, ShouldReturnNotMoreThanMaxCount) {
  const int number_of_recent = 3;
  const int number_of_bookmarks = number_of_recent;
  std::unique_ptr<BookmarkModel> model = CreateModelWithRecentBookmarks(
      number_of_bookmarks, number_of_recent, threshold_time());

  const int max_count = number_of_recent - 1;
  std::vector<const bookmarks::BookmarkNode*> result =
      GetRecentlyVisitedBookmarks(model.get(), max_count, max_count,
                                  threshold_time(),
                                  /*creation_date_fallback=*/false);
  EXPECT_THAT(result, SizeIs(max_count));
}

}  // namespace ntp_snippets
