// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/fake_compositor_dependencies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace {
const int32 kSubframeRouteId = 20;
const int32 kSubframeWidgetRouteId = 21;
const int32 kFrameProxyRouteId = 22;
const int32 kSubframeSurfaceId = 43;
}  // namespace

namespace content {

// RenderFrameImplTest creates a RenderFrameImpl that is a child of the
// main frame, and has its own RenderWidget. This behaves like an out
// of process frame even though it is in the same process as its parent.
class RenderFrameImplTest : public RenderViewTest {
 public:
  ~RenderFrameImplTest() override {}

  void SetUp() override {
    RenderViewTest::SetUp();
    EXPECT_FALSE(static_cast<RenderFrameImpl*>(view_->GetMainRenderFrame())
                     ->is_subframe_);

    FrameMsg_NewFrame_WidgetParams widget_params;
    widget_params.routing_id = kSubframeWidgetRouteId;
    widget_params.surface_id = kSubframeSurfaceId;
    widget_params.hidden = false;

    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

    LoadHTML("Parent frame <iframe name='frame'></iframe>");

    RenderFrameImpl::FromWebFrame(
        view_->GetMainRenderFrame()->GetWebFrame()->firstChild())
        ->OnSwapOut(kFrameProxyRouteId, false, FrameReplicationState());

    RenderFrameImpl::CreateFrame(kSubframeRouteId, kFrameProxyRouteId,
                                 MSG_ROUTING_NONE, MSG_ROUTING_NONE,
                                 FrameReplicationState(), &compositor_deps_,
                                 widget_params);

    frame_ = RenderFrameImpl::FromRoutingID(kSubframeRouteId);
    EXPECT_TRUE(frame_->is_subframe_);
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
     // Do this before shutting down V8 in RenderViewTest::TearDown().
     // http://crbug.com/328552
     __lsan_do_leak_check();
#endif
     RenderViewTest::TearDown();
  }

  RenderFrameImpl* frame() { return frame_; }

  content::RenderWidget* frame_widget() const {
    return frame_->render_widget_.get();
  }

 private:
  RenderFrameImpl* frame_;
  FakeCompositorDependencies compositor_deps_;
};

class RenderFrameTestObserver : public RenderFrameObserver {
 public:
  explicit RenderFrameTestObserver(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame), visible_(false) {}

  ~RenderFrameTestObserver() override {}

  void WasShown() override { visible_ = true; }
  void WasHidden() override { visible_ = false; }

  bool visible() { return visible_; }

 private:
  bool visible_;
};

#if defined(OS_ANDROID)
// See https://crbug.com/472717
#define MAYBE_SubframeWidget DISABLED_SubframeWidget
#define MAYBE_FrameResize DISABLED_FrameResize
#define MAYBE_FrameWasShown DISABLED_FrameWasShown
#else
#define MAYBE_SubframeWidget SubframeWidget
#define MAYBE_FrameResize FrameResize
#define MAYBE_FrameWasShown FrameWasShown
#endif

// Verify that a frame with a RenderFrameProxy as a parent has its own
// RenderWidget.
TEST_F(RenderFrameImplTest, MAYBE_SubframeWidget) {
  EXPECT_TRUE(frame_widget());
  // We can't convert to RenderWidget* directly, because
  // it and RenderView are two unrelated base classes
  // of RenderViewImpl. If a class has multiple base classes,
  // each base class pointer will be distinct, and direct casts
  // between unrelated base classes are undefined, even if they share
  // a common derived class. The compiler has no way in general of
  // determining the displacement between the two classes, so these
  // types of casts cannot be implemented in a type safe way.
  // To overcome this, we make two legal static casts:
  // first, downcast from RenderView* to RenderViewImpl*,
  // then upcast from RenderViewImpl* to RenderWidget*.
  EXPECT_NE(frame_widget(),
            static_cast<content::RenderWidget*>(
                static_cast<content::RenderViewImpl*>((view_))));
}

// Verify a subframe RenderWidget properly processes its viewport being
// resized.
TEST_F(RenderFrameImplTest, MAYBE_FrameResize) {
  ViewMsg_Resize_Params resize_params;
  gfx::Size size(200, 200);
  resize_params.screen_info = blink::WebScreenInfo();
  resize_params.new_size = size;
  resize_params.physical_backing_size = size;
  resize_params.top_controls_height = 0.f;
  resize_params.top_controls_shrink_blink_size = false;
  resize_params.resizer_rect = gfx::Rect();
  resize_params.is_fullscreen_granted = false;

  ViewMsg_Resize resize_message(0, resize_params);
  frame_widget()->OnMessageReceived(resize_message);

  EXPECT_EQ(frame_widget()->webwidget()->size(), blink::WebSize(size));
}

// Verify a subframe RenderWidget properly processes a WasShown message.
TEST_F(RenderFrameImplTest, MAYBE_FrameWasShown) {
  RenderFrameTestObserver observer(frame());

  ViewMsg_WasShown was_shown_message(0, true, ui::LatencyInfo());
  frame_widget()->OnMessageReceived(was_shown_message);

  EXPECT_FALSE(frame_widget()->is_hidden());
  EXPECT_TRUE(observer.visible());
}

}  // namespace
