// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/remote_database_manager.h"

#include <vector>

#include "chrome/browser/safe_browsing/android_safe_browsing_api_handler.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

//
// RemoteSafeBrowsingDatabaseManager::ClientRequest methods
//
class RemoteSafeBrowsingDatabaseManager::ClientRequest {
 public:
  ClientRequest(Client* client,
                RemoteSafeBrowsingDatabaseManager* db_manager,
                const GURL& url);

  static void OnRequestDoneWeak(const base::WeakPtr<ClientRequest>& req,
                                SBThreatType matched_threat_type,
                                const std::string& metadata);
  void OnRequestDone(SBThreatType matched_threat_type,
                     const std::string& metadata);

  // Accessors
  Client* client() const { return client_; }
  const GURL& url() const { return url_; }
  base::WeakPtr<ClientRequest> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  Client* client_;
  RemoteSafeBrowsingDatabaseManager* db_manager_;
  GURL url_;
  base::WeakPtrFactory<ClientRequest> weak_factory_;
};

RemoteSafeBrowsingDatabaseManager::ClientRequest::ClientRequest(
    Client* client,
    RemoteSafeBrowsingDatabaseManager* db_manager,
    const GURL& url)
    : client_(client), db_manager_(db_manager), url_(url), weak_factory_(this) {
}

// Static
void RemoteSafeBrowsingDatabaseManager::ClientRequest::OnRequestDoneWeak(
    const base::WeakPtr<ClientRequest>& req,
    SBThreatType matched_threat_type,
    const std::string& metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!req)
    return;  // Previously canceled
  req->OnRequestDone(matched_threat_type, metadata);
}

void RemoteSafeBrowsingDatabaseManager::ClientRequest::OnRequestDone(
    SBThreatType matched_threat_type,
    const std::string& metadata) {
  VLOG(1) << "OnRequestDone for client " << client_ << " and URL " << url_;
  client_->OnCheckBrowseUrlResult(url_, matched_threat_type, metadata);
  db_manager_->CancelCheck(client_);
}

//
// RemoteSafeBrowsingDatabaseManager methods
//

// TODO(nparker): Add tests for this class once implemented.
RemoteSafeBrowsingDatabaseManager::RemoteSafeBrowsingDatabaseManager()
    : enabled_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(nparker): Implement the initialization glue.
  // Check flag to see if this is enabled.
  // Ask AndroidSafeBrowsingAPIHandler if Java API is correct version.
}

RemoteSafeBrowsingDatabaseManager::~RemoteSafeBrowsingDatabaseManager() {
  DCHECK(!enabled_);
}

bool RemoteSafeBrowsingDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(url::kHttpsScheme) || url.SchemeIs(url::kHttpScheme) ||
         url.SchemeIs(url::kFtpScheme);
}

bool RemoteSafeBrowsingDatabaseManager::download_protection_enabled()
    const {
  return false;
}

bool RemoteSafeBrowsingDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  NOTREACHED();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  NOTREACHED();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::MatchMalwareIP(
    const std::string& ip_address) {
  NOTREACHED();
  return false;
}

bool RemoteSafeBrowsingDatabaseManager::MatchCsdWhitelistUrl(const GURL& url) {
  NOTREACHED();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::MatchDownloadWhitelistUrl(
    const GURL& url) {
  NOTREACHED();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::MatchDownloadWhitelistString(
    const std::string& str) {
  NOTREACHED();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::MatchInclusionWhitelistUrl(
    const GURL& url) {
  NOTREACHED();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::IsMalwareKillSwitchOn() {
  NOTREACHED();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::IsCsdWhitelistKillSwitchOn() {
  NOTREACHED();
  return true;
}

bool RemoteSafeBrowsingDatabaseManager::CheckBrowseUrl(const GURL& url,
                                                       Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!enabled_)
    return true;

  if (!CanCheckUrl(url))
    return true;  // Safe, continue right away.

  scoped_ptr<ClientRequest> req(new ClientRequest(client, this, url));
  std::vector<SBThreatType> threat_types;
  threat_types.push_back(SB_THREAT_TYPE_URL_MALWARE);

  VLOG(1) << "Checking for client " << client << " and URL " << url;
  bool started = api_handler_.StartURLCheck(
      base::Bind(&ClientRequest::OnRequestDoneWeak, req->GetWeakPtr()), url,
      threat_types);
  if (!started) {
    LOG(DFATAL) << "Failed to start Safe Browsing request";
    return true;
  }

  current_requests_.push_back(req.release());

  // Defer the resource load.
  return false;
}

void RemoteSafeBrowsingDatabaseManager::CancelCheck(Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(enabled_);
  for (auto itr = current_requests_.begin(); itr != current_requests_.end();
       ++itr) {
    if ((*itr)->client() == client) {
      VLOG(1) << "Canceling check for URL " << (*itr)->url();
      delete *itr;
      current_requests_.erase(itr);
      return;
    }
  }
  NOTREACHED();
}

void RemoteSafeBrowsingDatabaseManager::StartOnIOThread() {
  VLOG(1) << "RemoteSafeBrowsing starting";
  enabled_ = true;
}

void RemoteSafeBrowsingDatabaseManager::StopOnIOThread(bool shutdown) {
  // |shutdown| is not used.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(1) << "RemoteSafeBrowsing stopping";

  // Call back and delete any remaining clients. OnRequestDone() modifies
  // |current_requests_|, so we make a copy first.
  std::vector<ClientRequest*> to_callback(current_requests_);
  for (auto req : to_callback) {
    VLOG(1) << "Stopping: Invoking unfinished req for URL " << req->url();
    req->OnRequestDone(SB_THREAT_TYPE_SAFE, std::string());
  }
  enabled_ = false;
}

