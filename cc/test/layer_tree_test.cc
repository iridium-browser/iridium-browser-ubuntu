// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_test.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/timing_function.h"
#include "cc/base/switches.h"
#include "cc/blimp/image_serialization_processor.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/output/buffer_to_texture_target_map.h"
#include "cc/proto/compositor_message_to_impl.pb.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_delegating_output_surface.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/proxy_impl.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/remote_channel_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/threaded_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

void CreateVirtualViewportLayers(Layer* root_layer,
                                 scoped_refptr<Layer> outer_scroll_layer,
                                 const gfx::Size& inner_bounds,
                                 const gfx::Size& outer_bounds,
                                 LayerTreeHost* host) {
  scoped_refptr<Layer> inner_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> overscroll_elasticity_layer = Layer::Create();
  scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
  scoped_refptr<Layer> outer_viewport_container_layer = Layer::Create();
  scoped_refptr<Layer> page_scale_layer = Layer::Create();

  root_layer->AddChild(inner_viewport_container_layer);
  inner_viewport_container_layer->AddChild(overscroll_elasticity_layer);
  overscroll_elasticity_layer->AddChild(page_scale_layer);
  page_scale_layer->AddChild(inner_viewport_scroll_layer);
  inner_viewport_scroll_layer->AddChild(outer_viewport_container_layer);
  outer_viewport_container_layer->AddChild(outer_scroll_layer);

  inner_viewport_scroll_layer->SetScrollClipLayerId(
      inner_viewport_container_layer->id());
  outer_scroll_layer->SetScrollClipLayerId(
      outer_viewport_container_layer->id());

  inner_viewport_container_layer->SetBounds(inner_bounds);
  inner_viewport_scroll_layer->SetBounds(outer_bounds);
  outer_viewport_container_layer->SetBounds(outer_bounds);

  inner_viewport_scroll_layer->SetIsContainerForFixedPositionLayers(true);
  outer_scroll_layer->SetIsContainerForFixedPositionLayers(true);
  host->GetLayerTree()->RegisterViewportLayers(
      overscroll_elasticity_layer, page_scale_layer,
      inner_viewport_scroll_layer, outer_scroll_layer);
}

void CreateVirtualViewportLayers(Layer* root_layer,
                                 const gfx::Size& inner_bounds,
                                 const gfx::Size& outer_bounds,
                                 const gfx::Size& scroll_bounds,
                                 LayerTreeHost* host) {
  scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();

  outer_viewport_scroll_layer->SetBounds(scroll_bounds);
  outer_viewport_scroll_layer->SetIsDrawable(true);
  CreateVirtualViewportLayers(root_layer, outer_viewport_scroll_layer,
                              inner_bounds, outer_bounds, host);
}

// Adapts LayerTreeHostImpl for test. Runs real code, then invokes test hooks.
class LayerTreeHostImplForTesting : public LayerTreeHostImpl {
 public:
  static std::unique_ptr<LayerTreeHostImplForTesting> Create(
      TestHooks* test_hooks,
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* host_impl_client,
      TaskRunnerProvider* task_runner_provider,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* stats_instrumentation) {
    return base::WrapUnique(new LayerTreeHostImplForTesting(
        test_hooks, settings, host_impl_client, task_runner_provider,
        shared_bitmap_manager, gpu_memory_buffer_manager, task_graph_runner,
        stats_instrumentation));
  }

 protected:
  LayerTreeHostImplForTesting(
      TestHooks* test_hooks,
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* host_impl_client,
      TaskRunnerProvider* task_runner_provider,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* stats_instrumentation)
      : LayerTreeHostImpl(settings,
                          host_impl_client,
                          task_runner_provider,
                          stats_instrumentation,
                          shared_bitmap_manager,
                          gpu_memory_buffer_manager,
                          task_graph_runner,
                          AnimationHost::CreateForTesting(ThreadInstance::IMPL),
                          0),
        test_hooks_(test_hooks),
        block_notify_ready_to_activate_for_testing_(false),
        notify_ready_to_activate_was_blocked_(false) {}

