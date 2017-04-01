// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/ntp_snippet.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;

std::unique_ptr<NTPSnippet> SnippetFromContentSuggestionJSON(
    const std::string& json) {
  auto json_value = base::JSONReader::Read(json);
  base::DictionaryValue* json_dict;
  if (!json_value->GetAsDictionary(&json_dict)) {
    return nullptr;
  }
  return NTPSnippet::CreateFromContentSuggestionsDictionary(*json_dict,
                                                            kArticlesRemoteId);
}

TEST(NTPSnippetTest, FromChromeContentSuggestionsDictionary) {
  const std::string kJsonStr =
      "{"
      "  \"ids\" : [\"http://localhost/foobar\"],"
      "  \"title\" : \"Foo Barred from Baz\","
      "  \"snippet\" : \"...\","
      "  \"fullPageUrl\" : \"http://localhost/foobar\","
      "  \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "  \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "  \"attribution\" : \"Foo News\","
      "  \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "  \"ampUrl\" : \"http://localhost/amp\","
      "  \"faviconUrl\" : \"http://localhost/favicon.ico\", "
      "  \"score\": 9001,\n"
      "  \"notificationInfo\": {\n"
      "    \"shouldNotify\": true,"
      "    \"deadline\": \"2016-06-30T13:01:37.000Z\"\n"
      "  }\n"
      "}";
  auto snippet = SnippetFromContentSuggestionJSON(kJsonStr);
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://localhost/foobar");
  EXPECT_EQ(snippet->title(), "Foo Barred from Baz");
  EXPECT_EQ(snippet->snippet(), "...");
  EXPECT_EQ(snippet->salient_image_url(), GURL("http://localhost/foobar.jpg"));
  EXPECT_EQ(snippet->score(), 9001);
  auto unix_publish_date = snippet->publish_date() - base::Time::UnixEpoch();
  auto expiry_duration = snippet->expiry_date() - snippet->publish_date();
  EXPECT_FLOAT_EQ(unix_publish_date.InSecondsF(), 1467284497.000000f);
  EXPECT_FLOAT_EQ(expiry_duration.InSecondsF(), 86400.000000f);

  EXPECT_EQ(snippet->publisher_name(), "Foo News");
  EXPECT_EQ(snippet->url(), GURL("http://localhost/foobar"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://localhost/amp"));

  EXPECT_TRUE(snippet->should_notify());
  auto notification_duration =
      snippet->notification_deadline() - snippet->publish_date();
  EXPECT_EQ(7200.0f, notification_duration.InSecondsF());
}

std::unique_ptr<NTPSnippet> SnippetFromChromeReaderDict(
    std::unique_ptr<base::DictionaryValue> dict) {
  if (!dict) {
    return nullptr;
  }
  return NTPSnippet::CreateFromChromeReaderDictionary(*dict);
}

const char kChromeReaderCreationTimestamp[] = "1234567890";
const char kChromeReaderExpiryTimestamp[] = "2345678901";

// Old form, from chromereader-pa.googleapis.com. Two sources.
std::unique_ptr<base::DictionaryValue> ChromeReaderSnippetWithTwoSources() {
  const std::string kJsonStr = base::StringPrintf(
      "{\n"
      "  \"contentInfo\": {\n"
      "    \"url\":                   \"http://url.com\",\n"
      "    \"title\":                 \"Source 1 Title\",\n"
      "    \"snippet\":               \"Source 1 Snippet\",\n"
      "    \"thumbnailUrl\":          \"http://url.com/thumbnail\",\n"
      "    \"creationTimestampSec\":  \"%s\",\n"
      "    \"expiryTimestampSec\":    \"%s\",\n"
      "    \"sourceCorpusInfo\": [{\n"
      "      \"corpusId\":            \"http://source1.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 1\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source1.amp.com\"\n"
      "    }, {\n"
      "      \"corpusId\":            \"http://source2.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 2\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source2.amp.com\"\n"
      "    }]\n"
      "  },\n"
      "  \"score\": 5.0\n"
      "}\n",
      kChromeReaderCreationTimestamp, kChromeReaderExpiryTimestamp);

  auto json_value = base::JSONReader::Read(kJsonStr);
  base::DictionaryValue* json_dict;
  if (!json_value->GetAsDictionary(&json_dict)) {
    return nullptr;
  }
  return json_dict->CreateDeepCopy();
}

TEST(NTPSnippetTest, TestMultipleSources) {
  auto snippet =
      SnippetFromChromeReaderDict(ChromeReaderSnippetWithTwoSources());
  ASSERT_THAT(snippet, NotNull());

  // Expect the first source to be chosen.
  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://source1.amp.com"));
}

TEST(NTPSnippetTest, TestMultipleIncompleteSources1) {
  // Set Source 2 to have no AMP url, and Source 1 to have no publisher name
  // Source 2 should win since we favor publisher name over amp url
  auto dict = ChromeReaderSnippetWithTwoSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(0, &source));
  source->Remove("publisherData.sourceName", nullptr);
  ASSERT_TRUE(sources->GetDictionary(1, &source));
  source->Remove("ampUrl", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source2.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 2"));
  EXPECT_EQ(snippet->amp_url(), GURL());
}

TEST(NTPSnippetTest, TestMultipleIncompleteSources2) {
  // Set Source 1 to have no AMP url, and Source 2 to have no publisher name
  // Source 1 should win in this case since we prefer publisher name to AMP url
  auto dict = ChromeReaderSnippetWithTwoSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(0, &source));
  source->Remove("ampUrl", nullptr);
  ASSERT_TRUE(sources->GetDictionary(1, &source));
  source->Remove("publisherData.sourceName", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet->amp_url(), GURL());
}

TEST(NTPSnippetTest, TestMultipleIncompleteSources3) {
  // Set source 1 to have no AMP url and no source, and source 2 to only have
  // amp url. There should be no snippets since we only add sources we consider
  // complete
  auto dict = ChromeReaderSnippetWithTwoSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(0, &source));
  source->Remove("publisherData.sourceName", nullptr);
  source->Remove("ampUrl", nullptr);
  ASSERT_TRUE(sources->GetDictionary(1, &source));
  source->Remove("publisherData.sourceName", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());
  ASSERT_FALSE(snippet->is_complete());
}

