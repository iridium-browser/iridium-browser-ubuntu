// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_EXPERIMENTS_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_EXPERIMENTS_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "net/nqe/effective_connection_type.h"

namespace previews {

namespace params {

// The maximum number of recent previews navigations the black list looks at to
// determine if a host is blacklisted.
size_t MaxStoredHistoryLengthForPerHostBlackList();

// The maximum number of recent previews navigations the black list looks at to
// determine if all previews navigations are disallowed.
size_t MaxStoredHistoryLengthForHostIndifferentBlackList();

// The maximum number of hosts allowed in the in memory black list.
size_t MaxInMemoryHostsInBlackList();

// The number of recent navigations that were opted out of for a given host that
// would trigger that host to be blacklisted.
int PerHostBlackListOptOutThreshold();

// The number of recent navigations that were opted out of that would trigger
// all previews navigations to be disallowed.
int HostIndifferentBlackListOptOutThreshold();

// The amount of time a host remains blacklisted due to opt outs.
base::TimeDelta PerHostBlackListDuration();

// The amount of time all previews navigations are disallowed due to opt outs.
base::TimeDelta HostIndifferentBlackListPerHostDuration();

// The amount of time after any opt out that no previews should be shown.
base::TimeDelta SingleOptOutDuration();

// The amount of time that an offline page is considered fresh enough to be
// shown as a preview.
base::TimeDelta OfflinePreviewFreshnessDuration();

// The threshold of EffectiveConnectionType above which previews should not be
// served.
net::EffectiveConnectionType EffectiveConnectionTypeThreshold();

}  // namespace params

enum class PreviewsType {
  NONE = 0,
  OFFLINE = 1,
  // Insert new enum values here. Keep values sequential to allow looping
  // from NONE+1 to LAST-1.
  LAST = 2,
};

typedef std::vector<std::pair<PreviewsType, int>> PreviewsTypeList;

// Returns true if any client-side previews experiment is active.
bool IsIncludedInClientSidePreviewsExperimentsFieldTrial();

// Returns true if the field trial that should enable previews for |type| for
// prohibitvely slow networks is active.
bool IsPreviewsTypeEnabled(PreviewsType type);

// Returns the version of preview treatment |type|. Defaults to 0 if not
// specified in field trial config.
int GetPreviewsTypeVersion(PreviewsType type);

// Returns the enabled PreviewsTypes with their version.
std::unique_ptr<PreviewsTypeList> GetEnabledPreviews();

// Sets the appropriate state for field trial and variations to imitate the
// offline pages field trial.
bool EnableOfflinePreviewsForTesting();

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_EXPERIMENTS_H_
