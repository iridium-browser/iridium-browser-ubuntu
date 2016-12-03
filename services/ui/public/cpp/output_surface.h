// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_OUTPUT_SURFACE_H_
#define SERVICES_UI_PUBLIC_CPP_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "cc/output/output_surface.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/cpp/window_surface.h"
#include "services/ui/public/cpp/window_surface_client.h"

namespace gpu {
class GpuChannelHost;
}

namespace ui {

class OutputSurface : public cc::OutputSurface, public WindowSurfaceClient {
 public:
  OutputSurface(scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
                std::unique_ptr<WindowSurface> surface);
  ~OutputSurface() override;

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame frame) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void DetachFromClient() override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

 private:
  // WindowSurfaceClient implementation:
  void OnResourcesReturned(
      WindowSurface* surface,
      mojo::Array<cc::ReturnedResource> resources) override;

  void SwapBuffersComplete();

  std::unique_ptr<cc::BeginFrameSource> begin_frame_source_;
  std::unique_ptr<WindowSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurface);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_OUTPUT_SURFACE_H_
