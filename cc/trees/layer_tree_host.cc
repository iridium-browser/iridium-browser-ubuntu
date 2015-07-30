// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <algorithm>
#include <stack>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/render_surface.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace {
static base::StaticAtomicSequenceNumber s_layer_tree_host_sequence_number;
}

namespace cc {

LayerTreeHost::InitParams::InitParams() {
}

LayerTreeHost::InitParams::~InitParams() {
}

scoped_ptr<LayerTreeHost> LayerTreeHost::CreateThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    InitParams* params) {
  DCHECK(params->main_task_runner.get());
  DCHECK(impl_task_runner.get());
  DCHECK(params->settings);
  scoped_ptr<LayerTreeHost> layer_tree_host(new LayerTreeHost(params));
  layer_tree_host->InitializeThreaded(
      params->main_task_runner, impl_task_runner,
      params->external_begin_frame_source.Pass());
  return layer_tree_host.Pass();
}

scoped_ptr<LayerTreeHost> LayerTreeHost::CreateSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    InitParams* params) {
  DCHECK(params->settings);
  scoped_ptr<LayerTreeHost> layer_tree_host(new LayerTreeHost(params));
  layer_tree_host->InitializeSingleThreaded(
      single_thread_client, params->main_task_runner,
      params->external_begin_frame_source.Pass());
  return layer_tree_host.Pass();
}

LayerTreeHost::LayerTreeHost(InitParams* params)
    : micro_benchmark_controller_(this),
      next_ui_resource_id_(1),
      inside_begin_main_frame_(false),
      needs_full_tree_sync_(true),
      needs_meta_info_recomputation_(true),
      client_(params->client),
      source_frame_number_(0),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      output_surface_lost_(true),
      settings_(*params->settings),
      debug_state_(settings_.initial_debug_state),
      top_controls_shrink_blink_size_(false),
      top_controls_height_(0.f),
      top_controls_shown_ratio_(0.f),
      device_scale_factor_(1.f),
      visible_(true),
      page_scale_factor_(1.f),
      min_page_scale_factor_(1.f),
      max_page_scale_factor_(1.f),
      has_gpu_rasterization_trigger_(false),
      content_is_suitable_for_gpu_rasterization_(true),
      gpu_rasterization_histogram_recorded_(false),
      background_color_(SK_ColorWHITE),
      has_transparent_background_(false),
      partial_texture_update_requests_(0),
      did_complete_scale_animation_(false),
      in_paint_layer_contents_(false),
      id_(s_layer_tree_host_sequence_number.GetNext() + 1),
      next_commit_forces_redraw_(false),
      shared_bitmap_manager_(params->shared_bitmap_manager),
      gpu_memory_buffer_manager_(params->gpu_memory_buffer_manager),
      task_graph_runner_(params->task_graph_runner),
      surface_id_namespace_(0u),
      next_surface_sequence_(1u) {
  if (settings_.accelerated_animation_enabled)
    animation_registrar_ = AnimationRegistrar::Create();
  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
}

void LayerTreeHost::InitializeThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  InitializeProxy(ThreadProxy::Create(this,
                                      main_task_runner,
                                      impl_task_runner,
                                      external_begin_frame_source.Pass()));
}

void LayerTreeHost::InitializeSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  InitializeProxy(
      SingleThreadProxy::Create(this,
                                single_thread_client,
                                main_task_runner,
                                external_begin_frame_source.Pass()));
}

void LayerTreeHost::InitializeForTesting(scoped_ptr<Proxy> proxy_for_testing) {
  InitializeProxy(proxy_for_testing.Pass());
}

void LayerTreeHost::InitializeProxy(scoped_ptr<Proxy> proxy) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");

  proxy_ = proxy.Pass();
  proxy_->Start();
  if (settings_.accelerated_animation_enabled) {
    animation_registrar_->set_supports_scroll_animations(
        proxy_->SupportsImplScrolling());
  }
}

LayerTreeHost::~LayerTreeHost() {
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(NULL);

  DCHECK(swap_promise_monitor_.empty());

  BreakSwapPromises(SwapPromise::COMMIT_FAILS);

  if (proxy_) {
    DCHECK(proxy_->IsMainThread());
    proxy_->Stop();
  }

  // We must clear any pointers into the layer tree prior to destroying it.
  RegisterViewportLayers(NULL, NULL, NULL, NULL);

  if (root_layer_.get()) {
    // The layer tree must be destroyed before the layer tree host. We've
    // made a contract with our animation controllers that the registrar
    // will outlive them, and we must make good.
    root_layer_ = NULL;
  }
}

void LayerTreeHost::SetLayerTreeHostClientReady() {
  proxy_->SetLayerTreeHostClientReady();
}

void LayerTreeHost::DeleteContentsTexturesOnImplThread(
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread());
  if (contents_texture_manager_)
    contents_texture_manager_->ClearAllMemory(resource_provider);
}

void LayerTreeHost::WillBeginMainFrame() {
  devtools_instrumentation::WillBeginMainThreadFrame(id(),
                                                     source_frame_number());
  client_->WillBeginMainFrame();
}

void LayerTreeHost::DidBeginMainFrame() {
  client_->DidBeginMainFrame();
}

void LayerTreeHost::BeginMainFrameNotExpectedSoon() {
  client_->BeginMainFrameNotExpectedSoon();
}

