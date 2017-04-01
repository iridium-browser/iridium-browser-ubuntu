// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_host.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/time/default_tick_clock.h"
#include "cc/base/switches.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_hittest.h"
#include "cc/surfaces/surface_manager.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/resize_lock.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost

DelegatedFrameHost::DelegatedFrameHost(const cc::FrameSinkId& frame_sink_id,
                                       DelegatedFrameHostClient* client)
    : frame_sink_id_(frame_sink_id),
      client_(client),
      compositor_(nullptr),
      tick_clock_(new base::DefaultTickClock()),
      last_compositor_frame_sink_id_(0),
      pending_delegated_ack_count_(0),
      skipped_frames_(false),
      background_color_(SK_ColorRED),
      current_scale_factor_(1.f),
      can_lock_compositor_(YES_CAN_LOCK),
      delegated_frame_evictor_(new DelegatedFrameEvictor(this)) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->AddObserver(this);
  id_allocator_.reset(new cc::SurfaceIdAllocator());
  factory->GetContextFactoryPrivate()->GetSurfaceManager()->RegisterFrameSinkId(
      frame_sink_id_);
  factory->GetContextFactoryPrivate()
      ->GetSurfaceManager()
      ->RegisterSurfaceFactoryClient(frame_sink_id_, this);
  surface_factory_ = base::MakeUnique<cc::SurfaceFactory>(
      frame_sink_id_, factory->GetContextFactoryPrivate()->GetSurfaceManager(),
      this);
}

void DelegatedFrameHost::WasShown(const ui::LatencyInfo& latency_info) {
  delegated_frame_evictor_->SetVisible(true);

  if (!local_frame_id_.is_valid() && !released_front_lock_.get()) {
    if (compositor_)
      released_front_lock_ = compositor_->GetCompositorLock();
  }

  if (compositor_) {
    compositor_->SetLatencyInfo(latency_info);
  }
}

bool DelegatedFrameHost::HasSavedFrame() {
  return delegated_frame_evictor_->HasFrame();
}

void DelegatedFrameHost::WasHidden() {
  delegated_frame_evictor_->SetVisible(false);
  released_front_lock_ = NULL;
}

void DelegatedFrameHost::MaybeCreateResizeLock() {
  if (!ShouldCreateResizeLock())
    return;
  DCHECK(compositor_);

  bool defer_compositor_lock =
      can_lock_compositor_ == NO_PENDING_RENDERER_FRAME ||
      can_lock_compositor_ == NO_PENDING_COMMIT;

  if (can_lock_compositor_ == YES_CAN_LOCK)
    can_lock_compositor_ = YES_DID_LOCK;

  resize_lock_ =
      client_->DelegatedFrameHostCreateResizeLock(defer_compositor_lock);
}

bool DelegatedFrameHost::ShouldCreateResizeLock() {
  static const bool is_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableResizeLock);
  if (is_disabled)
    return false;

  if (!client_->DelegatedFrameCanCreateResizeLock())
    return false;

  if (resize_lock_)
    return false;

  gfx::Size desired_size = client_->DelegatedFrameHostDesiredSizeInDIP();
  if (desired_size == current_frame_size_in_dip_ || desired_size.IsEmpty())
    return false;

  if (!compositor_)
    return false;

  return true;
}

void DelegatedFrameHost::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& output_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  // Only ARGB888 and RGB565 supported as of now.
  bool format_support = ((preferred_color_type == kAlpha_8_SkColorType) ||
                         (preferred_color_type == kRGB_565_SkColorType) ||
                         (preferred_color_type == kN32_SkColorType));
  DCHECK(format_support);
  if (!CanCopyToBitmap()) {
    callback.Run(SkBitmap(), content::READBACK_SURFACE_UNAVAILABLE);
    return;
  }

  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(
          base::Bind(&CopyFromCompositingSurfaceHasResult, output_size,
                     preferred_color_type, callback));
  if (!src_subrect.IsEmpty())
    request->set_area(src_subrect);
  RequestCopyOfOutput(std::move(request));
}

