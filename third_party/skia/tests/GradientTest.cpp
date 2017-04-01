/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkColorShader.h"
#include "SkGradientShader.h"
#include "SkShader.h"
#include "SkSurface.h"
#include "SkTemplates.h"
#include "Test.h"

// https://code.google.com/p/chromium/issues/detail?id=448299
// Giant (inverse) matrix causes overflow when converting/computing using 32.32
// Before the fix, we would assert (and then crash).
static void test_big_grad(skiatest::Reporter* reporter) {
    const SkColor colors[] = { SK_ColorRED, SK_ColorBLUE };
    const SkPoint pts[] = {{ 15, 14.7112684f }, { 0.709064007f, 12.6108112f }};
    SkPaint paint;
    paint.setShader(SkGradientShader::MakeLinear(pts, colors, nullptr, 2,
                                                 SkShader::kClamp_TileMode));

    SkBitmap bm;
    bm.allocN32Pixels(2000, 1);
    SkCanvas c(bm);

    const SkScalar affine[] = {
        1.06608627e-06f, 4.26434525e-07f, 6.2855f, 2.6611f, 273.4393f, 244.0046f
    };
    SkMatrix matrix;
    matrix.setAffine(affine);
    c.concat(matrix);

    c.drawPaint(paint);
}

struct GradRec {
    int             fColorCount;
    const SkColor*  fColors;
    const SkScalar* fPos;
    const SkPoint*  fPoint;   // 2
    const SkScalar* fRadius; // 2
    SkShader::TileMode fTileMode;

    void gradCheck(skiatest::Reporter* reporter, const sk_sp<SkShader>& shader,
                   SkShader::GradientInfo* info,
                   SkShader::GradientType gt) const {
        SkAutoTMalloc<SkColor> colorStorage(fColorCount);
        SkAutoTMalloc<SkScalar> posStorage(fColorCount);

        info->fColorCount = fColorCount;
        info->fColors = colorStorage;
        info->fColorOffsets = posStorage.get();
        REPORTER_ASSERT(reporter, shader->asAGradient(info) == gt);

        REPORTER_ASSERT(reporter, info->fColorCount == fColorCount);
        REPORTER_ASSERT(reporter,
                        !memcmp(info->fColors, fColors, fColorCount * sizeof(SkColor)));
        REPORTER_ASSERT(reporter,
                        !memcmp(info->fColorOffsets, fPos, fColorCount * sizeof(SkScalar)));
        REPORTER_ASSERT(reporter, fTileMode == info->fTileMode);
    }
};


static void none_gradproc(skiatest::Reporter* reporter, const GradRec&, const GradRec&) {
    sk_sp<SkShader> s(SkShader::MakeEmptyShader());
    REPORTER_ASSERT(reporter, SkShader::kNone_GradientType == s->asAGradient(nullptr));
}

static void color_gradproc(skiatest::Reporter* reporter, const GradRec& rec, const GradRec&) {
    sk_sp<SkShader> s(new SkColorShader(rec.fColors[0]));
    REPORTER_ASSERT(reporter, SkShader::kColor_GradientType == s->asAGradient(nullptr));

    SkShader::GradientInfo info;
    info.fColors = nullptr;
    info.fColorCount = 0;
    s->asAGradient(&info);
    REPORTER_ASSERT(reporter, 1 == info.fColorCount);
}

static void linear_gradproc(skiatest::Reporter* reporter, const GradRec& buildRec,
                            const GradRec& checkRec) {
    sk_sp<SkShader> s(SkGradientShader::MakeLinear(buildRec.fPoint, buildRec.fColors, buildRec.fPos,
                                                   buildRec.fColorCount, buildRec.fTileMode));

    SkShader::GradientInfo info;
    checkRec.gradCheck(reporter, s, &info, SkShader::kLinear_GradientType);
    REPORTER_ASSERT(reporter, !memcmp(info.fPoint, checkRec.fPoint, 2 * sizeof(SkPoint)));
}

static void radial_gradproc(skiatest::Reporter* reporter, const GradRec& buildRec,
                            const GradRec& checkRec) {
    sk_sp<SkShader> s(SkGradientShader::MakeRadial(buildRec.fPoint[0], buildRec.fRadius[0],
                                                   buildRec.fColors, buildRec.fPos,
                                                   buildRec.fColorCount, buildRec.fTileMode));

    SkShader::GradientInfo info;
    checkRec.gradCheck(reporter, s, &info, SkShader::kRadial_GradientType);
    REPORTER_ASSERT(reporter, info.fPoint[0] == checkRec.fPoint[0]);
    REPORTER_ASSERT(reporter, info.fRadius[0] == checkRec.fRadius[0]);
}