void LayerTreeHost::BeginMainFrame(const BeginFrameArgs& args) {
  inside_begin_main_frame_ = true;
  client_->BeginMainFrame(args);
  inside_begin_main_frame_ = false;
}

void LayerTreeHost::DidStopFlinging() {
  proxy_->MainThreadHasStoppedFlinging();
}

void LayerTreeHost::Layout() {
  client_->Layout();
}

void LayerTreeHost::BeginCommitOnImplThread(LayerTreeHostImpl* host_impl) {
  DCHECK(proxy_->IsImplThread());
  TRACE_EVENT0("cc", "LayerTreeHost::CommitTo");
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::CommitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::FinishCommitOnImplThread(LayerTreeHostImpl* host_impl) {
  DCHECK(proxy_->IsImplThread());

  // If there are linked evicted backings, these backings' resources may be put
  // into the impl tree, so we can't draw yet. Determine this before clearing
  // all evicted backings.
  bool new_impl_tree_has_no_evicted_resources = false;
  if (contents_texture_manager_) {
    new_impl_tree_has_no_evicted_resources =
        !contents_texture_manager_->LinkedEvictedBackingsExist();

    // If the memory limit has been increased since this now-finishing
    // commit began, and the extra now-available memory would have been used,
    // then request another commit.
    if (contents_texture_manager_->MaxMemoryLimitBytes() <
            host_impl->memory_allocation_limit_bytes() &&
        contents_texture_manager_->MaxMemoryLimitBytes() <
            contents_texture_manager_->MaxMemoryNeededBytes()) {
      host_impl->SetNeedsCommit();
    }

    host_impl->set_max_memory_needed_bytes(
        contents_texture_manager_->MaxMemoryNeededBytes());

    contents_texture_manager_->UpdateBackingsState(
        host_impl->resource_provider());
    contents_texture_manager_->ReduceMemory(host_impl->resource_provider());
  }

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace &&
      frame_viewer_instrumentation::IsTracingLayerTreeSnapshots() &&
      root_layer()) {
    LayerTreeHostCommon::CallFunctionForSubtree(
        root_layer(), [](Layer* layer) { layer->DidBeginTracing(); });
  }

  LayerTreeImpl* sync_tree = host_impl->sync_tree();

  if (next_commit_forces_redraw_) {
    sync_tree->ForceRedrawNextActivation();
    next_commit_forces_redraw_ = false;
  }

  sync_tree->set_source_frame_number(source_frame_number());

  if (needs_full_tree_sync_) {
    sync_tree->SetRootLayer(TreeSynchronizer::SynchronizeTrees(
        root_layer(), sync_tree->DetachLayerTree(), sync_tree));
  }
  sync_tree->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (hud_layer_.get()) {
    LayerImpl* hud_impl = LayerTreeHostCommon::FindLayerInSubtree(
        sync_tree->root_layer(), hud_layer_->id());
    sync_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    sync_tree->set_hud_layer(NULL);
  }

  sync_tree->set_background_color(background_color_);
  sync_tree->set_has_transparent_background(has_transparent_background_);

  if (page_scale_layer_.get() && inner_viewport_scroll_layer_.get()) {
    sync_tree->SetViewportLayersFromIds(
        overscroll_elasticity_layer_.get() ? overscroll_elasticity_layer_->id()
                                           : Layer::INVALID_ID,
        page_scale_layer_->id(), inner_viewport_scroll_layer_->id(),
        outer_viewport_scroll_layer_.get() ? outer_viewport_scroll_layer_->id()
                                           : Layer::INVALID_ID);
    DCHECK(inner_viewport_scroll_layer_->IsContainerForFixedPositionLayers());
  } else {
    sync_tree->ClearViewportLayers();
  }

  sync_tree->RegisterSelection(selection_);

  sync_tree->PushPageScaleFromMainThread(
      page_scale_factor_, min_page_scale_factor_, max_page_scale_factor_);
  sync_tree->elastic_overscroll()->PushFromMainThread(elastic_overscroll_);
  if (sync_tree->IsActiveTree())
    sync_tree->elastic_overscroll()->PushPendingToActive();

  sync_tree->PassSwapPromises(&swap_promise_list_);

  sync_tree->set_top_controls_shrink_blink_size(
      top_controls_shrink_blink_size_);
  sync_tree->set_top_controls_height(top_controls_height_);
  sync_tree->PushTopControlsFromMainThread(top_controls_shown_ratio_);

  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentIsSuitableForGpuRasterization(
      content_is_suitable_for_gpu_rasterization_);
  RecordGpuRasterizationHistogram();

  host_impl->SetViewportSize(device_viewport_size_);
  host_impl->SetDeviceScaleFactor(device_scale_factor_);
  host_impl->SetDebugState(debug_state_);
  if (pending_page_scale_animation_) {
    sync_tree->SetPendingPageScaleAnimation(
        pending_page_scale_animation_.Pass());
  }

  if (!ui_resource_request_queue_.empty()) {
    sync_tree->set_ui_resource_request_queue(ui_resource_request_queue_);
    ui_resource_request_queue_.clear();
  }

  DCHECK(!sync_tree->ViewportSizeInvalid());

  if (new_impl_tree_has_no_evicted_resources) {
    if (sync_tree->ContentsTexturesPurged())
      sync_tree->ResetContentsTexturesPurged();
  }

  sync_tree->set_has_ever_been_drawn(false);
  sync_tree->SetPropertyTrees(property_trees_);

  {
    TRACE_EVENT0("cc", "LayerTreeHost::PushProperties");
    TreeSynchronizer::PushProperties(root_layer(), sync_tree->root_layer());
  }

  micro_benchmark_controller_.ScheduleImplBenchmarks(host_impl);
}

