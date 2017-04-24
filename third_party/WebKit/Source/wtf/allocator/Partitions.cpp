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

#include "wtf/allocator/Partitions.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/debug/alias.h"
#include "wtf/allocator/PartitionAllocator.h"

namespace WTF {

const char* const Partitions::kAllocatedObjectPoolName =
    "partition_alloc/allocated_objects";

base::subtle::SpinLock Partitions::s_initializationLock;
bool Partitions::s_initialized = false;

base::PartitionAllocatorGeneric Partitions::m_fastMallocAllocator;
base::PartitionAllocatorGeneric Partitions::m_arrayBufferAllocator;
base::PartitionAllocatorGeneric Partitions::m_bufferAllocator;
base::SizeSpecificPartitionAllocator<1024> Partitions::m_layoutAllocator;
Partitions::ReportPartitionAllocSizeFunction Partitions::m_reportSizeFunction =
    nullptr;

void Partitions::initialize(
    ReportPartitionAllocSizeFunction reportSizeFunction) {
  base::subtle::SpinLock::Guard guard(s_initializationLock);

  if (!s_initialized) {
    base::PartitionAllocGlobalInit(&Partitions::handleOutOfMemory);
    m_fastMallocAllocator.init();
    m_arrayBufferAllocator.init();
    m_bufferAllocator.init();
    m_layoutAllocator.init();
    m_reportSizeFunction = reportSizeFunction;
    s_initialized = true;
  }
}

void Partitions::decommitFreeableMemory() {
  RELEASE_ASSERT(isMainThread());
  if (!s_initialized)
    return;

  PartitionPurgeMemoryGeneric(arrayBufferPartition(),
                              base::PartitionPurgeDecommitEmptyPages);
  PartitionPurgeMemoryGeneric(bufferPartition(),
                              base::PartitionPurgeDecommitEmptyPages);
  PartitionPurgeMemoryGeneric(fastMallocPartition(),
                              base::PartitionPurgeDecommitEmptyPages);
  PartitionPurgeMemory(layoutPartition(),
                       base::PartitionPurgeDecommitEmptyPages);
}

void Partitions::reportMemoryUsageHistogram() {
  static size_t observedMaxSizeInMB = 0;

  if (!m_reportSizeFunction)
    return;
  // We only report the memory in the main thread.
  if (!isMainThread())
    return;
  // +1 is for rounding up the sizeInMB.
  size_t sizeInMB = Partitions::totalSizeOfCommittedPages() / 1024 / 1024 + 1;
  if (sizeInMB > observedMaxSizeInMB) {
    m_reportSizeFunction(sizeInMB);
    observedMaxSizeInMB = sizeInMB;
  }
}

void Partitions::dumpMemoryStats(
    bool isLightDump,
    base::PartitionStatsDumper* partitionStatsDumper) {
  // Object model and rendering partitions are not thread safe and can be
  // accessed only on the main thread.
  DCHECK(isMainThread());

  decommitFreeableMemory();
  PartitionDumpStatsGeneric(fastMallocPartition(), "fast_malloc", isLightDump,
                            partitionStatsDumper);
  PartitionDumpStatsGeneric(arrayBufferPartition(), "array_buffer", isLightDump,
                            partitionStatsDumper);
  PartitionDumpStatsGeneric(bufferPartition(), "buffer", isLightDump,
                            partitionStatsDumper);
  PartitionDumpStats(layoutPartition(), "layout", isLightDump,
                     partitionStatsDumper);
}

static NEVER_INLINE void partitionsOutOfMemoryUsing2G() {
  size_t signature = 2UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NEVER_INLINE void partitionsOutOfMemoryUsing1G() {
  size_t signature = 1UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NEVER_INLINE void partitionsOutOfMemoryUsing512M() {
  size_t signature = 512 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NEVER_INLINE void partitionsOutOfMemoryUsing256M() {
  size_t signature = 256 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NEVER_INLINE void partitionsOutOfMemoryUsing128M() {
  size_t signature = 128 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NEVER_INLINE void partitionsOutOfMemoryUsing64M() {
  size_t signature = 64 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NEVER_INLINE void partitionsOutOfMemoryUsing32M() {
  size_t signature = 32 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NEVER_INLINE void partitionsOutOfMemoryUsing16M() {
  size_t signature = 16 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NEVER_INLINE void partitionsOutOfMemoryUsingLessThan16M() {
  size_t signature = 16 * 1024 * 1024 - 1;
  base::debug::Alias(&signature);
  DLOG(FATAL) << "ParitionAlloc: out of memory with < 16M usage (error:"
              << GetAllocPageErrorCode() << ")";
}

void Partitions::handleOutOfMemory() {
  volatile size_t totalUsage = totalSizeOfCommittedPages();
  uint32_t allocPageErrorCode = GetAllocPageErrorCode();
  base::debug::Alias(&allocPageErrorCode);

  if (totalUsage >= 2UL * 1024 * 1024 * 1024)
    partitionsOutOfMemoryUsing2G();
  if (totalUsage >= 1UL * 1024 * 1024 * 1024)
    partitionsOutOfMemoryUsing1G();
  if (totalUsage >= 512 * 1024 * 1024)
    partitionsOutOfMemoryUsing512M();
  if (totalUsage >= 256 * 1024 * 1024)
    partitionsOutOfMemoryUsing256M();
  if (totalUsage >= 128 * 1024 * 1024)
    partitionsOutOfMemoryUsing128M();
  if (totalUsage >= 64 * 1024 * 1024)
    partitionsOutOfMemoryUsing64M();
  if (totalUsage >= 32 * 1024 * 1024)
    partitionsOutOfMemoryUsing32M();
  if (totalUsage >= 16 * 1024 * 1024)
    partitionsOutOfMemoryUsing16M();
  partitionsOutOfMemoryUsingLessThan16M();
}

}  // namespace WTF