static void sweep_gradproc(skiatest::Reporter* reporter, const GradRec& buildRec,
                           const GradRec& checkRec) {
    sk_sp<SkShader> s(SkGradientShader::MakeSweep(buildRec.fPoint[0].fX, buildRec.fPoint[0].fY,
                                                  buildRec.fColors, buildRec.fPos,
                                                  buildRec.fColorCount));

    SkShader::GradientInfo info;
    checkRec.gradCheck(reporter, s, &info, SkShader::kSweep_GradientType);
    REPORTER_ASSERT(reporter, info.fPoint[0] == checkRec.fPoint[0]);
}

static void conical_gradproc(skiatest::Reporter* reporter, const GradRec& buildRec,
                             const GradRec& checkRec) {
    sk_sp<SkShader> s(SkGradientShader::MakeTwoPointConical(buildRec.fPoint[0],
                                                            buildRec.fRadius[0],
                                                            buildRec.fPoint[1],
                                                            buildRec.fRadius[1],
                                                            buildRec.fColors,
                                                            buildRec.fPos,
                                                            buildRec.fColorCount,
                                                            buildRec.fTileMode));

    SkShader::GradientInfo info;
    checkRec.gradCheck(reporter, s, &info, SkShader::kConical_GradientType);
    REPORTER_ASSERT(reporter, !memcmp(info.fPoint, checkRec.fPoint, 2 * sizeof(SkPoint)));
    REPORTER_ASSERT(reporter, !memcmp(info.fRadius, checkRec.fRadius, 2 * sizeof(SkScalar)));
}

// Ensure that repeated color gradients behave like drawing a single color
static void TestConstantGradient(skiatest::Reporter*) {
    const SkPoint pts[] = {
        { 0, 0 },
        { SkIntToScalar(10), 0 }
    };
    SkColor colors[] = { SK_ColorBLUE, SK_ColorBLUE };
    const SkScalar pos[] = { 0, SK_Scalar1 };
    SkPaint paint;
    paint.setShader(SkGradientShader::MakeLinear(pts, colors, pos, 2, SkShader::kClamp_TileMode));
    SkBitmap outBitmap;
    outBitmap.allocN32Pixels(10, 1);
    SkCanvas canvas(outBitmap);
    canvas.drawPaint(paint);
    SkAutoLockPixels alp(outBitmap);
    for (int i = 0; i < 10; i++) {
        // The following is commented out because it currently fails
        // Related bug: https://code.google.com/p/skia/issues/detail?id=1098

        // REPORTER_ASSERT(reporter, SK_ColorBLUE == outBitmap.getColor(i, 0));
    }
}

typedef void (*GradProc)(skiatest::Reporter* reporter, const GradRec&, const GradRec&);

static void TestGradientShaders(skiatest::Reporter* reporter) {
    static const SkColor gColors[] = { SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE };
    static const SkScalar gPos[] = { 0, SK_ScalarHalf, SK_Scalar1 };
    static const SkPoint gPts[] = {
        { 0, 0 },
        { SkIntToScalar(10), SkIntToScalar(20) }
    };
    static const SkScalar gRad[] = { SkIntToScalar(1), SkIntToScalar(2) };

    GradRec rec;
    rec.fColorCount = SK_ARRAY_COUNT(gColors);
    rec.fColors = gColors;
    rec.fPos = gPos;
    rec.fPoint = gPts;
    rec.fRadius = gRad;
    rec.fTileMode = SkShader::kClamp_TileMode;

    static const GradProc gProcs[] = {
        none_gradproc,
        color_gradproc,
        linear_gradproc,
        radial_gradproc,
        sweep_gradproc,
        conical_gradproc,
    };

    for (size_t i = 0; i < SK_ARRAY_COUNT(gProcs); ++i) {
        gProcs[i](reporter, rec, rec);
    }
}

