// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_test.h"

#include "base/run_loop.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"

namespace extensions {

DeclarativeContentConditionTrackerTest::DeclarativeContentConditionTrackerTest()
    : profile_(new TestingProfile) {}

DeclarativeContentConditionTrackerTest::
~DeclarativeContentConditionTrackerTest() {
  // MockRenderProcessHosts are deleted from the message loop, and their
  // deletion must complete before RenderViewHostTestEnabler's destructor is
  // run.
  base::RunLoop().RunUntilIdle();
}

scoped_ptr<content::WebContents>
DeclarativeContentConditionTrackerTest::MakeTab() {
  return make_scoped_ptr(content::WebContentsTester::CreateTestWebContents(
      profile_.get(),
      nullptr));
}

content::MockRenderProcessHost*
DeclarativeContentConditionTrackerTest::GetMockRenderProcessHost(
    content::WebContents* contents) {
  return static_cast<content::MockRenderProcessHost*>(
      contents->GetRenderViewHost()->GetProcess());
}

}  // namespace extensions