TEST(NTPSnippetTest, ShouldFillInCreation) {
  auto dict = ChromeReaderSnippetWithTwoSources();
  ASSERT_TRUE(dict->Remove("contentInfo.creationTimestampSec", nullptr));
  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  // Publish date should have been filled with "now" - just make sure it's not
  // empty and not the test default value.
  base::Time publish_date = snippet->publish_date();
  EXPECT_FALSE(publish_date.is_null());
  EXPECT_NE(publish_date,
            NTPSnippet::TimeFromJsonString(kChromeReaderCreationTimestamp));
  // Expiry date should have kept the test default value.
  base::Time expiry_date = snippet->expiry_date();
  EXPECT_FALSE(expiry_date.is_null());
  EXPECT_EQ(expiry_date,
            NTPSnippet::TimeFromJsonString(kChromeReaderExpiryTimestamp));
}

TEST(NTPSnippetTest, ShouldFillInExpiry) {
  auto dict = ChromeReaderSnippetWithTwoSources();
  ASSERT_TRUE(dict->Remove("contentInfo.expiryTimestampSec", nullptr));
  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  base::Time publish_date = snippet->publish_date();
  ASSERT_FALSE(publish_date.is_null());
  // Expiry date should have been filled with creation date + offset.
  base::Time expiry_date = snippet->expiry_date();
  EXPECT_FALSE(expiry_date.is_null());
  EXPECT_EQ(publish_date + base::TimeDelta::FromMinutes(
                               kChromeReaderDefaultExpiryTimeMins),
            expiry_date);
}

TEST(NTPSnippetTest, ShouldFillInCreationAndExpiry) {
  auto dict = ChromeReaderSnippetWithTwoSources();
  ASSERT_TRUE(dict->Remove("contentInfo.creationTimestampSec", nullptr));
  ASSERT_TRUE(dict->Remove("contentInfo.expiryTimestampSec", nullptr));
  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  // Publish date should have been filled with "now" - just make sure it's not
  // empty and not the test default value.
  base::Time publish_date = snippet->publish_date();
  EXPECT_FALSE(publish_date.is_null());
  EXPECT_NE(publish_date,
            NTPSnippet::TimeFromJsonString(kChromeReaderCreationTimestamp));
  // Expiry date should have been filled with creation date + offset.
  base::Time expiry_date = snippet->expiry_date();
  EXPECT_FALSE(expiry_date.is_null());
  EXPECT_EQ(publish_date + base::TimeDelta::FromMinutes(
                               kChromeReaderDefaultExpiryTimeMins),
            expiry_date);
}

