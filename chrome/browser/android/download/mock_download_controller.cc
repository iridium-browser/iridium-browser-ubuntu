// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/mock_download_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chrome {
namespace android {

MockDownloadController::MockDownloadController()
    : approve_file_access_request_(true) {
}

MockDownloadController::~MockDownloadController() {}

void MockDownloadController::CreateGETDownload(
    int render_process_id, int render_view_id,
    bool must_download, const DownloadInfo& info) {
}

void MockDownloadController::OnDownloadStarted(
    content::DownloadItem* download_item) {
}

void MockDownloadController::StartContextMenuDownload(
    const content::ContextMenuParams& params,
    content::WebContents* web_contents,
    bool is_link, const std::string& extra_headers) {
}

void MockDownloadController::DangerousDownloadValidated(
    content::WebContents* web_contents,
    const std::string& download_guid,
    bool accept) {}

void MockDownloadController::AcquireFileAccessPermission(
    content::WebContents* web_contents,
    const DownloadControllerBase::AcquireFileAccessPermissionCallback& cb) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(cb, approve_file_access_request_));
}

void MockDownloadController::SetApproveFileAccessRequestForTesting(
    bool approve) {
  approve_file_access_request_ = approve;
}

}  // namespace android
}  // namespace chrome
