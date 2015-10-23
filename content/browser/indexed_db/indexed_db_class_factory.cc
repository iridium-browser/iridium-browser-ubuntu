// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator_impl.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"

namespace content {

static IndexedDBClassFactory::GetterCallback* s_factory_getter;
static ::base::LazyInstance<IndexedDBClassFactory>::Leaky s_factory =
    LAZY_INSTANCE_INITIALIZER;

void IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetterCallback* cb) {
  s_factory_getter = cb;
}

IndexedDBClassFactory* IndexedDBClassFactory::Get() {
  if (s_factory_getter)
    return (*s_factory_getter)();
  else
    return s_factory.Pointer();
}

IndexedDBTransaction* IndexedDBClassFactory::CreateIndexedDBTransaction(
    int64 id,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
    const std::set<int64>& scope,
    blink::WebIDBTransactionMode mode,
    IndexedDBDatabase* db,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  return new IndexedDBTransaction(id, callbacks, scope, mode, db,
                                  backing_store_transaction);
}

LevelDBTransaction* IndexedDBClassFactory::CreateLevelDBTransaction(
    LevelDBDatabase* db) {
  return new LevelDBTransaction(db);
}

content::LevelDBIteratorImpl* IndexedDBClassFactory::CreateIteratorImpl(
    scoped_ptr<leveldb::Iterator> iterator) {
  return new LevelDBIteratorImpl(iterator.Pass());
}

}  // namespace content
