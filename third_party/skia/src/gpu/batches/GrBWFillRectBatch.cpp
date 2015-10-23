/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrBWFillRectBatch.h"

#include "GrBatchFlushState.h"
#include "GrColor.h"
#include "GrDefaultGeoProcFactory.h"
#include "GrPrimitiveProcessor.h"
#include "GrQuad.h"
#include "GrVertexBatch.h"

class GrBatchFlushState;
class SkMatrix;
struct SkRect;

class BWFillRectBatch : public GrVertexBatch {
public:
    struct Geometry {
        SkMatrix fViewMatrix;
        SkRect fRect;
        SkRect fLocalRect;
        SkMatrix fLocalMatrix;
        GrColor fColor;
        bool fHasLocalRect;
        bool fHasLocalMatrix;
    };

    static GrDrawBatch* Create(const Geometry& geometry) {
        return SkNEW_ARGS(BWFillRectBatch, (geometry));
    }

    const char* name() const override { return "RectBatch"; }

    void getInvariantOutputColor(GrInitInvariantOutput* out) const override {
        // When this is called on a batch, there is only one geometry bundle
        out->setKnownFourComponents(fGeoData[0].fColor);
    }

    void getInvariantOutputCoverage(GrInitInvariantOutput* out) const override {
        out->setKnownSingleComponent(0xff);
    }

    SkSTArray<1, Geometry, true>* geoData() { return &fGeoData; }

private:
    BWFillRectBatch(const Geometry& geometry) {
        this->initClassID<BWFillRectBatch>();
        fGeoData.push_back(geometry);

        fBounds = geometry.fRect;
        geometry.fViewMatrix.mapRect(&fBounds);
    }

    GrColor color() const { return fBatch.fColor; }
    bool usesLocalCoords() const { return fBatch.fUsesLocalCoords; }
    bool colorIgnored() const { return fBatch.fColorIgnored; }
    const SkMatrix& viewMatrix() const { return fGeoData[0].fViewMatrix; }
    const SkMatrix& localMatrix() const { return fGeoData[0].fLocalMatrix; }
    bool hasLocalRect() const { return fGeoData[0].fHasLocalRect; }
    bool hasLocalMatrix() const { return fGeoData[0].fHasLocalMatrix; }
    bool coverageIgnored() const { return fBatch.fCoverageIgnored; }

    void initBatchTracker(const GrPipelineOptimizations& init) override {
        // Handle any color overrides
        if (!init.readsColor()) {
            fGeoData[0].fColor = GrColor_ILLEGAL;
        }
        init.getOverrideColorIfSet(&fGeoData[0].fColor);

        // setup batch properties
        fBatch.fColorIgnored = !init.readsColor();
        fBatch.fColor = fGeoData[0].fColor;
        fBatch.fUsesLocalCoords = init.readsLocalCoords();
        fBatch.fCoverageIgnored = !init.readsCoverage();
    }

    void onPrepareDraws(Target* target) override {
        SkAutoTUnref<const GrGeometryProcessor> gp(this->createRectGP());
        if (!gp) {
            SkDebugf("Could not create GrGeometryProcessor\n");
            return;
        }

        target->initDraw(gp, this->pipeline());

        int instanceCount = fGeoData.count();
        size_t vertexStride = gp->getVertexStride();
        SkASSERT(this->hasLocalRect() ?
                 vertexStride == sizeof(GrDefaultGeoProcFactory::PositionColorLocalCoordAttr) :
                 vertexStride == sizeof(GrDefaultGeoProcFactory::PositionColorAttr));
        QuadHelper helper;
        void* vertices = helper.init(target, vertexStride, instanceCount);

        if (!vertices) {
            return;
        }

        for (int i = 0; i < instanceCount; i++) {
            const Geometry& geom = fGeoData[i];

            intptr_t offset = reinterpret_cast<intptr_t>(vertices) +
                              kVerticesPerQuad * i * vertexStride;
            SkPoint* positions = reinterpret_cast<SkPoint*>(offset);

            positions->setRectFan(geom.fRect.fLeft, geom.fRect.fTop,
                                  geom.fRect.fRight, geom.fRect.fBottom, vertexStride);
            geom.fViewMatrix.mapPointsWithStride(positions, vertexStride, kVerticesPerQuad);

            // TODO we should only do this if local coords are being read
            if (geom.fHasLocalRect) {
                static const int kLocalOffset = sizeof(SkPoint) + sizeof(GrColor);
                SkPoint* coords = reinterpret_cast<SkPoint*>(offset + kLocalOffset);
                coords->setRectFan(geom.fLocalRect.fLeft, geom.fLocalRect.fTop,
                                   geom.fLocalRect.fRight, geom.fLocalRect.fBottom,
                                   vertexStride);
                if (geom.fHasLocalMatrix) {
                    geom.fLocalMatrix.mapPointsWithStride(coords, vertexStride, kVerticesPerQuad);
                }
            }

            static const int kColorOffset = sizeof(SkPoint);
            GrColor* vertColor = reinterpret_cast<GrColor*>(offset + kColorOffset);
            for (int j = 0; j < 4; ++j) {
                *vertColor = geom.fColor;
                vertColor = (GrColor*) ((intptr_t) vertColor + vertexStride);
            }
        }

        helper.recordDraw(target);
    }

