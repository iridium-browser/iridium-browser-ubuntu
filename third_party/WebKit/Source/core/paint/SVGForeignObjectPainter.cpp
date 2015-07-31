// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGForeignObjectPainter.h"

#include "core/layout/svg/LayoutSVGForeignObject.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"

namespace blink {

void SVGForeignObjectPainter::paint(const PaintInfo& paintInfo)
{
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    TransformRecorder transformRecorder(*paintInfoBeforeFiltering.context, m_layoutSVGForeignObject, m_layoutSVGForeignObject.localTransform());

    // When transitioning from SVG to block painters we need to keep the PaintInfo rect up-to-date
    // because it can be used for clipping.
    m_layoutSVGForeignObject.updatePaintInfoRect(paintInfoBeforeFiltering.rect);

    OwnPtr<FloatClipRecorder> clipRecorder;
    if (SVGLayoutSupport::isOverflowHidden(&m_layoutSVGForeignObject))
        clipRecorder = adoptPtr(new FloatClipRecorder(*paintInfoBeforeFiltering.context, m_layoutSVGForeignObject, paintInfoBeforeFiltering.phase, m_layoutSVGForeignObject.viewportRect()));

    SVGPaintContext paintContext(m_layoutSVGForeignObject, paintInfoBeforeFiltering);
    bool continueRendering = true;
    if (paintContext.paintInfo().phase == PaintPhaseForeground)
        continueRendering = paintContext.applyClipMaskAndFilterIfNecessary();

    if (continueRendering) {
        // Paint all phases of FO elements atomically as though the FO element established its own stacking context.
        bool preservePhase = paintContext.paintInfo().phase == PaintPhaseSelection || paintContext.paintInfo().phase == PaintPhaseTextClip;
        const LayoutPoint childPoint = IntPoint();
        paintContext.paintInfo().phase = preservePhase ? paintContext.paintInfo().phase : PaintPhaseBlockBackground;
        BlockPainter(m_layoutSVGForeignObject).paint(paintContext.paintInfo(), childPoint);
        if (!preservePhase) {
            paintContext.paintInfo().phase = PaintPhaseChildBlockBackgrounds;
            BlockPainter(m_layoutSVGForeignObject).paint(paintContext.paintInfo(), childPoint);
            paintContext.paintInfo().phase = PaintPhaseFloat;
            BlockPainter(m_layoutSVGForeignObject).paint(paintContext.paintInfo(), childPoint);
            paintContext.paintInfo().phase = PaintPhaseForeground;
            BlockPainter(m_layoutSVGForeignObject).paint(paintContext.paintInfo(), childPoint);
            paintContext.paintInfo().phase = PaintPhaseOutline;
            BlockPainter(m_layoutSVGForeignObject).paint(paintContext.paintInfo(), childPoint);
        }
    }
}

} // namespace blink
