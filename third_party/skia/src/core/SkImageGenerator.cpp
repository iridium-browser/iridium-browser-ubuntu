/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkImage.h"
#include "SkImageGenerator.h"
#include "SkNextID.h"

SkImageGenerator::SkImageGenerator(const SkImageInfo& info, uint32_t uniqueID)
    : fInfo(info)
    , fUniqueID(kNeedNewImageUniqueID == uniqueID ? SkNextID::ImageID() : uniqueID)
{}

bool SkImageGenerator::getPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                                 SkPMColor ctable[], int* ctableCount) {
    if (kUnknown_SkColorType == info.colorType()) {
        return false;
    }
    if (nullptr == pixels) {
        return false;
    }
    if (rowBytes < info.minRowBytes()) {
        return false;
    }

    if (kIndex_8_SkColorType == info.colorType()) {
        if (nullptr == ctable || nullptr == ctableCount) {
            return false;
        }
    } else {
        if (ctableCount) {
            *ctableCount = 0;
        }
        ctableCount = nullptr;
        ctable = nullptr;
    }

    const bool success = this->onGetPixels(info, pixels, rowBytes, ctable, ctableCount);
    if (success && ctableCount) {
        SkASSERT(*ctableCount >= 0 && *ctableCount <= 256);
    }
    return success;
}

bool SkImageGenerator::getPixels(const SkImageInfo& info, void* pixels, size_t rowBytes) {
    SkASSERT(kIndex_8_SkColorType != info.colorType());
    if (kIndex_8_SkColorType == info.colorType()) {
        return false;
    }
    return this->getPixels(info, pixels, rowBytes, nullptr, nullptr);
}

bool SkImageGenerator::queryYUV8(SkYUVSizeInfo* sizeInfo, SkYUVColorSpace* colorSpace) const {
    SkASSERT(sizeInfo);

    return this->onQueryYUV8(sizeInfo, colorSpace);
}

bool SkImageGenerator::getYUV8Planes(const SkYUVSizeInfo& sizeInfo, void* planes[3]) {
    SkASSERT(sizeInfo.fSizes[SkYUVSizeInfo::kY].fWidth >= 0);
    SkASSERT(sizeInfo.fSizes[SkYUVSizeInfo::kY].fHeight >= 0);
    SkASSERT(sizeInfo.fSizes[SkYUVSizeInfo::kU].fWidth >= 0);
    SkASSERT(sizeInfo.fSizes[SkYUVSizeInfo::kU].fHeight >= 0);
    SkASSERT(sizeInfo.fSizes[SkYUVSizeInfo::kV].fWidth >= 0);
    SkASSERT(sizeInfo.fSizes[SkYUVSizeInfo::kV].fHeight >= 0);
    SkASSERT(sizeInfo.fWidthBytes[SkYUVSizeInfo::kY] >=
            (size_t) sizeInfo.fSizes[SkYUVSizeInfo::kY].fWidth);
    SkASSERT(sizeInfo.fWidthBytes[SkYUVSizeInfo::kU] >=
            (size_t) sizeInfo.fSizes[SkYUVSizeInfo::kU].fWidth);
    SkASSERT(sizeInfo.fWidthBytes[SkYUVSizeInfo::kV] >=
            (size_t) sizeInfo.fSizes[SkYUVSizeInfo::kV].fWidth);
    SkASSERT(planes && planes[0] && planes[1] && planes[2]);

    return this->onGetYUV8Planes(sizeInfo, planes);
}

GrTexture* SkImageGenerator::generateTexture(GrContext* ctx, const SkImageInfo& info,
                                             const SkIPoint& origin) {
    SkIRect srcRect = SkIRect::MakeXYWH(origin.x(), origin.y(), info.width(), info.height());
    if (!SkIRect::MakeWH(fInfo.width(), fInfo.height()).contains(srcRect)) {
        return nullptr;
    }
    return this->onGenerateTexture(ctx, info, origin);
}

bool SkImageGenerator::computeScaledDimensions(SkScalar scale, SupportedSizes* sizes) {
    if (scale > 0 && scale <= 1) {
        return this->onComputeScaledDimensions(scale, sizes);
    }
    return false;
}