    bool onCombineIfPossible(GrBatch* t, const GrCaps& caps) override {
        BWFillRectBatch* that = t->cast<BWFillRectBatch>();
        if (!GrPipeline::CanCombine(*this->pipeline(), this->bounds(), *that->pipeline(),
                                    that->bounds(), caps)) {
            return false;
        }

        if (this->hasLocalRect() != that->hasLocalRect()) {
            return false;
        }

        SkASSERT(this->usesLocalCoords() == that->usesLocalCoords());
        if (!this->hasLocalRect() && this->usesLocalCoords()) {
            if (!this->viewMatrix().cheapEqualTo(that->viewMatrix())) {
                return false;
            }

            if (this->hasLocalMatrix() != that->hasLocalMatrix()) {
                return false;
            }

            if (this->hasLocalMatrix() && !this->localMatrix().cheapEqualTo(that->localMatrix())) {
                return false;
            }
        }

        if (this->color() != that->color()) {
            fBatch.fColor = GrColor_ILLEGAL;
        }
        fGeoData.push_back_n(that->geoData()->count(), that->geoData()->begin());
        this->joinBounds(that->bounds());
        return true;
    }


    /** We always use per-vertex colors so that rects can be batched across color changes. Sometimes
        we  have explicit local coords and sometimes not. We *could* always provide explicit local
        coords and just duplicate the positions when the caller hasn't provided a local coord rect,
        but we haven't seen a use case which frequently switches between local rect and no local
        rect draws.

        The color param is used to determine whether the opaque hint can be set on the draw state.
        The caller must populate the vertex colors itself.

        The vertex attrib order is always pos, color, [local coords].
     */
    const GrGeometryProcessor* createRectGP() const {
        using namespace GrDefaultGeoProcFactory;
        Color color(Color::kAttribute_Type);
        Coverage coverage(this->coverageIgnored() ? Coverage::kNone_Type : Coverage::kSolid_Type);

        // if we have a local rect, then we apply the localMatrix directly to the localRect to
        // generate vertex local coords
        if (this->hasLocalRect()) {
            LocalCoords localCoords(LocalCoords::kHasExplicit_Type);
            return GrDefaultGeoProcFactory::Create(color, coverage, localCoords, SkMatrix::I());
        } else {
            LocalCoords localCoords(LocalCoords::kUsePosition_Type,
                                    this->hasLocalMatrix() ? &this->localMatrix() : NULL);
            return GrDefaultGeoProcFactory::CreateForDeviceSpace(color, coverage, localCoords,
                                                                 this->viewMatrix());
        }
    }

    struct BatchTracker {
        GrColor fColor;
        bool fUsesLocalCoords;
        bool fColorIgnored;
        bool fCoverageIgnored;
    };

    BatchTracker fBatch;
    SkSTArray<1, Geometry, true> fGeoData;
};

namespace GrBWFillRectBatch {
GrDrawBatch* Create(GrColor color,
                    const SkMatrix& viewMatrix,
                    const SkRect& rect,
                    const SkRect* localRect,
                    const SkMatrix* localMatrix) {
    BWFillRectBatch::Geometry geometry;
    geometry.fColor = color;
    geometry.fViewMatrix = viewMatrix;
    geometry.fRect = rect;

    if (localRect) {
        geometry.fHasLocalRect = true;
        geometry.fLocalRect = *localRect;
    } else {
        geometry.fHasLocalRect = false;
    }

    if (localMatrix) {
        geometry.fHasLocalMatrix = true;
        geometry.fLocalMatrix = *localMatrix;
    } else {
        geometry.fHasLocalMatrix = false;
    }

    return BWFillRectBatch::Create(geometry);
}
};

///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef GR_TEST_UTILS

#include "GrBatchTest.h"

DRAW_BATCH_TEST_DEFINE(RectBatch) {
    BWFillRectBatch::Geometry geometry;
    geometry.fColor = GrRandomColor(random);

    geometry.fRect = GrTest::TestRect(random);
    geometry.fHasLocalRect = random->nextBool();

    if (geometry.fHasLocalRect) {
        geometry.fViewMatrix = GrTest::TestMatrixInvertible(random);
        geometry.fLocalRect = GrTest::TestRect(random);
    } else {
        geometry.fViewMatrix = GrTest::TestMatrix(random);
    }

    geometry.fHasLocalMatrix = random->nextBool();
    if (geometry.fHasLocalMatrix) {
        geometry.fLocalMatrix = GrTest::TestMatrix(random);
    }

    return BWFillRectBatch::Create(geometry);
}

#endif
