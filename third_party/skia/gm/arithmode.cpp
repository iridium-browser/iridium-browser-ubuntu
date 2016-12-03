/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkShader.h"

#include "SkArithmeticMode.h"
#include "SkGradientShader.h"
#define WW  100
#define HH  32

static SkBitmap make_bm() {
    SkBitmap bm;
    bm.allocN32Pixels(WW, HH);
    bm.eraseColor(SK_ColorTRANSPARENT);
    return bm;
}

static SkBitmap make_src() {
    SkBitmap bm = make_bm();
    SkCanvas canvas(bm);
    SkPaint paint;
    SkPoint pts[] = { {0, 0}, {SkIntToScalar(WW), SkIntToScalar(HH)} };
    SkColor colors[] = {
        SK_ColorTRANSPARENT, SK_ColorGREEN, SK_ColorCYAN,
        SK_ColorRED, SK_ColorMAGENTA, SK_ColorWHITE,
    };
    paint.setShader(SkGradientShader::MakeLinear(pts, colors, nullptr, SK_ARRAY_COUNT(colors),
                                                 SkShader::kClamp_TileMode));
    canvas.drawPaint(paint);
    return bm;
}

static SkBitmap make_dst() {
    SkBitmap bm = make_bm();
    SkCanvas canvas(bm);
    SkPaint paint;
    SkPoint pts[] = { {0, SkIntToScalar(HH)}, {SkIntToScalar(WW), 0} };
    SkColor colors[] = {
        SK_ColorBLUE, SK_ColorYELLOW, SK_ColorBLACK, SK_ColorGREEN,
        sk_tool_utils::color_to_565(SK_ColorGRAY)
    };
    paint.setShader(SkGradientShader::MakeLinear(pts, colors, nullptr, SK_ARRAY_COUNT(colors),
                                                 SkShader::kClamp_TileMode));
    canvas.drawPaint(paint);
    return bm;
}

static void show_k_text(SkCanvas* canvas, SkScalar x, SkScalar y, const SkScalar k[]) {
    SkPaint paint;
    paint.setTextSize(SkIntToScalar(24));
    paint.setAntiAlias(true);
    sk_tool_utils::set_portable_typeface(&paint);
    for (int i = 0; i < 4; ++i) {
        SkString str;
        str.appendScalar(k[i]);
        SkScalar width = paint.measureText(str.c_str(), str.size());
        canvas->drawText(str.c_str(), str.size(), x, y + paint.getTextSize(), paint);
        x += width + SkIntToScalar(10);
    }
}

class ArithmodeGM : public skiagm::GM {
public:
    ArithmodeGM () {}

protected:

    virtual SkString onShortName() {
        return SkString("arithmode");
    }

    virtual SkISize onISize() { return SkISize::Make(640, 572); }

    virtual void onDraw(SkCanvas* canvas) {
        SkBitmap src = make_src();
        SkBitmap dst = make_dst();

        const SkScalar one = SK_Scalar1;
        static const SkScalar K[] = {
            0, 0, 0, 0,
            0, 0, 0, one,
            0, one, 0, 0,
            0, 0, one, 0,
            0, one, one, 0,
            0, one, -one, 0,
            0, one/2, one/2, 0,
            0, one/2, one/2, one/4,
            0, one/2, one/2, -one/4,
            one/4, one/2, one/2, 0,
            -one/4, one/2, one/2, 0,
        };

        const SkScalar* k = K;
        const SkScalar* stop = k + SK_ARRAY_COUNT(K);
        SkScalar y = 0;
        SkScalar gap = SkIntToScalar(src.width() + 20);
        while (k < stop) {
            SkScalar x = 0;
            canvas->drawBitmap(src, x, y, nullptr);
            x += gap;
            canvas->drawBitmap(dst, x, y, nullptr);
            x += gap;
            SkRect rect = SkRect::MakeXYWH(x, y, SkIntToScalar(WW), SkIntToScalar(HH));
            canvas->saveLayer(&rect, nullptr);
            canvas->drawBitmap(dst, x, y, nullptr);
            SkPaint paint;
            paint.setXfermode(SkArithmeticMode::Make(k[0], k[1], k[2], k[3]));
            canvas->drawBitmap(src, x, y, &paint);
            canvas->restore();
            x += gap;
            show_k_text(canvas, x, y, k);
            k += 4;
            y += SkIntToScalar(src.height() + 12);
        }

        // Draw two special cases to test enforcePMColor. In these cases, we
        // draw the dst bitmap twice, the first time it is halved and inverted,
        // leading to invalid premultiplied colors. If we enforcePMColor, these
        // invalid values should be clamped, and will not contribute to the
        // second draw.
        for (int i = 0; i < 2; i++) {
            const bool enforcePMColor = (i == 0);
            SkScalar x = gap;
            canvas->drawBitmap(dst, x, y, nullptr);
            x += gap;
            SkRect rect = SkRect::MakeXYWH(x, y, SkIntToScalar(WW), SkIntToScalar(HH));
            canvas->saveLayer(&rect, nullptr);
            SkPaint paint1;
            paint1.setXfermode(SkArithmeticMode::Make(0, -one / 2, 0, 1, enforcePMColor));
            canvas->drawBitmap(dst, x, y, &paint1);
            SkPaint paint2;
            paint2.setXfermode(SkArithmeticMode::Make(0, one / 2, -one, 1));
            canvas->drawBitmap(dst, x, y, &paint2);
            canvas->restore();
            x += gap;

            // Label
            SkPaint paint;
            paint.setTextSize(SkIntToScalar(24));
            paint.setAntiAlias(true);
            sk_tool_utils::set_portable_typeface(&paint);
            SkString str(enforcePMColor ? "enforcePM" : "no enforcePM");
            canvas->drawText(str.c_str(), str.size(), x, y + paint.getTextSize(), paint);

            y += SkIntToScalar(src.height() + 12);
        }
    }

private:
    typedef GM INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

DEF_GM( return new ArithmodeGM; )
