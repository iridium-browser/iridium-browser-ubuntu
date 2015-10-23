/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "SkCanvas.h"
#include "SkImage.h"
#include "SkImageCacherator.h"
#include "SkPictureRecorder.h"
#include "SkSurface.h"

#if SK_SUPPORT_GPU
#include "GrContext.h"
#include "GrTexture.h"
#include "../src/image/SkImage_Gpu.h"
#endif

static void draw_something(SkCanvas* canvas, const SkRect& bounds) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorRED);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(10);
    canvas->drawRect(bounds, paint);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SK_ColorBLUE);
    canvas->drawOval(bounds, paint);
}

/*
 *  Exercise drawing pictures inside an image, showing that the image version is pixelated
 *  (correctly) when it is inside an image.
 */
class ImagePictGM : public skiagm::GM {
    SkAutoTUnref<SkPicture> fPicture;
    SkAutoTUnref<SkImage>   fImage0;
    SkAutoTUnref<SkImage>   fImage1;
public:
    ImagePictGM() {}

protected:
    SkString onShortName() override {
        return SkString("image-picture");
    }

    SkISize onISize() override {
        return SkISize::Make(850, 450);
    }

    void onOnceBeforeDraw() override {
        const SkRect bounds = SkRect::MakeXYWH(100, 100, 100, 100);
        SkPictureRecorder recorder;
        draw_something(recorder.beginRecording(bounds), bounds);
        fPicture.reset(recorder.endRecording());

        // extract enough just for the oval.
        const SkISize size = SkISize::Make(100, 100);

        SkMatrix matrix;
        matrix.setTranslate(-100, -100);
        fImage0.reset(SkImage::NewFromPicture(fPicture, size, &matrix, nullptr));
        matrix.postTranslate(-50, -50);
        matrix.postRotate(45);
        matrix.postTranslate(50, 50);
        fImage1.reset(SkImage::NewFromPicture(fPicture, size, &matrix, nullptr));
    }

    void drawSet(SkCanvas* canvas) const {
        SkMatrix matrix = SkMatrix::MakeTrans(-100, -100);
        canvas->drawPicture(fPicture, &matrix, nullptr);
        canvas->drawImage(fImage0, 150, 0);
        canvas->drawImage(fImage1, 300, 0);
    }

    void onDraw(SkCanvas* canvas) override {
        canvas->translate(20, 20);

        this->drawSet(canvas);

        canvas->save();
        canvas->translate(0, 130);
        canvas->scale(0.25f, 0.25f);
        this->drawSet(canvas);
        canvas->restore();

        canvas->save();
        canvas->translate(0, 200);
        canvas->scale(2, 2);
        this->drawSet(canvas);
        canvas->restore();
    }

private:
    typedef skiagm::GM INHERITED;
};
DEF_GM( return new ImagePictGM; )

///////////////////////////////////////////////////////////////////////////////////////////////////

static SkImageGenerator* make_pic_generator(GrContext*, SkPicture* pic) {
    SkMatrix matrix;
    matrix.setTranslate(-100, -100);
    return SkImageGenerator::NewFromPicture(SkISize::Make(100, 100), pic, &matrix, nullptr);
}

class RasterGenerator : public SkImageGenerator {
public:
    RasterGenerator(const SkBitmap& bm) : SkImageGenerator(bm.info()), fBM(bm) {}
protected:
    bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                     SkPMColor*, int*) override {
        return fBM.readPixels(info, pixels, rowBytes, 0, 0);
    }
private:
    SkBitmap fBM;
};
static SkImageGenerator* make_ras_generator(GrContext*, SkPicture* pic) {
    SkBitmap bm;
    bm.allocN32Pixels(100, 100);
    SkCanvas canvas(bm);
    canvas.clear(0);
    canvas.translate(-100, -100);
    canvas.drawPicture(pic);
    return new RasterGenerator(bm);
}

class EmptyGenerator : public SkImageGenerator {
public:
    EmptyGenerator(const SkImageInfo& info) : SkImageGenerator(info) {}
};

#if SK_SUPPORT_GPU
class TextureGenerator : public SkImageGenerator {
public:
    TextureGenerator(GrContext* ctx, const SkImageInfo& info, SkPicture* pic)
        : SkImageGenerator(info)
        , fCtx(SkRef(ctx))
    {
        SkAutoTUnref<SkSurface> surface(SkSurface::NewRenderTarget(ctx, SkSurface::kNo_Budgeted,
                                                                   info, 0));
        surface->getCanvas()->clear(0);
        surface->getCanvas()->translate(-100, -100);
        surface->getCanvas()->drawPicture(pic);
        SkAutoTUnref<SkImage> image(surface->newImageSnapshot());
        fTexture.reset(SkRef(image->getTexture()));
    }
protected:
    GrTexture* onGenerateTexture(GrContext* ctx, SkImageUsageType, const SkIRect* subset) override {
        if (ctx) {
            SkASSERT(ctx == fCtx.get());
        }

        if (!subset) {
            return SkRef(fTexture.get());
        }
        // need to copy the subset into a new texture
        GrSurfaceDesc desc = fTexture->desc();
        desc.fWidth = subset->width();
        desc.fHeight = subset->height();

        GrTexture* dst = fCtx->textureProvider()->createTexture(desc, false);
        fCtx->copySurface(dst, fTexture, *subset, SkIPoint::Make(0, 0));
        return dst;
    }
private:
    SkAutoTUnref<GrContext> fCtx;
    SkAutoTUnref<GrTexture> fTexture;
};
static SkImageGenerator* make_tex_generator(GrContext* ctx, SkPicture* pic) {
    const SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);

    if (!ctx) {
        return new EmptyGenerator(info);
    }
    return new TextureGenerator(ctx, info, pic);
}
#endif

