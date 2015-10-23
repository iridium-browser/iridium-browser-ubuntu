// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_io_surface.h"

#include "base/logging.h"
#include "content/common/mac/io_surface_manager.h"
#include "ui/gfx/buffer_format_util.h"

namespace content {
namespace {

uint32_t LockFlags(gfx::BufferUsage usage) {
  switch (usage) {
    case gfx::BufferUsage::MAP:
      return kIOSurfaceLockAvoidSync;
    case gfx::BufferUsage::PERSISTENT_MAP:
      return 0;
    case gfx::BufferUsage::SCANOUT:
      return 0;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

GpuMemoryBufferImplIOSurface::GpuMemoryBufferImplIOSurface(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const DestructionCallback& callback,
    IOSurfaceRef io_surface,
    uint32_t lock_flags)
    : GpuMemoryBufferImpl(id, size, format, callback),
      io_surface_(io_surface),
      lock_flags_(lock_flags) {}

GpuMemoryBufferImplIOSurface::~GpuMemoryBufferImplIOSurface() {
}

// static
scoped_ptr<GpuMemoryBufferImpl> GpuMemoryBufferImplIOSurface::CreateFromHandle(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    const DestructionCallback& callback) {
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(
      IOSurfaceManager::GetInstance()->AcquireIOSurface(handle.id));
  if (!io_surface)
    return nullptr;

  return make_scoped_ptr<GpuMemoryBufferImpl>(
      new GpuMemoryBufferImplIOSurface(handle.id, size, format, callback,
                                       io_surface.release(), LockFlags(usage)));
}

bool GpuMemoryBufferImplIOSurface::Map(void** data) {
  DCHECK(!mapped_);
  IOReturn status = IOSurfaceLock(io_surface_, lock_flags_, NULL);
  DCHECK_NE(status, kIOReturnCannotLock);
  mapped_ = true;
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(GetFormat());
  for (size_t plane = 0; plane < num_planes; ++plane)
    data[plane] = IOSurfaceGetBaseAddressOfPlane(io_surface_, plane);
  return true;
}

void GpuMemoryBufferImplIOSurface::Unmap() {
  DCHECK(mapped_);
  IOSurfaceUnlock(io_surface_, lock_flags_, NULL);
  mapped_ = false;
}

void GpuMemoryBufferImplIOSurface::GetStride(int* strides) const {
  size_t num_planes = gfx::NumberOfPlanesForBufferFormat(GetFormat());
  for (size_t plane = 0; plane < num_planes; ++plane)
    strides[plane] = IOSurfaceGetBytesPerRowOfPlane(io_surface_, plane);
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplIOSurface::GetHandle() const {
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::IO_SURFACE_BUFFER;
  handle.id = id_;
  return handle;
}

}  // namespace content