  void CreateResourceAndRasterBufferProvider(
      std::unique_ptr<RasterBufferProvider>* raster_buffer_provider,
      std::unique_ptr<ResourcePool>* resource_pool) override {
    test_hooks_->CreateResourceAndRasterBufferProvider(
        this, raster_buffer_provider, resource_pool);
  }

  void WillBeginImplFrame(const BeginFrameArgs& args) override {
    LayerTreeHostImpl::WillBeginImplFrame(args);
    test_hooks_->WillBeginImplFrameOnThread(this, args);
  }

  void DidFinishImplFrame() override {
    LayerTreeHostImpl::DidFinishImplFrame();
    test_hooks_->DidFinishImplFrameOnThread(this);
  }

  void BeginMainFrameAborted(
      CommitEarlyOutReason reason,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises) override {
    LayerTreeHostImpl::BeginMainFrameAborted(reason, std::move(swap_promises));
    test_hooks_->BeginMainFrameAbortedOnThread(this, reason);
  }

  void ReadyToCommit() override {
    LayerTreeHostImpl::ReadyToCommit();
    test_hooks_->ReadyToCommitOnThread(this);
  }

  void BeginCommit() override {
    LayerTreeHostImpl::BeginCommit();
    test_hooks_->BeginCommitOnThread(this);
  }

  void CommitComplete() override {
    test_hooks_->WillCommitCompleteOnThread(this);
    LayerTreeHostImpl::CommitComplete();
    test_hooks_->CommitCompleteOnThread(this);
  }

  bool PrepareTiles() override {
    test_hooks_->WillPrepareTilesOnThread(this);
    return LayerTreeHostImpl::PrepareTiles();
  }

  DrawResult PrepareToDraw(FrameData* frame) override {
    test_hooks_->WillPrepareToDrawOnThread(this);
    DrawResult draw_result = LayerTreeHostImpl::PrepareToDraw(frame);
    return test_hooks_->PrepareToDrawOnThread(this, frame, draw_result);
  }

  void DrawLayers(FrameData* frame) override {
    LayerTreeHostImpl::DrawLayers(frame);
    test_hooks_->DrawLayersOnThread(this);
  }

  void NotifyReadyToActivate() override {
    if (block_notify_ready_to_activate_for_testing_) {
      notify_ready_to_activate_was_blocked_ = true;
    } else {
      LayerTreeHostImpl::NotifyReadyToActivate();
      test_hooks_->NotifyReadyToActivateOnThread(this);
    }
  }

  void NotifyReadyToDraw() override {
    LayerTreeHostImpl::NotifyReadyToDraw();
    test_hooks_->NotifyReadyToDrawOnThread(this);
  }

  void NotifyAllTileTasksCompleted() override {
    LayerTreeHostImpl::NotifyAllTileTasksCompleted();
    test_hooks_->NotifyAllTileTasksCompleted(this);
  }

  void BlockNotifyReadyToActivateForTesting(bool block) override {
    CHECK(task_runner_provider()->ImplThreadTaskRunner())
        << "Not supported for single-threaded mode.";
    block_notify_ready_to_activate_for_testing_ = block;
    if (!block && notify_ready_to_activate_was_blocked_) {
      task_runner_provider_->ImplThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(&LayerTreeHostImplForTesting::NotifyReadyToActivate,
                     base::Unretained(this)));
      notify_ready_to_activate_was_blocked_ = false;
    }
  }

  void ActivateSyncTree() override {
    test_hooks_->WillActivateTreeOnThread(this);
    LayerTreeHostImpl::ActivateSyncTree();
    DCHECK(!pending_tree());
    test_hooks_->DidActivateTreeOnThread(this);
  }

  bool InitializeRenderer(OutputSurface* output_surface) override {
    bool success = LayerTreeHostImpl::InitializeRenderer(output_surface);
    test_hooks_->InitializedRendererOnThread(this, success);
    return success;
  }

  void SetVisible(bool visible) override {
    LayerTreeHostImpl::SetVisible(visible);
    test_hooks_->DidSetVisibleOnImplTree(this, visible);
  }

  bool AnimateLayers(base::TimeTicks monotonic_time) override {
    test_hooks_->WillAnimateLayers(this, monotonic_time);
    bool result = LayerTreeHostImpl::AnimateLayers(monotonic_time);
    test_hooks_->AnimateLayers(this, monotonic_time);
    return result;
  }

  void UpdateAnimationState(bool start_ready_animations) override {
    LayerTreeHostImpl::UpdateAnimationState(start_ready_animations);
    bool has_unfinished_animation = false;
    for (const auto& it :
         animation_host()->active_element_animations_for_testing()) {
      if (it.second->HasActiveAnimation()) {
        has_unfinished_animation = true;
        break;
      }
    }
    test_hooks_->UpdateAnimationState(this, has_unfinished_animation);
  }

  void NotifyTileStateChanged(const Tile* tile) override {
    LayerTreeHostImpl::NotifyTileStateChanged(tile);
    test_hooks_->NotifyTileStateChangedOnThread(this, tile);
  }

 private:
  TestHooks* test_hooks_;
  bool block_notify_ready_to_activate_for_testing_;
  bool notify_ready_to_activate_was_blocked_;
};

