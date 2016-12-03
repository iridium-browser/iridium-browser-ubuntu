/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Sk4fLinearGradient.h"
#include "SkGradientShaderPriv.h"
#include "SkLinearGradient.h"
#include "SkRadialGradient.h"
#include "SkTwoPointConicalGradient.h"
#include "SkSweepGradient.h"

void SkGradientShaderBase::Descriptor::flatten(SkWriteBuffer& buffer) const {
    buffer.writeColorArray(fColors, fCount);
    if (fPos) {
        buffer.writeBool(true);
        buffer.writeScalarArray(fPos, fCount);
    } else {
        buffer.writeBool(false);
    }
    buffer.write32(fTileMode);
    buffer.write32(fGradFlags);
    if (fLocalMatrix) {
        buffer.writeBool(true);
        buffer.writeMatrix(*fLocalMatrix);
    } else {
        buffer.writeBool(false);
    }
}

bool SkGradientShaderBase::DescriptorScope::unflatten(SkReadBuffer& buffer) {
    fCount = buffer.getArrayCount();
    if (fCount > kStorageCount) {
        size_t allocSize = (sizeof(SkColor) + sizeof(SkScalar)) * fCount;
        fDynamicStorage.reset(allocSize);
        fColors = (SkColor*)fDynamicStorage.get();
        fPos = (SkScalar*)(fColors + fCount);
    } else {
        fColors = fColorStorage;
        fPos = fPosStorage;
    }

    if (!buffer.readColorArray(const_cast<SkColor*>(fColors), fCount)) {
        return false;
    }
    if (buffer.readBool()) {
        if (!buffer.readScalarArray(const_cast<SkScalar*>(fPos), fCount)) {
            return false;
        }
    } else {
        fPos = nullptr;
    }

    fTileMode = (SkShader::TileMode)buffer.read32();
    fGradFlags = buffer.read32();

    if (buffer.readBool()) {
        fLocalMatrix = &fLocalMatrixStorage;
        buffer.readMatrix(&fLocalMatrixStorage);
    } else {
        fLocalMatrix = nullptr;
    }
    return buffer.isValid();
}

////////////////////////////////////////////////////////////////////////////////////////////

SkGradientShaderBase::SkGradientShaderBase(const Descriptor& desc, const SkMatrix& ptsToUnit)
    : INHERITED(desc.fLocalMatrix)
    , fPtsToUnit(ptsToUnit)
{
    fPtsToUnit.getType();  // Precache so reads are threadsafe.
    SkASSERT(desc.fCount > 1);

    fGradFlags = SkToU8(desc.fGradFlags);

    SkASSERT((unsigned)desc.fTileMode < SkShader::kTileModeCount);
    SkASSERT(SkShader::kTileModeCount == SK_ARRAY_COUNT(gTileProcs));
    fTileMode = desc.fTileMode;
    fTileProc = gTileProcs[desc.fTileMode];

    /*  Note: we let the caller skip the first and/or last position.
        i.e. pos[0] = 0.3, pos[1] = 0.7
        In these cases, we insert dummy entries to ensure that the final data
        will be bracketed by [0, 1].
        i.e. our_pos[0] = 0, our_pos[1] = 0.3, our_pos[2] = 0.7, our_pos[3] = 1

        Thus colorCount (the caller's value, and fColorCount (our value) may
        differ by up to 2. In the above example:
            colorCount = 2
            fColorCount = 4
     */
    fColorCount = desc.fCount;
    // check if we need to add in dummy start and/or end position/colors
    bool dummyFirst = false;
    bool dummyLast = false;
    if (desc.fPos) {
        dummyFirst = desc.fPos[0] != 0;
        dummyLast = desc.fPos[desc.fCount - 1] != SK_Scalar1;
        fColorCount += dummyFirst + dummyLast;
    }

    if (fColorCount > kColorStorageCount) {
        size_t size = sizeof(SkColor) + sizeof(Rec);
        if (desc.fPos) {
            size += sizeof(SkScalar);
        }
        fOrigColors = reinterpret_cast<SkColor*>(
                                        sk_malloc_throw(size * fColorCount));
    }
    else {
        fOrigColors = fStorage;
    }

    // Now copy over the colors, adding the dummies as needed
    {
        SkColor* origColors = fOrigColors;
        if (dummyFirst) {
            *origColors++ = desc.fColors[0];
        }
        memcpy(origColors, desc.fColors, desc.fCount * sizeof(SkColor));
        if (dummyLast) {
            origColors += desc.fCount;
            *origColors = desc.fColors[desc.fCount - 1];
        }
    }

    if (desc.fPos && fColorCount) {
        fOrigPos = (SkScalar*)(fOrigColors + fColorCount);
        fRecs = (Rec*)(fOrigPos + fColorCount);
    } else {
        fOrigPos = nullptr;
        fRecs = (Rec*)(fOrigColors + fColorCount);
    }

    if (fColorCount > 2) {
        Rec* recs = fRecs;
        recs->fPos = 0;
        //  recs->fScale = 0; // unused;
        recs += 1;
        if (desc.fPos) {
            SkScalar* origPosPtr = fOrigPos;
            *origPosPtr++ = 0;

            /*  We need to convert the user's array of relative positions into
                fixed-point positions and scale factors. We need these results
                to be strictly monotonic (no two values equal or out of order).
                Hence this complex loop that just jams a zero for the scale
                value if it sees a segment out of order, and it assures that
                we start at 0 and end at 1.0
            */
            SkScalar prev = 0;
            int startIndex = dummyFirst ? 0 : 1;
            int count = desc.fCount + dummyLast;
            for (int i = startIndex; i < count; i++) {
                // force the last value to be 1.0
                SkScalar curr;
                if (i == desc.fCount) {  // we're really at the dummyLast
                    curr = 1;
                } else {
                    curr = SkScalarPin(desc.fPos[i], 0, 1);
                }
                *origPosPtr++ = curr;

                recs->fPos = SkScalarToFixed(curr);
                SkFixed diff = SkScalarToFixed(curr - prev);
                if (diff > 0) {
                    recs->fScale = (1 << 24) / diff;
                } else {
                    recs->fScale = 0; // ignore this segment
                }
                // get ready for the next value
                prev = curr;
                recs += 1;
            }
        } else {    // assume even distribution
            fOrigPos = nullptr;

            SkFixed dp = SK_Fixed1 / (desc.fCount - 1);
            SkFixed p = dp;
            SkFixed scale = (desc.fCount - 1) << 8;  // (1 << 24) / dp
            for (int i = 1; i < desc.fCount - 1; i++) {
                recs->fPos   = p;
                recs->fScale = scale;
                recs += 1;
                p += dp;
            }
            recs->fPos = SK_Fixed1;
            recs->fScale = scale;
        }
    } else if (desc.fPos) {
        SkASSERT(2 == fColorCount);
        fOrigPos[0] = SkScalarPin(desc.fPos[0], 0, 1);
        fOrigPos[1] = SkScalarPin(desc.fPos[1], fOrigPos[0], 1);
        if (0 == fOrigPos[0] && 1 == fOrigPos[1]) {
            fOrigPos = nullptr;
        }
    }
    this->initCommon();
}

SkGradientShaderBase::~SkGradientShaderBase() {
    if (fOrigColors != fStorage) {
        sk_free(fOrigColors);
    }
}

