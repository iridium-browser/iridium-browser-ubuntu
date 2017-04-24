/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
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
 *
 */

#include "core/layout/LayoutTextControl.h"

#include "core/dom/StyleChangeReason.h"
#include "core/html/TextControlElement.h"
#include "core/layout/HitTestResult.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

LayoutTextControl::LayoutTextControl(TextControlElement* element)
    : LayoutBlockFlow(element) {
  ASSERT(element);
}

LayoutTextControl::~LayoutTextControl() {}

TextControlElement* LayoutTextControl::textControlElement() const {
  return toTextControlElement(node());
}

HTMLElement* LayoutTextControl::innerEditorElement() const {
  return textControlElement()->innerEditorElement();
}

void LayoutTextControl::styleDidChange(StyleDifference diff,
                                       const ComputedStyle* oldStyle) {
  LayoutBlockFlow::styleDidChange(diff, oldStyle);
  Element* innerEditor = innerEditorElement();
  if (!innerEditor)
    return;
  LayoutBlock* innerEditorLayoutObject =
      toLayoutBlock(innerEditor->layoutObject());
  if (innerEditorLayoutObject) {
    // We may have set the width and the height in the old style in layout().
    // Reset them now to avoid getting a spurious layout hint.
    innerEditorLayoutObject->mutableStyleRef().setHeight(Length());
    innerEditorLayoutObject->mutableStyleRef().setWidth(Length());
    innerEditorLayoutObject->setStyle(createInnerEditorStyle(styleRef()));
    innerEditor->setNeedsStyleRecalc(
        SubtreeStyleChange,
        StyleChangeReasonForTracing::create(StyleChangeReason::Control));
  }
  textControlElement()->updatePlaceholderVisibility();
}

static inline void updateUserModifyProperty(TextControlElement& node,
                                            ComputedStyle& style) {
  style.setUserModify(node.isDisabledOrReadOnly() ? READ_ONLY
                                                  : READ_WRITE_PLAINTEXT_ONLY);
}

void LayoutTextControl::adjustInnerEditorStyle(
    ComputedStyle& textBlockStyle) const {
  // The inner block, if present, always has its direction set to LTR,
  // so we need to inherit the direction and unicode-bidi style from the
  // element.
  textBlockStyle.setDirection(style()->direction());
  textBlockStyle.setUnicodeBidi(style()->getUnicodeBidi());

  updateUserModifyProperty(*textControlElement(), textBlockStyle);
}

int LayoutTextControl::textBlockLogicalHeight() const {
  return (logicalHeight() - borderAndPaddingLogicalHeight()).toInt();
}

int LayoutTextControl::textBlockLogicalWidth() const {
  Element* innerEditor = innerEditorElement();
  ASSERT(innerEditor);

  LayoutUnit unitWidth = logicalWidth() - borderAndPaddingLogicalWidth();
  if (innerEditor->layoutObject())
    unitWidth -= innerEditor->layoutBox()->paddingStart() +
                 innerEditor->layoutBox()->paddingEnd();

  return unitWidth.toInt();
}

void LayoutTextControl::updateFromElement() {
  Element* innerEditor = innerEditorElement();
  if (innerEditor && innerEditor->layoutObject())
    updateUserModifyProperty(*textControlElement(),
                             innerEditor->layoutObject()->mutableStyleRef());
}

int LayoutTextControl::scrollbarThickness() const {
  // FIXME: We should get the size of the scrollbar from the LayoutTheme
  // instead.
  return ScrollbarTheme::theme().scrollbarThickness();
}

