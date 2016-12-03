/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkCanvas.h"
#include "SkSVGRenderContext.h"
#include "SkSVGPoly.h"
#include "SkSVGValue.h"

SkSVGPoly::SkSVGPoly(SkSVGTag t) : INHERITED(t) {}

void SkSVGPoly::setPoints(const SkSVGPointsType& pts) {
    fPath.reset();
    fPath.addPoly(pts.value().begin(),
                  pts.value().count(),
                  this->tag() == SkSVGTag::kPolygon); // only polygons are auto-closed
}

void SkSVGPoly::onSetAttribute(SkSVGAttribute attr, const SkSVGValue& v) {
    switch (attr) {
    case SkSVGAttribute::kPoints:
        if (const auto* pts = v.as<SkSVGPointsValue>()) {
            this->setPoints(*pts);
        }
        break;
    default:
        this->INHERITED::onSetAttribute(attr, v);
    }
}

void SkSVGPoly::onDraw(SkCanvas* canvas, const SkSVGLengthContext&, const SkPaint& paint) const {
    canvas->drawPath(fPath, paint);
}
