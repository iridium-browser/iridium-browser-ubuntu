// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBValue.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "platform/blob/BlobData.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/modules/indexeddb/WebIDBValue.h"
#include "v8/include/v8.h"
#include "wtf/PtrUtil.h"

namespace blink {

IDBValue::IDBValue() = default;

IDBValue::IDBValue(const WebIDBValue& value, v8::Isolate* isolate)
    : IDBValue(value.data, value.webBlobInfo, value.primaryKey, value.keyPath) {
  m_isolate = isolate;
  m_externalAllocatedSize = m_data ? static_cast<int64_t>(m_data->size()) : 0l;
  if (m_externalAllocatedSize)
    m_isolate->AdjustAmountOfExternalAllocatedMemory(m_externalAllocatedSize);
}

IDBValue::IDBValue(PassRefPtr<SharedBuffer> data,
                   const WebVector<WebBlobInfo>& webBlobInfo,
                   IDBKey* primaryKey,
                   const IDBKeyPath& keyPath)
    : m_data(data),
      m_blobData(WTF::makeUnique<Vector<RefPtr<BlobDataHandle>>>()),
      m_blobInfo(WTF::wrapUnique(new Vector<WebBlobInfo>(webBlobInfo.size()))),
      m_primaryKey(primaryKey && primaryKey->isValid() ? primaryKey : nullptr),
      m_keyPath(keyPath) {
  for (size_t i = 0; i < webBlobInfo.size(); ++i) {
    const WebBlobInfo& info = (*m_blobInfo)[i] = webBlobInfo[i];
    m_blobData->push_back(
        BlobDataHandle::create(info.uuid(), info.type(), info.size()));
  }
}

IDBValue::IDBValue(const IDBValue* value,
                   IDBKey* primaryKey,
                   const IDBKeyPath& keyPath)
    : m_data(value->m_data),
      m_blobData(WTF::makeUnique<Vector<RefPtr<BlobDataHandle>>>()),
      m_blobInfo(
          WTF::wrapUnique(new Vector<WebBlobInfo>(value->m_blobInfo->size()))),
      m_primaryKey(primaryKey),
      m_keyPath(keyPath) {
  for (size_t i = 0; i < value->m_blobInfo->size(); ++i) {
    const WebBlobInfo& info = (*m_blobInfo)[i] = value->m_blobInfo->at(i);
    m_blobData->push_back(
        BlobDataHandle::create(info.uuid(), info.type(), info.size()));
  }
}

IDBValue::~IDBValue() {
  if (m_isolate)
    m_isolate->AdjustAmountOfExternalAllocatedMemory(-m_externalAllocatedSize);
}

PassRefPtr<IDBValue> IDBValue::create() {
  return adoptRef(new IDBValue());
}

PassRefPtr<IDBValue> IDBValue::create(const WebIDBValue& value,
                                      v8::Isolate* isolate) {
  return adoptRef(new IDBValue(value, isolate));
}

PassRefPtr<IDBValue> IDBValue::create(const IDBValue* value,
                                      IDBKey* primaryKey,
                                      const IDBKeyPath& keyPath) {
  return adoptRef(new IDBValue(value, primaryKey, keyPath));
}

Vector<String> IDBValue::getUUIDs() const {
  Vector<String> uuids;
  uuids.reserveCapacity(m_blobInfo->size());
  for (const auto& info : *m_blobInfo)
    uuids.push_back(info.uuid());
  return uuids;
}

RefPtr<SerializedScriptValue> IDBValue::createSerializedValue() const {
  return SerializedScriptValue::create(m_data->data(), m_data->size());
}

bool IDBValue::isNull() const {
  return !m_data.get();
}

}  // namespace blink
