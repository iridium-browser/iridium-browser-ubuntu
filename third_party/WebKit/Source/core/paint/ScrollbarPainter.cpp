// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ScrollbarPainter.h"

#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/BlockPainter.h"
#include "platform/graphics/GraphicsContext.h"
namespace blink {

void ScrollbarPainter::paintPart(GraphicsContext* graphicsContext, ScrollbarPart partType, const IntRect& rect)
{
    LayoutScrollbarPart* partRenderer = m_renderScrollbar.getPart(partType);
    if (!partRenderer)
        return;
    paintIntoRect(partRenderer, graphicsContext, m_renderScrollbar.location(), LayoutRect(rect));
}

void ScrollbarPainter::paintIntoRect(LayoutScrollbarPart* renderScrollbarPart, GraphicsContext* graphicsContext, const LayoutPoint& paintOffset, const LayoutRect& rect)
{
    // Make sure our dimensions match the rect.
    // FIXME: Setting these is a bad layering violation!
    renderScrollbarPart->setLocation(rect.location() - toSize(paintOffset));
    renderScrollbarPart->setWidth(rect.width());
    renderScrollbarPart->setHeight(rect.height());

    // Now do the paint.
    PaintInfo paintInfo(graphicsContext, pixelSnappedIntRect(rect), PaintPhaseBlockBackground, PaintBehaviorNormal);
    BlockPainter blockPainter(*renderScrollbarPart);
    blockPainter.paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhaseChildBlockBackgrounds;
    blockPainter.paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhaseFloat;
    blockPainter.paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhaseForeground;
    blockPainter.paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhaseOutline;
    blockPainter.paint(paintInfo, paintOffset);
}

} // namespace blink
