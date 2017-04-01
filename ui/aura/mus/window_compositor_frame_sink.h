// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_COMPOSITOR_FRAME_SINK_H_
#define UI_AURA_MUS_WINDOW_COMPOSITOR_FRAME_SINK_H_

#include "base/macros.h"
#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace aura {

class WindowCompositorFrameSinkBinding;

class WindowCompositorFrameSink
    : public cc::CompositorFrameSink,
      public cc::mojom::MojoCompositorFrameSinkClient,
      public cc::ExternalBeginFrameSourceClient {
 public:
  // static
  static std::unique_ptr<WindowCompositorFrameSink> Create(
      const cc::FrameSinkId& frame_sink_id,
      scoped_refptr<cc::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      std::unique_ptr<WindowCompositorFrameSinkBinding>*
          compositor_frame_sink_binding);

  ~WindowCompositorFrameSink() override;

  // cc::CompositorFrameSink implementation.
  bool BindToClient(cc::CompositorFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;

 private:
  WindowCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      scoped_refptr<cc::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      cc::mojom::MojoCompositorFrameSinkPtrInfo compositor_frame_sink_info,
      cc::mojom::MojoCompositorFrameSinkClientRequest client_request);

  // cc::mojom::MojoCompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_args) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface() override;

  // cc::ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  gfx::Size last_submitted_frame_size_;
  cc::LocalFrameId local_frame_id_;
  cc::SurfaceIdAllocator id_allocator_;
  std::unique_ptr<cc::ExternalBeginFrameSource> begin_frame_source_;
  cc::mojom::MojoCompositorFrameSinkPtrInfo compositor_frame_sink_info_;
  cc::mojom::MojoCompositorFrameSinkClientRequest client_request_;
  cc::mojom::MojoCompositorFrameSinkPtr compositor_frame_sink_;
  std::unique_ptr<mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient>>
      client_binding_;
  std::unique_ptr<base::ThreadChecker> thread_checker_;
  const cc::FrameSinkId frame_sink_id_;

  DISALLOW_COPY_AND_ASSIGN(WindowCompositorFrameSink);
};

// A WindowCompositorFrameSinkBinding is a bundle of mojo interfaces that is
// created by WindowCompositorFrameSink::Create and is used by or implemented by
// Mus when passed into Window::AttachCompositorFrameSink.
// WindowCompositorFrameSinkBinding has no standalone functionality. Its purpose
// is to allow safely creating and attaching a CompositorFrameSink on one
// thread and using it on another.
class WindowCompositorFrameSinkBinding {
 public:
  ~WindowCompositorFrameSinkBinding();

 private:
  friend class WindowPortMus;
  friend class WindowCompositorFrameSink;

  WindowCompositorFrameSinkBinding(
      cc::mojom::MojoCompositorFrameSinkRequest compositor_frame_sink_request,
      mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSinkClient>
          compositor_frame_sink_client);

  cc::mojom::MojoCompositorFrameSinkRequest compositor_frame_sink_request_;
  mojo::InterfacePtrInfo<cc::mojom::MojoCompositorFrameSinkClient>
      compositor_frame_sink_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowCompositorFrameSinkBinding);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_COMPOSITOR_FRAME_SINK_H_
