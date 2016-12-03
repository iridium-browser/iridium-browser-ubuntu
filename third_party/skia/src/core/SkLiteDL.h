/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkLiteDL_DEFINED
#define SkLiteDL_DEFINED

#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkDrawable.h"
#include "SkRect.h"
#include "SkTDArray.h"

class SkLiteDL final : public SkDrawable {
public:
    static sk_sp<SkLiteDL> New(SkRect);
    void reset(SkRect);

    void makeThreadsafe();
    bool empty() const { return fUsed == 0; }

    void save();
    void saveLayer(const SkRect*, const SkPaint*, const SkImageFilter*, SkCanvas::SaveLayerFlags);
    void restore();

    void    concat (const SkMatrix&);
    void setMatrix (const SkMatrix&);
    void translate(SkScalar, SkScalar);
    void translateZ(SkScalar);

    void clipPath  (const   SkPath&, SkRegion::Op, bool aa);
    void clipRect  (const   SkRect&, SkRegion::Op, bool aa);
    void clipRRect (const  SkRRect&, SkRegion::Op, bool aa);
    void clipRegion(const SkRegion&, SkRegion::Op);

    void drawPaint (const SkPaint&);
    void drawPath  (const SkPath&, const SkPaint&);
    void drawRect  (const SkRect&, const SkPaint&);
    void drawOval  (const SkRect&, const SkPaint&);
    void drawArc   (const SkRect&, SkScalar, SkScalar, bool, const SkPaint&);
    void drawRRect (const SkRRect&, const SkPaint&);
    void drawDRRect(const SkRRect&, const SkRRect&, const SkPaint&);

    void drawAnnotation     (const SkRect&, const char*, SkData*);
    void drawDrawable       (SkDrawable*, const SkMatrix*);
    void drawPicture        (const SkPicture*, const SkMatrix*, const SkPaint*);
    void drawShadowedPicture(const SkPicture*, const SkMatrix*, const SkPaint*);

    void drawText       (const void*, size_t, SkScalar, SkScalar, const SkPaint&);
    void drawPosText    (const void*, size_t, const SkPoint[], const SkPaint&);
    void drawPosTextH   (const void*, size_t, const SkScalar[], SkScalar, const SkPaint&);
    void drawTextOnPath (const void*, size_t, const SkPath&, const SkMatrix*, const SkPaint&);
    void drawTextRSXform(const void*, size_t, const SkRSXform[], const SkRect*, const SkPaint&);
    void drawTextBlob   (const SkTextBlob*, SkScalar,SkScalar, const SkPaint&);

    void drawBitmap    (const SkBitmap&, SkScalar, SkScalar,            const SkPaint*);
    void drawBitmapNine(const SkBitmap&, const SkIRect&, const SkRect&, const SkPaint*);
    void drawBitmapRect(const SkBitmap&, const SkRect*,  const SkRect&, const SkPaint*,
                        SkCanvas::SrcRectConstraint);
    void drawBitmapLattice(const SkBitmap&, const SkCanvas::Lattice&, const SkRect&,
                           const SkPaint*);

    void drawImage    (const SkImage*, SkScalar,SkScalar,             const SkPaint*);
    void drawImageNine(const SkImage*, const SkIRect&, const SkRect&, const SkPaint*);
    void drawImageRect(const SkImage*, const SkRect*, const SkRect&,  const SkPaint*,
                       SkCanvas::SrcRectConstraint);
    void drawImageLattice(const SkImage*, const SkCanvas::Lattice&, const SkRect&, const SkPaint*);

    void drawPatch(const SkPoint[12], const SkColor[4], const SkPoint[4],
                   SkXfermode*, const SkPaint&);
    void drawPoints(SkCanvas::PointMode, size_t, const SkPoint[], const SkPaint&);
    void drawVertices(SkCanvas::VertexMode, int, const SkPoint[], const SkPoint[], const SkColor[],
                      SkXfermode*, const uint16_t[], int, const SkPaint&);
    void drawAtlas(const SkImage*, const SkRSXform[], const SkRect[], const SkColor[], int,
                   SkXfermode::Mode, const SkRect*, const SkPaint*);

private:
    SkLiteDL(SkRect);
    ~SkLiteDL();

    SkRect   onGetBounds() override;
    void onDraw(SkCanvas*) override;

    template <typename T, typename... Args>
    void* push(size_t, Args&&...);

    template <typename Fn, typename... Args>
    void map(const Fn[], Args...);

    SkAutoTMalloc<uint8_t> fBytes;
    size_t                 fUsed;
    size_t                 fReserved;
    SkRect                 fBounds;
};

#endif//SkLiteDL_DEFINED
