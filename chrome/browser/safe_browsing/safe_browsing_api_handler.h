// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between
// RemoteSafeBrowsingDatabaseManager and Java-based API to check URLs.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_API_HANDLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_API_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "url/gurl.h"

class SafeBrowsingApiHandler {
 public:
  // Singleton interface.
  static void SetInstance(SafeBrowsingApiHandler* instance);
  static SafeBrowsingApiHandler* GetInstance();

  typedef base::Callback<void(SBThreatType sb_threat_type,
                              const std::string& metadata)> URLCheckCallback;

  // Makes Native->Java call and invokes callback when check is done.
  virtual void StartURLCheck(const URLCheckCallback& callback,
                             const GURL& url,
                             const std::vector<SBThreatType>& threat_types) = 0;

  virtual ~SafeBrowsingApiHandler() {}

 private:
  // Pointer not owned.
  static SafeBrowsingApiHandler* instance_;
};
#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_API_HANDLER_H_
