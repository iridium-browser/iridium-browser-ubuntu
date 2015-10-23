// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/test/rendering_test.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"

namespace android_webview {

class SmokeTest : public RenderingTest {
  void StartTest() override { browser_view_renderer_->PostInvalidate(); }

  void DidDrawOnRT(SharedRendererState* functor) override {
    EndTest();
  }
};

RENDERING_TEST_F(SmokeTest);

class ClearViewTest : public RenderingTest {
 public:
  ClearViewTest() : on_draw_count_(0) {}

  void StartTest() override {
    browser_view_renderer_->PostInvalidate();
    browser_view_renderer_->ClearView();
  }

  void DidOnDraw(bool success) override {
    on_draw_count_++;
    if (on_draw_count_ == 1) {
      // First OnDraw should be skipped due to ClearView.
      EXPECT_FALSE(success);
      browser_view_renderer_->DidUpdateContent();  // Unset ClearView.
      browser_view_renderer_->PostInvalidate();
    } else {
      // Following OnDraws should succeed.
      EXPECT_TRUE(success);
    }
  }

  void DidDrawOnRT(SharedRendererState* functor) override {
    EndTest();
  }
 private:
  int on_draw_count_;
};

RENDERING_TEST_F(ClearViewTest);

class TestAnimateInAndOutOfScreen : public RenderingTest {
 public:
  TestAnimateInAndOutOfScreen() : on_draw_count_(0), draw_gl_count_on_rt_(0) {}

  void StartTest() override {
    new_constraints_ = ParentCompositorDrawConstraints(
        false, gfx::Transform(), window_->surface_size().IsEmpty());
    new_constraints_.transform.Scale(2.0, 2.0);
    browser_view_renderer_->PostInvalidate();
  }

  void WillOnDraw() override {
    // Step 0: A single onDraw on screen. The parent draw constraints
    // of the BVR will updated to be the initial constraints.
    // Step 1: A single onDrraw off screen. The parent draw constraints of the
    // BVR will be updated to the new constraints.
    // Step 2: This onDraw is to introduce the DrawGL that animates the
    // webview onto the screen on render thread. End the test when the parent
    // draw constraints of BVR is updated to initial constraints.
    if (on_draw_count_ == 1 || on_draw_count_ == 2)
      browser_view_renderer_->PrepareToDraw(gfx::Vector2d(), gfx::Rect());
  }

  void DidOnDraw(bool success) override {
    EXPECT_TRUE(success);
    on_draw_count_++;
  }

  bool WillDrawOnRT(SharedRendererState* functor,
                    AwDrawGLInfo* draw_info) override {
    if (draw_gl_count_on_rt_ == 1) {
      draw_gl_count_on_rt_++;
      ui_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&RenderingTest::PostInvalidate, base::Unretained(this)));
      return false;
    }

    draw_info->width = window_->surface_size().width();
    draw_info->height = window_->surface_size().height();
    draw_info->is_layer = false;

    gfx::Transform transform;
    if (draw_gl_count_on_rt_ == 0)
      transform = new_constraints_.transform;

    transform.matrix().asColMajorf(draw_info->transform);
    return true;
  }

  void DidDrawOnRT(SharedRendererState* functor) override {
    draw_gl_count_on_rt_++;
  }

  bool DrawConstraintsEquals(
      const ParentCompositorDrawConstraints& constraints1,
      const ParentCompositorDrawConstraints& constraints2) {
    if (constraints1.is_layer != constraints2.is_layer ||
        constraints1.transform != constraints2.transform)
      return false;

    return !constraints1.is_layer ||
           constraints1.surface_rect_empty == constraints2.surface_rect_empty;
  }

  void ParentDrawConstraintsUpdated(
      const ParentCompositorDrawConstraints& constraints) override {
    switch (on_draw_count_) {
      case 1u:
        EXPECT_TRUE(DrawConstraintsEquals(constraints, new_constraints_));
        break;
      case 3u:
        EXPECT_TRUE(DrawConstraintsEquals(constraints, initial_constraints_));
        EndTest();
        break;
      // There will be a following 4th onDraw. But the hardware renderer won't
      // post back the draw constraints in DrawGL because the constraints
      // don't change.
      default:
        FAIL();
    }
  }

 private:
  int on_draw_count_;
  int draw_gl_count_on_rt_;
  ParentCompositorDrawConstraints initial_constraints_;
  ParentCompositorDrawConstraints new_constraints_;
};

RENDERING_TEST_F(TestAnimateInAndOutOfScreen);

}  // namespace android_webview