// Implementation of LayerTreeHost callback interface.
class LayerTreeHostClientForTesting : public LayerTreeHostClient,
                                      public LayerTreeHostSingleThreadClient {
 public:
  static std::unique_ptr<LayerTreeHostClientForTesting> Create(
      TestHooks* test_hooks) {
    return base::WrapUnique(new LayerTreeHostClientForTesting(test_hooks));
  }
  ~LayerTreeHostClientForTesting() override {}

  void WillBeginMainFrame() override { test_hooks_->WillBeginMainFrame(); }

  void DidBeginMainFrame() override { test_hooks_->DidBeginMainFrame(); }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    test_hooks_->BeginMainFrame(args);
  }

  void UpdateLayerTreeHost() override { test_hooks_->UpdateLayerTreeHost(); }

  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {
    test_hooks_->ApplyViewportDeltas(inner_delta, outer_delta,
                                     elastic_overscroll_delta, page_scale,
                                     top_controls_delta);
  }

  void RequestNewOutputSurface() override {
    test_hooks_->RequestNewOutputSurface();
  }

  void DidInitializeOutputSurface() override {
    test_hooks_->DidInitializeOutputSurface();
  }

  void DidFailToInitializeOutputSurface() override {
    test_hooks_->DidFailToInitializeOutputSurface();
    RequestNewOutputSurface();
  }

  void WillCommit() override { test_hooks_->WillCommit(); }

  void DidCommit() override { test_hooks_->DidCommit(); }

  void DidCommitAndDrawFrame() override {
    test_hooks_->DidCommitAndDrawFrame();
  }

  void DidCompleteSwapBuffers() override {
    test_hooks_->DidCompleteSwapBuffers();
  }

  void DidPostSwapBuffers() override {}
  void DidAbortSwapBuffers() override {}
  void RequestScheduleComposite() override { test_hooks_->ScheduleComposite(); }
  void DidCompletePageScaleAnimation() override {}
  void BeginMainFrameNotExpectedSoon() override {
    test_hooks_->BeginMainFrameNotExpectedSoon();
  }

 private:
  explicit LayerTreeHostClientForTesting(TestHooks* test_hooks)
      : test_hooks_(test_hooks) {}

  TestHooks* test_hooks_;
};

