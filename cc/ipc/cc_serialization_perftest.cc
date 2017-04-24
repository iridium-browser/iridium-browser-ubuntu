// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "cc/ipc/cc_param_traits.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/ipc/compositor_frame_metadata_struct_traits.h"
#include "cc/ipc/compositor_frame_struct_traits.h"
#include "cc/ipc/render_pass_struct_traits.h"
#include "cc/ipc/selection_struct_traits.h"
#include "cc/ipc/shared_quad_state_struct_traits.h"
#include "cc/ipc/surface_id_struct_traits.h"
#include "cc/ipc/transferable_resource_struct_traits.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/picture_draw_quad.h"
#include "gpu/ipc/common/mailbox_holder_struct_traits.h"
#include "gpu/ipc/common/mailbox_struct_traits.h"
#include "gpu/ipc/common/sync_token_struct_traits.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "ui/events/mojo/latency_info_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry.mojom.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/mojo/selection_bound_struct_traits.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kNumWarmupRuns = 20;
static const int kTimeCheckInterval = 10;

enum class UseSingleSharedQuadState { YES, NO };

class CCSerializationPerfTest : public testing::Test {
 protected:
  static bool ReadMessage(const IPC::Message* msg, CompositorFrame* frame) {
    base::PickleIterator iter(*msg);
    return IPC::ParamTraits<CompositorFrame>::Read(msg, &iter, frame);
  }

  static void RunDeserializationTestParamTraits(
      const std::string& test_name,
      const CompositorFrame& frame,
      UseSingleSharedQuadState single_sqs) {
    IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
    IPC::ParamTraits<CompositorFrame>::Write(&msg, frame);
    for (int i = 0; i < kNumWarmupRuns; ++i) {
      CompositorFrame compositor_frame;
      ReadMessage(&msg, &compositor_frame);
    }

    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks end =
        start + base::TimeDelta::FromMilliseconds(kTimeLimitMillis);
    base::TimeTicks now = start;
    base::TimeDelta min_time;
    size_t count = 0;
    while (start < end) {
      for (int i = 0; i < kTimeCheckInterval; ++i) {
        CompositorFrame compositor_frame;
        ReadMessage(&msg, &compositor_frame);
        now = base::TimeTicks::Now();
        // We don't count iterations after the end time.
        if (now < end)
          ++count;
      }

      if (now - start < min_time || min_time.is_zero())
        min_time = now - start;
      start = now;
    }

    perf_test::PrintResult(
        "ParamTraits deserialization: min_frame_deserialization_time",
        single_sqs == UseSingleSharedQuadState::YES
            ? "_per_render_pass_shared_quad_state"
            : "_per_quad_shared_quad_state",
        test_name, min_time.InMillisecondsF() / kTimeCheckInterval * 1000, "us",
        true);
    perf_test::PrintResult("ParamTraits deserialization: num runs in 2 seconds",
                           single_sqs == UseSingleSharedQuadState::YES
                               ? "_per_render_pass_shared_quad_state"
                               : "_per_quad_shared_quad_state",
                           test_name, count, "", true);
  }

  static void RunSerializationTestParamTraits(
      const std::string& test_name,
      const CompositorFrame& frame,
      UseSingleSharedQuadState single_sqs) {
    for (int i = 0; i < kNumWarmupRuns; ++i) {
      IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
      IPC::ParamTraits<CompositorFrame>::Write(&msg, frame);
    }

    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks end =
        start + base::TimeDelta::FromMilliseconds(kTimeLimitMillis);
    base::TimeTicks now = start;
    base::TimeDelta min_time;
    size_t count = 0;
    while (start < end) {
      for (int i = 0; i < kTimeCheckInterval; ++i) {
        IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
        IPC::ParamTraits<CompositorFrame>::Write(&msg, frame);
        now = base::TimeTicks::Now();
        // We don't count iterations after the end time.
        if (now < end)
          ++count;
      }

      if (now - start < min_time || min_time.is_zero())
        min_time = now - start;
      start = now;
    }

    perf_test::PrintResult(
        "ParamTraits serialization: min_frame_serialization_time",
        single_sqs == UseSingleSharedQuadState::YES
            ? "_per_render_pass_shared_quad_state"
            : "_per_quad_shared_quad_state",
        test_name, min_time.InMillisecondsF() / kTimeCheckInterval * 1000, "us",
        true);
    perf_test::PrintResult("ParamTraits serialization: num runs in 2 seconds",
                           single_sqs == UseSingleSharedQuadState::YES
                               ? "_per_render_pass_shared_quad_state"
                               : "_per_quad_shared_quad_state",
                           test_name, count, "", true);
  }

