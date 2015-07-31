// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/InlinePainter.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/LineBoxListPainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include <limits>

namespace blink {

void InlinePainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_layoutInline);
    LineBoxListPainter(*m_layoutInline.lineBoxes()).paint(&m_layoutInline, paintInfo, paintOffset);
}

void InlinePainter::paintOutline(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    const ComputedStyle& styleToUse = m_layoutInline.styleRef();
    if (!styleToUse.hasOutline())
        return;

    if (styleToUse.outlineStyleIsAuto()) {
        if (LayoutTheme::theme().shouldDrawDefaultFocusRing(&m_layoutInline)) {
            Vector<LayoutRect> focusRingRects;
            m_layoutInline.addFocusRingRects(focusRingRects, paintOffset);
            LayoutRect focusRingBoundingRect;
            for (const auto& rect : focusRingRects)
                focusRingBoundingRect.unite(rect);

            LayoutObjectDrawingRecorder recorder(*paintInfo.context, m_layoutInline, paintInfo.phase, focusRingBoundingRect);
            if (recorder.canUseCachedDrawing())
                return;

            // Only paint the focus ring by hand if the theme isn't able to draw the focus ring.
            ObjectPainter(m_layoutInline).paintFocusRing(paintInfo, styleToUse, focusRingRects);
        }
        return;
    }

    if (styleToUse.outlineStyle() == BNONE)
        return;

    Vector<LayoutRect> rects;

    rects.append(LayoutRect());
    for (InlineFlowBox* curr = m_layoutInline.firstLineBox(); curr; curr = curr->nextLineBox()) {
        RootInlineBox& root = curr->root();
        LayoutUnit top = std::max<LayoutUnit>(root.lineTop(), curr->logicalTop());
        LayoutUnit bottom = std::min<LayoutUnit>(root.lineBottom(), curr->logicalBottom());
        rects.append(LayoutRect(curr->x(), top, curr->logicalWidth(), bottom - top));
    }
    rects.append(LayoutRect());

    Color outlineColor = m_layoutInline.resolveColor(styleToUse, CSSPropertyOutlineColor);
    bool useTransparencyLayer = outlineColor.hasAlpha();

    int outlineWidth = styleToUse.outlineWidth();
    LayoutRect bounds;
    for (const auto& rect : rects) {
        LayoutRect rectCopy(rect);
        rectCopy.expand(LayoutSize(outlineWidth, outlineWidth));
        bounds.unite(rectCopy);
    }
    bounds.moveBy(paintOffset);

    LayoutObjectDrawingRecorder recorder(*paintInfo.context, m_layoutInline, paintInfo.phase, bounds);
    if (recorder.canUseCachedDrawing())
        return;

    GraphicsContext* graphicsContext = paintInfo.context;
    if (useTransparencyLayer) {
        graphicsContext->beginLayer(static_cast<float>(outlineColor.alpha()) / 255);
        outlineColor = Color(outlineColor.red(), outlineColor.green(), outlineColor.blue());
    }

    for (unsigned i = 1; i < rects.size() - 1; i++)
        paintOutlineForLine(graphicsContext, paintOffset, rects.at(i - 1), rects.at(i), rects.at(i + 1), outlineColor);

    if (useTransparencyLayer)
        graphicsContext->endLayer();
}