// Adapts LayerTreeHost for test. Injects LayerTreeHostImplForTesting.
class LayerTreeHostForTesting : public LayerTreeHost {
 public:
  static std::unique_ptr<LayerTreeHostForTesting> Create(
      TestHooks* test_hooks,
      CompositorMode mode,
      LayerTreeHostClientForTesting* client,
      RemoteProtoChannel* remote_proto_channel,
      SharedBitmapManager* shared_bitmap_manager,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      TaskGraphRunner* task_graph_runner,
      const LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source,
      ImageSerializationProcessor* image_serialization_processor) {
    LayerTreeHost::InitParams params;
    params.client = client;
    params.shared_bitmap_manager = shared_bitmap_manager;
    params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
    params.task_graph_runner = task_graph_runner;
    params.settings = &settings;
    params.image_serialization_processor = image_serialization_processor;

    params.animation_host =
        AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    std::unique_ptr<LayerTreeHostForTesting> layer_tree_host(
        new LayerTreeHostForTesting(test_hooks, &params, mode));
    std::unique_ptr<TaskRunnerProvider> task_runner_provider =
        TaskRunnerProvider::Create(main_task_runner, impl_task_runner);
    std::unique_ptr<Proxy> proxy;
    switch (mode) {
      case CompositorMode::SINGLE_THREADED:
        proxy = SingleThreadProxy::Create(layer_tree_host.get(), client,
                                          task_runner_provider.get());
        break;
      case CompositorMode::THREADED:
        DCHECK(impl_task_runner.get());
        proxy = ProxyMain::CreateThreaded(layer_tree_host.get(),
                                          task_runner_provider.get());
        break;
      case CompositorMode::REMOTE:
        DCHECK(!external_begin_frame_source);
        // The Remote LayerTreeHost on the client has the impl task runner.
        if (task_runner_provider->HasImplThread()) {
          proxy = base::MakeUnique<RemoteChannelImpl>(
              layer_tree_host.get(), remote_proto_channel,
              task_runner_provider.get());
        } else {
          proxy = ProxyMain::CreateRemote(remote_proto_channel,
                                          layer_tree_host.get(),
                                          task_runner_provider.get());

          // The LayerTreeHost on the server will never have an output surface.
          // Set output_surface_lost_ to false by default.
          layer_tree_host->SetOutputSurfaceLostForTesting(false);
        }
        break;
    }
    layer_tree_host->InitializeForTesting(
        std::move(task_runner_provider), std::move(proxy),
        std::move(external_begin_frame_source));
    return layer_tree_host;
  }

  std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* host_impl_client) override {
    std::unique_ptr<LayerTreeHostImpl> host_impl =
        LayerTreeHostImplForTesting::Create(
            test_hooks_, settings(), host_impl_client, task_runner_provider(),
            shared_bitmap_manager(), gpu_memory_buffer_manager(),
            task_graph_runner(), rendering_stats_instrumentation());
    input_handler_weak_ptr_ = host_impl->AsWeakPtr();
    return host_impl;
  }

  void SetNeedsCommit() override {
    if (!test_started_)
      return;
    LayerTreeHost::SetNeedsCommit();
  }

  void SetNeedsUpdateLayers() override {
    if (!test_started_)
      return;
    LayerTreeHost::SetNeedsUpdateLayers();
  }

  void set_test_started(bool started) { test_started_ = started; }

 private:
  LayerTreeHostForTesting(TestHooks* test_hooks,
                          LayerTreeHost::InitParams* params,
                          CompositorMode mode)
      : LayerTreeHost(params, mode),
        test_hooks_(test_hooks),
        test_started_(false) {}

  TestHooks* test_hooks_;
  bool test_started_;
};

class LayerTreeTestDelegatingOutputSurfaceClient
    : public TestDelegatingOutputSurfaceClient {
 public:
  explicit LayerTreeTestDelegatingOutputSurfaceClient(TestHooks* hooks)
      : hooks_(hooks) {}

  // TestDelegatingOutputSurfaceClient implementation.
  void DisplayReceivedCompositorFrame(const CompositorFrame& frame) override {
    hooks_->DisplayReceivedCompositorFrameOnThread(frame);
  }
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              const RenderPassList& render_passes) override {
    hooks_->DisplayWillDrawAndSwapOnThread(will_draw_and_swap, render_passes);
  }
  void DisplayDidDrawAndSwap() override {
    hooks_->DisplayDidDrawAndSwapOnThread();
  }

 private:
  TestHooks* hooks_;
};

LayerTreeTest::LayerTreeTest()
    : remote_proto_channel_bridge_(this),
      image_serialization_processor_(
          base::WrapUnique(new FakeImageSerializationProcessor)),
      delegating_output_surface_client_(
          new LayerTreeTestDelegatingOutputSurfaceClient(this)),
      weak_factory_(this) {
  main_thread_weak_ptr_ = weak_factory_.GetWeakPtr();

  // Tests should timeout quickly unless --cc-layer-tree-test-no-timeout was
  // specified (for running in a debugger).
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kCCLayerTreeTestNoTimeout))
    timeout_seconds_ = 5;
  if (command_line->HasSwitch(switches::kCCLayerTreeTestLongTimeout))
    timeout_seconds_ = 5 * 60;
}

