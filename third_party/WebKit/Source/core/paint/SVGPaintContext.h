/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 Zoltan Herczeg <zherczeg@webkit.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGPaintContext_h
#define SVGPaintContext_h

#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "core/paint/ClipPathClipper.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGFilterPainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/transforms/AffineTransform.h"
#include <memory>

namespace blink {

class LayoutObject;
class LayoutSVGResourceFilter;
class LayoutSVGResourceMasker;
class SVGResources;

// This class hooks up the correct paint property transform node when spv2 is
// enabled, and otherwise works like a TransformRecorder which emits Transform
// display items for spv1.
class SVGTransformContext : public TransformRecorder {
  STACK_ALLOCATED();

 public:
  SVGTransformContext(GraphicsContext& context,
                      const LayoutObject& object,
                      const AffineTransform& transform)
      : TransformRecorder(context, object, transform) {
    if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
      const auto* objectProperties = object.paintProperties();
      if (!objectProperties)
        return;
      if (object.isSVGRoot()) {
        // If a transform exists, we can rely on a layer existing to apply it.
        DCHECK(!objectProperties || !objectProperties->transform() ||
               object.hasLayer());
        if (objectProperties->svgLocalToBorderBoxTransform()) {
          DCHECK(objectProperties->svgLocalToBorderBoxTransform()->matrix() ==
                 transform.toTransformationMatrix());
          auto& paintController = context.getPaintController();
          PaintChunkProperties properties(
              paintController.currentPaintChunkProperties());
          properties.propertyTreeState.setTransform(
              objectProperties->svgLocalToBorderBoxTransform());
          m_transformPropertyScope.emplace(paintController, object, properties);
        }
      } else {
        DCHECK(object.isSVG());
        // Should only be used by LayoutSVGRoot.
        DCHECK(!objectProperties->svgLocalToBorderBoxTransform());

        if (objectProperties->transform()) {
          DCHECK(objectProperties->transform()->matrix() ==
                 transform.toTransformationMatrix());
          auto& paintController = context.getPaintController();
          PaintChunkProperties properties(
              paintController.currentPaintChunkProperties());
          properties.propertyTreeState.setTransform(
              objectProperties->transform());
          m_transformPropertyScope.emplace(paintController, object, properties);
        }
      }
    }
  }

 private:
  Optional<ScopedPaintChunkProperties> m_transformPropertyScope;
};

class SVGPaintContext {
  STACK_ALLOCATED();

 public:
  SVGPaintContext(const LayoutObject& object, const PaintInfo& paintInfo)
      : m_object(object),
        m_paintInfo(paintInfo),
        m_filter(nullptr),
        m_masker(nullptr)
  {
  }

  ~SVGPaintContext();

  PaintInfo& paintInfo() {
    return m_filterPaintInfo ? *m_filterPaintInfo : m_paintInfo;
  }

  // Return true if these operations aren't necessary or if they are
  // successfully applied.
  bool applyClipMaskAndFilterIfNecessary();

  static void paintSubtree(GraphicsContext&, const LayoutObject*);

  // TODO(fs): This functions feels a bit misplaced (we don't want this to
  // turn into the new kitchen sink). Move it if a better location surfaces.
  static bool paintForLayoutObject(
      const PaintInfo&,
      const ComputedStyle&,
      const LayoutObject&,
      LayoutSVGResourceMode,
      SkPaint&,
      const AffineTransform* additionalPaintServerTransform = nullptr);

 private:
  void applyCompositingIfNecessary();
  void applyPaintPropertyState();
  void applyClipIfNecessary();

  // Return true if no masking is necessary or if the mask is successfully
  // applied.
  bool applyMaskIfNecessary(SVGResources*);

  // Return true if no filtering is necessary or if the filter is successfully
  // applied.
  bool applyFilterIfNecessary(SVGResources*);

  bool isIsolationInstalled() const;

  const LayoutObject& m_object;
  PaintInfo m_paintInfo;
  std::unique_ptr<PaintInfo> m_filterPaintInfo;
  LayoutSVGResourceFilter* m_filter;
  LayoutSVGResourceMasker* m_masker;
  std::unique_ptr<CompositingRecorder> m_compositingRecorder;
  Optional<ClipPathClipper> m_clipPathClipper;
  std::unique_ptr<SVGFilterRecordingContext> m_filterRecordingContext;
  Optional<ScopedPaintChunkProperties> m_scopedPaintChunkProperties;
#if DCHECK_IS_ON()
  bool m_applyClipMaskAndFilterIfNecessaryCalled = false;
#endif
};

}  // namespace blink

#endif  // SVGPaintContext_h