void LayerTreeHost::WillCommit() {
  client_->WillCommit();
}

void LayerTreeHost::UpdateHudLayer() {
  if (debug_state_.ShowHudInfo()) {
    if (!hud_layer_.get())
      hud_layer_ = HeadsUpDisplayLayer::Create();

    if (root_layer_.get() && !hud_layer_->parent())
      root_layer_->AddChild(hud_layer_);
  } else if (hud_layer_.get()) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = NULL;
  }
}

void LayerTreeHost::CommitComplete() {
  source_frame_number_++;
  client_->DidCommit();
  if (did_complete_scale_animation_) {
    client_->DidCompletePageScaleAnimation();
    did_complete_scale_animation_ = false;
  }
}

void LayerTreeHost::SetOutputSurface(scoped_ptr<OutputSurface> surface) {
  TRACE_EVENT0("cc", "LayerTreeHost::SetOutputSurface");
  DCHECK(output_surface_lost_);
  DCHECK(surface);

  proxy_->SetOutputSurface(surface.Pass());
}

void LayerTreeHost::RequestNewOutputSurface() {
  client_->RequestNewOutputSurface();
}

void LayerTreeHost::DidInitializeOutputSurface() {
  output_surface_lost_ = false;

  if (!contents_texture_manager_ && !settings_.impl_side_painting) {
    contents_texture_manager_ =
        PrioritizedResourceManager::Create(proxy_.get());
    surface_memory_placeholder_ =
        contents_texture_manager_->CreateTexture(gfx::Size(), RGBA_8888);
  }

  if (root_layer()) {
    LayerTreeHostCommon::CallFunctionForSubtree(
        root_layer(), [](Layer* layer) { layer->OnOutputSurfaceCreated(); });
  }

  client_->DidInitializeOutputSurface();
}

void LayerTreeHost::DidFailToInitializeOutputSurface() {
  DCHECK(output_surface_lost_);
  client_->DidFailToInitializeOutputSurface();
}

scoped_ptr<LayerTreeHostImpl> LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(proxy_->IsImplThread());
  scoped_ptr<LayerTreeHostImpl> host_impl = LayerTreeHostImpl::Create(
      settings_, client, proxy_.get(), rendering_stats_instrumentation_.get(),
      shared_bitmap_manager_, gpu_memory_buffer_manager_, task_graph_runner_,
      id_);
  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentIsSuitableForGpuRasterization(
      content_is_suitable_for_gpu_rasterization_);
  shared_bitmap_manager_ = NULL;
  gpu_memory_buffer_manager_ = NULL;
  task_graph_runner_ = NULL;
  top_controls_manager_weak_ptr_ =
      host_impl->top_controls_manager()->AsWeakPtr();
  input_handler_weak_ptr_ = host_impl->AsWeakPtr();
  return host_impl.Pass();
}

void LayerTreeHost::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseOutputSurface");
  DCHECK(proxy_->IsMainThread());

  if (output_surface_lost_)
    return;

  output_surface_lost_ = true;
  SetNeedsCommit();
}

void LayerTreeHost::FinishAllRendering() {
  proxy_->FinishAllRendering();
}

void LayerTreeHost::SetDeferCommits(bool defer_commits) {
  proxy_->SetDeferCommits(defer_commits);
}

void LayerTreeHost::SetNeedsDisplayOnAllLayers() {
  std::stack<Layer*> layer_stack;
  layer_stack.push(root_layer());
  while (!layer_stack.empty()) {
    Layer* current_layer = layer_stack.top();
    layer_stack.pop();
    current_layer->SetNeedsDisplay();
    for (unsigned int i = 0; i < current_layer->children().size(); i++) {
      layer_stack.push(current_layer->child_at(i));
    }
  }
}

const RendererCapabilities& LayerTreeHost::GetRendererCapabilities() const {
  return proxy_->GetRendererCapabilities();
}

