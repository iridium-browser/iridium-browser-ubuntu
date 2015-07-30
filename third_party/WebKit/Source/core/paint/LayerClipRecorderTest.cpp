// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/LayerClipRecorder.h"

#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/DeprecatedPaintLayerCompositor.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class LayerClipRecorderTest : public RenderingTest {
public:
    LayerClipRecorderTest() : m_layoutView(nullptr) { }

protected:
    LayoutView& layoutView() { return *m_layoutView; }
    DisplayItemList& rootDisplayItemList() { return *layoutView().layer()->graphicsLayerBacking()->displayItemList(); }

private:
    virtual void SetUp() override
    {
        RuntimeEnabledFeatures::setSlimmingPaintEnabled(true);

        RenderingTest::SetUp();
        enableCompositing();

        m_layoutView = document().view()->layoutView();
        ASSERT_TRUE(m_layoutView);
    }

    LayoutView* m_layoutView;
};

void drawEmptyClip(GraphicsContext& context, LayoutView& layoutView, PaintPhase phase, const FloatRect& bound)
{
    LayoutRect rect(1, 1, 9, 9);
    ClipRect clipRect(rect);
    LayerClipRecorder LayerClipRecorder(context, *layoutView.compositor()->rootLayer()->layoutObject(), DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
}

void drawRectInClip(GraphicsContext& context, LayoutView& layoutView, PaintPhase phase, const FloatRect& bound)
{
    IntRect rect(1, 1, 9, 9);
    ClipRect clipRect((LayoutRect(rect)));
    LayerClipRecorder LayerClipRecorder(context, *layoutView.compositor()->rootLayer()->layoutObject(), DisplayItem::ClipLayerForeground, clipRect, 0, LayoutPoint(), PaintLayerFlags());
    LayoutObjectDrawingRecorder drawingRecorder(context, layoutView, phase, bound);
    if (!drawingRecorder.canUseCachedDrawing())
        context.drawRect(rect);
}

TEST_F(LayerClipRecorderTest, Single)
{
    GraphicsContext context(&rootDisplayItemList());
    FloatRect bound = layoutView().viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().displayItems().size());

    drawRectInClip(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)3, rootDisplayItemList().displayItems().size());
    EXPECT_TRUE(rootDisplayItemList().displayItems()[0]->isClip());
    EXPECT_TRUE(rootDisplayItemList().displayItems()[1]->isDrawing());
    EXPECT_TRUE(rootDisplayItemList().displayItems()[2]->isEndClip());
}

TEST_F(LayerClipRecorderTest, Empty)
{
    GraphicsContext context(&rootDisplayItemList());
    FloatRect bound = layoutView().viewRect();
    EXPECT_EQ((size_t)0, rootDisplayItemList().displayItems().size());

    drawEmptyClip(context, layoutView(), PaintPhaseForeground, bound);
    rootDisplayItemList().commitNewDisplayItems();
    EXPECT_EQ((size_t)0, rootDisplayItemList().displayItems().size());
}

}
}
