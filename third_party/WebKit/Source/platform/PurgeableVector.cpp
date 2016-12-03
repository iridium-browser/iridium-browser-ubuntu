/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "platform/PurgeableVector.h"

#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator.h"
#include "platform/web_process_memory_dump.h"
#include "wtf/Assertions.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"
#include <cstring>
#include <utility>

namespace blink {

// DiscardableMemory allocations are expensive and page-grained. We only use
// them when there's a reasonable amount of memory to be saved by the OS
// discarding the memory.
static const size_t minimumDiscardableAllocationSize = 4 * 4096;

PurgeableVector::PurgeableVector(PurgeableOption purgeable)
    : m_discardableCapacity(0)
    , m_discardableSize(0)
    , m_isPurgeable(purgeable == Purgeable)
{
}

PurgeableVector::~PurgeableVector()
{
}

void PurgeableVector::reserveCapacity(size_t capacity)
{
    if (m_isPurgeable) {
        if (reservePurgeableCapacity(capacity, UseExactCapacity))
            return;
        // Fallback to non-purgeable buffer allocation in case discardable memory allocation failed.
    }

    if (!m_vector.capacity()) {
        // Using reserveInitialCapacity() on the underlying vector ensures that the vector uses the
        // exact specified capacity to avoid consuming too much memory for small resources.
        m_vector.reserveInitialCapacity(capacity);
    } else {
        m_vector.reserveCapacity(capacity);
    }

    moveDataFromDiscardableToVector();
}

void PurgeableVector::onMemoryDump(const String& dumpName, WebProcessMemoryDump* memoryDump) const
{
    ASSERT(!(m_discardable && m_vector.size()));
    if (m_discardable) {
        WebMemoryAllocatorDump* dump = memoryDump->createDiscardableMemoryAllocatorDump(
            StringUTF8Adaptor(dumpName).asStringPiece().as_string(), m_discardable.get());
        dump->addScalar("discardable_size", "bytes", m_discardableSize);
    } else if (m_vector.size()) {
        WebMemoryAllocatorDump* dump = memoryDump->createMemoryAllocatorDump(dumpName);
        dump->addScalar("size", "bytes", m_vector.size());
        memoryDump->addSuballocation(dump->guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
    }
}

void PurgeableVector::moveDataFromDiscardableToVector()
{
    if (m_discardable) {
        m_vector.append(static_cast<const char*>(m_discardable->data()), m_discardableSize);
        clearDiscardable();
    }
}

void PurgeableVector::clearDiscardable()
{
    m_discardable = nullptr;
    m_discardableCapacity = 0;
    m_discardableSize = 0;
}

void PurgeableVector::append(const char* data, size_t length)
{
    if (!m_isPurgeable) {
        m_vector.append(data, length);
        return;
    }

    const size_t currentSize = m_discardable ? m_discardableSize : m_vector.size();
    const size_t newBufferSize = currentSize + length;

    if (!reservePurgeableCapacity(newBufferSize, UseExponentialGrowth)) {
        moveDataFromDiscardableToVector();
        m_vector.append(data, length);
        return;
    }

    ASSERT(m_discardableSize + length <= m_discardableCapacity);
    memcpy(static_cast<char*>(m_discardable->data()) + m_discardableSize, data, length);
    m_discardableSize += length;
}

void PurgeableVector::grow(size_t newSize)
{
    ASSERT(newSize >= size());

    if (m_isPurgeable) {
        if (reservePurgeableCapacity(newSize, UseExponentialGrowth)) {
            m_discardableSize = newSize;
            return;
        }
        moveDataFromDiscardableToVector();
    }

    m_vector.resize(newSize);
}

void PurgeableVector::clear()
{
    clearDiscardable();
    m_vector.clear();
}

char* PurgeableVector::data()
{
    return m_discardable ? static_cast<char*>(m_discardable->data()) : m_vector.data();
}

size_t PurgeableVector::size() const
{
    return m_discardable ? m_discardableSize : m_vector.size();
}

void PurgeableVector::adopt(Vector<char>& other)
{
    if (size() > 0)
        clear();

    if (!m_isPurgeable) {
        m_vector.swap(other);
        return;
    }

    if (other.isEmpty())
        return;

    append(other.data(), other.size());
    other.clear();
}

bool PurgeableVector::reservePurgeableCapacity(size_t capacity, PurgeableAllocationStrategy allocationStrategy)
{
    ASSERT(m_isPurgeable);

    if (m_discardable && m_discardableCapacity >= capacity) {
        ASSERT(!m_vector.capacity());
        return true;
    }

    if (capacity < minimumDiscardableAllocationSize)
        return false;

    if (allocationStrategy == UseExponentialGrowth)
        capacity = adjustPurgeableCapacity(capacity);

    std::unique_ptr<base::DiscardableMemory> discardable =
        base::DiscardableMemoryAllocator::GetInstance()->AllocateLockedDiscardableMemory(capacity);
    ASSERT(discardable);

    m_discardableCapacity = capacity;
    // Copy the data that was either in the previous purgeable buffer or in the vector to the new
    // purgeable buffer.
    if (m_discardable) {
        memcpy(discardable->data(), m_discardable->data(), m_discardableSize);
    } else {
        memcpy(discardable->data(), m_vector.data(), m_vector.size());
        m_discardableSize = m_vector.size();
        m_vector.clear();
    }

    m_discardable = std::move(discardable);
    ASSERT(!m_vector.capacity());
    return true;
}

size_t PurgeableVector::adjustPurgeableCapacity(size_t capacity) const
{
    ASSERT(capacity >= minimumDiscardableAllocationSize);

    const float growthFactor = 1.5;
    size_t newCapacity = std::max(capacity, static_cast<size_t>(m_discardableCapacity * growthFactor));

    // Discardable memory has page-granularity so align to the next page here to minimize
    // fragmentation.
    // Since the page size is only used below to minimize fragmentation it's still safe to use it
    // even if it gets out of sync (e.g. due to the use of huge pages).
    const size_t kPageSize = 4096;
    newCapacity = (newCapacity + kPageSize - 1) & ~(kPageSize - 1);

    return std::max(capacity, newCapacity); // Overflow check.
}

} // namespace blink
