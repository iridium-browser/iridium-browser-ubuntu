// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

#include "android_webview/renderer/aw_render_process_observer.h"
#include "base/compiler_specific.h"

namespace visitedlink {
class VisitedLinkSlave;
}

namespace android_webview {

class AwContentRendererClient : public content::ContentRendererClient {
 public:
  AwContentRendererClient();
  ~AwContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RenderViewCreated(content::RenderView* render_view) override;
  bool HasErrorPage(int http_status_code, std::string* error_domain) override;
  void GetNavigationErrorStrings(content::RenderView* render_view,
                                 blink::WebFrame* frame,
                                 const blink::WebURLRequest& failed_request,
                                 const blink::WebURLError& error,
                                 std::string* error_html,
                                 base::string16* error_description) override;
  unsigned long long VisitedLinkHash(const char* canonical_url,
                                     size_t length) override;
  bool IsLinkVisited(unsigned long long link_hash) override;
  void AddKeySystems(std::vector<media::KeySystemInfo>* key_systems) override;

  bool HandleNavigation(content::RenderFrame* render_frame,
                        content::DocumentState* document_state,
                        int opener_id,
                        blink::WebFrame* frame,
                        const blink::WebURLRequest& request,
                        blink::WebNavigationType type,
                        blink::WebNavigationPolicy default_policy,
                        bool is_redirect) override;
  bool ShouldOverridePageVisibilityState(
      const content::RenderFrame* render_frame,
      blink::WebPageVisibilityState* override_state) override;

 private:
  scoped_ptr<AwRenderProcessObserver> aw_render_process_observer_;
  scoped_ptr<visitedlink::VisitedLinkSlave> visited_link_slave_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_CONTENT_RENDERER_CLIENT_H_
