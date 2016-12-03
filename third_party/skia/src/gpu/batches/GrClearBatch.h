/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrClearBatch_DEFINED
#define GrClearBatch_DEFINED

#include "GrBatch.h"
#include "GrBatchFlushState.h"
#include "GrGpu.h"
#include "GrGpuCommandBuffer.h"
#include "GrRenderTarget.h"

class GrClearBatch final : public GrBatch {
public:
    DEFINE_BATCH_CLASS_ID

    static sk_sp<GrClearBatch> Make(const SkIRect& rect,  GrColor color, GrRenderTarget* rt) {
        return sk_sp<GrClearBatch>(new GrClearBatch(rect, color, rt));
    }

    const char* name() const override { return "Clear"; }

    uint32_t renderTargetUniqueID() const override { return fRenderTarget.get()->getUniqueID(); }
    GrRenderTarget* renderTarget() const override { return fRenderTarget.get(); }

    SkString dumpInfo() const override {
        SkString string;
        string.printf("Color: 0x%08x, Rect [L: %d, T: %d, R: %d, B: %d], RT: %d",
                      fColor, fRect.fLeft, fRect.fTop, fRect.fRight, fRect.fBottom,
                      fRenderTarget.get()->getUniqueID());
        string.append(INHERITED::dumpInfo());
        return string;
    }

    void setColor(GrColor color) { fColor = color; }

private:
    GrClearBatch(const SkIRect& rect,  GrColor color, GrRenderTarget* rt)
        : INHERITED(ClassID())
        , fRect(rect)
        , fColor(color)
        , fRenderTarget(rt) {
        this->setBounds(SkRect::Make(rect), HasAABloat::kNo, IsZeroArea::kNo);
    }

    bool onCombineIfPossible(GrBatch* t, const GrCaps& caps) override {
        // This could be much more complicated. Currently we look at cases where the new clear
        // contains the old clear, or when the new clear is a subset of the old clear and is the
        // same color.
        GrClearBatch* cb = t->cast<GrClearBatch>();
        SkASSERT(cb->fRenderTarget == fRenderTarget);
        if (cb->fRect.contains(fRect)) {
            fRect = cb->fRect;
            this->replaceBounds(*t);
            fColor = cb->fColor;
            return true;
        } else if (cb->fColor == fColor && fRect.contains(cb->fRect)) {
            return true;
        }
        return false;
    }

    void onPrepare(GrBatchFlushState*) override {}

    void onDraw(GrBatchFlushState* state) override {
        state->commandBuffer()->clear(fRect, fColor, fRenderTarget.get());
    }

    SkIRect                                                 fRect;
    GrColor                                                 fColor;
    GrPendingIOResource<GrRenderTarget, kWrite_GrIOType>    fRenderTarget;

    typedef GrBatch INHERITED;
};

#endif
