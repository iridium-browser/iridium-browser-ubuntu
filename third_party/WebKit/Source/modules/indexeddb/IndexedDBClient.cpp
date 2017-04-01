// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IndexedDBClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"

namespace blink {

IndexedDBClient::IndexedDBClient(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

IndexedDBClient::IndexedDBClient(WorkerClients& clients)
    : Supplement<WorkerClients>(clients) {}

IndexedDBClient* IndexedDBClient::from(ExecutionContext* context) {
  if (context->isDocument())
    return static_cast<IndexedDBClient*>(Supplement<LocalFrame>::from(
        toDocument(*context).frame(), supplementName()));

  WorkerClients* clients = toWorkerGlobalScope(*context).clients();
  ASSERT(clients);
  return static_cast<IndexedDBClient*>(
      Supplement<WorkerClients>::from(clients, supplementName()));
}

const char* IndexedDBClient::supplementName() {
  return "IndexedDBClient";
}

DEFINE_TRACE(IndexedDBClient) {
  Supplement<LocalFrame>::trace(visitor);
  Supplement<WorkerClients>::trace(visitor);
}

void provideIndexedDBClientTo(LocalFrame& frame, IndexedDBClient* client) {
  Supplement<LocalFrame>::provideTo(frame, IndexedDBClient::supplementName(),
                                    client);
}

void provideIndexedDBClientToWorker(WorkerClients* clients,
                                    IndexedDBClient* client) {
  clients->provideSupplement(IndexedDBClient::supplementName(), client);
}

}  // namespace blink
