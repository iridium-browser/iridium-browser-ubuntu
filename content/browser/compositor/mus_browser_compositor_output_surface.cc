// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/mus_browser_compositor_output_surface.h"

#include <utility>

#include "cc/output/compositor_frame.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/texture_draw_quad.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_surface.h"

namespace content {

MusBrowserCompositorOutputSurface::MusBrowserCompositorOutputSurface(
    ui::Window* window,
    scoped_refptr<ContextProviderCommandBuffer> context,
    scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
    cc::SyntheticBeginFrameSource* begin_frame_source,
    std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
        overlay_candidate_validator)
    : GpuBrowserCompositorOutputSurface(std::move(context),
                                        std::move(vsync_manager),
                                        begin_frame_source,
                                        std::move(overlay_candidate_validator)),
      ui_window_(window) {
  ui_window_surface_ =
      ui_window_->RequestSurface(ui::mojom::SurfaceType::DEFAULT);
}

MusBrowserCompositorOutputSurface::~MusBrowserCompositorOutputSurface() {}

void MusBrowserCompositorOutputSurface::SwapBuffers(cc::CompositorFrame frame) {
  const gfx::Rect bounds(ui_window_->bounds().size());
  cc::CompositorFrame ui_frame;
  ui_frame.metadata = std::move(frame.metadata);
  ui_frame.delegated_frame_data = base::MakeUnique<cc::DelegatedFrameData>();
  const cc::RenderPassId render_pass_id(1, 1);
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  const bool has_transparent_background = true;
  pass->SetAll(render_pass_id, bounds, bounds, gfx::Transform(),
               has_transparent_background);
  // The SharedQuadState is owned by the SharedQuadStateList
  // shared_quad_state_list.
  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds.size(), bounds, bounds,
              false /* is_clipped */, 1.f /* opacity */, SkXfermode::kSrc_Mode,
              0 /* sorting_context_id */);

  cc::TransferableResource resource;
  resource.id = AllocateResourceId();
  resource.format = cc::ResourceFormat::RGBA_8888;
  resource.filter = GL_LINEAR;
  resource.size = frame.gl_frame_data->size;

  const gpu::Mailbox& mailbox = GetMailboxFromResourceId(resource.id);
  DCHECK(!mailbox.IsZero());
  const gfx::Rect rect(frame.gl_frame_data->size);

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
  ui_frame.delegated_frame_data->resource_list.push_back(std::move(resource));

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

  ui_frame.delegated_frame_data->render_pass_list.push_back(std::move(pass));
  // ui_frame_surface_ will be destroyed by MusBrowserCompositorOutputSurface's
  // destructor, and the callback of SubmitCompositorFrame() will not be fired
  // after ui_window_surface_ is destroyed, so it is safe to use
  // base::Unretained(this) here.
  ui_window_surface_->SubmitCompositorFrame(
      std::move(ui_frame),
      base::Bind(&MusBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted,
                 base::Unretained(this), std::vector<ui::LatencyInfo>(),
                 gfx::SwapResult::SWAP_ACK, nullptr));
  return;
}

bool MusBrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  if (!GpuBrowserCompositorOutputSurface::BindToClient(client))
    return false;
  ui_window_surface_->BindToThread();
  ui_window_surface_->set_client(this);
  return true;
}

void MusBrowserCompositorOutputSurface::OnResourcesReturned(
    ui::WindowSurface* surface,
    mojo::Array<cc::ReturnedResource> resources) {
  for (const auto& resource : resources) {
    DCHECK_EQ(1, resource.count);
    const gpu::Mailbox& mailbox = GetMailboxFromResourceId(resource.id);
    GetCommandBufferProxy()->ReturnFrontBuffer(mailbox, resource.sync_token,
                                               resource.lost);
    FreeResourceId(resource.id);
  }
}

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
