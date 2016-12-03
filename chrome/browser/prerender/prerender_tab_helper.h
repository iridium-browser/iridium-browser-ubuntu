// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_histograms.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {
class PasswordManager;
}

namespace prerender {

class PrerenderManager;

// PrerenderTabHelper is responsible for recording perceived pageload times
// to compare PLT's with prerendering enabled and disabled.
class PrerenderTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PrerenderTabHelper> {
 public:
  ~PrerenderTabHelper() override;

  // content::WebContentsObserver implementation.
  void DidGetRedirectForResourceRequest(
      content::RenderFrameHost* render_frame_host,
      const content::ResourceRedirectDetails& details) override;
  void DidStopLoading() override;
  void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) override;
  void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      ui::PageTransition transition_type) override;

  // Called when the URL of the main frame changed, either when the load
  // commits, or a redirect happens.
  void MainFrameUrlDidChange(const GURL& url);

  // Called when this prerendered WebContents has just been swapped in.
  void PrerenderSwappedIn();

  // Called when a control prerender is resolved. Applies to the next load.
  void WouldHavePrerenderedNextLoad(Origin origin);

 private:
  explicit PrerenderTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PrerenderTabHelper>;

  void RecordPerceivedPageLoadTime(
      base::TimeDelta perceived_page_load_time,
      double fraction_plt_elapsed_at_swap_in);

  // Retrieves the PrerenderManager, or NULL, if none was found.
  PrerenderManager* MaybeGetPrerenderManager() const;

  // Returns whether the WebContents being observed is currently prerendering.
  bool IsPrerendering();

  // The type the current pending navigation, if there is one. If the tab is a
  // prerender before swap, the value is always NAVIGATION_TYPE_PRERENDERED,
  // even if the prerender is not currently loading.
  NavigationType navigation_type_;

  // If |navigation_type_| is not NAVIGATION_TYPE_NORMAL, the origin of the
  // relevant prerender. Otherwise, ORIGIN_NONE.
  Origin origin_;

  // True if the next load will be associated with a control prerender. This
  // extra state is needed because control prerenders are resolved before the
  // actual load begins. |next_load_origin_| gives the origin of the control
  // prerender.
  bool next_load_is_control_prerender_;
  Origin next_load_origin_;

  // System time at which the current load was started for the purpose of
  // the perceived page load time (PPLT). If null, there is no current
  // load.
  base::TimeTicks pplt_load_start_;

  // System time at which the actual pageload started (pre-swapin), if
  // a applicable (in cases when a prerender that was still loading was
  // swapped in).
  base::TimeTicks actual_load_start_;

  // Current URL being loaded.
  GURL url_;

  base::WeakPtrFactory<PrerenderTabHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTabHelper);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TAB_HELPER_H_
