/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkAndroidSDKCanvas.h"

#include "SkColorFilter.h"
#include "SkDrawLooper.h"
#include "SkImageFilter.h"
#include "SkPaint.h"
#include "SkPathEffect.h"
#include "SkShader.h"
#include "SkSurface.h"
#include "SkTLazy.h"

namespace {

/** Discard SkShaders not exposed by the Android Java API. */

void CheckShader(SkPaint* paint) {
    SkShader* shader = paint->getShader();
    if (!shader) {
        return;
    }

    if (shader->isAImage()) {
        return;
    }
    if (shader->asACompose(nullptr)) {
        return;
    }
    SkShader::GradientType gtype = shader->asAGradient(nullptr);
    if (gtype == SkShader::kLinear_GradientType ||
        gtype == SkShader::kRadial_GradientType ||
        gtype == SkShader::kSweep_GradientType) {
        return;
    }
    paint->setShader(nullptr);
}

void Filter(SkPaint* paint) {

    uint32_t flags = paint->getFlags();
    flags &= ~SkPaint::kLCDRenderText_Flag;
    paint->setFlags(flags);

    // Android doesn't support blend modes above kLighten_Mode
    if (paint->getBlendMode() > SkBlendMode::kLighten) {
        paint->setBlendMode(SkBlendMode::kSrcOver);
    }

    // Force bilinear scaling or none
    if (paint->getFilterQuality() != kNone_SkFilterQuality) {
        paint->setFilterQuality(kLow_SkFilterQuality);
    }

    CheckShader(paint);

    // Android SDK only supports mode & matrix color filters
    // (and, again, no modes above kLighten_Mode).
    SkColorFilter* cf = paint->getColorFilter();
    if (cf) {
        SkColor color;
        SkBlendMode mode;
        SkScalar srcColorMatrix[20];
        bool isMode = cf->asColorMode(&color, &mode);
        if (isMode && (int)mode > (int)SkBlendMode::kLighten) {
            paint->setColorFilter(
                SkColorFilter::MakeModeFilter(color, SkBlendMode::kSrcOver));
        } else if (!isMode && !cf->asColorMatrix(srcColorMatrix)) {
            paint->setColorFilter(nullptr);
        }
    }

#ifdef SK_BUILD_FOR_ANDROID_FRAMEWORK
    SkPathEffect* pe = paint->getPathEffect();
    if (pe && !pe->exposedInAndroidJavaAPI()) {
        paint->setPathEffect(nullptr);
    }
#endif

    // TODO: Android doesn't support all the flags that can be passed to
    // blur filters; we need plumbing to get them out.

    paint->setImageFilter(nullptr);
    paint->setLooper(nullptr);
};

}  // namespace

#define FILTER(p)             \
    SkPaint filteredPaint(p); \
    Filter(&filteredPaint);

#define FILTER_PTR(p)                          \
    SkTLazy<SkPaint> lazyPaint;                \
    SkPaint* filteredPaint = (SkPaint*) p;     \
    if (p) {                                   \
        filteredPaint = lazyPaint.set(*p);     \
        Filter(filteredPaint);                 \
    }


SkAndroidSDKCanvas::SkAndroidSDKCanvas() : fProxyTarget(nullptr) { }

void SkAndroidSDKCanvas::reset(SkCanvas* newTarget) { fProxyTarget = newTarget; }

