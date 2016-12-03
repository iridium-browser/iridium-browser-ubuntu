// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/snapshot_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
class NavigationEntry;
class NavigationHandle;
}

namespace offline_pages {

// Attaches to every WebContent shown in a tab. Waits until the WebContent is
// loaded to proper degree and then makes a snapshot of the page. Removes the
// oldest snapshot in the 'ring buffer'. As a result, there is always up to N
// snapshots of recent pages on the device.
class RecentTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RecentTabHelper>,
      public SnapshotController::Client {
 public:
  ~RecentTabHelper() override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void DocumentOnLoadCompletedInMainFrame() override;

  // SnapshotController::Client
  void StartSnapshot() override;

  // Delegate that is used by RecentTabHelper to get external dependencies.
  // Default implementation lives in .cc file, while tests provide an override.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual std::unique_ptr<OfflinePageArchiver> CreatePageArchiver(
        content::WebContents* web_contents) = 0;
    virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() = 0;
    // There is no expectations that tab_id is always present.
    virtual bool GetTabId(content::WebContents* web_contents, int* tab_id) = 0;
  };
  void SetDelegate(std::unique_ptr<RecentTabHelper::Delegate> delegate);

  bool is_page_ready_for_snapshot() const {
    return is_page_ready_for_snapshot_;
  }

 private:
  explicit RecentTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<RecentTabHelper>;


  void EnsureInitialized();
  void ContinueSnapshotWithIdsToPurge(const std::vector<int64_t>& page_ids);
  void ContinueSnapshotAfterPurge(OfflinePageModel::DeletePageResult result);
  void SavePageCallback(OfflinePageModel::SavePageResult result,
                        int64_t offline_id);

  void ReportSnapshotCompleted();
  bool IsSamePage() const;
  ClientId client_id() const;

  // Page model is a service, no ownership. Can be null - for example, in
  // case when tab is in incognito profile.
  OfflinePageModel* page_model_;
  // If false, never make snapshots off the attached WebContents.
  // Not page-specific.
  bool snapshots_enabled_;
  // Becomes true during navigation if the page is ready for snapshot as
  // indicated by at least one callback from SnapshotController.
  bool is_page_ready_for_snapshot_;
  // If empty, the tab does not have AndroidId and can not capture pages.
  std::string tab_id_;

  // The URL of the page that is currently being snapshotted. Used to check,
  // during async operations, that WebContents still contains the same page.
  GURL snapshot_url_;
  // This starts out null and used as a flag for EnsureInitialized() to do the
  // initialization only once.
  std::unique_ptr<SnapshotController> snapshot_controller_;

  std::unique_ptr<Delegate> delegate_;

  base::WeakPtrFactory<RecentTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabHelper);
};

}  // namespace offline_pages
#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_
