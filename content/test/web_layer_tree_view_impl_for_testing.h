// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
#define CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_

#include "base/memory/scoped_ptr.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"

namespace cc {
class LayerTreeHost;
}

namespace blink { class WebLayer; }

namespace content {

class WebLayerTreeViewImplForTesting
    : public blink::WebLayerTreeView,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient {
 public:
  WebLayerTreeViewImplForTesting();
  virtual ~WebLayerTreeViewImplForTesting();

  void Initialize();

  // blink::WebLayerTreeView implementation.
  virtual void setRootLayer(const blink::WebLayer& layer);
  virtual void clearRootLayer();
  virtual void setViewportSize(const blink::WebSize& unused_deprecated,
                               const blink::WebSize& device_viewport_size);
  virtual void setViewportSize(const blink::WebSize& device_viewport_size);
  virtual blink::WebSize layoutViewportSize() const;
  virtual blink::WebSize deviceViewportSize() const;
  virtual void setDeviceScaleFactor(float scale_factor);
  virtual float deviceScaleFactor() const;
  virtual void setBackgroundColor(blink::WebColor);
  virtual void setHasTransparentBackground(bool transparent);
  virtual void setVisible(bool visible);
  virtual void setPageScaleFactorAndLimits(float page_scale_factor,
                                           float minimum,
                                           float maximum);
  virtual void startPageScaleAnimation(const blink::WebPoint& destination,
                                       bool use_anchor,
                                       float new_page_scale,
                                       double duration_sec);
  virtual void setNeedsAnimate();
  virtual void didStopFlinging();
  virtual void finishAllRendering();
  virtual void setDeferCommits(bool defer_commits);
  virtual void registerForAnimations(blink::WebLayer* layer);
  virtual void registerViewportLayers(
      const blink::WebLayer* overscrollElasticityLayer,
      const blink::WebLayer* pageScaleLayerLayer,
      const blink::WebLayer* innerViewportScrollLayer,
      const blink::WebLayer* outerViewportScrollLayer) override;
  virtual void clearViewportLayers() override;
  virtual void registerSelection(const blink::WebSelection& selection) override;
  virtual void clearSelection() override;

  // cc::LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void Layout() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override {}
  void DidFailToInitializeOutputSurface() override;
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {}
  void DidCompleteSwapBuffers() override {}
  void DidCompletePageScaleAnimation() override {}
  void RecordFrameTimingEvents(
      scoped_ptr<cc::FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<cc::FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override {}

  // cc::LayerTreeHostSingleThreadClient implementation.
  void DidPostSwapBuffers() override {}
  void DidAbortSwapBuffers() override {}

 private:
  cc::TestTaskGraphRunner task_graph_runner_;
  scoped_ptr<cc::LayerTreeHost> layer_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerTreeViewImplForTesting);
};

}  // namespace content

#endif  // CONTENT_TEST_WEB_LAYER_TREE_VIEW_IMPL_FOR_TESTING_H_