void LayerTreeHost::SetNeedsAnimate() {
  proxy_->SetNeedsAnimate();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsUpdateLayers() {
  proxy_->SetNeedsUpdateLayers();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsCommit() {
  if (!prepaint_callback_.IsCancelled()) {
    TRACE_EVENT_INSTANT0("cc",
                         "LayerTreeHost::SetNeedsCommit::cancel prepaint",
                         TRACE_EVENT_SCOPE_THREAD);
    prepaint_callback_.Cancel();
  }
  proxy_->SetNeedsCommit();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  needs_meta_info_recomputation_ = true;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetNeedsMetaInfoRecomputation(bool needs_recomputation) {
  needs_meta_info_recomputation_ = needs_recomputation;
}

void LayerTreeHost::SetNeedsRedraw() {
  SetNeedsRedrawRect(gfx::Rect(device_viewport_size_));
}

void LayerTreeHost::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  proxy_->SetNeedsRedraw(damage_rect);
}

bool LayerTreeHost::CommitRequested() const {
  return proxy_->CommitRequested();
}

bool LayerTreeHost::BeginMainFrameRequested() const {
  return proxy_->BeginMainFrameRequested();
}


void LayerTreeHost::SetNextCommitWaitsForActivation() {
  proxy_->SetNextCommitWaitsForActivation();
}

void LayerTreeHost::SetNextCommitForcesRedraw() {
  next_commit_forces_redraw_ = true;
}

void LayerTreeHost::SetAnimationEvents(
    scoped_ptr<AnimationEventsVector> events) {
  DCHECK(proxy_->IsMainThread());
  animation_registrar_->SetAnimationEvents(events.Pass());
}

void LayerTreeHost::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (root_layer_.get() == root_layer.get())
    return;

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(NULL);
  root_layer_ = root_layer;
  if (root_layer_.get()) {
    DCHECK(!root_layer_->parent());
    root_layer_->SetLayerTreeHost(this);
  }

  if (hud_layer_.get())
    hud_layer_->RemoveFromParent();

  // Reset gpu rasterization flag.
  // This flag is sticky until a new tree comes along.
  content_is_suitable_for_gpu_rasterization_ = true;
  gpu_rasterization_histogram_recorded_ = false;

  SetNeedsFullTreeSync();
}

void LayerTreeHost::SetDebugState(const LayerTreeDebugState& debug_state) {
  LayerTreeDebugState new_debug_state =
      LayerTreeDebugState::Unite(settings_.initial_debug_state, debug_state);

  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());

  SetNeedsCommit();
  proxy_->SetDebugState(debug_state);
}

void LayerTreeHost::SetHasGpuRasterizationTrigger(bool has_trigger) {
  if (has_trigger == has_gpu_rasterization_trigger_)
    return;

  has_gpu_rasterization_trigger_ = has_trigger;
  TRACE_EVENT_INSTANT1("cc",
                       "LayerTreeHost::SetHasGpuRasterizationTrigger",
                       TRACE_EVENT_SCOPE_THREAD,
                       "has_trigger",
                       has_gpu_rasterization_trigger_);
}

void LayerTreeHost::SetViewportSize(const gfx::Size& device_viewport_size) {
  if (device_viewport_size == device_viewport_size_)
    return;

  device_viewport_size_ = device_viewport_size;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetTopControlsHeight(float height, bool shrink) {
  if (top_controls_height_ == height &&
      top_controls_shrink_blink_size_ == shrink)
    return;

  top_controls_height_ = height;
  top_controls_shrink_blink_size_ = shrink;
  SetNeedsCommit();
}

void LayerTreeHost::SetTopControlsShownRatio(float ratio) {
  if (top_controls_shown_ratio_ == ratio)
    return;

  top_controls_shown_ratio_ = ratio;
  SetNeedsCommit();
}

void LayerTreeHost::ApplyPageScaleDeltaFromImplSide(float page_scale_delta) {
  DCHECK(CommitRequested());
  if (page_scale_delta == 1.f)
    return;
  page_scale_factor_ *= page_scale_delta;
  property_trees_.needs_rebuild = true;
}

void LayerTreeHost::SetPageScaleFactorAndLimits(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  if (page_scale_factor == page_scale_factor_ &&
      min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return;

  page_scale_factor_ = page_scale_factor;
  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  if (!visible)
    ReduceMemoryUsage();
  proxy_->SetVisible(visible);
}

void LayerTreeHost::SetThrottleFrameProduction(bool throttle) {
  proxy_->SetThrottleFrameProduction(throttle);
}

void LayerTreeHost::StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                            bool use_anchor,
                                            float scale,
                                            base::TimeDelta duration) {
  pending_page_scale_animation_.reset(
      new PendingPageScaleAnimation(
          target_offset,
          use_anchor,
          scale,
          duration));

  SetNeedsCommit();
}

void LayerTreeHost::NotifyInputThrottledUntilCommit() {
  proxy_->NotifyInputThrottledUntilCommit();
}

void LayerTreeHost::Composite(base::TimeTicks frame_begin_time) {
  DCHECK(!proxy_->HasImplThread());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  SingleThreadProxy* proxy = static_cast<SingleThreadProxy*>(proxy_.get());

  SetLayerTreeHostClientReady();
  proxy->CompositeImmediately(frame_begin_time);
}

bool LayerTreeHost::UpdateLayers(ResourceUpdateQueue* queue) {
  DCHECK(!output_surface_lost_);

  if (!root_layer())
    return false;

  DCHECK(!root_layer()->parent());

  bool result = UpdateLayers(root_layer(), queue);

  micro_benchmark_controller_.DidUpdateLayers();

  return result || next_commit_forces_redraw_;
}

void LayerTreeHost::DidCompletePageScaleAnimation() {
  did_complete_scale_animation_ = true;
}

static Layer* FindFirstScrollableLayer(Layer* layer) {
  if (!layer)
    return NULL;

  if (layer->scrollable())
    return layer;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    Layer* found = FindFirstScrollableLayer(layer->children()[i].get());
    if (found)
      return found;
  }

  return NULL;
}

