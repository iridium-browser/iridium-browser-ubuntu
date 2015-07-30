// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread_task_runner_handle.h"
#include "base/threading/simple_thread.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/resources/resource_provider.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/test/fake_delegated_renderer_layer.h"
#include "cc/test/test_context_provider.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/frame_time.h"

namespace cc {
namespace {

class NoMessageLoopOutputSurface : public OutputSurface {
 public:
  NoMessageLoopOutputSurface() : OutputSurface(TestContextProvider::Create()) {}
  ~NoMessageLoopOutputSurface() override {}

  // OutputSurface overrides.
  void SwapBuffers(CompositorFrame* frame) override {
    DCHECK(client_);
    client_->DidSwapBuffers();
    client_->DidSwapBuffersComplete();
  }
};

class LayerTreeHostNoMessageLoopTest
    : public testing::Test,
      public base::DelegateSimpleThread::Delegate,
      public LayerTreeHostClient,
      public LayerTreeHostSingleThreadClient {
 public:
  LayerTreeHostNoMessageLoopTest()
      : did_initialize_output_surface_(false),
        did_commit_(false),
        did_commit_and_draw_frame_(false),
        size_(100, 100),
        no_loop_thread_(this, "LayerTreeHostNoMessageLoopTest") {}
  ~LayerTreeHostNoMessageLoopTest() override {}

  // LayerTreeHostClient overrides.
  void WillBeginMainFrame() override {}
  void BeginMainFrame(const BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void DidBeginMainFrame() override {}
  void Layout() override {}
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void ApplyViewportDeltas(const gfx::Vector2d& scroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewOutputSurface() override {
    layer_tree_host_->SetOutputSurface(
        make_scoped_ptr<OutputSurface>(new NoMessageLoopOutputSurface));
  }
  void DidInitializeOutputSurface() override {
    did_initialize_output_surface_ = true;
  }
  void DidFailToInitializeOutputSurface() override {}
  void WillCommit() override {}
  void DidCommit() override { did_commit_ = true; }
  void DidCommitAndDrawFrame() override { did_commit_and_draw_frame_ = true; }
  void DidCompleteSwapBuffers() override {}
  void DidCompletePageScaleAnimation() override {}

  // LayerTreeHostSingleThreadClient overrides.
  void DidPostSwapBuffers() override {}
  void DidAbortSwapBuffers() override {}

  void RunTest() {
    no_loop_thread_.Start();
    no_loop_thread_.Join();
  }

  // base::DelegateSimpleThread::Delegate override.
  void Run() override {
    ASSERT_FALSE(base::ThreadTaskRunnerHandle::IsSet());
    RunTestWithoutMessageLoop();
    EXPECT_FALSE(base::ThreadTaskRunnerHandle::IsSet());
  }

 protected:
  virtual void RunTestWithoutMessageLoop() = 0;

  void SetupLayerTreeHost() {
    LayerTreeSettings settings;
    settings.single_thread_proxy_scheduler = false;
    settings.verify_property_trees = true;
    settings.raster_enabled = false;

    LayerTreeHost::InitParams params;
    params.client = this;
    params.settings = &settings;
    layer_tree_host_ = LayerTreeHost::CreateSingleThreaded(this, &params);
    layer_tree_host_->SetViewportSize(size_);
    layer_tree_host_->SetRootLayer(root_layer_);
  }

  void Composite() {
    did_commit_ = false;
    did_commit_and_draw_frame_ = false;
    layer_tree_host_->Composite(gfx::FrameTime::Now());
    EXPECT_TRUE(did_initialize_output_surface_);
    EXPECT_TRUE(did_commit_);
    EXPECT_TRUE(did_commit_and_draw_frame_);
  }

  void TearDownLayerTreeHost() {
    // Explicit teardown to make failures easier to debug.
    layer_tree_host_ = nullptr;
    root_layer_ = nullptr;
  }

  // All protected member variables are accessed only on |no_loop_thread_|.
  scoped_ptr<LayerTreeHost> layer_tree_host_;
  scoped_refptr<Layer> root_layer_;

  bool did_initialize_output_surface_;
  bool did_commit_;
  bool did_commit_and_draw_frame_;
  gfx::Size size_;

 private:
  base::DelegateSimpleThread no_loop_thread_;
};

class LayerTreeHostNoMessageLoopSmokeTest
    : public LayerTreeHostNoMessageLoopTest {
 protected:
  void RunTestWithoutMessageLoop() override {
    gfx::Size size(100, 100);

    // Set up root layer.
    {
      scoped_refptr<SolidColorLayer> solid_color_layer =
          SolidColorLayer::Create();
      solid_color_layer->SetBackgroundColor(SK_ColorRED);
      solid_color_layer->SetBounds(size_);
      solid_color_layer->SetIsDrawable(true);
      root_layer_ = solid_color_layer;
    }

    SetupLayerTreeHost();
    Composite();
    TearDownLayerTreeHost();
  }
};

TEST_F(LayerTreeHostNoMessageLoopSmokeTest, SmokeTest) {
  RunTest();
}

class LayerTreeHostNoMessageLoopDelegatedLayer
    : public LayerTreeHostNoMessageLoopTest,
      public DelegatedFrameResourceCollectionClient {
 protected:
  void RunTestWithoutMessageLoop() override {
    resource_collection_ = new DelegatedFrameResourceCollection;
    frame_provider_ = new DelegatedFrameProvider(
        resource_collection_.get(), CreateFrameDataWithResource(998));

    root_layer_ = Layer::Create();
    delegated_layer_ =
        FakeDelegatedRendererLayer::Create(frame_provider_.get());
    delegated_layer_->SetBounds(size_);
    delegated_layer_->SetIsDrawable(true);
    root_layer_->AddChild(delegated_layer_);

    SetupLayerTreeHost();

    // Draw first frame.
    Composite();

    // Prepare and draw second frame.
    frame_provider_->SetFrameData(CreateFrameDataWithResource(999));
    Composite();

    // Resource from first frame should be returned.
    CheckReturnedResource(1u);

    TearDownLayerTreeHost();
    delegated_layer_ = NULL;
    frame_provider_ = NULL;

    // Resource from second frame should be returned.
    CheckReturnedResource(1u);
    resource_collection_ = NULL;
  }

  // DelegatedFrameResourceCollectionClient overrides.
  void UnusedResourcesAreAvailable() override {}

 private:
  scoped_ptr<DelegatedFrameData> CreateFrameDataWithResource(
      ResourceProvider::ResourceId resource_id) {
    scoped_ptr<DelegatedFrameData> frame(new DelegatedFrameData);
    gfx::Rect frame_rect(size_);

    scoped_ptr<RenderPass> root_pass(RenderPass::Create());
    root_pass->SetNew(
        RenderPassId(1, 1), frame_rect, frame_rect, gfx::Transform());
    frame->render_pass_list.push_back(root_pass.Pass());

    TransferableResource resource;
    resource.id = resource_id;
    resource.mailbox_holder.texture_target = GL_TEXTURE_2D;
    resource.mailbox_holder.mailbox = gpu::Mailbox::Generate();
    frame->resource_list.push_back(resource);

    return frame.Pass();
  }

  void CheckReturnedResource(size_t expected_num) {
    ReturnedResourceArray returned_resources;
    resource_collection_->TakeUnusedResourcesForChildCompositor(
        &returned_resources);
    EXPECT_EQ(expected_num, returned_resources.size());
  }

  scoped_refptr<DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<DelegatedFrameProvider> frame_provider_;
  scoped_refptr<DelegatedRendererLayer> delegated_layer_;
};

TEST_F(LayerTreeHostNoMessageLoopDelegatedLayer, SingleDelegatedLayer) {
  RunTest();
}

}  // namespace
}  // namespace cc
