// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/precache/core/precache_url_table.h"

class GURL;

namespace base {
class FilePath;
class Time;
}

namespace sql {
class Connection;
}

namespace precache {

// Class that tracks information related to precaching. This class can be
// constructed or destroyed on any threads, but all other methods must be called
// on the same thread (e.g. the DB thread).
class PrecacheDatabase : public base::RefCountedThreadSafe<PrecacheDatabase> {
 public:
  // A PrecacheDatabase can be constructed on any thread.
  PrecacheDatabase();

  // Initializes the precache database, using the specified database file path.
  // Init must be called before any other methods.
  bool Init(const base::FilePath& db_path);

  // Deletes precache history from the precache URL table that is more than 60
  // days older than |current_time|.
  void DeleteExpiredPrecacheHistory(const base::Time& current_time);

  // Delete all history entries from the database.
  void ClearHistory();

  // Report precache-related metrics in response to a URL being fetched, where
  // the fetch was motivated by precaching.
  void RecordURLPrefetch(const GURL& url,
                         const base::TimeDelta& latency,
                         const base::Time& fetch_time,
                         int64 size,
                         bool was_cached);

  // Report precache-related metrics in response to a URL being fetched, where
  // the fetch was not motivated by precaching. |is_connection_cellular|
  // indicates whether the current network connection is a cellular network.
  void RecordURLNonPrefetch(const GURL& url,
                            const base::TimeDelta& latency,
                            const base::Time& fetch_time,
                            int64 size,
                            bool was_cached,
                            int host_rank,
                            bool is_connection_cellular);

 private:
  friend class base::RefCountedThreadSafe<PrecacheDatabase>;
  friend class PrecacheDatabaseTest;

  ~PrecacheDatabase();

  bool IsDatabaseAccessible() const;

  // Flushes any buffered write operations. |buffered_writes_| will be empty
  // after calling this function. To maximize performance, all the buffered
  // writes are run in a single database transaction.
  void Flush();

  // Same as Flush(), but also updates the flag |is_flush_posted_| to indicate
  // that a flush is no longer posted.
  void PostedFlush();

  // Post a call to PostedFlush() on the current thread's MessageLoop, if
  // |buffered_writes_| is non-empty and there isn't already a flush call
  // posted.
  void MaybePostFlush();

  scoped_ptr<sql::Connection> db_;

  // Table that keeps track of URLs that are in the cache because of precaching,
  // and wouldn't be in the cache otherwise. If |buffered_writes_| is non-empty,
  // then this table will not be up to date until the next call to Flush().
  PrecacheURLTable precache_url_table_;

  // A vector of write operations to be run on the database.
  std::vector<base::Closure> buffered_writes_;

  // Set of URLs that have been modified in |buffered_writes_|. It's a hash set
  // of strings, and not GURLs, because there is no hash function on GURL.
  base::hash_set<std::string> buffered_urls_;

  // Flag indicating whether or not a call to Flush() has been posted to run in
  // the future.
  bool is_flush_posted_;

  // ThreadChecker used to ensure that all methods other than the constructor
  // or destructor are called on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheDatabase);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_DATABASE_H_
