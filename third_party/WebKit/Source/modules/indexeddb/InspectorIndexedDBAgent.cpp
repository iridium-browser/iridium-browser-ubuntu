/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "modules/indexeddb/InspectorIndexedDBAgent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/events/EventListener.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/V8InspectorString.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/GlobalIndexedDB.h"
#include "modules/indexeddb/IDBCursor.h"
#include "modules/indexeddb/IDBCursorWithValue.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBFactory.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/Vector.h"

using blink::protocol::Array;
using blink::protocol::IndexedDB::DatabaseWithObjectStores;
using blink::protocol::IndexedDB::DataEntry;
using blink::protocol::IndexedDB::Key;
using blink::protocol::IndexedDB::KeyPath;
using blink::protocol::IndexedDB::KeyRange;
using blink::protocol::IndexedDB::ObjectStore;
using blink::protocol::IndexedDB::ObjectStoreIndex;

typedef blink::protocol::IndexedDB::Backend::RequestDatabaseNamesCallback
    RequestDatabaseNamesCallback;
typedef blink::protocol::IndexedDB::Backend::RequestDatabaseCallback
    RequestDatabaseCallback;
typedef blink::protocol::IndexedDB::Backend::RequestDataCallback
    RequestDataCallback;
typedef blink::protocol::IndexedDB::Backend::ClearObjectStoreCallback
    ClearObjectStoreCallback;
typedef blink::protocol::IndexedDB::Backend::DeleteDatabaseCallback
    DeleteDatabaseCallback;

namespace blink {

namespace IndexedDBAgentState {
static const char indexedDBAgentEnabled[] = "indexedDBAgentEnabled";
};

namespace {

static const char kIndexedDBObjectGroup[] = "indexeddb";
static const char kNoDocumentError[] = "No document for given frame found";

class GetDatabaseNamesCallback final : public EventListener {
  WTF_MAKE_NONCOPYABLE(GetDatabaseNamesCallback);

 public:
  static GetDatabaseNamesCallback* create(
      std::unique_ptr<RequestDatabaseNamesCallback> requestCallback,
      const String& securityOrigin) {
    return new GetDatabaseNamesCallback(std::move(requestCallback),
                                        securityOrigin);
  }

  ~GetDatabaseNamesCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    if (event->type() != EventTypeNames::success) {
      m_requestCallback->sendFailure(Response::Error("Unexpected event type."));
      return;
    }

    IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
    IDBAny* requestResult = idbRequest->resultAsAny();
    if (requestResult->getType() != IDBAny::DOMStringListType) {
      m_requestCallback->sendFailure(
          Response::Error("Unexpected result type."));
      return;
    }

    DOMStringList* databaseNamesList = requestResult->domStringList();
    std::unique_ptr<protocol::Array<String>> databaseNames =
        protocol::Array<String>::create();
    for (size_t i = 0; i < databaseNamesList->length(); ++i)
      databaseNames->addItem(databaseNamesList->item(i));
    m_requestCallback->sendSuccess(std::move(databaseNames));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { EventListener::trace(visitor); }

 private:
  GetDatabaseNamesCallback(
      std::unique_ptr<RequestDatabaseNamesCallback> requestCallback,
      const String& securityOrigin)
      : EventListener(EventListener::CPPEventListenerType),
        m_requestCallback(std::move(requestCallback)),
        m_securityOrigin(securityOrigin) {}
  std::unique_ptr<RequestDatabaseNamesCallback> m_requestCallback;
  String m_securityOrigin;
};

class DeleteCallback final : public EventListener {
  WTF_MAKE_NONCOPYABLE(DeleteCallback);

 public:
  static DeleteCallback* create(
      std::unique_ptr<DeleteDatabaseCallback> requestCallback,
      const String& securityOrigin) {
    return new DeleteCallback(std::move(requestCallback), securityOrigin);
  }

  ~DeleteCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    if (event->type() != EventTypeNames::success) {
      m_requestCallback->sendFailure(
          Response::Error("Failed to delete database."));
      return;
    }
    m_requestCallback->sendSuccess();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { EventListener::trace(visitor); }

