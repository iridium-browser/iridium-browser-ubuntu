// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/surfaces_instance.h"

#include <algorithm>
#include <utility>

#include "android_webview/browser/aw_gl_surface.h"
#include "android_webview/browser/aw_render_thread_context_provider.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/parent_output_surface.h"
#include "base/memory/ptr_util.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace android_webview {

namespace {
SurfacesInstance* g_surfaces_instance = nullptr;
}  // namespace

// static
scoped_refptr<SurfacesInstance> SurfacesInstance::GetOrCreateInstance() {
  if (g_surfaces_instance)
    return make_scoped_refptr(g_surfaces_instance);
  return make_scoped_refptr(new SurfacesInstance);
}

SurfacesInstance::SurfacesInstance()
    : next_surface_client_id_(1u) {
  cc::RendererSettings settings;

  // Should be kept in sync with compositor_impl_android.cc.
  settings.allow_antialiasing = false;
  settings.highp_threshold_min = 2048;

  // Webview does not own the surface so should not clear it.
  settings.should_clear_root_render_pass = false;

  surface_manager_.reset(new cc::SurfaceManager);
  surface_id_allocator_.reset(
      new cc::SurfaceIdAllocator(next_surface_client_id_++));
  surface_manager_->RegisterSurfaceClientId(surface_id_allocator_->client_id());

  std::unique_ptr<cc::BeginFrameSource> begin_frame_source(
      new cc::StubBeginFrameSource);
  std::unique_ptr<cc::TextureMailboxDeleter> texture_mailbox_deleter(
      new cc::TextureMailboxDeleter(nullptr));
  std::unique_ptr<ParentOutputSurface> output_surface_holder(
      new ParentOutputSurface(AwRenderThreadContextProvider::Create(
          make_scoped_refptr(new AwGLSurface),
          DeferredGpuCommandService::GetInstance())));
  output_surface_ = output_surface_holder.get();
  std::unique_ptr<cc::DisplayScheduler> scheduler(new cc::DisplayScheduler(
      begin_frame_source.get(), nullptr,
      output_surface_holder->capabilities().max_frames_pending));
  display_.reset(new cc::Display(
      nullptr /* shared_bitmap_manager */,
      nullptr /* gpu_memory_buffer_manager */, settings,
      std::move(begin_frame_source), std::move(output_surface_holder),
      std::move(scheduler), std::move(texture_mailbox_deleter)));
  display_->Initialize(this, surface_manager_.get(),
                       surface_id_allocator_->client_id());
  display_->SetVisible(true);

  surface_factory_.reset(new cc::SurfaceFactory(surface_manager_.get(), this));

  DCHECK(!g_surfaces_instance);
  g_surfaces_instance = this;

}

SurfacesInstance::~SurfacesInstance() {
  DCHECK_EQ(g_surfaces_instance, this);
  g_surfaces_instance = nullptr;

  DCHECK(child_ids_.empty());
  if (!root_id_.is_null())
    surface_factory_->Destroy(root_id_);

  surface_manager_->InvalidateSurfaceClientId(
      surface_id_allocator_->client_id());
}

uint32_t SurfacesInstance::AllocateSurfaceClientId() {
  return next_surface_client_id_++;
}

cc::SurfaceManager* SurfacesInstance::GetSurfaceManager() {
  return surface_manager_.get();
}

void SurfacesInstance::DrawAndSwap(const gfx::Size& viewport,
                                   const gfx::Rect& clip,
                                   const gfx::Transform& transform,
                                   const gfx::Size& frame_size,
                                   const cc::SurfaceId& child_id) {
  DCHECK(std::find(child_ids_.begin(), child_ids_.end(), child_id) !=
         child_ids_.end());

  // Create a frame with a single SurfaceDrawQuad referencing the child
  // Surface and transformed using the given transform.
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetAll(cc::RenderPassId(1, 1), gfx::Rect(viewport), clip,
                      gfx::Transform(), false);

  cc::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->quad_to_target_transform = transform;
  quad_state->quad_layer_bounds = frame_size;
  quad_state->visible_quad_layer_rect = gfx::Rect(frame_size);
  quad_state->opacity = 1.f;

  cc::SurfaceDrawQuad* surface_quad =
      render_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  surface_quad->SetNew(quad_state, gfx::Rect(quad_state->quad_layer_bounds),
                       gfx::Rect(quad_state->quad_layer_bounds), child_id);

  std::unique_ptr<cc::DelegatedFrameData> delegated_frame(
      new cc::DelegatedFrameData);
  delegated_frame->render_pass_list.push_back(std::move(render_pass));
  cc::CompositorFrame frame;
  frame.delegated_frame_data = std::move(delegated_frame);
  frame.metadata.referenced_surfaces = child_ids_;

  if (root_id_.is_null()) {
    root_id_ = surface_id_allocator_->GenerateId();
    surface_factory_->Create(root_id_);
    display_->SetSurfaceId(root_id_, 1.f);
  }
  surface_factory_->SubmitCompositorFrame(root_id_, std::move(frame),
                                          cc::SurfaceFactory::DrawCallback());

  output_surface_->UpdateStencilTest();
  display_->Resize(viewport);
  display_->SetExternalClip(clip);
  display_->DrawAndSwap();
}

void SurfacesInstance::AddChildId(const cc::SurfaceId& child_id) {
  DCHECK(std::find(child_ids_.begin(), child_ids_.end(), child_id) ==
         child_ids_.end());
  child_ids_.push_back(child_id);
  if (!root_id_.is_null())
    SetEmptyRootFrame();
}

void SurfacesInstance::RemoveChildId(const cc::SurfaceId& child_id) {
  auto itr = std::find(child_ids_.begin(), child_ids_.end(), child_id);
  DCHECK(itr != child_ids_.end());
  child_ids_.erase(itr);
  if (!root_id_.is_null())
    SetEmptyRootFrame();
}

void SurfacesInstance::SetEmptyRootFrame() {
  cc::CompositorFrame empty_frame;
  empty_frame.delegated_frame_data =
      base::WrapUnique(new cc::DelegatedFrameData);
  empty_frame.metadata.referenced_surfaces = child_ids_;
  surface_factory_->SubmitCompositorFrame(root_id_, std::move(empty_frame),
                                          cc::SurfaceFactory::DrawCallback());
}

void SurfacesInstance::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  // Root surface should have no resources to return.
  CHECK(resources.empty());
}

void SurfacesInstance::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // Parent compsitor calls DrawAndSwap directly and doesn't use
  // BeginFrameSource.
}

}  // namespace android_webview
