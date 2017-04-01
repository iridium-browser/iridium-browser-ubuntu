/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "wtf/typed_arrays/ArrayBufferContents.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "wtf/Assertions.h"
#include "wtf/allocator/Partitions.h"
#include <string.h>

namespace WTF {

void ArrayBufferContents::defaultAdjustAmountOfExternalAllocatedMemoryFunction(
    int64_t diff) {
  // Do nothing by default.
}

ArrayBufferContents::AdjustAmountOfExternalAllocatedMemoryFunction
    ArrayBufferContents::s_adjustAmountOfExternalAllocatedMemoryFunction =
        defaultAdjustAmountOfExternalAllocatedMemoryFunction;

#if DCHECK_IS_ON()
ArrayBufferContents::AdjustAmountOfExternalAllocatedMemoryFunction
    ArrayBufferContents::
        s_lastUsedAdjustAmountOfExternalAllocatedMemoryFunction;
#endif

ArrayBufferContents::ArrayBufferContents()
    : m_holder(adoptRef(new DataHolder())) {}

ArrayBufferContents::ArrayBufferContents(
    unsigned numElements,
    unsigned elementByteSize,
    SharingType isShared,
    ArrayBufferContents::InitializationPolicy policy)
    : m_holder(adoptRef(new DataHolder())) {
  // Do not allow 32-bit overflow of the total size.
  unsigned totalSize = numElements * elementByteSize;
  if (numElements) {
    if (totalSize / numElements != elementByteSize) {
      return;
    }
  }

  m_holder->allocateNew(totalSize, isShared, policy);
}

ArrayBufferContents::ArrayBufferContents(void* data,
                                         unsigned sizeInBytes,
                                         SharingType isShared)
    : m_holder(adoptRef(new DataHolder())) {
  if (data) {
    m_holder->adopt(data, sizeInBytes, isShared);
  } else {
    DCHECK_EQ(sizeInBytes, 0u);
    sizeInBytes = 0;
    // Allow null data if size is 0 bytes, make sure data is valid pointer.
    // (PartitionAlloc guarantees valid pointer for size 0)
    m_holder->allocateNew(sizeInBytes, isShared, ZeroInitialize);
  }
}

ArrayBufferContents::~ArrayBufferContents() {}

void ArrayBufferContents::neuter() {
  m_holder.clear();
}

void ArrayBufferContents::transfer(ArrayBufferContents& other) {
  DCHECK(!isShared());
  DCHECK(!other.m_holder->data());
  other.m_holder = m_holder;
  neuter();
}

void ArrayBufferContents::shareWith(ArrayBufferContents& other) {
  DCHECK(isShared());
  DCHECK(!other.m_holder->data());
  other.m_holder = m_holder;
}

void ArrayBufferContents::copyTo(ArrayBufferContents& other) {
  DCHECK(!m_holder->isShared() && !other.m_holder->isShared());
  other.m_holder->copyMemoryFrom(*m_holder);
}

void ArrayBufferContents::allocateMemoryWithFlags(size_t size,
                                                  InitializationPolicy policy,
                                                  int flags,
                                                  void*& data) {
  data = PartitionAllocGenericFlags(
      WTF::Partitions::bufferPartition(), flags, size,
      WTF_HEAP_PROFILER_TYPE_NAME(ArrayBufferContents));
  if (policy == ZeroInitialize && data)
    memset(data, '\0', size);
}

void ArrayBufferContents::allocateMemory(size_t size,
                                         InitializationPolicy policy,
                                         void*& data) {
  allocateMemoryWithFlags(size, policy, 0, data);
}

void ArrayBufferContents::allocateMemoryOrNull(size_t size,
                                               InitializationPolicy policy,
                                               void*& data) {
  allocateMemoryWithFlags(size, policy, base::PartitionAllocReturnNull, data);
}

void ArrayBufferContents::freeMemory(void* data, size_t size) {
  Partitions::bufferFree(data);
}

ArrayBufferContents::DataHolder::DataHolder()
    : m_data(nullptr), m_sizeInBytes(0), m_isShared(NotShared) {}

ArrayBufferContents::DataHolder::~DataHolder() {
  ArrayBufferContents::freeMemory(m_data, m_sizeInBytes);

  adjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(m_sizeInBytes));

  m_data = nullptr;
  m_sizeInBytes = 0;
  m_isShared = NotShared;
}

void ArrayBufferContents::DataHolder::allocateNew(unsigned sizeInBytes,
                                                  SharingType isShared,
                                                  InitializationPolicy policy) {
  DCHECK(!m_data);
  DCHECK_EQ(m_sizeInBytes, 0u);

  ArrayBufferContents::allocateMemory(sizeInBytes, policy, m_data);
  if (!m_data)
    return;

  m_sizeInBytes = sizeInBytes;
  m_isShared = isShared;

  adjustAmountOfExternalAllocatedMemory(m_sizeInBytes);
}

void ArrayBufferContents::DataHolder::adopt(void* data,
                                            unsigned sizeInBytes,
                                            SharingType isShared) {
  DCHECK(!m_data);
  DCHECK_EQ(m_sizeInBytes, 0u);

  m_data = data;
  m_sizeInBytes = sizeInBytes;
  m_isShared = isShared;

  adjustAmountOfExternalAllocatedMemory(m_sizeInBytes);
}

void ArrayBufferContents::DataHolder::copyMemoryFrom(const DataHolder& source) {
  DCHECK(!m_data);
  DCHECK_EQ(m_sizeInBytes, 0u);

  ArrayBufferContents::allocateMemory(source.sizeInBytes(), DontInitialize,
                                      m_data);
  if (!m_data)
    return;

  m_sizeInBytes = source.sizeInBytes();
  memcpy(m_data, source.data(), source.sizeInBytes());

  adjustAmountOfExternalAllocatedMemory(m_sizeInBytes);
}

}  // namespace WTF