void InlinePainter::paintOutlineForLine(GraphicsContext* graphicsContext, const LayoutPoint& paintOffset,
    const LayoutRect& lastline, const LayoutRect& thisline, const LayoutRect& nextline, const Color outlineColor)
{
    const ComputedStyle& styleToUse = m_layoutInline.styleRef();
    int outlineWidth = styleToUse.outlineWidth();
    EBorderStyle outlineStyle = styleToUse.outlineStyle();

    bool antialias = BoxPainter::shouldAntialiasLines(graphicsContext);

    int offset = m_layoutInline.style()->outlineOffset();

    LayoutRect box(LayoutPoint(paintOffset.x() + thisline.x() - offset, paintOffset.y() + thisline.y() - offset),
        LayoutSize(thisline.width() + offset, thisline.height() + offset));

    IntRect pixelSnappedBox = pixelSnappedIntRect(box);
    if (pixelSnappedBox.width() < 0 || pixelSnappedBox.height() < 0)
        return;
    // Note that we use IntRect below for working with solely x/width values, simplifying logic at cost of a bit of memory.
    IntRect pixelSnappedLastLine = pixelSnappedIntRect(paintOffset.x() + lastline.x() - offset, 0, lastline.width() + offset, 0);
    IntRect pixelSnappedNextLine = pixelSnappedIntRect(paintOffset.x() + nextline.x() - offset, 0, nextline.width() + offset, 0);

    const int fallbackMaxOutlineX = std::numeric_limits<int>::max();
    const int fallbackMinOutlineX = std::numeric_limits<int>::min();

    // left edge
    ObjectPainter::drawLineForBoxSide(graphicsContext,
        pixelSnappedBox.x() - outlineWidth,
        pixelSnappedBox.y() - (lastline.isEmpty() || thisline.x() < lastline.x() || (lastline.maxX() - 1) <= thisline.x() ? outlineWidth : 0),
        pixelSnappedBox.x(),
        pixelSnappedBox.maxY() + (nextline.isEmpty() || thisline.x() <= nextline.x() || (nextline.maxX() - 1) <= thisline.x() ? outlineWidth : 0),
        BSLeft,
        outlineColor, outlineStyle,
        (lastline.isEmpty() || thisline.x() < lastline.x() || (lastline.maxX() - 1) <= thisline.x() ? outlineWidth : -outlineWidth),
        (nextline.isEmpty() || thisline.x() <= nextline.x() || (nextline.maxX() - 1) <= thisline.x() ? outlineWidth : -outlineWidth),
        antialias);

    // right edge
    ObjectPainter::drawLineForBoxSide(graphicsContext,
        pixelSnappedBox.maxX(),
        pixelSnappedBox.y() - (lastline.isEmpty() || lastline.maxX() < thisline.maxX() || (thisline.maxX() - 1) <= lastline.x() ? outlineWidth : 0),
        pixelSnappedBox.maxX() + outlineWidth,
        pixelSnappedBox.maxY() + (nextline.isEmpty() || nextline.maxX() <= thisline.maxX() || (thisline.maxX() - 1) <= nextline.x() ? outlineWidth : 0),
        BSRight,
        outlineColor, outlineStyle,
        (lastline.isEmpty() || lastline.maxX() < thisline.maxX() || (thisline.maxX() - 1) <= lastline.x() ? outlineWidth : -outlineWidth),
        (nextline.isEmpty() || nextline.maxX() <= thisline.maxX() || (thisline.maxX() - 1) <= nextline.x() ? outlineWidth : -outlineWidth),
        antialias);
    // upper edge
    if (thisline.x() < lastline.x()) {
        ObjectPainter::drawLineForBoxSide(graphicsContext,
            pixelSnappedBox.x() - outlineWidth,
            pixelSnappedBox.y() - outlineWidth,
            std::min(pixelSnappedBox.maxX() + outlineWidth, (lastline.isEmpty() ? fallbackMaxOutlineX : pixelSnappedLastLine.x())),
            pixelSnappedBox.y(),
            BSTop, outlineColor, outlineStyle,
            outlineWidth,
            (!lastline.isEmpty() && paintOffset.x() + lastline.x() + 1 < pixelSnappedBox.maxX() + outlineWidth) ? -outlineWidth : outlineWidth,
            antialias);
    }

    if (lastline.maxX() < thisline.maxX()) {
        ObjectPainter::drawLineForBoxSide(graphicsContext,
            std::max(lastline.isEmpty() ? fallbackMinOutlineX : pixelSnappedLastLine.maxX(), pixelSnappedBox.x() - outlineWidth),
            pixelSnappedBox.y() - outlineWidth,
            pixelSnappedBox.maxX() + outlineWidth,
            pixelSnappedBox.y(),
            BSTop, outlineColor, outlineStyle,
            (!lastline.isEmpty() && pixelSnappedBox.x() - outlineWidth < paintOffset.x() + lastline.maxX()) ? -outlineWidth : outlineWidth,
            outlineWidth, antialias);
    }

    if (thisline.x() == thisline.maxX()) {
        ObjectPainter::drawLineForBoxSide(graphicsContext,
            pixelSnappedBox.x() - outlineWidth,
            pixelSnappedBox.y() - outlineWidth,
            pixelSnappedBox.maxX() + outlineWidth,
            pixelSnappedBox.y(),
            BSTop, outlineColor, outlineStyle,
            outlineWidth,
            outlineWidth,
            antialias);
    }

    // lower edge
    if (thisline.x() < nextline.x()) {
        ObjectPainter::drawLineForBoxSide(graphicsContext,
            pixelSnappedBox.x() - outlineWidth,
            pixelSnappedBox.maxY(),
            std::min(pixelSnappedBox.maxX() + outlineWidth, !nextline.isEmpty() ? pixelSnappedNextLine.x() + 1 : fallbackMaxOutlineX),
            pixelSnappedBox.maxY() + outlineWidth,
            BSBottom, outlineColor, outlineStyle,
            outlineWidth,
            (!nextline.isEmpty() && paintOffset.x() + nextline.x() + 1 < pixelSnappedBox.maxX() + outlineWidth) ? -outlineWidth : outlineWidth,
            antialias);
    }

    if (nextline.maxX() < thisline.maxX()) {
        ObjectPainter::drawLineForBoxSide(graphicsContext,
            std::max(!nextline.isEmpty() ? pixelSnappedNextLine.maxX() : fallbackMinOutlineX, pixelSnappedBox.x() - outlineWidth),
            pixelSnappedBox.maxY(),
            pixelSnappedBox.maxX() + outlineWidth,
            pixelSnappedBox.maxY() + outlineWidth,
            BSBottom, outlineColor, outlineStyle,
            (!nextline.isEmpty() && pixelSnappedBox.x() - outlineWidth < paintOffset.x() + nextline.maxX()) ? -outlineWidth : outlineWidth,
            outlineWidth, antialias);
    }

    if (thisline.x() == thisline.maxX()) {
        ObjectPainter::drawLineForBoxSide(graphicsContext,
            pixelSnappedBox.x() - outlineWidth,
            pixelSnappedBox.maxY(),
            pixelSnappedBox.maxX() + outlineWidth,
            pixelSnappedBox.maxY() + outlineWidth,
            BSBottom, outlineColor, outlineStyle,
            outlineWidth,
            outlineWidth,
            antialias);
    }
}

} // namespace blink
