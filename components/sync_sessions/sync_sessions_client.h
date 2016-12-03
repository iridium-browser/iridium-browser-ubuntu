// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_CLIENT_H_
#define COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_CLIENT_H_

#include <memory>

#include "base/macros.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
}

namespace browser_sync {
class LocalSessionEventRouter;
class SyncedWindowDelegatesGetter;
}

namespace favicon {
class FaviconService;
}

namespace history {
class HistoryService;
}

namespace sync_sessions {

// Interface for clients of a sync sessions datatype. Should be used as a getter
// for services and data the Sync Sessions datatype depends on.
class SyncSessionsClient {
 public:
  SyncSessionsClient();
  virtual ~SyncSessionsClient();

  // Getters for services that sessions depends on.
  virtual bookmarks::BookmarkModel* GetBookmarkModel() = 0;
  virtual favicon::FaviconService* GetFaviconService() = 0;
  virtual history::HistoryService* GetHistoryService() = 0;

  // Checks if the given url is considered interesting enough to sync. Most urls
  // are considered interesting. Examples of ones that are not are invalid urls,
  // files, and chrome internal pages.
  // TODO(zea): make this a standalone function if the url constants are
  // componentized.
  virtual bool ShouldSyncURL(const GURL& url) const = 0;

  // Returns the SyncedWindowDelegatesGetter for this client.
  virtual browser_sync::SyncedWindowDelegatesGetter*
  GetSyncedWindowDelegatesGetter() = 0;

  // Returns a LocalSessionEventRouter instance that is customized for the
  // embedder's context.
  virtual std::unique_ptr<browser_sync::LocalSessionEventRouter>
  GetLocalSessionEventRouter() = 0;

  // TODO(zea): add getters for the history and favicon services for the favicon
  // cache to consume once it's componentized.

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncSessionsClient);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SYNC_SESSIONS_CLIENT_H_
