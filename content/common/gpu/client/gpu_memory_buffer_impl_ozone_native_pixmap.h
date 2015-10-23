// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_OZONE_NATIVE_PIXMAP_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_OZONE_NATIVE_PIXMAP_H_

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace ui {
class ClientNativePixmap;
}

namespace content {

// Implementation of GPU memory buffer based on Ozone native pixmap.
class GpuMemoryBufferImplOzoneNativePixmap : public GpuMemoryBufferImpl {
 public:
  ~GpuMemoryBufferImplOzoneNativePixmap() override;

  static scoped_ptr<GpuMemoryBufferImpl> CreateFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const DestructionCallback& callback);

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map(void** data) override;
  void Unmap() override;
  void GetStride(int* stride) const override;
  gfx::GpuMemoryBufferHandle GetHandle() const override;

 private:
  GpuMemoryBufferImplOzoneNativePixmap(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      const DestructionCallback& callback,
      scoped_ptr<ui::ClientNativePixmap> native_pixmap);

  scoped_ptr<ui::ClientNativePixmap> pixmap_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplOzoneNativePixmap);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_OZONE_NATIVE_PIXMAP_H_