void SkGradientShaderBase::initCommon() {
    unsigned colorAlpha = 0xFF;
    for (int i = 0; i < fColorCount; i++) {
        colorAlpha &= SkColorGetA(fOrigColors[i]);
    }
    fColorsAreOpaque = colorAlpha == 0xFF;
}

void SkGradientShaderBase::flatten(SkWriteBuffer& buffer) const {
    Descriptor desc;
    desc.fColors = fOrigColors;
    desc.fPos = fOrigPos;
    desc.fCount = fColorCount;
    desc.fTileMode = fTileMode;
    desc.fGradFlags = fGradFlags;

    const SkMatrix& m = this->getLocalMatrix();
    desc.fLocalMatrix = m.isIdentity() ? nullptr : &m;
    desc.flatten(buffer);
}

void SkGradientShaderBase::FlipGradientColors(SkColor* colorDst, Rec* recDst,
                                              SkColor* colorSrc, Rec* recSrc,
                                              int count) {
    SkAutoSTArray<8, SkColor> colorsTemp(count);
    for (int i = 0; i < count; ++i) {
        int offset = count - i - 1;
        colorsTemp[i] = colorSrc[offset];
    }
    if (count > 2) {
        SkAutoSTArray<8, Rec> recsTemp(count);
        for (int i = 0; i < count; ++i) {
            int offset = count - i - 1;
            recsTemp[i].fPos = SK_Fixed1 - recSrc[offset].fPos;
            recsTemp[i].fScale = recSrc[offset].fScale;
        }
        memcpy(recDst, recsTemp.get(), count * sizeof(Rec));
    }
    memcpy(colorDst, colorsTemp.get(), count * sizeof(SkColor));
}

bool SkGradientShaderBase::isOpaque() const {
    return fColorsAreOpaque;
}

static unsigned rounded_divide(unsigned numer, unsigned denom) {
    return (numer + (denom >> 1)) / denom;
}

bool SkGradientShaderBase::onAsLuminanceColor(SkColor* lum) const {
    // we just compute an average color.
    // possibly we could weight this based on the proportional width for each color
    //   assuming they are not evenly distributed in the fPos array.
    int r = 0;
    int g = 0;
    int b = 0;
    const int n = fColorCount;
    for (int i = 0; i < n; ++i) {
        SkColor c = fOrigColors[i];
        r += SkColorGetR(c);
        g += SkColorGetG(c);
        b += SkColorGetB(c);
    }
    *lum = SkColorSetRGB(rounded_divide(r, n), rounded_divide(g, n), rounded_divide(b, n));
    return true;
}

SkGradientShaderBase::GradientShaderBaseContext::GradientShaderBaseContext(
        const SkGradientShaderBase& shader, const ContextRec& rec)
    : INHERITED(shader, rec)
#ifdef SK_SUPPORT_LEGACY_GRADIENT_DITHERING
    , fDither(true)
#else
    , fDither(rec.fPaint->isDither())
#endif
    , fCache(shader.refCache(getPaintAlpha(), fDither))
{
    const SkMatrix& inverse = this->getTotalInverse();

    fDstToIndex.setConcat(shader.fPtsToUnit, inverse);

    fDstToIndexProc = fDstToIndex.getMapXYProc();
    fDstToIndexClass = (uint8_t)SkShader::Context::ComputeMatrixClass(fDstToIndex);

    // now convert our colors in to PMColors
    unsigned paintAlpha = this->getPaintAlpha();

    fFlags = this->INHERITED::getFlags();
    if (shader.fColorsAreOpaque && paintAlpha == 0xFF) {
        fFlags |= kOpaqueAlpha_Flag;
    }
}

SkGradientShaderBase::GradientShaderCache::GradientShaderCache(
        U8CPU alpha, bool dither, const SkGradientShaderBase& shader)
    : fCacheAlpha(alpha)
    , fCacheDither(dither)
    , fShader(shader)
{
    // Only initialize the cache in getCache16/32.
    fCache16 = nullptr;
    fCache32 = nullptr;
    fCache16Storage = nullptr;
    fCache32PixelRef = nullptr;
}

SkGradientShaderBase::GradientShaderCache::~GradientShaderCache() {
    sk_free(fCache16Storage);
    SkSafeUnref(fCache32PixelRef);
}

#define Fixed_To_Dot8(x)        (((x) + 0x80) >> 8)

/** We take the original colors, not our premultiplied PMColors, since we can
    build a 16bit table as long as the original colors are opaque, even if the
    paint specifies a non-opaque alpha.
*/
void SkGradientShaderBase::GradientShaderCache::Build16bitCache(
        uint16_t cache[], SkColor c0, SkColor c1, int count, bool dither) {
    SkASSERT(count > 1);
    SkASSERT(SkColorGetA(c0) == 0xFF);
    SkASSERT(SkColorGetA(c1) == 0xFF);

    SkFixed r = SkColorGetR(c0);
    SkFixed g = SkColorGetG(c0);
    SkFixed b = SkColorGetB(c0);

    SkFixed dr = SkIntToFixed(SkColorGetR(c1) - r) / (count - 1);
    SkFixed dg = SkIntToFixed(SkColorGetG(c1) - g) / (count - 1);
    SkFixed db = SkIntToFixed(SkColorGetB(c1) - b) / (count - 1);

    r = SkIntToFixed(r) + 0x8000;
    g = SkIntToFixed(g) + 0x8000;
    b = SkIntToFixed(b) + 0x8000;

    if (dither) {
        do {
            unsigned rr = r >> 16;
            unsigned gg = g >> 16;
            unsigned bb = b >> 16;
            cache[0] = SkPackRGB16(SkR32ToR16(rr), SkG32ToG16(gg), SkB32ToB16(bb));
            cache[kCache16Count] = SkDitherPack888ToRGB16(rr, gg, bb);
            cache += 1;
            r += dr;
            g += dg;
            b += db;
        } while (--count != 0);
    } else {
        do {
            unsigned rr = r >> 16;
            unsigned gg = g >> 16;
            unsigned bb = b >> 16;
            cache[0] = SkPackRGB16(SkR32ToR16(rr), SkG32ToG16(gg), SkB32ToB16(bb));
            cache[kCache16Count] = cache[0];
            cache += 1;
            r += dr;
            g += dg;
            b += db;
        } while (--count != 0);
    }
}

/*
 *  r,g,b used to be SkFixed, but on gcc (4.2.1 mac and 4.6.3 goobuntu) in
 *  release builds, we saw a compiler error where the 0xFF parameter in
 *  SkPackARGB32() was being totally ignored whenever it was called with
 *  a non-zero add (e.g. 0x8000).
 *
 *  We found two work-arounds:
 *      1. change r,g,b to unsigned (or just one of them)
 *      2. change SkPackARGB32 to + its (a << SK_A32_SHIFT) value instead
 *         of using |
 *
 *  We chose #1 just because it was more localized.
 *  See http://code.google.com/p/skia/issues/detail?id=1113
 *
 *  The type SkUFixed encapsulate this need for unsigned, but logically Fixed.
 */
typedef uint32_t SkUFixed;

