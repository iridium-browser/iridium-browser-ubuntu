/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkImageEncoderPriv.h"

#ifdef SK_HAS_PNG_LIBRARY

#include "SkColor.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkMath.h"
#include "SkStream.h"
#include "SkTemplates.h"
#include "SkUnPreMultiply.h"
#include "SkUtils.h"
#include "transform_scanline.h"

#include "png.h"

// Suppress most PNG warnings when calling image decode functions.
static const bool c_suppressPNGImageDecoderWarnings = true;

static void sk_error_fn(png_structp png_ptr, png_const_charp msg) {
    if (!c_suppressPNGImageDecoderWarnings) {
        SkDEBUGF(("------ png error %s\n", msg));
    }
    longjmp(png_jmpbuf(png_ptr), 1);
}

static void sk_write_fn(png_structp png_ptr, png_bytep data, png_size_t len) {
    SkWStream* sk_stream = (SkWStream*)png_get_io_ptr(png_ptr);
    if (!sk_stream->write(data, len)) {
        png_error(png_ptr, "sk_write_fn Error!");
    }
}

static transform_scanline_proc choose_proc(const SkImageInfo& info) {
    const bool isGammaEncoded = info.gammaCloseToSRGB();
    switch (info.colorType()) {
        case kRGBA_8888_SkColorType:
            switch (info.alphaType()) {
                case kOpaque_SkAlphaType:
                    return transform_scanline_RGBX;
                case kUnpremul_SkAlphaType:
                    return transform_scanline_memcpy;
                case kPremul_SkAlphaType:
                    return isGammaEncoded ? transform_scanline_srgbA :
                                            transform_scanline_rgbA;
                default:
                    SkASSERT(false);
                    return nullptr;
            }
        case kBGRA_8888_SkColorType:
            switch (info.alphaType()) {
                case kOpaque_SkAlphaType:
                    return transform_scanline_BGRX;
                case kUnpremul_SkAlphaType:
                    return transform_scanline_BGRA;
                case kPremul_SkAlphaType:
                    return isGammaEncoded ? transform_scanline_sbgrA :
                                            transform_scanline_bgrA;
                default:
                    SkASSERT(false);
                    return nullptr;
            }
        case kRGB_565_SkColorType:
            return transform_scanline_565;
        case kARGB_4444_SkColorType:
            switch (info.alphaType()) {
                case kOpaque_SkAlphaType:
                    return transform_scanline_444;
                case kPremul_SkAlphaType:
                    // 4444 is assumed to be legacy premul.
                    return transform_scanline_4444;
                default:
                    SkASSERT(false);
                    return nullptr;
            }
        case kIndex_8_SkColorType:
        case kGray_8_SkColorType:
            return transform_scanline_memcpy;
        default:
            SkASSERT(false);
            return nullptr;
    }
}

/*  Pack palette[] with the corresponding colors, and if the image has alpha, also
    pack trans[] and return the number of alphas[] entries written. If the image is
    opaque, the return value will always be 0.
*/
static inline int pack_palette(SkColorTable* ctable, png_color* SK_RESTRICT palette,
                               png_byte* SK_RESTRICT alphas, const SkImageInfo& info) {
    const SkPMColor* colors = ctable->readColors();
    const int count = ctable->count();
    SkPMColor storage[256];
    if (kPremul_SkAlphaType == info.alphaType()) {
        // Unpremultiply the colors.
        const SkImageInfo rgbaInfo = info.makeColorType(kRGBA_8888_SkColorType);
        transform_scanline_proc proc = choose_proc(rgbaInfo);
        proc((char*) storage, (const char*) colors, ctable->count(), 4);
        colors = storage;
    }

    int numWithAlpha = 0;
    if (kOpaque_SkAlphaType != info.alphaType()) {
        // PNG requires that all non-opaque colors come first in the palette.  Write these first.
        for (int i = 0; i < count; i++) {
            uint8_t alpha = SkGetPackedA32(colors[i]);
            if (0xFF != alpha) {
                alphas[numWithAlpha] = alpha;
                palette[numWithAlpha].red   = SkGetPackedR32(colors[i]);
                palette[numWithAlpha].green = SkGetPackedG32(colors[i]);
                palette[numWithAlpha].blue  = SkGetPackedB32(colors[i]);
                numWithAlpha++;
            }
        }

    }

    if (0 == numWithAlpha) {
        // All of the entries are opaque.
        for (int i = 0; i < count; i++) {
            SkPMColor c = *colors++;
            palette[i].red   = SkGetPackedR32(c);
            palette[i].green = SkGetPackedG32(c);
            palette[i].blue  = SkGetPackedB32(c);
        }
    } else {
        // We have already written the non-opaque colors.  Now just write the opaque colors.
        int currIndex = numWithAlpha;
        int i = 0;
        while (currIndex != count) {
            uint8_t alpha = SkGetPackedA32(colors[i]);
            if (0xFF == alpha) {
                palette[currIndex].red   = SkGetPackedR32(colors[i]);
                palette[currIndex].green = SkGetPackedG32(colors[i]);
                palette[currIndex].blue  = SkGetPackedB32(colors[i]);
                currIndex++;
            }

            i++;
        }
    }

    return numWithAlpha;
}

static bool do_encode(SkWStream*, const SkPixmap&, int, int, png_color_8&);

