// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/quirks/quirks_manager.h"
#include "content/public/test/test_utils.h"
#include "net/url_request/test_url_fetcher_factory.h"

namespace quirks {

namespace {

const char kFakeIccJson[] = "{\n  \"icc\": \"AAAIkCAgICACEAAA\"\n}";
const char kFakeIccData[] = {0x00, 0x00, 0x08, 0x90, 0x20, 0x20,
                             0x20, 0x20, 0x02, 0x10, 0x00, 0x00};

// Create FakeURLFetcher which returns fake icc file json.
std::unique_ptr<net::URLFetcher> CreateFakeURLFetcherSuccess(
    const GURL& url,
    net::URLFetcherDelegate* delegate) {
  net::URLFetcher* fetcher =
      new net::FakeURLFetcher(url, delegate, kFakeIccJson, net::HTTP_OK,
                              net::URLRequestStatus::SUCCESS);
  return base::WrapUnique(fetcher);
}

// Create FakeURLFetcher which replies with HTTP_NOT_FOUND.
std::unique_ptr<net::URLFetcher> CreateFakeURLFetcherFailure(
    const GURL& url,
    net::URLFetcherDelegate* delegate) {
  net::URLFetcher* fetcher = new net::FakeURLFetcher(
      url, delegate, "File not found", net::HTTP_NOT_FOUND,
      net::URLRequestStatus::FAILED);
  return base::WrapUnique(fetcher);
}

// Full path to fake icc file in <tmp test directory>/display_profiles/.
base::FilePath GetPathForIccFile(int64_t product_id) {
  return QuirksManager::Get()
      ->delegate()
      ->GetDownloadDisplayProfileDirectory()
      .Append(quirks::IdToFileName(product_id));
}

}  // namespace

class QuirksBrowserTest : public InProcessBrowserTest {
 public:
  QuirksBrowserTest() : file_existed_(false) {}

 protected:
  ~QuirksBrowserTest() override = default;

  // Query QuirksManager for icc file, then run msg loop to wait for callback.
  // |find_fake_file| indicates that URLFetcher should respond with success.
  void TestQuirksClient(int64_t product_id, bool find_fake_file) {
    // Set up fake url getter.
    QuirksManager::Get()->SetFakeQuirksFetcherCreatorForTests(
        base::Bind(find_fake_file ? &CreateFakeURLFetcherSuccess
                                  : &CreateFakeURLFetcherFailure));

    base::RunLoop run_loop;
    end_message_loop_ = run_loop.QuitClosure();

    quirks::QuirksManager::Get()->RequestIccProfilePath(
        product_id, base::Bind(&QuirksBrowserTest::OnQuirksClientFinished,
                               base::Unretained(this)));

    run_loop.Run();
  }

  // Callback from RequestIccProfilePath().
  void OnQuirksClientFinished(const base::FilePath& path, bool downloaded) {
    icc_path_ = path;
    file_existed_ = !downloaded;
    ASSERT_TRUE(!end_message_loop_.is_null());
    end_message_loop_.Run();
  }

  void SetUpOnMainThread() override {
    // NOTE: QuirksManager::Initialize() isn't necessary here, since it'll be
    // called in ChromeBrowserMainPartsChromeos::PreMainMessageLoopRun().

    // Create display_profiles subdirectory under temp profile directory.
    const base::FilePath path =
        QuirksManager::Get()->delegate()->GetDownloadDisplayProfileDirectory();
    base::File::Error error = base::File::FILE_OK;
    bool created = base::CreateDirectoryAndGetError(path, &error);
    ASSERT_TRUE(created);

    // Quirks clients can't run until after login.
    quirks::QuirksManager::Get()->OnLoginCompleted();
  }

  base::Closure end_message_loop_;  // Callback to terminate message loop.
  base::FilePath icc_path_;         // Path to icc file if found or downloaded.
  bool file_existed_;               // File was previously downloaded.

 private:
  DISALLOW_COPY_AND_ASSIGN(QuirksBrowserTest);
};

IN_PROC_BROWSER_TEST_F(QuirksBrowserTest, DownloadIccFile) {
  // Request a file from fake Quirks Server, verify that file is written with
  // correct location and data.
  TestQuirksClient(0x0000aaaa, true);
  base::FilePath path = GetPathForIccFile(0x0000aaaa);
  EXPECT_EQ(icc_path_, path);
  EXPECT_EQ(file_existed_, false);
  EXPECT_TRUE(base::PathExists(path));
  char data[32];
  ReadFile(path, data, sizeof(data));
  EXPECT_EQ(0, memcmp(data, kFakeIccData, sizeof(kFakeIccData)));

  // Retest same file, this time expect it to already exist.
  TestQuirksClient(0x0000aaaa, true);
  EXPECT_EQ(icc_path_, path);
  EXPECT_EQ(file_existed_, true);

  // Finally, request a file that doesn't exist on fake Quirks Server.
  TestQuirksClient(0x1111bbbb, false);
  EXPECT_EQ(icc_path_, base::FilePath());
  EXPECT_EQ(file_existed_, false);
  EXPECT_FALSE(base::PathExists(GetPathForIccFile(0x1111bbbb)));
}

}  // namespace quirks
