// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/gles2/command_buffer_local.h"

#include "base/bind.h"
#include "components/view_manager/gles2/command_buffer_local_client.h"
#include "components/view_manager/gles2/gpu_memory_tracker.h"
#include "components/view_manager/gles2/mojo_gpu_memory_buffer.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_memory.h"
#include "ui/gl/gl_surface.h"

namespace gles2 {

const unsigned int GL_MAP_CHROMIUM = 0x78F1;

CommandBufferLocal::CommandBufferLocal(
    CommandBufferLocalClient* client,
    gfx::AcceleratedWidget widget,
    const scoped_refptr<gles2::GpuState>& gpu_state)
    : widget_(widget),
      gpu_state_(gpu_state),
      client_(client),
      weak_factory_(this) {
}

CommandBufferLocal::~CommandBufferLocal() {
  command_buffer_.reset();
  if (decoder_.get()) {
    bool have_context = decoder_->GetGLContext()->MakeCurrent(surface_.get());
    decoder_->Destroy(have_context);
    decoder_.reset();
  }
}

bool CommandBufferLocal::Initialize() {
  if (widget_ == gfx::kNullAcceleratedWidget) {
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(1, 1));
  } else {
    surface_ = gfx::GLSurface::CreateViewGLSurface(widget_);
    gfx::VSyncProvider* vsync_provider =
        surface_ ? surface_->GetVSyncProvider() : nullptr;
    if (vsync_provider) {
      vsync_provider->GetVSyncParameters(
          base::Bind(&CommandBufferLocal::OnUpdateVSyncParameters,
                     weak_factory_.GetWeakPtr()));
    }
  }

  if (!surface_.get())
    return false;

  // TODO(piman): virtual contexts, gpu preference.
  context_ = gfx::GLContext::CreateGLContext(
      gpu_state_->share_group(), surface_.get(), gfx::PreferIntegratedGpu);
  if (!context_.get())
    return false;

  if (!context_->MakeCurrent(surface_.get()))
    return false;

  // TODO(piman): ShaderTranslatorCache is currently per-ContextGroup but
  // only needs to be per-thread.
  bool bind_generates_resource = false;
  scoped_refptr<gpu::gles2::ContextGroup> context_group =
      new gpu::gles2::ContextGroup(
          gpu_state_->mailbox_manager(), new gles2::GpuMemoryTracker,
          new gpu::gles2::ShaderTranslatorCache,
          new gpu::gles2::FramebufferCompletenessCache, nullptr, nullptr,
          nullptr, bind_generates_resource);

  command_buffer_.reset(
      new gpu::CommandBufferService(context_group->transfer_buffer_manager()));
  bool result = command_buffer_->Initialize();
  DCHECK(result);

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group.get()));
  scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(), decoder_.get(),
                                         decoder_.get()));
  decoder_->set_engine(scheduler_.get());
  decoder_->SetResizeCallback(
      base::Bind(&CommandBufferLocal::OnResize, base::Unretained(this)));
  decoder_->SetWaitSyncPointCallback(base::Bind(
      &CommandBufferLocal::OnWaitSyncPoint, base::Unretained(this)));

  gpu::gles2::DisallowedFeatures disallowed_features;

  // TODO(piman): attributes.
  std::vector<int32> attrib_vector;
  if (!decoder_->Initialize(surface_, context_, false /* offscreen */,
                            gfx::Size(1, 1), disallowed_features,
                            attrib_vector))
    return false;

  command_buffer_->SetPutOffsetChangeCallback(base::Bind(
      &CommandBufferLocal::PumpCommands, base::Unretained(this)));
  command_buffer_->SetGetBufferChangeCallback(base::Bind(
      &gpu::GpuScheduler::SetGetBuffer, base::Unretained(scheduler_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&CommandBufferLocal::OnParseError, base::Unretained(this)));
  return true;
}

gpu::CommandBuffer* CommandBufferLocal::GetCommandBuffer() {
  return command_buffer_.get();
}

/******************************************************************************/
// gpu::GpuControl:
/******************************************************************************/

gpu::Capabilities CommandBufferLocal::GetCapabilities() {
  return decoder_->GetCapabilities();
}