void DelegatedFrameHost::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  if (!CanCopyToVideoFrame()) {
    callback.Run(gfx::Rect(), false);
    return;
  }

  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &DelegatedFrameHost::CopyFromCompositingSurfaceHasResultForVideo,
          AsWeakPtr(),  // For caching the ReadbackYUVInterface on this class.
          nullptr, target, callback));
  request->set_area(src_subrect);
  RequestCopyOfOutput(std::move(request));
}

bool DelegatedFrameHost::CanCopyToBitmap() const {
  return compositor_ &&
         client_->DelegatedFrameHostGetLayer()->has_external_content();
}

bool DelegatedFrameHost::CanCopyToVideoFrame() const {
  return compositor_ &&
         client_->DelegatedFrameHostGetLayer()->has_external_content();
}

void DelegatedFrameHost::BeginFrameSubscription(
    std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  frame_subscriber_ = std::move(subscriber);
}

void DelegatedFrameHost::EndFrameSubscription() {
  idle_frame_subscriber_textures_.clear();
  frame_subscriber_.reset();
}

cc::FrameSinkId DelegatedFrameHost::GetFrameSinkId() {
  return frame_sink_id_;
}

cc::SurfaceId DelegatedFrameHost::SurfaceIdAtPoint(
    cc::SurfaceHittestDelegate* delegate,
    const gfx::Point& point,
    gfx::Point* transformed_point) {
  cc::SurfaceId surface_id(frame_sink_id_, local_frame_id_);
  if (!surface_id.is_valid())
    return surface_id;
  cc::SurfaceHittest hittest(delegate, GetSurfaceManager());
  gfx::Transform target_transform;
  cc::SurfaceId target_local_frame_id =
      hittest.GetTargetSurfaceAtPoint(surface_id, point, &target_transform);
  *transformed_point = point;
  if (target_local_frame_id.is_valid())
    target_transform.TransformPoint(transformed_point);
  return target_local_frame_id;
}

bool DelegatedFrameHost::TransformPointToLocalCoordSpace(
    const gfx::Point& point,
    const cc::SurfaceId& original_surface,
    gfx::Point* transformed_point) {
  cc::SurfaceId surface_id(frame_sink_id_, local_frame_id_);
  if (!surface_id.is_valid())
    return false;
  *transformed_point = point;
  if (original_surface == surface_id)
    return true;

  cc::SurfaceHittest hittest(nullptr, GetSurfaceManager());
  return hittest.TransformPointToTargetSurface(original_surface, surface_id,
                                               transformed_point);
}

bool DelegatedFrameHost::TransformPointToCoordSpaceForView(
    const gfx::Point& point,
    RenderWidgetHostViewBase* target_view,
    gfx::Point* transformed_point) {
  if (!local_frame_id_.is_valid())
    return false;

  return target_view->TransformPointToLocalCoordSpace(
      point, cc::SurfaceId(frame_sink_id_, local_frame_id_), transformed_point);
}

bool DelegatedFrameHost::ShouldSkipFrame(gfx::Size size_in_dip) const {
  // Should skip a frame only when another frame from the renderer is guaranteed
  // to replace it. Otherwise may cause hangs when the renderer is waiting for
  // the completion of latency infos (such as when taking a Snapshot.)
  if (can_lock_compositor_ == NO_PENDING_RENDERER_FRAME ||
      can_lock_compositor_ == NO_PENDING_COMMIT || !resize_lock_.get())
    return false;

  return size_in_dip != resize_lock_->expected_size();
}

void DelegatedFrameHost::WasResized() {
  if (client_->DelegatedFrameHostDesiredSizeInDIP() !=
          current_frame_size_in_dip_ &&
      !client_->DelegatedFrameHostIsVisible())
    EvictDelegatedFrame();
  MaybeCreateResizeLock();
  UpdateGutters();
}

SkColor DelegatedFrameHost::GetGutterColor() const {
  // In fullscreen mode resizing is uncommon, so it makes more sense to
  // make the initial switch to fullscreen mode look better by using black as
  // the gutter color.
  return client_->DelegatedFrameHostGetGutterColor(background_color_);
}

