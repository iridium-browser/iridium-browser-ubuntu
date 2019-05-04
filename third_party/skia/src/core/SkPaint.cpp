/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPaint.h"

#include "SkColorFilter.h"
#include "SkColorSpacePriv.h"
#include "SkColorSpaceXformSteps.h"
#include "SkData.h"
#include "SkDraw.h"
#include "SkGraphics.h"
#include "SkImageFilter.h"
#include "SkMaskFilter.h"
#include "SkMaskGamma.h"
#include "SkMutex.h"
#include "SkOpts.h"
#include "SkPaintDefaults.h"
#include "SkPaintPriv.h"
#include "SkPathEffect.h"
#include "SkReadBuffer.h"
#include "SkSafeRange.h"
#include "SkScalar.h"
#include "SkShader.h"
#include "SkShaderBase.h"
#include "SkStringUtils.h"
#include "SkStroke.h"
#include "SkStrokeRec.h"
#include "SkSurfacePriv.h"
#include "SkTLazy.h"
#include "SkTo.h"
#include "SkTypeface.h"
#include "SkWriteBuffer.h"

static inline uint32_t set_clear_mask(uint32_t bits, bool cond, uint32_t mask) {
    return cond ? bits | mask : bits & ~mask;
}

// define this to get a printf for out-of-range parameter in setters
// e.g. setTextSize(-1)
//#define SK_REPORT_API_RANGE_CHECK

SkPaint::SkPaint() {
    fTextSize   = SkPaintDefaults_TextSize;
    fTextScaleX = SK_Scalar1;
    fTextSkewX  = 0;
    fColor4f    = { 0, 0, 0, 1 }; // opaque black
    fWidth      = 0;
    fMiterLimit = SkPaintDefaults_MiterLimit;
    fBlendMode  = (unsigned)SkBlendMode::kSrcOver;

    // Zero all bitfields, then set some non-zero defaults.
    fBitfieldsUInt           = 0;
    fBitfields.fFlags        = SkPaintDefaults_Flags;
    fBitfields.fCapType      = kDefault_Cap;
    fBitfields.fJoinType     = kDefault_Join;
    fBitfields.fStyle        = kFill_Style;
    fBitfields.fTextEncoding = static_cast<unsigned>(kUTF8_SkTextEncoding);
    fBitfields.fHinting      = static_cast<unsigned>(SkPaintDefaults_Hinting);
}

SkPaint::SkPaint(const SkPaint& src)
#define COPY(field) field(src.field)
    : COPY(fTypeface)
    , COPY(fPathEffect)
    , COPY(fShader)
    , COPY(fMaskFilter)
    , COPY(fColorFilter)
    , COPY(fDrawLooper)
    , COPY(fImageFilter)
    , COPY(fTextSize)
    , COPY(fTextScaleX)
    , COPY(fTextSkewX)
    , COPY(fColor4f)
    , COPY(fWidth)
    , COPY(fMiterLimit)
    , COPY(fBlendMode)
    , COPY(fBitfields)
#undef COPY
{}

SkPaint::SkPaint(SkPaint&& src) {
#define MOVE(field) field = std::move(src.field)
    MOVE(fTypeface);
    MOVE(fPathEffect);
    MOVE(fShader);
    MOVE(fMaskFilter);
    MOVE(fColorFilter);
    MOVE(fDrawLooper);
    MOVE(fImageFilter);
    MOVE(fTextSize);
    MOVE(fTextScaleX);
    MOVE(fTextSkewX);
    MOVE(fColor4f);
    MOVE(fWidth);
    MOVE(fMiterLimit);
    MOVE(fBlendMode);
    MOVE(fBitfields);
#undef MOVE
}

SkPaint::~SkPaint() {}

