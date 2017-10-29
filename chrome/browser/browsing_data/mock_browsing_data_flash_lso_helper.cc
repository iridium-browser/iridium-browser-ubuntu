// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_flash_lso_helper.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataFlashLSOHelper::MockBrowsingDataFlashLSOHelper(
    content::BrowserContext* browser_context) {
}
void MockBrowsingDataFlashLSOHelper::StartFetching(
    const GetSitesWithFlashDataCallback& callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = callback;
}

void MockBrowsingDataFlashLSOHelper::DeleteFlashLSOsForSite(
    const std::string& site, const base::Closure& callback) {
  std::vector<std::string>::iterator entry =
      std::find(domains_.begin(), domains_.end(), site);
  ASSERT_TRUE(entry != domains_.end());
  domains_.erase(entry);
  if (!callback.is_null())
    callback.Run();
}

void MockBrowsingDataFlashLSOHelper::AddFlashLSODomain(
    const std::string& domain) {
  domains_.push_back(domain);
}

void MockBrowsingDataFlashLSOHelper::Notify() {
  callback_.Run(domains_);
  callback_ = GetSitesWithFlashDataCallback();
}

bool MockBrowsingDataFlashLSOHelper::AllDeleted() {
  return domains_.empty();
}

MockBrowsingDataFlashLSOHelper::~MockBrowsingDataFlashLSOHelper() {
}
