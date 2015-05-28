// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "components/favicon/core/favicon_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/favicon_url.h"

class GURL;
class SkBitmap;

namespace gfx {
class Image;
}

namespace bookmarks {
class BookmarkModel;
}

namespace content {
struct FaviconStatus;
}

namespace favicon {
class FaviconDriverObserver;
class FaviconHandler;
class FaviconService;
}

namespace history {
class HistoryService;
}

// FaviconTabHelper works with favicon::FaviconHandlers to fetch the favicons.
//
// FetchFavicon fetches the given page's icons. It requests the icons from the
// history backend. If the icon is not available or expired, the icon will be
// downloaded and saved in the history backend.
//
class FaviconTabHelper : public content::WebContentsObserver,
                         public favicon::FaviconDriver,
                         public content::WebContentsUserData<FaviconTabHelper> {
 public:
  ~FaviconTabHelper() override;

  static void CreateForWebContents(content::WebContents* web_contents);

  // Initiates loading the favicon for the specified url.
  void FetchFavicon(const GURL& url);

  // Returns the favicon for this tab, or IDR_DEFAULT_FAVICON if the tab does
  // not have a favicon. The default implementation uses the current navigation
  // entry. This will return an empty bitmap if there are no navigation
  // entries, which should rarely happen.
  gfx::Image GetFavicon() const;

  // Returns true if we have the favicon for the page.
  bool FaviconIsValid() const;

  // Returns whether the favicon should be displayed. If this returns false, no
  // space is provided for the favicon, and the favicon is never displayed.
  bool ShouldDisplayFavicon();

  // Returns the current tab's favicon urls. If this is empty,
  // DidUpdateFaviconURL has not yet been called for the current navigation.
  const std::vector<content::FaviconURL>& favicon_urls() const {
    return favicon_urls_;
  }

  // content::WebContentsObserver override. Must be public, because also
  // called from PrerenderContents.
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;

  // Saves the favicon for the current page.
  void SaveFavicon();

  void AddObserver(favicon::FaviconDriverObserver* observer);
  void RemoveObserver(favicon::FaviconDriverObserver* observer);

  // favicon::FaviconDriver methods.
  int StartDownload(const GURL& url, int max_bitmap_size) override;
  bool IsOffTheRecord() override;
  bool IsBookmarked(const GURL& url) override;
  GURL GetActiveURL() override;
  base::string16 GetActiveTitle() override;
  bool GetActiveFaviconValidity() override;
  void SetActiveFaviconValidity(bool valid) override;
  GURL GetActiveFaviconURL() override;
  void SetActiveFaviconURL(const GURL& url) override;
  gfx::Image GetActiveFaviconImage() override;
  void SetActiveFaviconImage(const gfx::Image& image) override;
  void OnFaviconAvailable(const gfx::Image& image,
                          const GURL& url,
                          bool is_active_favicon) override;

  // Favicon download callback.
  void DidDownloadFavicon(
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

 private:
  friend class content::WebContentsUserData<FaviconTabHelper>;
  friend class FaviconTabHelperTest;

  // Creates a new FaviconTabHelper bound to |web_contents|. Initialize
  // |favicon_service_|, |bookmark_model_| and |history_service_| from the
  // corresponding parameter.
  FaviconTabHelper(content::WebContents* web_contents,
                   favicon::FaviconService* favicon_service,
                   history::HistoryService* history_service,
                   bookmarks::BookmarkModel* bookmark_model);

  // content::WebContentsObserver overrides.
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  // Helper method that returns the active navigation entry's favicon.
  content::FaviconStatus& GetFaviconStatus();

  // KeyedService used by FaviconTabHelper. They may be null during testing, but
  // if they are defined, they must outlive the FaviconTabHelper.
  favicon::FaviconService* favicon_service_;
  history::HistoryService* history_service_;
  bookmarks::BookmarkModel* bookmark_model_;

  std::vector<content::FaviconURL> favicon_urls_;

  // Bypass cache when downloading favicons for this page URL.
  GURL bypass_cache_page_url_;

  // FaviconHandlers used to download the different kind of favicons. Both
  // |touch_icon_handler_| and |large_icon_handler_| may be null depending
  // on the platform or variations.
  scoped_ptr<favicon::FaviconHandler> favicon_handler_;
  scoped_ptr<favicon::FaviconHandler> touch_icon_handler_;
  scoped_ptr<favicon::FaviconHandler> large_icon_handler_;

  ObserverList<favicon::FaviconDriverObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FaviconTabHelper);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