 private:
  DeleteCallback(std::unique_ptr<DeleteDatabaseCallback> requestCallback,
                 const String& securityOrigin)
      : EventListener(EventListener::CPPEventListenerType),
        m_requestCallback(std::move(requestCallback)),
        m_securityOrigin(securityOrigin) {}
  std::unique_ptr<DeleteDatabaseCallback> m_requestCallback;
  String m_securityOrigin;
};

template <typename RequestCallback>
class OpenDatabaseCallback;
template <typename RequestCallback>
class UpgradeDatabaseCallback;

template <typename RequestCallback>
class ExecutableWithDatabase
    : public RefCounted<ExecutableWithDatabase<RequestCallback>> {
 public:
  ExecutableWithDatabase(ScriptState* scriptState)
      : m_scriptState(scriptState) {}
  virtual ~ExecutableWithDatabase() {}
  void start(IDBFactory* idbFactory,
             SecurityOrigin*,
             const String& databaseName) {
    OpenDatabaseCallback<RequestCallback>* openCallback =
        OpenDatabaseCallback<RequestCallback>::create(this);
    UpgradeDatabaseCallback<RequestCallback>* upgradeCallback =
        UpgradeDatabaseCallback<RequestCallback>::create(this);
    DummyExceptionStateForTesting exceptionState;
    IDBOpenDBRequest* idbOpenDBRequest =
        idbFactory->open(getScriptState(), databaseName, exceptionState);
    if (exceptionState.hadException()) {
      getRequestCallback()->sendFailure(
          Response::Error("Could not open database."));
      return;
    }
    idbOpenDBRequest->addEventListener(EventTypeNames::upgradeneeded,
                                       upgradeCallback, false);
    idbOpenDBRequest->addEventListener(EventTypeNames::success, openCallback,
                                       false);
  }
  virtual void execute(IDBDatabase*) = 0;
  virtual RequestCallback* getRequestCallback() = 0;
  ExecutionContext* context() const {
    return m_scriptState->getExecutionContext();
  }
  ScriptState* getScriptState() const { return m_scriptState.get(); }

 private:
  RefPtr<ScriptState> m_scriptState;
};

template <typename RequestCallback>
class OpenDatabaseCallback final : public EventListener {
 public:
  static OpenDatabaseCallback* create(
      ExecutableWithDatabase<RequestCallback>* executableWithDatabase) {
    return new OpenDatabaseCallback(executableWithDatabase);
  }

  ~OpenDatabaseCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext* context, Event* event) override {
    if (event->type() != EventTypeNames::success) {
      m_executableWithDatabase->getRequestCallback()->sendFailure(
          Response::Error("Unexpected event type."));
      return;
    }

    IDBOpenDBRequest* idbOpenDBRequest =
        static_cast<IDBOpenDBRequest*>(event->target());
    IDBAny* requestResult = idbOpenDBRequest->resultAsAny();
    if (requestResult->getType() != IDBAny::IDBDatabaseType) {
      m_executableWithDatabase->getRequestCallback()->sendFailure(
          Response::Error("Unexpected result type."));
      return;
    }

    IDBDatabase* idbDatabase = requestResult->idbDatabase();
    m_executableWithDatabase->execute(idbDatabase);
    V8PerIsolateData::from(
        m_executableWithDatabase->getScriptState()->isolate())
        ->runEndOfScopeTasks();
    idbDatabase->close();
  }

 private:
  OpenDatabaseCallback(
      ExecutableWithDatabase<RequestCallback>* executableWithDatabase)
      : EventListener(EventListener::CPPEventListenerType),
        m_executableWithDatabase(executableWithDatabase) {}
  RefPtr<ExecutableWithDatabase<RequestCallback>> m_executableWithDatabase;
};

template <typename RequestCallback>
class UpgradeDatabaseCallback final : public EventListener {
 public:
  static UpgradeDatabaseCallback* create(
      ExecutableWithDatabase<RequestCallback>* executableWithDatabase) {
    return new UpgradeDatabaseCallback(executableWithDatabase);
  }

  ~UpgradeDatabaseCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext* context, Event* event) override {
    if (event->type() != EventTypeNames::upgradeneeded) {
      m_executableWithDatabase->getRequestCallback()->sendFailure(
          Response::Error("Unexpected event type."));
      return;
    }

    // If an "upgradeneeded" event comes through then the database that
    // had previously been enumerated was deleted. We don't want to
    // implicitly re-create it here, so abort the transaction.
    IDBOpenDBRequest* idbOpenDBRequest =
        static_cast<IDBOpenDBRequest*>(event->target());
    NonThrowableExceptionState exceptionState;
    idbOpenDBRequest->transaction()->abort(exceptionState);
    m_executableWithDatabase->getRequestCallback()->sendFailure(
        Response::Error("Aborted upgrade."));
  }