void DelegatedFrameHost::UpdateGutters() {
  if (!local_frame_id_.is_valid()) {
    right_gutter_.reset();
    bottom_gutter_.reset();
    return;
  }

  if (current_frame_size_in_dip_.width() <
      client_->DelegatedFrameHostDesiredSizeInDIP().width()) {
    right_gutter_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    right_gutter_->SetColor(GetGutterColor());
    int width = client_->DelegatedFrameHostDesiredSizeInDIP().width() -
                current_frame_size_in_dip_.width();
    // The right gutter also includes the bottom-right corner, if necessary.
    int height = client_->DelegatedFrameHostDesiredSizeInDIP().height();
    right_gutter_->SetBounds(
        gfx::Rect(current_frame_size_in_dip_.width(), 0, width, height));

    client_->DelegatedFrameHostGetLayer()->Add(right_gutter_.get());
  } else {
    right_gutter_.reset();
  }

  if (current_frame_size_in_dip_.height() <
      client_->DelegatedFrameHostDesiredSizeInDIP().height()) {
    bottom_gutter_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    bottom_gutter_->SetColor(GetGutterColor());
    int width = current_frame_size_in_dip_.width();
    int height = client_->DelegatedFrameHostDesiredSizeInDIP().height() -
                 current_frame_size_in_dip_.height();
    bottom_gutter_->SetBounds(
        gfx::Rect(0, current_frame_size_in_dip_.height(), width, height));
    client_->DelegatedFrameHostGetLayer()->Add(bottom_gutter_.get());

  } else {
    bottom_gutter_.reset();
  }
}

gfx::Size DelegatedFrameHost::GetRequestedRendererSize() const {
  if (resize_lock_)
    return resize_lock_->expected_size();
  else
    return client_->DelegatedFrameHostDesiredSizeInDIP();
}

void DelegatedFrameHost::CheckResizeLock() {
  if (!resize_lock_ ||
      resize_lock_->expected_size() != current_frame_size_in_dip_)
    return;

  // Since we got the size we were looking for, unlock the compositor. But delay
  // the release of the lock until we've kicked a frame with the new texture, to
  // avoid resizing the UI before we have a chance to draw a "good" frame.
  resize_lock_->UnlockCompositor();
}

void DelegatedFrameHost::AttemptFrameSubscriberCapture(
    const gfx::Rect& damage_rect) {
  if (!frame_subscriber() || !CanCopyToVideoFrame())
    return;

  const base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeTicks present_time;
  if (vsync_interval_ <= base::TimeDelta()) {
    present_time = now;
  } else {
    const int64_t intervals_elapsed = (now - vsync_timebase_) / vsync_interval_;
    present_time = vsync_timebase_ + (intervals_elapsed + 1) * vsync_interval_;
  }

  scoped_refptr<media::VideoFrame> frame;
  RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback callback;
  if (!frame_subscriber()->ShouldCaptureFrame(damage_rect, present_time, &frame,
                                              &callback))
    return;

  // Get a texture to re-use; else, create a new one.
  scoped_refptr<OwnedMailbox> subscriber_texture;
  if (!idle_frame_subscriber_textures_.empty()) {
    subscriber_texture = idle_frame_subscriber_textures_.back();
    idle_frame_subscriber_textures_.pop_back();
  } else if (display_compositor::GLHelper* helper =
                 ImageTransportFactory::GetInstance()->GetGLHelper()) {
    subscriber_texture = new OwnedMailbox(helper);
  }

  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateRequest(base::Bind(
          &DelegatedFrameHost::CopyFromCompositingSurfaceHasResultForVideo,
          AsWeakPtr(), subscriber_texture, frame,
          base::Bind(callback, present_time)));
  // Setting the source in this copy request asks that the layer abort any prior
  // uncommitted copy requests made on behalf of the same frame subscriber.
  // This will not affect any of the copy requests spawned elsewhere from
  // DelegatedFrameHost (e.g., a call to CopyFromCompositingSurface() for
  // screenshots) since those copy requests do not specify |frame_subscriber()|
  // as a source.
  request->set_source(frame_subscriber());
  if (subscriber_texture.get()) {
    request->SetTextureMailbox(cc::TextureMailbox(
        subscriber_texture->mailbox(), subscriber_texture->sync_token(),
        subscriber_texture->target()));
  }

  if (local_frame_id_.is_valid()) {
    // To avoid unnecessary composites, go directly to the Surface rather than
    // through RequestCopyOfOutput (which goes through the browser
    // compositor).
    if (!request_copy_of_output_callback_for_testing_.is_null())
      request_copy_of_output_callback_for_testing_.Run(std::move(request));
    else
      surface_factory_->RequestCopyOfSurface(std::move(request));
  } else {
    request->set_area(gfx::Rect(current_frame_size_in_dip_));
    RequestCopyOfOutput(std::move(request));
  }
}

