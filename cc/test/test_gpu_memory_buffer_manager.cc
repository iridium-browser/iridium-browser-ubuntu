// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_gpu_memory_buffer_manager.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

size_t SubsamplingFactor(gfx::BufferFormat format, int plane) {
  switch (format) {
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::DXT5:
    case gfx::BufferFormat::ETC1:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::UYVY_422:
      return 1;
    case gfx::BufferFormat::YUV_420: {
      static size_t factor[] = {1, 2, 2};
      DCHECK_LT(static_cast<size_t>(plane), arraysize(factor));
      return factor[plane];
    }
  }
  NOTREACHED();
  return 0;
}

size_t StrideInBytes(size_t width, gfx::BufferFormat format, int plane) {
  switch (format) {
    case gfx::BufferFormat::ATCIA:
    case gfx::BufferFormat::DXT5:
      DCHECK_EQ(plane, 0);
      return width;
    case gfx::BufferFormat::ATC:
    case gfx::BufferFormat::DXT1:
    case gfx::BufferFormat::ETC1:
      DCHECK_EQ(plane, 0);
      DCHECK_EQ(width % 2, 0u);
      return width / 2;
    case gfx::BufferFormat::R_8:
      return (width + 3) & ~0x3;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::UYVY_422:
      DCHECK_EQ(plane, 0);
      return width * 2;
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_8888:
      DCHECK_EQ(plane, 0);
      return width * 4;
    case gfx::BufferFormat::YUV_420:
      return width / SubsamplingFactor(format, plane);
  }
  NOTREACHED();
  return 0;
}

size_t BufferSizeInBytes(const gfx::Size& size, gfx::BufferFormat format) {
  size_t size_in_bytes = 0;
  int num_planes = static_cast<int>(gfx::NumberOfPlanesForBufferFormat(format));
  for (int i = 0; i < num_planes; ++i) {
    size_in_bytes += StrideInBytes(size.width(), format, i) *
                     (size.height() / SubsamplingFactor(format, i));
  }
  return size_in_bytes;
}

class GpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  GpuMemoryBufferImpl(const gfx::Size& size,
                      gfx::BufferFormat format,
                      scoped_ptr<base::SharedMemory> shared_memory)
      : size_(size),
        format_(format),
        shared_memory_(shared_memory.Pass()),
        mapped_(false) {}

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map(void** data) override {
    DCHECK(!mapped_);
    if (!shared_memory_->Map(BufferSizeInBytes(size_, format_)))
      return false;
    mapped_ = true;
    size_t offset = 0;
    int num_planes =
        static_cast<int>(gfx::NumberOfPlanesForBufferFormat(format_));
    for (int i = 0; i < num_planes; ++i) {
      data[i] = reinterpret_cast<uint8*>(shared_memory_->memory()) + offset;
      offset += StrideInBytes(size_.width(), format_, i) *
                (size_.height() / SubsamplingFactor(format_, i));
    }
    return true;
  }
  void Unmap() override {
    DCHECK(mapped_);
    shared_memory_->Unmap();
    mapped_ = false;
  }
  bool IsMapped() const override { return mapped_; }
  gfx::BufferFormat GetFormat() const override { return format_; }
  void GetStride(int* stride) const override {
    int num_planes =
        static_cast<int>(gfx::NumberOfPlanesForBufferFormat(format_));
    for (int i = 0; i < num_planes; ++i)
      stride[i] =
          base::checked_cast<int>(StrideInBytes(size_.width(), format_, i));
  }
  gfx::GpuMemoryBufferId GetId() const override {
    NOTREACHED();
    return gfx::GpuMemoryBufferId(0);
  }
  gfx::GpuMemoryBufferHandle GetHandle() const override {
    gfx::GpuMemoryBufferHandle handle;
    handle.type = gfx::SHARED_MEMORY_BUFFER;
    handle.handle = shared_memory_->handle();
    return handle;
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }

 private:
  const gfx::Size size_;
  gfx::BufferFormat format_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  bool mapped_;
};

}  // namespace

TestGpuMemoryBufferManager::TestGpuMemoryBufferManager() {
}

TestGpuMemoryBufferManager::~TestGpuMemoryBufferManager() {
}

scoped_ptr<gfx::GpuMemoryBuffer>
TestGpuMemoryBufferManager::AllocateGpuMemoryBuffer(const gfx::Size& size,
                                                    gfx::BufferFormat format,
                                                    gfx::BufferUsage usage) {
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  if (!shared_memory->CreateAnonymous(BufferSizeInBytes(size, format)))
    return nullptr;
  return make_scoped_ptr<gfx::GpuMemoryBuffer>(
      new GpuMemoryBufferImpl(size, format, shared_memory.Pass()));
}

gfx::GpuMemoryBuffer*
TestGpuMemoryBufferManager::GpuMemoryBufferFromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
}

void TestGpuMemoryBufferManager::SetDestructionSyncPoint(
    gfx::GpuMemoryBuffer* buffer,
    uint32 sync_point) {
}

}  // namespace cc
