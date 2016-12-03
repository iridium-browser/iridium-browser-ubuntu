// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_TEST_H_
#define CC_TEST_LAYER_TREE_TEST_H_

#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "cc/animation/animation_delegate.h"
#include "cc/test/remote_proto_channel_bridge.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_hooks.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
class AnimationPlayer;
class FakeLayerTreeHostClient;
class FakeOutputSurface;
class LayerImpl;
class LayerTreeHost;
class LayerTreeHostForTesting;
class LayerTreeHostClient;
class LayerTreeHostImpl;
class LayerTreeTestDelegatingOutputSurfaceClient;
class ProxyImpl;
class ProxyMain;
class RemoteChannelImplForTest;
class TestContextProvider;
class TestDelegatingOutputSurface;
class TestGpuMemoryBufferManager;
class TestTaskGraphRunner;
class TestWebGraphicsContext3D;

// Creates the virtual viewport layer hierarchy under the given root_layer.
// Convenient overload of the method below that creates a scrolling layer as
// the outer viewport scroll layer.
void CreateVirtualViewportLayers(Layer* root_layer,
                                 const gfx::Size& inner_bounds,
                                 const gfx::Size& outer_bounds,
                                 const gfx::Size& scroll_bounds,
                                 LayerTreeHost* host);

// Creates the virtual viewport layer hierarchy under the given root_layer.
// Uses the given scroll layer as the content "outer viewport scroll layer".
void CreateVirtualViewportLayers(Layer* root_layer,
                                 scoped_refptr<Layer> outer_scroll_layer,
                                 const gfx::Size& outer_bounds,
                                 const gfx::Size& scroll_bounds,
                                 LayerTreeHost* host);

class BeginTask;
class LayerTreeHostClientForTesting;
class TimeoutTask;

// The LayerTreeTests runs with the main loop running. It instantiates a single
// LayerTreeHostForTesting and associated LayerTreeHostImplForTesting and
// LayerTreeHostClientForTesting.
//
// BeginTest() is called once the main message loop is running and the layer
// tree host is initialized.
//
// Key stages of the drawing loop, e.g. drawing or commiting, redirect to
// LayerTreeTest methods of similar names. To track the commit process, override
// these functions.
//
// The test continues until someone calls EndTest. EndTest can be called on any
// thread, but be aware that ending the test is an asynchronous process.
class LayerTreeTest : public testing::Test, public TestHooks {
 public:
  ~LayerTreeTest() override;

  virtual void EndTest();
  void EndTestAfterDelayMs(int delay_milliseconds);

  void PostAddAnimationToMainThreadPlayer(
      AnimationPlayer* player_to_receive_animation);
  void PostAddInstantAnimationToMainThreadPlayer(
      AnimationPlayer* player_to_receive_animation);
  void PostAddLongAnimationToMainThreadPlayer(
      AnimationPlayer* player_to_receive_animation);
  void PostSetDeferCommitsToMainThread(bool defer_commits);
  void PostSetNeedsCommitToMainThread();
  void PostSetNeedsUpdateLayersToMainThread();
  void PostSetNeedsRedrawToMainThread();
  void PostSetNeedsRedrawRectToMainThread(const gfx::Rect& damage_rect);
  void PostSetVisibleToMainThread(bool visible);
  void PostSetNextCommitForcesRedrawToMainThread();
  void PostCompositeImmediatelyToMainThread();
  void PostNextCommitWaitsForActivationToMainThread();

  void DoBeginTest();
  void Timeout();

 protected:
  LayerTreeTest();

  virtual void InitializeSettings(LayerTreeSettings* settings) {}

  void RealEndTest();

  virtual void DispatchAddAnimationToPlayer(
      AnimationPlayer* player_to_receive_animation,
      double animation_duration);
  void DispatchSetDeferCommits(bool defer_commits);
  void DispatchSetNeedsCommit();
  void DispatchSetNeedsUpdateLayers();
  void DispatchSetNeedsRedraw();
  void DispatchSetNeedsRedrawRect(const gfx::Rect& damage_rect);
  void DispatchSetVisible(bool visible);
  void DispatchSetNextCommitForcesRedraw();
  void DispatchDidAddAnimation();
  void DispatchCompositeImmediately();
  void DispatchNextCommitWaitsForActivation();

  std::unique_ptr<OutputSurface> ReleaseOutputSurfaceOnLayerTreeHost();
  void SetVisibleOnLayerTreeHost(bool visible);

  virtual void AfterTest() = 0;
  virtual void WillBeginTest();
  virtual void BeginTest() = 0;
  virtual void SetupTree();

  virtual void RunTest(CompositorMode mode);