void DelegatedFrameHost::SwapDelegatedFrame(uint32_t compositor_frame_sink_id,
                                            cc::CompositorFrame frame) {
#if defined(OS_CHROMEOS)
  DCHECK(!resize_lock_ || !client_->IsAutoResizeEnabled());
#endif
  float frame_device_scale_factor = frame.metadata.device_scale_factor;

  DCHECK(!frame.render_pass_list.empty());

  cc::RenderPass* root_pass = frame.render_pass_list.back().get();

  gfx::Size frame_size = root_pass->output_rect.size();
  gfx::Size frame_size_in_dip =
      gfx::ConvertSizeToDIP(frame_device_scale_factor, frame_size);

  gfx::Rect damage_rect = root_pass->damage_rect;
  damage_rect.Intersect(gfx::Rect(frame_size));
  gfx::Rect damage_rect_in_dip =
      gfx::ConvertRectToDIP(frame_device_scale_factor, damage_rect);

  if (ShouldSkipFrame(frame_size_in_dip)) {
    cc::ReturnedResourceArray resources;
    cc::TransferableResource::ReturnResources(frame.resource_list, &resources);

    skipped_latency_info_list_.insert(skipped_latency_info_list_.end(),
                                      frame.metadata.latency_info.begin(),
                                      frame.metadata.latency_info.end());

    client_->DelegatedFrameHostSendReclaimCompositorResources(
        compositor_frame_sink_id, true /* is_swap_ack*/, resources);
    skipped_frames_ = true;
    return;
  }

  if (skipped_frames_) {
    skipped_frames_ = false;
    damage_rect = gfx::Rect(frame_size);
    damage_rect_in_dip = gfx::Rect(frame_size_in_dip);

    // Give the same damage rect to the compositor.
    cc::RenderPass* root_pass = frame.render_pass_list.back().get();
    root_pass->damage_rect = damage_rect;
  }

  if (compositor_frame_sink_id != last_compositor_frame_sink_id_) {
    // Resource ids are scoped by the output surface.
    // If the originating output surface doesn't match the last one, it
    // indicates the renderer's output surface may have been recreated, in which
    // case we should recreate the DelegatedRendererLayer, to avoid matching
    // resources from the old one with resources from the new one which would
    // have the same id. Changing the layer to showing painted content destroys
    // the DelegatedRendererLayer.
    EvictDelegatedFrame();

    surface_factory_->Reset();
    if (!surface_returned_resources_.empty()) {
      SendReclaimCompositorResources(last_compositor_frame_sink_id_,
                                     false /* is_swap_ack */);
    }
    last_compositor_frame_sink_id_ = compositor_frame_sink_id;
  }
  bool skip_frame = false;
  pending_delegated_ack_count_++;

  background_color_ = frame.metadata.root_background_color;

  bool did_send_ack_callback = false;
  if (frame_size.IsEmpty()) {
    DCHECK(frame.resource_list.empty());
    EvictDelegatedFrame();
  } else {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    cc::SurfaceManager* manager =
        factory->GetContextFactoryPrivate()->GetSurfaceManager();
    bool allocated_new_local_frame_id = false;
    if (!local_frame_id_.is_valid() || frame_size != current_surface_size_ ||
        frame_size_in_dip != current_frame_size_in_dip_) {
      local_frame_id_ = id_allocator_->GenerateId();
      allocated_new_local_frame_id = true;
    }

    gfx::Size desired_size = client_->DelegatedFrameHostDesiredSizeInDIP();
    if (desired_size != frame_size_in_dip && !desired_size.IsEmpty()) {
      skip_frame = true;
      skipped_latency_info_list_.insert(skipped_latency_info_list_.end(),
                                        frame.metadata.latency_info.begin(),
                                        frame.metadata.latency_info.end());
      frame.metadata.latency_info.clear();
    } else {
      frame.metadata.latency_info.insert(frame.metadata.latency_info.end(),
                                         skipped_latency_info_list_.begin(),
                                         skipped_latency_info_list_.end());
      skipped_latency_info_list_.clear();
    }

    cc::SurfaceFactory::DrawCallback ack_callback;
    if (compositor_ && !skip_frame) {
      ack_callback = base::Bind(&DelegatedFrameHost::SurfaceDrawn, AsWeakPtr(),
                                compositor_frame_sink_id);
      did_send_ack_callback = true;
    }
    surface_factory_->SubmitCompositorFrame(local_frame_id_, std::move(frame),
                                            ack_callback);
    if (allocated_new_local_frame_id) {
      // manager must outlive compositors using it.
      cc::SurfaceId surface_id(frame_sink_id_, local_frame_id_);
      cc::SurfaceInfo surface_info(surface_id, frame_device_scale_factor,
                                   frame_size);
      client_->DelegatedFrameHostGetLayer()->SetShowSurface(
          surface_info, manager->reference_factory());
      current_surface_size_ = frame_size;
      current_scale_factor_ = frame_device_scale_factor;
    }
  }
  released_front_lock_ = NULL;
  current_frame_size_in_dip_ = frame_size_in_dip;
  CheckResizeLock();

  UpdateGutters();

  if (!damage_rect_in_dip.IsEmpty()) {
    client_->DelegatedFrameHostGetLayer()->OnDelegatedFrameDamage(
        damage_rect_in_dip);
  }

  // Note that |compositor_| may be reset by SetShowSurface or
  // SetShowDelegatedContent above.
  if (!compositor_ || skip_frame) {
    SendReclaimCompositorResources(compositor_frame_sink_id,
                                   !did_send_ack_callback /* is_swap_ack */);
  } else {
    can_lock_compositor_ = NO_PENDING_COMMIT;
  }
  if (local_frame_id_.is_valid()) {
    delegated_frame_evictor_->SwappedFrame(
        client_->DelegatedFrameHostIsVisible());
  }
  // Note: the frame may have been evicted immediately.
}

