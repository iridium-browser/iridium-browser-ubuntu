/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This is a GPU-backend specific test. It relies on static intializers to work

#include "SkTypes.h"

#if SK_SUPPORT_GPU && SK_ALLOW_STATIC_GLOBAL_INITIALIZERS

#include "GrAutoLocaleSetter.h"
#include "GrBatchTest.h"
#include "GrContextFactory.h"
#include "GrDrawContextPriv.h"
#include "GrDrawingManager.h"
#include "GrInvariantOutput.h"
#include "GrPipeline.h"
#include "GrResourceProvider.h"
#include "GrTest.h"
#include "GrXferProcessor.h"
#include "SkChecksum.h"
#include "SkRandom.h"
#include "Test.h"

#include "batches/GrDrawBatch.h"

#include "effects/GrConfigConversionEffect.h"
#include "effects/GrPorterDuffXferProcessor.h"
#include "effects/GrXfermodeFragmentProcessor.h"

#include "gl/GrGLGpu.h"
#include "glsl/GrGLSLFragmentProcessor.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLProgramBuilder.h"

/*
 * A dummy processor which just tries to insert a massive key and verify that it can retrieve the
 * whole thing correctly
 */
static const uint32_t kMaxKeySize = 1024;

class GLBigKeyProcessor : public GrGLSLFragmentProcessor {
public:
    void emitCode(EmitArgs& args) override {
        // pass through
        GrGLSLFragmentBuilder* fragBuilder = args.fFragBuilder;
        if (args.fInputColor) {
            fragBuilder->codeAppendf("%s = %s;\n", args.fOutputColor, args.fInputColor);
        } else {
            fragBuilder->codeAppendf("%s = vec4(1.0);\n", args.fOutputColor);
        }
    }

    static void GenKey(const GrProcessor& processor, const GrGLSLCaps&, GrProcessorKeyBuilder* b) {
        for (uint32_t i = 0; i < kMaxKeySize; i++) {
            b->add32(i);
        }
    }

private:
    typedef GrGLSLFragmentProcessor INHERITED;
};

class BigKeyProcessor : public GrFragmentProcessor {
public:
    static sk_sp<GrFragmentProcessor> Make() {
        return sk_sp<GrFragmentProcessor>(new BigKeyProcessor);
    }

    const char* name() const override { return "Big Ole Key"; }

    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override {
        return new GLBigKeyProcessor;
    }

private:
    BigKeyProcessor() {
        this->initClassID<BigKeyProcessor>();
    }
    virtual void onGetGLSLProcessorKey(const GrGLSLCaps& caps,
                                       GrProcessorKeyBuilder* b) const override {
        GLBigKeyProcessor::GenKey(*this, caps, b);
    }
    bool onIsEqual(const GrFragmentProcessor&) const override { return true; }
    void onComputeInvariantOutput(GrInvariantOutput* inout) const override { }

    GR_DECLARE_FRAGMENT_PROCESSOR_TEST;

    typedef GrFragmentProcessor INHERITED;
};

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(BigKeyProcessor);

sk_sp<GrFragmentProcessor> BigKeyProcessor::TestCreate(GrProcessorTestData*) {
    return BigKeyProcessor::Make();
}

//////////////////////////////////////////////////////////////////////////////

class BlockInputFragmentProcessor : public GrFragmentProcessor {
public:
    static sk_sp<GrFragmentProcessor> Make(sk_sp<GrFragmentProcessor> fp) {
        return sk_sp<GrFragmentProcessor>(new BlockInputFragmentProcessor(fp));
    }

    const char* name() const override { return "Block Input"; }

    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override { return new GLFP; }

private:
    class GLFP : public GrGLSLFragmentProcessor {
    public:
        void emitCode(EmitArgs& args) override {
            this->emitChild(0, nullptr, args);
        }

    private:
        typedef GrGLSLFragmentProcessor INHERITED;
    };

    BlockInputFragmentProcessor(sk_sp<GrFragmentProcessor> child) {
        this->initClassID<BlockInputFragmentProcessor>();
        this->registerChildProcessor(std::move(child));
    }

    void onGetGLSLProcessorKey(const GrGLSLCaps& caps, GrProcessorKeyBuilder* b) const override {}

    bool onIsEqual(const GrFragmentProcessor&) const override { return true; }

    void onComputeInvariantOutput(GrInvariantOutput* inout) const override {
        inout->setToOther(kRGBA_GrColorComponentFlags, GrColor_WHITE,
                          GrInvariantOutput::kWillNot_ReadInput);
        this->childProcessor(0).computeInvariantOutput(inout);
    }

