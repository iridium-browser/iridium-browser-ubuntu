// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerScrollableArea.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace blink {
namespace {

class MockChromeClient : public EmptyChromeClient {
 public:
  MOCK_METHOD3(mockSetToolTip, void(LocalFrame*, const String&, TextDirection));
  void setToolTip(LocalFrame& frame,
                  const String& tooltipText,
                  TextDirection dir) override {
    mockSetToolTip(&frame, tooltipText, dir);
  }
};

}  // namespace {

class PaintLayerScrollableAreaTest : public RenderingTest {
 public:
  PaintLayerScrollableAreaTest()
      : RenderingTest(EmptyFrameLoaderClient::create()),
        m_chromeClient(new MockChromeClient) {}

  ~PaintLayerScrollableAreaTest() {
    testing::Mock::VerifyAndClearExpectations(&chromeClient());
  }

  MockChromeClient& chromeClient() const override { return *m_chromeClient; }

  BackgroundPaintLocation backgroundPaintLocation(const char* elementId) {
    PaintLayer* paintLayer =
        toLayoutBoxModelObject(getLayoutObjectByElementId(elementId))->layer();
    return paintLayer->backgroundPaintLocation();
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    enableCompositing();
  }

  Persistent<MockChromeClient> m_chromeClient;
};

