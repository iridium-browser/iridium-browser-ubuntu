/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkImageGenerator.h"

SkImageGenerator::Result SkImageGenerator::getPixels(const SkImageInfo& info, void* pixels,
                                                     size_t rowBytes, const Options* options,
                                                     SkPMColor ctable[], int* ctableCount) {
    if (kUnknown_SkColorType == info.colorType()) {
        return kInvalidConversion;
    }
    if (NULL == pixels) {
        return kInvalidParameters;
    }
    if (rowBytes < info.minRowBytes()) {
        return kInvalidParameters;
    }

    if (kIndex_8_SkColorType == info.colorType()) {
        if (NULL == ctable || NULL == ctableCount) {
            return kInvalidParameters;
        }
    } else {
        if (ctableCount) {
            *ctableCount = 0;
        }
        ctableCount = NULL;
        ctable = NULL;
    }

    // Default options.
    Options optsStorage;
    if (NULL == options) {
        options = &optsStorage;
    }
    const Result result = this->onGetPixels(info, pixels, rowBytes, *options, ctable, ctableCount);

    if ((kIncompleteInput == result || kSuccess == result) && ctableCount) {
        SkASSERT(*ctableCount >= 0 && *ctableCount <= 256);
    }
    return result;
}

SkImageGenerator::Result SkImageGenerator::getPixels(const SkImageInfo& info, void* pixels,
                                                     size_t rowBytes) {
    SkASSERT(kIndex_8_SkColorType != info.colorType());
    if (kIndex_8_SkColorType == info.colorType()) {
        return kInvalidConversion;
    }
    return this->getPixels(info, pixels, rowBytes, NULL, NULL, NULL);
}

bool SkImageGenerator::getYUV8Planes(SkISize sizes[3], void* planes[3], size_t rowBytes[3],
                                     SkYUVColorSpace* colorSpace) {
#ifdef SK_DEBUG
    // In all cases, we need the sizes array
    SkASSERT(sizes);

    bool isValidWithPlanes = (planes) && (rowBytes) &&
        ((planes[0]) && (planes[1]) && (planes[2]) &&
         (0  != rowBytes[0]) && (0  != rowBytes[1]) && (0  != rowBytes[2]));
    bool isValidWithoutPlanes =
        ((NULL == planes) ||
         ((NULL == planes[0]) && (NULL == planes[1]) && (NULL == planes[2]))) &&
        ((NULL == rowBytes) ||
         ((0 == rowBytes[0]) && (0 == rowBytes[1]) && (0 == rowBytes[2])));

    // Either we have all planes and rowBytes information or we have none of it
    // Having only partial information is not supported
    SkASSERT(isValidWithPlanes || isValidWithoutPlanes);

    // If we do have planes information, make sure all sizes are non 0
    // and all rowBytes are valid
    SkASSERT(!isValidWithPlanes ||
             ((sizes[0].fWidth  >= 0) &&
              (sizes[0].fHeight >= 0) &&
              (sizes[1].fWidth  >= 0) &&
              (sizes[1].fHeight >= 0) &&
              (sizes[2].fWidth  >= 0) &&
              (sizes[2].fHeight >= 0) &&
              (rowBytes[0] >= (size_t)sizes[0].fWidth) &&
              (rowBytes[1] >= (size_t)sizes[1].fWidth) &&
              (rowBytes[2] >= (size_t)sizes[2].fWidth)));
#endif

    return this->onGetYUV8Planes(sizes, planes, rowBytes, colorSpace);
}

bool SkImageGenerator::onGetYUV8Planes(SkISize sizes[3], void* planes[3], size_t rowBytes[3]) {
    return false;
}

bool SkImageGenerator::onGetYUV8Planes(SkISize sizes[3], void* planes[3], size_t rowBytes[3],
                                       SkYUVColorSpace* colorSpace) {
    // In order to maintain compatibility with clients that implemented the original
    // onGetYUV8Planes interface, we assume that the color space is JPEG.
    // TODO(rileya): remove this and the old onGetYUV8Planes once clients switch over to
    // the new interface.
    if (colorSpace) {
        *colorSpace = kJPEG_SkYUVColorSpace;
    }
    return this->onGetYUV8Planes(sizes, planes, rowBytes);
}

/////////////////////////////////////////////////////////////////////////////////////////////

SkData* SkImageGenerator::onRefEncodedData() {
    return NULL;
}

#ifdef SK_SUPPORT_LEGACY_OPTIONLESS_GET_PIXELS
SkImageGenerator::Result SkImageGenerator::onGetPixels(const SkImageInfo&, void*, size_t,
                                                       SkPMColor*, int*) {
    return kUnimplemented;
}
#endif

SkImageGenerator::Result SkImageGenerator::onGetPixels(const SkImageInfo& info, void* dst,
                                                       size_t rb, const Options& options,
                                                       SkPMColor* colors, int* colorCount) {
#ifdef SK_SUPPORT_LEGACY_OPTIONLESS_GET_PIXELS
    return this->onGetPixels(info, dst, rb, colors, colorCount);
#else
    return kUnimplemented;
#endif
}
