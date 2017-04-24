/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkCanvas.h"
#include "SkColorFilter.h"
#include "SkGradientShader.h"
#include "SkLocalMatrixShader.h"
#include "SkRandom.h"
#include "SkVertices.h"

static constexpr SkScalar kShaderSize = 40;
static sk_sp<SkShader> make_shader1(SkScalar shaderScale) {
    const SkColor colors[] = {
        SK_ColorRED, SK_ColorCYAN, SK_ColorGREEN, SK_ColorWHITE,
        SK_ColorMAGENTA, SK_ColorBLUE, SK_ColorYELLOW,
    };
    const SkPoint pts[] = {{kShaderSize / 4, 0}, {3 * kShaderSize / 4, kShaderSize}};
    const SkMatrix localMatrix = SkMatrix::MakeScale(shaderScale, shaderScale);

    sk_sp<SkShader> grad = SkGradientShader::MakeLinear(pts, colors, nullptr,
                                                        SK_ARRAY_COUNT(colors),
                                                        SkShader::kMirror_TileMode, 0,
                                                        &localMatrix);
    // Throw in a couple of local matrix wrappers for good measure.
    return shaderScale == 1
        ? grad
        : sk_make_sp<SkLocalMatrixShader>(
              sk_make_sp<SkLocalMatrixShader>(std::move(grad), SkMatrix::MakeTrans(-10, 0)),
              SkMatrix::MakeTrans(10, 0));
}

static sk_sp<SkShader> make_shader2() {
    return SkShader::MakeColorShader(SK_ColorBLUE);
}

static sk_sp<SkColorFilter> make_color_filter() {
    return SkColorFilter::MakeModeFilter(0xFFAABBCC, SkBlendMode::kDarken);
}

static constexpr SkScalar kMeshSize = 30;

// start with the center of a 3x3 grid of vertices.
static constexpr uint16_t kMeshFan[] = {
        4,
        0, 1, 2, 5, 8, 7, 6, 3, 0
};

static const int kMeshIndexCnt = (int)SK_ARRAY_COUNT(kMeshFan);
static const int kMeshVertexCnt = 9;

static void fill_mesh(SkPoint pts[kMeshVertexCnt], SkPoint texs[kMeshVertexCnt],
                      SkColor colors[kMeshVertexCnt], SkScalar shaderScale) {
    pts[0].set(0, 0);
    pts[1].set(kMeshSize / 2, 3);
    pts[2].set(kMeshSize, 0);
    pts[3].set(3, kMeshSize / 2);
    pts[4].set(kMeshSize / 2, kMeshSize / 2);
    pts[5].set(kMeshSize - 3, kMeshSize / 2);
    pts[6].set(0, kMeshSize);
    pts[7].set(kMeshSize / 2, kMeshSize - 3);
    pts[8].set(kMeshSize, kMeshSize);

    const auto shaderSize = kShaderSize * shaderScale;
    texs[0].set(0, 0);
    texs[1].set(shaderSize / 2, 0);
    texs[2].set(shaderSize, 0);
    texs[3].set(0, shaderSize / 2);
    texs[4].set(shaderSize / 2, shaderSize / 2);
    texs[5].set(shaderSize, shaderSize / 2);
    texs[6].set(0, shaderSize);
    texs[7].set(shaderSize / 2, shaderSize);
    texs[8].set(shaderSize, shaderSize);

    SkRandom rand;
    for (size_t i = 0; i < kMeshVertexCnt; ++i) {
        colors[i] = rand.nextU() | 0xFF000000;
    }
}

class VerticesGM : public skiagm::GM {
    SkPoint                 fPts[kMeshVertexCnt];
    SkPoint                 fTexs[kMeshVertexCnt];
    SkColor                 fColors[kMeshVertexCnt];
    sk_sp<SkShader>         fShader1;
    sk_sp<SkShader>         fShader2;
    sk_sp<SkColorFilter>    fColorFilter;
    sk_sp<SkVertices>       fVertices;
    bool                    fUseObject;
    SkScalar                fShaderScale;

public:
    VerticesGM(bool useObject, SkScalar shaderScale = 1)
        : fUseObject(useObject), fShaderScale(shaderScale) {}

protected:

    void onOnceBeforeDraw() override {
        fill_mesh(fPts, fTexs, fColors, fShaderScale);
        fShader1 = make_shader1(fShaderScale);
        fShader2 = make_shader2();
        fColorFilter = make_color_filter();
        if (fUseObject) {
            std::unique_ptr<SkPoint[]> points(new SkPoint[kMeshVertexCnt]);
            std::unique_ptr<SkPoint[]> texs(new SkPoint[kMeshVertexCnt]);
            std::unique_ptr<SkColor[]> colors(new SkColor[kMeshVertexCnt]);
            std::unique_ptr<uint16_t[]> indices(new uint16_t[kMeshIndexCnt]);
            memcpy(points.get(), fPts, sizeof(SkPoint) * kMeshVertexCnt);
            memcpy(colors.get(), fColors, sizeof(SkColor) * kMeshVertexCnt);
            memcpy(texs.get(), fTexs, sizeof(SkPoint) * kMeshVertexCnt);
            memcpy(indices.get(), kMeshFan, sizeof(uint16_t) * kMeshIndexCnt);
            // Older libstdc++ does not allow moving a std::unique_ptr<T[]> into a
            // std::unique_ptr<const T[]>. Hence the release() calls below.
            fVertices = SkVertices::MakeIndexed(
                    SkCanvas::kTriangleFan_VertexMode,
                    std::unique_ptr<const SkPoint[]>((const SkPoint*)points.release()),
                    std::unique_ptr<const SkColor[]>((const SkColor*)colors.release()),
                    std::unique_ptr<const SkPoint[]>((const SkPoint*)texs.release()),
                    kMeshVertexCnt,
                    std::unique_ptr<const uint16_t[]>((const uint16_t*)indices.release()),
                    kMeshIndexCnt);
        }
    }

    SkString onShortName() override {
        SkString name("vertices");
        if (fUseObject) {
            name.append("_object");
        }
        if (fShaderScale != 1) {
            name.append("_scaled_shader");
        }
        return name;
    }

    SkISize onISize() override {
        return SkISize::Make(975, 1175);
    }

    void onDraw(SkCanvas* canvas) override {
        const SkBlendMode modes[] = {
            SkBlendMode::kClear,
            SkBlendMode::kSrc,
            SkBlendMode::kDst,
            SkBlendMode::kSrcOver,
            SkBlendMode::kDstOver,
            SkBlendMode::kSrcIn,
            SkBlendMode::kDstIn,
            SkBlendMode::kSrcOut,
            SkBlendMode::kDstOut,
            SkBlendMode::kSrcATop,
            SkBlendMode::kDstATop,
            SkBlendMode::kXor,
            SkBlendMode::kPlus,
            SkBlendMode::kModulate,
            SkBlendMode::kScreen,
            SkBlendMode::kOverlay,
            SkBlendMode::kDarken,
            SkBlendMode::kLighten,
            SkBlendMode::kColorDodge,
            SkBlendMode::kColorBurn,
            SkBlendMode::kHardLight,
            SkBlendMode::kSoftLight,
            SkBlendMode::kDifference,
            SkBlendMode::kExclusion,
            SkBlendMode::kMultiply,
            SkBlendMode::kHue,
            SkBlendMode::kSaturation,
            SkBlendMode::kColor,
            SkBlendMode::kLuminosity,
        };

        SkPaint paint;

        canvas->translate(4, 4);
        int x = 0;
        for (auto mode : modes) {
            canvas->save();
            for (uint8_t alpha : {0xFF, 0x80}) {
                for (const auto& cf : {sk_sp<SkColorFilter>(nullptr), fColorFilter}) {
                    for (const auto& shader : {fShader1, fShader2}) {
                        static constexpr struct {
                            bool fHasColors;
                            bool fHasTexs;
                        } kAttrs[] = {{true, false}, {false, true}, {true, true}};
                        for (auto attrs : kAttrs) {
                            paint.setShader(shader);
                            paint.setColorFilter(cf);
                            paint.setAlpha(alpha);
                            if (fUseObject) {
                                uint32_t flags = 0;
                                flags |=
                                        attrs.fHasColors ? 0 : SkCanvas::kIgnoreColors_VerticesFlag;
                                flags |= attrs.fHasTexs ? 0
                                                        : SkCanvas::kIgnoreTexCoords_VerticesFlag;
                                canvas->drawVertices(fVertices, mode, paint, flags);
                            } else {
                                const SkColor* colors = attrs.fHasColors ? fColors : nullptr;
                                const SkPoint* texs = attrs.fHasTexs ? fTexs : nullptr;
                                canvas->drawVertices(SkCanvas::kTriangleFan_VertexMode,
                                                     kMeshVertexCnt, fPts, texs, colors, mode,
                                                     kMeshFan, kMeshIndexCnt, paint);
                            }
                            canvas->translate(40, 0);
                            ++x;
                        }
                    }
                }
            }
            canvas->restore();
            canvas->translate(0, 40);
        }
    }

private:
    typedef skiagm::GM INHERITED;
};