void LayerTreeHost::RecordGpuRasterizationHistogram() {
  // Gpu rasterization is only supported when impl-side painting is enabled.
  if (gpu_rasterization_histogram_recorded_ || !settings_.impl_side_painting)
    return;

  // Record how widely gpu rasterization is enabled.
  // This number takes device/gpu whitelisting/backlisting into account.
  // Note that we do not consider the forced gpu rasterization mode, which is
  // mostly used for debugging purposes.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationEnabled",
                        settings_.gpu_rasterization_enabled);
  if (settings_.gpu_rasterization_enabled) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationTriggered",
                          has_gpu_rasterization_trigger_);
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationSuitableContent",
                          content_is_suitable_for_gpu_rasterization_);
    // Record how many pages actually get gpu rasterization when enabled.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationUsed",
                          (has_gpu_rasterization_trigger_ &&
                           content_is_suitable_for_gpu_rasterization_));
  }

  gpu_rasterization_histogram_recorded_ = true;
}

bool LayerTreeHost::UsingSharedMemoryResources() {
  return GetRendererCapabilities().using_shared_memory_resources;
}

bool LayerTreeHost::UpdateLayers(Layer* root_layer,
                                 ResourceUpdateQueue* queue) {
  TRACE_EVENT1("cc", "LayerTreeHost::UpdateLayers",
               "source_frame_number", source_frame_number());

  RenderSurfaceLayerList render_surface_layer_list;

  UpdateHudLayer();

  Layer* root_scroll = FindFirstScrollableLayer(root_layer);
  Layer* page_scale_layer = page_scale_layer_.get();
  if (!page_scale_layer && root_scroll)
    page_scale_layer = root_scroll->parent();

  if (hud_layer_.get()) {
    hud_layer_->PrepareForCalculateDrawProperties(device_viewport_size(),
                                                  device_scale_factor_);
  }

  bool can_render_to_separate_surface = true;
  // TODO(vmpstr): Passing 0 as the current render surface layer list id means
  // that we won't be able to detect if a layer is part of
  // |render_surface_layer_list|.  Change this if this information is
  // required.
  int render_surface_layer_list_id = 0;
  LayerTreeHostCommon::CalcDrawPropsMainInputs inputs(
      root_layer, device_viewport_size(), gfx::Transform(),
      device_scale_factor_, page_scale_factor_, page_scale_layer,
      elastic_overscroll_, overscroll_elasticity_layer_.get(),
      GetRendererCapabilities().max_texture_size, settings_.can_use_lcd_text,
      settings_.layers_always_allowed_lcd_text, can_render_to_separate_surface,
      settings_.layer_transforms_should_scale_layer_contents,
      settings_.verify_property_trees, &render_surface_layer_list,
      render_surface_layer_list_id, &property_trees_);

  // This is a temporary state of affairs until impl-side painting is shipped
  // everywhere and main thread property trees can be used in all cases.
  // This code here implies that even if verify property trees is on,
  // no verification will occur and only property trees will be used on the
  // main thread.
  if (using_only_property_trees()) {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::CalcDrawProps");

    LayerTreeHostCommon::PreCalculateMetaInformation(root_layer);

    bool preserves_2d_axis_alignment = false;
    gfx::Transform identity_transform;
    LayerList update_layer_list;

    LayerTreeHostCommon::UpdateRenderSurfaces(
        root_layer, can_render_to_separate_surface, identity_transform,
        preserves_2d_axis_alignment);
    {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
                   "LayerTreeHostCommon::ComputeVisibleRectsWithPropertyTrees");
      BuildPropertyTreesAndComputeVisibleRects(
          root_layer, page_scale_layer, page_scale_factor_,
          device_scale_factor_, gfx::Rect(device_viewport_size_),
          identity_transform, &property_trees_, &update_layer_list);
    }

    for (const auto& layer : update_layer_list)
      layer->SavePaintProperties();

    base::AutoReset<bool> painting(&in_paint_layer_contents_, true);
    bool did_paint_content = false;
    for (const auto& layer : update_layer_list) {
      // TODO(enne): temporarily clobber draw properties visible rect.
      layer->draw_properties().visible_content_rect =
          layer->visible_rect_from_property_trees();
      did_paint_content |= layer->Update(queue, nullptr);
      content_is_suitable_for_gpu_rasterization_ &=
          layer->IsSuitableForGpuRasterization();
    }
    return did_paint_content;
  }

  {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::CalcDrawProps");
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }

  // Reset partial texture update requests.
  partial_texture_update_requests_ = 0;

  bool did_paint_content = false;
  bool need_more_updates = false;
  PaintLayerContents(render_surface_layer_list, queue, &did_paint_content,
                     &need_more_updates);
  if (need_more_updates) {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::posting prepaint task");
    prepaint_callback_.Reset(base::Bind(&LayerTreeHost::TriggerPrepaint,
                                        base::Unretained(this)));
    static base::TimeDelta prepaint_delay =
        base::TimeDelta::FromMilliseconds(100);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, prepaint_callback_.callback(), prepaint_delay);
  }

  return did_paint_content;
}

void LayerTreeHost::TriggerPrepaint() {
  prepaint_callback_.Cancel();
  TRACE_EVENT0("cc", "LayerTreeHost::TriggerPrepaint");
  SetNeedsCommit();
}

void LayerTreeHost::ReduceMemoryUsage() {
  if (!root_layer())
    return;

  LayerTreeHostCommon::CallFunctionForSubtree(
      root_layer(), [](Layer* layer) { layer->ReduceMemoryUsage(); });
}