 private:
  UpgradeDatabaseCallback(
      ExecutableWithDatabase<RequestCallback>* executableWithDatabase)
      : EventListener(EventListener::CPPEventListenerType),
        m_executableWithDatabase(executableWithDatabase) {}
  RefPtr<ExecutableWithDatabase<RequestCallback>> m_executableWithDatabase;
};

static IDBTransaction* transactionForDatabase(
    ScriptState* scriptState,
    IDBDatabase* idbDatabase,
    const String& objectStoreName,
    const String& mode = IndexedDBNames::readonly) {
  DummyExceptionStateForTesting exceptionState;
  StringOrStringSequenceOrDOMStringList scope;
  scope.setString(objectStoreName);
  IDBTransaction* idbTransaction =
      idbDatabase->transaction(scriptState, scope, mode, exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  return idbTransaction;
}

static IDBObjectStore* objectStoreForTransaction(
    IDBTransaction* idbTransaction,
    const String& objectStoreName) {
  DummyExceptionStateForTesting exceptionState;
  IDBObjectStore* idbObjectStore =
      idbTransaction->objectStore(objectStoreName, exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  return idbObjectStore;
}

static IDBIndex* indexForObjectStore(IDBObjectStore* idbObjectStore,
                                     const String& indexName) {
  DummyExceptionStateForTesting exceptionState;
  IDBIndex* idbIndex = idbObjectStore->index(indexName, exceptionState);
  if (exceptionState.hadException())
    return nullptr;
  return idbIndex;
}

static std::unique_ptr<KeyPath> keyPathFromIDBKeyPath(
    const IDBKeyPath& idbKeyPath) {
  std::unique_ptr<KeyPath> keyPath;
  switch (idbKeyPath.getType()) {
    case IDBKeyPath::NullType:
      keyPath = KeyPath::create().setType(KeyPath::TypeEnum::Null).build();
      break;
    case IDBKeyPath::StringType:
      keyPath = KeyPath::create()
                    .setType(KeyPath::TypeEnum::String)
                    .setString(idbKeyPath.string())
                    .build();
      break;
    case IDBKeyPath::ArrayType: {
      keyPath = KeyPath::create().setType(KeyPath::TypeEnum::Array).build();
      std::unique_ptr<protocol::Array<String>> array =
          protocol::Array<String>::create();
      const Vector<String>& stringArray = idbKeyPath.array();
      for (size_t i = 0; i < stringArray.size(); ++i)
        array->addItem(stringArray[i]);
      keyPath->setArray(std::move(array));
      break;
    }
    default:
      ASSERT_NOT_REACHED();
  }

  return keyPath;
}

class DatabaseLoader final
    : public ExecutableWithDatabase<RequestDatabaseCallback> {
 public:
  static PassRefPtr<DatabaseLoader> create(
      ScriptState* scriptState,
      std::unique_ptr<RequestDatabaseCallback> requestCallback) {
    return adoptRef(
        new DatabaseLoader(scriptState, std::move(requestCallback)));
  }

  ~DatabaseLoader() override {}

  void execute(IDBDatabase* idbDatabase) override {
    const IDBDatabaseMetadata databaseMetadata = idbDatabase->metadata();

    std::unique_ptr<protocol::Array<protocol::IndexedDB::ObjectStore>>
        objectStores =
            protocol::Array<protocol::IndexedDB::ObjectStore>::create();

    for (const auto& storeMapEntry : databaseMetadata.objectStores) {
      const IDBObjectStoreMetadata& objectStoreMetadata = *storeMapEntry.value;

      std::unique_ptr<protocol::Array<protocol::IndexedDB::ObjectStoreIndex>>
          indexes =
              protocol::Array<protocol::IndexedDB::ObjectStoreIndex>::create();

      for (const auto& metadataMapEntry : objectStoreMetadata.indexes) {
        const IDBIndexMetadata& indexMetadata = *metadataMapEntry.value;

        std::unique_ptr<ObjectStoreIndex> objectStoreIndex =
            ObjectStoreIndex::create()
                .setName(indexMetadata.name)
                .setKeyPath(keyPathFromIDBKeyPath(indexMetadata.keyPath))
                .setUnique(indexMetadata.unique)
                .setMultiEntry(indexMetadata.multiEntry)
                .build();
        indexes->addItem(std::move(objectStoreIndex));
      }

      std::unique_ptr<ObjectStore> objectStore =
          ObjectStore::create()
              .setName(objectStoreMetadata.name)
              .setKeyPath(keyPathFromIDBKeyPath(objectStoreMetadata.keyPath))
              .setAutoIncrement(objectStoreMetadata.autoIncrement)
              .setIndexes(std::move(indexes))
              .build();
      objectStores->addItem(std::move(objectStore));
    }
    std::unique_ptr<DatabaseWithObjectStores> result =
        DatabaseWithObjectStores::create()
            .setName(idbDatabase->name())
            .setVersion(idbDatabase->version())
            .setObjectStores(std::move(objectStores))
            .build();

    m_requestCallback->sendSuccess(std::move(result));
  }

  RequestDatabaseCallback* getRequestCallback() override {
    return m_requestCallback.get();
  }

 private:
  DatabaseLoader(ScriptState* scriptState,
                 std::unique_ptr<RequestDatabaseCallback> requestCallback)
      : ExecutableWithDatabase(scriptState),
        m_requestCallback(std::move(requestCallback)) {}
  std::unique_ptr<RequestDatabaseCallback> m_requestCallback;
};

static IDBKey* idbKeyFromInspectorObject(protocol::IndexedDB::Key* key) {
  IDBKey* idbKey;

  if (!key)
    return nullptr;
  String type = key->getType();

  DEFINE_STATIC_LOCAL(String, s_number, ("number"));
  DEFINE_STATIC_LOCAL(String, s_string, ("string"));
  DEFINE_STATIC_LOCAL(String, s_date, ("date"));
  DEFINE_STATIC_LOCAL(String, s_array, ("array"));

  if (type == s_number) {
    if (!key->hasNumber())
      return nullptr;
    idbKey = IDBKey::createNumber(key->getNumber(0));
  } else if (type == s_string) {
    if (!key->hasString())
      return nullptr;
    idbKey = IDBKey::createString(key->getString(String()));
  } else if (type == s_date) {
    if (!key->hasDate())
      return nullptr;
    idbKey = IDBKey::createDate(key->getDate(0));
  } else if (type == s_array) {
    IDBKey::KeyArray keyArray;
    auto array = key->getArray(nullptr);
    for (size_t i = 0; array && i < array->length(); ++i)
      keyArray.push_back(idbKeyFromInspectorObject(array->get(i)));
    idbKey = IDBKey::createArray(keyArray);
  } else {
    return nullptr;
  }

  return idbKey;
}

static IDBKeyRange* idbKeyRangeFromKeyRange(
    protocol::IndexedDB::KeyRange* keyRange) {
  IDBKey* idbLower = idbKeyFromInspectorObject(keyRange->getLower(nullptr));
  if (keyRange->hasLower() && !idbLower)
    return nullptr;

  IDBKey* idbUpper = idbKeyFromInspectorObject(keyRange->getUpper(nullptr));
  if (keyRange->hasUpper() && !idbUpper)
    return nullptr;

  IDBKeyRange::LowerBoundType lowerBoundType =
      keyRange->getLowerOpen() ? IDBKeyRange::LowerBoundOpen
                               : IDBKeyRange::LowerBoundClosed;
  IDBKeyRange::UpperBoundType upperBoundType =
      keyRange->getUpperOpen() ? IDBKeyRange::UpperBoundOpen
                               : IDBKeyRange::UpperBoundClosed;
  return IDBKeyRange::create(idbLower, idbUpper, lowerBoundType,
                             upperBoundType);
}

class DataLoader;

class OpenCursorCallback final : public EventListener {
 public:
  static OpenCursorCallback* create(
      v8_inspector::V8InspectorSession* v8Session,
      ScriptState* scriptState,
      std::unique_ptr<RequestDataCallback> requestCallback,
      int skipCount,
      unsigned pageSize) {
    return new OpenCursorCallback(v8Session, scriptState,
                                  std::move(requestCallback), skipCount,
                                  pageSize);
  }

  ~OpenCursorCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    if (event->type() != EventTypeNames::success) {
      m_requestCallback->sendFailure(Response::Error("Unexpected event type."));
      return;
    }

    IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
    IDBAny* requestResult = idbRequest->resultAsAny();
    if (requestResult->getType() == IDBAny::IDBValueType) {
      end(false);
      return;
    }
    if (requestResult->getType() != IDBAny::IDBCursorWithValueType) {
      m_requestCallback->sendFailure(
          Response::Error("Unexpected result type."));
      return;
    }

    IDBCursorWithValue* idbCursor = requestResult->idbCursorWithValue();

    if (m_skipCount) {
      DummyExceptionStateForTesting exceptionState;
      idbCursor->advance(m_skipCount, exceptionState);
      if (exceptionState.hadException()) {
        m_requestCallback->sendFailure(
            Response::Error("Could not advance cursor."));
      }
      m_skipCount = 0;
      return;
    }

    if (m_result->length() == m_pageSize) {
      end(true);
      return;
    }

    // Continue cursor before making injected script calls, otherwise
    // transaction might be finished.
    DummyExceptionStateForTesting exceptionState;
    idbCursor->continueFunction(nullptr, nullptr, exceptionState);
    if (exceptionState.hadException()) {
      m_requestCallback->sendFailure(
          Response::Error("Could not continue cursor."));
      return;
    }

    Document* document = toDocument(m_scriptState->getExecutionContext());
    if (!document)
      return;
    ScriptState* scriptState = m_scriptState.get();
    ScriptState::Scope scope(scriptState);
    v8::Local<v8::Context> context = scriptState->context();
    v8_inspector::StringView objectGroup =
        toV8InspectorStringView(kIndexedDBObjectGroup);
    std::unique_ptr<DataEntry> dataEntry =
        DataEntry::create()
            .setKey(m_v8Session->wrapObject(
                context, idbCursor->key(scriptState).v8Value(), objectGroup))
            .setPrimaryKey(m_v8Session->wrapObject(
                context, idbCursor->primaryKey(scriptState).v8Value(),
                objectGroup))
            .setValue(m_v8Session->wrapObject(
                context, idbCursor->value(scriptState).v8Value(), objectGroup))
            .build();
    m_result->addItem(std::move(dataEntry));
  }

  void end(bool hasMore) {
    m_requestCallback->sendSuccess(std::move(m_result), hasMore);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { EventListener::trace(visitor); }

 private:
  OpenCursorCallback(v8_inspector::V8InspectorSession* v8Session,
                     ScriptState* scriptState,
                     std::unique_ptr<RequestDataCallback> requestCallback,
                     int skipCount,
                     unsigned pageSize)
      : EventListener(EventListener::CPPEventListenerType),
        m_v8Session(v8Session),
        m_scriptState(scriptState),
        m_requestCallback(std::move(requestCallback)),
        m_skipCount(skipCount),
        m_pageSize(pageSize) {
    m_result = Array<DataEntry>::create();
  }

  v8_inspector::V8InspectorSession* m_v8Session;
  RefPtr<ScriptState> m_scriptState;
  std::unique_ptr<RequestDataCallback> m_requestCallback;
  int m_skipCount;
  unsigned m_pageSize;
  std::unique_ptr<Array<DataEntry>> m_result;
};

class DataLoader final : public ExecutableWithDatabase<RequestDataCallback> {
 public:
  static PassRefPtr<DataLoader> create(
      v8_inspector::V8InspectorSession* v8Session,
      ScriptState* scriptState,
      std::unique_ptr<RequestDataCallback> requestCallback,
      const String& objectStoreName,
      const String& indexName,
      IDBKeyRange* idbKeyRange,
      int skipCount,
      unsigned pageSize) {
    return adoptRef(new DataLoader(
        v8Session, scriptState, std::move(requestCallback), objectStoreName,
        indexName, idbKeyRange, skipCount, pageSize));
  }

  ~DataLoader() override {}

  void execute(IDBDatabase* idbDatabase) override {
    IDBTransaction* idbTransaction = transactionForDatabase(
        getScriptState(), idbDatabase, m_objectStoreName);
    if (!idbTransaction) {
      m_requestCallback->sendFailure(
          Response::Error("Could not get transaction"));
      return;
    }
    IDBObjectStore* idbObjectStore =
        objectStoreForTransaction(idbTransaction, m_objectStoreName);
    if (!idbObjectStore) {
      m_requestCallback->sendFailure(
          Response::Error("Could not get object store"));
      return;
    }

    IDBRequest* idbRequest;
    if (!m_indexName.isEmpty()) {
      IDBIndex* idbIndex = indexForObjectStore(idbObjectStore, m_indexName);
      if (!idbIndex) {
        m_requestCallback->sendFailure(Response::Error("Could not get index"));
        return;
      }

      idbRequest = idbIndex->openCursor(getScriptState(), m_idbKeyRange.get(),
                                        WebIDBCursorDirectionNext);
    } else {
      idbRequest = idbObjectStore->openCursor(
          getScriptState(), m_idbKeyRange.get(), WebIDBCursorDirectionNext);
    }
    OpenCursorCallback* openCursorCallback = OpenCursorCallback::create(
        m_v8Session, getScriptState(), std::move(m_requestCallback),
        m_skipCount, m_pageSize);
    idbRequest->addEventListener(EventTypeNames::success, openCursorCallback,
                                 false);
  }

  RequestDataCallback* getRequestCallback() override {
    return m_requestCallback.get();
  }
  DataLoader(v8_inspector::V8InspectorSession* v8Session,
             ScriptState* scriptState,
             std::unique_ptr<RequestDataCallback> requestCallback,
             const String& objectStoreName,
             const String& indexName,
             IDBKeyRange* idbKeyRange,
             int skipCount,
             unsigned pageSize)
      : ExecutableWithDatabase(scriptState),
        m_v8Session(v8Session),
        m_requestCallback(std::move(requestCallback)),
        m_objectStoreName(objectStoreName),
        m_indexName(indexName),
        m_idbKeyRange(idbKeyRange),
        m_skipCount(skipCount),
        m_pageSize(pageSize) {}

  v8_inspector::V8InspectorSession* m_v8Session;
  std::unique_ptr<RequestDataCallback> m_requestCallback;
  String m_objectStoreName;
  String m_indexName;
  Persistent<IDBKeyRange> m_idbKeyRange;
  int m_skipCount;
  unsigned m_pageSize;
};

}  // namespace

// static
InspectorIndexedDBAgent::InspectorIndexedDBAgent(
    InspectedFrames* inspectedFrames,
    v8_inspector::V8InspectorSession* v8Session)
    : m_inspectedFrames(inspectedFrames), m_v8Session(v8Session) {}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent() {}

void InspectorIndexedDBAgent::restore() {
  if (m_state->booleanProperty(IndexedDBAgentState::indexedDBAgentEnabled,
                               false)) {
    enable();
  }
}

void InspectorIndexedDBAgent::didCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == m_inspectedFrames->root()) {
    m_v8Session->releaseObjectGroup(
        toV8InspectorStringView(kIndexedDBObjectGroup));
  }
}

