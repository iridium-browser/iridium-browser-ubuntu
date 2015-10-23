// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_ref_counted_memory.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"

namespace gfx {

GLImageRefCountedMemory::GLImageRefCountedMemory(const gfx::Size& size,
                                                 unsigned internalformat)
    : GLImageMemory(size, internalformat) {
}

GLImageRefCountedMemory::~GLImageRefCountedMemory() {
  DCHECK(!ref_counted_memory_.get());
}

bool GLImageRefCountedMemory::Initialize(
    base::RefCountedMemory* ref_counted_memory,
    gfx::BufferFormat format) {
  if (!GLImageMemory::Initialize(ref_counted_memory->front(), format))
    return false;

  DCHECK(!ref_counted_memory_.get());
  ref_counted_memory_ = ref_counted_memory;
  return true;
}

void GLImageRefCountedMemory::Destroy(bool have_context) {
  GLImageMemory::Destroy(have_context);
  ref_counted_memory_ = NULL;
}

void GLImageRefCountedMemory::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {
  // Log size 0 if |ref_counted_memory_| has been released.
  size_t size_in_bytes = ref_counted_memory_ ? ref_counted_memory_->size() : 0;

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size_in_bytes));
}

}  // namespace gfx
