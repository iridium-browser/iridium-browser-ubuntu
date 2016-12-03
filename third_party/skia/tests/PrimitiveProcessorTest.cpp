/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This is a GPU-backend specific test. It relies on static intializers to work

#include "SkTypes.h"
#include "Test.h"

#if SK_SUPPORT_GPU
#include "GrBatchFlushState.h"
#include "GrDrawContext.h"
#include "GrDrawContextPriv.h"
#include "GrContext.h"
#include "GrGeometryProcessor.h"
#include "GrGpu.h"
#include "GrTextureProvider.h"
#include "glsl/GrGLSLGeometryProcessor.h"
#include "glsl/GrGLSLVarying.h"
#include "batches/GrVertexBatch.h"
#include "SkString.h"

namespace {
class Batch : public GrVertexBatch {
public:
    DEFINE_BATCH_CLASS_ID

    const char* name() const override { return "Dummy Batch"; }
    void computePipelineOptimizations(GrInitInvariantOutput* color,
                                      GrInitInvariantOutput* coverage,
                                      GrBatchToXPOverrides* overrides) const override {
        color->setUnknownFourComponents();
        coverage->setUnknownSingleComponent();
    }

    void initBatchTracker(const GrXPOverridesForBatch& overrides) override {}

    Batch(int numAttribs)
        : INHERITED(ClassID())
        , fNumAttribs(numAttribs) {
        this->setBounds(SkRect::MakeWH(1.f, 1.f), HasAABloat::kNo, IsZeroArea::kNo);
    }

private:
    bool onCombineIfPossible(GrBatch*, const GrCaps&) override { return false; }
    void onPrepareDraws(Target* target) const override {
        class GP : public GrGeometryProcessor {
        public:
            GP(int numAttribs) {
                this->initClassID<GP>();
                SkASSERT(numAttribs > 1);
                for (auto i = 0; i < numAttribs; ++i) {
                    fAttribNames.push_back().printf("attr%d", i);
                }
                for (auto i = 0; i < numAttribs; ++i) {
                    this->addVertexAttrib(fAttribNames[i].c_str(), kVec2f_GrVertexAttribType);
                }
            };
            const char* name() const override { return "Dummy GP"; }

            GrGLSLPrimitiveProcessor* createGLSLInstance(const GrGLSLCaps&) const override {
                class GLSLGP : public GrGLSLGeometryProcessor {
                public:
                    void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
                        const GP& gp = args.fGP.cast<GP>();
                        args.fVaryingHandler->emitAttributes(gp);
                        this->setupPosition(args.fVertBuilder, gpArgs, gp.fAttribs[0].fName);
                    }
                    void setData(const GrGLSLProgramDataManager& pdman,
                                 const GrPrimitiveProcessor& primProc) override {}
                };
                return new GLSLGP();
            }
            void getGLSLProcessorKey(const GrGLSLCaps&,
                                     GrProcessorKeyBuilder* builder) const override {
                builder->add32(this->numAttribs());
            }

        private:
            SkTArray<SkString> fAttribNames;
        };
        SkAutoTUnref<GrGeometryProcessor> gp(new GP(fNumAttribs));
        QuadHelper helper;
        size_t vertexStride = gp->getVertexStride();
        SkPoint* vertices = reinterpret_cast<SkPoint*>(helper.init(target, vertexStride, 1));
        vertices->setRectFan(0.f, 0.f, 1.f, 1.f, vertexStride);
        helper.recordDraw(target, gp);
    }

    int fNumAttribs;

    typedef GrVertexBatch INHERITED;
};
}

DEF_GPUTEST_FOR_ALL_CONTEXTS(VertexAttributeCount, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();

    sk_sp<GrDrawContext> drawContext(context->makeDrawContext(SkBackingFit::kApprox,
                                                              1, 1, kRGBA_8888_GrPixelConfig,
                                                              nullptr));
    if (!drawContext) {
        ERRORF(reporter, "Could not create draw context.");
        return;
    }
    int attribCnt = context->caps()->maxVertexAttributes();
    if (!attribCnt) {
        ERRORF(reporter, "No attributes allowed?!");
        return;
    }
    context->flush();
    context->resetGpuStats();
#if GR_GPU_STATS
    REPORTER_ASSERT(reporter, context->getGpu()->stats()->numDraws() == 0);
    REPORTER_ASSERT(reporter, context->getGpu()->stats()->numFailedDraws() == 0);
#endif
    SkAutoTUnref<GrDrawBatch> batch;
    GrPaint grPaint;
    // This one should succeed.
    batch.reset(new Batch(attribCnt));
    drawContext->drawContextPriv().testingOnly_drawBatch(grPaint, batch);
    context->flush();
#if GR_GPU_STATS
    REPORTER_ASSERT(reporter, context->getGpu()->stats()->numDraws() == 1);
    REPORTER_ASSERT(reporter, context->getGpu()->stats()->numFailedDraws() == 0);
#endif
    context->resetGpuStats();
    // This one should fail.
    batch.reset(new Batch(attribCnt+1));
    drawContext->drawContextPriv().testingOnly_drawBatch(grPaint, batch);
    context->flush();
#if GR_GPU_STATS
    REPORTER_ASSERT(reporter, context->getGpu()->stats()->numDraws() == 0);
    REPORTER_ASSERT(reporter, context->getGpu()->stats()->numFailedDraws() == 1);
#endif
}
#endif