void LayoutTextControl::computeLogicalHeight(
    LayoutUnit logicalHeight,
    LayoutUnit logicalTop,
    LogicalExtentComputedValues& computedValues) const {
  HTMLElement* innerEditor = innerEditorElement();
  ASSERT(innerEditor);
  if (LayoutBox* innerEditorBox = innerEditor->layoutBox()) {
    LayoutUnit nonContentHeight = innerEditorBox->borderAndPaddingHeight() +
                                  innerEditorBox->marginHeight();
    logicalHeight = computeControlLogicalHeight(
        innerEditorBox->lineHeight(true, HorizontalLine,
                                   PositionOfInteriorLineBoxes),
        nonContentHeight);

    // We are able to have a horizontal scrollbar if the overflow style is
    // scroll, or if its auto and there's no word wrap.
    if (style()->overflowInlineDirection() == EOverflow::kScroll ||
        (style()->overflowInlineDirection() == EOverflow::kAuto &&
         innerEditor->layoutObject()->style()->overflowWrap() ==
             NormalOverflowWrap))
      logicalHeight += scrollbarThickness();

    // FIXME: The logical height of the inner text box should have been added
    // before calling computeLogicalHeight to avoid this hack.
    setIntrinsicContentLogicalHeight(logicalHeight);

    logicalHeight += borderAndPaddingHeight();
  }

  LayoutBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

void LayoutTextControl::hitInnerEditorElement(
    HitTestResult& result,
    const LayoutPoint& pointInContainer,
    const LayoutPoint& accumulatedOffset) {
  HTMLElement* innerEditor = innerEditorElement();
  if (!innerEditor->layoutObject())
    return;

  LayoutPoint adjustedLocation = accumulatedOffset + location();
  LayoutPoint localPoint =
      pointInContainer -
      toLayoutSize(adjustedLocation + innerEditor->layoutBox()->location());
  if (hasOverflowClip())
    localPoint += scrolledContentOffset();
  result.setNodeAndPosition(innerEditor, localPoint);
}

static const char* const fontFamiliesWithInvalidCharWidth[] = {
    "American Typewriter",
    "Arial Hebrew",
    "Chalkboard",
    "Cochin",
    "Corsiva Hebrew",
    "Courier",
    "Euphemia UCAS",
    "Geneva",
    "Gill Sans",
    "Hei",
    "Helvetica",
    "Hoefler Text",
    "InaiMathi",
    "Kai",
    "Lucida Grande",
    "Marker Felt",
    "Monaco",
    "Mshtakan",
    "New Peninim MT",
    "Osaka",
    "Raanana",
    "STHeiti",
    "Symbol",
    "Times",
    "Apple Braille",
    "Apple LiGothic",
    "Apple LiSung",
    "Apple Symbols",
    "AppleGothic",
    "AppleMyungjo",
    "#GungSeo",
    "#HeadLineA",
    "#PCMyungjo",
    "#PilGi",
};

// For font families where any of the fonts don't have a valid entry in the OS/2
// table for avgCharWidth, fallback to the legacy webkit behavior of getting the
// avgCharWidth from the width of a '0'. This only seems to apply to a fixed
// number of Mac fonts, but, in order to get similar rendering across platforms,
// we do this check for all platforms.
bool LayoutTextControl::hasValidAvgCharWidth(const SimpleFontData* fontData,
                                             const AtomicString& family) {
  // Some fonts match avgCharWidth to CJK full-width characters.
  // Heuristic check to avoid such fonts.
  DCHECK(fontData);
  if (!fontData)
    return false;
  const FontMetrics& metrics = fontData->getFontMetrics();
  if (metrics.hasZeroWidth() &&
      fontData->avgCharWidth() > metrics.zeroWidth() * 1.7)
    return false;

  static HashSet<AtomicString>* fontFamiliesWithInvalidCharWidthMap = nullptr;

  if (family.isEmpty())
    return false;

  if (!fontFamiliesWithInvalidCharWidthMap) {
    fontFamiliesWithInvalidCharWidthMap = new HashSet<AtomicString>;

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(fontFamiliesWithInvalidCharWidth);
         ++i)
      fontFamiliesWithInvalidCharWidthMap->insert(
          AtomicString(fontFamiliesWithInvalidCharWidth[i]));
  }

  return !fontFamiliesWithInvalidCharWidthMap->contains(family);
}

float LayoutTextControl::getAvgCharWidth(const AtomicString& family) const {
  const Font& font = style()->font();

  const SimpleFontData* primaryFont = font.primaryFont();
  if (primaryFont && hasValidAvgCharWidth(primaryFont, family))
    return roundf(primaryFont->avgCharWidth());

  const UChar ch = '0';
  const String str = String(&ch, 1);
  TextRun textRun =
      constructTextRun(font, str, styleRef(), TextRun::AllowTrailingExpansion);
  return font.width(textRun);
}

float LayoutTextControl::scaleEmToUnits(int x) const {
  // This matches the unitsPerEm value for MS Shell Dlg and Courier New from the
  // "head" font table.
  float unitsPerEm = 2048.0f;
  return roundf(style()->font().getFontDescription().computedSize() * x /
                unitsPerEm);
}