TEST(NTPSnippetTest, ShouldNotOverwriteExpiry) {
  auto dict = ChromeReaderSnippetWithTwoSources();
  ASSERT_TRUE(dict->Remove("contentInfo.creationTimestampSec", nullptr));
  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  // Expiry date should have kept the test default value.
  base::Time expiry_date = snippet->expiry_date();
  EXPECT_FALSE(expiry_date.is_null());
  EXPECT_EQ(expiry_date,
            NTPSnippet::TimeFromJsonString(kChromeReaderExpiryTimestamp));
}

// Old form, from chromereader-pa.googleapis.com. Three sources.
std::unique_ptr<base::DictionaryValue> ChromeReaderSnippetWithThreeSources() {
  const std::string kJsonStr = base::StringPrintf(
      "{\n"
      "  \"contentInfo\": {\n"
      "    \"url\":                   \"http://url.com\",\n"
      "    \"title\":                 \"Source 1 Title\",\n"
      "    \"snippet\":               \"Source 1 Snippet\",\n"
      "    \"thumbnailUrl\":          \"http://url.com/thumbnail\",\n"
      "    \"creationTimestampSec\":  \"%s\",\n"
      "    \"expiryTimestampSec\":    \"%s\",\n"
      "    \"sourceCorpusInfo\": [{\n"
      "      \"corpusId\":            \"http://source1.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 1\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source1.amp.com\"\n"
      "    }, {\n"
      "      \"corpusId\":            \"http://source2.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 2\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source2.amp.com\"\n"
      "    }, {\n"
      "      \"corpusId\":            \"http://source3.com\",\n"
      "      \"publisherData\": {\n"
      "        \"sourceName\":        \"Source 3\"\n"
      "      },\n"
      "      \"ampUrl\": \"http://source3.amp.com\"\n"
      "    }]\n"
      "  },\n"
      "  \"score\": 5.0\n"
      "}\n",
      kChromeReaderCreationTimestamp, kChromeReaderExpiryTimestamp);

  auto json_value = base::JSONReader::Read(kJsonStr);
  base::DictionaryValue* json_dict;
  if (!json_value->GetAsDictionary(&json_dict)) {
    return nullptr;
  }
  return json_dict->CreateDeepCopy();
}

TEST(NTPSnippetTest, TestMultipleCompleteSources1) {
  // Test 2 complete sources, we should choose the first complete source
  auto dict = ChromeReaderSnippetWithThreeSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(1, &source));
  source->Remove("publisherData.sourceName", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_THAT(snippet->GetAllIDs(),
              ElementsAre("http://url.com", "http://source1.com",
                          "http://source2.com", "http://source3.com"));
  EXPECT_EQ(snippet->url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://source1.amp.com"));
}

TEST(NTPSnippetTest, TestMultipleCompleteSources2) {
  // Test 2 complete sources, we should choose the first complete source
  auto dict = ChromeReaderSnippetWithThreeSources();
  base::ListValue* sources;
  ASSERT_TRUE(dict->GetList("contentInfo.sourceCorpusInfo", &sources));
  base::DictionaryValue* source;
  ASSERT_TRUE(sources->GetDictionary(0, &source));
  source->Remove("publisherData.sourceName", nullptr);

  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source2.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 2"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://source2.amp.com"));
}

TEST(NTPSnippetTest, TestMultipleCompleteSources3) {
  // Test 3 complete sources, we should choose the first complete source
  auto dict = ChromeReaderSnippetWithThreeSources();
  auto snippet = SnippetFromChromeReaderDict(std::move(dict));
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://url.com");
  EXPECT_EQ(snippet->url(), GURL("http://source1.com"));
  EXPECT_EQ(snippet->publisher_name(), std::string("Source 1"));
  EXPECT_EQ(snippet->amp_url(), GURL("http://source1.amp.com"));
}