void DelegatedFrameHost::ClearDelegatedFrame() {
  if (local_frame_id_.is_valid())
    EvictDelegatedFrame();
}

void DelegatedFrameHost::SendReclaimCompositorResources(
    uint32_t compositor_frame_sink_id,
    bool is_swap_ack) {
  client_->DelegatedFrameHostSendReclaimCompositorResources(
      compositor_frame_sink_id, is_swap_ack, surface_returned_resources_);
  surface_returned_resources_.clear();
  if (is_swap_ack) {
    DCHECK_GT(pending_delegated_ack_count_, 0);
    pending_delegated_ack_count_--;
  }
}

void DelegatedFrameHost::SurfaceDrawn(uint32_t compositor_frame_sink_id) {
  SendReclaimCompositorResources(compositor_frame_sink_id,
                                 true /* is_swap_ack */);
}

void DelegatedFrameHost::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;
  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
  if (!pending_delegated_ack_count_) {
    SendReclaimCompositorResources(last_compositor_frame_sink_id_,
                                   false /* is_swap_ack */);
  }
}

void DelegatedFrameHost::WillDrawSurface(const cc::LocalFrameId& id,
                                         const gfx::Rect& damage_rect) {
  // Frame subscribers are only interested in changes to the target surface, so
  // do not attempt capture if |damage_rect| is empty.  This prevents the draws
  // of parent surfaces from triggering extra frame captures, which can affect
  // smoothness.
  if (id != local_frame_id_ || damage_rect.IsEmpty())
    return;
  AttemptFrameSubscriberCapture(damage_rect);
}