LayerTreeTest::~LayerTreeTest() {}

Proxy* LayerTreeTest::remote_client_proxy() const {
  DCHECK(IsRemoteTest());
  return remote_client_layer_tree_host_
             ? remote_client_layer_tree_host_->proxy()
             : nullptr;
}

bool LayerTreeTest::IsRemoteTest() const {
  return mode_ == CompositorMode::REMOTE;
}

gfx::Vector2dF LayerTreeTest::ScrollDelta(LayerImpl* layer_impl) {
  gfx::ScrollOffset delta =
      layer_impl->layer_tree_impl()
          ->property_trees()
          ->scroll_tree.GetScrollOffsetDeltaForTesting(layer_impl->id());
  return gfx::Vector2dF(delta.x(), delta.y());
}

void LayerTreeTest::EndTest() {
  if (ended_)
    return;
  ended_ = true;

  // For the case where we EndTest during BeginTest(), set a flag to indicate
  // that the test should end the second BeginTest regains control.
  if (beginning_) {
    end_when_begin_returns_ = true;
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeTest::RealEndTest, main_thread_weak_ptr_));
  }
}

void LayerTreeTest::EndTestAfterDelayMs(int delay_milliseconds) {
  main_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&LayerTreeTest::EndTest, main_thread_weak_ptr_),
      base::TimeDelta::FromMilliseconds(delay_milliseconds));
}

void LayerTreeTest::PostAddAnimationToMainThreadPlayer(
    AnimationPlayer* player_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchAddAnimationToPlayer,
                 main_thread_weak_ptr_,
                 base::Unretained(player_to_receive_animation), 0.000004));
}

void LayerTreeTest::PostAddInstantAnimationToMainThreadPlayer(
    AnimationPlayer* player_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchAddAnimationToPlayer,
                 main_thread_weak_ptr_,
                 base::Unretained(player_to_receive_animation), 0.0));
}

void LayerTreeTest::PostAddLongAnimationToMainThreadPlayer(
    AnimationPlayer* player_to_receive_animation) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchAddAnimationToPlayer,
                 main_thread_weak_ptr_,
                 base::Unretained(player_to_receive_animation), 1.0));
}

void LayerTreeTest::PostSetDeferCommitsToMainThread(bool defer_commits) {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&LayerTreeTest::DispatchSetDeferCommits,
                            main_thread_weak_ptr_, defer_commits));
}

void LayerTreeTest::PostSetNeedsCommitToMainThread() {
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&LayerTreeTest::DispatchSetNeedsCommit,
                                         main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsUpdateLayersToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&LayerTreeTest::DispatchSetNeedsUpdateLayers,
                            main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsRedrawToMainThread() {
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&LayerTreeTest::DispatchSetNeedsRedraw,
                                         main_thread_weak_ptr_));
}

void LayerTreeTest::PostSetNeedsRedrawRectToMainThread(
    const gfx::Rect& damage_rect) {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&LayerTreeTest::DispatchSetNeedsRedrawRect,
                            main_thread_weak_ptr_, damage_rect));
}

void LayerTreeTest::PostSetVisibleToMainThread(bool visible) {
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&LayerTreeTest::DispatchSetVisible,
                                         main_thread_weak_ptr_, visible));
}

void LayerTreeTest::PostSetNextCommitForcesRedrawToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&LayerTreeTest::DispatchSetNextCommitForcesRedraw,
                            main_thread_weak_ptr_));
}

void LayerTreeTest::PostCompositeImmediatelyToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&LayerTreeTest::DispatchCompositeImmediately,
                            main_thread_weak_ptr_));
}

void LayerTreeTest::PostNextCommitWaitsForActivationToMainThread() {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DispatchNextCommitWaitsForActivation,
                 main_thread_weak_ptr_));
}

