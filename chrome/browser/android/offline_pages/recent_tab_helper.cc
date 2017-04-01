// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

#include <queue>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_notification_bridge.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::RecentTabHelper);

namespace {
class DefaultDelegate: public offline_pages::RecentTabHelper::Delegate {
 public:
  DefaultDelegate() {}

  // offline_pages::RecentTabHelper::Delegate
  std::unique_ptr<offline_pages::OfflinePageArchiver> CreatePageArchiver(
      content::WebContents* web_contents) override {
    return base::MakeUnique<offline_pages::OfflinePageMHTMLArchiver>(
        web_contents);
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }
  bool GetTabId(content::WebContents* web_contents, int* tab_id) override {
    return offline_pages::OfflinePageUtils::GetTabId(web_contents, tab_id);
  }
};
}  // namespace

namespace offline_pages {

bool RecentTabHelper::SnapshotProgressInfo::IsForLastN() {
  // A last_n snapshot always has an invalid request id.
  return request_id == OfflinePageModel::kInvalidOfflineId;
}

RecentTabHelper::RecentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      delegate_(new DefaultDelegate()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

RecentTabHelper::~RecentTabHelper() {
}

void RecentTabHelper::SetDelegate(
    std::unique_ptr<RecentTabHelper::Delegate> delegate) {
  DCHECK(delegate);
  delegate_ = std::move(delegate);
}

void RecentTabHelper::ObserveAndDownloadCurrentPage(
    const ClientId& client_id, int64_t request_id) {
  EnsureInitialized();
  snapshot_info_ =
      base::MakeUnique<SnapshotProgressInfo>(client_id, request_id);

  // If this tab helper is not enabled, immediately give the job back to
  // RequestCoordinator.
  if (!snapshots_enabled_ || !page_model_) {
    ReportDownloadStatusToRequestCoordinator();
    weak_ptr_factory_.InvalidateWeakPtrs();
    snapshot_info_.reset();
    return;
  }

  // No snapshots yet happened on the current page - return and wait for some.
  if (!is_page_ready_for_snapshot_)
    return;

  // If snapshot already happened and we missed it, go ahead and snapshot now.
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = web_contents()->GetLastCommittedURL();
  save_page_params.client_id = client_id;
  save_page_params.proposed_offline_id = request_id;
  page_model_->SavePage(
      save_page_params,
      delegate_->CreatePageArchiver(web_contents()),
      base::Bind(&RecentTabHelper::SavePageCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

// Initialize lazily. It needs TabAndroid for initialization, which is also a
// TabHelper - so can't initialize in constructor because of uncertain order
// of creation of TabHelpers.
void RecentTabHelper::EnsureInitialized() {
  if (snapshot_controller_)  // Initialized already.
    return;

  snapshot_controller_.reset(
      new SnapshotController(delegate_->GetTaskRunner(), this));
  snapshot_controller_->Stop();  // It is reset when navigation commits.

  int tab_id_number = 0;
  tab_id_.clear();

  if (delegate_->GetTabId(web_contents(), &tab_id_number))
    tab_id_ = base::IntToString(tab_id_number);

  // TODO(dimich): When we have BackgroundOffliner, avoid capturing prerenderer
  // WebContents with its origin as well.
  snapshots_enabled_ = !tab_id_.empty() &&
                       !web_contents()->GetBrowserContext()->IsOffTheRecord();

  if (!snapshots_enabled_)
    return;

  page_model_ = OfflinePageModelFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

void RecentTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || navigation_handle->IsSamePage() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  EnsureInitialized();
  if (!snapshots_enabled_)
    return;

  // Cancel tasks in flight that relate to the previous page.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // We navigated to a different page, lets report progress to Background
  // Offliner.
  ReportDownloadStatusToRequestCoordinator();

  if (offline_pages::IsOffliningRecentPagesEnabled()) {
    // Note: at any point in time |snapshot_info_| might be replaced with one
    // created by ObserveAndDownloadCurrentPage for a download snapshot request.
    snapshot_info_ =
        base::MakeUnique<SnapshotProgressInfo>(GetRecentPagesClientId());
  } else {
    snapshot_info_.reset();
  }

  is_page_ready_for_snapshot_ = false;

  // New navigation, new snapshot session.
  snapshot_url_ = web_contents()->GetLastCommittedURL();

  // Check for conditions that would cause us not to snapshot.
  bool can_save = !navigation_handle->IsErrorPage() &&
                  OfflinePageModel::CanSaveURL(snapshot_url_) &&
                  OfflinePageUtils::GetOfflinePageFromWebContents(
                      web_contents()) == nullptr;

  UMA_HISTOGRAM_BOOLEAN("OfflinePages.CanSaveRecentPage", can_save);

  // Always reset so that posted tasks get canceled.
  snapshot_controller_->Reset();

  if (!can_save)
    snapshot_controller_->Stop();
}

void RecentTabHelper::DocumentAvailableInMainFrame() {
  EnsureInitialized();
  snapshot_controller_->DocumentAvailableInMainFrame();
}

void RecentTabHelper::DocumentOnLoadCompletedInMainFrame() {
  EnsureInitialized();
  snapshot_controller_->DocumentOnLoadCompletedInMainFrame();
}

void RecentTabHelper::WebContentsDestroyed() {
  // WebContents (and maybe Tab) is destroyed, report status to Offliner.
  ReportDownloadStatusToRequestCoordinator();
}


// This starts a sequence of async operations chained through callbacks:
// - compute the set of old 'last_n' pages that have to be purged
// - delete the pages found in the previous step
// - snapshot the current web contents
// Along the chain, the original URL is passed and compared, to detect
// possible navigation and cancel snapshot in that case.
void RecentTabHelper::StartSnapshot() {
  is_page_ready_for_snapshot_ = true;

  if (!snapshots_enabled_ || !page_model_ || !snapshot_info_) {
    ReportSnapshotCompleted();
    return;
  }

  // Remove previously captured pages for this tab.
  page_model_->GetOfflineIdsForClientId(
    GetRecentPagesClientId(),
    base::Bind(&RecentTabHelper::ContinueSnapshotWithIdsToPurge,
               weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabHelper::ContinueSnapshotWithIdsToPurge(
    const std::vector<int64_t>& page_ids) {
  DCHECK(snapshot_info_);

  // Also remove the download page if this is not a first snapshot.
  std::vector<int64_t> ids(page_ids);
  ids.push_back(snapshot_info_->request_id);

  page_model_->DeletePagesByOfflineId(
      ids, base::Bind(&RecentTabHelper::ContinueSnapshotAfterPurge,
                      weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabHelper::ContinueSnapshotAfterPurge(
    OfflinePageModel::DeletePageResult result) {
  DCHECK(snapshot_info_);
  DCHECK_EQ(snapshot_url_, web_contents()->GetLastCommittedURL());
  if (result != OfflinePageModel::DeletePageResult::SUCCESS) {
    ReportSnapshotCompleted();
    return;
  }

  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = snapshot_url_;
  save_page_params.client_id = snapshot_info_->client_id;
  save_page_params.proposed_offline_id = snapshot_info_->request_id;
  save_page_params.is_background = false;
  page_model_->SavePage(save_page_params,
                        delegate_->CreatePageArchiver(web_contents()),
                        base::Bind(&RecentTabHelper::SavePageCallback,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void RecentTabHelper::SavePageCallback(OfflinePageModel::SavePageResult result,
                                       int64_t offline_id) {
  DCHECK(snapshot_info_);
  snapshot_info_->page_snapshot_completed = (result == SavePageResult::SUCCESS);
  ReportSnapshotCompleted();
}

void RecentTabHelper::ReportSnapshotCompleted() {
  snapshot_controller_->PendingSnapshotCompleted();
  // Tell RequestCoordinator how the request should be processed further.
  ReportDownloadStatusToRequestCoordinator();
}

void RecentTabHelper::ReportDownloadStatusToRequestCoordinator() {
  if (!snapshot_info_ || snapshot_info_->IsForLastN())
    return;

  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!request_coordinator)
    return;

  // It is OK to call these methods more then once, depending on
  // number of snapshots attempted in this tab helper. If the request_id is not
  // in the list of RequestCoordinator, these calls have no effect.
  if (snapshot_info_->page_snapshot_completed)
    request_coordinator->MarkRequestCompleted(snapshot_info_->request_id);
  else
    request_coordinator->EnableForOffliner(snapshot_info_->request_id,
                                           snapshot_info_->client_id);
}

ClientId RecentTabHelper::GetRecentPagesClientId() const {
  return ClientId(kLastNNamespace, tab_id_);
}

}  // namespace offline_pages