TEST_F(PaintLayerScrollableAreaTest,
       CanPaintBackgroundOntoScrollingContentsLayer) {
  document().frame()->settings()->setPreferCompositingToLCDTextEnabled(true);
  setBodyInnerHTML(
      "<style>"
      ".scroller { overflow: scroll; will-change: transform; width: 300px; "
      "height: 300px;} .spacer { height: 1000px; }"
      "#scroller13::-webkit-scrollbar { width: 13px; height: 13px;}"
      "</style>"
      "<div id='scroller1' class='scroller' style='background: white local;'>"
      "    <div id='negative-composited-child' style='background-color: red; "
      "width: 1px; height: 1px; position: absolute; backface-visibility: "
      "hidden; z-index: -1'></div>"
      "    <div class='spacer'></div>"
      "</div>"
      "<div id='scroller2' class='scroller' style='background: white "
      "content-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller3' class='scroller' style='background: white local "
      "content-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller4' class='scroller' style='background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg), white local;'><div "
      "class='spacer'></div></div>"
      "<div id='scroller5' class='scroller' style='background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white "
      "local;'><div class='spacer'></div></div>"
      "<div id='scroller6' class='scroller' style='background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white "
      "padding-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller7' class='scroller' style='background: "
      "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUg) local, white "
      "content-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller8' class='scroller' style='background: white "
      "border-box;'><div class='spacer'></div></div>"
      "<div id='scroller9' class='scroller' style='background: white "
      "border-box; border: 10px solid black;'><div class='spacer'></div></div>"
      "<div id='scroller10' class='scroller' style='background: white "
      "border-box; border: 10px solid rgba(0, 0, 0, 0.5);'><div "
      "class='spacer'></div></div>"
      "<div id='scroller11' class='scroller' style='background: white "
      "content-box;'><div class='spacer'></div></div>"
      "<div id='scroller12' class='scroller' style='background: white "
      "content-box; padding: 10px;'><div class='spacer'></div></div>"
      "<div id='scroller13' class='scroller' style='background: white "
      "border-box;'><div class='spacer'></div></div>"
      "<div id='scroller14' class='scroller' style='background: white; border: "
      "1px solid black; outline: 1px solid blue; outline-offset: -1px;'><div "
      "class='spacer'></div></div>"
      "<div id='scroller15' class='scroller' style='background: white; border: "
      "1px solid black; outline: 1px solid blue; outline-offset: -2px;'><div "
      "class='spacer'></div></div>"
      "<div id='scroller16' class='scroller' style='background: white; clip: "
      "rect(0px,10px,10px,0px);'><div class='spacer'></div></div>"
      "<div id='scroller17' class='scroller' style='background:"
      "rgba(255, 255, 255, 0.5) border-box; border: 5px solid "
      "rgba(0, 0, 0, 0.5);'><div class='spacer'></div></div>");

  // #scroller1 cannot paint background into scrolling contents layer because it
  // has a negative z-index child.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer,
            backgroundPaintLocation("scroller1"));

  // #scroller2 cannot paint background into scrolling contents layer because it
  // has a content-box clip without local attachment.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer,
            backgroundPaintLocation("scroller2"));

  // #scroller3 can paint background into scrolling contents layer.
  EXPECT_EQ(BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller3"));

  // #scroller4 cannot paint background into scrolling contents layer because
  // the background image is not locally attached.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer,
            backgroundPaintLocation("scroller4"));

  // #scroller5 can paint background into scrolling contents layer because both
  // the image and color are locally attached.
  EXPECT_EQ(BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller5"));

  // #scroller6 can paint background into scrolling contents layer because the
  // image is locally attached and even though the color is not, it is filled to
  // the padding box so it will be drawn the same as a locally attached
  // background.
  EXPECT_EQ(BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller6"));

  // #scroller7 cannot paint background into scrolling contents layer because
  // the color is filled to the content box and we have padding so it is not
  // equivalent to a locally attached background.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer,
            backgroundPaintLocation("scroller7"));

  // #scroller8 can paint background into scrolling contents layer because its
  // border-box is equivalent to its padding box since it has no border.
  EXPECT_EQ(BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller8"));

  // #scroller9 can paint background into scrolling contents layer because its
  // border is opaque so it completely covers the background outside of the
  // padding-box.
  EXPECT_EQ(BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller9"));

  // #scroller10 paints the background into both layers because its border is
  // partially transparent so the background must be drawn to the
  // border-box edges.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer | BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller10"));

  // #scroller11 can paint background into scrolling contents layer because its
  // content-box is equivalent to its padding box since it has no padding.
  EXPECT_EQ(BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller11"));

  // #scroller12 cannot paint background into scrolling contents layer because
  // it has padding so its content-box is not equivalent to its padding-box.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer,
            backgroundPaintLocation("scroller12"));

  // #scroller13 paints the background into both layers because it has a custom
  // scrollbar which the background may need to draw under.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer | BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller13"));

  // #scroller14 can paint background into scrolling contents layer because the
  // outline is drawn outside the padding box.
  EXPECT_EQ(BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller14"));

  // #scroller15 can paint background into scrolling contents layer because
  // the outline is drawn into the decoration layer which will not be covered
  // up.
  EXPECT_EQ(BackgroundPaintInScrollingContents,
            backgroundPaintLocation("scroller15"));

  // #scroller16 cannot paint background into scrolling contents layer because
  // the scroller has a clip which would not be respected by the scrolling
  // contents layer.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer,
            backgroundPaintLocation("scroller16"));

  // #scroller17 can only be painted once as it is translucent, and it must
  // be painted in the graphics layer to be under the translucent border.
  EXPECT_EQ(BackgroundPaintInGraphicsLayer,
            backgroundPaintLocation("scroller17"));
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueContainedLayersPromoted) {
  RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

  setBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; "
      "contain: paint; background: white local content-box; "
      "border: 10px solid rgba(0, 255, 0, 0.5); }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* scroller = document().getElementById("scroller");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking());
  ASSERT_TRUE(paintLayer->graphicsLayerBacking());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());
}

// Tests that we don't promote scrolling content which would not be contained.
// Promoting the scroller would also require promoting the positioned div
// which would lose subpixel anti-aliasing due to its transparent background.
TEST_F(PaintLayerScrollableAreaTest, NonContainedLayersNotPromoted) {
  RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

  setBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; "
      "background: white local content-box; "
      "border: 10px solid rgba(0, 255, 0, 0.5); }"
      "#scrolled { height: 300px; }"
      "#positioned { position: relative; }"
      "</style>"
      "<div id=\"scroller\">"
      "  <div id=\"positioned\">Not contained by scroller.</div>"
      "  <div id=\"scrolled\"></div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* scroller = document().getElementById("scroller");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, TransparentLayersNotPromoted) {
  RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

  setBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "rgba(0, 255, 0, 0.5) local content-box; border: 10px solid rgba(0, 255, "
      "0, 0.5); contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* scroller = document().getElementById("scroller");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersDepromotedOnStyleChange) {
  RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

  setBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "white local content-box; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* scroller = document().getElementById("scroller");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());

  // Change the background to transparent
  scroller->setAttribute(
      HTMLNames::styleAttr,
      "background: rgba(255,255,255,0.5) local content-box;");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());
}

