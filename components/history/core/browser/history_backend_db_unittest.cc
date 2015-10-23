// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History unit tests come in two flavors:
//
// 1. The more complicated style is that the unit test creates a full history
//    service. This spawns a background thread for the history backend, and
//    all communication is asynchronous. This is useful for testing more
//    complicated things or end-to-end behavior.
//
// 2. The simpler style is to create a history backend on this thread and
//    access it directly without a HistoryService object. This is much simpler
//    because communication is synchronous. Generally, sets should go through
//    the history backend (since there is a lot of logic) but gets can come
//    directly from the HistoryDatabase. This is because the backend generally
//    has no logic in the getter except threading stuff, which we don't want
//    to run.

#include "components/history/core/browser/history_backend.h"

#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/download_constants.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/test/history_backend_db_base_test.h"
#include "components/history/core/test/test_history_database.h"

namespace history {
namespace {

// This must be outside the anonymous namespace for the friend statement in
// HistoryBackend to work.
class HistoryBackendDBTest : public HistoryBackendDBBaseTest {
 public:
  HistoryBackendDBTest() {}
  ~HistoryBackendDBTest() override {}
};

TEST_F(HistoryBackendDBTest, ClearBrowsingData_Downloads) {
  CreateBackendAndDatabase();

  // Initially there should be nothing in the downloads database.
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // Add a download, test that it was added correctly, remove it, test that it
  // was removed.
  base::Time now = base::Time();
  uint32 id = 1;
  EXPECT_TRUE(AddDownload(id, DownloadState::COMPLETE, base::Time()));
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());

  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("current-path")),
            downloads[0].current_path);
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("target-path")),
            downloads[0].target_path);
  EXPECT_EQ(1UL, downloads[0].url_chain.size());
  EXPECT_EQ(GURL("foo-url"), downloads[0].url_chain[0]);
  EXPECT_EQ(std::string("http://referrer.com/"),
            std::string(downloads[0].referrer_url.spec()));
  EXPECT_EQ(now, downloads[0].start_time);
  EXPECT_EQ(now, downloads[0].end_time);
  EXPECT_EQ(0, downloads[0].received_bytes);
  EXPECT_EQ(512, downloads[0].total_bytes);
  EXPECT_EQ(DownloadState::COMPLETE, downloads[0].state);
  EXPECT_EQ(DownloadDangerType::NOT_DANGEROUS, downloads[0].danger_type);
  EXPECT_EQ(kTestDownloadInterruptReasonNone, downloads[0].interrupt_reason);
  EXPECT_FALSE(downloads[0].opened);
  EXPECT_EQ("by_ext_id", downloads[0].by_ext_id);
  EXPECT_EQ("by_ext_name", downloads[0].by_ext_name);
  EXPECT_EQ("application/vnd.oasis.opendocument.text", downloads[0].mime_type);
  EXPECT_EQ("application/octet-stream", downloads[0].original_mime_type);

  db_->QueryDownloads(&downloads);
  EXPECT_EQ(1U, downloads.size());
  db_->RemoveDownload(id);
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());
}

