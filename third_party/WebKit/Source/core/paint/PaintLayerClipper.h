/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
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

#ifndef PaintLayerClipper_h
#define PaintLayerClipper_h

#include "core/CoreExport.h"
#include "core/layout/ClipRectsCache.h"

#include "platform/graphics/paint/GeometryMapper.h"
#include "platform/scroll/ScrollTypes.h"

#include "wtf/Allocator.h"

namespace blink {

class PaintLayer;

enum ShouldRespectOverflowClipType { IgnoreOverflowClip, RespectOverflowClip };

class ClipRectsContext {
  STACK_ALLOCATED();

 public:
  ClipRectsContext(const PaintLayer* root,
                   ClipRectsCacheSlot slot,
                   OverlayScrollbarClipBehavior overlayScrollbarClipBehavior =
                       IgnoreOverlayScrollbarSize,
                   const LayoutSize& accumulation = LayoutSize())
      : rootLayer(root),
        overlayScrollbarClipBehavior(overlayScrollbarClipBehavior),
        m_cacheSlot(slot),
        subPixelAccumulation(accumulation),
        respectOverflowClip(slot == PaintingClipRectsIgnoringOverflowClip
                                ? IgnoreOverflowClip
                                : RespectOverflowClip),
        respectOverflowClipForViewport(
            slot == RootRelativeClipRectsIgnoringViewportClip
                ? IgnoreOverflowClip
                : RespectOverflowClip) {}

  void setIgnoreOverflowClip() {
    DCHECK(!usesCache() || m_cacheSlot == PaintingClipRects);
    DCHECK(respectOverflowClip == RespectOverflowClip);
    if (usesCache())
      m_cacheSlot = PaintingClipRectsIgnoringOverflowClip;
    respectOverflowClip = IgnoreOverflowClip;
  }

  bool usesCache() const { return m_cacheSlot != UncachedClipRects; }

  ClipRectsCacheSlot cacheSlot() const { return m_cacheSlot; }

  const PaintLayer* rootLayer;
  const OverlayScrollbarClipBehavior overlayScrollbarClipBehavior;

 private:
  friend class PaintLayerClipper;

  ClipRectsCacheSlot m_cacheSlot;
  LayoutSize subPixelAccumulation;
  ShouldRespectOverflowClipType respectOverflowClip;
  ShouldRespectOverflowClipType respectOverflowClipForViewport;
};

// PaintLayerClipper is responsible for computing and caching clip
// rects.
//
// These clip rects have two types: background and foreground.
//
// The "background rect" for a PaintLayer is almost the same as its visual
// rect in the space of some ancestor PaintLayer (specified by rootLayer on
// ClipRectsContext).
// The only differences are that:
//   * The unclipped rect at the start is LayoutRect::infiniteIntRect,
// rather than the local overflow bounds of the PaintLayer.
//   * CSS clip, the extent of visualOverflowRect(), and SVG root viewport
// clipping is applied.
// Thus, for example if there are no clips then the background rect will be
// infinite. Also, whether overflow clip of the ancestor should be applied is a
// parameter.
//
// The "foreground rect" for a PaintLayer is its "background rect", intersected
// with any clip applied by this PaintLayer to its children.

// Motivation for this class:
//
// The main reason for this cache is that we compute the clip rects during
// a layout tree walk but need them during a paint tree walk (see example
// below for some explanations).
//
// A lot of complexity in this class come from the difference in inheritance
// between 'overflow' and 'clip':
// * 'overflow' applies based on the containing blocks chain.
//    (http://www.w3.org/TR/CSS2/visufx.html#propdef-overflow)
// * 'clip' applies to all descendants.
//    (http://www.w3.org/TR/CSS2/visufx.html#propdef-clip)
//
// Let's take an example:
// <!DOCTYPE html>
// <div id="container" style="position: absolute; height: 100px; width: 100px">
//   <div id="inflow" style="height: 200px; width: 200px;
//       background-color: purple"></div>
//   <div id="fixed" style="height: 200px; width: 200px; position: fixed;
//       background-color: orange"></div>
// </div>
//
// The paint tree looks like:
//               html
//              /   |
//             /    |
//            /     |
//      container  fixed
//         |
//         |
//       inflow
//
// If we add "overflow: hidden" to #container, the overflow clip will apply to
// #inflow but not to #fixed. That's because #fixed's containing block is above
// #container and thus 'overflow' doesn't apply to it. During our tree walk,
// #fixed is a child of #container, which is the reason why we keep 3 clip rects
// depending on the 'position' of the elements.
//
// Now instead if we add "clip: rect(0px, 100px, 100px, 0px)" to #container,
// the clip will apply to both #inflow and #fixed. That's because 'clip'
// applies to any descendant, regardless of containing blocks. Note that
// #container and #fixed are siblings in the paint tree but #container does
// clip #fixed. This is the reason why we compute the painting clip rects during
// a layout tree walk and cache them for painting.
class CORE_EXPORT PaintLayerClipper {
  DISALLOW_NEW();

