// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutObject.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "platform/json/JSONValues.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutObjectTest : public RenderingTest {
 public:
  LayoutObjectTest() : RenderingTest(EmptyFrameLoaderClient::create()) {}
};

TEST_F(LayoutObjectTest, LayoutDecoratedNameCalledWithPositionedObject) {
  setBodyInnerHTML("<div id='div' style='position: fixed'>test</div>");
  Element* div = document().getElementById(AtomicString("div"));
  ASSERT(div);
  LayoutObject* obj = div->layoutObject();
  ASSERT(obj);
  EXPECT_STREQ("LayoutBlockFlow (positioned)",
               obj->decoratedName().ascii().data());
}

// Some display checks.
TEST_F(LayoutObjectTest, DisplayNoneCreateObject) {
  setBodyInnerHTML("<div style='display:none'></div>");
  EXPECT_EQ(nullptr, document().body()->firstChild()->layoutObject());
}

TEST_F(LayoutObjectTest, DisplayBlockCreateObject) {
  setBodyInnerHTML("<foo style='display:block'></foo>");
  LayoutObject* layoutObject = document().body()->firstChild()->layoutObject();
  EXPECT_NE(nullptr, layoutObject);
  EXPECT_TRUE(layoutObject->isLayoutBlockFlow());
  EXPECT_FALSE(layoutObject->isInline());
}

TEST_F(LayoutObjectTest, DisplayInlineBlockCreateObject) {
  setBodyInnerHTML("<foo style='display:inline-block'></foo>");
  LayoutObject* layoutObject = document().body()->firstChild()->layoutObject();
  EXPECT_NE(nullptr, layoutObject);
  EXPECT_TRUE(layoutObject->isLayoutBlockFlow());
  EXPECT_TRUE(layoutObject->isInline());
}

// Containing block test.
TEST_F(LayoutObjectTest, ContainingBlockLayoutViewShouldBeNull) {
  EXPECT_EQ(nullptr, layoutView().containingBlock());
}

TEST_F(LayoutObjectTest, ContainingBlockBodyShouldBeDocumentElement) {
  EXPECT_EQ(document().body()->layoutObject()->containingBlock(),
            document().documentElement()->layoutObject());
}

TEST_F(LayoutObjectTest, ContainingBlockDocumentElementShouldBeLayoutView) {
  EXPECT_EQ(document().documentElement()->layoutObject()->containingBlock(),
            layoutView());
}

TEST_F(LayoutObjectTest, ContainingBlockStaticLayoutObjectShouldBeParent) {
  setBodyInnerHTML("<foo style='position:static'></foo>");
  LayoutObject* bodyLayoutObject = document().body()->layoutObject();
  LayoutObject* layoutObject = bodyLayoutObject->slowFirstChild();
  EXPECT_EQ(layoutObject->containingBlock(), bodyLayoutObject);
}

TEST_F(LayoutObjectTest,
       ContainingBlockAbsoluteLayoutObjectShouldBeLayoutView) {
  setBodyInnerHTML("<foo style='position:absolute'></foo>");
  LayoutObject* layoutObject =
      document().body()->layoutObject()->slowFirstChild();
  EXPECT_EQ(layoutObject->containingBlock(), layoutView());
}

TEST_F(
    LayoutObjectTest,
    ContainingBlockAbsoluteLayoutObjectShouldBeNonStaticallyPositionedBlockAncestor) {
  setBodyInnerHTML(
      "<div style='position:relative'><bar "
      "style='position:absolute'></bar></div>");
  LayoutObject* containingBlocklayoutObject =
      document().body()->layoutObject()->slowFirstChild();
  LayoutObject* layoutObject = containingBlocklayoutObject->slowFirstChild();
  EXPECT_EQ(layoutObject->containingBlock(), containingBlocklayoutObject);
}

TEST_F(
    LayoutObjectTest,
    ContainingBlockAbsoluteLayoutObjectShouldNotBeNonStaticallyPositionedInlineAncestor) {
  setBodyInnerHTML(
      "<span style='position:relative'><bar "
      "style='position:absolute'></bar></span>");
  LayoutObject* bodyLayoutObject = document().body()->layoutObject();
  LayoutObject* layoutObject =
      bodyLayoutObject->slowFirstChild()->slowFirstChild();

  // Sanity check: Make sure we don't generate anonymous objects.
  EXPECT_EQ(nullptr, bodyLayoutObject->slowFirstChild()->nextSibling());
  EXPECT_EQ(nullptr, layoutObject->slowFirstChild());
  EXPECT_EQ(nullptr, layoutObject->nextSibling());

  EXPECT_EQ(layoutObject->containingBlock(), bodyLayoutObject);
}

TEST_F(LayoutObjectTest, PaintingLayerOfOverflowClipLayerUnderColumnSpanAll) {
  setBodyInnerHTML(
      "<div id='columns' style='columns: 3'>"
      "  <div style='column-span: all'>"
      "    <div id='overflow-clip-layer' style='height: 100px; overflow: "
      "hidden'></div>"
      "  </div>"
      "</div>");

  LayoutObject* overflowClipObject =
      getLayoutObjectByElementId("overflow-clip-layer");
  LayoutBlock* columns = toLayoutBlock(getLayoutObjectByElementId("columns"));
  EXPECT_EQ(columns->layer(), overflowClipObject->paintingLayer());
}

