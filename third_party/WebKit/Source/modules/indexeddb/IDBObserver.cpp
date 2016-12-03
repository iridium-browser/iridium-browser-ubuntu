// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/ExceptionCode.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBObserverCallback.h"
#include "modules/indexeddb/IDBObserverChanges.h"
#include "modules/indexeddb/IDBObserverInit.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/WebIDBObserverImpl.h"

namespace blink {

IDBObserver* IDBObserver::create(IDBObserverCallback& callback, const IDBObserverInit& options)
{
    return new IDBObserver(callback, options);
}

IDBObserver::IDBObserver(IDBObserverCallback& callback, const IDBObserverInit& options)
    : m_callback(&callback)
    , m_transaction(options.transaction())
    , m_values(options.values())
    , m_noRecords(options.noRecords())
{
    // TODO(palakj): Throw an exception if unknown operation type.
    DCHECK_EQ(m_operationTypes.size(), static_cast<size_t>(WebIDBOperationTypeCount));
    m_operationTypes.reset();
    m_operationTypes[WebIDBAdd] = options.operationTypes().contains(IndexedDBNames::add);
    m_operationTypes[WebIDBPut] = options.operationTypes().contains(IndexedDBNames::put);
    m_operationTypes[WebIDBDelete] = options.operationTypes().contains(IndexedDBNames::kDelete);
    m_operationTypes[WebIDBClear] = options.operationTypes().contains(IndexedDBNames::clear);
}

void IDBObserver::observe(IDBDatabase* database, IDBTransaction* transaction, ExceptionState& exceptionState)
{
    if (transaction->isFinished() || transaction->isFinishing()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }
    if (!transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return;
    }
    if (!database->backend()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::databaseClosedErrorMessage);
        return;
    }

    std::unique_ptr<WebIDBObserverImpl> observer = WebIDBObserverImpl::create(this);
    WebIDBObserverImpl* observerPtr = observer.get();
    int32_t observerId = database->backend()->addObserver(std::move(observer), transaction->id());
    m_observerIds.add(observerId, database);
    observerPtr->setId(observerId);
}

void IDBObserver::unobserve(IDBDatabase* database, ExceptionState& exceptionState)
{
    if (!database->backend()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::databaseClosedErrorMessage);
        return;
    }

    Vector<int32_t> observerIdsToRemove;
    for (const auto& it : m_observerIds) {
        if (it.value == database)
            observerIdsToRemove.append(it.key);
    }
    m_observerIds.removeAll(observerIdsToRemove);

    if (!observerIdsToRemove.isEmpty())
        database->backend()->removeObservers(observerIdsToRemove);
}

void IDBObserver::removeObserver(int32_t id)
{
    m_observerIds.remove(id);
}

void IDBObserver::onChange(int32_t id, const WebVector<WebIDBObservation>& observations, const WebVector<int32_t>& observationIndex)
{
    auto it = m_observerIds.find(id);
    DCHECK(it != m_observerIds.end());
    m_callback->handleChanges(*IDBObserverChanges::create(it->value, observations, observationIndex), *this);
}

DEFINE_TRACE(IDBObserver)
{
    visitor->trace(m_callback);
    visitor->trace(m_observerIds);
}

} // namespace blink