TEST_F(PaintLayerScrollableAreaTest, OpaqueLayersPromotedOnStyleChange) {
  RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled(true);

  setBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "rgba(255,255,255,0.5) local content-box; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* scroller = document().getElementById("scroller");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());

  // Change the background to transparent
  scroller->setAttribute(HTMLNames::styleAttr,
                         "background: white local content-box;");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking());
  ASSERT_TRUE(paintLayer->graphicsLayerBacking());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());
}

// Tests that a transform on the scroller or an ancestor will prevent promotion
// TODO(flackr): Allow integer transforms as long as all of the ancestor
// transforms are also integer.
TEST_F(PaintLayerScrollableAreaTest, OnlyNonTransformedOpaqueLayersPromoted) {
  ScopedCompositeOpaqueScrollersForTest compositeOpaqueScrollers(true);

  setBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "white local content-box; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"scroller\"><div id=\"scrolled\"></div></div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* parent = document().getElementById("parent");
  Element* scroller = document().getElementById("scroller");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking());
  ASSERT_TRUE(paintLayer->graphicsLayerBacking());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Change the parent to have a transform.
  parent->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());

  // Change the parent to have no transform again.
  parent->removeAttribute(HTMLNames::styleAttr);
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking());
  ASSERT_TRUE(paintLayer->graphicsLayerBacking());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Apply a transform to the scroller directly.
  scroller->setAttribute(HTMLNames::styleAttr, "transform: translate(1px, 0);");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());
}

// Test that opacity applied to the scroller or an ancestor will cause the
// scrolling contents layer to not be promoted.
TEST_F(PaintLayerScrollableAreaTest, OnlyOpaqueLayersPromoted) {
  ScopedCompositeOpaqueScrollersForTest compositeOpaqueScrollers(true);

  setBodyInnerHTML(
      "<style>"
      "#scroller { overflow: scroll; height: 200px; width: 200px; background: "
      "white local content-box; contain: paint; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"parent\">"
      "  <div id=\"scroller\"><div id=\"scrolled\"></div></div>"
      "</div>");
  document().view()->updateAllLifecyclePhases();

  EXPECT_TRUE(RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled());
  Element* parent = document().getElementById("parent");
  Element* scroller = document().getElementById("scroller");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking());
  ASSERT_TRUE(paintLayer->graphicsLayerBacking());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Change the parent to be partially translucent.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 0.5;");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());

  // Change the parent to be opaque again.
  parent->setAttribute(HTMLNames::styleAttr, "opacity: 1;");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking());
  ASSERT_TRUE(paintLayer->graphicsLayerBacking());
  EXPECT_TRUE(paintLayer->graphicsLayerBacking()->contentsOpaque());

  // Make the scroller translucent.
  scroller->setAttribute(HTMLNames::styleAttr, "opacity: 0.5");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());
  EXPECT_FALSE(paintLayer->graphicsLayerBacking());
}

