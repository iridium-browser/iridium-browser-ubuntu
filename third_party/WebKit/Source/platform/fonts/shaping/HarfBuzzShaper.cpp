/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFallbackIterator.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/SmallCapsIterator.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/fonts/opentype/OpenTypeCapsSupport.h"
#include "platform/fonts/shaping/CaseMappingHarfBuzzBufferFiller.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"
#include "platform/fonts/shaping/RunSegmenter.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/text/TextBreakIterator.h"
#include "wtf/Compiler.h"
#include "wtf/MathExtras.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/Unicode.h"
#include <algorithm>
#include <hb.h>
#include <memory>
#include <unicode/uchar.h>
#include <unicode/uscript.h>

namespace blink {
using FeaturesVector = Vector<hb_feature_t, 6>;

template <typename T>
class HarfBuzzScopedPtr {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(HarfBuzzScopedPtr);

 public:
  typedef void (*DestroyFunction)(T*);

  HarfBuzzScopedPtr(T* ptr, DestroyFunction destroy)
      : m_ptr(ptr), m_destroy(destroy) {
    ASSERT(m_destroy);
  }
  ~HarfBuzzScopedPtr() {
    if (m_ptr)
      (*m_destroy)(m_ptr);
  }

  T* get() { return m_ptr; }
  void set(T* ptr) { m_ptr = ptr; }

