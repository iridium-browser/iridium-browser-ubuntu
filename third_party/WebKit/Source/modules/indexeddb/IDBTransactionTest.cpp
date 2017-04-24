/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/IDBTransaction.h"

#include <memory>

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/MockWebIDBDatabase.h"
#include "platform/SharedBuffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

void deactivateNewTransactions(v8::Isolate* isolate) {
  V8PerIsolateData::from(isolate)->runEndOfScopeTasks();
}

class FakeIDBDatabaseCallbacks final : public IDBDatabaseCallbacks {
 public:
  static FakeIDBDatabaseCallbacks* create() {
    return new FakeIDBDatabaseCallbacks();
  }
  void onVersionChange(int64_t oldVersion, int64_t newVersion) override {}
  void onForcedClose() override {}
  void onAbort(int64_t transactionId, DOMException* error) override {}
  void onComplete(int64_t transactionId) override {}

 private:
  FakeIDBDatabaseCallbacks() {}
};

TEST(IDBTransactionTest, EnsureLifetime) {
  V8TestingScope scope;
  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::create();
  EXPECT_CALL(*backend, close()).Times(1);
  Persistent<IDBDatabase> db =
      IDBDatabase::create(scope.getExecutionContext(), std::move(backend),
                          FakeIDBDatabaseCallbacks::create(), scope.isolate());

  const int64_t transactionId = 1234;
  HashSet<String> transactionScope = HashSet<String>();
  transactionScope.insert("test-store-name");
  Persistent<IDBTransaction> transaction =
      IDBTransaction::createNonVersionChange(
          scope.getScriptState(), transactionId, transactionScope,
          WebIDBTransactionModeReadOnly, db.get());
  PersistentHeapHashSet<WeakMember<IDBTransaction>> set;
  set.insert(transaction);

  ThreadState::current()->collectAllGarbage();
  EXPECT_EQ(1u, set.size());

  Persistent<IDBRequest> request = IDBRequest::create(
      scope.getScriptState(), IDBAny::createUndefined(), transaction.get());
  deactivateNewTransactions(scope.isolate());

  ThreadState::current()->collectAllGarbage();
  EXPECT_EQ(1u, set.size());

  // This will generate an abort() call to the back end which is dropped by the
  // fake proxy, so an explicit onAbort call is made.
  scope.getExecutionContext()->notifyContextDestroyed();
  transaction->onAbort(DOMException::create(AbortError, "Aborted"));
  transaction.clear();

  ThreadState::current()->collectAllGarbage();
  EXPECT_EQ(0u, set.size());
}

TEST(IDBTransactionTest, TransactionFinish) {
  V8TestingScope scope;
  const int64_t transactionId = 1234;

  std::unique_ptr<MockWebIDBDatabase> backend = MockWebIDBDatabase::create();
  EXPECT_CALL(*backend, commit(transactionId)).Times(1);
  EXPECT_CALL(*backend, close()).Times(1);
  Persistent<IDBDatabase> db =
      IDBDatabase::create(scope.getExecutionContext(), std::move(backend),
                          FakeIDBDatabaseCallbacks::create(), scope.isolate());

  HashSet<String> transactionScope = HashSet<String>();
  transactionScope.insert("test-store-name");
  Persistent<IDBTransaction> transaction =
      IDBTransaction::createNonVersionChange(
          scope.getScriptState(), transactionId, transactionScope,
          WebIDBTransactionModeReadOnly, db.get());
  PersistentHeapHashSet<WeakMember<IDBTransaction>> set;
  set.insert(transaction);

  ThreadState::current()->collectAllGarbage();
  EXPECT_EQ(1u, set.size());

  deactivateNewTransactions(scope.isolate());

  ThreadState::current()->collectAllGarbage();
  EXPECT_EQ(1u, set.size());

  transaction.clear();

  ThreadState::current()->collectAllGarbage();
  EXPECT_EQ(1u, set.size());

  // Stop the context, so events don't get queued (which would keep the
  // transaction alive).
  scope.getExecutionContext()->notifyContextDestroyed();

  // Fire an abort to make sure this doesn't free the transaction during use.
  // The test will not fail if it is, but ASAN would notice the error.
  db->onAbort(transactionId, DOMException::create(AbortError, "Aborted"));

  // onAbort() should have cleared the transaction's reference to the database.
  ThreadState::current()->collectAllGarbage();
  EXPECT_EQ(0u, set.size());
}

}  // namespace
}  // namespace blink
