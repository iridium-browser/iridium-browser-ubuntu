// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerScriptCachedMetadataHandler.h"

#include "core/workers/WorkerGlobalScope.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/loader/fetch/CachedMetadata.h"

namespace blink {

ServiceWorkerScriptCachedMetadataHandler::
    ServiceWorkerScriptCachedMetadataHandler(
        WorkerGlobalScope* workerGlobalScope,
        const KURL& scriptURL,
        const Vector<char>* metaData)
    : m_workerGlobalScope(workerGlobalScope), m_scriptURL(scriptURL) {
  if (metaData)
    m_cachedMetadata = CachedMetadata::createFromSerializedData(
        metaData->data(), metaData->size());
}

ServiceWorkerScriptCachedMetadataHandler::
    ~ServiceWorkerScriptCachedMetadataHandler() {}

DEFINE_TRACE(ServiceWorkerScriptCachedMetadataHandler) {
  visitor->trace(m_workerGlobalScope);
  CachedMetadataHandler::trace(visitor);
}

void ServiceWorkerScriptCachedMetadataHandler::setCachedMetadata(
    uint32_t dataTypeID,
    const char* data,
    size_t size,
    CacheType type) {
  if (type != SendToPlatform)
    return;
  m_cachedMetadata = CachedMetadata::create(dataTypeID, data, size);
  const Vector<char>& serializedData = m_cachedMetadata->serializedData();
  ServiceWorkerGlobalScopeClient::from(m_workerGlobalScope)
      ->setCachedMetadata(m_scriptURL, serializedData.data(),
                          serializedData.size());
}

void ServiceWorkerScriptCachedMetadataHandler::clearCachedMetadata(
    CacheType type) {
  if (type != SendToPlatform)
    return;
  m_cachedMetadata = nullptr;
  ServiceWorkerGlobalScopeClient::from(m_workerGlobalScope)
      ->clearCachedMetadata(m_scriptURL);
}

PassRefPtr<CachedMetadata>
ServiceWorkerScriptCachedMetadataHandler::cachedMetadata(
    uint32_t dataTypeID) const {
  if (!m_cachedMetadata || m_cachedMetadata->dataTypeID() != dataTypeID)
    return nullptr;
  return m_cachedMetadata.get();
}

String ServiceWorkerScriptCachedMetadataHandler::encoding() const {
  return emptyString;
}

}  // namespace blink