 private:
  T* m_ptr;
  DestroyFunction m_destroy;
};

HarfBuzzShaper::HarfBuzzShaper(const UChar* text,
                               unsigned length,
                               TextDirection direction)
    : m_normalizedBuffer(text),
      m_normalizedBufferLength(length),
      m_textDirection(direction) {}

namespace {

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built
// without hb-icu. See http://crbug.com/356929
static inline hb_script_t ICUScriptToHBScript(UScriptCode script) {
  if (UNLIKELY(script == USCRIPT_INVALID_CODE))
    return HB_SCRIPT_INVALID;

  return hb_script_from_string(uscript_getShortName(script), -1);
}

static inline hb_direction_t TextDirectionToHBDirection(
    TextDirection dir,
    FontOrientation orientation,
    const SimpleFontData* fontData) {
  hb_direction_t harfBuzzDirection =
      isVerticalAnyUpright(orientation) &&
              !fontData->isTextOrientationFallback()
          ? HB_DIRECTION_TTB
          : HB_DIRECTION_LTR;
  return dir == TextDirection::kRtl ? HB_DIRECTION_REVERSE(harfBuzzDirection)
                                    : harfBuzzDirection;
}

inline bool shapeRange(hb_buffer_t* harfBuzzBuffer,
                       hb_feature_t* fontFeatures,
                       unsigned fontFeaturesSize,
                       const SimpleFontData* currentFont,
                       PassRefPtr<UnicodeRangeSet> currentFontRangeSet,
                       UScriptCode currentRunScript,
                       hb_direction_t direction,
                       hb_language_t language) {
  const FontPlatformData* platformData = &(currentFont->platformData());
  HarfBuzzFace* face = platformData->harfBuzzFace();
  if (!face) {
    DLOG(ERROR) << "Could not create HarfBuzzFace from FontPlatformData.";
    return false;
  }

  hb_buffer_set_language(harfBuzzBuffer, language);
  hb_buffer_set_script(harfBuzzBuffer, ICUScriptToHBScript(currentRunScript));
  hb_buffer_set_direction(harfBuzzBuffer, direction);

  hb_font_t* hbFont = face->getScaledFont(std::move(currentFontRangeSet));
  hb_shape(hbFont, harfBuzzBuffer, fontFeatures, fontFeaturesSize);

  return true;
}

}  // namespace

bool HarfBuzzShaper::extractShapeResults(hb_buffer_t* harfBuzzBuffer,
                                         ShapeResult* shapeResult,
                                         bool& fontCycleQueued,
                                         Deque<HolesQueueItem>* holesQueue,
                                         const HolesQueueItem& currentQueueItem,
                                         const Font* font,
                                         const SimpleFontData* currentFont,
                                         UScriptCode currentRunScript,
                                         bool isLastResort) const {
  enum ClusterResult { Shaped, NotDef, Unknown };
  ClusterResult currentClusterResult = Unknown;
  ClusterResult previousClusterResult = Unknown;
  unsigned previousCluster = 0;
  unsigned currentCluster = 0;

  // Find first notdef glyph in harfBuzzBuffer.
  unsigned numGlyphs = hb_buffer_get_length(harfBuzzBuffer);
  hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(harfBuzzBuffer, 0);

  unsigned lastChangePosition = 0;

  if (!numGlyphs) {
    DLOG(ERROR) << "HarfBuzz returned empty glyph buffer after shaping.";
    return false;
  }

  for (unsigned glyphIndex = 0; glyphIndex <= numGlyphs; ++glyphIndex) {
    // Iterating by clusters, check for when the state switches from shaped
    // to non-shaped and vice versa. Taking into account the edge cases of
    // beginning of the run and end of the run.
    previousCluster = currentCluster;
    currentCluster = glyphInfo[glyphIndex].cluster;

    if (glyphIndex < numGlyphs) {
      // Still the same cluster, merge shaping status.
      if (previousCluster == currentCluster && glyphIndex != 0) {
        if (glyphInfo[glyphIndex].codepoint == 0) {
          currentClusterResult = NotDef;
        } else {
          // We can only call the current cluster fully shapped, if
          // all characters that are part of it are shaped, so update
          // currentClusterResult to Shaped only if the previous
          // characters have been shaped, too.
          currentClusterResult =
              currentClusterResult == Shaped ? Shaped : NotDef;
        }
        continue;
      }
      // We've moved to a new cluster.
      previousClusterResult = currentClusterResult;
      currentClusterResult =
          glyphInfo[glyphIndex].codepoint == 0 ? NotDef : Shaped;
    } else {
      // The code below operates on the "flanks"/changes between NotDef
      // and Shaped. In order to keep the code below from explictly
      // dealing with character indices and run end, we explicitly
      // terminate the cluster/run here by setting the result value to the
      // opposite of what it was, leading to atChange turning true.
      previousClusterResult = currentClusterResult;
      currentClusterResult = currentClusterResult == NotDef ? Shaped : NotDef;
    }

    bool atChange = (previousClusterResult != currentClusterResult) &&
                    previousClusterResult != Unknown;
    if (!atChange)
      continue;

    // Compute the range indices of consecutive shaped or .notdef glyphs.
    // Cluster information for RTL runs becomes reversed, e.g. character 0
    // has cluster index 5 in a run of 6 characters.
    unsigned numCharacters = 0;
    unsigned numGlyphsToInsert = 0;
    unsigned startIndex = 0;
    if (HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(harfBuzzBuffer))) {
      startIndex = glyphInfo[lastChangePosition].cluster;
      if (glyphIndex == numGlyphs) {
        numCharacters = currentQueueItem.m_startIndex +
                        currentQueueItem.m_numCharacters -
                        glyphInfo[lastChangePosition].cluster;
        numGlyphsToInsert = numGlyphs - lastChangePosition;
      } else {
        numCharacters = glyphInfo[glyphIndex].cluster -
                        glyphInfo[lastChangePosition].cluster;
        numGlyphsToInsert = glyphIndex - lastChangePosition;
      }
    } else {
      // Direction Backwards
      startIndex = glyphInfo[glyphIndex - 1].cluster;
      if (lastChangePosition == 0) {
        numCharacters = currentQueueItem.m_startIndex +
                        currentQueueItem.m_numCharacters -
                        glyphInfo[glyphIndex - 1].cluster;
      } else {
        numCharacters = glyphInfo[lastChangePosition - 1].cluster -
                        glyphInfo[glyphIndex - 1].cluster;
      }
      numGlyphsToInsert = glyphIndex - lastChangePosition;
    }

    if (currentClusterResult == Shaped && !isLastResort) {
      // Now it's clear that we need to continue processing.
      if (!fontCycleQueued) {
        holesQueue->append(HolesQueueItem(HolesQueueNextFont, 0, 0));
        fontCycleQueued = true;
      }

      // Here we need to put character positions.
      ASSERT(numCharacters);
      holesQueue->append(
          HolesQueueItem(HolesQueueRange, startIndex, numCharacters));
    }

    // If numCharacters is 0, that means we hit a NotDef before shaping the
    // whole grapheme. We do not append it here. For the next glyph we
    // encounter, atChange will be true, and the characters corresponding to
    // the grapheme will be added to the TODO queue again, attempting to
    // shape the whole grapheme with the next font.
    // When we're getting here with the last resort font, we have no other
    // choice than adding boxes to the ShapeResult.
    if ((currentClusterResult == NotDef && numCharacters) || isLastResort) {
      hb_direction_t direction = TextDirectionToHBDirection(
          m_textDirection, font->getFontDescription().orientation(),
          currentFont);
      // Here we need to specify glyph positions.
      ShapeResult::RunInfo* run = new ShapeResult::RunInfo(
          currentFont, direction, ICUScriptToHBScript(currentRunScript),
          startIndex, numGlyphsToInsert, numCharacters);
      shapeResult->insertRun(WTF::wrapUnique(run), lastChangePosition,
                             numGlyphsToInsert, harfBuzzBuffer);
    }
    lastChangePosition = glyphIndex;
  }
  return true;
}