TEST(NTPSnippetTest, ShouldSupportMultipleIdsFromContentSuggestionsServer) {
  const std::string kJsonStr =
      "{"
      "  \"ids\" : [\"http://localhost/foobar\", \"012345\"],"
      "  \"title\" : \"Foo Barred from Baz\","
      "  \"snippet\" : \"...\","
      "  \"fullPageUrl\" : \"http://localhost/foobar\","
      "  \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "  \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "  \"attribution\" : \"Foo News\","
      "  \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "  \"ampUrl\" : \"http://localhost/amp\","
      "  \"faviconUrl\" : \"http://localhost/favicon.ico\" "
      "}";
  auto snippet = SnippetFromContentSuggestionJSON(kJsonStr);
  ASSERT_THAT(snippet, NotNull());

  EXPECT_EQ(snippet->id(), "http://localhost/foobar");
  EXPECT_THAT(snippet->GetAllIDs(),
              ElementsAre("http://localhost/foobar", "012345"));
}

TEST(NTPSnippetTest, CreateFromProtoToProtoRoundtrip) {
  SnippetProto proto;
  proto.add_ids("foo");
  proto.add_ids("bar");
  proto.set_title("a suggestion title");
  proto.set_snippet("the snippet describing the suggestion.");
  proto.set_salient_image_url("http://google.com/logo/");
  proto.set_publish_date(1476095492);
  proto.set_expiry_date(1476354691);
  proto.set_score(0.1f);
  proto.set_dismissed(false);
  proto.set_remote_category_id(1);
  auto source = proto.add_sources();
  source->set_url("http://cool-suggestions.com/");
  source->set_publisher_name("Great Suggestions Inc.");
  source->set_amp_url("http://cdn.ampproject.org/c/foo/");

  std::unique_ptr<NTPSnippet> snippet = NTPSnippet::CreateFromProto(proto);
  ASSERT_THAT(snippet, NotNull());
  // The snippet database relies on the fact that the first id in the protocol
  // buffer is considered the unique id.
  EXPECT_EQ(snippet->id(), "foo");
  // Unfortunately, we only have MessageLite protocol buffers in Chrome, so
  // comparing via DebugString() or MessageDifferencer is not working.
  // So we either need to compare field-by-field (maintenance heavy) or
  // compare the binary version (unusable diagnostic). Deciding for the latter.
  std::string proto_serialized, round_tripped_serialized;
  proto.SerializeToString(&proto_serialized);
  snippet->ToProto().SerializeToString(&round_tripped_serialized);
  EXPECT_EQ(proto_serialized, round_tripped_serialized);
}

// New form, from chromecontentsuggestions-pa.googleapis.com.
std::unique_ptr<base::DictionaryValue> ContentSuggestionSnippet() {
  const std::string kJsonStr =
      "{"
      "  \"ids\" : [\"http://localhost/foobar\"],"
      "  \"title\" : \"Foo Barred from Baz\","
      "  \"snippet\" : \"...\","
      "  \"fullPageUrl\" : \"http://localhost/foobar\","
      "  \"creationTime\" : \"2016-06-30T11:01:37.000Z\","
      "  \"expirationTime\" : \"2016-07-01T11:01:37.000Z\","
      "  \"attribution\" : \"Foo News\","
      "  \"imageUrl\" : \"http://localhost/foobar.jpg\","
      "  \"ampUrl\" : \"http://localhost/amp\","
      "  \"faviconUrl\" : \"http://localhost/favicon.ico\", "
      "  \"score\": 9001\n"
      "}";
  auto json_value = base::JSONReader::Read(kJsonStr);
  base::DictionaryValue* json_dict;
  CHECK(json_value->GetAsDictionary(&json_dict));
  return json_dict->CreateDeepCopy();
}

TEST(NTPSnippetTest, NotifcationInfoAllSpecified) {
  auto json = ContentSuggestionSnippet();
  json->SetBoolean("notificationInfo.shouldNotify", true);
  json->SetString("notificationInfo.deadline", "2016-06-30T13:01:37.000Z");
  auto snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*json, 0);
  EXPECT_TRUE(snippet->should_notify());
  EXPECT_EQ(7200.0f,
            (snippet->notification_deadline() - snippet->publish_date())
                .InSecondsF());
}

TEST(NTPSnippetTest, NotificationInfoDeadlineInvalid) {
  auto json = ContentSuggestionSnippet();
  json->SetBoolean("notificationInfo.shouldNotify", true);
  json->SetInteger("notificationInfo.notificationDeadline", 0);
  auto snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*json, 0);
  EXPECT_TRUE(snippet->should_notify());
  EXPECT_EQ(base::Time::Max(), snippet->notification_deadline());
}

