// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerPainter.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

struct PaintLayerPainterTestParam {
  PaintLayerPainterTestParam(bool rootLayerScrolling, bool slimmingPaintV2)
      : rootLayerScrolling(rootLayerScrolling),
        slimmingPaintV2(slimmingPaintV2) {}

  bool rootLayerScrolling;
  bool slimmingPaintV2;
};

class PaintLayerPainterTest
    : public testing::WithParamInterface<PaintLayerPainterTestParam>,
      private ScopedRootLayerScrollingForTest,
      public PaintControllerPaintTestBase {
  USING_FAST_MALLOC(PaintLayerPainterTest);

 public:
  PaintLayerPainterTest()
      : ScopedRootLayerScrollingForTest(GetParam().rootLayerScrolling),
        PaintControllerPaintTestBase(GetParam().slimmingPaintV2) {}

 private:
  void SetUp() override {
    PaintControllerPaintTestBase::SetUp();
    enableCompositing();
  }
};

INSTANTIATE_TEST_CASE_P(
    All,
    PaintLayerPainterTest,
    ::testing::Values(PaintLayerPainterTestParam(
                          false,
                          false),  // non-root-layer-scrolls, slimming-paint-v1
                      PaintLayerPainterTestParam(
                          false,
                          true),  // non-root-layer-scrolls, slimming-paint-v2
                      PaintLayerPainterTestParam(
                          true,
                          false),  // root-layer-scrolls, slimming-paint-v1
                      PaintLayerPainterTestParam(
                          true,
                          true)));  // root-layer-scrolls, slimming-paint-v2