bool SkImageGenerator::generateScaledPixels(const SkPixmap& scaledPixels) {
    if (scaledPixels.width() <= 0 || scaledPixels.height() <= 0) {
        return false;
    }
    return this->onGenerateScaledPixels(scaledPixels);
}

bool SkImageGenerator::accessScaledImage(const SkRect& src, const SkMatrix& matrix,
                                         SkFilterQuality fq, ScaledImageRec* rec) {
    SkASSERT(fInfo.bounds().contains(src));
    return this->onAccessScaledImage(src, matrix, fq, rec);
}

/////////////////////////////////////////////////////////////////////////////////////////////

SkData* SkImageGenerator::onRefEncodedData(GrContext* ctx) {
    return nullptr;
}

bool SkImageGenerator::onGetPixels(const SkImageInfo& info, void* dst, size_t rb,
                                   SkPMColor* colors, int* colorCount) {
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include "SkBitmap.h"
#include "SkColorTable.h"

static bool reset_and_return_false(SkBitmap* bitmap) {
    bitmap->reset();
    return false;
}

bool SkImageGenerator::tryGenerateBitmap(SkBitmap* bitmap, const SkImageInfo& info,
                                         SkBitmap::Allocator* allocator) {
    if (0 == info.getSafeSize(info.minRowBytes())) {
        return false;
    }
    if (!bitmap->setInfo(info)) {
        return reset_and_return_false(bitmap);
    }

    SkPMColor ctStorage[256];
    memset(ctStorage, 0xFF, sizeof(ctStorage)); // init with opaque-white for the moment
    sk_sp<SkColorTable> ctable(new SkColorTable(ctStorage, 256));
    if (!bitmap->tryAllocPixels(allocator, ctable.get())) {
        // SkResourceCache's custom allcator can'thandle ctables, so it may fail on
        // kIndex_8_SkColorTable.
        // https://bug.skia.org/4355
#if 1
        // ignore the allocator, and see if we can succeed without it
        if (!bitmap->tryAllocPixels(nullptr, ctable.get())) {
            return reset_and_return_false(bitmap);
        }
#else
        // this is the up-scale technique, not fully debugged, but we keep it here at the moment
        // to remind ourselves that this might be better than ignoring the allocator.

        info = SkImageInfo::MakeN32(info.width(), info.height(), info.alphaType());
        if (!bitmap->setInfo(info)) {
            return reset_and_return_false(bitmap);
        }
        // we pass nullptr for the ctable arg, since we are now explicitly N32
        if (!bitmap->tryAllocPixels(allocator, nullptr)) {
            return reset_and_return_false(bitmap);
        }
#endif
    }

    bitmap->lockPixels();
    if (!bitmap->getPixels()) {
        return reset_and_return_false(bitmap);
    }

    int ctCount = 0;
    if (!this->getPixels(bitmap->info(), bitmap->getPixels(), bitmap->rowBytes(),
                         ctStorage, &ctCount)) {
        return reset_and_return_false(bitmap);
    }

    if (ctCount > 0) {
        SkASSERT(kIndex_8_SkColorType == bitmap->colorType());
        // we and bitmap should be owners
        SkASSERT(!ctable->unique());

        // Now we need to overwrite the ctable we built earlier, with the correct colors.
        // This does mean that we may have made the table too big, but that cannot be avoided
        // until we can change SkImageGenerator's API to return us the ctable *before* we have to
        // allocate space for all the pixels.
        ctable->dangerous_overwriteColors(ctStorage, ctCount);
    } else {
        SkASSERT(kIndex_8_SkColorType != bitmap->colorType());
        // we should be the only owner
        SkASSERT(ctable->unique());
    }
    return true;
}

#include "SkGraphics.h"

static SkGraphics::ImageGeneratorFromEncodedFactory gFactory;

SkGraphics::ImageGeneratorFromEncodedFactory
SkGraphics::SetImageGeneratorFromEncodedFactory(ImageGeneratorFromEncodedFactory factory)
{
    ImageGeneratorFromEncodedFactory prev = gFactory;
    gFactory = factory;
    return prev;
}

SkImageGenerator* SkImageGenerator::NewFromEncoded(SkData* data) {
    if (nullptr == data) {
        return nullptr;
    }
    if (gFactory) {
        if (SkImageGenerator* generator = gFactory(data)) {
            return generator;
        }
    }
    return SkImageGenerator::NewFromEncodedImpl(data);
}