  static void RunDeserializationTestStructTraits(
      const std::string& test_name,
      const CompositorFrame& frame,
      UseSingleSharedQuadState single_sqs) {
    auto data = mojom::CompositorFrame::Serialize(&frame);
    DCHECK_GT(data.size(), 0u);
    for (int i = 0; i < kNumWarmupRuns; ++i) {
      CompositorFrame compositor_frame;
      mojom::CompositorFrame::Deserialize(data, &compositor_frame);
    }

    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks end =
        start + base::TimeDelta::FromMilliseconds(kTimeLimitMillis);
    base::TimeTicks now = start;
    base::TimeDelta min_time;
    size_t count = 0;
    while (start < end) {
      for (int i = 0; i < kTimeCheckInterval; ++i) {
        CompositorFrame compositor_frame;
        mojom::CompositorFrame::Deserialize(data, &compositor_frame);
        now = base::TimeTicks::Now();
        // We don't count iterations after the end time.
        if (now < end)
          ++count;
      }

      if (now - start < min_time || min_time.is_zero())
        min_time = now - start;
      start = now;
    }

    perf_test::PrintResult(
        "StructTraits deserialization min_frame_deserialization_time",
        single_sqs == UseSingleSharedQuadState::YES
            ? "_per_render_pass_shared_quad_state"
            : "_per_quad_shared_quad_state",
        test_name, min_time.InMillisecondsF() / kTimeCheckInterval * 1000, "us",
        true);
    perf_test::PrintResult(
        "StructTraits deserialization: num runs in 2 seconds",
        single_sqs == UseSingleSharedQuadState::YES
            ? "_per_render_pass_shared_quad_state"
            : "_per_quad_shared_quad_state",
        test_name, count, "", true);
  }

  static void RunSerializationTestStructTraits(
      const std::string& test_name,
      const CompositorFrame& frame,
      UseSingleSharedQuadState single_sqs) {
    for (int i = 0; i < kNumWarmupRuns; ++i) {
      auto data = mojom::CompositorFrame::Serialize(&frame);
      DCHECK_GT(data.size(), 0u);
    }

    base::TimeTicks start = base::TimeTicks::Now();
    base::TimeTicks end =
        start + base::TimeDelta::FromMilliseconds(kTimeLimitMillis);
    base::TimeTicks now = start;
    base::TimeDelta min_time;
    size_t count = 0;
    while (start < end) {
      for (int i = 0; i < kTimeCheckInterval; ++i) {
        auto data = mojom::CompositorFrame::Serialize(&frame);
        DCHECK_GT(data.size(), 0u);
        now = base::TimeTicks::Now();
        // We don't count iterations after the end time.
        if (now < end)
          ++count;
      }

      if (now - start < min_time || min_time.is_zero())
        min_time = now - start;
      start = now;
    }

    perf_test::PrintResult(
        "StructTraits serialization min_frame_serialization_time",
        single_sqs == UseSingleSharedQuadState::YES
            ? "_per_render_pass_shared_quad_state"
            : "_per_quad_shared_quad_state",
        test_name, min_time.InMillisecondsF() / kTimeCheckInterval * 1000, "us",
        true);
    perf_test::PrintResult("StructTraits serialization: num runs in 2 seconds",
                           single_sqs == UseSingleSharedQuadState::YES
                               ? "_per_render_pass_shared_quad_state"
                               : "_per_quad_shared_quad_state",
                           test_name, count, "", true);
  }