void SkAndroidSDKCanvas::onDrawPaint(const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawPaint(filteredPaint);
}
void SkAndroidSDKCanvas::onDrawPoints(PointMode pMode,
                                               size_t count,
                                               const SkPoint pts[],
                                               const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawPoints(pMode, count, pts, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawOval(const SkRect& r, const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawOval(r, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawArc(const SkRect& r, SkScalar startAngle, SkScalar sweepAngle,
                                   bool useCenter, const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawArc(r, startAngle, sweepAngle, useCenter, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawRect(const SkRect& r, const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawRect(r, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawRRect(const SkRRect& r, const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawRRect(r, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawPath(const SkPath& path, const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawPath(path, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawBitmap(const SkBitmap& bitmap,
                                               SkScalar left,
                                               SkScalar top,
                                               const SkPaint* paint) {
    FILTER_PTR(paint);
    fProxyTarget->drawBitmap(bitmap, left, top, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawBitmapRect(const SkBitmap& bitmap,
                                                   const SkRect* src,
                                                   const SkRect& dst,
                                                   const SkPaint* paint,
                                                   SkCanvas::SrcRectConstraint constraint) {
    FILTER_PTR(paint);
    fProxyTarget->legacy_drawBitmapRect(bitmap, src, dst, filteredPaint, constraint);
}
void SkAndroidSDKCanvas::onDrawBitmapNine(const SkBitmap& bitmap,
                                                   const SkIRect& center,
                                                   const SkRect& dst,
                                                   const SkPaint* paint) {
    FILTER_PTR(paint);
    fProxyTarget->drawBitmapNine(bitmap, center, dst, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawVertices(VertexMode vMode,
                                                 int vertexCount,
                                                 const SkPoint vertices[],
                    const SkPoint texs[], const SkColor colors[], SkBlendMode bmode,
                    const uint16_t indices[], int indexCount,
                    const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawVertices(vMode, vertexCount, vertices, texs, colors,
                               bmode, indices, indexCount, filteredPaint);
}

void SkAndroidSDKCanvas::onDrawDRRect(const SkRRect& outer,
                                               const SkRRect& inner,
                                               const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawDRRect(outer, inner, filteredPaint);
}

void SkAndroidSDKCanvas::onDrawText(const void* text,
                                             size_t byteLength,
                                             SkScalar x,
                                             SkScalar y,
                                             const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawText(text, byteLength, x, y, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawPosText(const void* text,
                                                size_t byteLength,
                                                const SkPoint pos[],
                                                const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawPosText(text, byteLength, pos, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawPosTextH(const void* text,
                                                 size_t byteLength,
                                                 const SkScalar xpos[],
                                                 SkScalar constY,
                                                 const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawPosTextH(text, byteLength, xpos, constY, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawTextOnPath(const void* text,
                                          size_t byteLength,
                                          const SkPath& path,
                                          const SkMatrix* matrix,
                                          const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawTextOnPath(text, byteLength, path, matrix, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawTextRSXform(const void* text, size_t byteLength,
                                           const SkRSXform xform[], const SkRect* cull,
                                           const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawTextRSXform(text, byteLength, xform, cull, filteredPaint);
}
void SkAndroidSDKCanvas::onDrawTextBlob(const SkTextBlob* blob,
                                                 SkScalar x,
                                                 SkScalar y,
                                                 const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawTextBlob(blob, x, y, filteredPaint);
}

void SkAndroidSDKCanvas::onDrawPatch(const SkPoint cubics[12],
                                              const SkColor colors[4],
                                              const SkPoint texCoords[4],
                                              SkBlendMode bmode,
                                              const SkPaint& paint) {
    FILTER(paint);
    fProxyTarget->drawPatch(cubics, colors, texCoords, bmode, filteredPaint);
}


void SkAndroidSDKCanvas::onDrawImage(const SkImage* image,
                                              SkScalar x,
                                              SkScalar y,
                                              const SkPaint* paint) {
    FILTER_PTR(paint);
    fProxyTarget->drawImage(image, x, y, filteredPaint);
}

void SkAndroidSDKCanvas::onDrawImageRect(const SkImage* image,
                                         const SkRect* in,
                                         const SkRect& out,
                                         const SkPaint* paint,
                                         SrcRectConstraint constraint) {
    FILTER_PTR(paint);
    fProxyTarget->legacy_drawImageRect(image, in, out, filteredPaint, constraint);
}

void SkAndroidSDKCanvas::onDrawPicture(const SkPicture* picture,
                                       const SkMatrix* matrix,
                                       const SkPaint* paint) {
    FILTER_PTR(paint);
    fProxyTarget->drawPicture(picture, matrix, filteredPaint);
}

void SkAndroidSDKCanvas::onDrawAtlas(const SkImage* atlas,
                                     const SkRSXform xform[],
                                     const SkRect tex[],
                                     const SkColor colors[],
                                     int count,
                                     SkBlendMode mode,
                                     const SkRect* cullRect,
                                     const SkPaint* paint) {
    FILTER_PTR(paint);
    fProxyTarget->drawAtlas(atlas, xform, tex, colors, count, mode, cullRect, filteredPaint);
}

void SkAndroidSDKCanvas::onDrawImageNine(const SkImage* image,
                                         const SkIRect& center,
                                         const SkRect& dst,
                                         const SkPaint* paint) {
    FILTER_PTR(paint);
    fProxyTarget->drawImageNine(image, center, dst, filteredPaint);
}


void SkAndroidSDKCanvas::onDrawDrawable(SkDrawable* drawable, const SkMatrix* matrix) {
    fProxyTarget->drawDrawable(drawable, matrix);
}

SkISize SkAndroidSDKCanvas::getBaseLayerSize() const {
    return fProxyTarget->getBaseLayerSize();
}
bool SkAndroidSDKCanvas::getClipBounds(SkRect* rect) const {
    return fProxyTarget->getClipBounds(rect);
}
bool SkAndroidSDKCanvas::getClipDeviceBounds(SkIRect* rect) const {
    return fProxyTarget->getClipDeviceBounds(rect);
}

bool SkAndroidSDKCanvas::isClipEmpty() const { return fProxyTarget->isClipEmpty(); }
bool SkAndroidSDKCanvas::isClipRect() const { return fProxyTarget->isClipRect(); }

sk_sp<SkSurface> SkAndroidSDKCanvas::onNewSurface(const SkImageInfo& info,
                                                  const SkSurfaceProps& props) {
    return fProxyTarget->makeSurface(info, &props);
}

bool SkAndroidSDKCanvas::onPeekPixels(SkPixmap* pmap) {
    return fProxyTarget->peekPixels(pmap);
}

bool SkAndroidSDKCanvas::onAccessTopLayerPixels(SkPixmap* pmap) {
    SkASSERT(pmap);
    SkImageInfo info;
    size_t rowBytes;
    const void* addr = fProxyTarget->accessTopLayerPixels(&info, &rowBytes, nullptr);
    if (addr) {
        pmap->reset(info, addr, rowBytes);
        return true;
    }
    return false;
}

void SkAndroidSDKCanvas::willSave() {
    fProxyTarget->save();
}

SkCanvas::SaveLayerStrategy SkAndroidSDKCanvas::getSaveLayerStrategy(const SaveLayerRec& rec) {
    fProxyTarget->saveLayer(rec);
    return SkCanvas::kNoLayer_SaveLayerStrategy;
}

void SkAndroidSDKCanvas::willRestore() {
    fProxyTarget->restore();
}

void SkAndroidSDKCanvas::didRestore() { }

void SkAndroidSDKCanvas::didConcat(const SkMatrix& m) {
    fProxyTarget->concat(m);
}

void SkAndroidSDKCanvas::didSetMatrix(const SkMatrix& m) {
    fProxyTarget->setMatrix(m);
}

void SkAndroidSDKCanvas::onClipRect(const SkRect& rect, SkClipOp op, ClipEdgeStyle style) {
    fProxyTarget->clipRect(rect, op, style);
}

void SkAndroidSDKCanvas::onClipRRect(const SkRRect& rrect, SkClipOp op, ClipEdgeStyle style) {
    fProxyTarget->clipRRect(rrect, op, style);
}

void SkAndroidSDKCanvas::onClipPath(const SkPath& path, SkClipOp op, ClipEdgeStyle style) {
    fProxyTarget->clipPath(path, op, style);
}

void SkAndroidSDKCanvas::onClipRegion(const SkRegion& region, SkClipOp op) {
    fProxyTarget->clipRegion(region, op);
}

void SkAndroidSDKCanvas::onDiscard() { fProxyTarget->discard(); }