void DelegatedFrameHost::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  client_->SetBeginFrameSource(begin_frame_source);
}

void DelegatedFrameHost::EvictDelegatedFrame() {
  client_->DelegatedFrameHostGetLayer()->SetShowSolidColorContent();
  if (local_frame_id_.is_valid()) {
    surface_factory_->EvictSurface();
    local_frame_id_ = cc::LocalFrameId();
  }
  delegated_frame_evictor_->DiscardedFrame();
  UpdateGutters();
}

// static
void DelegatedFrameHost::ReturnSubscriberTexture(
    base::WeakPtr<DelegatedFrameHost> dfh,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    const gpu::SyncToken& sync_token) {
  if (!subscriber_texture.get())
    return;
  if (!dfh)
    return;

  subscriber_texture->UpdateSyncToken(sync_token);

  if (dfh->frame_subscriber_ && subscriber_texture->texture_id())
    dfh->idle_frame_subscriber_textures_.push_back(subscriber_texture);
}

// static
void DelegatedFrameHost::CopyFromCompositingSurfaceFinishedForVideo(
    scoped_refptr<media::VideoFrame> video_frame,
    base::WeakPtr<DelegatedFrameHost> dfh,
    const base::Callback<void(bool)>& callback,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    std::unique_ptr<cc::SingleReleaseCallback> release_callback,
    bool result) {
  callback.Run(result);

  gpu::SyncToken sync_token;
  if (result) {
    display_compositor::GLHelper* gl_helper =
        ImageTransportFactory::GetInstance()->GetGLHelper();
    gl_helper->GenerateSyncToken(&sync_token);
  }
  if (release_callback) {
    // A release callback means the texture came from the compositor, so there
    // should be no |subscriber_texture|.
    DCHECK(!subscriber_texture.get());
    const bool lost_resource = !sync_token.HasData();
    release_callback->Run(sync_token, lost_resource);
  }
  ReturnSubscriberTexture(dfh, subscriber_texture, sync_token);
}

