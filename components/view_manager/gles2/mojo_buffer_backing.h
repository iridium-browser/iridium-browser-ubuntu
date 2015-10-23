// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GLES2_MOJO_BUFFER_BACKING_H_
#define COMPONENTS_VIEW_MANAGER_GLES2_MOJO_BUFFER_BACKING_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/common/buffer.h"
#include "third_party/mojo/src/mojo/public/cpp/system/core.h"

namespace gles2 {

class MojoBufferBacking : public gpu::BufferBacking {
 public:
  MojoBufferBacking(mojo::ScopedSharedBufferHandle handle,
                    void* memory,
                    size_t size);
  ~MojoBufferBacking() override;

  static scoped_ptr<gpu::BufferBacking> Create(
      mojo::ScopedSharedBufferHandle handle,
      size_t size);

  void* GetMemory() const override;
  size_t GetSize() const override;

 private:
  mojo::ScopedSharedBufferHandle handle_;
  void* memory_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(MojoBufferBacking);
};

}  // namespace gles2

#endif  // COMPONENTS_VIEW_MANAGER_GLES2_MOJO_BUFFER_BACKING_H_