Response InspectorIndexedDBAgent::enable() {
  m_state->setBoolean(IndexedDBAgentState::indexedDBAgentEnabled, true);
  return Response::OK();
}

Response InspectorIndexedDBAgent::disable() {
  m_state->setBoolean(IndexedDBAgentState::indexedDBAgentEnabled, false);
  m_v8Session->releaseObjectGroup(
      toV8InspectorStringView(kIndexedDBObjectGroup));
  return Response::OK();
}

static Response assertIDBFactory(Document* document, IDBFactory*& result) {
  LocalDOMWindow* domWindow = document->domWindow();
  if (!domWindow)
    return Response::Error("No IndexedDB factory for given frame found");
  IDBFactory* idbFactory = GlobalIndexedDB::indexedDB(*domWindow);

  if (!idbFactory)
    return Response::Error("No IndexedDB factory for given frame found");
  result = idbFactory;
  return Response::OK();
}

void InspectorIndexedDBAgent::requestDatabaseNames(
    const String& securityOrigin,
    std::unique_ptr<RequestDatabaseNamesCallback> requestCallback) {
  LocalFrame* frame =
      m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
  Document* document = frame ? frame->document() : nullptr;
  if (!document) {
    requestCallback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idbFactory = nullptr;
  Response response = assertIDBFactory(document, idbFactory);
  if (!response.isSuccess()) {
    requestCallback->sendFailure(response);
    return;
  }

  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState) {
    requestCallback->sendFailure(Response::InternalError());
    return;
  }
  ScriptState::Scope scope(scriptState);
  DummyExceptionStateForTesting exceptionState;
  IDBRequest* idbRequest =
      idbFactory->getDatabaseNames(scriptState, exceptionState);
  if (exceptionState.hadException()) {
    requestCallback->sendFailure(
        Response::Error("Could not obtain database names."));
    return;
  }
  idbRequest->addEventListener(
      EventTypeNames::success,
      GetDatabaseNamesCallback::create(
          std::move(requestCallback),
          document->getSecurityOrigin()->toRawString()),
      false);
}

