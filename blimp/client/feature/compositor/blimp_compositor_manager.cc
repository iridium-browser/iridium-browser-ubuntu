// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_compositor_manager.h"

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/feature/compositor/blimp_gpu_memory_buffer_manager.h"
#include "blimp/client/feature/compositor/blimp_layer_tree_settings.h"
#include "blimp/common/compositor/blimp_task_graph_runner.h"
#include "cc/proto/compositor_message.pb.h"

namespace blimp {
namespace client {

namespace {
base::LazyInstance<blimp::BlimpTaskGraphRunner> g_task_graph_runner =
    LAZY_INSTANCE_INITIALIZER;

const int kDummyTabId = 0;
}  // namespace

BlimpCompositorManager::BlimpCompositorManager(
    RenderWidgetFeature* render_widget_feature,
    cc::SurfaceManager* surface_manager,
    BlimpGpuMemoryBufferManager* gpu_memory_buffer_manager,
    SurfaceIdAllocationCallback callback)
    : render_widget_feature_(render_widget_feature),
      surface_manager_(surface_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      surface_id_allocation_callback_(callback),
      visible_(false),
      layer_(cc::Layer::Create()),
      active_compositor_(nullptr) {
  DCHECK(render_widget_feature_);
  DCHECK(surface_manager_);
  DCHECK(gpu_memory_buffer_manager_);
  DCHECK(!surface_id_allocation_callback_.is_null());

  render_widget_feature_->SetDelegate(kDummyTabId, this);
}

BlimpCompositorManager::~BlimpCompositorManager() {
  render_widget_feature_->RemoveDelegate(kDummyTabId);
  if (compositor_thread_)
    compositor_thread_->Stop();
}

void BlimpCompositorManager::SetVisible(bool visible) {
  visible_ = visible;
  if (active_compositor_)
    active_compositor_->SetVisible(visible_);
}

bool BlimpCompositorManager::OnTouchEvent(const ui::MotionEvent& motion_event) {
  if (active_compositor_)
    return active_compositor_->OnTouchEvent(motion_event);
  return false;
}

void BlimpCompositorManager::GenerateLayerTreeSettings(
    cc::LayerTreeSettings* settings) {
  PopulateCommonLayerTreeSettings(settings);
}

std::unique_ptr<BlimpCompositor> BlimpCompositorManager::CreateBlimpCompositor(
    int render_widget_id,
    cc::SurfaceManager* surface_manager,
    uint32_t surface_client_id,
    BlimpCompositorClient* client) {
  return base::MakeUnique<BlimpCompositor>(render_widget_id, surface_manager,
                                           surface_client_id, client);
}

void BlimpCompositorManager::OnRenderWidgetCreated(int render_widget_id) {
  CHECK(!GetCompositor(render_widget_id));

  compositors_[render_widget_id] =
      CreateBlimpCompositor(render_widget_id, surface_manager_,
                            surface_id_allocation_callback_.Run(), this);
}

void BlimpCompositorManager::OnRenderWidgetInitialized(int render_widget_id) {
  if (active_compositor_ &&
      active_compositor_->render_widget_id() == render_widget_id)
    return;

  // Detach the content layer from the old compositor.
  layer_->RemoveAllChildren();

  if (active_compositor_) {
    VLOG(1) << "Hiding currently active compositor for render widget: "
            << active_compositor_->render_widget_id();
    active_compositor_->SetVisible(false);
  }

  active_compositor_ = GetCompositor(render_widget_id);
  CHECK(active_compositor_);

  active_compositor_->SetVisible(visible_);
  layer_->AddChild(active_compositor_->layer());
}

void BlimpCompositorManager::OnRenderWidgetDeleted(int render_widget_id) {
  CompositorMap::const_iterator it = compositors_.find(render_widget_id);
  CHECK(it != compositors_.end());

  // Reset the |active_compositor_| if that is what we're destroying right now.
  if (active_compositor_ == it->second.get()) {
    layer_->RemoveAllChildren();
    active_compositor_ = nullptr;
  }

  compositors_.erase(it);
}

void BlimpCompositorManager::OnCompositorMessageReceived(
    int render_widget_id,
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  BlimpCompositor* compositor = GetCompositor(render_widget_id);
  CHECK(compositor);

  compositor->OnCompositorMessageReceived(std::move(message));
}

cc::LayerTreeSettings* BlimpCompositorManager::GetLayerTreeSettings() {
  if (!settings_) {
    settings_.reset(new cc::LayerTreeSettings);

    // TODO(khushalsagar): The server should selectively send only those
    // LayerTreeSettings which should remain consistent across the server and
    // client. Since it currently overrides all settings, ignore them.
    // See crbug/577985.
    GenerateLayerTreeSettings(settings_.get());
    settings_
      ->abort_commit_before_output_surface_creation = false;
    settings_->renderer_settings.buffer_to_texture_target_map =
        BlimpGpuMemoryBufferManager::GetDefaultBufferToTextureTargetMap();
    settings_->use_output_surface_begin_frame_source = true;
  }

  return settings_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
BlimpCompositorManager::GetCompositorTaskRunner() {
  if (compositor_thread_)
    return compositor_thread_->task_runner();

  base::Thread::Options thread_options;
#if defined(OS_ANDROID)
  thread_options.priority = base::ThreadPriority::DISPLAY;
#endif
  compositor_thread_.reset(new base::Thread("Compositor"));
  compositor_thread_->StartWithOptions(thread_options);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      compositor_thread_->task_runner();
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed),
                 false));
  // TODO(dtrainor): Determine whether or not we can disallow waiting.

  return task_runner;
}

cc::TaskGraphRunner* BlimpCompositorManager::GetTaskGraphRunner() {
  return g_task_graph_runner.Pointer();
}

gpu::GpuMemoryBufferManager*
BlimpCompositorManager::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager_;
}

cc::ImageSerializationProcessor*
BlimpCompositorManager::GetImageSerializationProcessor() {
  return BlobImageSerializationProcessor::current();
}

void BlimpCompositorManager::SendWebGestureEvent(
    int render_widget_id,
    const blink::WebGestureEvent& gesture_event) {
  render_widget_feature_->SendWebGestureEvent(kDummyTabId,
                                             render_widget_id,
                                             gesture_event);
}

void BlimpCompositorManager::SendCompositorMessage(
    int render_widget_id,
    const cc::proto::CompositorMessage& message) {
  render_widget_feature_->SendCompositorMessage(kDummyTabId,
                                               render_widget_id,
                                               message);
}

BlimpCompositor* BlimpCompositorManager::GetCompositor(int render_widget_id) {
  CompositorMap::const_iterator it = compositors_.find(render_widget_id);
  if (it == compositors_.end())
    return nullptr;
  return it->second.get();
}

}  // namespace client
}  // namespace blimp