TEST_P(PaintLayerPainterTest, CachedSubsequence) {
  setBodyInnerHTML(
      "<div id='container1' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content1' style='position: absolute; width: 100px; height: "
      "100px; background-color: red'></div>"
      "</div>"
      "<div id='container2' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content2' style='position: absolute; width: 100px; height: "
      "100px; background-color: green'></div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  LayoutObject& container1 =
      *document().getElementById("container1")->layoutObject();
  PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
  LayoutObject& content1 =
      *document().getElementById("content1")->layoutObject();
  LayoutObject& container2 =
      *document().getElementById("container2")->layoutObject();
  PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
  LayoutObject& content2 =
      *document().getElementById("content2")->layoutObject();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 13,
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container1, backgroundType),
          TestDisplayItem(content1, backgroundType),
          TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container2, backgroundType),
          TestDisplayItem(content2, backgroundType),
          TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence));
    } else {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 15,
          TestDisplayItem(layoutView(),
                          DisplayItem::kClipFrameToVisibleContentRect),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container1, backgroundType),
          TestDisplayItem(content1, backgroundType),
          TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container2, backgroundType),
          TestDisplayItem(content2, backgroundType),
          TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
          TestDisplayItem(layoutView(),
                          DisplayItem::clipTypeToEndClipType(
                              DisplayItem::kClipFrameToVisibleContentRect)));
    }
  } else {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));
  }

  toHTMLElement(content1.node())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(paintWithoutCommit());

  EXPECT_EQ(6, numCachedNewItems());

  commit();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 13,
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container1, backgroundType),
          TestDisplayItem(content1, backgroundType),
          TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container2, backgroundType),
          TestDisplayItem(content2, backgroundType),
          TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence));
    } else {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 15,
          TestDisplayItem(layoutView(),
                          DisplayItem::kClipFrameToVisibleContentRect),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container1, backgroundType),
          TestDisplayItem(content1, backgroundType),
          TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container2, backgroundType),
          TestDisplayItem(content2, backgroundType),
          TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
          TestDisplayItem(layoutView(),
                          DisplayItem::clipTypeToEndClipType(
                              DisplayItem::kClipFrameToVisibleContentRect)));
    }
  } else {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));
  }
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceForSVGRoot) {
  setBodyInnerHTML(
      "<svg id='svg' style='position: relative'>"
      "  <rect id='rect' x='10' y='10' width='100' height='100' rx='15' "
      "ry='15'/>"
      "</svg>"
      "<div id='div' style='position: relative; width: 50x; height: "
      "50px'></div>");
  document().view()->updateAllLifecyclePhases();

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  LayoutObject& svg = *document().getElementById("svg")->layoutObject();
  PaintLayer& svgLayer = *toLayoutBoxModelObject(svg).layer();
  LayoutObject& rect = *document().getElementById("rect")->layoutObject();
  LayoutObject& div = *document().getElementById("div")->layoutObject();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      // SPv2 slips the clip box (see BoxClipper).
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 10,
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(svgLayer, DisplayItem::kSubsequence),
          TestDisplayItem(svg, DisplayItem::kBeginTransform),
          TestDisplayItem(rect, foregroundType),
          TestDisplayItem(svg, DisplayItem::kEndTransform),
          TestDisplayItem(svgLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence));
    } else {
      // SPv2 slips the clip box (see BoxClipper).
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 12,
          TestDisplayItem(layoutView(),
                          DisplayItem::kClipFrameToVisibleContentRect),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(svgLayer, DisplayItem::kSubsequence),
          TestDisplayItem(svg, DisplayItem::kBeginTransform),
          TestDisplayItem(rect, foregroundType),
          TestDisplayItem(svg, DisplayItem::kEndTransform),
          TestDisplayItem(svgLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
          TestDisplayItem(layoutView(),
                          DisplayItem::clipTypeToEndClipType(
                              DisplayItem::kClipFrameToVisibleContentRect)));
    }
  } else {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 10,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
        TestDisplayItem(svgLayer, DisplayItem::kSubsequence),
        TestDisplayItem(svg, DisplayItem::kClipLayerForeground),
        TestDisplayItem(svg, DisplayItem::kBeginTransform),
        TestDisplayItem(rect, foregroundType),
        TestDisplayItem(svg, DisplayItem::kEndTransform),
        TestDisplayItem(svg, DisplayItem::clipTypeToEndClipType(
                                 DisplayItem::kClipLayerForeground)),
        TestDisplayItem(svgLayer, DisplayItem::kEndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));
  }

  // Change the color of the div. This should not invalidate the subsequence
  // for the SVG root.
  toHTMLElement(div.node())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: relative; width: 50x; height: 50px; "
                     "background-color: green");
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(paintWithoutCommit());

  // Reuse of SVG and document background. 2 fewer with SPv2 enabled because
  // clip display items don't appear in SPv2 display lists.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    EXPECT_EQ(6, numCachedNewItems());
  else
    EXPECT_EQ(8, numCachedNewItems());

  commit();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 11,
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(svgLayer, DisplayItem::kSubsequence),
          TestDisplayItem(svg, DisplayItem::kBeginTransform),
          TestDisplayItem(rect, foregroundType),
          TestDisplayItem(svg, DisplayItem::kEndTransform),
          TestDisplayItem(svgLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(div, backgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence));
    } else {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 13,
          TestDisplayItem(layoutView(),
                          DisplayItem::kClipFrameToVisibleContentRect),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(svgLayer, DisplayItem::kSubsequence),
          TestDisplayItem(svg, DisplayItem::kBeginTransform),
          TestDisplayItem(rect, foregroundType),
          TestDisplayItem(svg, DisplayItem::kEndTransform),
          TestDisplayItem(svgLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(div, backgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
          TestDisplayItem(layoutView(),
                          DisplayItem::clipTypeToEndClipType(
                              DisplayItem::kClipFrameToVisibleContentRect)));
    }
  } else {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
        TestDisplayItem(svgLayer, DisplayItem::kSubsequence),
        TestDisplayItem(svg, DisplayItem::kClipLayerForeground),
        TestDisplayItem(svg, DisplayItem::kBeginTransform),
        TestDisplayItem(rect, foregroundType),
        TestDisplayItem(svg, DisplayItem::kEndTransform),
        TestDisplayItem(svg, DisplayItem::clipTypeToEndClipType(
                                 DisplayItem::kClipLayerForeground)),
        TestDisplayItem(svgLayer, DisplayItem::kEndSubsequence),
        TestDisplayItem(div, backgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
        TestDisplayItem(layoutView(),
                        DisplayItem::clipTypeToEndClipType(
                            DisplayItem::kClipFrameToVisibleContentRect)));
  }
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnInterestRectChange) {
  // TODO(wangxianzhu): SPv2 deals with interest rect differently, so disable
  // this test for SPv2 temporarily.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return;

  setBodyInnerHTML(
      "<div id='container1' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content1' style='position: absolute; width: 100px; height: "
      "100px; background-color: green'></div>"
      "</div>"
      "<div id='container2' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content2a' style='position: absolute; width: 100px; height: "
      "100px; background-color: green'></div>"
      "  <div id='content2b' style='position: absolute; top: 200px; width: "
      "100px; height: 100px; background-color: green'></div>"
      "</div>"
      "<div id='container3' style='position: absolute; z-index: 2; left: "
      "300px; top: 0; width: 200px; height: 200px; background-color: blue'>"
      "  <div id='content3' style='position: absolute; width: 200px; height: "
      "200px; background-color: green'></div>"
      "</div>");
  rootPaintController().invalidateAll();

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  LayoutObject& container1 =
      *document().getElementById("container1")->layoutObject();
  PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
  LayoutObject& content1 =
      *document().getElementById("content1")->layoutObject();
  LayoutObject& container2 =
      *document().getElementById("container2")->layoutObject();
  PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
  LayoutObject& content2a =
      *document().getElementById("content2a")->layoutObject();
  LayoutObject& content2b =
      *document().getElementById("content2b")->layoutObject();
  LayoutObject& container3 =
      *document().getElementById("container3")->layoutObject();
  PaintLayer& container3Layer = *toLayoutBoxModelObject(container3).layer();
  LayoutObject& content3 =
      *document().getElementById("content3")->layoutObject();

  document().view()->updateAllLifecyclePhasesExceptPaint();
  IntRect interestRect(0, 0, 400, 300);
  paint(&interestRect);

  // Container1 is fully in the interest rect;
  // Container2 is partly (including its stacking chidren) in the interest rect;
  // Content2b is out of the interest rect and output nothing;
  // Container3 is partly in the interest rect.
  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 15,
      TestDisplayItem(layoutView(), documentBackgroundType),
      TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
      TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
      TestDisplayItem(container1, backgroundType),
      TestDisplayItem(content1, backgroundType),
      TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
      TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
      TestDisplayItem(container2, backgroundType),
      TestDisplayItem(content2a, backgroundType),
      TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
      TestDisplayItem(container3Layer, DisplayItem::kSubsequence),
      TestDisplayItem(container3, backgroundType),
      TestDisplayItem(content3, backgroundType),
      TestDisplayItem(container3Layer, DisplayItem::kEndSubsequence),
      TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));

  document().view()->updateAllLifecyclePhasesExceptPaint();
  IntRect newInterestRect(0, 100, 300, 1000);
  EXPECT_TRUE(paintWithoutCommit(&newInterestRect));

  // Container1 becomes partly in the interest rect, but uses cached subsequence
  // because it was fully painted before;
  // Container2's intersection with the interest rect changes;
  // Content2b is out of the interest rect and outputs nothing;
  // Container3 becomes out of the interest rect and outputs empty subsequence
  // pair.
  EXPECT_EQ(7, numCachedNewItems());

  commit();

  EXPECT_DISPLAY_LIST(
      rootPaintController().getDisplayItemList(), 14,
      TestDisplayItem(layoutView(), documentBackgroundType),
      TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
      TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
      TestDisplayItem(container1, backgroundType),
      TestDisplayItem(content1, backgroundType),
      TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
      TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
      TestDisplayItem(container2, backgroundType),
      TestDisplayItem(content2a, backgroundType),
      TestDisplayItem(content2b, backgroundType),
      TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
      TestDisplayItem(container3Layer, DisplayItem::kSubsequence),
      TestDisplayItem(container3Layer, DisplayItem::kEndSubsequence),
      TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));
}