void InspectorIndexedDBAgent::requestDatabase(
    const String& securityOrigin,
    const String& databaseName,
    std::unique_ptr<RequestDatabaseCallback> requestCallback) {
  LocalFrame* frame =
      m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
  Document* document = frame ? frame->document() : nullptr;
  if (!document) {
    requestCallback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idbFactory = nullptr;
  Response response = assertIDBFactory(document, idbFactory);
  if (!response.isSuccess()) {
    requestCallback->sendFailure(response);
    return;
  }

  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState) {
    requestCallback->sendFailure(Response::InternalError());
    return;
  }

  ScriptState::Scope scope(scriptState);
  RefPtr<DatabaseLoader> databaseLoader =
      DatabaseLoader::create(scriptState, std::move(requestCallback));
  databaseLoader->start(idbFactory, document->getSecurityOrigin(),
                        databaseName);
}

void InspectorIndexedDBAgent::requestData(
    const String& securityOrigin,
    const String& databaseName,
    const String& objectStoreName,
    const String& indexName,
    int skipCount,
    int pageSize,
    Maybe<protocol::IndexedDB::KeyRange> keyRange,
    std::unique_ptr<RequestDataCallback> requestCallback) {
  LocalFrame* frame =
      m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
  Document* document = frame ? frame->document() : nullptr;
  if (!document) {
    requestCallback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idbFactory = nullptr;
  Response response = assertIDBFactory(document, idbFactory);
  if (!response.isSuccess()) {
    requestCallback->sendFailure(response);
    return;
  }

  IDBKeyRange* idbKeyRange = keyRange.isJust()
                                 ? idbKeyRangeFromKeyRange(keyRange.fromJust())
                                 : nullptr;
  if (keyRange.isJust() && !idbKeyRange) {
    requestCallback->sendFailure(Response::Error("Can not parse key range."));
    return;
  }

  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState) {
    requestCallback->sendFailure(Response::InternalError());
    return;
  }

  ScriptState::Scope scope(scriptState);
  RefPtr<DataLoader> dataLoader = DataLoader::create(
      m_v8Session, scriptState, std::move(requestCallback), objectStoreName,
      indexName, idbKeyRange, skipCount, pageSize);
  dataLoader->start(idbFactory, document->getSecurityOrigin(), databaseName);
}