void LayerTreeHost::SetPrioritiesForSurfaces(size_t surface_memory_bytes) {
  DCHECK(surface_memory_placeholder_);

  // Surfaces have a place holder for their memory since they are managed
  // independantly but should still be tracked and reduce other memory usage.
  surface_memory_placeholder_->SetTextureManager(
      contents_texture_manager_.get());
  surface_memory_placeholder_->set_request_priority(
      PriorityCalculator::RenderSurfacePriority());
  surface_memory_placeholder_->SetToSelfManagedMemoryPlaceholder(
      surface_memory_bytes);
}

void LayerTreeHost::SetPrioritiesForLayers(
    const RenderSurfaceLayerList& update_list) {
  PriorityCalculator calculator;
  typedef LayerIterator<Layer> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&update_list);
  for (LayerIteratorType it = LayerIteratorType::Begin(&update_list);
       it != end;
       ++it) {
    if (it.represents_itself()) {
      it->SetTexturePriorities(calculator);
    } else if (it.represents_target_render_surface()) {
      if (it->mask_layer())
        it->mask_layer()->SetTexturePriorities(calculator);
      if (it->replica_layer() && it->replica_layer()->mask_layer())
        it->replica_layer()->mask_layer()->SetTexturePriorities(calculator);
    }
  }
}

void LayerTreeHost::PrioritizeTextures(
    const RenderSurfaceLayerList& render_surface_layer_list) {
  if (!contents_texture_manager_)
    return;

  contents_texture_manager_->ClearPriorities();

  size_t memory_for_render_surfaces_metric =
      CalculateMemoryForRenderSurfaces(render_surface_layer_list);

  SetPrioritiesForLayers(render_surface_layer_list);
  SetPrioritiesForSurfaces(memory_for_render_surfaces_metric);

  contents_texture_manager_->PrioritizeTextures();
}

size_t LayerTreeHost::CalculateMemoryForRenderSurfaces(
    const RenderSurfaceLayerList& update_list) {
  size_t readback_bytes = 0;
  size_t contents_texture_bytes = 0;

  // Start iteration at 1 to skip the root surface as it does not have a texture
  // cost.
  for (size_t i = 1; i < update_list.size(); ++i) {
    Layer* render_surface_layer = update_list.at(i);
    RenderSurface* render_surface = render_surface_layer->render_surface();

    size_t bytes =
        Resource::MemorySizeBytes(render_surface->content_rect().size(),
                                  RGBA_8888);
    contents_texture_bytes += bytes;

    if (render_surface_layer->background_filters().IsEmpty() &&
        render_surface_layer->uses_default_blend_mode())
      continue;

    if (!readback_bytes) {
      readback_bytes = Resource::MemorySizeBytes(device_viewport_size_,
                                                 RGBA_8888);
    }
  }
  return readback_bytes + contents_texture_bytes;
}

void LayerTreeHost::PaintMasksForRenderSurface(Layer* render_surface_layer,
                                               ResourceUpdateQueue* queue,
                                               bool* did_paint_content,
                                               bool* need_more_updates) {
  // Note: Masks and replicas only exist for layers that own render surfaces. If
  // we reach this point in code, we already know that at least something will
  // be drawn into this render surface, so the mask and replica should be
  // painted.

  Layer* mask_layer = render_surface_layer->mask_layer();
  if (mask_layer) {
    *did_paint_content |= mask_layer->Update(queue, NULL);
    *need_more_updates |= mask_layer->NeedMoreUpdates();
  }

  Layer* replica_mask_layer =
      render_surface_layer->replica_layer() ?
      render_surface_layer->replica_layer()->mask_layer() : NULL;
  if (replica_mask_layer) {
    *did_paint_content |= replica_mask_layer->Update(queue, NULL);
    *need_more_updates |= replica_mask_layer->NeedMoreUpdates();
  }
}

void LayerTreeHost::PaintLayerContents(
    const RenderSurfaceLayerList& render_surface_layer_list,
    ResourceUpdateQueue* queue,
    bool* did_paint_content,
    bool* need_more_updates) {
  OcclusionTracker<Layer> occlusion_tracker(
      root_layer_->render_surface()->content_rect());
  occlusion_tracker.set_minimum_tracking_size(
      settings_.minimum_occlusion_tracking_size);

  PrioritizeTextures(render_surface_layer_list);

  in_paint_layer_contents_ = true;

  // Iterates front-to-back to allow for testing occlusion and performing
  // culling during the tree walk.
  typedef LayerIterator<Layer> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list);
  for (LayerIteratorType it =
           LayerIteratorType::Begin(&render_surface_layer_list);
       it != end;
       ++it) {
    occlusion_tracker.EnterLayer(it);

    if (it.represents_target_render_surface()) {
      PaintMasksForRenderSurface(
          *it, queue, did_paint_content, need_more_updates);
    } else if (it.represents_itself()) {
      DCHECK(!it->paint_properties().bounds.IsEmpty());
      *did_paint_content |= it->Update(queue, &occlusion_tracker);
      *need_more_updates |= it->NeedMoreUpdates();
      // Note the '&&' with previous is-suitable state.
      // This means that once the layer-tree becomes unsuitable for gpu
      // rasterization due to some content, it will continue to be unsuitable
      // even if that content is replaced by gpu-friendly content.
      // This is to avoid switching back-and-forth between gpu and sw
      // rasterization which may be both bad for performance and visually
      // jarring.
      content_is_suitable_for_gpu_rasterization_ &=
          it->IsSuitableForGpuRasterization();
    }

    occlusion_tracker.LeaveLayer(it);
  }

  in_paint_layer_contents_ = false;
}