TEST_F(LayoutObjectTest, FloatUnderBlock) {
  setBodyInnerHTML(
      "<div id='layered-div' style='position: absolute'>"
      "  <div id='container'>"
      "    <div id='floating' style='float: left'>FLOAT</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject* layeredDiv =
      toLayoutBoxModelObject(getLayoutObjectByElementId("layered-div"));
  LayoutBoxModelObject* container =
      toLayoutBoxModelObject(getLayoutObjectByElementId("container"));
  LayoutObject* floating = getLayoutObjectByElementId("floating");

  EXPECT_EQ(layeredDiv->layer(), layeredDiv->paintingLayer());
  EXPECT_EQ(layeredDiv->layer(), floating->paintingLayer());
  EXPECT_EQ(container, floating->container());
  EXPECT_EQ(container, floating->containingBlock());
}

TEST_F(LayoutObjectTest, FloatUnderInline) {
  setBodyInnerHTML(
      "<div id='layered-div' style='position: absolute'>"
      "  <div id='container'>"
      "    <span id='layered-span' style='position: relative'>"
      "      <div id='floating' style='float: left'>FLOAT</div>"
      "    </span>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject* layeredDiv =
      toLayoutBoxModelObject(getLayoutObjectByElementId("layered-div"));
  LayoutBoxModelObject* container =
      toLayoutBoxModelObject(getLayoutObjectByElementId("container"));
  LayoutBoxModelObject* layeredSpan =
      toLayoutBoxModelObject(getLayoutObjectByElementId("layered-span"));
  LayoutObject* floating = getLayoutObjectByElementId("floating");

  EXPECT_EQ(layeredDiv->layer(), layeredDiv->paintingLayer());
  EXPECT_EQ(layeredSpan->layer(), layeredSpan->paintingLayer());
  EXPECT_EQ(layeredDiv->layer(), floating->paintingLayer());
  EXPECT_EQ(container, floating->container());
  EXPECT_EQ(container, floating->containingBlock());

  LayoutObject::AncestorSkipInfo skipInfo(layeredSpan);
  EXPECT_EQ(container, floating->container(&skipInfo));
  EXPECT_TRUE(skipInfo.ancestorSkipped());

  skipInfo = LayoutObject::AncestorSkipInfo(container);
  EXPECT_EQ(container, floating->container(&skipInfo));
  EXPECT_FALSE(skipInfo.ancestorSkipped());
}

TEST_F(LayoutObjectTest, MutableForPaintingClearPaintFlags) {
  LayoutObject* object = document().body()->layoutObject();
  object->setShouldDoFullPaintInvalidation();
  EXPECT_TRUE(object->shouldDoFullPaintInvalidation());
  object->m_bitfields.setChildShouldCheckForPaintInvalidation(true);
  EXPECT_TRUE(object->m_bitfields.childShouldCheckForPaintInvalidation());
  object->setMayNeedPaintInvalidation();
  EXPECT_TRUE(object->mayNeedPaintInvalidation());
  object->setMayNeedPaintInvalidationSubtree();
  EXPECT_TRUE(object->mayNeedPaintInvalidationSubtree());
  object->setMayNeedPaintInvalidationAnimatedBackgroundImage();
  EXPECT_TRUE(object->mayNeedPaintInvalidationAnimatedBackgroundImage());
  object->setShouldInvalidateSelection();
  EXPECT_TRUE(object->shouldInvalidateSelection());
  object->setBackgroundChangedSinceLastPaintInvalidation();
  EXPECT_TRUE(object->backgroundChangedSinceLastPaintInvalidation());
  object->setNeedsPaintPropertyUpdate();
  EXPECT_TRUE(object->needsPaintPropertyUpdate());
  object->m_bitfields.setDescendantNeedsPaintPropertyUpdate(true);
  EXPECT_TRUE(object->descendantNeedsPaintPropertyUpdate());

  ScopedSlimmingPaintV2ForTest enableSPv2(true);
  document().lifecycle().advanceTo(DocumentLifecycle::InPrePaint);
  object->getMutableForPainting().clearPaintFlags();

  EXPECT_FALSE(object->shouldDoFullPaintInvalidation());
  EXPECT_FALSE(object->m_bitfields.childShouldCheckForPaintInvalidation());
  EXPECT_FALSE(object->mayNeedPaintInvalidation());
  EXPECT_FALSE(object->mayNeedPaintInvalidationSubtree());
  EXPECT_FALSE(object->mayNeedPaintInvalidationAnimatedBackgroundImage());
  EXPECT_FALSE(object->shouldInvalidateSelection());
  EXPECT_FALSE(object->backgroundChangedSinceLastPaintInvalidation());
  EXPECT_FALSE(object->needsPaintPropertyUpdate());
  EXPECT_FALSE(object->descendantNeedsPaintPropertyUpdate());
}

}  // namespace blink