void SkGradientShaderBase::GradientShaderCache::Build32bitCache(
        SkPMColor cache[], SkColor c0, SkColor c1,
        int count, U8CPU paintAlpha, uint32_t gradFlags, bool dither) {
    SkASSERT(count > 1);

    // need to apply paintAlpha to our two endpoints
    uint32_t a0 = SkMulDiv255Round(SkColorGetA(c0), paintAlpha);
    uint32_t a1 = SkMulDiv255Round(SkColorGetA(c1), paintAlpha);


    const bool interpInPremul = SkToBool(gradFlags &
                           SkGradientShader::kInterpolateColorsInPremul_Flag);

    uint32_t r0 = SkColorGetR(c0);
    uint32_t g0 = SkColorGetG(c0);
    uint32_t b0 = SkColorGetB(c0);

    uint32_t r1 = SkColorGetR(c1);
    uint32_t g1 = SkColorGetG(c1);
    uint32_t b1 = SkColorGetB(c1);

    if (interpInPremul) {
        r0 = SkMulDiv255Round(r0, a0);
        g0 = SkMulDiv255Round(g0, a0);
        b0 = SkMulDiv255Round(b0, a0);

        r1 = SkMulDiv255Round(r1, a1);
        g1 = SkMulDiv255Round(g1, a1);
        b1 = SkMulDiv255Round(b1, a1);
    }

    SkFixed da = SkIntToFixed(a1 - a0) / (count - 1);
    SkFixed dr = SkIntToFixed(r1 - r0) / (count - 1);
    SkFixed dg = SkIntToFixed(g1 - g0) / (count - 1);
    SkFixed db = SkIntToFixed(b1 - b0) / (count - 1);

    /*  We pre-add 1/8 to avoid having to add this to our [0] value each time
        in the loop. Without this, the bias for each would be
            0x2000  0xA000  0xE000  0x6000
        With this trick, we can add 0 for the first (no-op) and just adjust the
        others.
     */
    const SkUFixed bias0 = dither ? 0x2000 : 0x8000;
    const SkUFixed bias1 = dither ? 0x8000 : 0;
    const SkUFixed bias2 = dither ? 0xC000 : 0;
    const SkUFixed bias3 = dither ? 0x4000 : 0;

    SkUFixed a = SkIntToFixed(a0) + bias0;
    SkUFixed r = SkIntToFixed(r0) + bias0;
    SkUFixed g = SkIntToFixed(g0) + bias0;
    SkUFixed b = SkIntToFixed(b0) + bias0;

    /*
     *  Our dither-cell (spatially) is
     *      0 2
     *      3 1
     *  Where
     *      [0] -> [-1/8 ... 1/8 ) values near 0
     *      [1] -> [ 1/8 ... 3/8 ) values near 1/4
     *      [2] -> [ 3/8 ... 5/8 ) values near 1/2
     *      [3] -> [ 5/8 ... 7/8 ) values near 3/4
     */

    if (0xFF == a0 && 0 == da) {
        do {
            cache[kCache32Count*0] = SkPackARGB32(0xFF, (r + 0    ) >> 16,
                                                        (g + 0    ) >> 16,
                                                        (b + 0    ) >> 16);
            cache[kCache32Count*1] = SkPackARGB32(0xFF, (r + bias1) >> 16,
                                                        (g + bias1) >> 16,
                                                        (b + bias1) >> 16);
            cache[kCache32Count*2] = SkPackARGB32(0xFF, (r + bias2) >> 16,
                                                        (g + bias2) >> 16,
                                                        (b + bias2) >> 16);
            cache[kCache32Count*3] = SkPackARGB32(0xFF, (r + bias3) >> 16,
                                                        (g + bias3) >> 16,
                                                        (b + bias3) >> 16);
            cache += 1;
            r += dr;
            g += dg;
            b += db;
        } while (--count != 0);
    } else if (interpInPremul) {
        do {
            cache[kCache32Count*0] = SkPackARGB32((a + 0    ) >> 16,
                                                  (r + 0    ) >> 16,
                                                  (g + 0    ) >> 16,
                                                  (b + 0    ) >> 16);
            cache[kCache32Count*1] = SkPackARGB32((a + bias1) >> 16,
                                                  (r + bias1) >> 16,
                                                  (g + bias1) >> 16,
                                                  (b + bias1) >> 16);
            cache[kCache32Count*2] = SkPackARGB32((a + bias2) >> 16,
                                                  (r + bias2) >> 16,
                                                  (g + bias2) >> 16,
                                                  (b + bias2) >> 16);
            cache[kCache32Count*3] = SkPackARGB32((a + bias3) >> 16,
                                                  (r + bias3) >> 16,
                                                  (g + bias3) >> 16,
                                                  (b + bias3) >> 16);
            cache += 1;
            a += da;
            r += dr;
            g += dg;
            b += db;
        } while (--count != 0);
    } else {    // interpolate in unpreml space
        do {
            cache[kCache32Count*0] = SkPremultiplyARGBInline((a + 0     ) >> 16,
                                                             (r + 0     ) >> 16,
                                                             (g + 0     ) >> 16,
                                                             (b + 0     ) >> 16);
            cache[kCache32Count*1] = SkPremultiplyARGBInline((a + bias1) >> 16,
                                                             (r + bias1) >> 16,
                                                             (g + bias1) >> 16,
                                                             (b + bias1) >> 16);
            cache[kCache32Count*2] = SkPremultiplyARGBInline((a + bias2) >> 16,
                                                             (r + bias2) >> 16,
                                                             (g + bias2) >> 16,
                                                             (b + bias2) >> 16);
            cache[kCache32Count*3] = SkPremultiplyARGBInline((a + bias3) >> 16,
                                                             (r + bias3) >> 16,
                                                             (g + bias3) >> 16,
                                                             (b + bias3) >> 16);
            cache += 1;
            a += da;
            r += dr;
            g += dg;
            b += db;
        } while (--count != 0);
    }
}

static inline int SkFixedToFFFF(SkFixed x) {
    SkASSERT((unsigned)x <= SK_Fixed1);
    return x - (x >> 16);
}

const uint16_t* SkGradientShaderBase::GradientShaderCache::getCache16() {
    fCache16InitOnce(SkGradientShaderBase::GradientShaderCache::initCache16, this);
    SkASSERT(fCache16);
    return fCache16;
}

void SkGradientShaderBase::GradientShaderCache::initCache16(GradientShaderCache* cache) {
    // double the count for dither entries
    const int entryCount = kCache16Count * 2;
    const size_t allocSize = sizeof(uint16_t) * entryCount;

    SkASSERT(nullptr == cache->fCache16Storage);
    cache->fCache16Storage = (uint16_t*)sk_malloc_throw(allocSize);
    cache->fCache16 = cache->fCache16Storage;
    if (cache->fShader.fColorCount == 2) {
        Build16bitCache(cache->fCache16, cache->fShader.fOrigColors[0],
                        cache->fShader.fOrigColors[1], kCache16Count, cache->fCacheDither);
    } else {
        Rec* rec = cache->fShader.fRecs;
        int prevIndex = 0;
        for (int i = 1; i < cache->fShader.fColorCount; i++) {
            int nextIndex = SkFixedToFFFF(rec[i].fPos) >> kCache16Shift;
            SkASSERT(nextIndex < kCache16Count);

            if (nextIndex > prevIndex)
                Build16bitCache(cache->fCache16 + prevIndex, cache->fShader.fOrigColors[i-1],
                                cache->fShader.fOrigColors[i], nextIndex - prevIndex + 1,
                                cache->fCacheDither);
            prevIndex = nextIndex;
        }
    }
}

const SkPMColor* SkGradientShaderBase::GradientShaderCache::getCache32() {
    fCache32InitOnce(SkGradientShaderBase::GradientShaderCache::initCache32, this);
    SkASSERT(fCache32);
    return fCache32;
}

