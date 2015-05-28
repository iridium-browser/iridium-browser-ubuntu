// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
#define CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_

#include <vector>

#include "base/basictypes.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "ui/base/page_transition_types.h"

struct FrameHostMsg_DidCommitProvisionalLoad_Params;

namespace content {

class TestRenderFrameHostCreationObserver : public WebContentsObserver {
 public:
  explicit TestRenderFrameHostCreationObserver(WebContents* web_contents);
  ~TestRenderFrameHostCreationObserver() override;

  // WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  RenderFrameHost* last_created_frame() const { return last_created_frame_; }

 private:
  RenderFrameHost* last_created_frame_;
};

class TestRenderFrameHost : public RenderFrameHostImpl,
                            public RenderFrameHostTester {
 public:
  TestRenderFrameHost(SiteInstance* site_instance,
                      RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      RenderWidgetHostDelegate* rwh_delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int routing_id,
                      int flags);
  ~TestRenderFrameHost() override;

  // RenderFrameHostImpl overrides (same values, but in Test* types)
  TestRenderViewHost* GetRenderViewHost() override;

  // RenderFrameHostTester implementation.
  TestRenderFrameHost* AppendChild(const std::string& frame_name) override;
  void SendNavigate(int page_id, const GURL& url) override;
  void SendFailedNavigate(int page_id, const GURL& url) override;
  void SendNavigateWithTransition(int page_id,
                                  const GURL& url,
                                  ui::PageTransition transition) override;
  void SetContentsMimeType(const std::string& mime_type) override;
  void SendBeforeUnloadACK(bool proceed) override;
  void SimulateSwapOutACK() override;

  void SendNavigateWithTransitionAndResponseCode(
      int page_id,
      const GURL& url, ui::PageTransition transition,
      int response_code);
  void SendNavigateWithOriginalRequestURL(
      int page_id,
      const GURL& url,
      const GURL& original_request_url);
  void SendNavigateWithFile(
      int page_id,
      const GURL& url,
      const base::FilePath& file_path);
  void SendNavigateWithParams(
      FrameHostMsg_DidCommitProvisionalLoad_Params* params);
  void SendNavigateWithRedirects(
      int page_id,
      const GURL& url,
      const std::vector<GURL>& redirects);
  void SendNavigateWithParameters(
      int page_id,
      const GURL& url,
      ui::PageTransition transition,
      const GURL& original_request_url,
      int response_code,
      const base::FilePath* file_path_for_history_item,
      const std::vector<GURL>& redirects);

  // With the current navigation logic this method is a no-op.
  // PlzNavigate: this method simulates receiving a BeginNavigation IPC.
  void SendRendererInitiatedNavigationRequest(const GURL& url,
                                              bool has_user_gesture);

  void DidDisownOpener();

  // If set, navigations will appear to have cleared the history list in the
  // RenderFrame
  // (FrameHostMsg_DidCommitProvisionalLoad_Params::history_list_was_cleared).
  // False by default.
  void set_simulate_history_list_was_cleared(bool cleared) {
    simulate_history_list_was_cleared_ = cleared;
  }

  // Advances the RenderFrameHost (and through it the RenderFrameHostManager) to
  // a state where a new navigation can be committed by a renderer. Currently,
  // this simulates a BeforeUnload ACK from the renderer.
  // PlzNavigate: this simulates a BeforeUnload ACK from the renderer, and the
  // interaction with the IO thread up until the response is ready to commit.
  void PrepareForCommit();

  // This method does the same as PrepareForCommit.
  // PlzNavigate: Beyond doing the same as PrepareForCommit, this method will
  // also simulate a server redirect to |redirect_url|. If the URL is empty the
  // redirect step is ignored.
  void PrepareForCommitWithServerRedirect(const GURL& redirect_url);

 private:
  TestRenderFrameHostCreationObserver child_creation_observer_;

  std::string contents_mime_type_;

  // See set_simulate_history_list_was_cleared() above.
  bool simulate_history_list_was_cleared_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderFrameHost);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_RENDER_FRAME_HOST_H_
