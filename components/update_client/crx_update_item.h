// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CRX_UPDATE_ITEM_H_
#define COMPONENTS_UPDATE_CLIENT_CRX_UPDATE_ITEM_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/update_client.h"

namespace update_client {

// This is the one and only per-item state structure. Designed to be hosted
// in a std::vector or a std::list. The two main members are |component|
// which is supplied by the the component updater client and |status| which
// is modified as the item is processed by the update pipeline. The expected
// transition graph is:
//
//                  on-demand                on-demand
//   +---------------------------> kNew <--------------+-------------+
//   |                              |                  |             |
//   |                              V                  |             |
//   |   +--------------------> kChecking -<-------+---|---<-----+   |
//   |   |                          |              |   |         |   |
//   |   |            error         V       no     |   |         |   |
//  kNoUpdate <---------------- [update?] ->---- kUpToDate     kUpdated
//     ^                            |                              ^
//     |                        yes |                              |
//     |        diff=false          V                              |
//     |          +-----------> kCanUpdate                         |
//     |          |                 |                              |
//     |          |                 V              no              |
//     |          |        [differential update?]->----+           |
//     |          |                 |                  |           |
//     |          |             yes |                  |           |
//     |          |   error         V                  |           |
//     |          +---------<- kDownloadingDiff        |           |
//     |          |                 |                  |           |
//     |          |                 |                  |           |
//     |          |   error         V                  |           |
//     |          +---------<- kUpdatingDiff ->--------|-----------+ success
//     |                                               |           |
//     |              error                            V           |
//     +----------------------------------------- kDownloading     |
//     |                                               |           |
//     |              error                            V           |
//     +------------------------------------------ kUpdating ->----+ success
//
// TODO(sorin): this data structure will be further refactored once
// the new update service is in place. For the time being, it remains as-is,
// since it is used by the old component update service.
struct CrxUpdateItem {
  enum class State {
    kNew,
    kChecking,
    kCanUpdate,
    kDownloadingDiff,
    kDownloading,
    kDownloaded,
    kUpdatingDiff,
    kUpdating,
    kUpdated,
    kUpToDate,
    kNoUpdate,
    kLastStatus
  };

  // Call CrxUpdateService::ChangeItemState to change |status|. The function may
  // enforce conditions or notify observers of the change.
  State state;

  std::string id;
  CrxComponent component;

  base::Time last_check;

  // A component can be made available for download from several urls.
  std::vector<GURL> crx_urls;
  std::vector<GURL> crx_diffurls;

  // The from/to version and fingerprint values.
  Version previous_version;
  Version next_version;
  std::string previous_fp;
  std::string next_fp;

  // True if the current update check cycle is on-demand.
  bool on_demand;

  // True if the differential update failed for any reason.
  bool diff_update_failed;

  // The error information for full and differential updates.
  // The |error_category| contains a hint about which module in the component
  // updater generated the error. The |error_code| constains the error and
  // the |extra_code1| usually contains a system error, but it can contain
  // any extended information that is relevant to either the category or the
  // error itself.
  int error_category;
  int error_code;
  int extra_code1;
  int diff_error_category;
  int diff_error_code;
  int diff_extra_code1;

  std::vector<CrxDownloader::DownloadMetrics> download_metrics;

  CrxUpdateItem();
  ~CrxUpdateItem();

  // Function object used to find a specific component.
  class FindById {
   public:
    explicit FindById(const std::string& id) : id_(id) {}

    bool operator()(CrxUpdateItem* item) const { return item->id == id_; }

   private:
    const std::string& id_;
  };
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CRX_UPDATE_ITEM_H_
