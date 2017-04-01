// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <stdint.h>

#include <sstream>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "cc/debug/lap_timer.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_json_parser.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/paths.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/perf/perf_test.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class LayerTreeHostPerfTest : public LayerTreeTest {
 public:
  LayerTreeHostPerfTest()
      : draw_timer_(kWarmupRuns,
                    base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
                    kTimeCheckInterval),
        commit_timer_(0, base::TimeDelta(), 1),
        full_damage_each_frame_(false),
        begin_frame_driven_drawing_(false),
        measure_commit_cost_(false) {
  }

  void InitializeSettings(LayerTreeSettings* settings) override {
    // LayerTreeTests give the Display's BeginFrameSource directly to the
    // LayerTreeHost like we do in the Browser process via
    // TestDelegatingOutputSurface, so setting disable_display_vsync here
    // unthrottles both the DisplayScheduler and the Scheduler.
    settings->renderer_settings.disable_display_vsync = true;
  }

  void BeginTest() override {
    BuildTree();
    PostSetNeedsCommitToMainThread();
  }

  void BeginMainFrame(const BeginFrameArgs& args) override {
    if (begin_frame_driven_drawing_ && !TestEnded()) {
      layer_tree_host()->SetNeedsAnimate();
      layer_tree_host()->SetNextCommitForcesRedraw();
    }
  }

  void BeginCommitOnThread(LayerTreeHostImpl* host_impl) override {
    if (measure_commit_cost_)
      commit_timer_.Start();
  }

  void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) override {
    if (measure_commit_cost_ && draw_timer_.IsWarmedUp()) {
      commit_timer_.NextLap();
    }
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    if (TestEnded() || CleanUpStarted())
      return;
    draw_timer_.NextLap();
    if (draw_timer_.HasTimeLimitExpired()) {
      CleanUpAndEndTest();
      return;
    }
    if (!begin_frame_driven_drawing_)
      host_impl->SetNeedsRedraw();
    if (full_damage_each_frame_)
      host_impl->SetFullViewportDamage();
  }

  virtual void CleanUpAndEndTest() { EndTest(); }

  virtual bool CleanUpStarted() { return false; }

  virtual void BuildTree() {}

  void AfterTest() override {
    CHECK(!test_name_.empty()) << "Must SetTestName() before AfterTest().";
    perf_test::PrintResult("layer_tree_host_frame_time", "", test_name_,
                           1000 * draw_timer_.MsPerLap(), "us", true);
    if (measure_commit_cost_) {
      perf_test::PrintResult("layer_tree_host_commit_time", "", test_name_,
                             1000 * commit_timer_.MsPerLap(), "us", true);
    }
  }

 protected:
  LapTimer draw_timer_;
  LapTimer commit_timer_;

  std::string test_name_;
  FakeContentLayerClient fake_content_layer_client_;
  bool full_damage_each_frame_;
  bool begin_frame_driven_drawing_;

  bool measure_commit_cost_;
};


class LayerTreeHostPerfTestJsonReader : public LayerTreeHostPerfTest {
 public:
  LayerTreeHostPerfTestJsonReader()
      : LayerTreeHostPerfTest() {
  }

  void SetTestName(const std::string& name) {
    test_name_ = name;
  }

  void ReadTestFile(const std::string& name) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(CCPaths::DIR_TEST_DATA, &test_data_dir));
    base::FilePath json_file = test_data_dir.AppendASCII(name + ".json");
    ASSERT_TRUE(base::ReadFileToString(json_file, &json_));
  }

  void BuildTree() override {
    gfx::Size viewport = gfx::Size(720, 1038);
    layer_tree()->SetViewportSize(viewport);
    scoped_refptr<Layer> root = ParseTreeFromJson(json_,
                                                  &fake_content_layer_client_);
    ASSERT_TRUE(root.get());
    layer_tree()->SetRootLayer(root);
    fake_content_layer_client_.set_bounds(viewport);
  }

 private:
  std::string json_;
};

// Simulates a tab switcher scene with two stacks of 10 tabs each.
TEST_F(LayerTreeHostPerfTestJsonReader, TenTenSingleThread) {
  SetTestName("10_10_single_thread");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::SINGLE_THREADED);
}

TEST_F(LayerTreeHostPerfTestJsonReader, TenTenThreaded) {
  SetTestName("10_10_threaded_impl_side");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::THREADED);
}

// Simulates a tab switcher scene with two stacks of 10 tabs each.
TEST_F(LayerTreeHostPerfTestJsonReader,
       TenTenSingleThread_FullDamageEachFrame) {
  full_damage_each_frame_ = true;
  SetTestName("10_10_single_thread_full_damage_each_frame");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::SINGLE_THREADED);
}

TEST_F(LayerTreeHostPerfTestJsonReader, TenTenThreaded_FullDamageEachFrame) {
  full_damage_each_frame_ = true;
  SetTestName("10_10_threaded_impl_side_full_damage_each_frame");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::THREADED);
}

// Invalidates a leaf layer in the tree on the main thread after every commit.
class LayerTreeHostPerfTestLeafInvalidates
    : public LayerTreeHostPerfTestJsonReader {
 public:
  void BuildTree() override {
    LayerTreeHostPerfTestJsonReader::BuildTree();

    // Find a leaf layer.
    for (layer_to_invalidate_ = layer_tree()->root_layer();
         layer_to_invalidate_->children().size();
         layer_to_invalidate_ = layer_to_invalidate_->children()[0].get()) {
    }
  }

  void DidCommitAndDrawFrame() override {
    if (TestEnded())
      return;

    layer_to_invalidate_->SetOpacity(
        layer_to_invalidate_->opacity() != 1.f ? 1.f : 0.5f);
  }

 protected:
  Layer* layer_to_invalidate_;
};