SkPaint& SkPaint::operator=(const SkPaint& src) {
    if (this == &src) {
        return *this;
    }

#define ASSIGN(field) field = src.field
    ASSIGN(fTypeface);
    ASSIGN(fPathEffect);
    ASSIGN(fShader);
    ASSIGN(fMaskFilter);
    ASSIGN(fColorFilter);
    ASSIGN(fDrawLooper);
    ASSIGN(fImageFilter);
    ASSIGN(fTextSize);
    ASSIGN(fTextScaleX);
    ASSIGN(fTextSkewX);
    ASSIGN(fColor4f);
    ASSIGN(fWidth);
    ASSIGN(fMiterLimit);
    ASSIGN(fBlendMode);
    ASSIGN(fBitfields);
#undef ASSIGN

    return *this;
}

SkPaint& SkPaint::operator=(SkPaint&& src) {
    if (this == &src) {
        return *this;
    }

#define MOVE(field) field = std::move(src.field)
    MOVE(fTypeface);
    MOVE(fPathEffect);
    MOVE(fShader);
    MOVE(fMaskFilter);
    MOVE(fColorFilter);
    MOVE(fDrawLooper);
    MOVE(fImageFilter);
    MOVE(fTextSize);
    MOVE(fTextScaleX);
    MOVE(fTextSkewX);
    MOVE(fColor4f);
    MOVE(fWidth);
    MOVE(fMiterLimit);
    MOVE(fBlendMode);
    MOVE(fBitfields);
#undef MOVE

    return *this;
}

bool operator==(const SkPaint& a, const SkPaint& b) {
#define EQUAL(field) (a.field == b.field)
    return EQUAL(fTypeface)
        && EQUAL(fPathEffect)
        && EQUAL(fShader)
        && EQUAL(fMaskFilter)
        && EQUAL(fColorFilter)
        && EQUAL(fDrawLooper)
        && EQUAL(fImageFilter)
        && EQUAL(fTextSize)
        && EQUAL(fTextScaleX)
        && EQUAL(fTextSkewX)
        && EQUAL(fColor4f)
        && EQUAL(fWidth)
        && EQUAL(fMiterLimit)
        && EQUAL(fBlendMode)
        && EQUAL(fBitfieldsUInt)
        ;
#undef EQUAL
}

#define DEFINE_REF_FOO(type)    sk_sp<Sk##type> SkPaint::ref##type() const { return f##type; }
DEFINE_REF_FOO(ColorFilter)
DEFINE_REF_FOO(DrawLooper)
DEFINE_REF_FOO(ImageFilter)
DEFINE_REF_FOO(MaskFilter)
DEFINE_REF_FOO(PathEffect)
DEFINE_REF_FOO(Shader)
DEFINE_REF_FOO(Typeface)
#undef DEFINE_REF_FOO

void SkPaint::reset() {
    SkPaint init;
    *this = init;
}

void SkPaint::setFilterQuality(SkFilterQuality quality) {
    fBitfields.fFilterQuality = quality;
}

void SkPaint::setHinting(SkFontHinting hintingLevel) {
    fBitfields.fHinting = static_cast<unsigned>(hintingLevel);
}

#ifdef SK_SUPPORT_LEGACY_PAINT_FLAGS
void SkPaint::setFlags(uint32_t flags) {
    fBitfields.fFlags = flags;
}
#endif

void SkPaint::setAntiAlias(bool doAA) {
    this->internal_setFlags(set_clear_mask(fBitfields.fFlags, doAA, kAntiAlias_Flag));
}

void SkPaint::setDither(bool doDither) {
    this->internal_setFlags(set_clear_mask(fBitfields.fFlags, doDither, kDither_Flag));
}

#ifdef SK_SUPPORT_LEGACY_PAINT_FONT_FIELDS
void SkPaint::setSubpixelText(bool doSubpixel) {
    this->internal_setFlags(set_clear_mask(fBitfields.fFlags, doSubpixel, kSubpixelText_Flag));
}

void SkPaint::setLCDRenderText(bool doLCDRender) {
    this->internal_setFlags(set_clear_mask(fBitfields.fFlags, doLCDRender, kLCDRenderText_Flag));
}

void SkPaint::setEmbeddedBitmapText(bool doEmbeddedBitmapText) {
    this->internal_setFlags(set_clear_mask(fBitfields.fFlags, doEmbeddedBitmapText, kEmbeddedBitmapText_Flag));
}

