// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_TEST_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_TEST_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class MockRenderProcessHost;
class WebContents;
}  // namespace content

namespace extensions {

// Creates a TestWebContents browser context and mocks out RenderViewHosts. The
// latter is done to avoid having to run renderer processes and because the
// actual RenderViewHost implementation depends on things not available in this
// configuration.
class DeclarativeContentConditionTrackerTest : public testing::Test {
 public:
  DeclarativeContentConditionTrackerTest();
  ~DeclarativeContentConditionTrackerTest() override;

 protected:
  // Creates a new WebContents and retains ownership.
  scoped_ptr<content::WebContents> MakeTab();

  // Gets the MockRenderProcessHost associated with a WebContents.
  content::MockRenderProcessHost* GetMockRenderProcessHost(
      content::WebContents* contents);

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  // Enables MockRenderProcessHosts.
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;

  const scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentConditionTrackerTest);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_TEST_H_
