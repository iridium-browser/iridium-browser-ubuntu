// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the SafeBrowsingDatabaseManager that sends URLs
// via IPC to a database that chromium doesn't manage locally.

#ifndef CHROME_BROWSER_SAFE_BROWSING_REMOTE_DATABASE_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_REMOTE_DATABASE_MANAGER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "url/gurl.h"

// An implementation that proxies requests to a service outside of Chromium.
// Does not manage a local database.
class RemoteSafeBrowsingDatabaseManager : public SafeBrowsingDatabaseManager {
 public:
  // Construct RemoteSafeBrowsingDatabaseManager.
  // Must be initialized by calling StartOnIOThread() before using.
  RemoteSafeBrowsingDatabaseManager();

  //
  // SafeBrowsingDatabaseManager implementation
  //

  bool CanCheckUrl(const GURL& url) const override;
  bool download_protection_enabled() const override;
  bool CheckBrowseUrl(const GURL& url, Client* client) override;
  void CancelCheck(Client* client) override;
  void StartOnIOThread() override;
  void StopOnIOThread(bool shutdown) override;

  // These will fail with DCHECK() since their functionality isn't implemented.
  // We may later add support for a subset of them.
  bool CheckDownloadUrl(const std::vector<GURL>& url_chain,
                        Client* client) override;
  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;
  bool MatchCsdWhitelistUrl(const GURL& url) override;
  bool MatchMalwareIP(const std::string& ip_address) override;
  bool MatchDownloadWhitelistUrl(const GURL& url) override;
  bool MatchDownloadWhitelistString(const std::string& str) override;
  bool MatchInclusionWhitelistUrl(const GURL& url) override;
  bool IsMalwareKillSwitchOn() override;
  bool IsCsdWhitelistKillSwitchOn() override;

  //
  // RemoteSafeBrowsingDatabaseManager implementation
  //

 private:
  ~RemoteSafeBrowsingDatabaseManager() override;
  class ClientRequest;  // Per-request tracker.

  // Requests currently outstanding.  This owns the ptrs.
  std::vector<ClientRequest*> current_requests_;
  bool enabled_;

  friend class base::RefCountedThreadSafe<RemoteSafeBrowsingDatabaseManager>;
  DISALLOW_COPY_AND_ASSIGN(RemoteSafeBrowsingDatabaseManager);
};  // class RemoteSafeBrowsingDatabaseManager

#endif  // CHROME_BROWSER_SAFE_BROWSING_REMOTE_DATABASE_MANAGER_H_
