// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_shared_memory.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_handle.h"

namespace gfx {
namespace {

// Returns true if the size is valid and false otherwise.
bool SizeInBytes(const gfx::Size& size,
                 gfx::BufferFormat format,
                 size_t* size_in_bytes) {
  if (size.IsEmpty())
    return false;

  size_t stride_in_bytes = 0;
  if (!GLImageMemory::StrideInBytes(size.width(), format, &stride_in_bytes))
    return false;
  base::CheckedNumeric<size_t> s = stride_in_bytes;
  s *= size.height();
  if (!s.IsValid())
    return false;
  *size_in_bytes = s.ValueOrDie();
  return true;
}

}  // namespace

GLImageSharedMemory::GLImageSharedMemory(const gfx::Size& size,
                                         unsigned internalformat)
    : GLImageMemory(size, internalformat) {
}

GLImageSharedMemory::~GLImageSharedMemory() {
  DCHECK(!shared_memory_);
}

bool GLImageSharedMemory::Initialize(const gfx::GpuMemoryBufferHandle& handle,
                                     gfx::BufferFormat format) {
  size_t size_in_bytes;
  if (!SizeInBytes(GetSize(), format, &size_in_bytes))
    return false;

  if (!base::SharedMemory::IsHandleValid(handle.handle))
    return false;

  base::SharedMemory shared_memory(handle.handle, true);

  // Duplicate the handle.
  base::SharedMemoryHandle duped_shared_memory_handle;
  if (!shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                    &duped_shared_memory_handle)) {
    DVLOG(0) << "Failed to duplicate shared memory handle.";
    return false;
  }

  scoped_ptr<base::SharedMemory> duped_shared_memory(
      new base::SharedMemory(duped_shared_memory_handle, true));
  if (!duped_shared_memory->Map(size_in_bytes)) {
    DVLOG(0) << "Failed to map shared memory.";
    return false;
  }

  if (!GLImageMemory::Initialize(
          static_cast<unsigned char*>(duped_shared_memory->memory()), format)) {
    return false;
  }

  DCHECK(!shared_memory_);
  shared_memory_ = duped_shared_memory.Pass();
  shared_memory_id_ = handle.id;
  return true;
}

void GLImageSharedMemory::Destroy(bool have_context) {
  GLImageMemory::Destroy(have_context);
  shared_memory_.reset();
}

void GLImageSharedMemory::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {
  size_t size_in_bytes = 0;

  if (shared_memory_) {
    bool result = SizeInBytes(GetSize(), format(), &size_in_bytes);
    DCHECK(result);
  }

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size_in_bytes));

  auto guid = gfx::GetGenericSharedMemoryGUIDForTracing(process_tracing_id,
                                                        shared_memory_id_);
  pmd->CreateSharedGlobalAllocatorDump(guid);
  pmd->AddOwnershipEdge(dump->guid(), guid);
}

}  // namespace gfx