void SkGradientShaderBase::GradientShaderCache::initCache32(GradientShaderCache* cache) {
    const int kNumberOfDitherRows = 4;
    const SkImageInfo info = SkImageInfo::MakeN32Premul(kCache32Count, kNumberOfDitherRows);

    SkASSERT(nullptr == cache->fCache32PixelRef);
    cache->fCache32PixelRef = SkMallocPixelRef::NewAllocate(info, 0, nullptr);
    cache->fCache32 = (SkPMColor*)cache->fCache32PixelRef->getAddr();
    if (cache->fShader.fColorCount == 2) {
        Build32bitCache(cache->fCache32, cache->fShader.fOrigColors[0],
                        cache->fShader.fOrigColors[1], kCache32Count, cache->fCacheAlpha,
                        cache->fShader.fGradFlags, cache->fCacheDither);
    } else {
        Rec* rec = cache->fShader.fRecs;
        int prevIndex = 0;
        for (int i = 1; i < cache->fShader.fColorCount; i++) {
            int nextIndex = SkFixedToFFFF(rec[i].fPos) >> kCache32Shift;
            SkASSERT(nextIndex < kCache32Count);

            if (nextIndex > prevIndex)
                Build32bitCache(cache->fCache32 + prevIndex, cache->fShader.fOrigColors[i-1],
                                cache->fShader.fOrigColors[i], nextIndex - prevIndex + 1,
                                cache->fCacheAlpha, cache->fShader.fGradFlags, cache->fCacheDither);
            prevIndex = nextIndex;
        }
    }
}

/*
 *  The gradient holds a cache for the most recent value of alpha. Successive
 *  callers with the same alpha value will share the same cache.
 */
SkGradientShaderBase::GradientShaderCache* SkGradientShaderBase::refCache(U8CPU alpha,
                                                                          bool dither) const {
    SkAutoMutexAcquire ama(fCacheMutex);
    if (!fCache || fCache->getAlpha() != alpha || fCache->getDither() != dither) {
        fCache.reset(new GradientShaderCache(alpha, dither, *this));
    }
    // Increment the ref counter inside the mutex to ensure the returned pointer is still valid.
    // Otherwise, the pointer may have been overwritten on a different thread before the object's
    // ref count was incremented.
    fCache.get()->ref();
    return fCache;
}

SK_DECLARE_STATIC_MUTEX(gGradientCacheMutex);
/*
 *  Because our caller might rebuild the same (logically the same) gradient
 *  over and over, we'd like to return exactly the same "bitmap" if possible,
 *  allowing the client to utilize a cache of our bitmap (e.g. with a GPU).
 *  To do that, we maintain a private cache of built-bitmaps, based on our
 *  colors and positions. Note: we don't try to flatten the fMapper, so if one
 *  is present, we skip the cache for now.
 */
void SkGradientShaderBase::getGradientTableBitmap(SkBitmap* bitmap) const {
    // our caller assumes no external alpha, so we ensure that our cache is
    // built with 0xFF
    SkAutoTUnref<GradientShaderCache> cache(this->refCache(0xFF, true));

    // build our key: [numColors + colors[] + {positions[]} + flags ]
    int count = 1 + fColorCount + 1;
    if (fColorCount > 2) {
        count += fColorCount - 1;    // fRecs[].fPos
    }

    SkAutoSTMalloc<16, int32_t> storage(count);
    int32_t* buffer = storage.get();

    *buffer++ = fColorCount;
    memcpy(buffer, fOrigColors, fColorCount * sizeof(SkColor));
    buffer += fColorCount;
    if (fColorCount > 2) {
        for (int i = 1; i < fColorCount; i++) {
            *buffer++ = fRecs[i].fPos;
        }
    }
    *buffer++ = fGradFlags;
    SkASSERT(buffer - storage.get() == count);

    ///////////////////////////////////

    static SkGradientBitmapCache* gCache;
    // each cache cost 1K of RAM, since each bitmap will be 1x256 at 32bpp
    static const int MAX_NUM_CACHED_GRADIENT_BITMAPS = 32;
    SkAutoMutexAcquire ama(gGradientCacheMutex);

    if (nullptr == gCache) {
        gCache = new SkGradientBitmapCache(MAX_NUM_CACHED_GRADIENT_BITMAPS);
    }
    size_t size = count * sizeof(int32_t);

    if (!gCache->find(storage.get(), size, bitmap)) {
        // force our cahce32pixelref to be built
        (void)cache->getCache32();
        bitmap->setInfo(SkImageInfo::MakeN32Premul(kCache32Count, 1));
        bitmap->setPixelRef(cache->getCache32PixelRef());

        gCache->add(storage.get(), size, *bitmap);
    }
}

void SkGradientShaderBase::commonAsAGradient(GradientInfo* info, bool flipGrad) const {
    if (info) {
        if (info->fColorCount >= fColorCount) {
            SkColor* colorLoc;
            Rec*     recLoc;
            if (flipGrad && (info->fColors || info->fColorOffsets)) {
                SkAutoSTArray<8, SkColor> colorStorage(fColorCount);
                SkAutoSTArray<8, Rec> recStorage(fColorCount);
                colorLoc = colorStorage.get();
                recLoc = recStorage.get();
                FlipGradientColors(colorLoc, recLoc, fOrigColors, fRecs, fColorCount);
            } else {
                colorLoc = fOrigColors;
                recLoc = fRecs;
            }
            if (info->fColors) {
                memcpy(info->fColors, colorLoc, fColorCount * sizeof(SkColor));
            }
            if (info->fColorOffsets) {
                if (fColorCount == 2) {
                    info->fColorOffsets[0] = 0;
                    info->fColorOffsets[1] = SK_Scalar1;
                } else if (fColorCount > 2) {
                    for (int i = 0; i < fColorCount; ++i) {
                        info->fColorOffsets[i] = SkFixedToScalar(recLoc[i].fPos);
                    }
                }
            }
        }
        info->fColorCount = fColorCount;
        info->fTileMode = fTileMode;
        info->fGradientFlags = fGradFlags;
    }
}