class ImageCacheratorGM : public skiagm::GM {
    SkString                         fName;
    SkImageGenerator*                (*fFactory)(GrContext*, SkPicture*);
    SkAutoTUnref<SkPicture>          fPicture;
    SkAutoTDelete<SkImageCacherator> fCache;
    SkAutoTDelete<SkImageCacherator> fCacheSubset;

public:
    ImageCacheratorGM(const char suffix[], SkImageGenerator* (*factory)(GrContext*, SkPicture*))
        : fFactory(factory)
    {
        fName.printf("image-cacherator-from-%s", suffix);
    }

protected:
    SkString onShortName() override {
        return fName;
    }

    SkISize onISize() override {
        return SkISize::Make(960, 450);
    }

    void onOnceBeforeDraw() override {
        const SkRect bounds = SkRect::MakeXYWH(100, 100, 100, 100);
        SkPictureRecorder recorder;
        draw_something(recorder.beginRecording(bounds), bounds);
        fPicture.reset(recorder.endRecording());
    }

    void makeCaches(GrContext* ctx) {
        auto gen = fFactory(ctx, fPicture);
        SkDEBUGCODE(const uint32_t genID = gen->uniqueID();)
        fCache.reset(SkImageCacherator::NewFromGenerator(gen));

        const SkIRect subset = SkIRect::MakeLTRB(50, 50, 100, 100);

        gen = fFactory(ctx, fPicture);
        SkDEBUGCODE(const uint32_t genSubsetID = gen->uniqueID();)
        fCacheSubset.reset(SkImageCacherator::NewFromGenerator(gen, &subset));

        // whole caches should have the same ID as the generator. Subsets should be diff
        SkASSERT(fCache->uniqueID() == genID);
        SkASSERT(fCacheSubset->uniqueID() != genID);
        SkASSERT(fCacheSubset->uniqueID() != genSubsetID);

        SkASSERT(fCache->info().dimensions() == SkISize::Make(100, 100));
        SkASSERT(fCacheSubset->info().dimensions() == SkISize::Make(50, 50));
    }

    static void draw_as_bitmap(SkCanvas* canvas, SkImageCacherator* cache, SkScalar x, SkScalar y) {
        SkBitmap bitmap;
        cache->lockAsBitmap(&bitmap);
        canvas->drawBitmap(bitmap, x, y);
    }

    static void draw_as_tex(SkCanvas* canvas, SkImageCacherator* cache, SkScalar x, SkScalar y) {
#if SK_SUPPORT_GPU
        SkAutoTUnref<GrTexture> texture(cache->lockAsTexture(canvas->getGrContext(),
                                                             kUntiled_SkImageUsageType));
        if (!texture) {
            // show placeholder if we have no texture
            SkPaint paint;
            paint.setStyle(SkPaint::kStroke_Style);
            SkRect r = SkRect::MakeXYWH(x, y, SkIntToScalar(cache->info().width()),
                                        SkIntToScalar(cache->info().width()));
            canvas->drawRect(r, paint);
            canvas->drawLine(r.left(), r.top(), r.right(), r.bottom(), paint);
            canvas->drawLine(r.left(), r.bottom(), r.right(), r.top(), paint);
            return;
        }
        // No API to draw a GrTexture directly, so we cheat and create a private image subclass
        SkAutoTUnref<SkImage> image(new SkImage_Gpu(cache->info().width(), cache->info().height(),
                                                    cache->uniqueID(), kPremul_SkAlphaType, texture,
                                                    0, SkSurface::kNo_Budgeted));
        canvas->drawImage(image, x, y);
#endif
    }

    void drawSet(SkCanvas* canvas) const {
        SkMatrix matrix = SkMatrix::MakeTrans(-100, -100);
        canvas->drawPicture(fPicture, &matrix, nullptr);

        // Draw the tex first, so it doesn't hit a lucky cache from the raster version. This
        // way we also can force the generateTexture call.

        draw_as_tex(canvas, fCache, 310, 0);
        draw_as_tex(canvas, fCacheSubset, 310+101, 0);

        draw_as_bitmap(canvas, fCache, 150, 0);
        draw_as_bitmap(canvas, fCacheSubset, 150+101, 0);
    }

    void onDraw(SkCanvas* canvas) override {
        this->makeCaches(canvas->getGrContext());

        canvas->translate(20, 20);

        this->drawSet(canvas);

        canvas->save();
        canvas->translate(0, 130);
        canvas->scale(0.25f, 0.25f);
        this->drawSet(canvas);
        canvas->restore();

        canvas->save();
        canvas->translate(0, 200);
        canvas->scale(2, 2);
        this->drawSet(canvas);
        canvas->restore();
    }

private:
    typedef skiagm::GM INHERITED;
};
DEF_GM( return new ImageCacheratorGM("picture", make_pic_generator); )
DEF_GM( return new ImageCacheratorGM("raster", make_ras_generator); )
#if SK_SUPPORT_GPU
    DEF_GM( return new ImageCacheratorGM("texture", make_tex_generator); )
#endif