TEST_F(HistoryBackendDBTest, MigrateDownloadsState) {
  // Create the db we want.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    // Open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Manually insert corrupted rows; there's infrastructure in place now to
    // make this impossible, at least according to the test above.
    for (int state = 0; state < 5; ++state) {
      sql::Statement s(db.GetUniqueStatement(
            "INSERT INTO downloads (id, full_path, url, start_time, "
            "received_bytes, total_bytes, state, end_time, opened) VALUES "
            "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1 + state);
      s.BindString(1, "path");
      s.BindString(2, "url");
      s.BindInt64(3, base::Time::Now().ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, state);
      s.BindInt64(7, base::Time::Now().ToTimeT());
      s.BindInt(8, state % 2);
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db using the HistoryDatabase, which should migrate from version
  // 22 to the current version, fixing just the row whose state was 3.
  // Then close the db so that we can re-open it directly.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // The version should have been updated.
      int cur_version = HistoryDatabase::GetCurrentVersion();
      ASSERT_LT(22, cur_version);
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, state, opened "
          "FROM downloads "
          "ORDER BY id"));
      int counter = 0;
      while (statement.Step()) {
        EXPECT_EQ(1 + counter, statement.ColumnInt64(0));
        // The only thing that migration should have changed was state from 3 to
        // 4.
        EXPECT_EQ(((counter == 3) ? 4 : counter), statement.ColumnInt(1));
        EXPECT_EQ(counter % 2, statement.ColumnInt(2));
        ++counter;
      }
      EXPECT_EQ(5, counter);
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadsReasonPathsAndDangerType) {
  base::Time now(base::Time::Now());

  // Create the db we want.  The schema didn't change from 22->23, so just
  // re-use the v22 file.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Manually insert some rows.
    sql::Statement s(db.GetUniqueStatement(
        "INSERT INTO downloads (id, full_path, url, start_time, "
        "received_bytes, total_bytes, state, end_time, opened) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    int64 id = 0;
    // Null path.
    s.BindInt64(0, ++id);
    s.BindString(1, std::string());
    s.BindString(2, "http://whatever.com/index.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
    s.Reset(true);

    // Non-null path.
    s.BindInt64(0, ++id);
    s.BindString(1, "/path/to/some/file");
    s.BindString(2, "http://whatever.com/index1.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
  }

  // Re-open the db using the HistoryDatabase, which should migrate from version
  // 23 to 24, creating the new tables and creating the new path, reason,
  // and danger columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // The version should have been updated.
      int cur_version = HistoryDatabase::GetCurrentVersion();
      ASSERT_LT(23, cur_version);
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      base::Time nowish(base::Time::FromTimeT(now.ToTimeT()));

      // Confirm downloads table is valid.
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, interrupt_reason, current_path, target_path, "
          "       danger_type, start_time, end_time "
          "FROM downloads ORDER BY id"));
      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(1, statement.ColumnInt64(0));
      EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
                statement.ColumnInt(1));
      EXPECT_EQ("", statement.ColumnString(2));
      EXPECT_EQ("", statement.ColumnString(3));
      // Implicit dependence on value of kDangerTypeNotDangerous from
      // download_database.cc.
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(5));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(6));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
                statement.ColumnInt(1));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(2));
      EXPECT_EQ("/path/to/some/file", statement.ColumnString(3));
      EXPECT_EQ(0, statement.ColumnInt(4));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(5));
      EXPECT_EQ(nowish.ToInternalValue(), statement.ColumnInt64(6));

      EXPECT_FALSE(statement.Step());
    }
    {
      // Confirm downloads_url_chains table is valid.
      sql::Statement statement(db.GetUniqueStatement(
          "SELECT id, chain_index, url FROM downloads_url_chains "
          " ORDER BY id, chain_index"));
      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(1, statement.ColumnInt64(0));
      EXPECT_EQ(0, statement.ColumnInt(1));
      EXPECT_EQ("http://whatever.com/index.html", statement.ColumnString(2));

      EXPECT_TRUE(statement.Step());
      EXPECT_EQ(2, statement.ColumnInt64(0));
      EXPECT_EQ(0, statement.ColumnInt(1));
      EXPECT_EQ("http://whatever.com/index1.html", statement.ColumnString(2));

      EXPECT_FALSE(statement.Step());
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateReferrer) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement s(db.GetUniqueStatement(
        "INSERT INTO downloads (id, full_path, url, start_time, "
        "received_bytes, total_bytes, state, end_time, opened) VALUES "
        "(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    int64 db_handle = 0;
    s.BindInt64(0, ++db_handle);
    s.BindString(1, "full_path");
    s.BindString(2, "http://whatever.com/index.html");
    s.BindInt64(3, now.ToTimeT());
    s.BindInt64(4, 100);
    s.BindInt64(5, 100);
    s.BindInt(6, 1);
    s.BindInt64(7, now.ToTimeT());
    s.BindInt(8, 1);
    ASSERT_TRUE(s.Run());
  }
  // Re-open the db using the HistoryDatabase, which should migrate to version
  // 26, creating the referrer column.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(26, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT referrer from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadedByExtension) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(26));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to version
  // 27, creating the by_ext_id and by_ext_name columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(27, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT by_ext_id, by_ext_name from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, MigrateDownloadValidators) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(27));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer, by_ext_id, by_ext_name) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      s.BindString(12, "by extension ID");
      s.BindString(13, "by extension name");
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating the etag and last_modified columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(28, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT etag, last_modified from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, PurgeArchivedDatabase) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(27));
  ASSERT_NO_FATAL_FAILURE(CreateArchivedDB());

  ASSERT_TRUE(base::PathExists(history_dir_.Append(kArchivedHistoryFilename)));

  CreateBackendAndDatabase();
  DeleteBackend();

  // We do not retain expired history entries in an archived database as of M37.
  // Verify that any legacy archived database is deleted on start-up.
  ASSERT_FALSE(base::PathExists(history_dir_.Append(kArchivedHistoryFilename)));
}