/////////////////////////////////////////////////////////////////////////////////////

DEF_GM(return new VerticesGM(true);)
DEF_GM(return new VerticesGM(false);)
DEF_GM(return new VerticesGM(false, 1 / kShaderSize);)

static void draw_batching(SkCanvas* canvas, bool useObject) {
    std::unique_ptr<SkPoint[]> pts(new SkPoint[kMeshVertexCnt]);
    std::unique_ptr<SkPoint[]> texs(new SkPoint[kMeshVertexCnt]);
    std::unique_ptr<SkColor[]> colors(new SkColor[kMeshVertexCnt]);
    fill_mesh(pts.get(), texs.get(), colors.get(), 1);

    SkTDArray<SkMatrix> matrices;
    matrices.push()->reset();
    matrices.push()->setTranslate(0, 40);
    SkMatrix* m = matrices.push();
    m->setRotate(45, kMeshSize / 2, kMeshSize / 2);
    m->postScale(1.2f, .8f, kMeshSize / 2, kMeshSize / 2);
    m->postTranslate(0, 80);

    auto shader = make_shader1(1);

    // Triangle fans can't batch so we convert to regular triangles,
    static constexpr int kNumTris = kMeshIndexCnt - 2;
    std::unique_ptr<uint16_t[]> indices(new uint16_t[3 * kNumTris]);
    for (size_t i = 0; i < kNumTris; ++i) {
        indices[3 * i] = kMeshFan[0];
        indices[3 * i + 1] = kMeshFan[i + 1];
        indices[3 * i + 2] = kMeshFan[i + 2];
    }

    sk_sp<SkVertices> vertices;
    if (useObject) {
        // Older libstdc++ does not allow moving a std::unique_ptr<T[]> into a
        // std::unique_ptr<const T[]>. Hence the release() calls below.
        vertices = SkVertices::MakeIndexed(
                SkCanvas::kTriangles_VertexMode,
                std::unique_ptr<const SkPoint[]>((const SkPoint*)pts.release()),
                std::unique_ptr<const SkColor[]>((const SkColor*)colors.release()),
                std::unique_ptr<const SkPoint[]>((const SkPoint*)texs.release()), kMeshVertexCnt,
                std::unique_ptr<const uint16_t[]>((const uint16_t*)indices.release()),
                3 * kNumTris);
    }
    canvas->save();
    canvas->translate(10, 10);
    for (bool useShader : {false, true}) {
        for (bool useTex : {false, true}) {
            for (const auto& m : matrices) {
                canvas->save();
                canvas->concat(m);
                SkPaint paint;
                paint.setShader(useShader ? shader : nullptr);
                if (useObject) {
                    uint32_t flags = useTex ? 0 : SkCanvas::kIgnoreTexCoords_VerticesFlag;
                    canvas->drawVertices(vertices, SkBlendMode::kModulate, paint, flags);
                } else {
                    const SkPoint* t = useTex ? texs.get() : nullptr;
                    canvas->drawVertices(SkCanvas::kTriangles_VertexMode, kMeshVertexCnt, pts.get(),
                                         t, colors.get(), indices.get(), kNumTris * 3, paint);
                }
                canvas->restore();
            }
            canvas->translate(0, 120);
        }
    }
    canvas->restore();
}

// This test exists to exercise batching in the gpu backend.
DEF_SIMPLE_GM(vertices_batching, canvas, 100, 500) {
    draw_batching(canvas, false);
    canvas->translate(50, 0);
    draw_batching(canvas, true);
}
