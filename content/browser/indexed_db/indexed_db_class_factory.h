// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_

#include <set>

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace leveldb {
class Iterator;
}  // namespace leveldb

namespace content {

class IndexedDBDatabase;
class IndexedDBDatabaseCallbacks;
class IndexedDBTransaction;
class LevelDBDatabase;
class LevelDBIteratorImpl;
class LevelDBTransaction;

// Use this factory to create some IndexedDB objects. Exists solely to
// facilitate tests which sometimes need to inject mock objects into the system.
class CONTENT_EXPORT IndexedDBClassFactory {
 public:
  typedef IndexedDBClassFactory* GetterCallback();

  static IndexedDBClassFactory* Get();

  static void SetIndexedDBClassFactoryGetter(GetterCallback* cb);

  virtual IndexedDBTransaction* CreateIndexedDBTransaction(
      int64 id,
      scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
      const std::set<int64>& scope,
      blink::WebIDBTransactionMode mode,
      IndexedDBDatabase* db,
      IndexedDBBackingStore::Transaction* backing_store_transaction);

  virtual LevelDBIteratorImpl* CreateIteratorImpl(
      scoped_ptr<leveldb::Iterator> iterator);
  virtual LevelDBTransaction* CreateLevelDBTransaction(LevelDBDatabase* db);

 protected:
  IndexedDBClassFactory() {}
  virtual ~IndexedDBClassFactory() {}
  friend struct base::DefaultLazyInstanceTraits<IndexedDBClassFactory>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_
