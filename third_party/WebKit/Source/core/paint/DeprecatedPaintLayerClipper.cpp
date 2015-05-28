/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "core/paint/DeprecatedPaintLayerClipper.h"

#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/paint/DeprecatedPaintLayer.h"

namespace blink {

static void adjustClipRectsForChildren(const LayoutObject& renderer, ClipRects& clipRects)
{
    EPosition position = renderer.style()->position();
    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (position == FixedPosition) {
        clipRects.setPosClipRect(clipRects.fixedClipRect());
        clipRects.setOverflowClipRect(clipRects.fixedClipRect());
        clipRects.setFixed(true);
    } else if (position == RelativePosition) {
        clipRects.setPosClipRect(clipRects.overflowClipRect());
    } else if (position == AbsolutePosition) {
        clipRects.setOverflowClipRect(clipRects.posClipRect());
    }
}

static void applyClipRects(const ClipRectsContext& context, LayoutObject& renderer, LayoutPoint offset, ClipRects& clipRects)
{
    ASSERT(renderer.hasOverflowClip() || renderer.hasClip());

    LayoutView* view = renderer.view();
    ASSERT(view);
    if (clipRects.fixed() && context.rootLayer->layoutObject() == view)
        offset -= view->frameView()->scrollOffsetForViewportConstrainedObjects();

    if (renderer.hasOverflowClip()) {
        ClipRect newOverflowClip = toLayoutBox(renderer).overflowClipRect(offset, context.scrollbarRelevancy);
        newOverflowClip.setHasRadius(renderer.style()->hasBorderRadius());
        clipRects.setOverflowClipRect(intersection(newOverflowClip, clipRects.overflowClipRect()));
        if (renderer.isPositioned())
            clipRects.setPosClipRect(intersection(newOverflowClip, clipRects.posClipRect()));
    }

    if (renderer.hasClip()) {
        LayoutRect newClip = toLayoutBox(renderer).clipRect(offset);
        clipRects.setPosClipRect(intersection(newClip, clipRects.posClipRect()));
        clipRects.setOverflowClipRect(intersection(newClip, clipRects.overflowClipRect()));
        clipRects.setFixedClipRect(intersection(newClip, clipRects.fixedClipRect()));
    }
}

DeprecatedPaintLayerClipper::DeprecatedPaintLayerClipper(LayoutBoxModelObject& renderer)
    : m_renderer(renderer)
{
}

ClipRects* DeprecatedPaintLayerClipper::clipRectsIfCached(const ClipRectsContext& context) const
{
    ASSERT(context.usesCache());
    if (!m_cache)
        return 0;
    ClipRectsCache::Entry& entry = m_cache->get(context.cacheSlot);
    // FIXME: We used to ASSERT that we always got a consistent root layer.
    // We should add a test that has an inconsistent root. See
    // http://crbug.com/366118 for an example.
    if (context.rootLayer != entry.root)
        return 0;
    ASSERT(entry.scrollbarRelevancy == context.scrollbarRelevancy);

#ifdef CHECK_CACHED_CLIP_RECTS
    // This code is useful to check cached clip rects, but is too expensive to leave enabled in debug builds by default.
    ClipRectsContext tempContext(context);
    tempContext.cacheSlot = UncachedClipRects;
    RefPtr<ClipRects> clipRects = ClipRects::create();
    calculateClipRects(tempContext, *clipRects);
    ASSERT(clipRects == *entry.clipRects);
#endif

    return entry.clipRects.get();
}

ClipRects* DeprecatedPaintLayerClipper::storeClipRectsInCache(const ClipRectsContext& context, ClipRects* parentClipRects, const ClipRects& clipRects) const
{
    ClipRectsCache::Entry& entry = cache().get(context.cacheSlot);
    entry.root = context.rootLayer;
#if ENABLE(ASSERT)
    entry.scrollbarRelevancy = context.scrollbarRelevancy;
#endif

    if (parentClipRects) {
        // If our clip rects match the clip rects of our parent, we share storage.
        if (clipRects == *parentClipRects) {
            entry.clipRects = parentClipRects;
            return parentClipRects;
        }
    }

    entry.clipRects = ClipRects::create(clipRects);
    return entry.clipRects.get();
}

ClipRects* DeprecatedPaintLayerClipper::getClipRects(const ClipRectsContext& context) const
{
    if (ClipRects* result = clipRectsIfCached(context))
        return result;

    // Note that it's important that we call getClipRects on our parent
    // before we call calculateClipRects so that calculateClipRects will hit
    // the cache.
    ClipRects* parentClipRects = 0;
    if (context.rootLayer != m_renderer.layer() && m_renderer.layer()->parent())
        parentClipRects = m_renderer.layer()->parent()->clipper().getClipRects(context);

    RefPtr<ClipRects> clipRects = ClipRects::create();
    calculateClipRects(context, *clipRects);
    return storeClipRectsInCache(context, parentClipRects, *clipRects);
}

void DeprecatedPaintLayerClipper::clearClipRectsIncludingDescendants()
{
    m_cache = nullptr;

    for (DeprecatedPaintLayer* layer = m_renderer.layer()->firstChild(); layer; layer = layer->nextSibling())
        layer->clipper().clearClipRectsIncludingDescendants();
}

void DeprecatedPaintLayerClipper::clearClipRectsIncludingDescendants(ClipRectsCacheSlot cacheSlot)
{
    if (m_cache)
        m_cache->clear(cacheSlot);

    for (DeprecatedPaintLayer* layer = m_renderer.layer()->firstChild(); layer; layer = layer->nextSibling())
        layer->clipper().clearClipRectsIncludingDescendants(cacheSlot);
}

LayoutRect DeprecatedPaintLayerClipper::childrenClipRect() const
{
    // FIXME: border-radius not accounted for.
    // FIXME: Regions not accounted for.
    DeprecatedPaintLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    // Need to use uncached clip rects, because the value of 'dontClipToOverflow' may be different from the painting path (<rdar://problem/11844909>).
    ClipRectsContext context(clippingRootLayer, UncachedClipRects);
    calculateRects(context, LayoutRect(m_renderer.view()->unscaledDocumentRect()), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return LayoutRect(clippingRootLayer->layoutObject()->localToAbsoluteQuad(FloatQuad(foregroundRect.rect())).enclosingBoundingBox());
}

LayoutRect DeprecatedPaintLayerClipper::localClipRect() const
{
    // FIXME: border-radius not accounted for.
    DeprecatedPaintLayer* clippingRootLayer = clippingRootForPainting();
    LayoutRect layerBounds;
    ClipRect backgroundRect, foregroundRect, outlineRect;
    ClipRectsContext context(clippingRootLayer, PaintingClipRects);
    calculateRects(context, LayoutRect(LayoutRect::infiniteIntRect()), layerBounds, backgroundRect, foregroundRect, outlineRect);

    LayoutRect clipRect = backgroundRect.rect();
    if (clipRect == LayoutRect::infiniteIntRect())
        return clipRect;

    LayoutPoint clippingRootOffset;
    m_renderer.layer()->convertToLayerCoords(clippingRootLayer, clippingRootOffset);
    clipRect.moveBy(-clippingRootOffset);

    return clipRect;
}

void DeprecatedPaintLayerClipper::calculateRects(const ClipRectsContext& context, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
    ClipRect& backgroundRect, ClipRect& foregroundRect, ClipRect& outlineRect, const LayoutPoint* offsetFromRoot) const
{
    bool isClippingRoot = m_renderer.layer() == context.rootLayer;

    if (!isClippingRoot && m_renderer.layer()->parent()) {
        backgroundRect = backgroundClipRect(context);
        backgroundRect.move(roundedIntSize(context.subPixelAccumulation));
        backgroundRect.intersect(paintDirtyRect);
    } else {
        backgroundRect = paintDirtyRect;
    }

    foregroundRect = backgroundRect;
    outlineRect = backgroundRect;

    LayoutPoint offset;
    if (offsetFromRoot)
        offset = *offsetFromRoot;
    else
        m_renderer.layer()->convertToLayerCoords(context.rootLayer, offset);
    layerBounds = LayoutRect(offset, LayoutSize(m_renderer.layer()->size()));

    // Update the clip rects that will be passed to child layers.
    if (m_renderer.hasOverflowClip()) {
        // This layer establishes a clip of some kind.
        if (!isClippingRoot || context.respectOverflowClip == RespectOverflowClip) {
            foregroundRect.intersect(toLayoutBox(m_renderer).overflowClipRect(offset, context.scrollbarRelevancy));
            if (m_renderer.style()->hasBorderRadius())
                foregroundRect.setHasRadius(true);
        }

        // If we establish an overflow clip at all, then go ahead and make sure our background
        // rect is intersected with our layer's bounds including our visual overflow,
        // since any visual overflow like box-shadow or border-outset is not clipped by overflow:auto/hidden.
        if (toLayoutBox(m_renderer).hasVisualOverflow()) {
            // FIXME: Perhaps we should be propagating the borderbox as the clip rect for children, even though
            //        we may need to inflate our clip specifically for shadows or outsets.
            // FIXME: Does not do the right thing with CSS regions yet, since we don't yet factor in the
            // individual region boxes as overflow.
            LayoutRect layerBoundsWithVisualOverflow = toLayoutBox(m_renderer).visualOverflowRect();
            toLayoutBox(m_renderer).flipForWritingMode(layerBoundsWithVisualOverflow); // DeprecatedPaintLayer are in physical coordinates, so the overflow has to be flipped.
            layerBoundsWithVisualOverflow.moveBy(offset);
            if (!isClippingRoot || context.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(layerBoundsWithVisualOverflow);
        } else {
            LayoutRect bounds = toLayoutBox(m_renderer).borderBoxRect();
            bounds.moveBy(offset);
            if (!isClippingRoot || context.respectOverflowClip == RespectOverflowClip)
                backgroundRect.intersect(bounds);
        }
    }

    // CSS clip (different than clipping due to overflow) can clip to any box, even if it falls outside of the border box.
    if (m_renderer.hasClip()) {
        // Clip applies to *us* as well, so go ahead and update the damageRect.
        LayoutRect newPosClip = toLayoutBox(m_renderer).clipRect(offset);
        backgroundRect.intersect(newPosClip);
        foregroundRect.intersect(newPosClip);
        outlineRect.intersect(newPosClip);
    }
}

void DeprecatedPaintLayerClipper::calculateClipRects(const ClipRectsContext& context, ClipRects& clipRects) const
{
    bool rootLayerScrolls = m_renderer.document().settings() && m_renderer.document().settings()->rootLayerScrolls();
    if (!m_renderer.layer()->parent() && !rootLayerScrolls) {
        // The root layer's clip rect is always infinite.
        clipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
        return;
    }

    bool isClippingRoot = m_renderer.layer() == context.rootLayer;

    // For transformed layers, the root layer was shifted to be us, so there is no need to
    // examine the parent. We want to cache clip rects with us as the root.
    DeprecatedPaintLayer* parentLayer = !isClippingRoot ? m_renderer.layer()->parent() : 0;

    // Ensure that our parent's clip has been calculated so that we can examine the values.
    if (parentLayer) {
        // FIXME: Why don't we just call getClipRects here?
        if (context.usesCache() && parentLayer->clipper().cachedClipRects(context)) {
            clipRects = *parentLayer->clipper().cachedClipRects(context);
        } else {
            parentLayer->clipper().calculateClipRects(context, clipRects);
        }
    } else {
        clipRects.reset(LayoutRect(LayoutRect::infiniteIntRect()));
    }

    adjustClipRectsForChildren(m_renderer, clipRects);

    if ((m_renderer.hasOverflowClip() && (context.respectOverflowClip == RespectOverflowClip || !isClippingRoot)) || m_renderer.hasClip()) {
        // This offset cannot use convertToLayerCoords, because sometimes our rootLayer may be across
        // some transformed layer boundary, for example, in the DeprecatedPaintLayerCompositor overlapMap, where
        // clipRects are needed in view space.
        applyClipRects(context, m_renderer, roundedLayoutPoint(m_renderer.localToContainerPoint(FloatPoint(), context.rootLayer->layoutObject())), clipRects);
    }
}

static ClipRect backgroundClipRectForPosition(const ClipRects& parentRects, EPosition position)
{
    if (position == FixedPosition)
        return parentRects.fixedClipRect();

    if (position == AbsolutePosition)
        return parentRects.posClipRect();

    return parentRects.overflowClipRect();
}

ClipRect DeprecatedPaintLayerClipper::backgroundClipRect(const ClipRectsContext& context) const
{
    ASSERT(m_renderer.layer()->parent());
    ASSERT(m_renderer.view());

    RefPtr<ClipRects> parentClipRects = ClipRects::create();
    if (m_renderer.layer() == context.rootLayer)
        parentClipRects->reset(LayoutRect(LayoutRect::infiniteIntRect()));
    else
        m_renderer.layer()->parent()->clipper().getOrCalculateClipRects(context, *parentClipRects);

    ClipRect result = backgroundClipRectForPosition(*parentClipRects, m_renderer.style()->position());

    // Note: infinite clipRects should not be scrolled here, otherwise they will accidentally no longer be considered infinite.
    if (parentClipRects->fixed() && context.rootLayer->layoutObject() == m_renderer.view() && result != LayoutRect(LayoutRect::infiniteIntRect()))
        result.move(m_renderer.view()->frameView()->scrollOffsetForViewportConstrainedObjects());

    return result;
}

void DeprecatedPaintLayerClipper::getOrCalculateClipRects(const ClipRectsContext& context, ClipRects& clipRects) const
{
    if (context.usesCache())
        clipRects = *getClipRects(context);
    else
        calculateClipRects(context, clipRects);
}

DeprecatedPaintLayer* DeprecatedPaintLayerClipper::clippingRootForPainting() const
{
    const DeprecatedPaintLayer* current = m_renderer.layer();
    // FIXME: getting rid of current->hasCompositedDeprecatedPaintLayerMapping() here breaks the
    // compositing/backing/no-backing-for-clip.html layout test, because there is a
    // "composited but paints into ancestor" layer involved. However, it doesn't make sense that
    // that check would be appropriate here but not inside the while loop below.
    if (current->isPaintInvalidationContainer() || current->hasCompositedDeprecatedPaintLayerMapping())
        return const_cast<DeprecatedPaintLayer*>(current);

    while (current) {
        if (current->isRootLayer())
            return const_cast<DeprecatedPaintLayer*>(current);

        current = current->compositingContainer();
        ASSERT(current);
        if (current->transform() || current->isPaintInvalidationContainer())
            return const_cast<DeprecatedPaintLayer*>(current);
    }

    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace blink