// Simulates a tab switcher scene with two stacks of 10 tabs each. Invalidate a
// property on a leaf layer in the tree every commit.
TEST_F(LayerTreeHostPerfTestLeafInvalidates, TenTenSingleThread) {
  SetTestName("10_10_single_thread_leaf_invalidates");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::SINGLE_THREADED);
}

TEST_F(LayerTreeHostPerfTestLeafInvalidates, TenTenThreaded) {
  SetTestName("10_10_threaded_impl_side_leaf_invalidates");
  ReadTestFile("10_10_layer_tree");
  RunTest(CompositorMode::THREADED);
}

// Simulates main-thread scrolling on each frame.
class ScrollingLayerTreePerfTest : public LayerTreeHostPerfTestJsonReader {
 public:
  ScrollingLayerTreePerfTest()
      : LayerTreeHostPerfTestJsonReader() {
  }

  void BuildTree() override {
    LayerTreeHostPerfTestJsonReader::BuildTree();
    scrollable_ = layer_tree()->root_layer()->children()[1];
    ASSERT_TRUE(scrollable_.get());
  }

  void UpdateLayerTreeHost() override {
    if (TestEnded())
      return;
    static const gfx::Vector2d delta = gfx::Vector2d(0, 10);
    scrollable_->SetScrollOffset(
        gfx::ScrollOffsetWithDelta(scrollable_->scroll_offset(), delta));
  }

 private:
  scoped_refptr<Layer> scrollable_;
};

TEST_F(ScrollingLayerTreePerfTest, LongScrollablePageSingleThread) {
  SetTestName("long_scrollable_page");
  ReadTestFile("long_scrollable_page");
  RunTest(CompositorMode::SINGLE_THREADED);
}

TEST_F(ScrollingLayerTreePerfTest, LongScrollablePageThreaded) {
  SetTestName("long_scrollable_page_threaded_impl_side");
  ReadTestFile("long_scrollable_page");
  RunTest(CompositorMode::THREADED);
}

// Simulates main-thread scrolling on each frame.
class BrowserCompositorInvalidateLayerTreePerfTest
    : public LayerTreeHostPerfTestJsonReader {
 public:
  BrowserCompositorInvalidateLayerTreePerfTest() = default;

  void BuildTree() override {
    LayerTreeHostPerfTestJsonReader::BuildTree();
    tab_contents_ = static_cast<TextureLayer*>(layer_tree()
                                                   ->root_layer()
                                                   ->children()[0]
                                                   ->children()[0]
                                                   ->children()[0]
                                                   ->children()[0]
                                                   .get());
    ASSERT_TRUE(tab_contents_.get());
  }

  void WillCommit() override {
    if (CleanUpStarted())
      return;
    gpu::Mailbox gpu_mailbox;
    std::ostringstream name_stream;
    name_stream << "name" << next_fence_sync_;
    gpu_mailbox.SetName(
        reinterpret_cast<const int8_t*>(name_stream.str().c_str()));
    std::unique_ptr<SingleReleaseCallback> callback =
        SingleReleaseCallback::Create(base::Bind(
            &BrowserCompositorInvalidateLayerTreePerfTest::ReleaseMailbox,
            base::Unretained(this)));

    gpu::SyncToken next_sync_token(gpu::CommandBufferNamespace::GPU_IO, 0,
                                   gpu::CommandBufferId::FromUnsafeValue(1),
                                   next_fence_sync_);
    next_sync_token.SetVerifyFlush();
    TextureMailbox mailbox(gpu_mailbox, next_sync_token, GL_TEXTURE_2D);
    next_fence_sync_++;

    tab_contents_->SetTextureMailbox(mailbox, std::move(callback));
    ++sent_mailboxes_count_;
    tab_contents_->SetNeedsDisplay();
  }

  void DidCommit() override {
    if (CleanUpStarted())
      return;
    layer_tree_host()->SetNeedsCommit();
  }

  void CleanUpAndEndTest() override {
    clean_up_started_ = true;
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&BrowserCompositorInvalidateLayerTreePerfTest::
                        CleanUpAndEndTestOnMainThread,
                   base::Unretained(this)));
  }

  void CleanUpAndEndTestOnMainThread() {
    tab_contents_->SetTextureMailbox(TextureMailbox(), nullptr);
    // ReleaseMailbox will end the test when we get the last mailbox back.
  }

  void ReleaseMailbox(const gpu::SyncToken& sync_token, bool lost_resource) {
    ++released_mailboxes_count_;
    if (released_mailboxes_count_ == sent_mailboxes_count_) {
      DCHECK(CleanUpStarted());
      EndTest();
    }
  }

  bool CleanUpStarted() override { return clean_up_started_; }

 private:
  scoped_refptr<TextureLayer> tab_contents_;
  uint64_t next_fence_sync_ = 1;
  bool clean_up_started_ = false;
  int sent_mailboxes_count_ = 0;
  int released_mailboxes_count_ = 0;
};

TEST_F(BrowserCompositorInvalidateLayerTreePerfTest, DenseBrowserUIThreaded) {
  measure_commit_cost_ = true;
  SetTestName("dense_layer_tree");
  ReadTestFile("dense_layer_tree");
  RunTest(CompositorMode::THREADED);
}

// Simulates a page with several large, transformed and animated layers.
TEST_F(LayerTreeHostPerfTestJsonReader, HeavyPageThreaded) {
  begin_frame_driven_drawing_ = true;
  measure_commit_cost_ = true;
  SetTestName("heavy_page");
  ReadTestFile("heavy_layer_tree");
  RunTest(CompositorMode::THREADED);
}

}  // namespace
}  // namespace cc
