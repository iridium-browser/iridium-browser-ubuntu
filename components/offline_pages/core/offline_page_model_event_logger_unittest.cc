// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_model_event_logger.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const char kNamespace[] = "last_n";
const char kUrl[] = "http://www.wikipedia.org";
const int64_t kOfflineId = 12345L;
const int kTimeLength = 21;
const char kPageSaved[] =
    "http://www.wikipedia.org is saved at last_n with id 12345";
const char kPageDeleted[] = "Page with ID 12345 has been deleted";
const char kPageExpired[] = "Page with ID 12345 has been expired";
const char kRecordStoreClearError[] = "Offline store clear failed";
const char kRecordStoreCleared[] = "Offline store cleared";
const char kRecordStoreReloadError[] =
    "There was an error reloading the offline store";

}  // namespace

TEST(OfflinePageModelEventLoggerTest, RecordsWhenLoggingIsOn) {
  OfflinePageModelEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(true);
  logger.RecordStoreCleared();
  logger.RecordPageSaved(kNamespace, kUrl, kOfflineId);
  logger.RecordPageDeleted(kOfflineId);
  logger.RecordPageExpired(kOfflineId);
  logger.RecordStoreClearError();
  logger.RecordStoreReloadError();
  logger.GetLogs(&log);

  EXPECT_EQ(6u, log.size());
  EXPECT_EQ(std::string(kRecordStoreCleared), log[5].substr(kTimeLength));
  EXPECT_EQ(std::string(kPageSaved), log[4].substr(kTimeLength));
  EXPECT_EQ(std::string(kPageDeleted), log[3].substr(kTimeLength));
  EXPECT_EQ(std::string(kPageExpired), log[2].substr(kTimeLength));
  EXPECT_EQ(std::string(kRecordStoreClearError), log[1].substr(kTimeLength));
  EXPECT_EQ(std::string(kRecordStoreReloadError), log[0].substr(kTimeLength));
}

TEST(OfflinePageModelEventLoggerTest, DoesNotRecordWhenLoggingIsOff) {
  OfflinePageModelEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(false);
  logger.RecordStoreCleared();
  logger.RecordPageSaved(kNamespace, kUrl, kOfflineId);
  logger.RecordPageDeleted(kOfflineId);
  logger.RecordPageExpired(kOfflineId);
  logger.RecordStoreClearError();
  logger.RecordStoreReloadError();
  logger.GetLogs(&log);

  EXPECT_EQ(0u, log.size());
}

TEST(OfflinePageModelEventLoggerTest, DoesNotExceedMaxSize) {
  OfflinePageModelEventLogger logger;
  std::vector<std::string> log;

  logger.SetIsLogging(true);
  for (size_t i = 0; i < kMaxLogCount + 1; ++i) {
    logger.RecordStoreCleared();
  }
  logger.GetLogs(&log);

  EXPECT_EQ(kMaxLogCount, log.size());
}

}  // namespace offline_pages