#ifndef SK_IGNORE_TO_STRING
void SkGradientShaderBase::toString(SkString* str) const {

    str->appendf("%d colors: ", fColorCount);

    for (int i = 0; i < fColorCount; ++i) {
        str->appendHex(fOrigColors[i], 8);
        if (i < fColorCount-1) {
            str->append(", ");
        }
    }

    if (fColorCount > 2) {
        str->append(" points: (");
        for (int i = 0; i < fColorCount; ++i) {
            str->appendScalar(SkFixedToScalar(fRecs[i].fPos));
            if (i < fColorCount-1) {
                str->append(", ");
            }
        }
        str->append(")");
    }

    static const char* gTileModeName[SkShader::kTileModeCount] = {
        "clamp", "repeat", "mirror"
    };

    str->append(" ");
    str->append(gTileModeName[fTileMode]);

    this->INHERITED::toString(str);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Return true if these parameters are valid/legal/safe to construct a gradient
//
static bool valid_grad(const SkColor colors[], const SkScalar pos[], int count, unsigned tileMode) {
    return nullptr != colors && count >= 1 && tileMode < (unsigned)SkShader::kTileModeCount;
}

static void desc_init(SkGradientShaderBase::Descriptor* desc,
                      const SkColor colors[], const SkScalar pos[], int colorCount,
                      SkShader::TileMode mode, uint32_t flags, const SkMatrix* localMatrix) {
    SkASSERT(colorCount > 1);

    desc->fColors       = colors;
    desc->fPos          = pos;
    desc->fCount        = colorCount;
    desc->fTileMode     = mode;
    desc->fGradFlags    = flags;
    desc->fLocalMatrix  = localMatrix;
}

// assumes colors is SkColor* and pos is SkScalar*
#define EXPAND_1_COLOR(count)                \
     SkColor tmp[2];                         \
     do {                                    \
         if (1 == count) {                   \
             tmp[0] = tmp[1] = colors[0];    \
             colors = tmp;                   \
             pos = nullptr;                  \
             count = 2;                      \
         }                                   \
     } while (0)

struct ColorStopOptimizer {
    ColorStopOptimizer(const SkColor* colors, const SkScalar* pos,
                       int count, SkShader::TileMode mode)
        : fColors(colors)
        , fPos(pos)
        , fCount(count) {

            if (!pos || count != 3) {
                return;
            }

            if (SkScalarNearlyEqual(pos[0], 0.0f) &&
                SkScalarNearlyEqual(pos[1], 0.0f) &&
                SkScalarNearlyEqual(pos[2], 1.0f)) {

                if (SkShader::kRepeat_TileMode == mode ||
                    SkShader::kMirror_TileMode == mode ||
                    colors[0] == colors[1]) {

                    // Ignore the leftmost color/pos.
                    fColors += 1;
                    fPos    += 1;
                    fCount   = 2;
                }
            } else if (SkScalarNearlyEqual(pos[0], 0.0f) &&
                       SkScalarNearlyEqual(pos[1], 1.0f) &&
                       SkScalarNearlyEqual(pos[2], 1.0f)) {

                if (SkShader::kRepeat_TileMode == mode ||
                    SkShader::kMirror_TileMode == mode ||
                    colors[1] == colors[2]) {

                    // Ignore the rightmost color/pos.
                    fCount  = 2;
                }
            }
    }

    const SkColor*  fColors;
    const SkScalar* fPos;
    int             fCount;
};

sk_sp<SkShader> SkGradientShader::MakeLinear(const SkPoint pts[2],
                                             const SkColor colors[],
                                             const SkScalar pos[], int colorCount,
                                             SkShader::TileMode mode,
                                             uint32_t flags,
                                             const SkMatrix* localMatrix) {
    if (!pts || !SkScalarIsFinite((pts[1] - pts[0]).length())) {
        return nullptr;
    }
    if (!valid_grad(colors, pos, colorCount, mode)) {
        return nullptr;
    }
    if (1 == colorCount) {
        return SkShader::MakeColorShader(colors[0]);
    }

    ColorStopOptimizer opt(colors, pos, colorCount, mode);

    SkGradientShaderBase::Descriptor desc;
    desc_init(&desc, opt.fColors, opt.fPos, opt.fCount, mode, flags, localMatrix);
    return sk_make_sp<SkLinearGradient>(pts, desc);
}

sk_sp<SkShader> SkGradientShader::MakeRadial(const SkPoint& center, SkScalar radius,
                                         const SkColor colors[],
                                         const SkScalar pos[], int colorCount,
                                         SkShader::TileMode mode,
                                         uint32_t flags,
                                         const SkMatrix* localMatrix) {
    if (radius <= 0) {
        return nullptr;
    }
    if (!valid_grad(colors, pos, colorCount, mode)) {
        return nullptr;
    }
    if (1 == colorCount) {
        return SkShader::MakeColorShader(colors[0]);
    }

    ColorStopOptimizer opt(colors, pos, colorCount, mode);

    SkGradientShaderBase::Descriptor desc;
    desc_init(&desc, opt.fColors, opt.fPos, opt.fCount, mode, flags, localMatrix);
    return sk_make_sp<SkRadialGradient>(center, radius, desc);
}

sk_sp<SkShader> SkGradientShader::MakeTwoPointConical(const SkPoint& start,
                                                  SkScalar startRadius,
                                                  const SkPoint& end,
                                                  SkScalar endRadius,
                                                  const SkColor colors[],
                                                  const SkScalar pos[],
                                                  int colorCount,
                                                  SkShader::TileMode mode,
                                                  uint32_t flags,
                                                  const SkMatrix* localMatrix) {
    if (startRadius < 0 || endRadius < 0) {
        return nullptr;
    }
    if (!valid_grad(colors, pos, colorCount, mode)) {
        return nullptr;
    }
    if (startRadius == endRadius) {
        if (start == end || startRadius == 0) {
            return SkShader::MakeEmptyShader();
        }
    }
    EXPAND_1_COLOR(colorCount);

    ColorStopOptimizer opt(colors, pos, colorCount, mode);

    bool flipGradient = startRadius > endRadius;

    SkGradientShaderBase::Descriptor desc;

    if (!flipGradient) {
        desc_init(&desc, opt.fColors, opt.fPos, opt.fCount, mode, flags, localMatrix);
        return sk_make_sp<SkTwoPointConicalGradient>(start, startRadius, end, endRadius,
                                                     flipGradient, desc);
    } else {
        SkAutoSTArray<8, SkColor> colorsNew(opt.fCount);
        SkAutoSTArray<8, SkScalar> posNew(opt.fCount);
        for (int i = 0; i < opt.fCount; ++i) {
            colorsNew[i] = opt.fColors[opt.fCount - i - 1];
        }

        if (pos) {
            for (int i = 0; i < opt.fCount; ++i) {
                posNew[i] = 1 - opt.fPos[opt.fCount - i - 1];
            }
            desc_init(&desc, colorsNew.get(), posNew.get(), opt.fCount, mode, flags, localMatrix);
        } else {
            desc_init(&desc, colorsNew.get(), nullptr, opt.fCount, mode, flags, localMatrix);
        }

        return sk_make_sp<SkTwoPointConicalGradient>(end, endRadius, start, startRadius,
                                                     flipGradient, desc);
    }
}

sk_sp<SkShader> SkGradientShader::MakeSweep(SkScalar cx, SkScalar cy,
                                        const SkColor colors[],
                                        const SkScalar pos[],
                                        int colorCount,
                                        uint32_t flags,
                                        const SkMatrix* localMatrix) {
    if (!valid_grad(colors, pos, colorCount, SkShader::kClamp_TileMode)) {
        return nullptr;
    }
    if (1 == colorCount) {
        return SkShader::MakeColorShader(colors[0]);
    }

    auto mode = SkShader::kClamp_TileMode;

    ColorStopOptimizer opt(colors, pos, colorCount, mode);

    SkGradientShaderBase::Descriptor desc;
    desc_init(&desc, opt.fColors, opt.fPos, opt.fCount, mode, flags, localMatrix);
    return sk_make_sp<SkSweepGradient>(cx, cy, desc);
}

SK_DEFINE_FLATTENABLE_REGISTRAR_GROUP_START(SkGradientShader)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkLinearGradient)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkRadialGradient)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkSweepGradient)
    SK_DEFINE_FLATTENABLE_REGISTRAR_ENTRY(SkTwoPointConicalGradient)
SK_DEFINE_FLATTENABLE_REGISTRAR_GROUP_END

///////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU

#include "GrContext.h"
#include "GrInvariantOutput.h"
#include "GrTextureStripAtlas.h"
#include "gl/GrGLContext.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLProgramDataManager.h"
#include "glsl/GrGLSLUniformHandler.h"
#include "SkGr.h"