static inline const SimpleFontData* fontDataAdjustedForOrientation(
    const SimpleFontData* originalFont,
    FontOrientation runOrientation,
    OrientationIterator::RenderOrientation renderOrientation) {
  if (!isVerticalBaseline(runOrientation))
    return originalFont;

  if (runOrientation == FontOrientation::VerticalRotated ||
      (runOrientation == FontOrientation::VerticalMixed &&
       renderOrientation == OrientationIterator::OrientationRotateSideways))
    return originalFont->verticalRightOrientationFontData().get();

  return originalFont;
}

bool HarfBuzzShaper::collectFallbackHintChars(
    const Deque<HolesQueueItem>& holesQueue,
    Vector<UChar32>& hint) const {
  if (!holesQueue.size())
    return false;

  hint.clear();

  size_t numCharsAdded = 0;
  for (auto it = holesQueue.begin(); it != holesQueue.end(); ++it) {
    if (it->m_action == HolesQueueNextFont)
      break;

    UChar32 hintChar;
    RELEASE_ASSERT(it->m_startIndex + it->m_numCharacters <=
                   m_normalizedBufferLength);
    UTF16TextIterator iterator(m_normalizedBuffer + it->m_startIndex,
                               it->m_numCharacters);
    while (iterator.consume(hintChar)) {
      hint.push_back(hintChar);
      numCharsAdded++;
      iterator.advance();
    }
  }
  return numCharsAdded > 0;
}