// Ensure OverlayScrollbarColorTheme get updated when page load
TEST_F(PaintLayerScrollableAreaTest, OverlayScrollbarColorThemeUpdated) {
  setBodyInnerHTML(
      "<style>"
      "div { overflow: scroll; }"
      "#white { background-color: white; }"
      "#black { background-color: black; }"
      "</style>"
      "<div id=\"none\">a</div>"
      "<div id=\"white\">b</div>"
      "<div id=\"black\">c</div>");
  document().view()->updateAllLifecyclePhases();

  Element* none = document().getElementById("none");
  Element* white = document().getElementById("white");
  Element* black = document().getElementById("black");

  PaintLayer* noneLayer = toLayoutBoxModelObject(none->layoutObject())->layer();
  PaintLayer* whiteLayer =
      toLayoutBoxModelObject(white->layoutObject())->layer();
  PaintLayer* blackLayer =
      toLayoutBoxModelObject(black->layoutObject())->layer();

  ASSERT_TRUE(noneLayer);
  ASSERT_TRUE(whiteLayer);
  ASSERT_TRUE(blackLayer);

  ASSERT_EQ(ScrollbarOverlayColorTheme::ScrollbarOverlayColorThemeDark,
            noneLayer->getScrollableArea()->getScrollbarOverlayColorTheme());
  ASSERT_EQ(ScrollbarOverlayColorTheme::ScrollbarOverlayColorThemeDark,
            whiteLayer->getScrollableArea()->getScrollbarOverlayColorTheme());
  ASSERT_EQ(ScrollbarOverlayColorTheme::ScrollbarOverlayColorThemeLight,
            blackLayer->getScrollableArea()->getScrollbarOverlayColorTheme());
}

// Test that css clip applied to the scroller will cause the
// scrolling contents layer to not be promoted.
TEST_F(PaintLayerScrollableAreaTest,
       OnlyAutoClippedScrollingContentsLayerPromoted) {
  setBodyInnerHTML(
      "<style>"
      ".clip { clip: rect(0px,60px,50px,0px); }"
      "#scroller { position: absolute; overflow: auto;"
      "height: 100px; width: 100px; background: grey;"
      "will-change:transform; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  document().view()->updateAllLifecyclePhases();

  Element* scroller = document().getElementById("scroller");
  PaintLayer* paintLayer =
      toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());

  // Add clip to scroller.
  scroller->setAttribute("class", "clip", ASSERT_NO_EXCEPTION);
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_FALSE(paintLayer->needsCompositedScrolling());

  // Change the scroller to be auto clipped again.
  scroller->removeAttribute("class");
  document().view()->updateAllLifecyclePhases();
  paintLayer = toLayoutBoxModelObject(scroller->layoutObject())->layer();
  ASSERT_TRUE(paintLayer);
  EXPECT_TRUE(paintLayer->needsCompositedScrolling());
}

TEST_F(PaintLayerScrollableAreaTest, HideTooltipWhenScrollPositionChanges) {
  setBodyInnerHTML(
      "<style>"
      "#scroller { width: 100px; height: 100px; overflow: scroll; }"
      "#scrolled { height: 300px; }"
      "</style>"
      "<div id=\"scroller\"><div id=\"scrolled\"></div></div>");
  document().view()->updateAllLifecyclePhases();

  Element* scroller = document().getElementById("scroller");
  PaintLayerScrollableArea* scrollableArea =
      toLayoutBoxModelObject(scroller->layoutObject())->getScrollableArea();
  ASSERT_TRUE(scrollableArea);

  EXPECT_CALL(chromeClient(), mockSetToolTip(document().frame(), String(), _))
      .Times(1);
  scrollableArea->setScrollOffset(ScrollOffset(1, 1), UserScroll);

  // Programmatic scrolling should not dismiss the tooltip, so setToolTip
  // should not be called for this invocation.
  EXPECT_CALL(chromeClient(), mockSetToolTip(document().frame(), String(), _))
      .Times(0);
  scrollableArea->setScrollOffset(ScrollOffset(2, 2), ProgrammaticScroll);
}
}
