/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkImageGeneratorUtils.h"
#include "SkBitmap.h"

class GeneratorFromEmpty : public SkImageGenerator {
public:
    GeneratorFromEmpty(const SkImageInfo& info) : SkImageGenerator(info) {}
};

SkImageGenerator* SkImageGeneratorUtils::NewEmpty(const SkImageInfo& info) {
    return SkNEW_ARGS(GeneratorFromEmpty, (info));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class GeneratorFromBitmap : public SkImageGenerator {
public:
    GeneratorFromBitmap(const SkBitmap& bm) : SkImageGenerator(bm.info()), fBM(bm) {
        if (!bm.isImmutable()) {
            bm.copyTo(&fBM);
        }
    }

protected:
    bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                     SkPMColor*, int*) override {
        return fBM.readPixels(info, pixels, rowBytes, 0, 0);
    }

private:
    SkBitmap fBM;
};

SkImageGenerator* SkImageGeneratorUtils::NewFromBitmap(const SkBitmap& bm) {
    return SkNEW_ARGS(GeneratorFromBitmap, (bm));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU

#include "GrContext.h"
#include "GrTexture.h"
#include "SkGr.h"

class GeneratorFromTexture : public SkImageGenerator {
public:
    GeneratorFromTexture(GrContext* ctx, GrTexture* tex, const SkImageInfo& info)
        : SkImageGenerator(info), fCtx(ctx), fTexture(tex)
    {}

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
#endif

SkImageGenerator* SkImageGeneratorUtils::NewFromTexture(GrContext* ctx, GrTexture* tex) {
#if SK_SUPPORT_GPU
    if (ctx && tex) {
        const GrSurfaceDesc desc = tex->desc();

        SkColorType ct;
        SkColorProfileType cpt;
        if (!GrPixelConfig2ColorAndProfileType(desc.fConfig, &ct, &cpt)) {
            return nullptr;
        }
        const SkAlphaType at = kPremul_SkAlphaType; // take isOpaque from caller?
        SkImageInfo info = SkImageInfo::Make(desc.fWidth, desc.fHeight, ct, at, cpt);
        return SkNEW_ARGS(GeneratorFromTexture, (ctx, tex, info));
    }
#endif
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "SkImage.h"

class GeneratorFromImage : public SkImageGenerator {
public:
    GeneratorFromImage(const SkImage* image, const SkImageInfo& info)
        : SkImageGenerator(info), fImage(image) {}

protected:
    bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                     SkPMColor*, int*) override {
        return fImage->readPixels(info, pixels, rowBytes, 0, 0);
    }

    GrTexture* onGenerateTexture(GrContext* ctx, SkImageUsageType, const SkIRect* subset) override {
        // waiting on https://code.google.com/p/skia/issues/detail?id=4233
        return nullptr;
    }

private:
    SkAutoTUnref<const SkImage> fImage;
};

SkImageGenerator* SkImageGeneratorUtils::NewFromImage(const SkImage* image) {
    if (image) {
        const SkColorType ct = kN32_SkColorType;
        const SkAlphaType at = image->isOpaque() ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
        const SkImageInfo info = SkImageInfo::Make(image->width(), image->height(), ct, at);
        return SkNEW_ARGS(GeneratorFromImage, (image, info));
    }
    return nullptr;
}