namespace {
using HolesQueueItem = HarfBuzzShaper::HolesQueueItem;
using HolesQueueItemAction = HarfBuzzShaper::HolesQueueItemAction;

void splitUntilNextCaseChange(
    const UChar* normalizedBuffer,
    Deque<HolesQueueItem>* queue,
    HolesQueueItem& currentQueueItem,
    SmallCapsIterator::SmallCapsBehavior& smallCapsBehavior) {
  unsigned numCharactersUntilCaseChange = 0;
  SmallCapsIterator smallCapsIterator(
      normalizedBuffer + currentQueueItem.m_startIndex,
      currentQueueItem.m_numCharacters);
  smallCapsIterator.consume(&numCharactersUntilCaseChange, &smallCapsBehavior);
  if (numCharactersUntilCaseChange > 0 &&
      numCharactersUntilCaseChange < currentQueueItem.m_numCharacters) {
    queue->prepend(HolesQueueItem(
        HolesQueueItemAction::HolesQueueRange,
        currentQueueItem.m_startIndex + numCharactersUntilCaseChange,
        currentQueueItem.m_numCharacters - numCharactersUntilCaseChange));
    currentQueueItem.m_numCharacters = numCharactersUntilCaseChange;
  }
}

hb_feature_t createFeature(hb_tag_t tag, uint32_t value = 0) {
  return {tag, value, 0 /* start */, static_cast<unsigned>(-1) /* end */};
}

void setFontFeatures(const Font* font, FeaturesVector* features) {
  const FontDescription& description = font->getFontDescription();

  static hb_feature_t noKern = createFeature(HB_TAG('k', 'e', 'r', 'n'));
  static hb_feature_t noVkrn = createFeature(HB_TAG('v', 'k', 'r', 'n'));
  switch (description.getKerning()) {
    case FontDescription::NormalKerning:
      // kern/vkrn are enabled by default
      break;
    case FontDescription::NoneKerning:
      features->push_back(description.isVerticalAnyUpright() ? noVkrn : noKern);
      break;
    case FontDescription::AutoKerning:
      break;
  }

  static hb_feature_t noClig = createFeature(HB_TAG('c', 'l', 'i', 'g'));
  static hb_feature_t noLiga = createFeature(HB_TAG('l', 'i', 'g', 'a'));
  switch (description.commonLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
      features->push_back(noLiga);
      features->push_back(noClig);
      break;
    case FontDescription::EnabledLigaturesState:
      // liga and clig are on by default
      break;
    case FontDescription::NormalLigaturesState:
      break;
  }
  static hb_feature_t dlig = createFeature(HB_TAG('d', 'l', 'i', 'g'), 1);
  switch (description.discretionaryLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
      // dlig is off by default
      break;
    case FontDescription::EnabledLigaturesState:
      features->push_back(dlig);
      break;
    case FontDescription::NormalLigaturesState:
      break;
  }
  static hb_feature_t hlig = createFeature(HB_TAG('h', 'l', 'i', 'g'), 1);
  switch (description.historicalLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
      // hlig is off by default
      break;
    case FontDescription::EnabledLigaturesState:
      features->push_back(hlig);
      break;
    case FontDescription::NormalLigaturesState:
      break;
  }
  static hb_feature_t noCalt = createFeature(HB_TAG('c', 'a', 'l', 't'));
  switch (description.contextualLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
      features->push_back(noCalt);
      break;
    case FontDescription::EnabledLigaturesState:
      // calt is on by default
      break;
    case FontDescription::NormalLigaturesState:
      break;
  }

  static hb_feature_t hwid = createFeature(HB_TAG('h', 'w', 'i', 'd'), 1);
  static hb_feature_t twid = createFeature(HB_TAG('t', 'w', 'i', 'd'), 1);
  static hb_feature_t qwid = createFeature(HB_TAG('q', 'w', 'i', 'd'), 1);
  switch (description.widthVariant()) {
    case HalfWidth:
      features->push_back(hwid);
      break;
    case ThirdWidth:
      features->push_back(twid);
      break;
    case QuarterWidth:
      features->push_back(qwid);
      break;
    case RegularWidth:
      break;
  }

  // font-variant-numeric:
  static hb_feature_t lnum = createFeature(HB_TAG('l', 'n', 'u', 'm'), 1);
  if (description.variantNumeric().numericFigureValue() ==
      FontVariantNumeric::LiningNums)
    features->push_back(lnum);

  static hb_feature_t onum = createFeature(HB_TAG('o', 'n', 'u', 'm'), 1);
  if (description.variantNumeric().numericFigureValue() ==
      FontVariantNumeric::OldstyleNums)
    features->push_back(onum);

  static hb_feature_t pnum = createFeature(HB_TAG('p', 'n', 'u', 'm'), 1);
  if (description.variantNumeric().numericSpacingValue() ==
      FontVariantNumeric::ProportionalNums)
    features->push_back(pnum);
  static hb_feature_t tnum = createFeature(HB_TAG('t', 'n', 'u', 'm'), 1);
  if (description.variantNumeric().numericSpacingValue() ==
      FontVariantNumeric::TabularNums)
    features->push_back(tnum);

  static hb_feature_t afrc = createFeature(HB_TAG('a', 'f', 'r', 'c'), 1);
  if (description.variantNumeric().numericFractionValue() ==
      FontVariantNumeric::StackedFractions)
    features->push_back(afrc);
  static hb_feature_t frac = createFeature(HB_TAG('f', 'r', 'a', 'c'), 1);
  if (description.variantNumeric().numericFractionValue() ==
      FontVariantNumeric::DiagonalFractions)
    features->push_back(frac);

  static hb_feature_t ordn = createFeature(HB_TAG('o', 'r', 'd', 'n'), 1);
  if (description.variantNumeric().ordinalValue() ==
      FontVariantNumeric::OrdinalOn)
    features->push_back(ordn);

  static hb_feature_t zero = createFeature(HB_TAG('z', 'e', 'r', 'o'), 1);
  if (description.variantNumeric().slashedZeroValue() ==
      FontVariantNumeric::SlashedZeroOn)
    features->push_back(zero);

  FontFeatureSettings* settings = description.featureSettings();
  if (!settings)
    return;

  // TODO(drott): crbug.com/450619 Implement feature resolution instead of
  // just appending the font-feature-settings.
  unsigned numFeatures = settings->size();
  for (unsigned i = 0; i < numFeatures; ++i) {
    hb_feature_t feature;
    const AtomicString& tag = settings->at(i).tag();
    feature.tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
    feature.value = settings->at(i).value();
    feature.start = 0;
    feature.end = static_cast<unsigned>(-1);
    features->push_back(feature);
  }
}

class CapsFeatureSettingsScopedOverlay final {
  STACK_ALLOCATED()

