/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrStrokeRectBatch_DEFINED
#define GrStrokeRectBatch_DEFINED

#include "GrColor.h"
#include "GrDefaultGeoProcFactory.h"
#include "GrVertexBatch.h"

class GrStrokeRectBatch : public GrVertexBatch {
public:
    struct Geometry {
        GrColor fColor;
        SkMatrix fViewMatrix;
        SkRect fRect;
        SkScalar fStrokeWidth;
    };

    static GrDrawBatch* Create(const Geometry& geometry, bool snapToPixelCenters) {
        return SkNEW_ARGS(GrStrokeRectBatch, (geometry, snapToPixelCenters));
    }

    const char* name() const override { return "GrStrokeRectBatch"; }

    void getInvariantOutputColor(GrInitInvariantOutput* out) const override {
        // When this is called on a batch, there is only one geometry bundle
        out->setKnownFourComponents(fGeoData[0].fColor);
    }

    void getInvariantOutputCoverage(GrInitInvariantOutput* out) const override {
        out->setKnownSingleComponent(0xff);
    }

private:
    void onPrepareDraws(Target*) override;
    void initBatchTracker(const GrPipelineOptimizations&) override;

    GrStrokeRectBatch(const Geometry& geometry, bool snapToPixelCenters);

    GrColor color() const { return fBatch.fColor; }
    bool usesLocalCoords() const { return fBatch.fUsesLocalCoords; }
    bool colorIgnored() const { return fBatch.fColorIgnored; }
    const SkMatrix& viewMatrix() const { return fGeoData[0].fViewMatrix; }
    bool hairline() const { return fBatch.fHairline; }
    bool coverageIgnored() const { return fBatch.fCoverageIgnored; }

    bool onCombineIfPossible(GrBatch* t, const GrCaps&) override {
        // if (!GrPipeline::CanCombine(*this->pipeline(), this->bounds(), *t->pipeline(),
        //     t->bounds(), caps)) {
        //     return false;
        // }
        // GrStrokeRectBatch* that = t->cast<StrokeRectBatch>();

        // NonAA stroke rects cannot batch right now
        // TODO make these batchable
        return false;
    }

    struct BatchTracker {
        GrColor fColor;
        bool fUsesLocalCoords;
        bool fColorIgnored;
        bool fCoverageIgnored;
        bool fHairline;
    };

    const static int kVertsPerHairlineRect = 5;
    const static int kVertsPerStrokeRect = 10;

    BatchTracker fBatch;
    SkSTArray<1, Geometry, true> fGeoData;
};

#endif
