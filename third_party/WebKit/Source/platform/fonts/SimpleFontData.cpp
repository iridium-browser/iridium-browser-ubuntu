/*
 * Copyright (C) 2005, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/SimpleFontData.h"

#include "SkPath.h"
#include "SkTypeface.h"
#include "SkTypes.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/VDMXParser.h"
#include "platform/fonts/skia/SkiaTextMetrics.h"
#include "platform/geometry/FloatRect.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include "wtf/allocator/Partitions.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"
#include <memory>
#include <unicode/unorm.h>
#include <unicode/utf16.h>

namespace blink {

const float smallCapsFontSizeMultiplier = 0.7f;
const float emphasisMarkFontSizeMultiplier = 0.5f;

#if OS(LINUX) || OS(ANDROID)
// This is the largest VDMX table which we'll try to load and parse.
static const size_t maxVDMXTableSize = 1024 * 1024;  // 1 MB
#endif

SimpleFontData::SimpleFontData(const FontPlatformData& platformData,
                               PassRefPtr<CustomFontData> customData,
                               bool isTextOrientationFallback,
                               bool subpixelAscentDescent)
    : m_maxCharWidth(-1),
      m_avgCharWidth(-1),
      m_platformData(platformData),
      m_verticalData(nullptr),
      m_customFontData(customData),
      m_isTextOrientationFallback(isTextOrientationFallback),
      m_hasVerticalGlyphs(false),
      visual_overflow_inflation_for_ascent_(0),
      visual_overflow_inflation_for_descent_(0) {
  platformInit(subpixelAscentDescent);
  platformGlyphInit();
  if (platformData.isVerticalAnyUpright() && !isTextOrientationFallback) {
    m_verticalData = platformData.verticalData();
    m_hasVerticalGlyphs =
        m_verticalData.get() && m_verticalData->hasVerticalMetrics();
  }
}

SimpleFontData::SimpleFontData(PassRefPtr<CustomFontData> customData,
                               float fontSize,
                               bool syntheticBold,
                               bool syntheticItalic)
    : m_platformData(
          FontPlatformData(fontSize, syntheticBold, syntheticItalic)),
      m_verticalData(nullptr),
      m_customFontData(customData),
      m_isTextOrientationFallback(false),
      m_hasVerticalGlyphs(false),
      visual_overflow_inflation_for_ascent_(0),
      visual_overflow_inflation_for_descent_(0) {}

void SimpleFontData::platformInit(bool subpixelAscentDescent) {
  if (!m_platformData.size()) {
    m_fontMetrics.reset();
    m_avgCharWidth = 0;
    m_maxCharWidth = 0;
    return;
  }

  SkPaint::FontMetrics metrics;

  m_platformData.setupPaint(&m_paint);
  m_paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  m_paint.getFontMetrics(&metrics);
  SkTypeface* face = m_paint.getTypeface();
  ASSERT(face);

  int vdmxAscent = 0, vdmxDescent = 0;
  bool isVDMXValid = false;

#if OS(LINUX) || OS(ANDROID)
  // Manually digging up VDMX metrics is only applicable when bytecode hinting
  // using FreeType.  With DirectWrite or CoreText, no bytecode hinting is ever
  // done.  This code should be pushed into FreeType (hinted font metrics).
  static const uint32_t vdmxTag = SkSetFourByteTag('V', 'D', 'M', 'X');
  int pixelSize = m_platformData.size() + 0.5;
  if (!m_paint.isAutohinted() &&
      (m_paint.getHinting() == SkPaint::kFull_Hinting ||
       m_paint.getHinting() == SkPaint::kNormal_Hinting)) {
    size_t vdmxSize = face->getTableSize(vdmxTag);
    if (vdmxSize && vdmxSize < maxVDMXTableSize) {
      uint8_t* vdmxTable = (uint8_t*)WTF::Partitions::fastMalloc(
          vdmxSize, WTF_HEAP_PROFILER_TYPE_NAME(SimpleFontData));
      if (vdmxTable &&
          face->getTableData(vdmxTag, 0, vdmxSize, vdmxTable) == vdmxSize &&
          parseVDMX(&vdmxAscent, &vdmxDescent, vdmxTable, vdmxSize, pixelSize))
        isVDMXValid = true;
      WTF::Partitions::fastFree(vdmxTable);
    }
  }
#endif

  float ascent;
  float descent;

  // Beware those who step here: This code is designed to match Win32 font
  // metrics *exactly* except:
  // - the adjustment of ascent/descent on Linux/Android
  // - metrics.fAscent and .fDesscent are not rounded to int for tiny fonts
  if (isVDMXValid) {
    ascent = vdmxAscent;
    descent = -vdmxDescent;
  } else if (subpixelAscentDescent &&
             (-metrics.fAscent < 3 ||
              -metrics.fAscent + metrics.fDescent < 2)) {
    // For tiny fonts, the rounding of fAscent and fDescent results in equal
    // baseline for different types of text baselines (crbug.com/338908).
    // Please see CanvasRenderingContext2D::getFontBaseline for the heuristic.
    ascent = -metrics.fAscent;
    descent = metrics.fDescent;
  } else {
    ascent = SkScalarRoundToScalar(-metrics.fAscent);
    descent = SkScalarRoundToScalar(metrics.fDescent);

    if (ascent < -metrics.fAscent)
      visual_overflow_inflation_for_ascent_ = 1;
    if (descent < metrics.fDescent) {
      visual_overflow_inflation_for_descent_ = 1;
#if OS(LINUX) || OS(ANDROID)
      // When subpixel positioning is enabled, if the descent is rounded down,
      // the descent part of the glyph may be truncated when displayed in a
      // 'overflow: hidden' container.  To avoid that, borrow 1 unit from the
      // ascent when possible.
      if (platformData().getFontRenderStyle().useSubpixelPositioning &&
          ascent >= 1) {
        ++descent;
        --ascent;
        // We should inflate overflow 1 more pixel for ascent instead.
        visual_overflow_inflation_for_descent_ = 0;
        ++visual_overflow_inflation_for_ascent_;
      }
#endif
    }
  }

#if OS(MACOSX)
  // We are preserving this ascent hack to match Safari's ascent adjustment
  // in their SimpleFontDataMac.mm, for details see crbug.com/445830.
  // We need to adjust Times, Helvetica, and Courier to closely match the
  // vertical metrics of their Microsoft counterparts that are the de facto
  // web standard. The AppKit adjustment of 20% is too big and is
  // incorrectly added to line spacing, so we use a 15% adjustment instead
  // and add it to the ascent.
  DEFINE_STATIC_LOCAL(AtomicString, timesName, ("Times"));
  DEFINE_STATIC_LOCAL(AtomicString, helveticaName, ("Helvetica"));
  DEFINE_STATIC_LOCAL(AtomicString, courierName, ("Courier"));
  String familyName = m_platformData.fontFamilyName();
  if (familyName == timesName || familyName == helveticaName ||
      familyName == courierName)
    ascent += floorf(((ascent + descent) * 0.15f) + 0.5f);
#endif

  m_fontMetrics.setAscent(ascent);
  m_fontMetrics.setDescent(descent);

  float xHeight;
  if (metrics.fXHeight) {
    xHeight = metrics.fXHeight;
#if OS(MACOSX)
    // Mac OS CTFontGetXHeight reports the bounding box height of x,
    // including parts extending below the baseline and apparently no x-height
    // value from the OS/2 table. However, the CSS ex unit
    // expects only parts above the baseline, hence measuring the glyph:
    // http://www.w3.org/TR/css3-values/#ex-unit
    const Glyph xGlyph = glyphForCharacter('x');
    if (xGlyph) {
      FloatRect glyphBounds(boundsForGlyph(xGlyph));
      // SkGlyph bounds, y down, based on rendering at (0,0).
      xHeight = -glyphBounds.y();
    }
#endif
    m_fontMetrics.setXHeight(xHeight);
  } else {
    xHeight = ascent * 0.56;  // Best guess from Windows font metrics.
    m_fontMetrics.setXHeight(xHeight);
    m_fontMetrics.setHasXHeight(false);
  }

  float lineGap = SkScalarToFloat(metrics.fLeading);
  m_fontMetrics.setLineGap(lineGap);
  m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) +
                               lroundf(lineGap));

  if (platformData().isVerticalAnyUpright() && !isTextOrientationFallback()) {
    static const uint32_t vheaTag = SkSetFourByteTag('v', 'h', 'e', 'a');
    static const uint32_t vorgTag = SkSetFourByteTag('V', 'O', 'R', 'G');
    size_t vheaSize = face->getTableSize(vheaTag);
    size_t vorgSize = face->getTableSize(vorgTag);
    if ((vheaSize > 0) || (vorgSize > 0))
      m_hasVerticalGlyphs = true;
  }

// In WebKit/WebCore/platform/graphics/SimpleFontData.cpp, m_spaceWidth is
// calculated for us, but we need to calculate m_maxCharWidth and
// m_avgCharWidth in order for text entry widgets to be sized correctly.
#if OS(WIN)
  m_maxCharWidth = SkScalarRoundToInt(metrics.fMaxCharWidth);

  // Older version of the DirectWrite API doesn't implement support for max
  // char width. Fall back on a multiple of the ascent. This is entirely
  // arbitrary but comes pretty close to the expected value in most cases.
  if (m_maxCharWidth < 1)
    m_maxCharWidth = ascent * 2;
#elif OS(MACOSX)
  // FIXME: The current avg/max character width calculation is not ideal,
  // it should check either the OS2 table or, better yet, query FontMetrics.
  // Sadly FontMetrics provides incorrect data on Mac at the moment.
  // https://crbug.com/420901
  m_maxCharWidth = std::max(m_avgCharWidth, m_fontMetrics.floatAscent());
#else
  // Better would be to rely on either fMaxCharWidth or fAveCharWidth.
  // skbug.com/3087
  m_maxCharWidth = SkScalarRoundToInt(metrics.fXMax - metrics.fXMin);

#endif

#if !OS(MACOSX)
  if (metrics.fAvgCharWidth) {
    m_avgCharWidth = SkScalarRoundToInt(metrics.fAvgCharWidth);
  } else {
#endif
    m_avgCharWidth = xHeight;
    const Glyph xGlyph = glyphForCharacter('x');
    if (xGlyph) {
      m_avgCharWidth = widthForGlyph(xGlyph);
    }
#if !OS(MACOSX)
  }
#endif

  if (int unitsPerEm = face->getUnitsPerEm())
    m_fontMetrics.setUnitsPerEm(unitsPerEm);
}

void SimpleFontData::platformGlyphInit() {
  SkTypeface* typeface = platformData().typeface();
  if (!typeface->countGlyphs()) {
    m_spaceGlyph = 0;
    m_spaceWidth = 0;
    m_zeroGlyph = 0;
    m_missingGlyphData.fontData = this;
    m_missingGlyphData.glyph = 0;
    return;
  }

  // Nasty hack to determine if we should round or ceil space widths.
  // If the font is monospace or fake monospace we ceil to ensure that
  // every character and the space are the same width.  Otherwise we round.
  m_spaceGlyph = glyphForCharacter(' ');
  float width = widthForGlyph(m_spaceGlyph);
  m_spaceWidth = width;
  m_zeroGlyph = glyphForCharacter('0');
  m_fontMetrics.setZeroWidth(widthForGlyph(m_zeroGlyph));

  m_missingGlyphData.fontData = this;
  m_missingGlyphData.glyph = 0;
}

const SimpleFontData* SimpleFontData::fontDataForCharacter(UChar32) const {
  return this;
}

Glyph SimpleFontData::glyphForCharacter(UChar32 codepoint) const {
  uint16_t glyph;
  SkTypeface* typeface = platformData().typeface();
  RELEASE_ASSERT(typeface);
  typeface->charsToGlyphs(&codepoint, SkTypeface::kUTF32_Encoding, &glyph, 1);
  return glyph;
}

bool SimpleFontData::isSegmented() const {
  return false;
}

PassRefPtr<SimpleFontData> SimpleFontData::verticalRightOrientationFontData()
    const {
  if (!m_derivedFontData)
    m_derivedFontData = DerivedFontData::create();
  if (!m_derivedFontData->verticalRightOrientation) {
    FontPlatformData verticalRightPlatformData(m_platformData);
    verticalRightPlatformData.setOrientation(FontOrientation::Horizontal);
    m_derivedFontData->verticalRightOrientation =
        create(verticalRightPlatformData,
               isCustomFont() ? CustomFontData::create() : nullptr, true);
  }
  return m_derivedFontData->verticalRightOrientation;
}

PassRefPtr<SimpleFontData> SimpleFontData::uprightOrientationFontData() const {
  if (!m_derivedFontData)
    m_derivedFontData = DerivedFontData::create();
  if (!m_derivedFontData->uprightOrientation)
    m_derivedFontData->uprightOrientation =
        create(m_platformData,
               isCustomFont() ? CustomFontData::create() : nullptr, true);
  return m_derivedFontData->uprightOrientation;
}

PassRefPtr<SimpleFontData> SimpleFontData::smallCapsFontData(
    const FontDescription& fontDescription) const {
  if (!m_derivedFontData)
    m_derivedFontData = DerivedFontData::create();
  if (!m_derivedFontData->smallCaps)
    m_derivedFontData->smallCaps =
        createScaledFontData(fontDescription, smallCapsFontSizeMultiplier);

  return m_derivedFontData->smallCaps;
}

PassRefPtr<SimpleFontData> SimpleFontData::emphasisMarkFontData(
    const FontDescription& fontDescription) const {
  if (!m_derivedFontData)
    m_derivedFontData = DerivedFontData::create();
  if (!m_derivedFontData->emphasisMark)
    m_derivedFontData->emphasisMark =
        createScaledFontData(fontDescription, emphasisMarkFontSizeMultiplier);

  return m_derivedFontData->emphasisMark;
}

bool SimpleFontData::isTextOrientationFallbackOf(
    const SimpleFontData* fontData) const {
  if (!isTextOrientationFallback() || !fontData->m_derivedFontData)
    return false;
  return fontData->m_derivedFontData->uprightOrientation == this ||
         fontData->m_derivedFontData->verticalRightOrientation == this;
}

std::unique_ptr<SimpleFontData::DerivedFontData>
SimpleFontData::DerivedFontData::create() {
  return WTF::wrapUnique(new DerivedFontData());
}

PassRefPtr<SimpleFontData> SimpleFontData::createScaledFontData(
    const FontDescription& fontDescription,
    float scaleFactor) const {
  const float scaledSize =
      lroundf(fontDescription.computedSize() * scaleFactor);
  return SimpleFontData::create(
      FontPlatformData(m_platformData, scaledSize),
      isCustomFont() ? CustomFontData::create() : nullptr);
}

FloatRect SimpleFontData::platformBoundsForGlyph(Glyph glyph) const {
  if (!m_platformData.size())
    return FloatRect();

  static_assert(sizeof(glyph) == 2, "Glyph id should not be truncated.");

  SkRect bounds;
  SkiaTextMetrics(&m_paint).getSkiaBoundsForGlyph(glyph, &bounds);
  return FloatRect(bounds);
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const {
  if (!m_platformData.size())
    return 0;

  static_assert(sizeof(glyph) == 2, "Glyph id should not be truncated.");

  return SkiaTextMetrics(&m_paint).getSkiaWidthForGlyph(glyph);
}

}  // namespace blink
