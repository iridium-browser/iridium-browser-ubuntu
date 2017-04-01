// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontFallbackIterator.h"

#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/SegmentedFontData.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/text/ICUError.h"

namespace blink {

PassRefPtr<FontFallbackIterator> FontFallbackIterator::create(
    const FontDescription& description,
    PassRefPtr<FontFallbackList> fallbackList,
    FontFallbackPriority fontFallbackPriority) {
  return adoptRef(new FontFallbackIterator(description, std::move(fallbackList),
                                           fontFallbackPriority));
}

FontFallbackIterator::FontFallbackIterator(
    const FontDescription& description,
    PassRefPtr<FontFallbackList> fallbackList,
    FontFallbackPriority fontFallbackPriority)
    : m_fontDescription(description),
      m_fontFallbackList(fallbackList),
      m_currentFontDataIndex(0),
      m_segmentedFaceIndex(0),
      m_fallbackStage(FontGroupFonts),
      m_fontFallbackPriority(fontFallbackPriority) {}

bool FontFallbackIterator::alreadyLoadingRangeForHintChar(UChar32 hintChar) {
  for (auto it = m_trackedLoadingRangeSets.begin();
       it != m_trackedLoadingRangeSets.end(); ++it) {
    if ((*it)->contains(hintChar))
      return true;
  }
  return false;
}

bool FontFallbackIterator::rangeSetContributesForHint(
    const Vector<UChar32> hintList,
    const FontDataForRangeSet* segmentedFace) {
  for (auto it = hintList.begin(); it != hintList.end(); ++it) {
    if (segmentedFace->contains(*it)) {
      if (!alreadyLoadingRangeForHintChar(*it))
        return true;
    }
  }
  return false;
}

void FontFallbackIterator::willUseRange(const AtomicString& family,
                                        const FontDataForRangeSet& rangeSet) {
  FontSelector* selector = m_fontFallbackList->getFontSelector();
  if (!selector)
    return;

  selector->willUseRange(m_fontDescription, family, rangeSet);
}

PassRefPtr<FontDataForRangeSet> FontFallbackIterator::uniqueOrNext(
    PassRefPtr<FontDataForRangeSet> candidate,
    const Vector<UChar32>& hintList) {
  SkTypeface* candidateTypeface =
      candidate->fontData()->platformData().typeface();
  if (!candidateTypeface)
    return next(hintList);

  uint32_t candidateId = candidateTypeface->uniqueID();
  if (m_uniqueFontDataForRangeSetsReturned.contains(candidateId)) {
    return next(hintList);
  }

  // We don't want to skip subsetted ranges because HarfBuzzShaper's behavior
  // depends on the subsetting.
  if (candidate->isEntireRange())
    m_uniqueFontDataForRangeSetsReturned.add(candidateId);
  return candidate;
}

PassRefPtr<FontDataForRangeSet> FontFallbackIterator::next(
    const Vector<UChar32>& hintList) {
  if (m_fallbackStage == OutOfLuck)
    return adoptRef(new FontDataForRangeSet());

  if (m_fallbackStage == FallbackPriorityFonts) {
    // Only try one fallback priority font,
    // then proceed to regular system fallback.
    m_fallbackStage = SystemFonts;
    RefPtr<FontDataForRangeSet> fallbackPriorityFontRange =
        adoptRef(new FontDataForRangeSet(fallbackPriorityFont(hintList[0])));
    if (fallbackPriorityFontRange->hasFontData())
      return uniqueOrNext(fallbackPriorityFontRange.release(), hintList);
    return next(hintList);
  }

  if (m_fallbackStage == SystemFonts) {
    // We've reached pref + system fallback.
    RefPtr<SimpleFontData> systemFont = uniqueSystemFontForHintList(hintList);
    if (systemFont) {
      // Fallback fonts are not retained in the FontDataCache.
      return uniqueOrNext(adoptRef(new FontDataForRangeSet(systemFont)),
                          hintList);
    }

    // If we don't have options from the system fallback anymore or had
    // previously returned them, we only have the last resort font left.
    // TODO: crbug.com/42217 Improve this by doing the last run with a last
    // resort font that has glyphs for everything, for example the Unicode
    // LastResort font, not just Times or Arial.
    FontCache* fontCache = FontCache::fontCache();
    m_fallbackStage = OutOfLuck;
    RefPtr<SimpleFontData> lastResort =
        fontCache->getLastResortFallbackFont(m_fontDescription).get();
    RELEASE_ASSERT(lastResort);
    // Don't skip the LastResort font in uniqueOrNext() since HarfBuzzShaper
    // needs to use this one to place missing glyph boxes.
    return adoptRef(new FontDataForRangeSetFromCache(lastResort));
  }

  ASSERT(m_fallbackStage == FontGroupFonts || m_fallbackStage == SegmentedFace);
  const FontData* fontData =
      m_fontFallbackList->fontDataAt(m_fontDescription, m_currentFontDataIndex);

  if (!fontData) {
    // If there is no fontData coming from the fallback list, it means
    // we are now looking at system fonts, either for prioritized symbol
    // or emoji fonts or by calling system fallback API.
    m_fallbackStage = isNonTextFallbackPriority(m_fontFallbackPriority)
                          ? FallbackPriorityFonts
                          : SystemFonts;
    return next(hintList);
  }

  // Otherwise we've received a fontData from the font-family: set of fonts,
  // and a non-segmented one in this case.
  if (!fontData->isSegmented()) {
    // Skip forward to the next font family for the next call to next().
    m_currentFontDataIndex++;
    if (!fontData->isLoading()) {
      RefPtr<SimpleFontData> nonSegmented =
          const_cast<SimpleFontData*>(toSimpleFontData(fontData));
      // The fontData object that we have here is tracked in m_fontList of
      // FontFallbackList and gets released in the font cache when the
      // FontFallbackList is destroyed.
      return uniqueOrNext(adoptRef(new FontDataForRangeSet(nonSegmented)),
                          hintList);
    }
    return next(hintList);
  }

  // Iterate over ranges of a segmented font below.

  const SegmentedFontData* segmented = toSegmentedFontData(fontData);
  if (m_fallbackStage != SegmentedFace) {
    m_segmentedFaceIndex = 0;
    m_fallbackStage = SegmentedFace;
  }

  ASSERT(m_segmentedFaceIndex < segmented->numFaces());
  RefPtr<FontDataForRangeSet> currentSegmentedFace =
      segmented->faceAt(m_segmentedFaceIndex);
  m_segmentedFaceIndex++;

  if (m_segmentedFaceIndex == segmented->numFaces()) {
    // Switch from iterating over a segmented face to the next family from
    // the font-family: group of fonts.
    m_fallbackStage = FontGroupFonts;
    m_currentFontDataIndex++;
  }

  if (rangeSetContributesForHint(hintList, currentSegmentedFace.get())) {
    const SimpleFontData* fontData = currentSegmentedFace->fontData();
    if (const CustomFontData* customFontData = fontData->customFontData())
      customFontData->beginLoadIfNeeded();
    if (!fontData->isLoading())
      return uniqueOrNext(currentSegmentedFace, hintList);
    m_trackedLoadingRangeSets.push_back(currentSegmentedFace);
  }

  return next(hintList);
}

PassRefPtr<SimpleFontData> FontFallbackIterator::fallbackPriorityFont(
    UChar32 hint) {
  return FontCache::fontCache()->fallbackFontForCharacter(
      m_fontDescription, hint,
      m_fontFallbackList->primarySimpleFontData(m_fontDescription),
      m_fontFallbackPriority);
}

static inline unsigned chooseHintIndex(const Vector<UChar32>& hintList) {
  // crbug.com/618178 has a test case where no Myanmar font is ever found,
  // because the run starts with a punctuation character with a script value of
  // common. Our current font fallback code does not find a very meaningful
  // result for this.
  // TODO crbug.com/668706 - Improve this situation.
  // So if we have multiple hint characters (which indicates that a
  // multi-character grapheme or more failed to shape, then we can try to be
  // smarter and select the first character that has an actual script value.
  DCHECK(hintList.size());
  if (hintList.size() <= 1)
    return 0;

  ICUError err;
  UScriptCode hintCharScript = uscript_getScript(hintList[0], &err);
  if (!U_SUCCESS(err) || hintCharScript > USCRIPT_INHERITED)
    return 0;

  for (size_t i = 1; i < hintList.size(); ++i) {
    UScriptCode newHintScript = uscript_getScript(hintList[i], &err);
    if (!U_SUCCESS(err))
      return 0;
    if (newHintScript > USCRIPT_INHERITED)
      return i;
  }
  return 0;
}

PassRefPtr<SimpleFontData> FontFallbackIterator::uniqueSystemFontForHintList(
    const Vector<UChar32>& hintList) {
  // When we're asked for a fallback for the same characters again, we give up
  // because the shaper must have previously tried shaping with the font
  // already.
  if (!hintList.size())
    return nullptr;

  FontCache* fontCache = FontCache::fontCache();
  UChar32 hint = hintList[chooseHintIndex(hintList)];

  if (!hint || m_previouslyAskedForHint.contains(hint))
    return nullptr;
  m_previouslyAskedForHint.add(hint);
  return fontCache->fallbackFontForCharacter(
      m_fontDescription, hint,
      m_fontFallbackList->primarySimpleFontData(m_fontDescription));
}

}  // namespace blink