TEST_P(PaintLayerPainterTest,
       CachedSubsequenceOnStyleChangeWithInterestRectClipping) {
  setBodyInnerHTML(
      "<div id='container1' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content1' style='position: absolute; width: 100px; height: "
      "100px; background-color: red'></div>"
      "</div>"
      "<div id='container2' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content2' style='position: absolute; width: 100px; height: "
      "100px; background-color: green'></div>"
      "</div>");
  document().view()->updateAllLifecyclePhasesExceptPaint();
  // PaintResult of all subsequences will be MayBeClippedByPaintDirtyRect.
  IntRect interestRect(0, 0, 50, 300);
  paint(&interestRect);

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  LayoutObject& container1 =
      *document().getElementById("container1")->layoutObject();
  PaintLayer& container1Layer = *toLayoutBoxModelObject(container1).layer();
  LayoutObject& content1 =
      *document().getElementById("content1")->layoutObject();
  LayoutObject& container2 =
      *document().getElementById("container2")->layoutObject();
  PaintLayer& container2Layer = *toLayoutBoxModelObject(container2).layer();
  LayoutObject& content2 =
      *document().getElementById("content2")->layoutObject();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 13,
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container1, backgroundType),
          TestDisplayItem(content1, backgroundType),
          TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container2, backgroundType),
          TestDisplayItem(content2, backgroundType),
          TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence));
    } else {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 15,
          TestDisplayItem(layoutView(),
                          DisplayItem::kClipFrameToVisibleContentRect),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container1, backgroundType),
          TestDisplayItem(content1, backgroundType),
          TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container2, backgroundType),
          TestDisplayItem(content2, backgroundType),
          TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
          TestDisplayItem(layoutView(),
                          DisplayItem::clipTypeToEndClipType(
                              DisplayItem::kClipFrameToVisibleContentRect)));
    }
  } else {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));
  }

  toHTMLElement(content1.node())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(paintWithoutCommit(&interestRect));

  EXPECT_EQ(6, numCachedNewItems());

  commit();

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 13,
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container1, backgroundType),
          TestDisplayItem(content1, backgroundType),
          TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container2, backgroundType),
          TestDisplayItem(content2, backgroundType),
          TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence));
    } else {
      EXPECT_DISPLAY_LIST(
          rootPaintController().getDisplayItemList(), 15,
          TestDisplayItem(layoutView(),
                          DisplayItem::kClipFrameToVisibleContentRect),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kSubsequence),
          TestDisplayItem(layoutView(), documentBackgroundType),
          TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
          TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container1, backgroundType),
          TestDisplayItem(content1, backgroundType),
          TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
          TestDisplayItem(container2, backgroundType),
          TestDisplayItem(content2, backgroundType),
          TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
          TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence),
          TestDisplayItem(*layoutView().layer(), DisplayItem::kEndSubsequence),
          TestDisplayItem(layoutView(),
                          DisplayItem::clipTypeToEndClipType(
                              DisplayItem::kClipFrameToVisibleContentRect)));
    }
  } else {
    EXPECT_DISPLAY_LIST(
        rootPaintController().getDisplayItemList(), 11,
        TestDisplayItem(layoutView(), documentBackgroundType),
        TestDisplayItem(htmlLayer, DisplayItem::kSubsequence),
        TestDisplayItem(container1Layer, DisplayItem::kSubsequence),
        TestDisplayItem(container1, backgroundType),
        TestDisplayItem(content1, backgroundType),
        TestDisplayItem(container1Layer, DisplayItem::kEndSubsequence),
        TestDisplayItem(container2Layer, DisplayItem::kSubsequence),
        TestDisplayItem(container2, backgroundType),
        TestDisplayItem(content2, backgroundType),
        TestDisplayItem(container2Layer, DisplayItem::kEndSubsequence),
        TestDisplayItem(htmlLayer, DisplayItem::kEndSubsequence));
  }
}