static void TestGradientOptimization(skiatest::Reporter* reporter) {
    static const struct {
        GradProc fProc;
        bool     fIsClampRestricted;
    } gProcInfo[] = {
        { linear_gradproc , false },
        { radial_gradproc , false },
        { sweep_gradproc  , true  }, // sweep is funky in that it always pretends to be kClamp.
        { conical_gradproc, false },
    };

    static const SkColor   gC_00[] = { 0xff000000, 0xff000000 };
    static const SkColor   gC_01[] = { 0xff000000, 0xffffffff };
    static const SkColor   gC_11[] = { 0xffffffff, 0xffffffff };
    static const SkColor  gC_001[] = { 0xff000000, 0xff000000, 0xffffffff };
    static const SkColor  gC_011[] = { 0xff000000, 0xffffffff, 0xffffffff };
    static const SkColor gC_0011[] = { 0xff000000, 0xff000000, 0xffffffff, 0xffffffff };

    static const SkScalar   gP_01[] = { 0, 1 };
    static const SkScalar  gP_001[] = { 0,   0, 1 };
    static const SkScalar  gP_011[] = { 0,   1, 1 };
    static const SkScalar  gP_0x1[] = { 0, .5f, 1 };
    static const SkScalar gP_0011[] = { 0, 0, 1, 1 };

    static const SkPoint    gPts[] = { {0, 0}, {1, 1} };
    static const SkScalar gRadii[] = { 1, 2 };

    static const struct {
        const SkColor*  fCol;
        const SkScalar* fPos;
        int             fCount;

        const SkColor*  fExpectedCol;
        const SkScalar* fExpectedPos;
        int             fExpectedCount;
        bool            fRequiresNonClamp;
    } gTests[] = {
        { gC_001,  gP_001, 3,  gC_01,  gP_01, 2, false },
        { gC_001,  gP_011, 3,  gC_00,  gP_01, 2, true  },
        { gC_001,  gP_0x1, 3, gC_001, gP_0x1, 3, false },
        { gC_001, nullptr, 3, gC_001, gP_0x1, 3, false },

        { gC_011,  gP_001, 3,  gC_11,  gP_01, 2, true  },
        { gC_011,  gP_011, 3,  gC_01,  gP_01, 2, false },
        { gC_011,  gP_0x1, 3, gC_011, gP_0x1, 3, false },
        { gC_011, nullptr, 3, gC_011, gP_0x1, 3, false },

        { gC_0011, gP_0011, 4, gC_0011, gP_0011, 4, false },
    };

    for (size_t i = 0; i < SK_ARRAY_COUNT(gProcInfo); ++i) {
        for (int mode = 0; mode < SkShader::kTileModeCount; ++mode) {
            if (gProcInfo[i].fIsClampRestricted && mode != SkShader::kClamp_TileMode) {
                continue;
            }

            for (size_t t = 0; t < SK_ARRAY_COUNT(gTests); ++t) {
                GradRec rec;
                rec.fColorCount = gTests[t].fCount;
                rec.fColors     = gTests[t].fCol;
                rec.fPos        = gTests[t].fPos;
                rec.fTileMode   = static_cast<SkShader::TileMode>(mode);
                rec.fPoint      = gPts;
                rec.fRadius     = gRadii;

                GradRec expected = rec;
                if (!gTests[t].fRequiresNonClamp || mode != SkShader::kClamp_TileMode) {
                    expected.fColorCount = gTests[t].fExpectedCount;
                    expected.fColors     = gTests[t].fExpectedCol;
                    expected.fPos        = gTests[t].fExpectedPos;
                }

                gProcInfo[i].fProc(reporter, rec, expected);
            }
        }
    }
}

static void test_nearly_vertical(skiatest::Reporter* reporter) {
    auto surface(SkSurface::MakeRasterN32Premul(200, 200));

    const SkPoint pts[] = {{ 100, 50 }, { 100.0001f, 50000 }};
    const SkColor colors[] = { SK_ColorBLACK, SK_ColorWHITE };
    const SkScalar pos[] = { 0, 1 };
    SkPaint paint;
    paint.setShader(SkGradientShader::MakeLinear(pts, colors, pos, 2, SkShader::kClamp_TileMode));

    surface->getCanvas()->drawPaint(paint);
}

static void test_vertical(skiatest::Reporter* reporter) {
    auto surface(SkSurface::MakeRasterN32Premul(200, 200));

    const SkPoint pts[] = {{ 100, 50 }, { 100, 50 }};
    const SkColor colors[] = { SK_ColorBLACK, SK_ColorWHITE };
    const SkScalar pos[] = { 0, 1 };
    SkPaint paint;
    paint.setShader(SkGradientShader::MakeLinear(pts, colors, pos, 2, SkShader::kClamp_TileMode));

    surface->getCanvas()->drawPaint(paint);
}

