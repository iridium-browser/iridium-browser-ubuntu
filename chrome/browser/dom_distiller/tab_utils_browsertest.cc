// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/web_contents_main_frame_observer.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

namespace {
const char* kSimpleArticlePath = "/dom_distiller/simple_article.html";
}  // namespace

class DomDistillerTabUtilsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }
};

class WebContentsMainFrameHelper : public content::WebContentsObserver {
 public:
  WebContentsMainFrameHelper(content::WebContents* web_contents,
                             const base::Closure& callback)
      : callback_(callback) {
    content::WebContentsObserver::Observe(web_contents);
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host->GetParent() &&
        validated_url.scheme() == kDomDistillerScheme)
      callback_.Run();
  }

 private:
  base::Closure callback_;
};

#if (defined(OS_LINUX) && defined(OS_CHROMEOS))
#define MAYBE_TestSwapWebContents DISABLED_TestSwapWebContents
#else
#define MAYBE_TestSwapWebContents TestSwapWebContents
#endif

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       MAYBE_TestSwapWebContents) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL& article_url = embedded_test_server()->GetURL(kSimpleArticlePath);

  // This blocks until the navigation has completely finished.
  ui_test_utils::NavigateToURL(browser(), article_url);

  DistillCurrentPageAndView(initial_web_contents);

  // Wait until the new WebContents has fully navigated.
  content::WebContents* after_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(after_web_contents != NULL);
  base::RunLoop new_url_loaded_runner;
  scoped_ptr<WebContentsMainFrameHelper> distilled_page_loaded(
      new WebContentsMainFrameHelper(after_web_contents,
                                     new_url_loaded_runner.QuitClosure()));
  new_url_loaded_runner.Run();

  // Verify the new URL is showing distilled content in a new WebContents.
  EXPECT_NE(initial_web_contents, after_web_contents);
  EXPECT_TRUE(
      after_web_contents->GetLastCommittedURL().SchemeIs(kDomDistillerScheme));
  EXPECT_EQ("Test Page Title",
            base::UTF16ToUTF8(after_web_contents->GetTitle()));
}

IN_PROC_BROWSER_TEST_F(DomDistillerTabUtilsBrowserTest,
                       TestDistillIntoWebContents) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  content::WebContents* source_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL& article_url = embedded_test_server()->GetURL(kSimpleArticlePath);

  // This blocks until the navigation has completely finished.
  ui_test_utils::NavigateToURL(browser(), article_url);

  // Create destination WebContents.
  content::WebContents::CreateParams create_params(
      source_web_contents->GetBrowserContext());
  content::WebContents* destination_web_contents =
      content::WebContents::Create(create_params);
  DCHECK(destination_web_contents);

  browser()->tab_strip_model()->AppendWebContents(destination_web_contents,
                                                  true);
  ASSERT_EQ(destination_web_contents,
            browser()->tab_strip_model()->GetWebContentsAt(1));

  DistillAndView(source_web_contents, destination_web_contents);

  // Wait until the destination WebContents has fully navigated.
  base::RunLoop new_url_loaded_runner;
  scoped_ptr<WebContentsMainFrameHelper> distilled_page_loaded(
      new WebContentsMainFrameHelper(destination_web_contents,
                                     new_url_loaded_runner.QuitClosure()));
  new_url_loaded_runner.Run();

  // Verify that the source WebContents is showing the original article.
  EXPECT_EQ(article_url, source_web_contents->GetLastCommittedURL());
  EXPECT_EQ("Test Page Title",
            base::UTF16ToUTF8(source_web_contents->GetTitle()));

  // Verify the destination WebContents is showing distilled content.
  EXPECT_TRUE(destination_web_contents->GetLastCommittedURL().SchemeIs(
      kDomDistillerScheme));
  EXPECT_EQ("Test Page Title",
            base::UTF16ToUTF8(destination_web_contents->GetTitle()));

  content::WebContentsDestroyedWatcher destroyed_watcher(
      destination_web_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(1, 0);
  destroyed_watcher.Wait();
}

}  // namespace dom_distiller