std::unique_ptr<OutputSurface>
LayerTreeTest::ReleaseOutputSurfaceOnLayerTreeHost() {
  if (IsRemoteTest()) {
    DCHECK(remote_client_layer_tree_host_);
    return remote_client_layer_tree_host_->ReleaseOutputSurface();
  }
  return layer_tree_host_->ReleaseOutputSurface();
}

void LayerTreeTest::SetVisibleOnLayerTreeHost(bool visible) {
  layer_tree_host_->SetVisible(visible);

  if (IsRemoteTest()) {
    DCHECK(remote_client_layer_tree_host_);
    remote_client_layer_tree_host_->SetVisible(visible);
  }
}

void LayerTreeTest::WillBeginTest() {
  SetVisibleOnLayerTreeHost(true);
}

void LayerTreeTest::DoBeginTest() {
  client_ = LayerTreeHostClientForTesting::Create(this);

  DCHECK(!impl_thread_ || impl_thread_->task_runner().get());

  if (IsRemoteTest()) {
    DCHECK(impl_thread_);
    layer_tree_host_ = LayerTreeHostForTesting::Create(
        this, mode_, client_.get(), &remote_proto_channel_bridge_.channel_main,
        nullptr, nullptr, task_graph_runner_.get(), settings_,
        base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr,
        image_serialization_processor_.get());
    DCHECK(remote_proto_channel_bridge_.channel_main.HasReceiver());
  } else {
    layer_tree_host_ = LayerTreeHostForTesting::Create(
        this, mode_, client_.get(), nullptr, shared_bitmap_manager_.get(),
        gpu_memory_buffer_manager_.get(), task_graph_runner_.get(), settings_,
        base::ThreadTaskRunnerHandle::Get(),
        impl_thread_ ? impl_thread_->task_runner() : nullptr, nullptr,
        image_serialization_processor_.get());
  }

  ASSERT_TRUE(layer_tree_host_);

  main_task_runner_ =
      layer_tree_host_->task_runner_provider()->MainThreadTaskRunner();
  impl_task_runner_ =
      layer_tree_host_->task_runner_provider()->ImplThreadTaskRunner();
  if (!impl_task_runner_) {
    // For tests, if there's no impl thread, make things easier by just giving
    // the main thread task runner.
    impl_task_runner_ = main_task_runner_;
  }

  if (timeout_seconds_) {
    timeout_.Reset(base::Bind(&LayerTreeTest::Timeout, base::Unretained(this)));
    main_task_runner_->PostDelayedTask(
        FROM_HERE, timeout_.callback(),
        base::TimeDelta::FromSeconds(timeout_seconds_));
  }

  started_ = true;
  beginning_ = true;
  SetupTree();
  WillBeginTest();
  BeginTest();
  beginning_ = false;
  if (end_when_begin_returns_)
    RealEndTest();

  // Allow commits to happen once BeginTest() has had a chance to post tasks
  // so that those tasks will happen before the first commit.
  if (layer_tree_host_) {
    static_cast<LayerTreeHostForTesting*>(layer_tree_host_.get())
        ->set_test_started(true);
  }
}

void LayerTreeTest::SetupTree() {
  if (!layer_tree()->root_layer()) {
    scoped_refptr<Layer> root_layer = Layer::Create();
    root_layer->SetBounds(gfx::Size(1, 1));
    layer_tree()->SetRootLayer(root_layer);
  }

  gfx::Size root_bounds = layer_tree()->root_layer()->bounds();
  gfx::Size device_root_bounds =
      gfx::ScaleToCeiledSize(root_bounds, layer_tree()->device_scale_factor());
  layer_tree()->SetViewportSize(device_root_bounds);
  layer_tree()->root_layer()->SetIsDrawable(true);
}

void LayerTreeTest::Timeout() {
  timed_out_ = true;
  EndTest();
}

void LayerTreeTest::RealEndTest() {
  // TODO(mithro): Make this method only end when not inside an impl frame.
  bool main_frame_will_happen;
  if (IsRemoteTest()) {
    main_frame_will_happen =
        remote_client_layer_tree_host_
            ? remote_client_proxy()->MainFrameWillHappenForTesting()
            : false;
  } else {
    main_frame_will_happen =
        layer_tree_host_ ? proxy()->MainFrameWillHappenForTesting() : false;
  }

  if (main_frame_will_happen && !timed_out_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeTest::RealEndTest, main_thread_weak_ptr_));
    return;
  }

  base::MessageLoop::current()->QuitWhenIdle();
}

