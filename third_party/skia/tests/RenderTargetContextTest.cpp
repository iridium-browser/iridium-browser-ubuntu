/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This is a GPU-backend specific test.

#include "Test.h"

#if SK_SUPPORT_GPU
#include "GrTextureProxy.h"
#include "GrRenderTargetContext.h"

static const int kSize = 64;

static sk_sp<GrRenderTargetContext> get_rtc(GrContext* ctx, bool wrapped) {

    if (wrapped) {
        return ctx->makeRenderTargetContext(SkBackingFit::kExact,
                                            kSize, kSize,
                                            kRGBA_8888_GrPixelConfig, nullptr);
    } else {
        return ctx->makeDeferredRenderTargetContext(SkBackingFit::kExact,
                                                    kSize, kSize,
                                                    kRGBA_8888_GrPixelConfig, nullptr);
    }
}

static void check_is_wrapped_status(skiatest::Reporter* reporter,
                                    GrRenderTargetContext* rtCtx,
                                    bool wrappedExpectation) {
    REPORTER_ASSERT(reporter, rtCtx->isWrapped_ForTesting() == wrappedExpectation);

    GrTextureProxy* tProxy = rtCtx->asTextureProxy();
    REPORTER_ASSERT(reporter, tProxy);

    REPORTER_ASSERT(reporter, tProxy->isWrapped_ForTesting() == wrappedExpectation);
}

DEF_GPUTEST_FOR_RENDERING_CONTEXTS(RenderTargetContextTest, reporter, ctxInfo) {
    GrContext* ctx = ctxInfo.grContext();

    // A wrapped rtCtx's textureProxy is also wrapped
    {
        sk_sp<GrRenderTargetContext> rtCtx(get_rtc(ctx, true));
        check_is_wrapped_status(reporter, rtCtx.get(), true);
    }

    // A deferred rtCtx's textureProxy is also deferred and GrRenderTargetContext::instantiate()
    // swaps both from deferred to wrapped
    {
        sk_sp<GrRenderTargetContext> rtCtx(get_rtc(ctx, false));

        check_is_wrapped_status(reporter, rtCtx.get(), false);

        GrRenderTarget* rt = rtCtx->instantiate();
        REPORTER_ASSERT(reporter, rt);

        check_is_wrapped_status(reporter, rtCtx.get(), true);
    }

    // Calling instantiate on a GrRenderTargetContext's textureProxy also instantiates the
    // GrRenderTargetContext
    {
        sk_sp<GrRenderTargetContext> rtCtx(get_rtc(ctx, false));

        check_is_wrapped_status(reporter, rtCtx.get(), false);

        GrTextureProxy* tProxy = rtCtx->asTextureProxy();
        REPORTER_ASSERT(reporter, tProxy);

        GrTexture* tex = tProxy->instantiate(ctx->textureProvider());
        REPORTER_ASSERT(reporter, tex);

        check_is_wrapped_status(reporter, rtCtx.get(), true);
    }

    // readPixels switches a deferred rtCtx to wrapped
    {
        sk_sp<GrRenderTargetContext> rtCtx(get_rtc(ctx, false));

        check_is_wrapped_status(reporter, rtCtx.get(), false);

        SkImageInfo dstInfo = SkImageInfo::MakeN32Premul(kSize, kSize);
        SkAutoTMalloc<uint32_t> dstBuffer(kSize * kSize);
        static const size_t kRowBytes = sizeof(uint32_t) * kSize;

        bool result = rtCtx->readPixels(dstInfo, dstBuffer.get(), kRowBytes, 0, 0);
        REPORTER_ASSERT(reporter, result);

        check_is_wrapped_status(reporter, rtCtx.get(), true);
    }

    // TODO: in a future world we should be able to add a test that the majority of
    // GrRenderTargetContext calls do not force the instantiation of a deferred 
    // GrRenderTargetContext
}
#endif