class ClearObjectStoreListener final : public EventListener {
  WTF_MAKE_NONCOPYABLE(ClearObjectStoreListener);

 public:
  static ClearObjectStoreListener* create(
      std::unique_ptr<ClearObjectStoreCallback> requestCallback) {
    return new ClearObjectStoreListener(std::move(requestCallback));
  }

  ~ClearObjectStoreListener() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    if (event->type() != EventTypeNames::complete) {
      m_requestCallback->sendFailure(Response::Error("Unexpected event type."));
      return;
    }

    m_requestCallback->sendSuccess();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { EventListener::trace(visitor); }

 private:
  ClearObjectStoreListener(
      std::unique_ptr<ClearObjectStoreCallback> requestCallback)
      : EventListener(EventListener::CPPEventListenerType),
        m_requestCallback(std::move(requestCallback)) {}

  std::unique_ptr<ClearObjectStoreCallback> m_requestCallback;
};

class ClearObjectStore final
    : public ExecutableWithDatabase<ClearObjectStoreCallback> {
 public:
  static PassRefPtr<ClearObjectStore> create(
      ScriptState* scriptState,
      const String& objectStoreName,
      std::unique_ptr<ClearObjectStoreCallback> requestCallback) {
    return adoptRef(new ClearObjectStore(scriptState, objectStoreName,
                                         std::move(requestCallback)));
  }

  ClearObjectStore(ScriptState* scriptState,
                   const String& objectStoreName,
                   std::unique_ptr<ClearObjectStoreCallback> requestCallback)
      : ExecutableWithDatabase(scriptState),
        m_objectStoreName(objectStoreName),
        m_requestCallback(std::move(requestCallback)) {}

  void execute(IDBDatabase* idbDatabase) override {
    IDBTransaction* idbTransaction =
        transactionForDatabase(getScriptState(), idbDatabase, m_objectStoreName,
                               IndexedDBNames::readwrite);
    if (!idbTransaction) {
      m_requestCallback->sendFailure(
          Response::Error("Could not get transaction"));
      return;
    }
    IDBObjectStore* idbObjectStore =
        objectStoreForTransaction(idbTransaction, m_objectStoreName);
    if (!idbObjectStore) {
      m_requestCallback->sendFailure(
          Response::Error("Could not get object store"));
      return;
    }

    DummyExceptionStateForTesting exceptionState;
    idbObjectStore->clear(getScriptState(), exceptionState);
    ASSERT(!exceptionState.hadException());
    if (exceptionState.hadException()) {
      ExceptionCode ec = exceptionState.code();
      m_requestCallback->sendFailure(Response::Error(
          String::format("Could not clear object store '%s': %d",
                         m_objectStoreName.utf8().data(), ec)));
      return;
    }
    idbTransaction->addEventListener(
        EventTypeNames::complete,
        ClearObjectStoreListener::create(std::move(m_requestCallback)), false);
  }

  ClearObjectStoreCallback* getRequestCallback() override {
    return m_requestCallback.get();
  }

 private:
  const String m_objectStoreName;
  std::unique_ptr<ClearObjectStoreCallback> m_requestCallback;
};

