// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/url_downloader.h"

#include <vector>

#include "base/files/file_util.h"
#import "base/mac/bind_objc_block.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DistillerViewerTest : public dom_distiller::DistillerViewerInterface {
 public:
  DistillerViewerTest(const GURL& url,
                      const DistillationFinishedCallback& callback)
      : dom_distiller::DistillerViewerInterface(nil, nil) {
    std::vector<ImageInfo> images;
    callback.Run(url, "html", images);
  }

  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override {}

  void SendJavaScript(const std::string& buffer) override {}
};

}  // namespace

class MockURLDownloader : public URLDownloader {
 public:
  MockURLDownloader(base::FilePath path)
      : URLDownloader(nil,
                      nil,
                      path,
                      base::Bind(&MockURLDownloader::OnEndDownload,
                                 base::Unretained(this)),
                      base::Bind(&MockURLDownloader::OnEndRemove,
                                 base::Unretained(this))) {}

  void RemoveOfflineFilesDirectory() {
    base::DeleteFile(OfflineDirectoryPath(), true);
  }

  void ClearCompletionTrackers() {
    downloaded_files_.clear();
    removed_files_.clear();
  }

  bool CheckExistenceOfOfflineURLPagePath(const GURL& url) {
    return base::PathExists(OfflineURLPagePath(url));
  }

  void FakeWorking() { working_ = true; }

  void FakeEndWorking() {
    working_ = false;
    HandleNextTask();
  }

  std::vector<GURL> downloaded_files_;
  std::vector<GURL> removed_files_;

 private:
  void DownloadURL(GURL url, bool offlineURLExists) override {
    if (offlineURLExists) {
      DownloadCompletionHandler(url, false);
      return;
    }
    distiller_.reset(new DistillerViewerTest(
        url,
        base::Bind(&URLDownloader::DistillerCallback, base::Unretained(this))));
  }

  void OnEndDownload(const GURL& url, bool success) {
    downloaded_files_.push_back(url);
  }

  void OnEndRemove(const GURL& url, bool success) {
    removed_files_.push_back(url);
  }
};

namespace {
class URLDownloaderTest : public testing::Test {
 public:
  std::unique_ptr<MockURLDownloader> downloader_;
  web::TestWebThreadBundle bundle_;

  URLDownloaderTest() {
    base::FilePath data_dir;
    base::PathService::Get(ios::DIR_USER_DATA, &data_dir);
    downloader_.reset(new MockURLDownloader(data_dir));
  }
  ~URLDownloaderTest() override {}

  void TearDown() override {
    downloader_->RemoveOfflineFilesDirectory();
    downloader_->ClearCompletionTrackers();
  }

  void WaitUntilCondition(ConditionBlock condition) {
    base::MessageLoop* messageLoop = base::MessageLoop::current();
    DCHECK(messageLoop);
    base::test::ios::WaitUntilCondition(condition, messageLoop,
                                        base::TimeDelta::FromSeconds(1));
  }
};

TEST_F(URLDownloaderTest, SingleDownload) {
  GURL url = GURL("http://test.com");
  ASSERT_FALSE(downloader_->CheckExistenceOfOfflineURLPagePath(url));
  ASSERT_EQ(0ul, downloader_->downloaded_files_.size());
  ASSERT_EQ(0ul, downloader_->removed_files_.size());

  downloader_->DownloadOfflineURL(url);

  WaitUntilCondition(^bool {
    return std::find(downloader_->downloaded_files_.begin(),
                     downloader_->downloaded_files_.end(),
                     url) != downloader_->downloaded_files_.end();
  });

  ASSERT_TRUE(downloader_->CheckExistenceOfOfflineURLPagePath(url));
}

TEST_F(URLDownloaderTest, DownloadAndRemove) {
  GURL url = GURL("http://test.com");
  GURL url2 = GURL("http://test2.com");
  ASSERT_FALSE(downloader_->CheckExistenceOfOfflineURLPagePath(url));
  ASSERT_FALSE(downloader_->CheckExistenceOfOfflineURLPagePath(url2));
  ASSERT_EQ(0ul, downloader_->downloaded_files_.size());
  ASSERT_EQ(0ul, downloader_->removed_files_.size());
  downloader_->FakeWorking();
  downloader_->DownloadOfflineURL(url);
  downloader_->DownloadOfflineURL(url2);
  downloader_->RemoveOfflineURL(url);
  downloader_->FakeEndWorking();

  WaitUntilCondition(^bool {
    return std::find(downloader_->removed_files_.begin(),
                     downloader_->removed_files_.end(),
                     url) != downloader_->removed_files_.end();
  });

  ASSERT_TRUE(std::find(downloader_->downloaded_files_.begin(),
                        downloader_->downloaded_files_.end(),
                        url) == downloader_->downloaded_files_.end());
  ASSERT_EQ(1ul, downloader_->downloaded_files_.size());
  ASSERT_EQ(1ul, downloader_->removed_files_.size());
  ASSERT_FALSE(downloader_->CheckExistenceOfOfflineURLPagePath(url));
  ASSERT_TRUE(downloader_->CheckExistenceOfOfflineURLPagePath(url2));
}

TEST_F(URLDownloaderTest, DownloadAndRemoveAndRedownload) {
  GURL url = GURL("http://test.com");
  ASSERT_FALSE(downloader_->CheckExistenceOfOfflineURLPagePath(url));
  downloader_->FakeWorking();
  downloader_->DownloadOfflineURL(url);
  downloader_->RemoveOfflineURL(url);
  downloader_->DownloadOfflineURL(url);
  downloader_->FakeEndWorking();

  WaitUntilCondition(^bool {
    return std::find(downloader_->removed_files_.begin(),
                     downloader_->removed_files_.end(),
                     url) != downloader_->removed_files_.end();
  });

  ASSERT_TRUE(std::find(downloader_->downloaded_files_.begin(),
                        downloader_->downloaded_files_.end(),
                        url) != downloader_->downloaded_files_.end());
  ASSERT_TRUE(std::find(downloader_->removed_files_.begin(),
                        downloader_->removed_files_.end(),
                        url) != downloader_->removed_files_.end());
  ASSERT_TRUE(downloader_->CheckExistenceOfOfflineURLPagePath(url));
}

}  // namespace