void LayerTreeHost::ApplyScrollAndScale(ScrollAndScaleSet* info) {
  ScopedPtrVector<SwapPromise>::iterator it = info->swap_promises.begin();
  for (; it != info->swap_promises.end(); ++it) {
    scoped_ptr<SwapPromise> swap_promise(info->swap_promises.take(it));
    TRACE_EVENT_FLOW_STEP0("input",
                           "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(swap_promise->TraceId()),
                           "Main thread scroll update");
    QueueSwapPromise(swap_promise.Pass());
  }

  gfx::Vector2dF inner_viewport_scroll_delta;
  gfx::Vector2dF outer_viewport_scroll_delta;

  if (root_layer_.get()) {
    for (size_t i = 0; i < info->scrolls.size(); ++i) {
      Layer* layer = LayerTreeHostCommon::FindLayerInSubtree(
          root_layer_.get(), info->scrolls[i].layer_id);
      if (!layer)
        continue;
      if (layer == outer_viewport_scroll_layer_.get()) {
        outer_viewport_scroll_delta += info->scrolls[i].scroll_delta;
      } else if (layer == inner_viewport_scroll_layer_.get()) {
        inner_viewport_scroll_delta += info->scrolls[i].scroll_delta;
      } else {
        layer->SetScrollOffsetFromImplSide(
            gfx::ScrollOffsetWithDelta(layer->scroll_offset(),
                                       info->scrolls[i].scroll_delta));
      }
    }
  }

  if (!inner_viewport_scroll_delta.IsZero() ||
      !outer_viewport_scroll_delta.IsZero() || info->page_scale_delta != 1.f ||
      !info->elastic_overscroll_delta.IsZero() || info->top_controls_delta) {
    // Preemptively apply the scroll offset and scale delta here before sending
    // it to the client.  If the client comes back and sets it to the same
    // value, then the layer can early out without needing a full commit.
    if (inner_viewport_scroll_layer_.get()) {
      inner_viewport_scroll_layer_->SetScrollOffsetFromImplSide(
          gfx::ScrollOffsetWithDelta(
              inner_viewport_scroll_layer_->scroll_offset(),
              inner_viewport_scroll_delta));
    }

    if (outer_viewport_scroll_layer_.get()) {
      outer_viewport_scroll_layer_->SetScrollOffsetFromImplSide(
          gfx::ScrollOffsetWithDelta(
              outer_viewport_scroll_layer_->scroll_offset(),
              outer_viewport_scroll_delta));
    }

    ApplyPageScaleDeltaFromImplSide(info->page_scale_delta);
    elastic_overscroll_ += info->elastic_overscroll_delta;
    if (!settings_.use_pinch_virtual_viewport) {
      // TODO(miletus): Make sure either this code path is totally gone,
      // or revisit the flooring here if the old pinch viewport code path
      // is causing problems with fractional scroll offset.
      client_->ApplyViewportDeltas(
          gfx::ToFlooredVector2d(inner_viewport_scroll_delta +
                                 outer_viewport_scroll_delta),
          info->page_scale_delta, info->top_controls_delta);
    } else {
      // TODO(ccameron): pass the elastic overscroll here so that input events
      // may be translated appropriately.
      client_->ApplyViewportDeltas(
          inner_viewport_scroll_delta, outer_viewport_scroll_delta,
          info->elastic_overscroll_delta, info->page_scale_delta,
          info->top_controls_delta);
    }
  }
}

void LayerTreeHost::StartRateLimiter() {
  if (inside_begin_main_frame_)
    return;

  if (!rate_limit_timer_.IsRunning()) {
    rate_limit_timer_.Start(FROM_HERE,
                            base::TimeDelta(),
                            this,
                            &LayerTreeHost::RateLimit);
  }
}

void LayerTreeHost::StopRateLimiter() {
  rate_limit_timer_.Stop();
}

void LayerTreeHost::RateLimit() {
  // Force a no-op command on the compositor context, so that any ratelimiting
  // commands will wait for the compositing context, and therefore for the
  // SwapBuffers.
  proxy_->ForceSerializeOnSwapBuffers();
  client_->RateLimitSharedMainThreadContext();
}

bool LayerTreeHost::AlwaysUsePartialTextureUpdates() {
  if (!proxy_->GetRendererCapabilities().allow_partial_texture_updates)
    return false;
  return !proxy_->HasImplThread();
}

size_t LayerTreeHost::MaxPartialTextureUpdates() const {
  size_t max_partial_texture_updates = 0;
  if (proxy_->GetRendererCapabilities().allow_partial_texture_updates &&
      !settings_.impl_side_painting) {
    max_partial_texture_updates =
        std::min(settings_.max_partial_texture_updates,
                 proxy_->MaxPartialTextureUpdates());
  }
  return max_partial_texture_updates;
}

bool LayerTreeHost::RequestPartialTextureUpdate() {
  if (partial_texture_update_requests_ >= MaxPartialTextureUpdates())
    return false;

  partial_texture_update_requests_++;
  return true;
}