static inline bool close_to_one_half(const SkFixed& val) {
    return SkScalarNearlyEqual(SkFixedToScalar(val), SK_ScalarHalf);
}

static inline int color_type_to_color_count(GrGradientEffect::ColorType colorType) {
    switch (colorType) {
#if GR_GL_USE_ACCURATE_HARD_STOP_GRADIENTS
        case GrGradientEffect::kHardStopCentered_ColorType:
            return 4;
        case GrGradientEffect::kHardStopLeftEdged_ColorType:
        case GrGradientEffect::kHardStopRightEdged_ColorType:
            return 3;
#endif
        case GrGradientEffect::kTwo_ColorType:
            return 2;
        case GrGradientEffect::kThree_ColorType:
            return 3;
        case GrGradientEffect::kTexture_ColorType:
            return 0;
    }

    SkDEBUGFAIL("Unhandled ColorType in color_type_to_color_count()");
    return -1;
}

GrGradientEffect::ColorType GrGradientEffect::determineColorType(
        const SkGradientShaderBase& shader) {
#if GR_GL_USE_ACCURATE_HARD_STOP_GRADIENTS
    if (shader.fOrigPos) {
        if (4 == shader.fColorCount) {
            if (SkScalarNearlyEqual(shader.fOrigPos[0], 0.0f) &&
                SkScalarNearlyEqual(shader.fOrigPos[1], 0.5f) &&
                SkScalarNearlyEqual(shader.fOrigPos[2], 0.5f) &&
                SkScalarNearlyEqual(shader.fOrigPos[3], 1.0f)) {

                return kHardStopCentered_ColorType;
            }
        } else if (3 == shader.fColorCount) {
            if (SkScalarNearlyEqual(shader.fOrigPos[0], 0.0f) &&
                SkScalarNearlyEqual(shader.fOrigPos[1], 0.0f) &&
                SkScalarNearlyEqual(shader.fOrigPos[2], 1.0f)) {

                return kHardStopLeftEdged_ColorType;
            } else if (SkScalarNearlyEqual(shader.fOrigPos[0], 0.0f) &&
                       SkScalarNearlyEqual(shader.fOrigPos[1], 1.0f) &&
                       SkScalarNearlyEqual(shader.fOrigPos[2], 1.0f)) {
                
                return kHardStopRightEdged_ColorType;
            }
        }
    }
#endif

    if (SkShader::kClamp_TileMode == shader.getTileMode()) {
        if (2 == shader.fColorCount) {
            return kTwo_ColorType;
        } else if (3 == shader.fColorCount &&
                   close_to_one_half(shader.getRecs()[1].fPos)) {
            return kThree_ColorType;
        }
    }

    return kTexture_ColorType;
}

void GrGradientEffect::GLSLProcessor::emitUniforms(GrGLSLUniformHandler* uniformHandler,
                                                   const GrGradientEffect& ge) {
    if (int colorCount = color_type_to_color_count(ge.getColorType())) {
        fColorsUni = uniformHandler->addUniformArray(kFragment_GrShaderFlag,
                                                     kVec4f_GrSLType,
                                                     kDefault_GrSLPrecision,
                                                     "Colors",
                                                     colorCount);
    } else {
        fFSYUni = uniformHandler->addUniform(kFragment_GrShaderFlag,
                                             kFloat_GrSLType, kDefault_GrSLPrecision,
                                             "GradientYCoordFS");
    }
}

static inline void set_after_interp_color_uni_array(const GrGLSLProgramDataManager& pdman,
                                       const GrGLSLProgramDataManager::UniformHandle uni,
                                       const SkTDArray<SkColor>& colors) {
    int count = colors.count();
    constexpr int kSmallCount = 10;

    SkAutoSTArray<4*kSmallCount, float> vals(4*count);

    for (int i = 0; i < colors.count(); i++) {
        // RGBA
        vals[4*i + 0] = SkColorGetR(colors[i]) / 255.f;
        vals[4*i + 1] = SkColorGetG(colors[i]) / 255.f;
        vals[4*i + 2] = SkColorGetB(colors[i]) / 255.f;
        vals[4*i + 3] = SkColorGetA(colors[i]) / 255.f;
    }

    pdman.set4fv(uni, colors.count(), vals.get());
}

static inline void set_before_interp_color_uni_array(const GrGLSLProgramDataManager& pdman,
                                              const GrGLSLProgramDataManager::UniformHandle uni,
                                              const SkTDArray<SkColor>& colors) {
    int count = colors.count();
    constexpr int kSmallCount = 10;

    SkAutoSTArray<4*kSmallCount, float> vals(4*count);

    for (int i = 0; i < count; i++) {
        float a = SkColorGetA(colors[i]) / 255.f;
        float aDiv255 = a / 255.f;

        // RGBA
        vals[4*i + 0] = SkColorGetR(colors[i]) * aDiv255;
        vals[4*i + 1] = SkColorGetG(colors[i]) * aDiv255;
        vals[4*i + 2] = SkColorGetB(colors[i]) * aDiv255;
        vals[4*i + 3] = a;
    }

    pdman.set4fv(uni, count, vals.get());
}

void GrGradientEffect::GLSLProcessor::onSetData(const GrGLSLProgramDataManager& pdman,
                                                const GrProcessor& processor) {
    const GrGradientEffect& e = processor.cast<GrGradientEffect>();

    switch (e.getColorType()) {
#if GR_GL_USE_ACCURATE_HARD_STOP_GRADIENTS
        case GrGradientEffect::kHardStopCentered_ColorType:
        case GrGradientEffect::kHardStopLeftEdged_ColorType:
        case GrGradientEffect::kHardStopRightEdged_ColorType:
#endif
        case GrGradientEffect::kTwo_ColorType:
        case GrGradientEffect::kThree_ColorType: {
            if (GrGradientEffect::kBeforeInterp_PremulType == e.getPremulType()) {
                set_before_interp_color_uni_array(pdman, fColorsUni, e.fColors);
            } else {
                set_after_interp_color_uni_array(pdman, fColorsUni, e.fColors);
            }

            break;
        }

        case GrGradientEffect::kTexture_ColorType: {
            SkScalar yCoord = e.getYCoord();
            if (yCoord != fCachedYCoord) {
                pdman.set1f(fFSYUni, yCoord);
                fCachedYCoord = yCoord;
            }
            break;
        }
    }
}

uint32_t GrGradientEffect::GLSLProcessor::GenBaseGradientKey(const GrProcessor& processor) {
    const GrGradientEffect& e = processor.cast<GrGradientEffect>();

    uint32_t key = 0;

    if (GrGradientEffect::kBeforeInterp_PremulType == e.getPremulType()) {
        key |= kPremulBeforeInterpKey;
    }

    if (GrGradientEffect::kTwo_ColorType == e.getColorType()) {
        key |= kTwoColorKey;
    } else if (GrGradientEffect::kThree_ColorType == e.getColorType()) {
        key |= kThreeColorKey;
    }
#if GR_GL_USE_ACCURATE_HARD_STOP_GRADIENTS
    else if (GrGradientEffect::kHardStopCentered_ColorType == e.getColorType()) {
        key |= kHardStopCenteredKey;
    } else if (GrGradientEffect::kHardStopLeftEdged_ColorType == e.getColorType()) {
        key |= kHardStopZeroZeroOneKey;
    } else if (GrGradientEffect::kHardStopRightEdged_ColorType == e.getColorType()) {
        key |= kHardStopZeroOneOneKey;
    }
   
    if (SkShader::TileMode::kClamp_TileMode == e.fTileMode) {
        key |= kClampTileMode;
    } else if (SkShader::TileMode::kRepeat_TileMode == e.fTileMode) {
        key |= kRepeatTileMode;
    } else {
        key |= kMirrorTileMode;
    }
#endif

    return key;
}

