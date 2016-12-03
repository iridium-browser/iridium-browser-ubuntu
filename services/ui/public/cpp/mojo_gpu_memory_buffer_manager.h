// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_MOJO_GPU_MEMORY_BUFFER_MANAGER_H_
#define SERVICES_UI_PUBLIC_CPP_MOJO_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace ui {

class MojoGpuMemoryBufferManager : public gpu::GpuMemoryBufferManager {
 public:
  MojoGpuMemoryBufferManager();
  ~MojoGpuMemoryBufferManager() override;

  // Overridden from gpu::GpuMemoryBufferManager:
  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle) override;
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBufferFromHandle(
      const gfx::GpuMemoryBufferHandle& handle,
      const gfx::Size& size,
      gfx::BufferFormat format) override;
  gfx::GpuMemoryBuffer* GpuMemoryBufferFromClientBuffer(
      ClientBuffer buffer) override;
  void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                               const gpu::SyncToken& sync_token) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoGpuMemoryBufferManager);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_MOJO_GPU_MEMORY_BUFFER_MANAGER_H_