// static
void DelegatedFrameHost::CopyFromCompositingSurfaceHasResultForVideo(
    base::WeakPtr<DelegatedFrameHost> dfh,
    scoped_refptr<OwnedMailbox> subscriber_texture,
    scoped_refptr<media::VideoFrame> video_frame,
    const base::Callback<void(const gfx::Rect&, bool)>& callback,
    std::unique_ptr<cc::CopyOutputResult> result) {
  base::ScopedClosureRunner scoped_callback_runner(
      base::Bind(callback, gfx::Rect(), false));
  base::ScopedClosureRunner scoped_return_subscriber_texture(base::Bind(
      &ReturnSubscriberTexture, dfh, subscriber_texture, gpu::SyncToken()));

  if (!dfh)
    return;
  if (result->IsEmpty())
    return;
  if (result->size().IsEmpty())
    return;

  // Compute the dest size we want after the letterboxing resize. Make the
  // coordinates and sizes even because we letterbox in YUV space
  // (see CopyRGBToVideoFrame). They need to be even for the UV samples to
  // line up correctly.
  // The video frame's visible_rect() and the result's size() are both physical
  // pixels.
  gfx::Rect region_in_frame = media::ComputeLetterboxRegion(
      video_frame->visible_rect(), result->size());
  region_in_frame =
      gfx::Rect(region_in_frame.x() & ~1, region_in_frame.y() & ~1,
                region_in_frame.width() & ~1, region_in_frame.height() & ~1);
  if (region_in_frame.IsEmpty())
    return;

  if (!result->HasTexture()) {
    DCHECK(result->HasBitmap());
    std::unique_ptr<SkBitmap> bitmap = result->TakeBitmap();
    // Scale the bitmap to the required size, if necessary.
    SkBitmap scaled_bitmap;
    if (result->size() != region_in_frame.size()) {
      skia::ImageOperations::ResizeMethod method =
          skia::ImageOperations::RESIZE_GOOD;
      scaled_bitmap = skia::ImageOperations::Resize(*bitmap.get(), method,
                                                    region_in_frame.width(),
                                                    region_in_frame.height());
    } else {
      scaled_bitmap = *bitmap.get();
    }

    {
      SkAutoLockPixels scaled_bitmap_locker(scaled_bitmap);

      media::CopyRGBToVideoFrame(
          reinterpret_cast<uint8_t*>(scaled_bitmap.getPixels()),
          scaled_bitmap.rowBytes(), region_in_frame, video_frame.get());
    }
    ignore_result(scoped_callback_runner.Release());
    callback.Run(region_in_frame, true);
    return;
  }

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  display_compositor::GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;
  if (subscriber_texture.get() && !subscriber_texture->texture_id())
    return;

  cc::TextureMailbox texture_mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> release_callback;
  result->TakeTexture(&texture_mailbox, &release_callback);
  DCHECK(texture_mailbox.IsTexture());

  gfx::Rect result_rect(result->size());

  display_compositor::ReadbackYUVInterface* yuv_readback_pipeline =
      dfh->yuv_readback_pipeline_.get();
  if (yuv_readback_pipeline == NULL ||
      yuv_readback_pipeline->scaler()->SrcSize() != result_rect.size() ||
      yuv_readback_pipeline->scaler()->SrcSubrect() != result_rect ||
      yuv_readback_pipeline->scaler()->DstSize() != region_in_frame.size()) {
    // The scaler chosen here is based on performance measurements of full
    // end-to-end systems.  When down-scaling, always use the "fast" scaler
    // because it performs well on both low- and high- end machines, provides
    // decent image quality, and doesn't overwhelm downstream video encoders
    // with too much entropy (which can drastically increase CPU utilization).
    // When up-scaling, always use "best" because the quality improvement is
    // huge with insignificant performance penalty.  Note that this strategy
    // differs from single-frame snapshot capture.
    display_compositor::GLHelper::ScalerQuality quality =
        ((result_rect.size().width() < region_in_frame.size().width()) &&
         (result_rect.size().height() < region_in_frame.size().height()))
            ? display_compositor::GLHelper::SCALER_QUALITY_BEST
            : display_compositor::GLHelper::SCALER_QUALITY_FAST;

    dfh->yuv_readback_pipeline_.reset(gl_helper->CreateReadbackPipelineYUV(
        quality, result_rect.size(), result_rect, region_in_frame.size(), true,
        true));
    yuv_readback_pipeline = dfh->yuv_readback_pipeline_.get();
  }

  ignore_result(scoped_callback_runner.Release());
  ignore_result(scoped_return_subscriber_texture.Release());

  base::Callback<void(bool result)> finished_callback = base::Bind(
      &DelegatedFrameHost::CopyFromCompositingSurfaceFinishedForVideo,
      video_frame, dfh->AsWeakPtr(), base::Bind(callback, region_in_frame),
      subscriber_texture, base::Passed(&release_callback));
  yuv_readback_pipeline->ReadbackYUV(
      texture_mailbox.mailbox(), texture_mailbox.sync_token(),
      video_frame->visible_rect(),
      video_frame->stride(media::VideoFrame::kYPlane),
      video_frame->data(media::VideoFrame::kYPlane),
      video_frame->stride(media::VideoFrame::kUPlane),
      video_frame->data(media::VideoFrame::kUPlane),
      video_frame->stride(media::VideoFrame::kVPlane),
      video_frame->data(media::VideoFrame::kVPlane), region_in_frame.origin(),
      finished_callback);
  media::LetterboxYUV(video_frame.get(), region_in_frame);
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ui::CompositorObserver implementation:

void DelegatedFrameHost::OnCompositingDidCommit(ui::Compositor* compositor) {
  if (can_lock_compositor_ == NO_PENDING_COMMIT) {
    can_lock_compositor_ = YES_CAN_LOCK;
    if (resize_lock_.get() && resize_lock_->GrabDeferredLock())
      can_lock_compositor_ = YES_DID_LOCK;
  }
  if (resize_lock_ &&
      resize_lock_->expected_size() == current_frame_size_in_dip_) {
    resize_lock_.reset();
    client_->DelegatedFrameHostResizeLockWasReleased();
    // We may have had a resize while we had the lock (e.g. if the lock expired,
    // or if the UI still gave us some resizes), so make sure we grab a new lock
    // if necessary.
    MaybeCreateResizeLock();
  }
}

void DelegatedFrameHost::OnCompositingStarted(ui::Compositor* compositor,
                                              base::TimeTicks start_time) {
  last_draw_ended_ = start_time;
}

void DelegatedFrameHost::OnCompositingEnded(ui::Compositor* compositor) {}

void DelegatedFrameHost::OnCompositingLockStateChanged(
    ui::Compositor* compositor) {
  // A compositor lock that is part of a resize lock timed out. We
  // should display a renderer frame.
  if (!compositor->IsLocked() && can_lock_compositor_ == YES_DID_LOCK) {
    can_lock_compositor_ = NO_PENDING_RENDERER_FRAME;
  }
}

void DelegatedFrameHost::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK_EQ(compositor, compositor_);
  ResetCompositor();
  DCHECK(!compositor_);
}