 public:
  CapsFeatureSettingsScopedOverlay(FeaturesVector*,
                                   FontDescription::FontVariantCaps);
  CapsFeatureSettingsScopedOverlay() = delete;
  ~CapsFeatureSettingsScopedOverlay();

 private:
  void overlayCapsFeatures(FontDescription::FontVariantCaps);
  void prependCounting(const hb_feature_t&);
  FeaturesVector* m_features;
  size_t m_countFeatures;
};

CapsFeatureSettingsScopedOverlay::CapsFeatureSettingsScopedOverlay(
    FeaturesVector* features,
    FontDescription::FontVariantCaps variantCaps)
    : m_features(features), m_countFeatures(0) {
  overlayCapsFeatures(variantCaps);
}

void CapsFeatureSettingsScopedOverlay::overlayCapsFeatures(
    FontDescription::FontVariantCaps variantCaps) {
  static hb_feature_t smcp = createFeature(HB_TAG('s', 'm', 'c', 'p'), 1);
  static hb_feature_t pcap = createFeature(HB_TAG('p', 'c', 'a', 'p'), 1);
  static hb_feature_t c2sc = createFeature(HB_TAG('c', '2', 's', 'c'), 1);
  static hb_feature_t c2pc = createFeature(HB_TAG('c', '2', 'p', 'c'), 1);
  static hb_feature_t unic = createFeature(HB_TAG('u', 'n', 'i', 'c'), 1);
  static hb_feature_t titl = createFeature(HB_TAG('t', 'i', 't', 'l'), 1);
  if (variantCaps == FontDescription::SmallCaps ||
      variantCaps == FontDescription::AllSmallCaps) {
    prependCounting(smcp);
    if (variantCaps == FontDescription::AllSmallCaps) {
      prependCounting(c2sc);
    }
  }
  if (variantCaps == FontDescription::PetiteCaps ||
      variantCaps == FontDescription::AllPetiteCaps) {
    prependCounting(pcap);
    if (variantCaps == FontDescription::AllPetiteCaps) {
      prependCounting(c2pc);
    }
  }
  if (variantCaps == FontDescription::Unicase) {
    prependCounting(unic);
  }
  if (variantCaps == FontDescription::TitlingCaps) {
    prependCounting(titl);
  }
}

void CapsFeatureSettingsScopedOverlay::prependCounting(
    const hb_feature_t& feature) {
  m_features->prepend(feature);
  m_countFeatures++;
}

CapsFeatureSettingsScopedOverlay::~CapsFeatureSettingsScopedOverlay() {
  m_features->remove(0, m_countFeatures);
}

}  // namespace

