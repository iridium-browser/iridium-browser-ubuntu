// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ScrollableAreaPainter.h"

#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "core/paint/DeprecatedPaintLayerScrollableArea.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ScrollbarPainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

void ScrollableAreaPainter::paintResizer(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    if (scrollableArea().box().style()->resize() == RESIZE_NONE)
        return;

    IntRect absRect = scrollableArea().resizerCornerRect(scrollableArea().box().pixelSnappedBorderBoxRect(), ResizerForPointer);
    if (absRect.isEmpty())
        return;
    absRect.moveBy(paintOffset);

    if (scrollableArea().resizer()) {
        if (!absRect.intersects(damageRect))
            return;
        ScrollbarPainter::paintIntoRect(scrollableArea().resizer(), context, paintOffset, LayoutRect(absRect));
        return;
    }

    if (!RuntimeEnabledFeatures::slimmingPaintEnabled() && !absRect.intersects(damageRect))
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(*context, scrollableArea().box(), DisplayItem::Resizer))
        return;

    LayoutObjectDrawingRecorder recorder(*context, scrollableArea().box(), DisplayItem::Resizer, absRect);

    drawPlatformResizerImage(context, absRect);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (!scrollableArea().hasOverlayScrollbars() && scrollableArea().hasScrollbar()) {
        GraphicsContextStateSaver stateSaver(*context);
        context->clip(absRect);
        IntRect largerCorner = absRect;
        largerCorner.setSize(IntSize(largerCorner.width() + 1, largerCorner.height() + 1));
        context->setStrokeColor(Color(217, 217, 217));
        context->setStrokeThickness(1.0f);
        context->setFillColor(Color::transparent);
        context->drawRect(largerCorner);
    }
}

void ScrollableAreaPainter::drawPlatformResizerImage(GraphicsContext* context, IntRect resizerCornerRect)
{
    float deviceScaleFactor = blink::deviceScaleFactor(scrollableArea().box().frame());

    RefPtr<Image> resizeCornerImage;
    IntSize cornerResizerSize;
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_REF(Image, resizeCornerImageHiRes, (Image::loadPlatformResource("textAreaResizeCorner@2x")));
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        DEFINE_STATIC_REF(Image, resizeCornerImageLoRes, (Image::loadPlatformResource("textAreaResizeCorner")));
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (scrollableArea().box().style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        context->save();
        context->translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context->scale(-1.0, 1.0);
        context->drawImage(resizeCornerImage.get(), IntRect(IntPoint(), cornerResizerSize));
        context->restore();
        return;
    }
    IntRect imageRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize);
    context->drawImage(resizeCornerImage.get(), imageRect);
}

void ScrollableAreaPainter::paintOverflowControls(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    if (!scrollableArea().box().hasOverflowClip())
        return;

    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = scrollableArea().cachedOverlayScrollbarOffset();

    IntRect localDamageRect = damageRect;
    localDamageRect.moveBy(-adjustedPaintOffset);

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the
    // second pass doesn't need to re-enter the LayoutTree to get it right.
    if (scrollableArea().hasOverlayScrollbars() && !paintingOverlayControls) {
        scrollableArea().setCachedOverlayScrollbarOffset(paintOffset);
        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((scrollableArea().horizontalScrollbar() && scrollableArea().layerForHorizontalScrollbar()) || (scrollableArea().verticalScrollbar() && scrollableArea().layerForVerticalScrollbar()))
            return;
        if (!overflowControlsIntersectRect(localDamageRect))
            return;

        LayoutView* layoutView = scrollableArea().box().view();

        DeprecatedPaintLayer* paintingRoot = scrollableArea().layer()->enclosingLayerWithCompositedDeprecatedPaintLayerMapping(IncludeSelf);
        if (!paintingRoot)
            paintingRoot = layoutView->layer();

        paintingRoot->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !scrollableArea().hasOverlayScrollbars())
        return;

    {
        if (scrollableArea().horizontalScrollbar() && !scrollableArea().layerForHorizontalScrollbar()) {
            TransformRecorder translateRecorder(*context, *scrollableArea().horizontalScrollbar(), AffineTransform::translation(adjustedPaintOffset.x(), adjustedPaintOffset.y()));
            scrollableArea().horizontalScrollbar()->paint(context, localDamageRect);
        }
        if (scrollableArea().verticalScrollbar() && !scrollableArea().layerForVerticalScrollbar()) {
            TransformRecorder translateRecorder(*context, *scrollableArea().verticalScrollbar(), AffineTransform::translation(adjustedPaintOffset.x(), adjustedPaintOffset.y()));
            scrollableArea().verticalScrollbar()->paint(context, localDamageRect);
        }
    }

    if (scrollableArea().layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, adjustedPaintOffset, damageRect);

    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, adjustedPaintOffset, damageRect);
}

bool ScrollableAreaPainter::overflowControlsIntersectRect(const IntRect& localRect) const
{
    const IntRect borderBox = scrollableArea().box().pixelSnappedBorderBoxRect();

    if (scrollableArea().rectForHorizontalScrollbar(borderBox).intersects(localRect))
        return true;

    if (scrollableArea().rectForVerticalScrollbar(borderBox).intersects(localRect))
        return true;

    if (scrollableArea().scrollCornerRect().intersects(localRect))
        return true;

    if (scrollableArea().resizerCornerRect(borderBox, ResizerForPointer).intersects(localRect))
        return true;

    return false;
}


void ScrollableAreaPainter::paintScrollCorner(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    IntRect absRect = scrollableArea().scrollCornerRect();
    if (absRect.isEmpty())
        return;
    absRect.moveBy(paintOffset);

    if (scrollableArea().scrollCorner()) {
        if (!absRect.intersects(damageRect))
            return;
        ScrollbarPainter::paintIntoRect(scrollableArea().scrollCorner(), context, paintOffset, LayoutRect(absRect));
        return;
    }

    if (!RuntimeEnabledFeatures::slimmingPaintEnabled() && !absRect.intersects(damageRect))
        return;

    // We don't want to paint white if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (scrollableArea().hasOverlayScrollbars())
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(*context, scrollableArea().box(), DisplayItem::ScrollbarCorner))
        return;

    LayoutObjectDrawingRecorder recorder(*context, scrollableArea().box(), DisplayItem::ScrollbarCorner, absRect);
    context->fillRect(absRect, Color::white);
}

DeprecatedPaintLayerScrollableArea& ScrollableAreaPainter::scrollableArea() const
{
    return *m_scrollableArea;
}

} // namespace blink