TEST(NTPSnippetTest, NotificationInfoDeadlineAbsent) {
  auto json = ContentSuggestionSnippet();
  json->SetBoolean("notificationInfo.shouldNotify", true);
  auto snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*json, 0);
  EXPECT_TRUE(snippet->should_notify());
  EXPECT_EQ(base::Time::Max(), snippet->notification_deadline());
}

TEST(NTPSnippetTest, NotificationInfoShouldNotifyInvalid) {
  auto json = ContentSuggestionSnippet();
  json->SetString("notificationInfo.shouldNotify", "non-bool");
  auto snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*json, 0);
  EXPECT_FALSE(snippet->should_notify());
}

TEST(NTPSnippetTest, NotificationInfoAbsent) {
  auto json = ContentSuggestionSnippet();
  auto snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*json, 0);
  EXPECT_FALSE(snippet->should_notify());
}

TEST(NTPSnippetTest, ToContentSuggestion) {
  auto json = ContentSuggestionSnippet();
  auto snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*json, 0);
  ASSERT_THAT(snippet, NotNull());
  ContentSuggestion sugg = snippet->ToContentSuggestion(
      Category::FromKnownCategory(KnownCategories::ARTICLES));

  EXPECT_THAT(sugg.id().category(),
              Eq(Category::FromKnownCategory(KnownCategories::ARTICLES)));
  EXPECT_THAT(sugg.id().id_within_category(), Eq("http://localhost/foobar"));
  EXPECT_THAT(sugg.url(), Eq(GURL("http://localhost/amp")));
  EXPECT_THAT(sugg.title(), Eq(base::UTF8ToUTF16("Foo Barred from Baz")));
  EXPECT_THAT(sugg.snippet_text(), Eq(base::UTF8ToUTF16("...")));
  EXPECT_THAT(sugg.publish_date().ToJavaTime(), Eq(1467284497000));
  EXPECT_THAT(sugg.publisher_name(), Eq(base::UTF8ToUTF16("Foo News")));
  EXPECT_THAT(sugg.score(), Eq(9001));
  EXPECT_THAT(sugg.download_suggestion_extra(), IsNull());
  EXPECT_THAT(sugg.recent_tab_suggestion_extra(), IsNull());
  EXPECT_THAT(sugg.notification_extra(), IsNull());
}

TEST(NTPSnippetTest, ToContentSuggestionWithNotificationInfo) {
  auto json = ContentSuggestionSnippet();
  json->SetBoolean("notificationInfo.shouldNotify", true);
  json->SetString("notificationInfo.deadline", "2016-06-30T13:01:37.000Z");
  auto snippet = NTPSnippet::CreateFromContentSuggestionsDictionary(*json, 0);
  ASSERT_THAT(snippet, NotNull());
  ContentSuggestion sugg = snippet->ToContentSuggestion(
      Category::FromKnownCategory(KnownCategories::ARTICLES));

  EXPECT_THAT(sugg.id().category(),
              Eq(Category::FromKnownCategory(KnownCategories::ARTICLES)));
  EXPECT_THAT(sugg.id().id_within_category(), Eq("http://localhost/foobar"));
  EXPECT_THAT(sugg.url(), Eq(GURL("http://localhost/amp")));
  EXPECT_THAT(sugg.title(), Eq(base::UTF8ToUTF16("Foo Barred from Baz")));
  EXPECT_THAT(sugg.snippet_text(), Eq(base::UTF8ToUTF16("...")));
  EXPECT_THAT(sugg.publish_date().ToJavaTime(), Eq(1467284497000));
  EXPECT_THAT(sugg.publisher_name(), Eq(base::UTF8ToUTF16("Foo News")));
  EXPECT_THAT(sugg.score(), Eq(9001));
  EXPECT_THAT(sugg.download_suggestion_extra(), IsNull());
  EXPECT_THAT(sugg.recent_tab_suggestion_extra(), IsNull());
  ASSERT_THAT(sugg.notification_extra(), NotNull());
  EXPECT_THAT(sugg.notification_extra()->deadline.ToJavaTime(),
              Eq(1467291697000));
}

}  // namespace
}  // namespace ntp_snippets