void LayerTreeTest::DispatchAddAnimationToPlayer(
    AnimationPlayer* player_to_receive_animation,
    double animation_duration) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (player_to_receive_animation) {
    AddOpacityTransitionToPlayer(player_to_receive_animation,
                                 animation_duration, 0, 0.5, true);
  }
}

void LayerTreeTest::DispatchSetDeferCommits(bool defer_commits) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetDeferCommits(defer_commits);
}

void LayerTreeTest::DispatchSetNeedsCommit() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsCommit();
}

void LayerTreeTest::DispatchSetNeedsUpdateLayers() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsUpdateLayers();
}

void LayerTreeTest::DispatchSetNeedsRedraw() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsRedraw();
}

void LayerTreeTest::DispatchSetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsRedrawRect(damage_rect);
}

void LayerTreeTest::DispatchSetVisible(bool visible) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    SetVisibleOnLayerTreeHost(visible);
}

void LayerTreeTest::DispatchSetNextCommitForcesRedraw() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNextCommitForcesRedraw();
}

void LayerTreeTest::DispatchCompositeImmediately() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->Composite(base::TimeTicks::Now());
}

void LayerTreeTest::DispatchNextCommitWaitsForActivation() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  if (layer_tree_host_)
    layer_tree_host_->SetNextCommitWaitsForActivation();
}

void LayerTreeTest::RunTest(CompositorMode mode) {
  mode_ = mode;
  if (mode_ == CompositorMode::THREADED || mode_ == CompositorMode::REMOTE) {
    impl_thread_.reset(new base::Thread("Compositor"));
    ASSERT_TRUE(impl_thread_->Start());
  }

  shared_bitmap_manager_.reset(new TestSharedBitmapManager);
  gpu_memory_buffer_manager_.reset(new TestGpuMemoryBufferManager);
  task_graph_runner_.reset(new TestTaskGraphRunner);

  // Spend less time waiting for BeginFrame because the output is
  // mocked out.
  settings_.renderer_settings.refresh_rate = 200.0;
  settings_.background_animation_rate = 200.0;
  settings_.verify_clip_tree_calculations = true;
  settings_.verify_transform_tree_calculations = true;
  settings_.renderer_settings.buffer_to_texture_target_map =
      DefaultBufferToTextureTargetMapForTesting();
  // The TestDelegatingOutputSurface will provide a BeginFrameSource.
  settings_.use_output_surface_begin_frame_source = true;
  InitializeSettings(&settings_);
  DCHECK(settings_.use_output_surface_begin_frame_source);
  DCHECK(!settings_.use_external_begin_frame_source);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeTest::DoBeginTest, base::Unretained(this)));

  base::RunLoop().Run();
  DestroyLayerTreeHost();

  timeout_.Cancel();

  ASSERT_FALSE(layer_tree_host_.get());
  client_ = nullptr;
  if (timed_out_) {
    FAIL() << "Test timed out";
    return;
  }
  AfterTest();
}

void LayerTreeTest::RequestNewOutputSurface() {
  scoped_refptr<TestContextProvider> shared_context_provider =
      TestContextProvider::Create();
  scoped_refptr<TestContextProvider> worker_context_provider =
      TestContextProvider::CreateWorker();

  auto delegating_output_surface = CreateDelegatingOutputSurface(
      std::move(shared_context_provider), std::move(worker_context_provider));
  delegating_output_surface->SetClient(delegating_output_surface_client_.get());

  if (IsRemoteTest()) {
    DCHECK(remote_client_layer_tree_host_);
    remote_client_layer_tree_host_->SetOutputSurface(
        std::move(delegating_output_surface));
  } else {
    layer_tree_host_->SetOutputSurface(std::move(delegating_output_surface));
  }
}