TEST_F(HistoryBackendDBTest, MigrateDownloadMimeType) {
  base::Time now(base::Time::Now());
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(28));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads (id, current_path, target_path, start_time, "
          "received_bytes, total_bytes, state, danger_type, interrupt_reason, "
          "end_time, opened, referrer, by_ext_id, by_ext_name, etag, "
          "last_modified) VALUES "
          "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
      s.BindInt64(0, 1);
      s.BindString(1, "current_path");
      s.BindString(2, "target_path");
      s.BindInt64(3, now.ToTimeT());
      s.BindInt64(4, 100);
      s.BindInt64(5, 100);
      s.BindInt(6, 1);
      s.BindInt(7, 0);
      s.BindInt(8, 0);
      s.BindInt64(9, now.ToTimeT());
      s.BindInt(10, 1);
      s.BindString(11, "referrer");
      s.BindString(12, "by extension ID");
      s.BindString(13, "by extension name");
      s.BindString(14, "etag");
      s.BindInt64(15, now.ToTimeT());
      ASSERT_TRUE(s.Run());
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "INSERT INTO downloads_url_chains (id, chain_index, url) VALUES "
          "(?, ?, ?)"));
      s.BindInt64(0, 4);
      s.BindInt64(1, 0);
      s.BindString(2, "url");
      ASSERT_TRUE(s.Run());
    }
  }
  // Re-open the db using the HistoryDatabase, which should migrate to the
  // current version, creating themime_type abd original_mime_type columns.
  CreateBackendAndDatabase();
  DeleteBackend();
  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    // The version should have been updated.
    int cur_version = HistoryDatabase::GetCurrentVersion();
    ASSERT_LE(29, cur_version);
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT value FROM meta WHERE key = 'version'"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(cur_version, s.ColumnInt(0));
    }
    {
      sql::Statement s(db.GetUniqueStatement(
          "SELECT mime_type, original_mime_type from downloads"));
      EXPECT_TRUE(s.Step());
      EXPECT_EQ(std::string(), s.ColumnString(0));
      EXPECT_EQ(std::string(), s.ColumnString(1));
    }
  }
}

TEST_F(HistoryBackendDBTest, ConfirmDownloadRowCreateAndDelete) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Add some downloads.
  uint32 id1 = 1, id2 = 2, id3 = 3;
  AddDownload(id1, DownloadState::COMPLETE, now);
  AddDownload(id2, DownloadState::COMPLETE, now + base::TimeDelta::FromDays(2));
  AddDownload(id3, DownloadState::COMPLETE, now - base::TimeDelta::FromDays(2));

  // Confirm that resulted in the correct number of rows in the DB.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(3, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(3, statement1.ColumnInt(0));
  }

  // Delete some rows and make sure the results are still correct.
  CreateBackendAndDatabase();
  db_->RemoveDownload(id2);
  db_->RemoveDownload(id3);
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select Count(*) from downloads_url_chains"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(1, statement1.ColumnInt(0));
  }
}

TEST_F(HistoryBackendDBTest, DownloadNukeRecordsMissingURLs) {
  CreateBackendAndDatabase();
  base::Time now(base::Time::Now());
  std::vector<GURL> url_chain;
  DownloadRow download(base::FilePath(FILE_PATH_LITERAL("foo-path")),
                       base::FilePath(FILE_PATH_LITERAL("foo-path")),
                       url_chain,
                       GURL(std::string()),
                       "application/octet-stream",
                       "application/octet-stream",
                       now,
                       now,
                       std::string(),
                       std::string(),
                       0,
                       512,
                       DownloadState::COMPLETE,
                       DownloadDangerType::NOT_DANGEROUS,
                       kTestDownloadInterruptReasonNone,
                       1,
                       0,
                       "by_ext_id",
                       "by_ext_name");

  // Creating records without any urls should fail.
  EXPECT_FALSE(db_->CreateDownload(download));

  download.url_chain.push_back(GURL("foo-url"));
  EXPECT_TRUE(db_->CreateDownload(download));

  // Pretend that the URLs were dropped.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "DELETE FROM downloads_url_chains WHERE id=1"));
    ASSERT_TRUE(statement.Run());
  }
  CreateBackendAndDatabase();
  std::vector<DownloadRow> downloads;
  db_->QueryDownloads(&downloads);
  EXPECT_EQ(0U, downloads.size());

  // QueryDownloads should have nuked the corrupt record.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::Statement statement(db.GetUniqueStatement(
            "SELECT count(*) from downloads"));
      ASSERT_TRUE(statement.Step());
      EXPECT_EQ(0, statement.ColumnInt(0));
    }
  }
}