 public:
  explicit PaintLayerClipper(const PaintLayer&, bool useGeometryMapper);

  void clearClipRectsIncludingDescendants();
  void clearClipRectsIncludingDescendants(ClipRectsCacheSlot);

  // Returns the background clip rect of the layer in the local coordinate
  // space. Only looks for clips up to the given ancestor.
  LayoutRect localClipRect(const PaintLayer* ancestorLayer) const;

  // Computes the same thing as backgroundRect in calculateRects(), but skips
  // applying CSS clip and the visualOverflowRect() of |m_layer|.
  ClipRect backgroundClipRect(const ClipRectsContext&) const;

  // This method figures out our layerBounds in coordinates relative to
  // |rootLayer|. It also computes our background and foreground clip rects
  // for painting/event handling. Pass offsetFromRoot if known.
  void calculateRects(const ClipRectsContext&,
                      const LayoutRect& paintDirtyRect,
                      LayoutRect& layerBounds,
                      ClipRect& backgroundRect,
                      ClipRect& foregroundRect,
                      const LayoutPoint* offsetFromRoot = 0) const;

  ClipRects& paintingClipRects(const PaintLayer* rootLayer,
                               ShouldRespectOverflowClipType,
                               const LayoutSize& subpixelAccumulation) const;

 private:
  void clearCache(ClipRectsCacheSlot);
  ClipRects& getClipRects(const ClipRectsContext&) const;

  void calculateClipRects(const ClipRectsContext&, ClipRects&) const;
  ClipRects* clipRectsIfCached(const ClipRectsContext&) const;
  ClipRects& storeClipRectsInCache(const ClipRectsContext&,
                                   ClipRects* parentClipRects,
                                   const ClipRects&) const;

  void getOrCalculateClipRects(const ClipRectsContext&, ClipRects&) const;

  bool shouldClipOverflow(const ClipRectsContext&) const;
  bool shouldRespectOverflowClip(const ClipRectsContext&) const;

  // Returned clip rect is in the space of the context's rootLayer.
  ClipRect clipRectWithGeometryMapper(const ClipRectsContext&,
                                      bool isForeground) const;
  // Mutates the given rect into a rect in the space of the context's
  // rootLayer.
  void mapLocalToRootWithGeometryMapper(const ClipRectsContext&,
                                        LayoutRect&) const;
  // Same as calculateRects, but using GeometryMapper.
  void calculateRectsWithGeometryMapper(
      const ClipRectsContext&,
      const LayoutRect& paintDirtyRect,
      LayoutRect& layerBounds,
      ClipRect& backgroundRect,
      ClipRect& foregroundRect,
      const LayoutPoint* offsetFromRoot = 0) const;

  ClipRect applyOverflowClipToBackgroundRectWithGeometryMapper(
      const ClipRectsContext&,
      const ClipRect&) const;

  const PaintLayer& m_layer;
  std::unique_ptr<GeometryMapper> m_geometryMapper;

  friend class PaintLayerClipperTest;
};

}  // namespace blink

#endif  // LayerClipper_h
