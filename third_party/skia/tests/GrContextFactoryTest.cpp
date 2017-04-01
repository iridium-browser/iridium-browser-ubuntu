/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkTypes.h"

#if SK_SUPPORT_GPU

#include "GrContextFactory.h"
#include "GrCaps.h"
#include "Test.h"

using namespace sk_gpu_test;

DEF_GPUTEST(GrContextFactory_NVPRContextOptionHasPathRenderingSupport, reporter, /*factory*/) {
    // Test that if NVPR is requested, the context always has path rendering
    // or the context creation fails.
    GrContextFactory testFactory;
    // Test that if NVPR is possible, caps are in sync.
    for (int i = 0; i < GrContextFactory::kContextTypeCnt; ++i) {
        GrContextFactory::ContextType ctxType = static_cast<GrContextFactory::ContextType>(i);
        GrContext* context = testFactory.get(ctxType,
                                             GrContextFactory::ContextOptions::kEnableNVPR);
        if (!context) {
            continue;
        }
        REPORTER_ASSERT(
            reporter,
            context->caps()->shaderCaps()->pathRenderingSupport());
    }
}

DEF_GPUTEST(GrContextFactory_NoPathRenderingUnlessNVPRRequested, reporter, /*factory*/) {
    // Test that if NVPR is not requested, the context never has path rendering support.

    GrContextFactory testFactory;
    for (int i = 0; i <= GrContextFactory::kLastContextType; ++i) {
        GrContextFactory::ContextType ctxType = (GrContextFactory::ContextType)i;
        GrContext* context = testFactory.get(ctxType);
        if (context) {
            REPORTER_ASSERT(
                reporter,
                !context->caps()->shaderCaps()->pathRenderingSupport());
        }
    }
}

DEF_GPUTEST(GrContextFactory_RequiredSRGBSupport, reporter, /*factory*/) {
    // Test that if sRGB support is requested, the context always has that capability
    // or the context creation fails. Also test that if the creation fails, a context
    // created without that flag would not have had sRGB support.
    GrContextFactory testFactory;
    // Test that if sRGB is requested, caps are in sync.
    for (int i = 0; i < GrContextFactory::kContextTypeCnt; ++i) {
        GrContextFactory::ContextType ctxType = static_cast<GrContextFactory::ContextType>(i);
        GrContext* context =
            testFactory.get(ctxType, GrContextFactory::ContextOptions::kRequireSRGBSupport);

        if (context) {
            REPORTER_ASSERT(reporter, context->caps()->srgbSupport());
        } else {
            context = testFactory.get(ctxType);
            if (context) {
                REPORTER_ASSERT(reporter, !context->caps()->srgbSupport());
            }
        }
    }
}

DEF_GPUTEST(GrContextFactory_abandon, reporter, /*factory*/) {
    GrContextFactory testFactory;
    for (int i = 0; i < GrContextFactory::kContextTypeCnt; ++i) {
        GrContextFactory::ContextType ctxType = (GrContextFactory::ContextType) i;
        ContextInfo info1 = testFactory.getContextInfo(ctxType);
        if (!info1.grContext()) {
            continue;
        }
        REPORTER_ASSERT(reporter, info1.testContext());
         // Ref for comparison. The API does not explicitly say that this stays alive.
        info1.grContext()->ref();
        testFactory.abandonContexts();

        // Test that we get different context after abandon.
        ContextInfo info2 = testFactory.getContextInfo(ctxType);
        REPORTER_ASSERT(reporter, info2.grContext());
        REPORTER_ASSERT(reporter, info2.testContext());

        REPORTER_ASSERT(reporter, info1.grContext() != info2.grContext());
        // The GL context should also change, but it also could get the same address.

        info1.grContext()->unref();
    }
}

#endif