void InspectorIndexedDBAgent::clearObjectStore(
    const String& securityOrigin,
    const String& databaseName,
    const String& objectStoreName,
    std::unique_ptr<ClearObjectStoreCallback> requestCallback) {
  LocalFrame* frame =
      m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
  Document* document = frame ? frame->document() : nullptr;
  if (!document) {
    requestCallback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idbFactory = nullptr;
  Response response = assertIDBFactory(document, idbFactory);
  if (!response.isSuccess()) {
    requestCallback->sendFailure(response);
    return;
  }

  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState) {
    requestCallback->sendFailure(Response::InternalError());
    return;
  }

  ScriptState::Scope scope(scriptState);
  RefPtr<ClearObjectStore> clearObjectStore = ClearObjectStore::create(
      scriptState, objectStoreName, std::move(requestCallback));
  clearObjectStore->start(idbFactory, document->getSecurityOrigin(),
                          databaseName);
}

void InspectorIndexedDBAgent::deleteDatabase(
    const String& securityOrigin,
    const String& databaseName,
    std::unique_ptr<DeleteDatabaseCallback> requestCallback) {
  LocalFrame* frame =
      m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
  Document* document = frame ? frame->document() : nullptr;
  if (!document) {
    requestCallback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idbFactory = nullptr;
  Response response = assertIDBFactory(document, idbFactory);
  if (!response.isSuccess()) {
    requestCallback->sendFailure(response);
    return;
  }

  ScriptState* scriptState = ScriptState::forMainWorld(frame);
  if (!scriptState) {
    requestCallback->sendFailure(Response::InternalError());
    return;
  }
  ScriptState::Scope scope(scriptState);
  DummyExceptionStateForTesting exceptionState;
  IDBRequest* idbRequest = idbFactory->closeConnectionsAndDeleteDatabase(
      scriptState, databaseName, exceptionState);
  if (exceptionState.hadException()) {
    requestCallback->sendFailure(Response::Error("Could not delete database."));
    return;
  }
  idbRequest->addEventListener(
      EventTypeNames::success,
      DeleteCallback::create(std::move(requestCallback),
                             document->getSecurityOrigin()->toRawString()),
      false);
}

DEFINE_TRACE(InspectorIndexedDBAgent) {
  visitor->trace(m_inspectedFrames);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
