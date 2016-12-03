// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file should not be build on Android but is currently getting built.
// TODO(vakh): Fix that: http://crbug.com/621647

#include "components/safe_browsing_db/v4_local_database_manager.h"

#include <vector>

#include "base/callback.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

// TODO(vakh): Implement this to populate the map appopriately.
// Filed as http://crbug.com/608075
StoreFileNameMap GetStoreFileNameMap() {
  return StoreFileNameMap({{GetUrlMalwareId(), "UrlMalware.store"},
                           {GetUrlSocEngId(), "UrlSoceng.store"}});
}

}  // namespace

V4LocalDatabaseManager::V4LocalDatabaseManager(const base::FilePath& base_path)
    : base_path_(base_path), enabled_(false) {
  DCHECK(!base_path_.empty());
  DVLOG(1) << "V4LocalDatabaseManager::V4LocalDatabaseManager: "
           << "base_path_: " << base_path_.AsUTF8Unsafe();
}

V4LocalDatabaseManager::~V4LocalDatabaseManager() {
  DCHECK(!enabled_);
}

bool V4LocalDatabaseManager::IsSupported() const {
  return true;
}

ThreatSource V4LocalDatabaseManager::GetThreatSource() const {
  return ThreatSource::LOCAL_PVER4;
}

bool V4LocalDatabaseManager::ChecksAreAlwaysAsync() const {
  return false;
}

bool V4LocalDatabaseManager::CanCheckResourceType(
    content::ResourceType resource_type) const {
  // We check all types since most checks are fast.
  return true;
}

bool V4LocalDatabaseManager::CanCheckUrl(const GURL& url) const {
  return url.SchemeIs(url::kHttpsScheme) || url.SchemeIs(url::kHttpScheme) ||
         url.SchemeIs(url::kFtpScheme);
}

bool V4LocalDatabaseManager::IsDownloadProtectionEnabled() const {
  // TODO(vakh): Investigate the possibility of using a command line switch for
  // this instead.
  return true;
}

bool V4LocalDatabaseManager::CheckDownloadUrl(
    const std::vector<GURL>& url_chain,
    Client* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(vakh): Implement this skeleton.
  return true;
}

bool V4LocalDatabaseManager::CheckExtensionIDs(
    const std::set<std::string>& extension_ids,
    Client* client) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchMalwareIP(const std::string& ip_address) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return false;
}

bool V4LocalDatabaseManager::MatchCsdWhitelistUrl(const GURL& url) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchDownloadWhitelistUrl(const GURL& url) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchDownloadWhitelistString(
    const std::string& str) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::MatchModuleWhitelistString(
    const std::string& str) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::CheckResourceUrl(const GURL& url, Client* client) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::IsMalwareKillSwitchOn() {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::IsCsdWhitelistKillSwitchOn() {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return true;
}

bool V4LocalDatabaseManager::CheckBrowseUrl(const GURL& url, Client* client) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!enabled_ || !CanCheckUrl(url)) {
    return true;
  }

  if (v4_database_) {
    base::hash_set<FullHash> full_hashes;
    V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);

    base::hash_set<UpdateListIdentifier> stores_to_look(
        {GetUrlMalwareId(), GetUrlSocEngId()});
    base::hash_set<HashPrefix> matched_hash_prefixes;
    base::hash_set<UpdateListIdentifier> matched_stores;
    MatchedHashPrefixMap matched_hash_prefix_map;
    for (const auto& full_hash : full_hashes) {
      v4_database_->GetStoresMatchingFullHash(full_hash, stores_to_look,
                                              &matched_hash_prefix_map);
      for (const auto& matched_pair : matched_hash_prefix_map) {
        matched_stores.insert(matched_pair.first);
        matched_hash_prefixes.insert(matched_pair.second);
      }
    }

    DCHECK_EQ(matched_stores.empty(), matched_hash_prefixes.empty());

    // TODO(vakh): Return false and fetch full hashes for the matching hash
    // prefixes.
    return matched_hash_prefixes.empty();
  } else {
    // TODO(vakh): Queue the check and process it when the database becomes
    // ready.
    return false;
  }
}

void V4LocalDatabaseManager::CancelCheck(Client* client) {
  // TODO(vakh): Implement this skeleton.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(enabled_);
}

void V4LocalDatabaseManager::StartOnIOThread(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  SafeBrowsingDatabaseManager::StartOnIOThread(request_context_getter, config);

  db_updated_callback_ = base::Bind(&V4LocalDatabaseManager::DatabaseUpdated,
                                    base::Unretained(this));

  SetupUpdateProtocolManager(request_context_getter, config);

  SetupDatabase();

  enabled_ = true;
}

void V4LocalDatabaseManager::SetupUpdateProtocolManager(
    net::URLRequestContextGetter* request_context_getter,
    const V4ProtocolConfig& config) {
  V4UpdateCallback callback = base::Bind(
      &V4LocalDatabaseManager::UpdateRequestCompleted, base::Unretained(this));

  v4_update_protocol_manager_ =
      V4UpdateProtocolManager::Create(request_context_getter, config, callback);
}

void V4LocalDatabaseManager::SetupDatabase() {
  DCHECK(!base_path_.empty());
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Only get a new task runner if there isn't one already. If the service has
  // previously been started and stopped, a task runner could already exist.
  if (!task_runner_) {
    base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
    task_runner_ = pool->GetSequencedTaskRunnerWithShutdownBehavior(
        pool->GetSequenceToken(), base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  }

  // Do not create the database on the IO thread since this may be an expensive
  // operation. Instead, do that on the task_runner and when the new database
  // has been created, swap it out on the IO thread.
  StoreFileNameMap store_file_name_map = GetStoreFileNameMap();
  DCHECK(!store_file_name_map.empty());
  NewDatabaseReadyCallback db_ready_callback = base::Bind(
      &V4LocalDatabaseManager::DatabaseReady, base::Unretained(this));
  V4Database::Create(task_runner_, base_path_, store_file_name_map,
                     db_ready_callback);
}

void V4LocalDatabaseManager::DatabaseReady(
    std::unique_ptr<V4Database> v4_database) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The following check is needed because it is possible that by the time the
  // database is ready, StopOnIOThread has been called.
  if (enabled_) {
    v4_database_ = std::move(v4_database);

    // The database is in place. Start fetching updates now.
    v4_update_protocol_manager_->ScheduleNextUpdate(
        v4_database_->GetStoreStateMap());
  } else {
    // Schedule the deletion of v4_database off IO thread.
    V4Database::Destroy(std::move(v4_database));
  }
}

void V4LocalDatabaseManager::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  enabled_ = false;

  // Delete the V4Database. Any pending writes to disk are completed.
  // This operation happens on the task_runner on which v4_database_ operates
  // and doesn't block the IO thread.
  V4Database::Destroy(std::move(v4_database_));

  // Delete the V4UpdateProtocolManager.
  // This cancels any in-flight update request.
  v4_update_protocol_manager_.reset();

  db_updated_callback_.Reset();

  SafeBrowsingDatabaseManager::StopOnIOThread(shutdown);
}

void V4LocalDatabaseManager::UpdateRequestCompleted(
    std::unique_ptr<ParsedServerResponse> parsed_server_response) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  v4_database_->ApplyUpdate(std::move(parsed_server_response),
                            db_updated_callback_);
}

void V4LocalDatabaseManager::DatabaseUpdated() {
  if (enabled_) {
    v4_update_protocol_manager_->ScheduleNextUpdate(
        v4_database_->GetStoreStateMap());
  }
}

}  // namespace safe_browsing