  bool HasImplThread() const { return !!impl_thread_; }
  base::SingleThreadTaskRunner* ImplThreadTaskRunner() {
    return impl_task_runner_.get();
  }
  base::SingleThreadTaskRunner* MainThreadTaskRunner() {
    return main_task_runner_.get();
  }
  Proxy* proxy() const {
    return layer_tree_host_ ? layer_tree_host_->proxy() : NULL;
  }
  Proxy* remote_client_proxy() const;
  TaskRunnerProvider* task_runner_provider() const;
  TaskGraphRunner* task_graph_runner() const {
    return task_graph_runner_.get();
  }
  bool TestEnded() const { return ended_; }

  LayerTreeHost* layer_tree_host();
  LayerTree* layer_tree() { return layer_tree_host()->GetLayerTree(); }
  LayerTreeHost* remote_client_layer_tree_host();
  SharedBitmapManager* shared_bitmap_manager() const {
    return shared_bitmap_manager_.get();
  }
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() {
    return gpu_memory_buffer_manager_.get();
  }

  void DestroyLayerTreeHost();
  void DestroyRemoteClientHost() override;

  void CreateRemoteClientHost(
      const proto::CompositorMessageToImpl& proto) override;

  // By default, output surface recreation is synchronous.
  void RequestNewOutputSurface() override;
  // Override this and call the base class to change what ContextProviders will
  // be used (such as for pixel tests). Or override it and create your own
  // TestDelegatingOutputSurface to control how it is created.
  virtual std::unique_ptr<TestDelegatingOutputSurface>
  CreateDelegatingOutputSurface(
      scoped_refptr<ContextProvider> compositor_context_provider,
      scoped_refptr<ContextProvider> worker_context_provider);
  // Override this and call the base class to change what ContextProvider will
  // be used, such as to prevent sharing the context with the delegating
  // OutputSurface. Or override it and create your own OutputSurface to change
  // what type of OutputSurface is used, such as a real OutputSurface for pixel
  // tests or a software-compositing OutputSurface.
  virtual std::unique_ptr<OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<ContextProvider> compositor_context_provider);

  bool IsRemoteTest() const;

  gfx::Vector2dF ScrollDelta(LayerImpl* layer_impl);

 private:
  LayerTreeSettings settings_;

  CompositorMode mode_;

  std::unique_ptr<LayerTreeHostClientForTesting> client_;
  std::unique_ptr<LayerTreeHost> layer_tree_host_;

  // The LayerTreeHost created by the cc embedder on the client in remote mode.
  std::unique_ptr<LayerTreeHostForTesting> remote_client_layer_tree_host_;

  RemoteProtoChannelBridge remote_proto_channel_bridge_;

  std::unique_ptr<ImageSerializationProcessor> image_serialization_processor_;

  bool beginning_ = false;
  bool end_when_begin_returns_ = false;
  bool timed_out_ = false;
  bool scheduled_ = false;
  bool started_ = false;
  bool ended_ = false;

  int timeout_seconds_ = false;

  std::unique_ptr<LayerTreeTestDelegatingOutputSurfaceClient>
      delegating_output_surface_client_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner_;
  std::unique_ptr<base::Thread> impl_thread_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<TestGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<TestTaskGraphRunner> task_graph_runner_;
  base::CancelableClosure timeout_;
  scoped_refptr<TestContextProvider> compositor_contexts_;
  base::WeakPtr<LayerTreeTest> main_thread_weak_ptr_;
  base::WeakPtrFactory<LayerTreeTest> weak_factory_;
};

}  // namespace cc

#define SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME)                   \
  TEST_F(TEST_FIXTURE_NAME, RunSingleThread_DelegatingRenderer) { \
    RunTest(CompositorMode::SINGLE_THREADED);                     \
  }                                                               \
  class SingleThreadDelegatingImplNeedsSemicolon##TEST_FIXTURE_NAME {}

#define MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)                   \
  TEST_F(TEST_FIXTURE_NAME, RunMultiThread_DelegatingRenderer) { \
    RunTest(CompositorMode::THREADED);                           \
  }                                                              \
  class MultiThreadDelegatingImplNeedsSemicolon##TEST_FIXTURE_NAME {}

#define SINGLE_AND_MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME) \
  SINGLE_THREAD_TEST_F(TEST_FIXTURE_NAME);                \
  MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)

// Some tests want to control when notify ready for activation occurs,
// but this is not supported in the single-threaded case.
#define MULTI_THREAD_BLOCKNOTIFY_TEST_F(TEST_FIXTURE_NAME) \
  MULTI_THREAD_TEST_F(TEST_FIXTURE_NAME)

#endif  // CC_TEST_LAYER_TREE_TEST_H_
