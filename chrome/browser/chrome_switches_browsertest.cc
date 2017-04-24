// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class HostRulesTest : public InProcessBrowserTest {
 protected:
  HostRulesTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Map all hosts to our local server.
    std::string host_rule("MAP * " +
                          embedded_test_server()->host_port_pair().ToString());
    command_line->AppendSwitchASCII(switches::kHostRules, host_rule);
    // Use no proxy or otherwise this test will fail on a machine that has a
    // proxy configured.
    command_line->AppendSwitch(switches::kNoProxyServer);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HostRulesTest);
};

IN_PROC_BROWSER_TEST_F(HostRulesTest, TestMap) {
  // Go to the empty page using www.google.com as the host.
  GURL local_url = embedded_test_server()->GetURL("/empty.html");
  GURL test_url(std::string("http://www.google.com") + local_url.path());
  ui_test_utils::NavigateToURL(browser(), test_url);

  std::string html;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.body.outerHTML);",
      &html));

  EXPECT_STREQ("<body></body>", html.c_str());
}
