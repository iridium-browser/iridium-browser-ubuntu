// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/banners/app_banner_data_fetcher.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/platform/modules/app_banner/WebAppBannerPromptReply.h"

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
}  // namespace content

namespace banners {
class AppBannerDataFetcher;

/**
 * Creates an app banner.
 *
 * Hooks the wiring together for getting the data for a particular app.
 * Monitors at most one app at a time, tracking the info for the most recently
 * requested app. Any work in progress for other apps is discarded.
 */
class AppBannerManager : public content::WebContentsObserver,
                         public AppBannerDataFetcher::Delegate {
 public:
  static void DisableSecureSchemeCheckForTesting();

  static void SetEngagementWeights(double direct_engagement,
                                   double indirect_engagement);

  // Returns whether or not the URLs match for everything except for the ref.
  static bool URLsAreForTheSamePage(const GURL& first, const GURL& second);

  explicit AppBannerManager(int icon_size);
  ~AppBannerManager() override;

  // WebContentsObserver overrides.
  void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 protected:
  AppBannerManager(content::WebContents* web_contents, int icon_size);

  void ReplaceWebContents(content::WebContents* web_contents);

  // Creates an AppBannerDataFetcher, which constructs an app banner.
  virtual AppBannerDataFetcher* CreateAppBannerDataFetcher(
      base::WeakPtr<AppBannerDataFetcher::Delegate> weak_delegate,
      const int ideal_icon_size) = 0;

  // Return whether the AppBannerDataFetcher is active.
  bool IsFetcherActive();

  scoped_refptr<AppBannerDataFetcher> data_fetcher() { return data_fetcher_; }
  int ideal_icon_size() { return ideal_icon_size_; }

 private:
  // AppBannerDataFetcher::Delegate overrides.
  bool HandleNonWebApp(const std::string& platform,
                       const GURL& url,
                       const std::string& id) override;

  // Called after the manager sends a message to the renderer regarding its
  // intention to show a prompt. The renderer will send a message back with the
  // opportunity to cancel.
  void OnBannerPromptReply(content::RenderFrameHost* render_frame_host,
                           int request_id,
                           blink::WebAppBannerPromptReply reply);

  // Cancels an active DataFetcher, stopping its banners from appearing.
  void CancelActiveFetcher();

  // Ideal icon size to use.
  const int ideal_icon_size_;

  // The type of navigation made to the page
  ui::PageTransition last_transition_type_;

  // Fetches the data required to display a banner for the current page.
  scoped_refptr<AppBannerDataFetcher> data_fetcher_;

  // A weak pointer is used as the lifetime of the ServiceWorkerContext is
  // longer than the lifetime of this banner manager. The banner manager
  // might be gone when calls sent to the ServiceWorkerContext are completed.
  base::WeakPtrFactory<AppBannerManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManager);
};  // class AppBannerManager

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_H_
