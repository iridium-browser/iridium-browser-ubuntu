// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSUI_STUB_CONTEXT_FACTORY_H_
#define ASH_SYSUI_STUB_CONTEXT_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "ui/compositor/compositor.h"

namespace cc {
class SingleThreadTaskGraphRunner;
}

namespace ash {
namespace sysui {

class StubContextFactory : public ui::ContextFactory {
 public:
  StubContextFactory();
  ~StubContextFactory() override;

 private:
  // ui::ContextFactory implementation.
  void CreateOutputSurface(base::WeakPtr<ui::Compositor> compositor) override;
  std::unique_ptr<ui::Reflector> CreateReflector(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer) override;
  void RemoveReflector(ui::Reflector* reflector) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  bool DoesCreateTestContexts() override;
  uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                 gfx::BufferUsage usage) override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  uint32_t AllocateSurfaceClientId() override;
  cc::SurfaceManager* GetSurfaceManager() override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;
  void SetDisplayColorSpace(ui::Compositor* compositor,
                            const gfx::ColorSpace& color_space) override;
  void SetAuthoritativeVSyncInterval(ui::Compositor* compositor,
                                     base::TimeDelta interval) override {}
  void SetDisplayVSyncParameters(ui::Compositor* compositor,
                                 base::TimeTicks timebase,
                                 base::TimeDelta interval) override {}
  void SetOutputIsSecure(ui::Compositor* compositor, bool secure) override {}
  void AddObserver(ui::ContextFactoryObserver* observer) override {}
  void RemoveObserver(ui::ContextFactoryObserver* observer) override {}

  uint32_t next_surface_id_namespace_;
  std::unique_ptr<cc::SingleThreadTaskGraphRunner> task_graph_runner_;

  DISALLOW_COPY_AND_ASSIGN(StubContextFactory);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_SYSUI_STUB_CONTEXT_FACTORY_H_