  static void RunCompositorFrameTest(const std::string& test_name,
                                     uint32_t num_quads,
                                     uint32_t num_passes,
                                     UseSingleSharedQuadState single_sqs) {
    CompositorFrame frame;

    for (uint32_t i = 0; i < num_passes; ++i) {
      std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
      render_pass->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
      for (uint32_t j = 0; j < num_quads; ++j) {
        if (j == 0 || single_sqs == UseSingleSharedQuadState::NO)
          render_pass->CreateAndAppendSharedQuadState();
        const gfx::Rect bounds(100, 100, 100, 100);
        const bool kForceAntiAliasingOff = true;
        SolidColorDrawQuad* quad =
            render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
        quad->SetNew(render_pass->shared_quad_state_list.back(), bounds, bounds,
                     SK_ColorRED, kForceAntiAliasingOff);
      }
      frame.render_pass_list.push_back(std::move(render_pass));
    }
    RunTest(test_name, std::move(frame), single_sqs);
  }

  static void RunTest(const std::string& test_name,
                      CompositorFrame frame,
                      UseSingleSharedQuadState single_sqs) {
    RunSerializationTestStructTraits(test_name, frame, single_sqs);
    RunDeserializationTestStructTraits(test_name, frame, single_sqs);
    RunSerializationTestParamTraits(test_name, frame, single_sqs);
    RunDeserializationTestParamTraits(test_name, frame, single_sqs);
  }
};

// Test for compositor frames with one render pass and 4000 quads.
TEST_F(CCSerializationPerfTest, DelegatedFrame_ManyQuads_1_4000) {
  // Case 1: One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyQuads_1_4000", 4000, 1,
                         UseSingleSharedQuadState::YES);
  // Case 2: One shared quad state for each quad.
  RunCompositorFrameTest("DelegatedFrame_ManyQuads_1_4000", 4000, 1,
                         UseSingleSharedQuadState::NO);
}

// Test for compositor frames with one render pass and 100000 quads.
TEST_F(CCSerializationPerfTest, DelegatedFrame_ManyQuads_1_100000) {
  // Case 1: One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyQuads_1_100000", 100000, 1,
                         UseSingleSharedQuadState::YES);
  // Case 2: One shared quad state for each quad.
  RunCompositorFrameTest("DelegatedFrame_ManyQuads_1_100000", 100000, 1,
                         UseSingleSharedQuadState::NO);
}

// Test for compositor frames with 100 render pass and each with 4000 quads.
TEST_F(CCSerializationPerfTest, DelegatedFrame_ManyQuads_100_4000) {
  // One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyQuads_100_4000", 4000, 100,
                         UseSingleSharedQuadState::YES);
}

// Done for https://crbug.com/691730. Test is too slow as is.
#if defined(OS_ANDROID)
#define MAYBE_DelegatedFrame_ManyQuads_10_100000 \
  DISABLED_DelegatedFrame_ManyQuads_10_100000
#else
#define MAYBE_DelegatedFrame_ManyQuads_10_100000 \
  DelegatedFrame_ManyQuads_10_100000
#endif
// Test for compositor frames with 10 render pass and each with 100000 quads.
TEST_F(CCSerializationPerfTest, MAYBE_DelegatedFrame_ManyQuads_10_100000) {
  // One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyQuads_10_100000", 100000, 10,
                         UseSingleSharedQuadState::YES);
}

// Test for compositor frames with 5 render pass and each with 100 quads.
TEST_F(CCSerializationPerfTest, DelegatedFrame_ManyRenderPasses_5_100) {
  // Case 1: One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_5_100", 100, 5,
                         UseSingleSharedQuadState::YES);
  // Case 2: One shared quad state for each quad.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_5_100", 100, 5,
                         UseSingleSharedQuadState::NO);
}

// Test for compositor frames with 10 render pass and each with 500 quads.
TEST_F(CCSerializationPerfTest, DelegatedFrame_ManyRenderPasses_10_500) {
  // Case 1: One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_10_500", 500, 10,
                         UseSingleSharedQuadState::YES);
  // Case 2: One shared quad state for each quad.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_10_500", 500, 10,
                         UseSingleSharedQuadState::NO);
}

// Test for compositor frames with 1000 render pass and each with 100 quads.
TEST_F(CCSerializationPerfTest, DelegatedFrame_ManyRenderPasses_1000_100) {
  // Case 1: One shared quad state for all quads in one render pass.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_1000_100", 100, 1000,
                         UseSingleSharedQuadState::YES);
  // Case 2: One shared quad state for each quad.
  RunCompositorFrameTest("DelegatedFrame_ManyRenderPasses_1000_100", 100, 1000,
                         UseSingleSharedQuadState::NO);
}

}  // namespace
}  // namespace cc
