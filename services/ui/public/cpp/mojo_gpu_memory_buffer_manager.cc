// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/mojo_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "services/ui/public/cpp/mojo_gpu_memory_buffer.h"

namespace ui {

MojoGpuMemoryBufferManager::MojoGpuMemoryBufferManager() {}

MojoGpuMemoryBufferManager::~MojoGpuMemoryBufferManager() {}

std::unique_ptr<gfx::GpuMemoryBuffer>
MojoGpuMemoryBufferManager::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle) {
  return MojoGpuMemoryBufferImpl::Create(size, format, usage);
}

std::unique_ptr<gfx::GpuMemoryBuffer>
MojoGpuMemoryBufferManager::CreateGpuMemoryBufferFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format) {
  const gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;
  return MojoGpuMemoryBufferImpl::CreateFromHandle(handle, size, format, usage);
}

gfx::GpuMemoryBuffer*
MojoGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return MojoGpuMemoryBufferImpl::FromClientBuffer(buffer);
}

void MojoGpuMemoryBufferManager::SetDestructionSyncToken(
    gfx::GpuMemoryBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  NOTIMPLEMENTED();
}

}  // namespace ui
