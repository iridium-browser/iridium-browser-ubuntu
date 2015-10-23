
/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "GrStencilAndCoverPathRenderer.h"
#include "GrCaps.h"
#include "GrContext.h"
#include "GrGpu.h"
#include "GrPath.h"
#include "GrRenderTarget.h"
#include "GrResourceProvider.h"
#include "GrStrokeInfo.h"

/*
 * For now paths only natively support winding and even odd fill types
 */
static GrPathRendering::FillType convert_skpath_filltype(SkPath::FillType fill) {
    switch (fill) {
        default:
            SkFAIL("Incomplete Switch\n");
        case SkPath::kWinding_FillType:
        case SkPath::kInverseWinding_FillType:
            return GrPathRendering::kWinding_FillType;
        case SkPath::kEvenOdd_FillType:
        case SkPath::kInverseEvenOdd_FillType:
            return GrPathRendering::kEvenOdd_FillType;
    }
}

GrPathRenderer* GrStencilAndCoverPathRenderer::Create(GrResourceProvider* resourceProvider,
                                                      const GrCaps& caps) {
    if (caps.shaderCaps()->pathRenderingSupport()) {
        return SkNEW_ARGS(GrStencilAndCoverPathRenderer, (resourceProvider));
    } else {
        return NULL;
    }
}

GrStencilAndCoverPathRenderer::GrStencilAndCoverPathRenderer(GrResourceProvider* resourceProvider)
    : fResourceProvider(resourceProvider) {    
}

bool GrStencilAndCoverPathRenderer::onCanDrawPath(const CanDrawPathArgs& args) const {
    if (args.fStroke->isHairlineStyle()) {
        return false;
    }
    if (!args.fPipelineBuilder->getStencil().isDisabled()) {
        return false;
    }
    if (args.fAntiAlias) {
        return args.fPipelineBuilder->getRenderTarget()->isStencilBufferMultisampled();
    } else {
        return true; // doesn't do per-path AA, relies on the target having MSAA
    }
}

static GrPath* get_gr_path(GrResourceProvider* resourceProvider, const SkPath& skPath,
                           const GrStrokeInfo& stroke) {
    GrUniqueKey key;
    bool isVolatile;
    GrPath::ComputeKey(skPath, stroke, &key, &isVolatile);
    SkAutoTUnref<GrPath> path(
        static_cast<GrPath*>(resourceProvider->findAndRefResourceByUniqueKey(key)));
    if (!path) {
        path.reset(resourceProvider->createPath(skPath, stroke));
        if (!isVolatile) {
            resourceProvider->assignUniqueKeyToResource(key, path);
        }
    } else {
        SkASSERT(path->isEqualTo(skPath, stroke));
    }
    return path.detach();
}

void GrStencilAndCoverPathRenderer::onStencilPath(const StencilPathArgs& args) {
    SkASSERT(!args.fPath->isInverseFillType());
    SkAutoTUnref<GrPathProcessor> pp(GrPathProcessor::Create(GrColor_WHITE, *args.fViewMatrix));
    SkAutoTUnref<GrPath> p(get_gr_path(fResourceProvider, *args.fPath, *args.fStroke));
    args.fTarget->stencilPath(*args.fPipelineBuilder, pp, p,
                              convert_skpath_filltype(args.fPath->getFillType()));
}

bool GrStencilAndCoverPathRenderer::onDrawPath(const DrawPathArgs& args) {
    SkASSERT(!args.fStroke->isHairlineStyle());
    const SkPath& path = *args.fPath;
    GrPipelineBuilder* pipelineBuilder = args.fPipelineBuilder;
    const SkMatrix& viewMatrix = *args.fViewMatrix;

    SkASSERT(pipelineBuilder->getStencil().isDisabled());

    if (args.fAntiAlias) {
        SkASSERT(pipelineBuilder->getRenderTarget()->isStencilBufferMultisampled());
        pipelineBuilder->enableState(GrPipelineBuilder::kHWAntialias_Flag);
    }

    SkAutoTUnref<GrPath> p(get_gr_path(fResourceProvider, path, *args.fStroke));

    if (path.isInverseFillType()) {
        GR_STATIC_CONST_SAME_STENCIL(kInvertedStencilPass,
            kZero_StencilOp,
            kZero_StencilOp,
            // We know our rect will hit pixels outside the clip and the user bits will be 0
            // outside the clip. So we can't just fill where the user bits are 0. We also need to
            // check that the clip bit is set.
            kEqualIfInClip_StencilFunc,
            0xffff,
            0x0000,
            0xffff);

        pipelineBuilder->setStencil(kInvertedStencilPass);

        // fake inverse with a stencil and cover
        SkAutoTUnref<GrPathProcessor> pp(GrPathProcessor::Create(GrColor_WHITE, viewMatrix));
        args.fTarget->stencilPath(*pipelineBuilder, pp, p,
                                  convert_skpath_filltype(path.getFillType()));

        SkMatrix invert = SkMatrix::I();
        SkRect bounds =
            SkRect::MakeLTRB(0, 0, SkIntToScalar(pipelineBuilder->getRenderTarget()->width()),
                             SkIntToScalar(pipelineBuilder->getRenderTarget()->height()));
        SkMatrix vmi;
        // mapRect through persp matrix may not be correct
        if (!viewMatrix.hasPerspective() && viewMatrix.invert(&vmi)) {
            vmi.mapRect(&bounds);
            // theoretically could set bloat = 0, instead leave it because of matrix inversion
            // precision.
            SkScalar bloat = viewMatrix.getMaxScale() * SK_ScalarHalf;
            bounds.outset(bloat, bloat);
        } else {
            if (!viewMatrix.invert(&invert)) {
                return false;
            }
        }
        const SkMatrix& viewM = viewMatrix.hasPerspective() ? SkMatrix::I() : viewMatrix;
        args.fTarget->drawBWRect(*pipelineBuilder, args.fColor, viewM, bounds, NULL, &invert);
    } else {
        GR_STATIC_CONST_SAME_STENCIL(kStencilPass,
            kZero_StencilOp,
            kZero_StencilOp,
            kNotEqual_StencilFunc,
            0xffff,
            0x0000,
            0xffff);

        pipelineBuilder->setStencil(kStencilPass);
        SkAutoTUnref<GrPathProcessor> pp(GrPathProcessor::Create(args.fColor, viewMatrix));
        args.fTarget->drawPath(*pipelineBuilder, pp, p,
                               convert_skpath_filltype(path.getFillType()));
    }

    pipelineBuilder->stencil()->setDisabled();
    return true;
}
