/*
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/layout/svg/line/SVGInlineTextBox.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/paint/SVGInlineTextBoxPainter.h"
#include "wtf/MathExtras.h"

namespace blink {

struct ExpectedSVGInlineTextBoxSize : public InlineTextBox {
  LayoutUnit float1;
  uint32_t bitfields : 1;
  Vector<SVGTextFragment> vector;
};

static_assert(sizeof(SVGInlineTextBox) == sizeof(ExpectedSVGInlineTextBoxSize),
              "SVGInlineTextBox has an unexpected size");

SVGInlineTextBox::SVGInlineTextBox(LineLayoutItem item,
                                   int start,
                                   unsigned short length)
    : InlineTextBox(item, start, length), m_startsNewTextChunk(false) {}

void SVGInlineTextBox::dirtyLineBoxes() {
  InlineTextBox::dirtyLineBoxes();

  // Clear the now stale text fragments.
  clearTextFragments();

  // And clear any following text fragments as the text on which they depend may
  // now no longer exist, or glyph positions may be wrong.
  InlineTextBox* nextBox = nextTextBox();
  if (nextBox)
    nextBox->dirtyLineBoxes();
}

int SVGInlineTextBox::offsetForPosition(LayoutUnit, bool) const {
  // SVG doesn't use the standard offset <-> position selection system, as it's
  // not suitable for SVGs complex needs. Vertical text selection, inline boxes
  // spanning multiple lines (contrary to HTML, etc.)
  ASSERT_NOT_REACHED();
  return 0;
}

int SVGInlineTextBox::offsetForPositionInFragment(
    const SVGTextFragment& fragment,
    LayoutUnit position,
    bool includePartialGlyphs) const {
  LineLayoutSVGInlineText lineLayoutItem =
      LineLayoutSVGInlineText(this->getLineLayoutItem());

  float scalingFactor = lineLayoutItem.scalingFactor();
  ASSERT(scalingFactor);

  const ComputedStyle& style = lineLayoutItem.styleRef();

  TextRun textRun = constructTextRun(style, fragment);

  // Eventually handle lengthAdjust="spacingAndGlyphs".
  // FIXME: Handle vertical text.
  if (fragment.isTransformed()) {
    AffineTransform fragmentTransform = fragment.buildFragmentTransform();
    textRun.setHorizontalGlyphStretch(
        clampTo<float>(fragmentTransform.xScale()));
  }

  return fragment.characterOffset - start() +
         lineLayoutItem.scaledFont().offsetForPosition(
             textRun, position * scalingFactor, includePartialGlyphs);
}

LayoutUnit SVGInlineTextBox::positionForOffset(int) const {
  // SVG doesn't use the offset <-> position selection system.
  ASSERT_NOT_REACHED();
  return LayoutUnit();
}

FloatRect SVGInlineTextBox::selectionRectForTextFragment(
    const SVGTextFragment& fragment,
    int startPosition,
    int endPosition,
    const ComputedStyle& style) const {
  ASSERT(startPosition < endPosition);

  LineLayoutSVGInlineText lineLayoutItem =
      LineLayoutSVGInlineText(this->getLineLayoutItem());

  float scalingFactor = lineLayoutItem.scalingFactor();
  ASSERT(scalingFactor);

  const Font& scaledFont = lineLayoutItem.scaledFont();
  const SimpleFontData* fontData = scaledFont.primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return FloatRect();

  const FontMetrics& scaledFontMetrics = fontData->getFontMetrics();
  FloatPoint textOrigin(fragment.x, fragment.y);
  if (scalingFactor != 1)
    textOrigin.scale(scalingFactor, scalingFactor);

  textOrigin.move(0, -scaledFontMetrics.floatAscent());

  FloatRect selectionRect = scaledFont.selectionRectForText(
      constructTextRun(style, fragment), textOrigin,
      fragment.height * scalingFactor, startPosition, endPosition);
  if (scalingFactor == 1)
    return selectionRect;

  selectionRect.scale(1 / scalingFactor);
  return selectionRect;
}

LayoutRect SVGInlineTextBox::localSelectionRect(int startPosition,
                                                int endPosition) const {
  int boxStart = start();
  startPosition = std::max(startPosition - boxStart, 0);
  endPosition = std::min(endPosition - boxStart, static_cast<int>(len()));
  if (startPosition >= endPosition)
    return LayoutRect();

  const ComputedStyle& style = getLineLayoutItem().styleRef();

  FloatRect selectionRect;
  int fragmentStartPosition = 0;
  int fragmentEndPosition = 0;

  unsigned textFragmentsSize = m_textFragments.size();
  for (unsigned i = 0; i < textFragmentsSize; ++i) {
    const SVGTextFragment& fragment = m_textFragments.at(i);

    fragmentStartPosition = startPosition;
    fragmentEndPosition = endPosition;
    if (!mapStartEndPositionsIntoFragmentCoordinates(
            fragment, fragmentStartPosition, fragmentEndPosition))
      continue;

    FloatRect fragmentRect = selectionRectForTextFragment(
        fragment, fragmentStartPosition, fragmentEndPosition, style);
    if (fragment.isTransformed())
      fragmentRect = fragment.buildFragmentTransform().mapRect(fragmentRect);

    selectionRect.unite(fragmentRect);
  }

  return LayoutRect(enclosingIntRect(selectionRect));
}

void SVGInlineTextBox::paint(const PaintInfo& paintInfo,
                             const LayoutPoint& paintOffset,
                             LayoutUnit,
                             LayoutUnit) const {
  SVGInlineTextBoxPainter(*this).paint(paintInfo, paintOffset);
}

TextRun SVGInlineTextBox::constructTextRun(
    const ComputedStyle& style,
    const SVGTextFragment& fragment) const {
  LineLayoutText text = getLineLayoutItem();
  CHECK(!text.needsLayout());

  TextRun run(
      // characters, will be set below if non-zero.
      static_cast<const LChar*>(nullptr),
      0,  // length, will be set below if non-zero.
      0,  // xPos, only relevant with allowTabs=true
      0,  // padding, only relevant for justified text, not relevant for SVG
      TextRun::AllowTrailingExpansion, direction(),
      dirOverride() ||
          style.rtlOrdering() == EOrder::kVisual /* directionalOverride */);

  if (fragment.length) {
    if (text.is8Bit())
      run.setText(text.characters8() + fragment.characterOffset,
                  fragment.length);
    else
      run.setText(text.characters16() + fragment.characterOffset,
                  fragment.length);
  }

  // We handle letter & word spacing ourselves.
  run.disableSpacing();

  // Propagate the maximum length of the characters buffer to the TextRun, even
  // when we're only processing a substring.
  run.setCharactersLength(text.textLength() - fragment.characterOffset);
  ASSERT(run.charactersLength() >= run.length());
  return run;
}