TEST_F(HistoryBackendDBTest, ConfirmDownloadInProgressCleanup) {
  // Create the DB.
  CreateBackendAndDatabase();

  base::Time now(base::Time::Now());

  // Put an IN_PROGRESS download in the DB.
  AddDownload(1, DownloadState::IN_PROGRESS, now);

  // Confirm that they made it into the DB unchanged.
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select state, interrupt_reason from downloads"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(DownloadStateToInt(DownloadState::IN_PROGRESS),
              statement1.ColumnInt(0));
    EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonNone),
              statement1.ColumnInt(1));
    EXPECT_FALSE(statement1.Step());
  }

  // Read in the DB through query downloads, then test that the
  // right transformation was returned.
  CreateBackendAndDatabase();
  std::vector<DownloadRow> results;
  db_->QueryDownloads(&results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(DownloadState::INTERRUPTED, results[0].state);
  EXPECT_EQ(kTestDownloadInterruptReasonCrash, results[0].interrupt_reason);

  // Allow the update to propagate, shut down the DB, and confirm that
  // the query updated the on disk database as well.
  base::MessageLoop::current()->RunUntilIdle();
  DeleteBackend();
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    sql::Statement statement(db.GetUniqueStatement(
        "Select Count(*) from downloads"));
    EXPECT_TRUE(statement.Step());
    EXPECT_EQ(1, statement.ColumnInt(0));

    sql::Statement statement1(db.GetUniqueStatement(
        "Select state, interrupt_reason from downloads"));
    EXPECT_TRUE(statement1.Step());
    EXPECT_EQ(DownloadStateToInt(DownloadState::INTERRUPTED),
              statement1.ColumnInt(0));
    EXPECT_EQ(DownloadInterruptReasonToInt(kTestDownloadInterruptReasonCrash),
              statement1.ColumnInt(1));
    EXPECT_FALSE(statement1.Step());
  }
}

TEST_F(HistoryBackendDBTest, MigratePresentations) {
  // Create the db we want. Use 22 since segments didn't change in that time
  // frame.
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(22));

  const SegmentID segment_id = 2;
  const URLID url_id = 3;
  const GURL url("http://www.foo.com");
  const std::string url_name(VisitSegmentDatabase::ComputeSegmentName(url));
  const base::string16 title(base::ASCIIToUTF16("Title1"));
  const base::Time segment_time(base::Time::Now());

  {
    // Re-open the db for manual manipulation.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));

    // Add an entry to urls.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO urls "
                           "(id, url, title, last_visit_time) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, url_id);
      s.BindString(1, url.spec());
      s.BindString16(2, title);
      s.BindInt64(3, segment_time.ToInternalValue());
      ASSERT_TRUE(s.Run());
    }

    // Add an entry to segments.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO segments "
                           "(id, name, url_id, pres_index) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, segment_id);
      s.BindString(1, url_name);
      s.BindInt64(2, url_id);
      s.BindInt(3, 4);  // pres_index
      ASSERT_TRUE(s.Run());
    }

    // And one to segment_usage.
    {
      sql::Statement s(db.GetUniqueStatement(
                           "INSERT INTO segment_usage "
                           "(id, segment_id, time_slot, visit_count) VALUES "
                           "(?, ?, ?, ?)"));
      s.BindInt64(0, 4);  // id.
      s.BindInt64(1, segment_id);
      s.BindInt64(2, segment_time.ToInternalValue());
      s.BindInt(3, 5);  // visit count.
      ASSERT_TRUE(s.Run());
    }
  }

  // Re-open the db, triggering migration.
  CreateBackendAndDatabase();

  std::vector<PageUsageData*> results;
  db_->QuerySegmentUsage(segment_time, 10, &results);
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(url, results[0]->GetURL());
  EXPECT_EQ(segment_id, results[0]->GetID());
  EXPECT_EQ(title, results[0]->GetTitle());
  STLDeleteElements(&results);
}

TEST_F(HistoryBackendDBTest, CheckLastCompatibleVersion) {
  ASSERT_NO_FATAL_FAILURE(CreateDBVersion(28));
  {
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      // Manually set last compatible version to one higher
      // than current version.
      sql::MetaTable meta;
      meta.Init(&db, 1, 1);
      meta.SetCompatibleVersionNumber(HistoryDatabase::GetCurrentVersion() + 1);
    }
  }
  // Try to create and init backend for non compatible db.
  // Allow failure in backend creation.
  CreateBackendAndDatabaseAllowFail();
  DeleteBackend();

  // Check that error delegate was called with correct init error status.
  EXPECT_EQ(sql::INIT_TOO_NEW, last_profile_error_);
  {
    // Re-open the db to check that it was not migrated.
    // Non compatible DB must be ignored.
    // Check that DB version in file remains the same.
    sql::Connection db;
    ASSERT_TRUE(db.Open(history_dir_.Append(kHistoryFilename)));
    {
      sql::MetaTable meta;
      meta.Init(&db, 1, 1);
      // Current browser version must be already higher than 28.
      ASSERT_LT(28, HistoryDatabase::GetCurrentVersion());
      // Expect that version in DB remains the same.
      EXPECT_EQ(28, meta.GetVersionNumber());
    }
  }
}

}  // namespace
}  // namespace history
