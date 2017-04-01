// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryRootFrameSinkId(1, 1);
static constexpr FrameSinkId kArbitraryChildFrameSinkId(2, 2);
static constexpr FrameSinkId kArbitraryLeftFrameSinkId(3, 3);
static constexpr FrameSinkId kArbitraryRightFrameSinkId(4, 4);

class EmptySurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  void ReturnResources(const ReturnedResourceArray& resources) override {}
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {}
};

class SurfacesPixelTest : public RendererPixelTest<GLRenderer> {
 public:
  SurfacesPixelTest()
      : factory_(kArbitraryRootFrameSinkId, &manager_, &client_) {}
  ~SurfacesPixelTest() override { factory_.EvictSurface(); }

 protected:
  SurfaceManager manager_;
  SurfaceIdAllocator allocator_;
  EmptySurfaceFactoryClient client_;
  SurfaceFactory factory_;
};

SharedQuadState* CreateAndAppendTestSharedQuadState(
    RenderPass* render_pass,
    const gfx::Transform& transform,
    const gfx::Size& size) {
  const gfx::Size layer_bounds = size;
  const gfx::Rect visible_layer_rect = gfx::Rect(size);
  const gfx::Rect clip_rect = gfx::Rect(size);
  bool is_clipped = false;
  float opacity = 1.f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, layer_bounds, visible_layer_rect, clip_rect,
                       is_clipped, opacity, blend_mode, 0);
  return shared_state;
}

// Draws a very simple frame with no surface references.
TEST_F(SurfacesPixelTest, DrawSimpleFrame) {
  gfx::Rect rect(device_viewport_size_);
  int id = 1;
  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(id, rect, rect, gfx::Transform());

  CreateAndAppendTestSharedQuadState(
      pass.get(), gfx::Transform(), device_viewport_size_);

  SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bool force_anti_aliasing_off = false;
  color_quad->SetNew(pass->shared_quad_state_list.back(),
                     rect,
                     rect,
                     SK_ColorGREEN,
                     force_anti_aliasing_off);


  CompositorFrame root_frame;
  root_frame.render_pass_list.push_back(std::move(pass));

  LocalFrameId root_local_frame_id = allocator_.GenerateId();
  SurfaceId root_surface_id(factory_.frame_sink_id(), root_local_frame_id);
  factory_.SubmitCompositorFrame(root_local_frame_id, std::move(root_frame),
                                 SurfaceFactory::DrawCallback());

  SurfaceAggregator aggregator(&manager_, resource_provider_.get(), true);
  CompositorFrame aggregated_frame = aggregator.Aggregate(root_surface_id);

  bool discard_alpha = false;
  ExactPixelComparator pixel_comparator(discard_alpha);
  RenderPassList* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(RunPixelTest(pass_list,
                           base::FilePath(FILE_PATH_LITERAL("green.png")),
                           pixel_comparator));
}

// Draws a frame with simple surface embedding.
TEST_F(SurfacesPixelTest, DrawSimpleAggregatedFrame) {
  gfx::Size child_size(200, 100);
  SurfaceFactory child_factory(kArbitraryChildFrameSinkId, &manager_, &client_);
  LocalFrameId child_local_frame_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_factory.frame_sink_id(),
                             child_local_frame_id);
  LocalFrameId root_local_frame_id = allocator_.GenerateId();
  SurfaceId root_surface_id(factory_.frame_sink_id(), root_local_frame_id);

  {
    gfx::Rect rect(device_viewport_size_);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(
        pass.get(), gfx::Transform(), device_viewport_size_);

    SurfaceDrawQuad* surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetNew(pass->shared_quad_state_list.back(),
                         gfx::Rect(child_size),
                         gfx::Rect(child_size),
                         child_surface_id);

    SolidColorDrawQuad* color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    color_quad->SetNew(pass->shared_quad_state_list.back(),
                       rect,
                       rect,
                       SK_ColorYELLOW,
                       force_anti_aliasing_off);

    CompositorFrame root_frame;
    root_frame.render_pass_list.push_back(std::move(pass));

    factory_.SubmitCompositorFrame(root_local_frame_id, std::move(root_frame),
                                   SurfaceFactory::DrawCallback());
  }

  {
    gfx::Rect rect(child_size);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(
        pass.get(), gfx::Transform(), child_size);

    SolidColorDrawQuad* color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    color_quad->SetNew(pass->shared_quad_state_list.back(),
                       rect,
                       rect,
                       SK_ColorBLUE,
                       force_anti_aliasing_off);

    CompositorFrame child_frame;
    child_frame.render_pass_list.push_back(std::move(pass));

    child_factory.SubmitCompositorFrame(child_local_frame_id,
                                        std::move(child_frame),
                                        SurfaceFactory::DrawCallback());
  }

  SurfaceAggregator aggregator(&manager_, resource_provider_.get(), true);
  CompositorFrame aggregated_frame = aggregator.Aggregate(root_surface_id);

  bool discard_alpha = false;
  ExactPixelComparator pixel_comparator(discard_alpha);
  RenderPassList* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(RunPixelTest(pass_list,
                           base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
                           pixel_comparator));

  child_factory.EvictSurface();
}

