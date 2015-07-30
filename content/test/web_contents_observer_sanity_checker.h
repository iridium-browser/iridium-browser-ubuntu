// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEB_CONTENTS_OBSERVER_SANITY_CHECKER_H_
#define CONTENT_TEST_WEB_CONTENTS_OBSERVER_SANITY_CHECKER_H_

#include <set>
#include <string>

#include "base/supports_user_data.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// If your test framework enables a ContentBrowserSanityChecker, this sanity
// check is automatically installed on all WebContentses during your test.
//
// WebContentsObserverSanityChecker is a WebContentsObserver that sanity-checks
// the sequence of observer calls, and CHECK()s if they are inconsistent. These
// checks are test-only code designed to find bugs in the implementation of the
// content layer by validating the contract between WebContents and its
// observers.
//
// For example, WebContentsObserver::RenderFrameCreated announces the existence
// of a new RenderFrameHost, so that method call must occur before the
// RenderFrameHost is referenced by some other WebContentsObserver method.
class WebContentsObserverSanityChecker : public WebContentsObserver,
                                         public base::SupportsUserData::Data {
 public:
  // Enables these checks on |web_contents|. Usually ContentBrowserSanityChecker
  // should call this for you.
  static void Enable(WebContents* web_contents);

  // WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameForInterstitialPageCreated(
      RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(RenderFrameHost* render_frame_host) override;
  void DidStartProvisionalLoadForFrame(RenderFrameHost* render_frame_host,
                                       const GURL& validated_url,
                                       bool is_error_page,
                                       bool is_iframe_srcdoc) override;
  void DidCommitProvisionalLoadForFrame(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;
  void DidFailProvisionalLoad(RenderFrameHost* render_frame_host,
                              const GURL& validated_url,
                              int error_code,
                              const base::string16& error_description) override;
  void DidNavigateMainFrame(const LoadCommittedDetails& details,
                            const FrameNavigateParams& params) override;
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                           const LoadCommittedDetails& details,
                           const FrameNavigateParams& params) override;
  void DocumentAvailableInMainFrame() override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void DocumentLoadedInFrame(RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void DidGetRedirectForResourceRequest(
      RenderFrameHost* render_frame_host,
      const ResourceRedirectDetails& details) override;
  void DidOpenRequestedURL(WebContents* new_contents,
                           RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

 private:
  explicit WebContentsObserverSanityChecker(WebContents* web_contents);
  ~WebContentsObserverSanityChecker() override;

  std::string Format(RenderFrameHost* render_frame_host);
  void AssertRenderFrameExists(RenderFrameHost* render_frame_host);
  void AssertMainFrameExists();

  std::set<std::pair<int, int>> current_hosts_;
  std::set<std::pair<int, int>> live_routes_;
  std::set<std::pair<int, int>> deleted_routes_;

  bool web_contents_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserverSanityChecker);
};

}  // namespace content

#endif  // CONTENT_TEST_WEB_CONTENTS_OBSERVER_SANITY_CHECKER_H_