    typedef GrFragmentProcessor INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

/*
 * Begin test code
 */
static const int kRenderTargetHeight = 1;
static const int kRenderTargetWidth = 1;

static sk_sp<GrDrawContext> random_draw_context(GrContext* context,
                                                SkRandom* random,
                                                const GrCaps* caps) {
    GrSurfaceOrigin origin = random->nextBool() ? kTopLeft_GrSurfaceOrigin
                                                : kBottomLeft_GrSurfaceOrigin;
    int sampleCnt = random->nextBool() ? SkTMin(4, caps->maxSampleCount()) : 0;

    sk_sp<GrDrawContext> drawContext(context->makeDrawContext(SkBackingFit::kExact,
                                                              kRenderTargetWidth,
                                                              kRenderTargetHeight,
                                                              kRGBA_8888_GrPixelConfig,
                                                              nullptr,
                                                              sampleCnt,
                                                              origin));
    return drawContext;
}

static void set_random_xpf(GrPaint* paint, GrProcessorTestData* d) {
    sk_sp<GrXPFactory> xpf(GrProcessorTestFactory<GrXPFactory>::Make(d));
    SkASSERT(xpf);
    paint->setXPFactory(std::move(xpf));
}

static sk_sp<GrFragmentProcessor> create_random_proc_tree(GrProcessorTestData* d,
                                                          int minLevels, int maxLevels) {
    SkASSERT(1 <= minLevels);
    SkASSERT(minLevels <= maxLevels);

    // Return a leaf node if maxLevels is 1 or if we randomly chose to terminate.
    // If returning a leaf node, make sure that it doesn't have children (e.g. another
    // GrComposeEffect)
    const float terminateProbability = 0.3f;
    if (1 == minLevels) {
        bool terminate = (1 == maxLevels) || (d->fRandom->nextF() < terminateProbability);
        if (terminate) {
            sk_sp<GrFragmentProcessor> fp;
            while (true) {
                fp = GrProcessorTestFactory<GrFragmentProcessor>::Make(d);
                SkASSERT(fp);
                if (0 == fp->numChildProcessors()) {
                    break;
                }
            }
            return fp;
        }
    }
    // If we didn't terminate, choose either the left or right subtree to fulfill
    // the minLevels requirement of this tree; the other child can have as few levels as it wants.
    // Also choose a random xfer mode that's supported by CreateFrom2Procs().
    if (minLevels > 1) {
        --minLevels;
    }
    sk_sp<GrFragmentProcessor> minLevelsChild(create_random_proc_tree(d, minLevels, maxLevels - 1));
    sk_sp<GrFragmentProcessor> otherChild(create_random_proc_tree(d, 1, maxLevels - 1));
    SkXfermode::Mode mode = static_cast<SkXfermode::Mode>(d->fRandom->nextRangeU(0,
                                                          SkXfermode::kLastCoeffMode));
    sk_sp<GrFragmentProcessor> fp;
    if (d->fRandom->nextF() < 0.5f) {
        fp = GrXfermodeFragmentProcessor::MakeFromTwoProcessors(std::move(minLevelsChild),
                                                                std::move(otherChild), mode);
        SkASSERT(fp);
    } else {
        fp = GrXfermodeFragmentProcessor::MakeFromTwoProcessors(std::move(otherChild),
                                                                std::move(minLevelsChild), mode);
        SkASSERT(fp);
    }
    return fp;
}

static void set_random_color_coverage_stages(GrPaint* paint,
                                             GrProcessorTestData* d,
                                             int maxStages) {
    // Randomly choose to either create a linear pipeline of procs or create one proc tree
    const float procTreeProbability = 0.5f;
    if (d->fRandom->nextF() < procTreeProbability) {
        // A full tree with 5 levels (31 nodes) may exceed the max allowed length of the gl
        // processor key; maxTreeLevels should be a number from 1 to 4 inclusive.
        const int maxTreeLevels = 4;
        sk_sp<GrFragmentProcessor> fp(create_random_proc_tree(d, 2, maxTreeLevels));
        paint->addColorFragmentProcessor(std::move(fp));
    } else {
        int numProcs = d->fRandom->nextULessThan(maxStages + 1);
        int numColorProcs = d->fRandom->nextULessThan(numProcs + 1);

        for (int s = 0; s < numProcs;) {
            sk_sp<GrFragmentProcessor> fp(GrProcessorTestFactory<GrFragmentProcessor>::Make(d));
            SkASSERT(fp);

            // finally add the stage to the correct pipeline in the drawstate
            if (s < numColorProcs) {
                paint->addColorFragmentProcessor(std::move(fp));
            } else {
                paint->addCoverageFragmentProcessor(std::move(fp));
            }
            ++s;
        }
    }
}

static bool set_random_state(GrPaint* paint, SkRandom* random) {
    if (random->nextBool()) {
        paint->setDisableOutputConversionToSRGB(true);
    }
    if (random->nextBool()) {
        paint->setAllowSRGBInputs(true);
    }
    if (random->nextBool()) {
        paint->setAntiAlias(true);
    }
    return random->nextBool();
}

// right now, the only thing we seem to care about in drawState's stencil is 'doesWrite()'
static const GrUserStencilSettings* get_random_stencil(SkRandom* random) {
    static constexpr GrUserStencilSettings kDoesWriteStencil(
        GrUserStencilSettings::StaticInit<
            0xffff,
            GrUserStencilTest::kAlways,
            0xffff,
            GrUserStencilOp::kReplace,
            GrUserStencilOp::kReplace,
            0xffff>()
    );
    static constexpr GrUserStencilSettings kDoesNotWriteStencil(
        GrUserStencilSettings::StaticInit<
            0xffff,
            GrUserStencilTest::kNever,
            0xffff,
            GrUserStencilOp::kKeep,
            GrUserStencilOp::kKeep,
            0xffff>()
    );

    if (random->nextBool()) {
        return &kDoesWriteStencil;
    } else {
        return &kDoesNotWriteStencil;
    }
}

bool GrDrawingManager::ProgramUnitTest(GrContext* context, int maxStages) {
    GrDrawingManager* drawingManager = context->drawingManager();

    // setup dummy textures
    GrSurfaceDesc dummyDesc;
    dummyDesc.fFlags = kRenderTarget_GrSurfaceFlag;
    dummyDesc.fConfig = kSkia8888_GrPixelConfig;
    dummyDesc.fWidth = 34;
    dummyDesc.fHeight = 18;
    SkAutoTUnref<GrTexture> dummyTexture1(
        context->textureProvider()->createTexture(dummyDesc, SkBudgeted::kNo, nullptr, 0));
    dummyDesc.fFlags = kNone_GrSurfaceFlags;
    dummyDesc.fConfig = kAlpha_8_GrPixelConfig;
    dummyDesc.fWidth = 16;
    dummyDesc.fHeight = 22;
    SkAutoTUnref<GrTexture> dummyTexture2(
        context->textureProvider()->createTexture(dummyDesc, SkBudgeted::kNo, nullptr, 0));

    if (!dummyTexture1 || ! dummyTexture2) {
        SkDebugf("Could not allocate dummy textures");
        return false;
    }

    GrTexture* dummyTextures[] = {dummyTexture1.get(), dummyTexture2.get()};

    // dummy scissor state
    GrScissorState scissor;

    SkRandom random;
    static const int NUM_TESTS = 1024;
    for (int t = 0; t < NUM_TESTS; t++) {
        // setup random render target(can fail)
        sk_sp<GrDrawContext> drawContext(random_draw_context(context, &random, context->caps()));
        if (!drawContext) {
            SkDebugf("Could not allocate drawContext");
            return false;
        }

        GrPaint grPaint;

        SkAutoTUnref<GrDrawBatch> batch(GrRandomDrawBatch(&random, context));
        SkASSERT(batch);

        GrProcessorTestData ptd(&random, context, context->caps(),
                                drawContext.get(), dummyTextures);
        set_random_color_coverage_stages(&grPaint, &ptd, maxStages);
        set_random_xpf(&grPaint, &ptd);
        bool snapToCenters = set_random_state(&grPaint, &random);
        const GrUserStencilSettings* uss = get_random_stencil(&random);

        drawContext->drawContextPriv().testingOnly_drawBatch(grPaint, batch, uss, snapToCenters);
    }
    // Flush everything, test passes if flush is successful(ie, no asserts are hit, no crashes)
    drawingManager->flush();

    // Validate that GrFPs work correctly without an input.
    sk_sp<GrDrawContext> drawContext(context->makeDrawContext(SkBackingFit::kExact,
                                                              kRenderTargetWidth,
                                                              kRenderTargetHeight,
                                                              kRGBA_8888_GrPixelConfig,
                                                              nullptr));
    if (!drawContext) {
        SkDebugf("Could not allocate a drawContext");
        return false;
    }

    int fpFactoryCnt = GrProcessorTestFactory<GrFragmentProcessor>::Count();
    for (int i = 0; i < fpFactoryCnt; ++i) {
        // Since FP factories internally randomize, call each 10 times.
        for (int j = 0; j < 10; ++j) {
            SkAutoTUnref<GrDrawBatch> batch(GrRandomDrawBatch(&random, context));
            SkASSERT(batch);
            GrProcessorTestData ptd(&random, context, context->caps(),
                                    drawContext.get(), dummyTextures);
            GrPaint grPaint;
            grPaint.setXPFactory(GrPorterDuffXPFactory::Make(SkXfermode::kSrc_Mode));

            sk_sp<GrFragmentProcessor> fp(
                GrProcessorTestFactory<GrFragmentProcessor>::MakeIdx(i, &ptd));
            sk_sp<GrFragmentProcessor> blockFP(
                BlockInputFragmentProcessor::Make(std::move(fp)));
            grPaint.addColorFragmentProcessor(std::move(blockFP));

            drawContext->drawContextPriv().testingOnly_drawBatch(grPaint, batch);
            drawingManager->flush();
        }
    }

    return true;
}

static int get_glprograms_max_stages(GrContext* context) {
    GrGLGpu* gpu = static_cast<GrGLGpu*>(context->getGpu());
    /*
     * For the time being, we only support the test with desktop GL or for android on
     * ARM platforms
     * TODO When we run ES 3.00 GLSL in more places, test again
     */
    if (kGL_GrGLStandard == gpu->glStandard() ||
        kARM_GrGLVendor == gpu->ctxInfo().vendor()) {
        return 6;
    } else if (kTegra3_GrGLRenderer == gpu->ctxInfo().renderer() ||
               kOther_GrGLRenderer == gpu->ctxInfo().renderer()) {
        return 1;
    }
    return 0;
}

static void test_glprograms_native(skiatest::Reporter* reporter,
                                   const sk_gpu_test::ContextInfo& ctxInfo) {
    int maxStages = get_glprograms_max_stages(ctxInfo.grContext());
    if (maxStages == 0) {
        return;
    }
    REPORTER_ASSERT(reporter, GrDrawingManager::ProgramUnitTest(ctxInfo.grContext(), maxStages));
}

static void test_glprograms_other_contexts(
            skiatest::Reporter* reporter,
            const sk_gpu_test::ContextInfo& ctxInfo) {
    int maxStages = get_glprograms_max_stages(ctxInfo.grContext());
#ifdef SK_BUILD_FOR_WIN
    // Some long shaders run out of temporary registers in the D3D compiler on ANGLE and
    // command buffer.
    maxStages = SkTMin(maxStages, 2);
#endif
    if (maxStages == 0) {
        return;
    }
    REPORTER_ASSERT(reporter, GrDrawingManager::ProgramUnitTest(ctxInfo.grContext(), maxStages));
}

static bool is_native_gl_context_type(sk_gpu_test::GrContextFactory::ContextType type) {
    return type == sk_gpu_test::GrContextFactory::kNativeGL_ContextType;
}

static bool is_other_rendering_gl_context_type(sk_gpu_test::GrContextFactory::ContextType type) {
    return !is_native_gl_context_type(type) &&
           kOpenGL_GrBackend == sk_gpu_test::GrContextFactory::ContextTypeBackend(type) &&
           sk_gpu_test::GrContextFactory::IsRenderingContext(type);
}

DEF_GPUTEST(GLPrograms, reporter, /*factory*/) {
    // Set a locale that would cause shader compilation to fail because of , as decimal separator.
    // skbug 3330
#ifdef SK_BUILD_FOR_WIN
    GrAutoLocaleSetter als("sv-SE");
#else
    GrAutoLocaleSetter als("sv_SE.UTF-8");
#endif

    // We suppress prints to avoid spew
    GrContextOptions opts;
    opts.fSuppressPrints = true;
    sk_gpu_test::GrContextFactory debugFactory(opts);
    skiatest::RunWithGPUTestContexts(test_glprograms_native, &is_native_gl_context_type,
                                     reporter, &debugFactory);
    skiatest::RunWithGPUTestContexts(test_glprograms_other_contexts,
                                     &is_other_rendering_gl_context_type, reporter, &debugFactory);
}

#endif
