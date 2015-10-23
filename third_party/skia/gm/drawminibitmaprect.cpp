/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkColorPriv.h"
#include "SkGradientShader.h"
#include "SkImage.h"
#include "SkRandom.h"
#include "SkShader.h"
#include "SkSurface.h"

static SkImage* makebm(SkCanvas* caller, int w, int h) {
    SkImageInfo info = SkImageInfo::MakeN32Premul(w, h);
    SkAutoTUnref<SkSurface> surface(caller->newSurface(info));
    if (NULL == surface) {
        surface.reset(SkSurface::NewRaster(info));
    }
    SkCanvas* canvas = surface->getCanvas();

    const SkScalar wScalar = SkIntToScalar(w);
    const SkScalar hScalar = SkIntToScalar(h);

    const SkPoint     pt = { wScalar / 2, hScalar / 2 };

    const SkScalar    radius = 4 * SkMaxScalar(wScalar, hScalar);

    static const SkColor     colors[] = { SK_ColorRED, SK_ColorYELLOW,
                                          SK_ColorGREEN, SK_ColorMAGENTA,
                                          SK_ColorBLUE, SK_ColorCYAN,
                                          SK_ColorRED};

    static const SkScalar    pos[] = {0,
                                      SK_Scalar1 / 6,
                                      2 * SK_Scalar1 / 6,
                                      3 * SK_Scalar1 / 6,
                                      4 * SK_Scalar1 / 6,
                                      5 * SK_Scalar1 / 6,
                                      SK_Scalar1};

    SkASSERT(SK_ARRAY_COUNT(colors) == SK_ARRAY_COUNT(pos));
    SkPaint     paint;
    SkRect rect = SkRect::MakeWH(wScalar, hScalar);
    SkMatrix mat = SkMatrix::I();
    for (int i = 0; i < 4; ++i) {
        paint.setShader(SkGradientShader::CreateRadial(
                        pt, radius,
                        colors, pos,
                        SK_ARRAY_COUNT(colors),
                        SkShader::kRepeat_TileMode,
                        0, &mat))->unref();
        canvas->drawRect(rect, paint);
        rect.inset(wScalar / 8, hScalar / 8);
        mat.postScale(SK_Scalar1 / 4, SK_Scalar1 / 4);
    }
    return surface->newImageSnapshot();
}

static const int gSize = 1024;
static const int gSurfaceSize = 2048;

// This GM calls drawImageRect several times using the same texture. This is
// intended to exercise batching of these calls.
class DrawMiniBitmapRectGM : public skiagm::GM {
public:
    DrawMiniBitmapRectGM(bool antiAlias) : fAA(antiAlias) {
        fName.set("drawminibitmaprect");
        if (fAA) {
            fName.appendf("_aa");
        }
    }

protected:
    SkString onShortName() override { return fName; }

    SkISize onISize() override { return SkISize::Make(gSize, gSize); }

    void onDraw(SkCanvas* canvas) override {
        if (NULL == fImage) {
            fImage.reset(makebm(canvas, gSurfaceSize, gSurfaceSize));
        }

        const SkRect dstRect = { 0, 0, SkIntToScalar(64), SkIntToScalar(64)};
        static const int kMaxSrcRectSize = 1 << (SkNextLog2(gSurfaceSize) + 2);

        static const int kPadX = 30;
        static const int kPadY = 40;

        int rowCount = 0;
        canvas->translate(SkIntToScalar(kPadX), SkIntToScalar(kPadY));
        canvas->save();
        SkRandom random;

        SkPaint paint;
        paint.setAntiAlias(fAA);
        for (int w = 1; w <= kMaxSrcRectSize; w *= 3) {
            for (int h = 1; h <= kMaxSrcRectSize; h *= 3) {

                const SkIRect srcRect =
                        SkIRect::MakeXYWH((gSurfaceSize - w) / 2, (gSurfaceSize - h) / 2, w, h);
                canvas->save();
                switch (random.nextU() % 3) {
                    case 0:
                        canvas->rotate(random.nextF() * 10.f);
                        break;
                    case 1:
                        canvas->rotate(-random.nextF() * 10.f);
                        break;
                    case 2:
                        // rect stays rect
                        break;
                }
                canvas->drawImageRect(fImage, srcRect, dstRect, &paint,
                                      SkCanvas::kFast_SrcRectConstraint);
                canvas->restore();

                canvas->translate(dstRect.width() + SK_Scalar1 * kPadX, 0);
                ++rowCount;
                if ((dstRect.width() + 2 * kPadX) * rowCount > gSize) {
                    canvas->restore();
                    canvas->translate(0, dstRect.height() + SK_Scalar1 * kPadY);
                    canvas->save();
                    rowCount = 0;
                }
            }
        }
        canvas->restore();
    }

private:
    bool                  fAA;
    SkAutoTUnref<SkImage> fImage;
    SkString              fName;

    typedef skiagm::GM INHERITED;
};

DEF_GM( return new DrawMiniBitmapRectGM(true); )
DEF_GM( return new DrawMiniBitmapRectGM(false); )