TEST_P(PaintLayerPainterTest, PaintPhaseOutline) {
  AtomicString styleWithoutOutline =
      "width: 50px; height: 50px; background-color: green";
  AtomicString styleWithOutline =
      "outline: 1px solid blue; " + styleWithoutOutline;
  setBodyInnerHTML(
      "<div id='self-painting-layer' style='position: absolute'>"
      "  <div id='non-self-painting-layer' style='overflow: hidden'>"
      "    <div>"
      "      <div id='outline'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  LayoutObject& outlineDiv =
      *document().getElementById("outline")->layoutObject();
  toHTMLElement(outlineDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithoutOutline);
  document().view()->updateAllLifecyclePhases();

  LayoutBoxModelObject& selfPaintingLayerObject = *toLayoutBoxModelObject(
      document().getElementById("self-painting-layer")->layoutObject());
  PaintLayer& selfPaintingLayer = *selfPaintingLayerObject.layer();
  ASSERT_TRUE(selfPaintingLayer.isSelfPaintingLayer());
  PaintLayer& nonSelfPaintingLayer =
      *toLayoutBoxModelObject(
           document().getElementById("non-self-painting-layer")->layoutObject())
           ->layer();
  ASSERT_FALSE(nonSelfPaintingLayer.isSelfPaintingLayer());
  ASSERT_TRUE(&nonSelfPaintingLayer == outlineDiv.enclosingLayer());

  EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantOutlines());

  // Outline on the self-painting-layer node itself doesn't affect
  // PaintPhaseDescendantOutlines.
  toHTMLElement(selfPaintingLayerObject.node())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: absolute; outline: 1px solid green");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(displayItemListContains(
      rootPaintController().getDisplayItemList(), selfPaintingLayerObject,
      DisplayItem::paintPhaseToDrawingType(PaintPhaseSelfOutlineOnly)));

  // needsPaintPhaseDescendantOutlines should be set when any descendant on the
  // same layer has outline.
  toHTMLElement(outlineDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithOutline);
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(selfPaintingLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseDescendantOutlines());
  paint();
  EXPECT_TRUE(displayItemListContains(
      rootPaintController().getDisplayItemList(), outlineDiv,
      DisplayItem::paintPhaseToDrawingType(PaintPhaseSelfOutlineOnly)));

  // needsPaintPhaseDescendantOutlines should be reset when no outline is
  // actually painted.
  toHTMLElement(outlineDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithoutOutline);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloat) {
  AtomicString styleWithoutFloat =
      "width: 50px; height: 50px; background-color: green";
  AtomicString styleWithFloat = "float: left; " + styleWithoutFloat;
  setBodyInnerHTML(
      "<div id='self-painting-layer' style='position: absolute'>"
      "  <div id='non-self-painting-layer' style='overflow: hidden'>"
      "    <div>"
      "      <div id='float' style='width: 10px; height: 10px; "
      "background-color: blue'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  LayoutObject& floatDiv = *document().getElementById("float")->layoutObject();
  toHTMLElement(floatDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithoutFloat);
  document().view()->updateAllLifecyclePhases();

  LayoutBoxModelObject& selfPaintingLayerObject = *toLayoutBoxModelObject(
      document().getElementById("self-painting-layer")->layoutObject());
  PaintLayer& selfPaintingLayer = *selfPaintingLayerObject.layer();
  ASSERT_TRUE(selfPaintingLayer.isSelfPaintingLayer());
  PaintLayer& nonSelfPaintingLayer =
      *toLayoutBoxModelObject(
           document().getElementById("non-self-painting-layer")->layoutObject())
           ->layer();
  ASSERT_FALSE(nonSelfPaintingLayer.isSelfPaintingLayer());
  ASSERT_TRUE(&nonSelfPaintingLayer == floatDiv.enclosingLayer());

  EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseFloat());
  EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseFloat());

  // needsPaintPhaseFloat should be set when any descendant on the same layer
  // has float.
  toHTMLElement(floatDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithFloat);
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(selfPaintingLayer.needsPaintPhaseFloat());
  EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseFloat());
  paint();
  EXPECT_TRUE(
      displayItemListContains(rootPaintController().getDisplayItemList(),
                              floatDiv, DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseFloat should be reset when there is no float actually
  // painted.
  toHTMLElement(floatDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithoutFloat);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseFloat());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloatUnderInlineLayer) {
  setBodyInnerHTML(
      "<div id='self-painting-layer' style='position: absolute'>"
      "  <div id='non-self-painting-layer' style='overflow: hidden'>"
      "    <span id='span' style='position: relative'>"
      "      <div id='float' style='width: 10px; height: 10px; "
      "background-color: blue; float: left'></div>"
      "    </span>"
      "  </div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  LayoutObject& floatDiv = *document().getElementById("float")->layoutObject();
  LayoutBoxModelObject& span = *toLayoutBoxModelObject(
      document().getElementById("span")->layoutObject());
  PaintLayer& spanLayer = *span.layer();
  ASSERT_TRUE(&spanLayer == floatDiv.enclosingLayer());
  ASSERT_FALSE(spanLayer.needsPaintPhaseFloat());
  LayoutBoxModelObject& selfPaintingLayerObject = *toLayoutBoxModelObject(
      document().getElementById("self-painting-layer")->layoutObject());
  PaintLayer& selfPaintingLayer = *selfPaintingLayerObject.layer();
  ASSERT_TRUE(selfPaintingLayer.isSelfPaintingLayer());
  PaintLayer& nonSelfPaintingLayer =
      *toLayoutBoxModelObject(
           document().getElementById("non-self-painting-layer")->layoutObject())
           ->layer();
  ASSERT_FALSE(nonSelfPaintingLayer.isSelfPaintingLayer());

  EXPECT_TRUE(selfPaintingLayer.needsPaintPhaseFloat());
  EXPECT_FALSE(nonSelfPaintingLayer.needsPaintPhaseFloat());
  EXPECT_FALSE(spanLayer.needsPaintPhaseFloat());
  EXPECT_TRUE(
      displayItemListContains(rootPaintController().getDisplayItemList(),
                              floatDiv, DisplayItem::kBoxDecorationBackground));
}

TEST_P(PaintLayerPainterTest, PaintPhaseBlockBackground) {
  AtomicString styleWithoutBackground = "width: 50px; height: 50px";
  AtomicString styleWithBackground =
      "background: blue; " + styleWithoutBackground;
  setBodyInnerHTML(
      "<div id='self-painting-layer' style='position: absolute'>"
      "  <div id='non-self-painting-layer' style='overflow: hidden'>"
      "    <div>"
      "      <div id='background'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  LayoutObject& backgroundDiv =
      *document().getElementById("background")->layoutObject();
  toHTMLElement(backgroundDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithoutBackground);
  document().view()->updateAllLifecyclePhases();

  LayoutBoxModelObject& selfPaintingLayerObject = *toLayoutBoxModelObject(
      document().getElementById("self-painting-layer")->layoutObject());
  PaintLayer& selfPaintingLayer = *selfPaintingLayerObject.layer();
  ASSERT_TRUE(selfPaintingLayer.isSelfPaintingLayer());
  PaintLayer& nonSelfPaintingLayer =
      *toLayoutBoxModelObject(
           document().getElementById("non-self-painting-layer")->layoutObject())
           ->layer();
  ASSERT_FALSE(nonSelfPaintingLayer.isSelfPaintingLayer());
  ASSERT_TRUE(&nonSelfPaintingLayer == backgroundDiv.enclosingLayer());

  EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      nonSelfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());

  // Background on the self-painting-layer node itself doesn't affect
  // PaintPhaseDescendantBlockBackgrounds.
  toHTMLElement(selfPaintingLayerObject.node())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: absolute; background: green");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      nonSelfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_TRUE(displayItemListContains(
      rootPaintController().getDisplayItemList(), selfPaintingLayerObject,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseDescendantBlockBackgrounds should be set when any descendant
  // on the same layer has Background.
  toHTMLElement(backgroundDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithBackground);
  document().view()->updateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(selfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      nonSelfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
  paint();
  EXPECT_TRUE(displayItemListContains(
      rootPaintController().getDisplayItemList(), backgroundDiv,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseDescendantBlockBackgrounds should be reset when no outline
  // is actually painted.
  toHTMLElement(backgroundDiv.node())
      ->setAttribute(HTMLNames::styleAttr, styleWithoutBackground);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(selfPaintingLayer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerRemoval) {
  setBodyInnerHTML(
      "<div id='layer' style='position: relative'>"
      "  <div style='height: 100px'>"
      "    <div style='height: 20px; outline: 1px solid red; background-color: "
      "green'>outline and background</div>"
      "    <div style='float: left'>float</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject& layerDiv = *toLayoutBoxModelObject(
      document().getElementById("layer")->layoutObject());
  PaintLayer& layer = *layerDiv.layer();
  ASSERT_TRUE(layer.isSelfPaintingLayer());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.needsPaintPhaseFloat());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  EXPECT_FALSE(htmlLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(htmlLayer.needsPaintPhaseFloat());
  EXPECT_FALSE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());

  toHTMLElement(layerDiv.node())->setAttribute(HTMLNames::styleAttr, "");
  document().view()->updateAllLifecyclePhases();

  EXPECT_FALSE(layerDiv.hasLayer());
  EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(htmlLayer.needsPaintPhaseFloat());
  EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerAddition) {
  setBodyInnerHTML(
      "<div id='will-be-layer'>"
      "  <div style='height: 100px'>"
      "    <div style='height: 20px; outline: 1px solid red; background-color: "
      "green'>outline and background</div>"
      "    <div style='float: left'>float</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject& layerDiv = *toLayoutBoxModelObject(
      document().getElementById("will-be-layer")->layoutObject());
  EXPECT_FALSE(layerDiv.hasLayer());

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(htmlLayer.needsPaintPhaseFloat());
  EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());

  toHTMLElement(layerDiv.node())
      ->setAttribute(HTMLNames::styleAttr, "position: relative");
  document().view()->updateAllLifecyclePhases();
  ASSERT_TRUE(layerDiv.hasLayer());
  PaintLayer& layer = *layerDiv.layer();
  ASSERT_TRUE(layer.isSelfPaintingLayer());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.needsPaintPhaseFloat());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingSelfPainting) {
  setBodyInnerHTML(
      "<div id='will-be-self-painting' style='width: 100px; height: 100px; "
      "overflow: hidden'>"
      "  <div>"
      "    <div style='outline: 1px solid red; background-color: "
      "green'>outline and background</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject& layerDiv = *toLayoutBoxModelObject(
      document().getElementById("will-be-self-painting")->layoutObject());
  ASSERT_TRUE(layerDiv.hasLayer());
  EXPECT_FALSE(layerDiv.layer()->isSelfPaintingLayer());

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());

  toHTMLElement(layerDiv.node())
      ->setAttribute(
          HTMLNames::styleAttr,
          "width: 100px; height: 100px; overflow: hidden; position: relative");
  document().view()->updateAllLifecyclePhases();
  PaintLayer& layer = *layerDiv.layer();
  ASSERT_TRUE(layer.isSelfPaintingLayer());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingNonSelfPainting) {
  setBodyInnerHTML(
      "<div id='will-be-non-self-painting' style='width: 100px; height: 100px; "
      "overflow: hidden; position: relative'>"
      "  <div>"
      "    <div style='outline: 1px solid red; background-color: "
      "green'>outline and background</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject& layerDiv = *toLayoutBoxModelObject(
      document().getElementById("will-be-non-self-painting")->layoutObject());
  ASSERT_TRUE(layerDiv.hasLayer());
  PaintLayer& layer = *layerDiv.layer();
  EXPECT_TRUE(layer.isSelfPaintingLayer());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());

  PaintLayer& htmlLayer =
      *toLayoutBoxModelObject(document().documentElement()->layoutObject())
           ->layer();
  EXPECT_FALSE(htmlLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());

  toHTMLElement(layerDiv.node())
      ->setAttribute(HTMLNames::styleAttr,
                     "width: 100px; height: 100px; overflow: hidden");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(layer.isSelfPaintingLayer());
  EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(htmlLayer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest,
       TableCollapsedBorderNeedsPaintPhaseDescendantBlockBackgrounds) {
  // TODO(wangxianzhu): Enable this test slimmingPaintInvalidation when its
  // fully functional.
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    return;

  // "position: relative" makes the table and td self-painting layers.
  // The table's layer should be marked needsPaintPhaseDescendantBlockBackground
  // because it will paint collapsed borders in the phase.
  setBodyInnerHTML(
      "<table id='table' style='position: relative; border-collapse: collapse'>"
      "  <tr><td style='position: relative; border: 1px solid "
      "green'>Cell</td></tr>"
      "</table>");

  LayoutBoxModelObject& table =
      *toLayoutBoxModelObject(getLayoutObjectByElementId("table"));
  ASSERT_TRUE(table.hasLayer());
  PaintLayer& layer = *table.layer();
  EXPECT_TRUE(layer.isSelfPaintingLayer());
  EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest,
       TableCollapsedBorderNeedsPaintPhaseDescendantBlockBackgroundsDynamic) {
  // TODO(wangxianzhu): Enable this test slimmingPaintInvalidation when its
  // fully functional.
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    return;

  setBodyInnerHTML(
      "<table id='table' style='position: relative'>"
      "  <tr><td style='position: relative; border: 1px solid "
      "green'>Cell</td></tr>"
      "</table>");

  LayoutBoxModelObject& table =
      *toLayoutBoxModelObject(getLayoutObjectByElementId("table"));
  ASSERT_TRUE(table.hasLayer());
  PaintLayer& layer = *table.layer();
  EXPECT_TRUE(layer.isSelfPaintingLayer());
  EXPECT_FALSE(layer.needsPaintPhaseDescendantBlockBackgrounds());

  toHTMLElement(table.node())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: relative; border-collapse: collapse");
  document().view()->updateAllLifecyclePhases();
  EXPECT_TRUE(layer.needsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, DontPaintWithTinyOpacity) {
  setBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001'></div>");
  PaintLayer* targetLayer =
      toLayoutBox(getLayoutObjectByElementId("target"))->layer();
  PaintLayerPaintingInfo paintingInfo(nullptr, LayoutRect(),
                                      GlobalPaintNormalPhase, LayoutSize());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    EXPECT_FALSE(
        PaintLayerPainter(*targetLayer).paintedOutputInvisible(paintingInfo));
  } else {
    EXPECT_TRUE(
        PaintLayerPainter(*targetLayer).paintedOutputInvisible(paintingInfo));
  }
}

TEST_P(PaintLayerPainterTest, DontPaintWithTinyOpacityAndBackdropFilter) {
  setBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "  backdrop-filter: blur(2px);'></div>");
  PaintLayer* targetLayer =
      toLayoutBox(getLayoutObjectByElementId("target"))->layer();
  PaintLayerPaintingInfo paintingInfo(nullptr, LayoutRect(),
                                      GlobalPaintNormalPhase, LayoutSize());
  EXPECT_FALSE(
      PaintLayerPainter(*targetLayer).paintedOutputInvisible(paintingInfo));
}

TEST_P(PaintLayerPainterTest, DoPaintWithCompositedTinyOpacity) {
  setBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      " will-change: transform'></div>");
  PaintLayer* targetLayer =
      toLayoutBox(getLayoutObjectByElementId("target"))->layer();
  PaintLayerPaintingInfo paintingInfo(nullptr, LayoutRect(),
                                      GlobalPaintNormalPhase, LayoutSize());
  EXPECT_FALSE(
      PaintLayerPainter(*targetLayer).paintedOutputInvisible(paintingInfo));
}

TEST_P(PaintLayerPainterTest, DoPaintWithNonTinyOpacity) {
  setBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.1'></div>");
  PaintLayer* targetLayer =
      toLayoutBox(getLayoutObjectByElementId("target"))->layer();
  PaintLayerPaintingInfo paintingInfo(nullptr, LayoutRect(),
                                      GlobalPaintNormalPhase, LayoutSize());
  EXPECT_FALSE(
      PaintLayerPainter(*targetLayer).paintedOutputInvisible(paintingInfo));
}

}  // namespace blink