void SkPaint::setAutohinted(bool useAutohinter) {
    this->internal_setFlags(set_clear_mask(fBitfields.fFlags, useAutohinter, kAutoHinting_Flag));
}

void SkPaint::setLinearText(bool doLinearText) {
    this->internal_setFlags(set_clear_mask(fBitfields.fFlags, doLinearText, kLinearText_Flag));
}

void SkPaint::setFakeBoldText(bool doFakeBold) {
    this->internal_setFlags(set_clear_mask(fBitfields.fFlags, doFakeBold, kFakeBoldText_Flag));
}
#endif

void SkPaint::setStyle(Style style) {
    if ((unsigned)style < kStyleCount) {
        fBitfields.fStyle = style;
    } else {
#ifdef SK_REPORT_API_RANGE_CHECK
        SkDebugf("SkPaint::setStyle(%d) out of range\n", style);
#endif
    }
}

void SkPaint::setColor(SkColor color) {
    fColor4f = SkColor4f::FromColor(color);
}

void SkPaint::setColor4f(const SkColor4f& color, SkColorSpace* colorSpace) {
    SkColorSpaceXformSteps steps{colorSpace,          kUnpremul_SkAlphaType,
                                 sk_srgb_singleton(), kUnpremul_SkAlphaType};
    fColor4f = color;
    steps.apply(fColor4f.vec());
}

void SkPaint::setAlpha(U8CPU a) {
    SkASSERT(a <= 255);
    fColor4f.fA = a * (1.0f / 255);
}

void SkPaint::setARGB(U8CPU a, U8CPU r, U8CPU g, U8CPU b) {
    this->setColor(SkColorSetARGB(a, r, g, b));
}

void SkPaint::setStrokeWidth(SkScalar width) {
    if (width >= 0) {
        fWidth = width;
    } else {
#ifdef SK_REPORT_API_RANGE_CHECK
        SkDebugf("SkPaint::setStrokeWidth() called with negative value\n");
#endif
    }
}

void SkPaint::setStrokeMiter(SkScalar limit) {
    if (limit >= 0) {
        fMiterLimit = limit;
    } else {
#ifdef SK_REPORT_API_RANGE_CHECK
        SkDebugf("SkPaint::setStrokeMiter() called with negative value\n");
#endif
    }
}

void SkPaint::setStrokeCap(Cap ct) {
    if ((unsigned)ct < kCapCount) {
        fBitfields.fCapType = SkToU8(ct);
    } else {
#ifdef SK_REPORT_API_RANGE_CHECK
        SkDebugf("SkPaint::setStrokeCap(%d) out of range\n", ct);
#endif
    }
}

void SkPaint::setStrokeJoin(Join jt) {
    if ((unsigned)jt < kJoinCount) {
        fBitfields.fJoinType = SkToU8(jt);
    } else {
#ifdef SK_REPORT_API_RANGE_CHECK
        SkDebugf("SkPaint::setStrokeJoin(%d) out of range\n", jt);
#endif
    }
}

///////////////////////////////////////////////////////////////////////////////

void SkPaint::setTextSize(SkScalar ts) {
    if (ts >= 0) {
        fTextSize = ts;
    } else {
#ifdef SK_REPORT_API_RANGE_CHECK
        SkDebugf("SkPaint::setTextSize() called with negative value\n");
#endif
    }
}

void SkPaint::setTextScaleX(SkScalar scaleX) {
    fTextScaleX = scaleX;
}

void SkPaint::setTextSkewX(SkScalar skewX) {
    fTextSkewX = skewX;
}