void GrGradientEffect::GLSLProcessor::emitColor(GrGLSLFPFragmentBuilder* fragBuilder,
                                                GrGLSLUniformHandler* uniformHandler,
                                                const GrGLSLCaps* glslCaps,
                                                const GrGradientEffect& ge,
                                                const char* gradientTValue,
                                                const char* outputColor,
                                                const char* inputColor,
                                                const SamplerHandle* texSamplers) {
    switch (ge.getColorType()) {
#if GR_GL_USE_ACCURATE_HARD_STOP_GRADIENTS
        case kHardStopCentered_ColorType: {
            const char* t      = gradientTValue;
            const char* colors = uniformHandler->getUniformCStr(fColorsUni);

            fragBuilder->codeAppendf("float clamp_t = clamp(%s, 0.0, 1.0);", t);

            // Account for tile mode
            if (SkShader::kRepeat_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("clamp_t = fract(%s);", t);
            } else if (SkShader::kMirror_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("if (%s < 0.0 || %s > 1.0) {", t, t);
                fragBuilder->codeAppendf("    if (mod(floor(%s), 2.0) == 0.0) {", t);
                fragBuilder->codeAppendf("        clamp_t = fract(%s);", t);
                fragBuilder->codeAppendf("    } else {");
                fragBuilder->codeAppendf("        clamp_t = 1.0 - fract(%s);", t);
                fragBuilder->codeAppendf("    }");
                fragBuilder->codeAppendf("}");
            }

            // Calculate color
            fragBuilder->codeAppendf("float relative_t = fract(2.0 * clamp_t);");
            if (SkShader::kClamp_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("relative_t += step(1.0, %s);", t);
            }

            fragBuilder->codeAppendf("vec4 start = %s[0];", colors);
            fragBuilder->codeAppendf("vec4 end   = %s[1];", colors);
            fragBuilder->codeAppendf("if (clamp_t >= 0.5) {");
            fragBuilder->codeAppendf("    start = %s[2];", colors);
            fragBuilder->codeAppendf("    end   = %s[3];", colors);
            fragBuilder->codeAppendf("}");
            fragBuilder->codeAppendf("vec4 colorTemp = mix(start, end, relative_t);");

            if (GrGradientEffect::kAfterInterp_PremulType == ge.getPremulType()) {
                fragBuilder->codeAppend("colorTemp.rgb *= colorTemp.a;");
            }
            fragBuilder->codeAppendf("%s = %s;", outputColor,
                                     (GrGLSLExpr4(inputColor) * GrGLSLExpr4("colorTemp")).c_str());

            break;
        }

        case kHardStopLeftEdged_ColorType: {
            const char* t      = gradientTValue;
            const char* colors = uniformHandler->getUniformCStr(fColorsUni);

            fragBuilder->codeAppendf("float clamp_t = clamp(%s, 0.0, 1.0);", t);

            // Account for tile mode
            if (SkShader::kRepeat_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("clamp_t = fract(%s);", t);
            } else if (SkShader::kMirror_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("if (%s < 0.0 || %s > 1.0) {", t, t);
                fragBuilder->codeAppendf("    if (mod(floor(%s), 2.0) == 0.0) {", t);
                fragBuilder->codeAppendf("        clamp_t = fract(%s);", t);
                fragBuilder->codeAppendf("    } else {");
                fragBuilder->codeAppendf("        clamp_t = 1.0 - fract(%s);", t);
                fragBuilder->codeAppendf("    }");
                fragBuilder->codeAppendf("}");
            }

            fragBuilder->codeAppendf("vec4 colorTemp = mix(%s[1], %s[2], clamp_t);", colors,
                                     colors);
            if (SkShader::kClamp_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("if (%s < 0.0) {", t);
                fragBuilder->codeAppendf("    colorTemp = %s[0];", colors);
                fragBuilder->codeAppendf("}");
            }

            if (GrGradientEffect::kAfterInterp_PremulType == ge.getPremulType()) {
                fragBuilder->codeAppend("colorTemp.rgb *= colorTemp.a;");
            }
            fragBuilder->codeAppendf("%s = %s;", outputColor,
                                     (GrGLSLExpr4(inputColor) * GrGLSLExpr4("colorTemp")).c_str());

            break;
        }

        case kHardStopRightEdged_ColorType: {
            const char* t      = gradientTValue;
            const char* colors = uniformHandler->getUniformCStr(fColorsUni);

            fragBuilder->codeAppendf("float clamp_t = clamp(%s, 0.0, 1.0);", t);

            // Account for tile mode
            if (SkShader::kRepeat_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("clamp_t = fract(%s);", t);
            } else if (SkShader::kMirror_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("if (%s < 0.0 || %s > 1.0) {", t, t);
                fragBuilder->codeAppendf("    if (mod(floor(%s), 2.0) == 0.0) {", t);
                fragBuilder->codeAppendf("        clamp_t = fract(%s);", t);
                fragBuilder->codeAppendf("    } else {");
                fragBuilder->codeAppendf("        clamp_t = 1.0 - fract(%s);", t);
                fragBuilder->codeAppendf("    }");
                fragBuilder->codeAppendf("}");
            }

            fragBuilder->codeAppendf("vec4 colorTemp = mix(%s[0], %s[1], clamp_t);", colors,
                                     colors);
            if (SkShader::kClamp_TileMode == ge.fTileMode) {
                fragBuilder->codeAppendf("if (%s > 1.0) {", t);
                fragBuilder->codeAppendf("    colorTemp = %s[2];", colors);
                fragBuilder->codeAppendf("}");
            }

            if (GrGradientEffect::kAfterInterp_PremulType == ge.getPremulType()) {
                fragBuilder->codeAppend("colorTemp.rgb *= colorTemp.a;");
            }
            fragBuilder->codeAppendf("%s = %s;", outputColor,
                                     (GrGLSLExpr4(inputColor) * GrGLSLExpr4("colorTemp")).c_str());

            break;
        }
#endif

        case kTwo_ColorType: {
            const char* t      = gradientTValue;
            const char* colors = uniformHandler->getUniformCStr(fColorsUni);

            fragBuilder->codeAppendf("vec4 colorTemp = mix(%s[0], %s[1], clamp(%s, 0.0, 1.0));",
                                     colors, colors, t);

            // We could skip this step if both colors are known to be opaque. Two
            // considerations:
            // The gradient SkShader reporting opaque is more restrictive than necessary in the two
            // pt case. Make sure the key reflects this optimization (and note that it can use the
            // same shader as thekBeforeIterp case). This same optimization applies to the 3 color
            // case below.
            if (GrGradientEffect::kAfterInterp_PremulType == ge.getPremulType()) {
                fragBuilder->codeAppend("colorTemp.rgb *= colorTemp.a;");
            }

            fragBuilder->codeAppendf("%s = %s;", outputColor,
                                     (GrGLSLExpr4(inputColor) * GrGLSLExpr4("colorTemp")).c_str());

            break;
        }

        case kThree_ColorType: {
            const char* t      = gradientTValue;
            const char* colors = uniformHandler->getUniformCStr(fColorsUni);

            fragBuilder->codeAppendf("float oneMinus2t = 1.0 - (2.0 * %s);", t);
            fragBuilder->codeAppendf("vec4 colorTemp = clamp(oneMinus2t, 0.0, 1.0) * %s[0];",
                                     colors);
            if (!glslCaps->canUseMinAndAbsTogether()) {
                // The Tegra3 compiler will sometimes never return if we have
                // min(abs(oneMinus2t), 1.0), or do the abs first in a separate expression.
                fragBuilder->codeAppendf("float minAbs = abs(oneMinus2t);");
                fragBuilder->codeAppendf("minAbs = minAbs > 1.0 ? 1.0 : minAbs;");
                fragBuilder->codeAppendf("colorTemp += (1.0 - minAbs) * %s[1];", colors);
            } else {
                fragBuilder->codeAppendf("colorTemp += (1.0 - min(abs(oneMinus2t), 1.0)) * %s[1];",
                                         colors);
            }
            fragBuilder->codeAppendf("colorTemp += clamp(-oneMinus2t, 0.0, 1.0) * %s[2];", colors);

            if (GrGradientEffect::kAfterInterp_PremulType == ge.getPremulType()) {
                fragBuilder->codeAppend("colorTemp.rgb *= colorTemp.a;");
            }

            fragBuilder->codeAppendf("%s = %s;", outputColor,
                                     (GrGLSLExpr4(inputColor) * GrGLSLExpr4("colorTemp")).c_str());

            break;
        }

        case kTexture_ColorType: {
            const char* fsyuni = uniformHandler->getUniformCStr(fFSYUni);

            fragBuilder->codeAppendf("vec2 coord = vec2(%s, %s);", gradientTValue, fsyuni);
            fragBuilder->codeAppendf("%s = ", outputColor);
            fragBuilder->appendTextureLookupAndModulate(inputColor, texSamplers[0], "coord");
            fragBuilder->codeAppend(";");

            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////

GrGradientEffect::GrGradientEffect(GrContext* ctx,
                                   const SkGradientShaderBase& shader,
                                   const SkMatrix& matrix,
                                   SkShader::TileMode tileMode) {

    fIsOpaque = shader.isOpaque();

    fColorType = this->determineColorType(shader);

    if (kTexture_ColorType != fColorType) {
        if (shader.fOrigColors) {
            fColors = SkTDArray<SkColor>(shader.fOrigColors, shader.fColorCount);
        }

#if GR_GL_USE_ACCURATE_HARD_STOP_GRADIENTS
        if (shader.fOrigPos) {
            fPositions = SkTDArray<SkScalar>(shader.fOrigPos, shader.fColorCount);
        }
#endif
    }

#if GR_GL_USE_ACCURATE_HARD_STOP_GRADIENTS
    fTileMode = tileMode;
#endif

    switch (fColorType) {
        // The two and three color specializations do not currently support tiling.
        case kTwo_ColorType:
        case kThree_ColorType:
#if GR_GL_USE_ACCURATE_HARD_STOP_GRADIENTS
        case kHardStopLeftEdged_ColorType:
        case kHardStopRightEdged_ColorType:
        case kHardStopCentered_ColorType:
#endif
            fRow = -1;

            if (SkGradientShader::kInterpolateColorsInPremul_Flag & shader.getGradFlags()) {
                fPremulType = kBeforeInterp_PremulType;
            } else {
                fPremulType = kAfterInterp_PremulType;
            }

            fCoordTransform.reset(kCoordSet, matrix);

            break;
        case kTexture_ColorType:
            // doesn't matter how this is set, just be consistent because it is part of the
            // effect key.
            fPremulType = kBeforeInterp_PremulType;

            SkBitmap bitmap;
            shader.getGradientTableBitmap(&bitmap);

            GrTextureStripAtlas::Desc desc;
            desc.fWidth  = bitmap.width();
            desc.fHeight = 32;
            desc.fRowHeight = bitmap.height();
            desc.fContext = ctx;
            desc.fConfig = SkImageInfo2GrPixelConfig(bitmap.info(), *ctx->caps());
            fAtlas = GrTextureStripAtlas::GetAtlas(desc);
            SkASSERT(fAtlas);

            // We always filter the gradient table. Each table is one row of a texture, always
            // y-clamp.
            GrTextureParams params;
            params.setFilterMode(GrTextureParams::kBilerp_FilterMode);
            params.setTileModeX(tileMode);

            fRow = fAtlas->lockRow(bitmap);
            if (-1 != fRow) {
                fYCoord = fAtlas->getYOffset(fRow)+SK_ScalarHalf*fAtlas->getNormalizedTexelHeight();
                fCoordTransform.reset(kCoordSet, matrix, fAtlas->getTexture(), params.filterMode());
                fTextureAccess.reset(fAtlas->getTexture(), params);
            } else {
                SkAutoTUnref<GrTexture> texture(
                    GrRefCachedBitmapTexture(ctx, bitmap, params,
                                             SkSourceGammaTreatment::kRespect));
                if (!texture) {
                    return;
                }
                fCoordTransform.reset(kCoordSet, matrix, texture, params.filterMode());
                fTextureAccess.reset(texture, params);
                fYCoord = SK_ScalarHalf;
            }

            this->addTextureAccess(&fTextureAccess);

            break;
    }

    this->addCoordTransform(&fCoordTransform);
}

GrGradientEffect::~GrGradientEffect() {
    if (this->useAtlas()) {
        fAtlas->unlockRow(fRow);
    }
}

bool GrGradientEffect::onIsEqual(const GrFragmentProcessor& processor) const {
    const GrGradientEffect& ge = processor.cast<GrGradientEffect>();

    if (this->fColorType == ge.getColorType()) {
        if (kTexture_ColorType == fColorType) {
            if (fYCoord != ge.getYCoord()) {
                return false;
            }
        } else {
            if (this->getPremulType() != ge.getPremulType() ||
                this->fColors.count() != ge.fColors.count()) {
                return false;
            }

            for (int i = 0; i < this->fColors.count(); i++) {
                if (*this->getColors(i) != *ge.getColors(i)) {
                    return false;
                }
            }
        }

        SkASSERT(this->useAtlas() == ge.useAtlas());
        return true;
    }

    return false;
}

void GrGradientEffect::onComputeInvariantOutput(GrInvariantOutput* inout) const {
    if (fIsOpaque) {
        inout->mulByUnknownOpaqueFourComponents();
    } else {
        inout->mulByUnknownFourComponents();
    }
}

int GrGradientEffect::RandomGradientParams(SkRandom* random,
                                           SkColor colors[],
                                           SkScalar** stops,
                                           SkShader::TileMode* tm) {
    int outColors = random->nextRangeU(1, kMaxRandomGradientColors);

    // if one color, omit stops, otherwise randomly decide whether or not to
    if (outColors == 1 || (outColors >= 2 && random->nextBool())) {
        *stops = nullptr;
    }

    SkScalar stop = 0.f;
    for (int i = 0; i < outColors; ++i) {
        colors[i] = random->nextU();
        if (*stops) {
            (*stops)[i] = stop;
            stop = i < outColors - 1 ? stop + random->nextUScalar1() * (1.f - stop) : 1.f;
        }
    }
    *tm = static_cast<SkShader::TileMode>(random->nextULessThan(SkShader::kTileModeCount));

    return outColors;
}

#endif