PassRefPtr<ShapeResult> HarfBuzzShaper::shapeResult(const Font* font) const {
  RefPtr<ShapeResult> result =
      ShapeResult::create(font, m_normalizedBufferLength, m_textDirection);
  HarfBuzzScopedPtr<hb_buffer_t> harfBuzzBuffer(hb_buffer_create(),
                                                hb_buffer_destroy);
  FeaturesVector fontFeatures;
  setFontFeatures(font, &fontFeatures);
  const FontDescription& fontDescription = font->getFontDescription();
  const hb_language_t language =
      fontDescription.localeOrDefault().harfbuzzLanguage();

  bool needsCapsHandling =
      fontDescription.variantCaps() != FontDescription::CapsNormal;
  OpenTypeCapsSupport capsSupport;
  FontOrientation orientation = font->getFontDescription().orientation();

  RunSegmenter::RunSegmenterRange segmentRange = {
      0, 0, USCRIPT_INVALID_CODE, OrientationIterator::OrientationInvalid,
      FontFallbackPriority::Invalid};
  RunSegmenter runSegmenter(m_normalizedBuffer, m_normalizedBufferLength,
                            orientation);

  Vector<UChar32> fallbackCharsHint;

  // TODO: Check whether this treatAsZerowidthspace from the previous script
  // segmentation plays a role here, does the new scriptRuniterator handle that
  // correctly?
  Deque<HolesQueueItem> holesQueue;
  while (runSegmenter.consume(&segmentRange)) {
    RefPtr<FontFallbackIterator> fallbackIterator =
        font->createFontFallbackIterator(segmentRange.fontFallbackPriority);

    holesQueue.append(HolesQueueItem(HolesQueueNextFont, 0, 0));
    holesQueue.append(HolesQueueItem(HolesQueueRange, segmentRange.start,
                                     segmentRange.end - segmentRange.start));

    RefPtr<FontDataForRangeSet> currentFontDataForRangeSet;

    bool fontCycleQueued = false;
    while (holesQueue.size()) {
      HolesQueueItem currentQueueItem = holesQueue.takeFirst();

      if (currentQueueItem.m_action == HolesQueueNextFont) {
        // For now, we're building a character list with which we probe
        // for needed fonts depending on the declared unicode-range of a
        // segmented CSS font. Alternatively, we can build a fake font
        // for the shaper and check whether any glyphs were found, or
        // define a new API on the shaper which will give us coverage
        // information?
        if (!collectFallbackHintChars(holesQueue, fallbackCharsHint)) {
          // Give up shaping since we cannot retrieve a font fallback
          // font without a hintlist.
          holesQueue.clear();
          break;
        }

        currentFontDataForRangeSet = fallbackIterator->next(fallbackCharsHint);
        if (!currentFontDataForRangeSet->fontData()) {
          DCHECK(!holesQueue.size());
          break;
        }
        fontCycleQueued = false;
        continue;
      }

      const SimpleFontData* fontData = currentFontDataForRangeSet->fontData();
      SmallCapsIterator::SmallCapsBehavior smallCapsBehavior =
          SmallCapsIterator::SmallCapsSameCase;
      if (needsCapsHandling) {
        capsSupport =
            OpenTypeCapsSupport(fontData->platformData().harfBuzzFace(),
                                fontDescription.variantCaps(),
                                ICUScriptToHBScript(segmentRange.script));
        if (capsSupport.needsRunCaseSplitting()) {
          splitUntilNextCaseChange(m_normalizedBuffer, &holesQueue,
                                   currentQueueItem, smallCapsBehavior);
        }
      }

      ASSERT(currentQueueItem.m_numCharacters);

      const SimpleFontData* smallcapsAdjustedFont =
          needsCapsHandling && capsSupport.needsSyntheticFont(smallCapsBehavior)
              ? fontData->smallCapsFontData(fontDescription).get()
              : fontData;

      // Compatibility with SimpleFontData approach of keeping a flag for
      // overriding drawing direction.
      // TODO: crbug.com/506224 This should go away in favor of storing that
      // information elsewhere, for example in ShapeResult.
      const SimpleFontData* directionAndSmallCapsAdjustedFont =
          fontDataAdjustedForOrientation(smallcapsAdjustedFont, orientation,
                                         segmentRange.renderOrientation);

      CaseMapIntend caseMapIntend = CaseMapIntend::KeepSameCase;
      if (needsCapsHandling)
        caseMapIntend = capsSupport.needsCaseChange(smallCapsBehavior);

      CaseMappingHarfBuzzBufferFiller(
          caseMapIntend, fontDescription.localeOrDefault(),
          harfBuzzBuffer.get(), m_normalizedBuffer, m_normalizedBufferLength,
          currentQueueItem.m_startIndex, currentQueueItem.m_numCharacters);

      CapsFeatureSettingsScopedOverlay capsOverlay(
          &fontFeatures, capsSupport.fontFeatureToUse(smallCapsBehavior));

      hb_direction_t direction = TextDirectionToHBDirection(
          m_textDirection, orientation, directionAndSmallCapsAdjustedFont);

      if (!shapeRange(harfBuzzBuffer.get(),
                      fontFeatures.isEmpty() ? 0 : fontFeatures.data(),
                      fontFeatures.size(), directionAndSmallCapsAdjustedFont,
                      currentFontDataForRangeSet->ranges(), segmentRange.script,
                      direction, language))
        DLOG(ERROR) << "Shaping range failed.";

      if (!extractShapeResults(
              harfBuzzBuffer.get(), result.get(), fontCycleQueued, &holesQueue,
              currentQueueItem, font, directionAndSmallCapsAdjustedFont,
              segmentRange.script, !fallbackIterator->hasNext()))
        DLOG(ERROR) << "Shape result extraction failed.";

      hb_buffer_reset(harfBuzzBuffer.get());
    }
  }
  return result.release();
}

}  // namespace blink
