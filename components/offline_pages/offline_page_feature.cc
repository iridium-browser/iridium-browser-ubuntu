// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_feature.h"

#include <string>

#include "base/feature_list.h"

namespace offline_pages {

const base::Feature kOfflineBookmarksFeature {
   "OfflineBookmarks", base::FEATURE_DISABLED_BY_DEFAULT
};

const base::Feature kOffliningRecentPagesFeature {
   "OfflineRecentPages", base::FEATURE_DISABLED_BY_DEFAULT
};

const base::Feature kOfflinePagesBackgroundLoadingFeature {
   "OfflinePagesBackgroundLoading", base::FEATURE_DISABLED_BY_DEFAULT
};

const base::Feature kOfflinePagesCTFeature {
   "OfflinePagesCT", base::FEATURE_ENABLED_BY_DEFAULT
};

const base::Feature kOfflinePagesSharingFeature{
    "OfflinePagesSharing", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBackgroundLoaderForDownloadsFeature{
    "BackgroundLoadingForDownloads", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsOfflinePagesEnabled() {
  return IsOfflineBookmarksEnabled() || IsOffliningRecentPagesEnabled() ||
         IsOfflinePagesBackgroundLoadingEnabled() ||
         IsOfflinePagesCTEnabled() || IsOfflinePagesSharingEnabled();
}

bool IsOfflineBookmarksEnabled() {
  return base::FeatureList::IsEnabled(kOfflineBookmarksFeature);
}

bool IsOffliningRecentPagesEnabled() {
  return  base::FeatureList::IsEnabled(kOffliningRecentPagesFeature);
}

bool IsOfflinePagesBackgroundLoadingEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesBackgroundLoadingFeature);
}

bool IsOfflinePagesCTEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesCTFeature);
}

bool IsOfflinePagesSharingEnabled() {
  return base::FeatureList::IsEnabled(kOfflinePagesSharingFeature);
}

bool IsBackgroundLoaderForDownloadsEnabled() {
  return base::FeatureList::IsEnabled(kBackgroundLoaderForDownloadsFeature);
}

}  // namespace offline_pages