int32_t CommandBufferLocal::CreateImage(ClientBuffer buffer,
                                        size_t width,
                                        size_t height,
                                        unsigned internalformat) {
  gles2::MojoGpuMemoryBufferImpl* gpu_memory_buffer =
      gles2::MojoGpuMemoryBufferImpl::FromClientBuffer(buffer);

  scoped_refptr<gfx::GLImageMemory> image(new gfx::GLImageMemory(
      gfx::Size(static_cast<int>(width), static_cast<int>(height)),
      internalformat));
  if (!image->Initialize(
          static_cast<const unsigned char*>(gpu_memory_buffer->GetMemory()),
          gpu_memory_buffer->GetFormat())) {
    return -1;
  }

  static int32 next_id = 1;
  int32 new_id = next_id++;

  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  image_manager->AddImage(image.get(), new_id);
  return new_id;
}

void CommandBufferLocal::DestroyImage(int32 id) {
  gpu::gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  image_manager->RemoveImage(id);
}

int32_t CommandBufferLocal::CreateGpuMemoryBufferImage(
    size_t width,
    size_t height,
    unsigned internalformat,
    unsigned usage) {
  DCHECK_EQ(usage, static_cast<unsigned>(GL_MAP_CHROMIUM));
  scoped_ptr<gfx::GpuMemoryBuffer> buffer(
      gles2::MojoGpuMemoryBufferImpl::Create(
          gfx::Size(static_cast<int>(width), static_cast<int>(height)),
          gpu::ImageFactory::DefaultBufferFormatForImageFormat(internalformat),
          gpu::ImageFactory::ImageUsageToGpuMemoryBufferUsage(usage)));
  if (!buffer)
    return -1;
  return CreateImage(buffer->AsClientBuffer(), width, height, internalformat);
}

uint32_t CommandBufferLocal::InsertSyncPoint() {
  return 0;
}

uint32_t CommandBufferLocal::InsertFutureSyncPoint() {
  NOTIMPLEMENTED();
  return 0;
}

void CommandBufferLocal::RetireSyncPoint(uint32_t sync_point) {
  NOTIMPLEMENTED();
}

void CommandBufferLocal::SignalSyncPoint(uint32_t sync_point,
                                              const base::Closure& callback) {
}

void CommandBufferLocal::SignalQuery(uint32_t query,
                                     const base::Closure& callback) {
  // TODO(piman)
  NOTIMPLEMENTED();
}

void CommandBufferLocal::SetSurfaceVisible(bool visible) {
  // TODO(piman)
  NOTIMPLEMENTED();
}

uint32_t CommandBufferLocal::CreateStreamTexture(uint32_t texture_id) {
  // TODO(piman)
  NOTIMPLEMENTED();
  return 0;
}

void CommandBufferLocal::SetLock(base::Lock* lock) {
  NOTIMPLEMENTED();
}

bool CommandBufferLocal::IsGpuChannelLost() {
  // This is only possible for out-of-process command buffers.
  return false;
}

void CommandBufferLocal::PumpCommands() {
  if (!decoder_->MakeCurrent()) {
    command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
    command_buffer_->SetParseError(::gpu::error::kLostContext);
    return;
  }
  scheduler_->PutChanged();
}

void CommandBufferLocal::OnResize(gfx::Size size, float scale_factor) {
  surface_->Resize(size);
}

void CommandBufferLocal::OnUpdateVSyncParameters(
    const base::TimeTicks timebase,
    const base::TimeDelta interval) {
  if (client_)
    client_->UpdateVSyncParameters(timebase.ToInternalValue(),
                                   interval.ToInternalValue());

}

bool CommandBufferLocal::OnWaitSyncPoint(uint32_t sync_point) {
  if (!sync_point)
    return true;
  if (gpu_state_->sync_point_manager()->IsSyncPointRetired(sync_point))
    return true;
  scheduler_->SetScheduled(false);
  gpu_state_->sync_point_manager()->AddSyncPointCallback(
      sync_point, base::Bind(&CommandBufferLocal::OnSyncPointRetired,
                             weak_factory_.GetWeakPtr()));
  return scheduler_->IsScheduled();
}

void CommandBufferLocal::OnParseError() {
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  OnContextLost(state.context_lost_reason);
}

void CommandBufferLocal::OnContextLost(uint32_t reason) {
  if (client_)
    client_->DidLoseContext();
}

void CommandBufferLocal::OnSyncPointRetired() {
  scheduler_->SetScheduled(true);
}

}  // namespace gles2
