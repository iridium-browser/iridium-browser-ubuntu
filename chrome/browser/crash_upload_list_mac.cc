// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list_mac.h"

#include "base/time/time.h"
#include "components/crash/app/crashpad_mac.h"

CrashUploadListMac::CrashUploadListMac(Delegate* delegate,
                                       const base::FilePath& upload_log_path)
    : CrashUploadList(delegate, upload_log_path) {
}

CrashUploadListMac::~CrashUploadListMac() {
}

void CrashUploadListMac::LoadUploadList() {
  std::vector<crash_reporter::UploadedReport> uploaded_reports;
  crash_reporter::GetUploadedReports(&uploaded_reports);

  ClearUploads();
  for (const crash_reporter::UploadedReport& uploaded_report :
       uploaded_reports) {
    AppendUploadInfo(
        UploadInfo(uploaded_report.remote_id,
                   base::Time::FromTimeT(uploaded_report.creation_time),
                   uploaded_report.local_id));
  }
}
