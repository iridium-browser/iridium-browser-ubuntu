/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkCanvas.h"
#include "SkMatrix.h"
#include "SkSVGNode.h"
#include "SkSVGRenderContext.h"
#include "SkSVGValue.h"
#include "SkTLazy.h"

SkSVGNode::SkSVGNode(SkSVGTag t) : fTag(t) { }

SkSVGNode::~SkSVGNode() { }

void SkSVGNode::render(const SkSVGRenderContext& ctx) const {
    SkSVGRenderContext localContext(ctx);

    if (this->onPrepareToRender(&localContext)) {
        this->onRender(localContext);
    }
}

bool SkSVGNode::onPrepareToRender(SkSVGRenderContext* ctx) const {
    ctx->applyPresentationAttributes(fPresentationAttributes);
    return true;
}

void SkSVGNode::setAttribute(SkSVGAttribute attr, const SkSVGValue& v) {
    this->onSetAttribute(attr, v);
}

void SkSVGNode::setFill(const SkSVGPaint& svgPaint) {
    fPresentationAttributes.fFill.set(svgPaint);
}

void SkSVGNode::setFillOpacity(const SkSVGNumberType& opacity) {
    fPresentationAttributes.fFillOpacity.set(
        SkSVGNumberType(SkTPin<SkScalar>(opacity.value(), 0, 1)));
}

void SkSVGNode::setOpacity(const SkSVGNumberType& opacity) {
    fPresentationAttributes.fOpacity.set(
        SkSVGNumberType(SkTPin<SkScalar>(opacity.value(), 0, 1)));
}

void SkSVGNode::setStroke(const SkSVGPaint& svgPaint) {
    fPresentationAttributes.fStroke.set(svgPaint);
}

void SkSVGNode::setStrokeOpacity(const SkSVGNumberType& opacity) {
    fPresentationAttributes.fStrokeOpacity.set(
        SkSVGNumberType(SkTPin<SkScalar>(opacity.value(), 0, 1)));
}

void SkSVGNode::setStrokeWidth(const SkSVGLength& strokeWidth) {
    fPresentationAttributes.fStrokeWidth.set(strokeWidth);
}

void SkSVGNode::onSetAttribute(SkSVGAttribute attr, const SkSVGValue& v) {
    switch (attr) {
    case SkSVGAttribute::kFill:
        if (const SkSVGPaintValue* paint = v.as<SkSVGPaintValue>()) {
            this->setFill(*paint);
        }
        break;
    case SkSVGAttribute::kFillOpacity:
        if (const SkSVGNumberValue* opacity = v.as<SkSVGNumberValue>()) {
            this->setFillOpacity(*opacity);
        }
        break;
    case SkSVGAttribute::kOpacity:
        if (const SkSVGNumberValue* opacity = v.as<SkSVGNumberValue>()) {
            this->setOpacity(*opacity);
        }
        break;
    case SkSVGAttribute::kStroke:
        if (const SkSVGPaintValue* paint = v.as<SkSVGPaintValue>()) {
            this->setStroke(*paint);
        }
        break;
    case SkSVGAttribute::kStrokeOpacity:
        if (const SkSVGNumberValue* opacity = v.as<SkSVGNumberValue>()) {
            this->setStrokeOpacity(*opacity);
        }
        break;
    case SkSVGAttribute::kStrokeLineCap:
        if (const SkSVGLineCapValue* lineCap = v.as<SkSVGLineCapValue>()) {
            fPresentationAttributes.fStrokeLineCap.set(*lineCap);
        }
        break;
    case SkSVGAttribute::kStrokeLineJoin:
        if (const SkSVGLineJoinValue* lineJoin = v.as<SkSVGLineJoinValue>()) {
            fPresentationAttributes.fStrokeLineJoin.set(*lineJoin);
        }
        break;
    case SkSVGAttribute::kStrokeWidth:
        if (const SkSVGLengthValue* strokeWidth = v.as<SkSVGLengthValue>()) {
            this->setStrokeWidth(*strokeWidth);
        }
        break;
    default:
        SkDebugf("attribute ID <%d> ignored for node <%d>\n", attr, fTag);
        break;
    }
}