#ifdef SK_SUPPORT_LEGACY_PAINTTEXTENCODING
void SkPaint::setTextEncoding(SkTextEncoding encoding) {
    if ((unsigned)encoding <= (unsigned)kGlyphID_SkTextEncoding) {
        fBitfields.fTextEncoding = (unsigned)encoding;
    } else {
#ifdef SK_REPORT_API_RANGE_CHECK
        SkDebugf("SkPaint::setTextEncoding(%d) out of range\n", encoding);
#endif
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////

#define MOVE_FIELD(Field) void SkPaint::set##Field(sk_sp<Sk##Field> f) { f##Field = std::move(f); }
MOVE_FIELD(Typeface)
MOVE_FIELD(ImageFilter)
MOVE_FIELD(Shader)
MOVE_FIELD(ColorFilter)
MOVE_FIELD(PathEffect)
MOVE_FIELD(MaskFilter)
MOVE_FIELD(DrawLooper)
#undef MOVE_FIELD
void SkPaint::setLooper(sk_sp<SkDrawLooper> looper) { fDrawLooper = std::move(looper); }

///////////////////////////////////////////////////////////////////////////////

#include "SkStream.h"

static uintptr_t asint(const void* p) {
    return reinterpret_cast<uintptr_t>(p);
}

#ifdef SK_DEBUG
    static void ASSERT_FITS_IN(uint32_t value, int bitCount) {
        SkASSERT(bitCount > 0 && bitCount <= 32);
        uint32_t mask = ~0U;
        mask >>= (32 - bitCount);
        SkASSERT(0 == (value & ~mask));
    }
#else
    #define ASSERT_FITS_IN(value, bitcount)
#endif

enum FlatFlags {
    kHasTypeface_FlatFlag = 0x1,
    kHasEffects_FlatFlag  = 0x2,

    kFlatFlagMask         = 0x3,
};

enum BitsPerField {
    kFlags_BPF  = 16,
    kHint_BPF   = 2,
    kFilter_BPF = 2,
    kFlatFlags_BPF  = 3,
};

static inline int BPF_Mask(int bits) {
    return (1 << bits) - 1;
}

// SkPaint originally defined flags, some of which now apply to SkFont. These are renames
// of those flags, split into categories depending on which objects they (now) apply to.

enum PaintFlagsForPaint {
    kAA_PaintFlagForPaint     = 0x01,
    kDither_PaintFlagForPaint = 0x04,
};

enum PaintFlagsForFont {
    kFakeBold_PaintFlagForFont       = 0x20,
    kLinear_PaintFlagForFont         = 0x40,
    kSubpixel_PaintFlagForFont       = 0x80,
    kLCD_PaintFlagForFont            = 0x200,
    kEmbeddedBitmap_PaintFlagForFont = 0x400,
    kAutoHinting_PaintFlagForFont    = 0x800,
};

static FlatFlags unpack_paint_flags(SkPaint* paint, uint32_t packed, SkFont* font) {
    uint32_t f = packed >> 16;
#ifdef SK_SUPPORT_LEGACY_PAINT_FONT_FIELDS
    paint->setFlags(f);
#else
    paint->setAntiAlias((f & kAA_PaintFlagForPaint) != 0);
    paint->setDither((f & kDither_PaintFlagForPaint) != 0);
#endif
    if (font) {
        font->setEmbolden((f & kFakeBold_PaintFlagForFont) != 0);
        font->setLinearMetrics((f & kLinear_PaintFlagForFont) != 0);
        font->setSubpixel((f & kSubpixel_PaintFlagForFont) != 0);
        font->setEmbeddedBitmaps((f & kEmbeddedBitmap_PaintFlagForFont) != 0);
        font->setForceAutoHinting((f & kAutoHinting_PaintFlagForFont) != 0);

        font->setHinting((SkFontHinting)((packed >> 14) & BPF_Mask(kHint_BPF)));

        if (f & kAA_PaintFlagForPaint) {
            if (f & kLCD_PaintFlagForFont) {
                font->setEdging(SkFont::Edging::kSubpixelAntiAlias);
            } else {
                font->setEdging(SkFont::Edging::kAntiAlias);
            }
        } else {
            font->setEdging(SkFont::Edging::kAlias);
        }
    }

    paint->setHinting((SkFontHinting)((packed >> 14) & BPF_Mask(kHint_BPF)));
    paint->setFilterQuality((SkFilterQuality)((packed >> 10) & BPF_Mask(kFilter_BPF)));
    return (FlatFlags)(packed & kFlatFlagMask);
}

template <typename T> uint32_t shift_bits(T value, unsigned shift, unsigned bits) {
    SkASSERT(shift + bits <= 32);
    uint32_t v = static_cast<uint32_t>(value);
    ASSERT_FITS_IN(v, bits);
    return v << shift;
}

/*  Packing the paint
 flags :  8  // 2...
 blend :  8  // 30+
 cap   :  2  // 3
 join  :  2  // 3
 style :  2  // 3
 filter:  2  // 4
 flat  :  8  // 1...
 total : 32
 */
static uint32_t pack_v68(const SkPaint& paint, unsigned flatFlags) {
    uint32_t packed = 0;
    packed |= shift_bits(((unsigned)paint.isDither() << 1) |
                          (unsigned)paint.isAntiAlias(), 0, 8);
    packed |= shift_bits(paint.getBlendMode(),      8, 8);
    packed |= shift_bits(paint.getStrokeCap(),     16, 2);
    packed |= shift_bits(paint.getStrokeJoin(),    18, 2);
    packed |= shift_bits(paint.getStyle(),         20, 2);
    packed |= shift_bits(paint.getFilterQuality(), 22, 2);
    packed |= shift_bits(flatFlags,                24, 8);
    return packed;
}

static uint32_t unpack_v68(SkPaint* paint, uint32_t packed, SkSafeRange& safe) {
    paint->setAntiAlias((packed & 1) != 0);
    paint->setDither((packed & 2) != 0);
    packed >>= 8;
    paint->setBlendMode(safe.checkLE(packed & 0xFF, SkBlendMode::kLastMode));
    packed >>= 8;
    paint->setStrokeCap(safe.checkLE(packed & 0x3, SkPaint::kLast_Cap));
    packed >>= 2;
    paint->setStrokeJoin(safe.checkLE(packed & 0x3, SkPaint::kLast_Join));
    packed >>= 2;
    paint->setStyle(safe.checkLE(packed & 0x3, SkPaint::kStrokeAndFill_Style));
    packed >>= 2;
    paint->setFilterQuality(safe.checkLE(packed & 0x3, kLast_SkFilterQuality));
    packed >>= 2;
    return packed;
}

/*  To save space/time, we analyze the paint, and write a truncated version of
    it if there are not tricky elements like shaders, etc.
 */
void SkPaintPriv::Flatten(const SkPaint& paint, SkWriteBuffer& buffer) {
    uint8_t flatFlags = 0;

    if (asint(paint.getPathEffect()) |
        asint(paint.getShader()) |
        asint(paint.getMaskFilter()) |
        asint(paint.getColorFilter()) |
        asint(paint.getLooper()) |
        asint(paint.getImageFilter())) {
        flatFlags |= kHasEffects_FlatFlag;
    }

    buffer.writeScalar(paint.getStrokeWidth());
    buffer.writeScalar(paint.getStrokeMiter());
    buffer.writeColor4f(paint.getColor4f());

    buffer.write32(pack_v68(paint, flatFlags));

    if (flatFlags & kHasEffects_FlatFlag) {
        buffer.writeFlattenable(paint.getPathEffect());
        buffer.writeFlattenable(paint.getShader());
        buffer.writeFlattenable(paint.getMaskFilter());
        buffer.writeFlattenable(paint.getColorFilter());
        buffer.writeFlattenable(paint.getLooper());
        buffer.writeFlattenable(paint.getImageFilter());
    }
}

SkReadPaintResult SkPaintPriv::Unflatten_PreV68(SkPaint* paint, SkReadBuffer& buffer, SkFont* font) {
    SkSafeRange safe;

    {
        SkScalar sz = buffer.readScalar();
        SkScalar sx = buffer.readScalar();
        SkScalar kx = buffer.readScalar();
#ifdef SK_SUPPORT_LEGACY_PAINT_FONT_FIELDS
        paint->setTextSize(sz);
        paint->setTextScaleX(sx);
        paint->setTextSkewX(kx);
#endif
        if (font) {
            font->setSize(sz);
            font->setScaleX(sx);
            font->setSkewX(kx);
        }
    }

    paint->setStrokeWidth(buffer.readScalar());
    paint->setStrokeMiter(buffer.readScalar());
    if (buffer.isVersionLT(SkReadBuffer::kFloat4PaintColor_Version)) {
        paint->setColor(buffer.readColor());
    } else {
        SkColor4f color;
        buffer.readColor4f(&color);
        paint->setColor4f(color, sk_srgb_singleton());
    }

    unsigned flatFlags = unpack_paint_flags(paint, buffer.readUInt(), font);

    uint32_t tmp = buffer.readUInt();
    paint->setStrokeCap(safe.checkLE((tmp >> 24) & 0xFF, SkPaint::kLast_Cap));
    paint->setStrokeJoin(safe.checkLE((tmp >> 16) & 0xFF, SkPaint::kLast_Join));
    paint->setStyle(safe.checkLE((tmp >> 12) & 0xF, SkPaint::kStrokeAndFill_Style));
    paint->private_internal_setTextEncoding(safe.checkLE((tmp >> 8) & 0xF, kGlyphID_SkTextEncoding));
    paint->setBlendMode(safe.checkLE(tmp & 0xFF, SkBlendMode::kLastMode));

    sk_sp<SkTypeface> tf;
    if (flatFlags & kHasTypeface_FlatFlag) {
        tf = buffer.readTypeface();
    }
#ifdef SK_SUPPORT_LEGACY_PAINT_FONT_FIELDS
    paint->setTypeface(tf);
#endif
    if (font) {
        font->setTypeface(tf);
    }

    if (flatFlags & kHasEffects_FlatFlag) {
        paint->setPathEffect(buffer.readPathEffect());
        paint->setShader(buffer.readShader());
        paint->setMaskFilter(buffer.readMaskFilter());
        paint->setColorFilter(buffer.readColorFilter());
        (void)buffer.read32();  // use to be SkRasterizer
        paint->setLooper(buffer.readDrawLooper());
        paint->setImageFilter(buffer.readImageFilter());
    } else {
        paint->setPathEffect(nullptr);
        paint->setShader(nullptr);
        paint->setMaskFilter(nullptr);
        paint->setColorFilter(nullptr);
        paint->setLooper(nullptr);
        paint->setImageFilter(nullptr);
    }

    if (!buffer.validate(safe)) {
        paint->reset();
        return kFailed_ReadPaint;
    }
    return kSuccess_PaintAndFont;
}

SkReadPaintResult SkPaintPriv::Unflatten(SkPaint* paint, SkReadBuffer& buffer, SkFont* font) {
    if (buffer.isVersionLT(SkReadBuffer::kPaintDoesntSerializeFonts_Version)) {
        return Unflatten_PreV68(paint, buffer, font);
    }

    SkSafeRange safe;

    paint->setStrokeWidth(buffer.readScalar());
    paint->setStrokeMiter(buffer.readScalar());
    {
        SkColor4f color;
        buffer.readColor4f(&color);
        paint->setColor4f(color, sk_srgb_singleton());
    }

    unsigned flatFlags = unpack_v68(paint, buffer.readUInt(), safe);

    if (flatFlags & kHasEffects_FlatFlag) {
        paint->setPathEffect(buffer.readPathEffect());
        paint->setShader(buffer.readShader());
        paint->setMaskFilter(buffer.readMaskFilter());
        paint->setColorFilter(buffer.readColorFilter());
        paint->setLooper(buffer.readDrawLooper());
        paint->setImageFilter(buffer.readImageFilter());
    } else {
        paint->setPathEffect(nullptr);
        paint->setShader(nullptr);
        paint->setMaskFilter(nullptr);
        paint->setColorFilter(nullptr);
        paint->setLooper(nullptr);
        paint->setImageFilter(nullptr);
    }

    if (!buffer.validate(safe)) {
        paint->reset();
        return kFailed_ReadPaint;
    }
    return kSuccess_JustPaint;
}

///////////////////////////////////////////////////////////////////////////////

bool SkPaint::getFillPath(const SkPath& src, SkPath* dst, const SkRect* cullRect,
                          SkScalar resScale) const {
    if (!src.isFinite()) {
        dst->reset();
        return false;
    }

    SkStrokeRec rec(*this, resScale);

    const SkPath* srcPtr = &src;
    SkPath tmpPath;

    if (fPathEffect && fPathEffect->filterPath(&tmpPath, src, &rec, cullRect)) {
        srcPtr = &tmpPath;
    }

    if (!rec.applyToPath(dst, *srcPtr)) {
        if (srcPtr == &tmpPath) {
            // If path's were copy-on-write, this trick would not be needed.
            // As it is, we want to save making a deep-copy from tmpPath -> dst
            // since we know we're just going to delete tmpPath when we return,
            // so the swap saves that copy.
            dst->swap(tmpPath);
        } else {
            *dst = *srcPtr;
        }
    }

    if (!dst->isFinite()) {
        dst->reset();
        return false;
    }
    return !rec.isHairlineStyle();
}

bool SkPaint::canComputeFastBounds() const {
    if (this->getLooper()) {
        return this->getLooper()->canComputeFastBounds(*this);
    }
    if (this->getImageFilter() && !this->getImageFilter()->canComputeFastBounds()) {
        return false;
    }
    return true;
}

const SkRect& SkPaint::doComputeFastBounds(const SkRect& origSrc,
                                           SkRect* storage,
                                           Style style) const {
    SkASSERT(storage);

    const SkRect* src = &origSrc;

    if (this->getLooper()) {
        SkASSERT(this->getLooper()->canComputeFastBounds(*this));
        this->getLooper()->computeFastBounds(*this, *src, storage);
        return *storage;
    }

    SkRect tmpSrc;
    if (this->getPathEffect()) {
        this->getPathEffect()->computeFastBounds(&tmpSrc, origSrc);
        src = &tmpSrc;
    }

    SkScalar radius = SkStrokeRec::GetInflationRadius(*this, style);
    *storage = src->makeOutset(radius, radius);

    if (this->getMaskFilter()) {
        as_MFB(this->getMaskFilter())->computeFastBounds(*storage, storage);
    }

    if (this->getImageFilter()) {
        *storage = this->getImageFilter()->computeFastBounds(*storage);
    }

    return *storage;
}

///////////////////////////////////////////////////////////////////////////////

// return true if the filter exists, and may affect alpha
static bool affects_alpha(const SkColorFilter* cf) {
    return cf && !(cf->getFlags() & SkColorFilter::kAlphaUnchanged_Flag);
}

// return true if the filter exists, and may affect alpha
static bool affects_alpha(const SkImageFilter* imf) {
    // TODO: check if we should allow imagefilters to broadcast that they don't affect alpha
    // ala colorfilters
    return imf != nullptr;
}

bool SkPaint::nothingToDraw() const {
    if (fDrawLooper) {
        return false;
    }
    switch ((SkBlendMode)fBlendMode) {
        case SkBlendMode::kSrcOver:
        case SkBlendMode::kSrcATop:
        case SkBlendMode::kDstOut:
        case SkBlendMode::kDstOver:
        case SkBlendMode::kPlus:
            if (0 == this->getAlpha()) {
                return !affects_alpha(fColorFilter.get()) && !affects_alpha(fImageFilter.get());
            }
            break;
        case SkBlendMode::kDst:
            return true;
        default:
            break;
    }
    return false;
}

uint32_t SkPaint::getHash() const {
    // We're going to hash 7 pointers and 11 32-bit values, finishing up with fBitfields,
    // so fBitfields should be 7 pointers and 10 32-bit values from the start.
    static_assert(offsetof(SkPaint, fBitfields) == 7 * sizeof(void*) + 10 * sizeof(uint32_t),
                  "SkPaint_notPackedTightly");
    return SkOpts::hash(reinterpret_cast<const uint32_t*>(this),
                        offsetof(SkPaint, fBitfields) + sizeof(fBitfields));
}