void DelegatedFrameHost::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, ImageTransportFactoryObserver implementation:

void DelegatedFrameHost::OnLostResources() {
  if (local_frame_id_.is_valid())
    EvictDelegatedFrame();
  idle_frame_subscriber_textures_.clear();
  yuv_readback_pipeline_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, private:

DelegatedFrameHost::~DelegatedFrameHost() {
  DCHECK(!compositor_);
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->GetContextFactory()->RemoveObserver(this);
  surface_factory_->EvictSurface();
  factory->GetContextFactoryPrivate()
      ->GetSurfaceManager()
      ->UnregisterSurfaceFactoryClient(frame_sink_id_);
  factory->GetContextFactoryPrivate()
      ->GetSurfaceManager()
      ->InvalidateFrameSinkId(frame_sink_id_);

  DCHECK(!vsync_manager_.get());
}

void DelegatedFrameHost::SetCompositor(ui::Compositor* compositor) {
  DCHECK(!compositor_);
  if (!compositor)
    return;
  compositor_ = compositor;
  compositor_->AddObserver(this);
  DCHECK(!vsync_manager_.get());
  vsync_manager_ = compositor_->vsync_manager();
  vsync_manager_->AddObserver(this);

  compositor_->AddFrameSink(frame_sink_id_);
}

void DelegatedFrameHost::ResetCompositor() {
  if (!compositor_)
    return;
  if (resize_lock_) {
    resize_lock_.reset();
    client_->DelegatedFrameHostResizeLockWasReleased();
  }
  if (compositor_->HasObserver(this))
    compositor_->RemoveObserver(this);
  if (vsync_manager_) {
    vsync_manager_->RemoveObserver(this);
    vsync_manager_ = nullptr;
  }

  compositor_->RemoveFrameSink(frame_sink_id_);
  compositor_ = nullptr;
}

void DelegatedFrameHost::LockResources() {
  DCHECK(local_frame_id_.is_valid());
  delegated_frame_evictor_->LockFrame();
}

void DelegatedFrameHost::RequestCopyOfOutput(
    std::unique_ptr<cc::CopyOutputRequest> request) {
  if (!request_copy_of_output_callback_for_testing_.is_null()) {
    request_copy_of_output_callback_for_testing_.Run(std::move(request));
  } else {
    client_->DelegatedFrameHostGetLayer()->RequestCopyOfOutput(
        std::move(request));
  }
}

void DelegatedFrameHost::UnlockResources() {
  DCHECK(local_frame_id_.is_valid());
  delegated_frame_evictor_->UnlockFrame();
}

}  // namespace content
