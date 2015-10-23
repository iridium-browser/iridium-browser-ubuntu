// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

class SitePerProcessAccessibilityBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessAccessibilityBrowserTest() {}
};

bool AccessibilityTreeContainsDocTitle(
    BrowserAccessibility* node,
    const std::string& title) {
  if (node->GetStringAttribute(ui::AX_ATTR_DOC_TITLE) == title)
    return true;
  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsDocTitle(node->PlatformGetChild(i), title))
      return true;
  }
  return false;
}

// Utility function to determine if an accessibility tree has finished loading
// or if the tree represents a page that hasn't finished loading yet.
bool AccessibilityTreeIsLoaded(BrowserAccessibilityManager* manager) {
  BrowserAccessibility* root = manager->GetRoot();
  return (root->GetFloatAttribute(ui::AX_ATTR_DOC_LOADING_PROGRESS) == 1.0 &&
          root->GetStringAttribute(ui::AX_ATTR_DOC_URL) != url::kAboutBlankURL);
}

// Times out on Android, not clear if it's an actual bug or just slow.
#if defined(OS_ANDROID)
#define MAYBE_CrossSiteIframeAccessibility DISABLED_CrossSiteIframeAccessibility
#else
#define MAYBE_CrossSiteIframeAccessibility CrossSiteIframeAccessibility
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessAccessibilityBrowserTest,
                       MAYBE_CrossSiteIframeAccessibility) {
  // Enable full accessibility for all current and future WebContents.
  BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  GURL main_url(test_server()->GetURL("files/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
          GetFrameTree()->root();

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(test_server()->GetURL("files/title1.html"));
  NavigateFrameToURL(child, http_url);

  // Load cross-site page into iframe.
  RenderFrameHostImpl* child_rfh =
      child->render_manager()->current_frame_host();
  RenderFrameDeletedObserver deleted_observer(child_rfh);
  GURL::Replacements replace_host;
  GURL cross_site_url(test_server()->GetURL("files/title2.html"));
  replace_host.SetHostStr("foo.com");
  cross_site_url = cross_site_url.ReplaceComponents(replace_host);
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(2U, root->child_count());
  SiteInstance* site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);

  // Wait until the iframe completes the swap.
  deleted_observer.WaitUntilDeleted();

  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  VLOG(1) << "Main frame accessibility tree:\n"
          << main_frame_manager->SnapshotAXTreeForTesting().ToString();

  RenderFrameHostImpl* child_frame = static_cast<RenderFrameHostImpl*>(
      child->current_frame_host());
  while (!AccessibilityTreeContainsDocTitle(main_frame_manager->GetRoot(),
                                            "Title Of Awesomeness")) {
    AccessibilityNotificationWaiter accessibility_waiter(main_frame,
                                                         ui::AX_EVENT_NONE);
    accessibility_waiter.ListenToAdditionalFrame(child_frame);
    accessibility_waiter.WaitForNotification();
  }

  // Assert that we can walk from the main frame down into the child frame
  // directly, getting correct roles and data along the way.
  BrowserAccessibility* ax_root = main_frame_manager->GetRoot();
  EXPECT_EQ(ui::AX_ROLE_ROOT_WEB_AREA, ax_root->GetRole());
  ASSERT_EQ(1U, ax_root->PlatformChildCount());

  BrowserAccessibility* ax_group = ax_root->PlatformGetChild(0);
  EXPECT_EQ(ui::AX_ROLE_GROUP, ax_group->GetRole());
  ASSERT_EQ(2U, ax_group->PlatformChildCount());

  BrowserAccessibility* ax_iframe = ax_group->PlatformGetChild(0);
  EXPECT_EQ(ui::AX_ROLE_IFRAME, ax_iframe->GetRole());
  ASSERT_EQ(1U, ax_iframe->PlatformChildCount());

  BrowserAccessibility* ax_child_frame_root = ax_iframe->PlatformGetChild(0);
  EXPECT_EQ(ui::AX_ROLE_ROOT_WEB_AREA, ax_child_frame_root->GetRole());
  ASSERT_EQ(1U, ax_child_frame_root->PlatformChildCount());
  EXPECT_EQ("Title Of Awesomeness",
            ax_child_frame_root->GetStringAttribute(ui::AX_ATTR_NAME));

  BrowserAccessibility* ax_child_frame_group =
      ax_child_frame_root->PlatformGetChild(0);
  EXPECT_EQ(ui::AX_ROLE_GROUP, ax_child_frame_group->GetRole());
  ASSERT_EQ(1U, ax_child_frame_group->PlatformChildCount());

  BrowserAccessibility* ax_child_frame_static_text =
      ax_child_frame_group->PlatformGetChild(0);
  EXPECT_EQ(ui::AX_ROLE_STATIC_TEXT, ax_child_frame_static_text->GetRole());
  ASSERT_EQ(0U, ax_child_frame_static_text->PlatformChildCount());

  // Last, check that the parent of the child frame root is correct.
  EXPECT_EQ(ax_child_frame_root->GetParent(), ax_iframe);
}

}  // namespace content
