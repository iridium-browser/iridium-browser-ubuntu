/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrInOrderCommandBuilder.h"

#include "GrBufferedDrawTarget.h"

#include "GrColor.h"
#include "SkPoint.h"

static bool path_fill_type_is_winding(const GrStencilSettings& pathStencilSettings) {
    static const GrStencilSettings::Face pathFace = GrStencilSettings::kFront_Face;
    bool isWinding = kInvert_StencilOp != pathStencilSettings.passOp(pathFace);
    if (isWinding) {
        // Double check that it is in fact winding.
        SkASSERT(kIncClamp_StencilOp == pathStencilSettings.passOp(pathFace));
        SkASSERT(kIncClamp_StencilOp == pathStencilSettings.failOp(pathFace));
        SkASSERT(0x1 != pathStencilSettings.writeMask(pathFace));
        SkASSERT(!pathStencilSettings.isTwoSided());
    }
    return isWinding;
}

GrTargetCommands::Cmd* GrInOrderCommandBuilder::recordDrawBatch(GrBatch* batch,
                                                                const GrCaps& caps) {
    GrBATCH_INFO("In-Recording (%s, %u)\n", batch->name(), batch->uniqueID());
    if (!this->cmdBuffer()->empty() &&
        Cmd::kDrawBatch_CmdType == this->cmdBuffer()->back().type()) {
        DrawBatch* previous = static_cast<DrawBatch*>(&this->cmdBuffer()->back());
        if (previous->batch()->combineIfPossible(batch, caps)) {
            GrBATCH_INFO("\tBatching with (%s, %u)\n",
                         previous->batch()->name(), previous->batch()->uniqueID());
            return NULL;
        }
    }

    return GrNEW_APPEND_TO_RECORDER(*this->cmdBuffer(), DrawBatch, (batch));
}

GrTargetCommands::Cmd*
GrInOrderCommandBuilder::recordDrawPaths(State* state,
                                         GrBufferedDrawTarget* bufferedDrawTarget,
                                         const GrPathProcessor* pathProc,
                                         const GrPathRange* pathRange,
                                         const void* indexValues,
                                         GrDrawTarget::PathIndexType indexType,
                                         const float transformValues[],
                                         GrDrawTarget::PathTransformType transformType,
                                         int count,
                                         const GrStencilSettings& stencilSettings,
                                         const GrPipelineOptimizations& opts) {
    SkASSERT(pathRange);
    SkASSERT(indexValues);
    SkASSERT(transformValues);

    char* savedIndices;
    float* savedTransforms;

    bufferedDrawTarget->appendIndicesAndTransforms(indexValues, indexType,
                                                   transformValues, transformType,
                                                   count, &savedIndices, &savedTransforms);

    if (!this->cmdBuffer()->empty() &&
        Cmd::kDrawPaths_CmdType == this->cmdBuffer()->back().type()) {
        // Try to combine this call with the previous DrawPaths. We do this by stenciling all the
        // paths together and then covering them in a single pass. This is not equivalent to two
        // separate draw calls, so we can only do it if there is no blending (no overlap would also
        // work). Note that it's also possible for overlapping paths to cancel each other's winding
        // numbers, and we only partially account for this by not allowing even/odd paths to be
        // combined. (Glyphs in the same font tend to wind the same direction so it works out OK.)
        DrawPaths* previous = static_cast<DrawPaths*>(&this->cmdBuffer()->back());
        if (pathRange == previous->pathRange() &&
            indexType == previous->fIndexType &&
            transformType == previous->fTransformType &&
            stencilSettings == previous->fStencilSettings &&
            path_fill_type_is_winding(stencilSettings) &&
            previous->fState == state &&
            !opts.willColorBlendWithDst()) {

            const int indexBytes = GrPathRange::PathIndexSizeInBytes(indexType);
            const int xformSize = GrPathRendering::PathTransformSize(transformType);
            if (&previous->fIndices[previous->fCount * indexBytes] == savedIndices &&
                (0 == xformSize ||
                 &previous->fTransforms[previous->fCount * xformSize] == savedTransforms)) {
                // Combine this DrawPaths call with the one previous.
                previous->fCount += count;
                return NULL;
            }
        }
    }

    DrawPaths* dp = GrNEW_APPEND_TO_RECORDER(*this->cmdBuffer(), DrawPaths, (state, pathRange));
    dp->fIndices = savedIndices;
    dp->fIndexType = indexType;
    dp->fTransforms = savedTransforms;
    dp->fTransformType = transformType;
    dp->fCount = count;
    dp->fStencilSettings = stencilSettings;
    return dp;
}
