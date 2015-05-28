// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/download_row.h"

#include "components/history/core/browser/download_constants.h"

namespace history {

DownloadRow::DownloadRow()
    : received_bytes(0),
      total_bytes(0),
      state(DownloadState::IN_PROGRESS),
      danger_type(DownloadDangerType::NOT_DANGEROUS),
      id(kInvalidDownloadId),
      opened(false) {
  // |interrupt_reason| is left undefined by this constructor as the value
  // has no meaning unless |state| is equal to kStateInterrupted.
}

DownloadRow::DownloadRow(const base::FilePath& current_path,
                         const base::FilePath& target_path,
                         const std::vector<GURL>& url_chain,
                         const GURL& referrer,
                         const std::string& mime_type,
                         const std::string& original_mime_type,
                         const base::Time& start,
                         const base::Time& end,
                         const std::string& etag,
                         const std::string& last_modified,
                         int64 received,
                         int64 total,
                         DownloadState download_state,
                         DownloadDangerType danger_type,
                         DownloadInterruptReason interrupt_reason,
                         DownloadId id,
                         bool download_opened,
                         const std::string& ext_id,
                         const std::string& ext_name)
    : current_path(current_path),
      target_path(target_path),
      url_chain(url_chain),
      referrer_url(referrer),
      mime_type(mime_type),
      original_mime_type(original_mime_type),
      start_time(start),
      end_time(end),
      etag(etag),
      last_modified(last_modified),
      received_bytes(received),
      total_bytes(total),
      state(download_state),
      danger_type(danger_type),
      interrupt_reason(interrupt_reason),
      id(id),
      opened(download_opened),
      by_ext_id(ext_id),
      by_ext_name(ext_name) {
}

DownloadRow::~DownloadRow() {
}

}  // namespace history
