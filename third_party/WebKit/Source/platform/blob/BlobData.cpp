/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "platform/blob/BlobData.h"

#include "platform/UUID.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/text/LineEnding.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/TextEncoding.h"
#include <memory>

namespace blink {

namespace {

// All consecutive items that are accumulate to < this number will have the
// data appended to the same item.
static const size_t kMaxConsolidatedItemSizeInBytes = 15 * 1024;

// http://dev.w3.org/2006/webapi/FileAPI/#constructorBlob
bool isValidBlobType(const String& type) {
  for (unsigned i = 0; i < type.length(); ++i) {
    UChar c = type[i];
    if (c < 0x20 || c > 0x7E)
      return false;
  }
  return true;
}

}  // namespace

const long long BlobDataItem::toEndOfFile = -1;

RawData::RawData() {}

void RawData::detachFromCurrentThread() {}

void BlobDataItem::detachFromCurrentThread() {
  data->detachFromCurrentThread();
  path = path.isolatedCopy();
  fileSystemURL = fileSystemURL.copy();
}

std::unique_ptr<BlobData> BlobData::create() {
  return WTF::wrapUnique(
      new BlobData(FileCompositionStatus::NO_UNKNOWN_SIZE_FILES));
}

std::unique_ptr<BlobData> BlobData::createForFileWithUnknownSize(
    const String& path) {
  std::unique_ptr<BlobData> data = WTF::wrapUnique(
      new BlobData(FileCompositionStatus::SINGLE_UNKNOWN_SIZE_FILE));
  data->m_items.push_back(BlobDataItem(path));
  return data;
}

void BlobData::detachFromCurrentThread() {
  m_contentType = m_contentType.isolatedCopy();
  for (size_t i = 0; i < m_items.size(); ++i)
    m_items.at(i).detachFromCurrentThread();
}

void BlobData::setContentType(const String& contentType) {
  if (isValidBlobType(contentType))
    m_contentType = contentType;
  else
    m_contentType = "";
}

void BlobData::appendData(PassRefPtr<RawData> data,
                          long long offset,
                          long long length) {
  CHECK_EQ(m_fileComposition, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  m_items.push_back(BlobDataItem(std::move(data), offset, length));
}

void BlobData::appendFile(const String& path,
                          long long offset,
                          long long length,
                          double expectedModificationTime) {
  CHECK_EQ(m_fileComposition, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  m_items.push_back(
      BlobDataItem(path, offset, length, expectedModificationTime));
}

void BlobData::appendBlob(PassRefPtr<BlobDataHandle> dataHandle,
                          long long offset,
                          long long length) {
  CHECK_EQ(m_fileComposition, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  m_items.push_back(BlobDataItem(std::move(dataHandle), offset, length));
}

void BlobData::appendFileSystemURL(const KURL& url,
                                   long long offset,
                                   long long length,
                                   double expectedModificationTime) {
  CHECK_EQ(m_fileComposition, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  m_items.push_back(
      BlobDataItem(url, offset, length, expectedModificationTime));
}

void BlobData::appendText(const String& text,
                          bool doNormalizeLineEndingsToNative) {
  CHECK_EQ(m_fileComposition, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  CString utf8Text = UTF8Encoding().encode(text, WTF::EntitiesForUnencodables);
  RefPtr<RawData> data = nullptr;
  Vector<char>* buffer;
  if (canConsolidateData(text.length())) {
    buffer = m_items.back().data->mutableData();
  } else {
    data = RawData::create();
    buffer = data->mutableData();
  }

  if (doNormalizeLineEndingsToNative) {
    normalizeLineEndingsToNative(utf8Text, *buffer);
  } else {
    buffer->append(utf8Text.data(), utf8Text.length());
  }

  if (data)
    m_items.push_back(BlobDataItem(std::move(data)));
}

void BlobData::appendBytes(const void* bytes, size_t length) {
  CHECK_EQ(m_fileComposition, FileCompositionStatus::NO_UNKNOWN_SIZE_FILES)
      << "Blobs with a unknown-size file cannot have other items.";
  if (canConsolidateData(length)) {
    m_items.back().data->mutableData()->append(static_cast<const char*>(bytes),
                                               length);
    return;
  }
  RefPtr<RawData> data = RawData::create();
  Vector<char>* buffer = data->mutableData();
  buffer->append(static_cast<const char*>(bytes), length);
  m_items.push_back(BlobDataItem(std::move(data)));
}

long long BlobData::length() const {
  long long length = 0;

  for (Vector<BlobDataItem>::const_iterator it = m_items.begin();
       it != m_items.end(); ++it) {
    const BlobDataItem& item = *it;
    if (item.length != BlobDataItem::toEndOfFile) {
      ASSERT(item.length >= 0);
      length += item.length;
      continue;
    }

    switch (item.type) {
      case BlobDataItem::Data:
        length += item.data->length();
        break;
      case BlobDataItem::File:
      case BlobDataItem::Blob:
      case BlobDataItem::FileSystemURL:
        return BlobDataItem::toEndOfFile;
    }
  }
  return length;
}

bool BlobData::canConsolidateData(size_t length) {
  if (m_items.isEmpty())
    return false;
  BlobDataItem& lastItem = m_items.back();
  if (lastItem.type != BlobDataItem::Data)
    return false;
  if (lastItem.data->length() + length > kMaxConsolidatedItemSizeInBytes)
    return false;
  return true;
}

BlobDataHandle::BlobDataHandle()
    : m_uuid(createCanonicalUUIDString()), m_size(0) {
  BlobRegistry::registerBlobData(m_uuid, BlobData::create());
}

BlobDataHandle::BlobDataHandle(std::unique_ptr<BlobData> data, long long size)
    : m_uuid(createCanonicalUUIDString()),
      m_type(data->contentType().isolatedCopy()),
      m_size(size) {
  BlobRegistry::registerBlobData(m_uuid, std::move(data));
}

BlobDataHandle::BlobDataHandle(const String& uuid,
                               const String& type,
                               long long size)
    : m_uuid(uuid.isolatedCopy()),
      m_type(isValidBlobType(type) ? type.isolatedCopy() : ""),
      m_size(size) {
  BlobRegistry::addBlobDataRef(m_uuid);
}

BlobDataHandle::~BlobDataHandle() {
  BlobRegistry::removeBlobDataRef(m_uuid);
}

}  // namespace blink