void LayerTreeHost::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTreeHost::UpdateTopControlsState(TopControlsState constraints,
                                           TopControlsState current,
                                           bool animate) {
  // Top controls are only used in threaded mode.
  proxy_->ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&TopControlsManager::UpdateTopControlsState,
                 top_controls_manager_weak_ptr_,
                 constraints,
                 current,
                 animate));
}

void LayerTreeHost::AnimateLayers(base::TimeTicks monotonic_time) {
  if (!settings_.accelerated_animation_enabled)
    return;

  AnimationEventsVector events;
  if (animation_registrar_->AnimateLayers(monotonic_time)) {
    animation_registrar_->UpdateAnimationState(true, &events);
    if (!events.empty())
      property_trees_.needs_rebuild = true;
  }
}

UIResourceId LayerTreeHost::CreateUIResource(UIResourceClient* client) {
  DCHECK(client);

  UIResourceId next_id = next_ui_resource_id_++;
  DCHECK(ui_resource_client_map_.find(next_id) ==
         ui_resource_client_map_.end());

  bool resource_lost = false;
  UIResourceRequest request(UIResourceRequest::UI_RESOURCE_CREATE, next_id,
                            client->GetBitmap(next_id, resource_lost));
  ui_resource_request_queue_.push_back(request);

  UIResourceClientData data;
  data.client = client;
  data.size = request.GetBitmap().GetSize();

  ui_resource_client_map_[request.GetId()] = data;
  return request.GetId();
}

// Deletes a UI resource.  May safely be called more than once.
void LayerTreeHost::DeleteUIResource(UIResourceId uid) {
  UIResourceClientMap::iterator iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return;

  UIResourceRequest request(UIResourceRequest::UI_RESOURCE_DELETE, uid);
  ui_resource_request_queue_.push_back(request);
  ui_resource_client_map_.erase(iter);
}

void LayerTreeHost::RecreateUIResources() {
  for (UIResourceClientMap::iterator iter = ui_resource_client_map_.begin();
       iter != ui_resource_client_map_.end();
       ++iter) {
    UIResourceId uid = iter->first;
    const UIResourceClientData& data = iter->second;
    bool resource_lost = true;
    UIResourceRequest request(UIResourceRequest::UI_RESOURCE_CREATE, uid,
                              data.client->GetBitmap(uid, resource_lost));
    ui_resource_request_queue_.push_back(request);
  }
}

// Returns the size of a resource given its id.
gfx::Size LayerTreeHost::GetUIResourceSize(UIResourceId uid) const {
  UIResourceClientMap::const_iterator iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return gfx::Size();

  const UIResourceClientData& data = iter->second;
  return data.size;
}

void LayerTreeHost::RegisterViewportLayers(
    scoped_refptr<Layer> overscroll_elasticity_layer,
    scoped_refptr<Layer> page_scale_layer,
    scoped_refptr<Layer> inner_viewport_scroll_layer,
    scoped_refptr<Layer> outer_viewport_scroll_layer) {
  overscroll_elasticity_layer_ = overscroll_elasticity_layer;
  page_scale_layer_ = page_scale_layer;
  inner_viewport_scroll_layer_ = inner_viewport_scroll_layer;
  outer_viewport_scroll_layer_ = outer_viewport_scroll_layer;
}

void LayerTreeHost::RegisterSelection(const LayerSelection& selection) {
  if (selection_ == selection)
    return;

  selection_ = selection;
  SetNeedsCommit();
}

int LayerTreeHost::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    scoped_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback) {
  return micro_benchmark_controller_.ScheduleRun(
      benchmark_name, value.Pass(), callback);
}

bool LayerTreeHost::SendMessageToMicroBenchmark(int id,
                                                scoped_ptr<base::Value> value) {
  return micro_benchmark_controller_.SendMessage(id, value.Pass());
}

void LayerTreeHost::InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.insert(monitor);
}

void LayerTreeHost::RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.erase(monitor);
}

void LayerTreeHost::NotifySwapPromiseMonitorsOfSetNeedsCommit() {
  std::set<SwapPromiseMonitor*>::iterator it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnSetNeedsCommitOnMain();
}

void LayerTreeHost::QueueSwapPromise(scoped_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  swap_promise_list_.push_back(swap_promise.Pass());
}

void LayerTreeHost::BreakSwapPromises(SwapPromise::DidNotSwapReason reason) {
  for (auto* swap_promise : swap_promise_list_)
    swap_promise->DidNotSwap(reason);
  swap_promise_list_.clear();
}

void LayerTreeHost::set_surface_id_namespace(uint32_t id_namespace) {
  surface_id_namespace_ = id_namespace;
}

SurfaceSequence LayerTreeHost::CreateSurfaceSequence() {
  return SurfaceSequence(surface_id_namespace_, next_surface_sequence_++);
}

void LayerTreeHost::SetChildrenNeedBeginFrames(
    bool children_need_begin_frames) const {
  proxy_->SetChildrenNeedBeginFrames(children_need_begin_frames);
}

void LayerTreeHost::SendBeginFramesToChildren(
    const BeginFrameArgs& args) const {
  client_->SendBeginFramesToChildren(args);
}

void LayerTreeHost::SetAuthoritativeVSyncInterval(
    const base::TimeDelta& interval) {
  proxy_->SetAuthoritativeVSyncInterval(interval);
}

}  // namespace cc