bool SkEncodeImageAsPNG(SkWStream* stream, const SkPixmap& src, const SkEncodeOptions& opts) {
    SkASSERT(!src.colorSpace() || src.colorSpace()->gammaCloseToSRGB() ||
             src.colorSpace()->gammaIsLinear());

    SkPixmap pixmap = src;
    if (SkEncodeOptions::PremulBehavior::kLegacy == opts.fPremulBehavior) {
        pixmap.setColorSpace(nullptr);
    } else {
        if (!pixmap.colorSpace()) {
            return false;
        }
    }

    if (!pixmap.addr() || pixmap.info().isEmpty()) {
        return false;
    }
    const SkColorType colorType = pixmap.colorType();
    switch (colorType) {
        case kIndex_8_SkColorType:
        case kGray_8_SkColorType:
        case kRGBA_8888_SkColorType:
        case kBGRA_8888_SkColorType:
        case kARGB_4444_SkColorType:
        case kRGB_565_SkColorType:
            break;
        default:
            return false;
    }

    const SkAlphaType alphaType = pixmap.alphaType();
    switch (alphaType) {
        case kUnpremul_SkAlphaType:
            if (kARGB_4444_SkColorType == colorType) {
                return false;
            }

            break;
        case kOpaque_SkAlphaType:
        case kPremul_SkAlphaType:
            break;
        default:
            return false;
    }

    const bool isOpaque = (kOpaque_SkAlphaType == alphaType);
    const int bitDepth = 8;
    png_color_8 sig_bit;
    sk_bzero(&sig_bit, sizeof(png_color_8));

    int pngColorType;
    switch (colorType) {
        case kIndex_8_SkColorType:
            sig_bit.red = 8;
            sig_bit.green = 8;
            sig_bit.blue = 8;
            sig_bit.alpha = 8;
            pngColorType = PNG_COLOR_TYPE_PALETTE;
            break;
        case kGray_8_SkColorType:
            sig_bit.gray = 8;
            pngColorType = PNG_COLOR_TYPE_GRAY;
            SkASSERT(isOpaque);
            break;
        case kRGBA_8888_SkColorType:
        case kBGRA_8888_SkColorType:
            sig_bit.red = 8;
            sig_bit.green = 8;
            sig_bit.blue = 8;
            sig_bit.alpha = 8;
            pngColorType = isOpaque ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA;
            break;
        case kARGB_4444_SkColorType:
            sig_bit.red = 4;
            sig_bit.green = 4;
            sig_bit.blue = 4;
            sig_bit.alpha = 4;
            pngColorType = isOpaque ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA;
            break;
        case kRGB_565_SkColorType:
            sig_bit.red = 5;
            sig_bit.green = 6;
            sig_bit.blue = 5;
            pngColorType = PNG_COLOR_TYPE_RGB;
            SkASSERT(isOpaque);
            break;
        default:
            return false;
    }
    if (kIndex_8_SkColorType == colorType) {
        SkColorTable* ctable = pixmap.ctable();
        if (!ctable || ctable->count() == 0) {
            return false;
        }

        // Currently, we always use 8-bit indices for paletted pngs.
        // When ctable->count() <= 16, we could potentially use 1, 2,
        // or 4 bit indices.
    }
    return do_encode(stream, pixmap, pngColorType, bitDepth, sig_bit);
}

static bool do_encode(SkWStream* stream, const SkPixmap& pixmap,
                      int pngColorType, int bitDepth, png_color_8& sig_bit) {
    png_structp png_ptr;
    png_infop info_ptr;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, sk_error_fn, nullptr);
    if (nullptr == png_ptr) {
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (nullptr == info_ptr) {
        png_destroy_write_struct(&png_ptr,  nullptr);
        return false;
    }

    /* Set error handling.  REQUIRED if you aren't supplying your own
    * error handling functions in the png_create_write_struct() call.
    */
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return false;
    }

    png_set_write_fn(png_ptr, (void*)stream, sk_write_fn, nullptr);

    /* Set the image information here.  Width and height are up to 2^31,
    * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
    * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
    * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
    * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
    * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
    * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
    */

    png_set_IHDR(png_ptr, info_ptr, pixmap.width(), pixmap.height(),
                 bitDepth, pngColorType,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    // set our colortable/trans arrays if needed
    png_color paletteColors[256];
    png_byte trans[256];
    if (kIndex_8_SkColorType == pixmap.colorType()) {
        SkColorTable* colorTable = pixmap.ctable();
        SkASSERT(colorTable);
        int numTrans = pack_palette(colorTable, paletteColors, trans, pixmap.info());
        png_set_PLTE(png_ptr, info_ptr, paletteColors, colorTable->count());
        if (numTrans > 0) {
            png_set_tRNS(png_ptr, info_ptr, trans, numTrans, nullptr);
        }
    }

    png_set_sBIT(png_ptr, info_ptr, &sig_bit);
    png_write_info(png_ptr, info_ptr);

    const char* srcImage = (const char*)pixmap.addr();
    SkAutoSTMalloc<1024, char> rowStorage(pixmap.width() << 2);
    char* storage = rowStorage.get();
    transform_scanline_proc proc = choose_proc(pixmap.info());

    for (int y = 0; y < pixmap.height(); y++) {
        png_bytep row_ptr = (png_bytep)storage;
        proc(storage, srcImage, pixmap.width(), SkColorTypeBytesPerPixel(pixmap.colorType()));
        png_write_rows(png_ptr, &row_ptr, 1);
        srcImage += pixmap.rowBytes();
    }

    png_write_end(png_ptr, info_ptr);

    /* clean up after the write, and free any memory allocated */
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return true;
}

#endif
