// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/CachingWordShapeIterator.h"

#include "platform/fonts/shaping/HarfBuzzShaper.h"

namespace blink {

PassRefPtr<const ShapeResult> CachingWordShapeIterator::shapeWordWithoutSpacing(
    const TextRun& wordRun,
    const Font* font) {
  ShapeCacheEntry* cacheEntry = m_shapeCache->add(wordRun, ShapeCacheEntry());
  if (cacheEntry && cacheEntry->m_shapeResult)
    return cacheEntry->m_shapeResult;

  unsigned wordLength = 0;
  std::unique_ptr<UChar[]> wordText = wordRun.normalizedUTF16(&wordLength);

  HarfBuzzShaper shaper(wordText.get(), wordLength, wordRun.direction());
  RefPtr<const ShapeResult> shapeResult = shaper.shapeResult(font);
  if (!shapeResult)
    return nullptr;

  if (cacheEntry)
    cacheEntry->m_shapeResult = shapeResult;

  return shapeResult.release();
}

}  // namespace blink
