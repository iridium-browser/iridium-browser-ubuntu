// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FtpBrowserTest : public InProcessBrowserTest {
 public:
  FtpBrowserTest()
      : ftp_server_(net::SpawnedTestServer::TYPE_FTP,
                    net::SpawnedTestServer::kLocalhost,
                    base::FilePath(FILE_PATH_LITERAL("chrome/test/data/ftp"))) {
  }

 protected:
  net::SpawnedTestServer ftp_server_;
};

void WaitForTitle(content::WebContents* contents, const char* expected_title) {
  content::TitleWatcher title_watcher(contents,
      base::ASCIIToUTF16(expected_title));

  EXPECT_EQ(base::ASCIIToUTF16(expected_title),
            title_watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(FtpBrowserTest, BasicFtpUrlAuthentication) {
  ASSERT_TRUE(ftp_server_.Start());
  ui_test_utils::NavigateToURL(
      browser(),
      ftp_server_.GetURLWithUserAndPassword("", "chrome", "chrome"));

  WaitForTitle(browser()->tab_strip_model()->GetActiveWebContents(),
               "Index of /");
}

// http://crbug.com/521409
#if defined(OS_WIN)
#define MAYBE_DirectoryListingNavigation DISABLED_DirectoryListingNavigation
#else
#define MAYBE_DirectoryListingNavigation DirectoryListingNavigation
#endif
IN_PROC_BROWSER_TEST_F(FtpBrowserTest, MAYBE_DirectoryListingNavigation) {
  ftp_server_.set_no_anonymous_ftp_user(true);
  ASSERT_TRUE(ftp_server_.Start());

  ui_test_utils::NavigateToURL(
      browser(),
      ftp_server_.GetURLWithUserAndPassword("", "chrome", "chrome"));

  // Navigate to directory dir1/ without needing to re-authenticate
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "var elements = document.getElementsByTagName('a');"
      "for (var i = 0; i < elements.length; i++) {"
      "  if (elements[i].innerHTML == 'dir1/') {"
      "    elements[i].click();"
      "  }"
      "}"));

  WaitForTitle(browser()->tab_strip_model()->GetActiveWebContents(),
               "Index of /dir1/");

  EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "var elements = document.getElementsByTagName('a');"
        "for (var i = 0; i < elements.length; i++) {"
        "  if (elements[i].innerHTML == 'test.html') {"
        "    elements[i].click();"
        "  }"
        "}"));

  WaitForTitle(browser()->tab_strip_model()->GetActiveWebContents(),
               "PASS");
}