void LayoutTextControl::computeIntrinsicLogicalWidths(
    LayoutUnit& minLogicalWidth,
    LayoutUnit& maxLogicalWidth) const {
  // Use average character width. Matches IE.
  AtomicString family = style()->font().getFontDescription().family().family();
  maxLogicalWidth = preferredContentLogicalWidth(
      const_cast<LayoutTextControl*>(this)->getAvgCharWidth(family));
  if (innerEditorElement()) {
    if (LayoutBox* innerEditorLayoutBox = innerEditorElement()->layoutBox())
      maxLogicalWidth += innerEditorLayoutBox->paddingStart() +
                         innerEditorLayoutBox->paddingEnd();
  }
  if (!style()->logicalWidth().isPercentOrCalc())
    minLogicalWidth = maxLogicalWidth;
}

void LayoutTextControl::computePreferredLogicalWidths() {
  ASSERT(preferredLogicalWidthsDirty());

  m_minPreferredLogicalWidth = LayoutUnit();
  m_maxPreferredLogicalWidth = LayoutUnit();
  const ComputedStyle& styleToUse = styleRef();

  if (styleToUse.logicalWidth().isFixed() &&
      styleToUse.logicalWidth().value() >= 0)
    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth =
        adjustContentBoxLogicalWidthForBoxSizing(
            styleToUse.logicalWidth().value());
  else
    computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth,
                                  m_maxPreferredLogicalWidth);

  if (styleToUse.logicalMinWidth().isFixed() &&
      styleToUse.logicalMinWidth().value() > 0) {
    m_maxPreferredLogicalWidth = std::max(
        m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(
                                        styleToUse.logicalMinWidth().value()));
    m_minPreferredLogicalWidth = std::max(
        m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(
                                        styleToUse.logicalMinWidth().value()));
  }

  if (styleToUse.logicalMaxWidth().isFixed()) {
    m_maxPreferredLogicalWidth = std::min(
        m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(
                                        styleToUse.logicalMaxWidth().value()));
    m_minPreferredLogicalWidth = std::min(
        m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(
                                        styleToUse.logicalMaxWidth().value()));
  }

  LayoutUnit toAdd = borderAndPaddingLogicalWidth();

  m_minPreferredLogicalWidth += toAdd;
  m_maxPreferredLogicalWidth += toAdd;

  clearPreferredLogicalWidthsDirty();
}

void LayoutTextControl::addOutlineRects(Vector<LayoutRect>& rects,
                                        const LayoutPoint& additionalOffset,
                                        IncludeBlockVisualOverflowOrNot) const {
  rects.push_back(LayoutRect(additionalOffset, size()));
}

LayoutObject* LayoutTextControl::layoutSpecialExcludedChild(
    bool relayoutChildren,
    SubtreeLayoutScope& layoutScope) {
  HTMLElement* placeholder = toTextControlElement(node())->placeholderElement();
  LayoutObject* placeholderLayoutObject =
      placeholder ? placeholder->layoutObject() : nullptr;
  if (!placeholderLayoutObject)
    return nullptr;
  if (relayoutChildren)
    layoutScope.setChildNeedsLayout(placeholderLayoutObject);
  return placeholderLayoutObject;
}

int LayoutTextControl::firstLineBoxBaseline() const {
  int result = LayoutBlock::firstLineBoxBaseline();
  if (result != -1)
    return result;

  // When the text is empty, |LayoutBlock::firstLineBoxBaseline()| cannot
  // compute the baseline because lineboxes do not exist.
  Element* innerEditor = innerEditorElement();
  if (!innerEditor || !innerEditor->layoutObject())
    return -1;

  LayoutBlock* innerEditorLayoutObject =
      toLayoutBlock(innerEditor->layoutObject());
  const SimpleFontData* fontData =
      innerEditorLayoutObject->style(true)->font().primaryFont();
  DCHECK(fontData);
  if (!fontData)
    return -1;

  LayoutUnit baseline(fontData->getFontMetrics().ascent(AlphabeticBaseline));
  for (LayoutObject* box = innerEditorLayoutObject; box && box != this;
       box = box->parent()) {
    if (box->isBox())
      baseline += toLayoutBox(box)->logicalTop();
  }
  return baseline.toInt();
}

}  // namespace blink