// A linear gradient interval can, due to numerical imprecision (likely in the divide)
// finish an interval with the final fx not landing outside of [p0...p1].
// The old code had an assert which this test triggered.
// We now explicitly clamp the resulting fx value.
static void test_linear_fuzz(skiatest::Reporter* reporter) {
    auto surface(SkSurface::MakeRasterN32Premul(1300, 630));

    const SkPoint pts[] = {{ 179.5f, -179.5f }, { 1074.5f, 715.5f }};
    const SkColor colors[] = { SK_ColorBLACK, SK_ColorWHITE, SK_ColorBLACK, SK_ColorWHITE };
    const SkScalar pos[] = {0, 0.200000003f, 0.800000012f, 1 };

    SkPaint paint;
    paint.setShader(SkGradientShader::MakeLinear(pts, colors, pos, 4, SkShader::kClamp_TileMode));

    SkRect r = {0, 83, 1254, 620};
    surface->getCanvas()->drawRect(r, paint);
}

// https://bugs.chromium.org/p/skia/issues/detail?id=5023
// We should still shade pixels for which the radius is exactly 0.
static void test_two_point_conical_zero_radius(skiatest::Reporter* reporter) {
    auto surface(SkSurface::MakeRasterN32Premul(5, 5));
    surface->getCanvas()->clear(SK_ColorRED);

    const SkColor colors[] = { SK_ColorGREEN, SK_ColorBLUE };
    SkPaint p;
    p.setShader(SkGradientShader::MakeTwoPointConical(
        SkPoint::Make(2.5f, 2.5f), 0,
        SkPoint::Make(3.0f, 3.0f), 10,
        colors, nullptr, SK_ARRAY_COUNT(colors), SkShader::kClamp_TileMode));
    surface->getCanvas()->drawPaint(p);

    // r == 0 for the center pixel.
    // verify that we draw it (no red bleed)
    SkPMColor centerPMColor;
    surface->readPixels(SkImageInfo::MakeN32Premul(1, 1), &centerPMColor, sizeof(SkPMColor), 2, 2);
    REPORTER_ASSERT(reporter, SkGetPackedR32(centerPMColor) == 0);
}

// http://crbug.com/599458
static void test_clamping_overflow(skiatest::Reporter*) {
    SkPaint p;
    const SkColor colors[] = { SK_ColorRED, SK_ColorGREEN };
    const SkPoint pts1[] = { SkPoint::Make(1001, 1000001), SkPoint::Make(1000.99f, 1000000) };

    p.setShader(SkGradientShader::MakeLinear(pts1, colors, nullptr, 2, SkShader::kClamp_TileMode));

    sk_sp<SkSurface> surface(SkSurface::MakeRasterN32Premul(50, 50));
    surface->getCanvas()->scale(100, 100);
    surface->getCanvas()->drawPaint(p);

    const SkPoint pts2[] = { SkPoint::Make(10000.99f, 1000000), SkPoint::Make(10001, 1000001) };
    p.setShader(SkGradientShader::MakeLinear(pts2, colors, nullptr, 2, SkShader::kClamp_TileMode));
    surface->getCanvas()->drawPaint(p);

    // Passes if we don't trigger asserts.
}

// http://crbug.com/636194
static void text_degenerate_linear(skiatest::Reporter*) {
    SkPaint p;
    const SkColor colors[] = { SK_ColorRED, SK_ColorGREEN };
    const SkPoint pts[] = {
        SkPoint::Make(-46058024627067344430605278824628224.0f, 0),
        SkPoint::Make(SK_ScalarMax, 0)
    };

    p.setShader(SkGradientShader::MakeLinear(pts, colors, nullptr, 2, SkShader::kClamp_TileMode));
    sk_sp<SkSurface> surface(SkSurface::MakeRasterN32Premul(50, 50));
    surface->getCanvas()->drawPaint(p);

    // Passes if we don't trigger asserts.
}

DEF_TEST(Gradient, reporter) {
    TestGradientShaders(reporter);
    TestGradientOptimization(reporter);
    TestConstantGradient(reporter);
    test_big_grad(reporter);
    test_nearly_vertical(reporter);
    test_vertical(reporter);
    test_linear_fuzz(reporter);
    test_two_point_conical_zero_radius(reporter);
    test_clamping_overflow(reporter);
    text_degenerate_linear(reporter);
}