// Tests a surface quad that has a non-identity transform into its pass.
TEST_F(SurfacesPixelTest, DrawAggregatedFrameWithSurfaceTransforms) {
  gfx::Size child_size(100, 200);
  gfx::Size quad_size(100, 100);
  // Structure:
  // root (200x200) -> left_child (100x200 @ 0x0,
  //                   right_child (100x200 @ 0x100)
  //   left_child -> top_green_quad (100x100 @ 0x0),
  //                 bottom_blue_quad (100x100 @ 0x100)
  //   right_child -> top_blue_quad (100x100 @ 0x0),
  //                  bottom_green_quad (100x100 @ 0x100)
  SurfaceFactory left_factory(kArbitraryLeftFrameSinkId, &manager_, &client_);
  SurfaceFactory right_factory(kArbitraryRightFrameSinkId, &manager_, &client_);
  LocalFrameId left_child_local_id = allocator_.GenerateId();
  SurfaceId left_child_id(left_factory.frame_sink_id(), left_child_local_id);
  LocalFrameId right_child_local_id = allocator_.GenerateId();
  SurfaceId right_child_id(right_factory.frame_sink_id(), right_child_local_id);
  LocalFrameId root_local_frame_id = allocator_.GenerateId();
  SurfaceId root_surface_id(factory_.frame_sink_id(), root_local_frame_id);

  {
    gfx::Rect rect(device_viewport_size_);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    gfx::Transform surface_transform;
    CreateAndAppendTestSharedQuadState(
        pass.get(), surface_transform, device_viewport_size_);

    SurfaceDrawQuad* left_surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    left_surface_quad->SetNew(pass->shared_quad_state_list.back(),
                              gfx::Rect(child_size),
                              gfx::Rect(child_size),
                              left_child_id);

    surface_transform.Translate(100, 0);
    CreateAndAppendTestSharedQuadState(
        pass.get(), surface_transform, device_viewport_size_);

    SurfaceDrawQuad* right_surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    right_surface_quad->SetNew(pass->shared_quad_state_list.back(),
                               gfx::Rect(child_size),
                               gfx::Rect(child_size),
                               right_child_id);

    CompositorFrame root_frame;
    root_frame.render_pass_list.push_back(std::move(pass));

    factory_.SubmitCompositorFrame(root_local_frame_id, std::move(root_frame),
                                   SurfaceFactory::DrawCallback());
  }

  {
    gfx::Rect rect(child_size);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(
        pass.get(), gfx::Transform(), child_size);

    SolidColorDrawQuad* top_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    top_color_quad->SetNew(pass->shared_quad_state_list.back(),
                           gfx::Rect(quad_size),
                           gfx::Rect(quad_size),
                           SK_ColorGREEN,
                           force_anti_aliasing_off);

    SolidColorDrawQuad* bottom_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_color_quad->SetNew(pass->shared_quad_state_list.back(),
                              gfx::Rect(0, 100, 100, 100),
                              gfx::Rect(0, 100, 100, 100),
                              SK_ColorBLUE,
                              force_anti_aliasing_off);

    CompositorFrame child_frame;
    child_frame.render_pass_list.push_back(std::move(pass));

    left_factory.SubmitCompositorFrame(left_child_local_id,
                                       std::move(child_frame),
                                       SurfaceFactory::DrawCallback());
  }

  {
    gfx::Rect rect(child_size);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(
        pass.get(), gfx::Transform(), child_size);

    SolidColorDrawQuad* top_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    top_color_quad->SetNew(pass->shared_quad_state_list.back(),
                           gfx::Rect(quad_size),
                           gfx::Rect(quad_size),
                           SK_ColorBLUE,
                           force_anti_aliasing_off);

    SolidColorDrawQuad* bottom_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_color_quad->SetNew(pass->shared_quad_state_list.back(),
                              gfx::Rect(0, 100, 100, 100),
                              gfx::Rect(0, 100, 100, 100),
                              SK_ColorGREEN,
                              force_anti_aliasing_off);

    CompositorFrame child_frame;
    child_frame.render_pass_list.push_back(std::move(pass));

    right_factory.SubmitCompositorFrame(right_child_local_id,
                                        std::move(child_frame),
                                        SurfaceFactory::DrawCallback());
  }

  SurfaceAggregator aggregator(&manager_, resource_provider_.get(), true);
  CompositorFrame aggregated_frame = aggregator.Aggregate(root_surface_id);

  bool discard_alpha = false;
  ExactPixelComparator pixel_comparator(discard_alpha);
  RenderPassList* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(RunPixelTest(
      pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      pixel_comparator));

  left_factory.EvictSurface();
  right_factory.EvictSurface();
}

}  // namespace
}  // namespace cc

#endif  // !defined(OS_ANDROID)
