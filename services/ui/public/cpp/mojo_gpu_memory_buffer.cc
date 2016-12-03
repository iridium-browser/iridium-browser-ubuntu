// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/mojo_gpu_memory_buffer.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/buffer_format_util.h"

namespace ui {

// TODO(rjkroege): Support running a destructor callback as necessary.
MojoGpuMemoryBufferImpl::~MojoGpuMemoryBufferImpl() {}

// static
std::unique_ptr<gfx::GpuMemoryBuffer> MojoGpuMemoryBufferImpl::Create(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  size_t bytes = gfx::BufferSizeForBufferFormat(size, format);

  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(bytes);
  if (!handle.is_valid())
    return nullptr;

  base::SharedMemoryHandle platform_handle;
  size_t shared_memory_size;
  bool readonly;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(handle), &platform_handle, &shared_memory_size, &readonly);
  if (result != MOJO_RESULT_OK)
    return nullptr;
  DCHECK_EQ(shared_memory_size, bytes);

  auto shared_memory =
      base::MakeUnique<base::SharedMemory>(platform_handle, readonly);
  const int stride = base::checked_cast<int>(
      gfx::RowSizeForBufferFormat(size.width(), format, 0));
  return base::WrapUnique(new MojoGpuMemoryBufferImpl(
      size, format, std::move(shared_memory), 0, stride));
}

// static
std::unique_ptr<gfx::GpuMemoryBuffer> MojoGpuMemoryBufferImpl::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);
  DCHECK(base::SharedMemory::IsHandleValid(handle.handle));
  const bool readonly = false;
  auto shared_memory =
      base::MakeUnique<base::SharedMemory>(handle.handle, readonly);
  return base::WrapUnique(new MojoGpuMemoryBufferImpl(
      size, format, std::move(shared_memory), handle.offset, handle.stride));
}

MojoGpuMemoryBufferImpl* MojoGpuMemoryBufferImpl::FromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<MojoGpuMemoryBufferImpl*>(buffer);
}

const unsigned char* MojoGpuMemoryBufferImpl::GetMemory() const {
  return static_cast<const unsigned char*>(shared_memory_->memory());
}

bool MojoGpuMemoryBufferImpl::Map() {
  DCHECK(!mapped_);
  DCHECK_EQ(static_cast<size_t>(stride_),
            gfx::RowSizeForBufferFormat(size_.width(), format_, 0));
  const size_t buffer_size = gfx::BufferSizeForBufferFormat(size_, format_);
  const size_t map_size = offset_ + buffer_size;
  if (!shared_memory_->Map(map_size))
    return false;
  mapped_ = true;
  return true;
}

void* MojoGpuMemoryBufferImpl::memory(size_t plane) {
  DCHECK(mapped_);
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  return reinterpret_cast<uint8_t*>(shared_memory_->memory()) + offset_ +
         gfx::BufferOffsetForBufferFormat(size_, format_, plane);
}

void MojoGpuMemoryBufferImpl::Unmap() {
  DCHECK(mapped_);
  shared_memory_->Unmap();
  mapped_ = false;
}

int MojoGpuMemoryBufferImpl::stride(size_t plane) const {
  DCHECK_LT(plane, gfx::NumberOfPlanesForBufferFormat(format_));
  return base::checked_cast<int>(gfx::RowSizeForBufferFormat(
      size_.width(), format_, static_cast<int>(plane)));
}

gfx::GpuMemoryBufferHandle MojoGpuMemoryBufferImpl::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.handle = shared_memory_->handle();
  handle.offset = offset_;
  handle.stride = stride_;

  return handle;
}

gfx::GpuMemoryBufferType MojoGpuMemoryBufferImpl::GetBufferType() const {
  return gfx::SHARED_MEMORY_BUFFER;
}

MojoGpuMemoryBufferImpl::MojoGpuMemoryBufferImpl(
    const gfx::Size& size,
    gfx::BufferFormat format,
    std::unique_ptr<base::SharedMemory> shared_memory,
    uint32_t offset,
    int32_t stride)
    : GpuMemoryBufferImpl(gfx::GenericSharedMemoryId(0), size, format),
      shared_memory_(std::move(shared_memory)),
      offset_(offset),
      stride_(stride) {}

}  // namespace ui