std::unique_ptr<TestDelegatingOutputSurface>
LayerTreeTest::CreateDelegatingOutputSurface(
    scoped_refptr<ContextProvider> compositor_context_provider,
    scoped_refptr<ContextProvider> worker_context_provider) {
  bool synchronous_composite =
      !HasImplThread() &&
      !layer_tree_host()->settings().single_thread_proxy_scheduler;
  // Disable reclaim resources by default to act like the Display lives
  // out-of-process.
  bool force_disable_reclaim_resources = true;
  return base::MakeUnique<TestDelegatingOutputSurface>(
      compositor_context_provider, std::move(worker_context_provider),
      CreateDisplayOutputSurface(compositor_context_provider),
      shared_bitmap_manager(), gpu_memory_buffer_manager(),
      layer_tree_host()->settings().renderer_settings, ImplThreadTaskRunner(),
      synchronous_composite, force_disable_reclaim_resources);
}

std::unique_ptr<OutputSurface> LayerTreeTest::CreateDisplayOutputSurface(
    scoped_refptr<ContextProvider> compositor_context_provider) {
  // By default the Display shares a context with the LayerTreeHostImpl.
  return FakeOutputSurface::Create3d(std::move(compositor_context_provider));
}

void LayerTreeTest::DestroyLayerTreeHost() {
  if (layer_tree_host_ && layer_tree_host_->GetLayerTree()->root_layer())
    layer_tree_host_->GetLayerTree()->root_layer()->SetLayerTreeHost(NULL);
  layer_tree_host_ = nullptr;

  DCHECK(!remote_proto_channel_bridge_.channel_main.HasReceiver());

  // Destroying the LayerTreeHost should destroy the remote client
  // LayerTreeHost.
  DCHECK(!remote_client_layer_tree_host_);
}

void LayerTreeTest::DestroyRemoteClientHost() {
  DCHECK(IsRemoteTest());
  DCHECK(remote_client_layer_tree_host_);

  remote_client_layer_tree_host_ = nullptr;
  DCHECK(!remote_proto_channel_bridge_.channel_impl.HasReceiver());
}

void LayerTreeTest::CreateRemoteClientHost(
    const proto::CompositorMessageToImpl& proto) {
  DCHECK(IsRemoteTest());
  DCHECK(!remote_client_layer_tree_host_);
  DCHECK(impl_thread_);
  DCHECK(proto.message_type() ==
         proto::CompositorMessageToImpl::INITIALIZE_IMPL);

  proto::InitializeImpl initialize_proto = proto.initialize_impl_message();
  LayerTreeSettings settings;
  settings.FromProtobuf(initialize_proto.layer_tree_settings());
  settings.abort_commit_before_output_surface_creation = false;
  remote_client_layer_tree_host_ = LayerTreeHostForTesting::Create(
      this, mode_, client_.get(), &remote_proto_channel_bridge_.channel_impl,
      nullptr, nullptr, task_graph_runner_.get(), settings,
      base::ThreadTaskRunnerHandle::Get(), impl_thread_->task_runner(), nullptr,
      image_serialization_processor_.get());

  DCHECK(remote_proto_channel_bridge_.channel_impl.HasReceiver());
  DCHECK(task_runner_provider()->HasImplThread());
}

TaskRunnerProvider* LayerTreeTest::task_runner_provider() const {
  // All LayerTreeTests can use the task runner provider to access the impl
  // thread. In the remote mode, the impl thread of the compositor lives on
  // the client, so return the task runner provider owned by the remote client
  // LayerTreeHost.
  LayerTreeHost* host = IsRemoteTest() ? remote_client_layer_tree_host_.get()
                                       : layer_tree_host_.get();

  // If this fails, the test has ended and there is no task runners to find
  // anymore.
  DCHECK(host);

  return host->task_runner_provider();
}

LayerTreeHost* LayerTreeTest::layer_tree_host() {
  DCHECK(task_runner_provider()->IsMainThread() ||
         task_runner_provider()->IsMainThreadBlocked());
  return layer_tree_host_.get();
}

LayerTreeHost* LayerTreeTest::remote_client_layer_tree_host() {
  DCHECK(IsRemoteTest());
  DCHECK(task_runner_provider()->IsMainThread() ||
         task_runner_provider()->IsMainThreadBlocked());
  return remote_client_layer_tree_host_.get();
}

}  // namespace cc