bool SVGInlineTextBox::mapStartEndPositionsIntoFragmentCoordinates(
    const SVGTextFragment& fragment,
    int& startPosition,
    int& endPosition) const {
  int fragmentOffsetInBox =
      static_cast<int>(fragment.characterOffset) - start();

  // Compute positions relative to the fragment.
  startPosition -= fragmentOffsetInBox;
  endPosition -= fragmentOffsetInBox;

  // Intersect with the fragment range.
  startPosition = std::max(startPosition, 0);
  endPosition = std::min(endPosition, static_cast<int>(fragment.length));

  return startPosition < endPosition;
}

void SVGInlineTextBox::paintDocumentMarker(GraphicsContext&,
                                           const LayoutPoint&,
                                           const DocumentMarker&,
                                           const ComputedStyle&,
                                           const Font&,
                                           bool) const {
  // SVG does not have support for generic document markers (e.g.,
  // spellchecking, etc).
}

void SVGInlineTextBox::paintTextMatchMarkerForeground(
    const PaintInfo& paintInfo,
    const LayoutPoint& point,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) const {
  SVGInlineTextBoxPainter(*this).paintTextMatchMarkerForeground(
      paintInfo, point, marker, style, font);
}

void SVGInlineTextBox::paintTextMatchMarkerBackground(
    const PaintInfo& paintInfo,
    const LayoutPoint& point,
    const DocumentMarker& marker,
    const ComputedStyle& style,
    const Font& font) const {
  SVGInlineTextBoxPainter(*this).paintTextMatchMarkerBackground(
      paintInfo, point, marker, style, font);
}

LayoutRect SVGInlineTextBox::calculateBoundaries() const {
  LineLayoutSVGInlineText lineLayoutItem =
      LineLayoutSVGInlineText(this->getLineLayoutItem());
  const SimpleFontData* fontData = lineLayoutItem.scaledFont().primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return LayoutRect();

  float scalingFactor = lineLayoutItem.scalingFactor();
  ASSERT(scalingFactor);
  LayoutUnit baseline(fontData->getFontMetrics().floatAscent() / scalingFactor);

  LayoutRect textBoundingRect;
  for (const SVGTextFragment& fragment : m_textFragments)
    textBoundingRect.unite(LayoutRect(fragment.overflowBoundingBox(baseline)));

  return textBoundingRect;
}

bool SVGInlineTextBox::nodeAtPoint(HitTestResult& result,
                                   const HitTestLocation& locationInContainer,
                                   const LayoutPoint& accumulatedOffset,
                                   LayoutUnit,
                                   LayoutUnit) {
  // FIXME: integrate with InlineTextBox::nodeAtPoint better.
  ASSERT(!isLineBreak());

  PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_TEXT_HITTESTING,
                                 result.hitTestRequest(),
                                 getLineLayoutItem().style()->pointerEvents());
  bool isVisible =
      getLineLayoutItem().style()->visibility() == EVisibility::kVisible;
  if (isVisible || !hitRules.requireVisible) {
    if (hitRules.canHitBoundingBox ||
        (hitRules.canHitStroke &&
         (getLineLayoutItem().style()->svgStyle().hasStroke() ||
          !hitRules.requireStroke)) ||
        (hitRules.canHitFill &&
         (getLineLayoutItem().style()->svgStyle().hasFill() ||
          !hitRules.requireFill))) {
      LayoutRect rect(location(), size());
      rect.moveBy(accumulatedOffset);
      if (locationInContainer.intersects(rect)) {
        LineLayoutSVGInlineText lineLayoutItem =
            LineLayoutSVGInlineText(this->getLineLayoutItem());
        const SimpleFontData* fontData =
            lineLayoutItem.scaledFont().primaryFont();
        DCHECK(fontData);
        if (!fontData)
          return false;

        DCHECK(lineLayoutItem.scalingFactor());
        float baseline = fontData->getFontMetrics().floatAscent() /
                         lineLayoutItem.scalingFactor();
        FloatPoint floatLocation = FloatPoint(locationInContainer.point());
        for (const SVGTextFragment& fragment : m_textFragments) {
          FloatQuad fragmentQuad = fragment.boundingQuad(baseline);
          if (fragmentQuad.containsPoint(floatLocation)) {
            lineLayoutItem.updateHitTestResult(
                result,
                locationInContainer.point() - toLayoutSize(accumulatedOffset));
            if (result.addNodeToListBasedTestResult(lineLayoutItem.node(),
                                                    locationInContainer,
                                                    rect) == StopHitTesting)
              return true;
          }
        }
      }
    }
  }
  return false;
}

}  // namespace blink
