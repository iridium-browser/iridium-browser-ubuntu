// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/mus_browser_compositor_output_surface.h"

#include <utility>

#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_frame.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/texture_draw_quad.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "services/ui/public/cpp/window_compositor_frame_sink.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

MusBrowserCompositorOutputSurface::MusBrowserCompositorOutputSurface(
    aura::Window* window,
    scoped_refptr<ui::ContextProviderCommandBuffer> context,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const UpdateVSyncParametersCallback& update_vsync_parameters_callback,
    std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
        overlay_candidate_validator)
    : GpuBrowserCompositorOutputSurface(std::move(context),
                                        update_vsync_parameters_callback,
                                        std::move(overlay_candidate_validator)),
      window_(window),
      begin_frame_source_(nullptr) {
  aura::WindowPortMus* window_port = aura::WindowPortMus::Get(window_);
  DCHECK(window_port);
  compositor_frame_sink_ = window_port->RequestCompositorFrameSink(
      context, gpu_memory_buffer_manager);
  compositor_frame_sink_->BindToClient(this);
}

MusBrowserCompositorOutputSurface::~MusBrowserCompositorOutputSurface() {}

cc::BeginFrameSource* MusBrowserCompositorOutputSurface::GetBeginFrameSource() {
  return begin_frame_source_;
}

void MusBrowserCompositorOutputSurface::SwapBuffers(
    cc::OutputSurfaceFrame frame) {
  cc::CompositorFrame ui_frame;
  ui_frame.metadata.device_scale_factor = display::Screen::GetScreen()
                                              ->GetDisplayNearestWindow(window_)
                                              .device_scale_factor();
  ui_frame.metadata.latency_info = std::move(frame.latency_info);
  // Reset latency_info to known empty state after moving contents.
  frame.latency_info.clear();
  const int render_pass_id = 1;
  const gfx::Rect bounds_in_dip = gfx::Rect(window_->bounds().size());
  const gfx::Rect bounds_in_pixels = gfx::ConvertRectToPixel(
      ui_frame.metadata.device_scale_factor, bounds_in_dip);
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  pass->SetNew(render_pass_id, bounds_in_pixels, bounds_in_pixels,
               gfx::Transform());
  // The SharedQuadState is owned by the SharedQuadStateList
  // shared_quad_state_list.
  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds_in_pixels.size(), bounds_in_pixels,
              bounds_in_pixels, false /* is_clipped */, 1.f /* opacity */,
              SkBlendMode::kSrc, 0 /* sorting_context_id */);

  cc::TransferableResource resource;
  resource.id = AllocateResourceId();
  resource.format = cc::ResourceFormat::RGBA_8888;
  resource.filter = GL_LINEAR;
  resource.size = frame.size;

  const gpu::Mailbox& mailbox = GetMailboxFromResourceId(resource.id);
  DCHECK(!mailbox.IsZero());
  const gfx::Rect rect(frame.size);

  // Call parent's SwapBuffers to generate the front buffer, and then send the
  // front buffer to mus.
  // TODO(penghuang): we should avoid extra copies here by sending frames to mus
  // directly from renderer.
  GpuBrowserCompositorOutputSurface::SwapBuffers(std::move(frame));
  GetCommandBufferProxy()->TakeFrontBuffer(mailbox);

  auto* gl = context_provider()->ContextGL();
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  resource.mailbox_holder =
      gpu::MailboxHolder(mailbox, sync_token, GL_TEXTURE_2D);
  resource.read_lock_fences_enabled = false;
  resource.is_software = false;
  resource.is_overlay_candidate = false;
  ui_frame.resource_list.push_back(std::move(resource));

  const bool needs_blending = true;
  const bool premultiplied_alpha = true;
  const gfx::PointF uv_top_left(0.f, 0.f);
  const gfx::PointF uv_bottom_right(1.f, 1.f);
  const uint32_t background_color = 0x00000000;
  const float vertex_opacity[4] = {1.f, 1.f, 1.f, 1.f};
  const bool y_flipped = true;
  const bool nearest_neighbor = false;
  const bool secure_output_only = false;

  cc::TextureDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
  quad->SetAll(sqs, rect, rect, rect, needs_blending, resource.id, gfx::Size(),
               premultiplied_alpha, uv_top_left, uv_bottom_right,
               background_color, vertex_opacity, y_flipped, nearest_neighbor,
               secure_output_only);

  ui_frame.render_pass_list.push_back(std::move(pass));

  compositor_frame_sink_->SubmitCompositorFrame(std::move(ui_frame));
}

void MusBrowserCompositorOutputSurface::SetBeginFrameSource(
    cc::BeginFrameSource* source) {
  begin_frame_source_ = source;
}

void MusBrowserCompositorOutputSurface::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  for (const auto& resource : resources) {
    DCHECK_EQ(1, resource.count);
    const gpu::Mailbox& mailbox = GetMailboxFromResourceId(resource.id);
    GetCommandBufferProxy()->ReturnFrontBuffer(mailbox, resource.sync_token,
                                               resource.lost);
    FreeResourceId(resource.id);
  }
}

void MusBrowserCompositorOutputSurface::SetTreeActivationCallback(
    const base::Closure& callback) {}

void MusBrowserCompositorOutputSurface::DidReceiveCompositorFrameAck() {
  OnGpuSwapBuffersCompleted(std::vector<ui::LatencyInfo>(),
                            gfx::SwapResult::SWAP_ACK, nullptr);
}

void MusBrowserCompositorOutputSurface::DidLoseCompositorFrameSink() {}

void MusBrowserCompositorOutputSurface::OnDraw(
    const gfx::Transform& transform,
    const gfx::Rect& viewport,
    bool resourceless_software_draw) {}

void MusBrowserCompositorOutputSurface::SetMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {}

void MusBrowserCompositorOutputSurface::SetExternalTilePriorityConstraints(
    const gfx::Rect& viewport_rect,
    const gfx::Transform& transform) {}

uint32_t MusBrowserCompositorOutputSurface::AllocateResourceId() {
  if (!free_resource_ids_.empty()) {
    uint32_t id = free_resource_ids_.back();
    free_resource_ids_.pop_back();
    return id;
  }
  // If there is no free resource id, we generate a new mailbox in the mailbox
  // vector, and the index of the new mailbox is the new allocated resource id.
  uint32_t id = mailboxes_.size();
  mailboxes_.push_back(gpu::Mailbox::Generate());
  return id;
}

void MusBrowserCompositorOutputSurface::FreeResourceId(uint32_t id) {
  DCHECK_LT(id, mailboxes_.size());
  DCHECK(std::find(free_resource_ids_.begin(), free_resource_ids_.end(), id) ==
         free_resource_ids_.end());
  free_resource_ids_.push_back(id);
}

const gpu::Mailbox& MusBrowserCompositorOutputSurface::GetMailboxFromResourceId(
    uint32_t id) {
  DCHECK_LT(id, mailboxes_.size());
  DCHECK(std::find(free_resource_ids_.begin(), free_resource_ids_.end(), id) ==
         free_resource_ids_.end());
  return mailboxes_[id];
}

}  // namespace content
