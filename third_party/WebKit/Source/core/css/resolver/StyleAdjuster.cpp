/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/css/resolver/StyleAdjuster.h"

#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/layout/LayoutTheme.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/Length.h"
#include "platform/transforms/TransformOperations.h"
#include "wtf/Assertions.h"

namespace blink {

using namespace HTMLNames;

static EDisplay equivalentBlockDisplay(EDisplay display) {
  switch (display) {
    case EDisplay::Block:
    case EDisplay::Table:
    case EDisplay::WebkitBox:
    case EDisplay::Flex:
    case EDisplay::Grid:
    case EDisplay::ListItem:
    case EDisplay::FlowRoot:
      return display;
    case EDisplay::InlineTable:
      return EDisplay::Table;
    case EDisplay::WebkitInlineBox:
      return EDisplay::WebkitBox;
    case EDisplay::InlineFlex:
      return EDisplay::Flex;
    case EDisplay::InlineGrid:
      return EDisplay::Grid;

    case EDisplay::Contents:
    case EDisplay::Inline:
    case EDisplay::InlineBlock:
    case EDisplay::TableRowGroup:
    case EDisplay::TableHeaderGroup:
    case EDisplay::TableFooterGroup:
    case EDisplay::TableRow:
    case EDisplay::TableColumnGroup:
    case EDisplay::TableColumn:
    case EDisplay::TableCell:
    case EDisplay::TableCaption:
      return EDisplay::Block;
    case EDisplay::None:
      ASSERT_NOT_REACHED();
      return display;
  }
  ASSERT_NOT_REACHED();
  return EDisplay::Block;
}

static bool isOutermostSVGElement(const Element* element) {
  return element && element->isSVGElement() &&
         toSVGElement(*element).isOutermostSVGSVGElement();
}

// CSS requires text-decoration to be reset at each DOM element for
// inline blocks, inline tables, shadow DOM crossings, floating elements,
// and absolute or relatively positioned elements. Outermost <svg> roots are
// considered to be atomic inline-level.
static bool doesNotInheritTextDecoration(const ComputedStyle& style,
                                         const Element* element) {
  return style.display() == EDisplay::InlineTable ||
         style.display() == EDisplay::InlineBlock ||
         style.display() == EDisplay::WebkitInlineBox ||
         isAtShadowBoundary(element) || style.isFloating() ||
         style.hasOutOfFlowPosition() || isOutermostSVGElement(element) ||
         isHTMLRTElement(element);
}

// Certain elements (<a>, <font>) override text decoration colors.  "The font
// element is expected to override the color of any text decoration that spans
// the text of the element to the used value of the element's 'color' property."
// (https://html.spec.whatwg.org/multipage/rendering.html#phrasing-content-3)
// The <a> behavior is non-standard.
static bool overridesTextDecorationColors(const Element* element) {
  return element &&
         (isHTMLFontElement(element) || isHTMLAnchorElement(element));
}

// FIXME: This helper is only needed because pseudoStyleForElement passes a null
// element to adjustComputedStyle, so we can't just use element->isInTopLayer().
static bool isInTopLayer(const Element* element, const ComputedStyle& style) {
  return (element && element->isInTopLayer()) ||
         style.styleType() == PseudoIdBackdrop;
}

static bool layoutParentStyleForcesZIndexToCreateStackingContext(
    const ComputedStyle& layoutParentStyle) {
  return layoutParentStyle.isDisplayFlexibleOrGridBox();
}

void StyleAdjuster::adjustStyleForEditing(ComputedStyle& style) {
  if (style.userModify() != READ_WRITE_PLAINTEXT_ONLY)
    return;
  // Collapsing whitespace is harmful in plain-text editing.
  if (style.whiteSpace() == EWhiteSpace::kNormal)
    style.setWhiteSpace(EWhiteSpace::kPreWrap);
  else if (style.whiteSpace() == EWhiteSpace::kNowrap)
    style.setWhiteSpace(EWhiteSpace::kPre);
  else if (style.whiteSpace() == EWhiteSpace::kPreLine)
    style.setWhiteSpace(EWhiteSpace::kPreWrap);
}

static void adjustStyleForFirstLetter(ComputedStyle& style) {
  if (style.styleType() != PseudoIdFirstLetter)
    return;

  // Force inline display (except for floating first-letters).
  style.setDisplay(style.isFloating() ? EDisplay::Block : EDisplay::Inline);

  // CSS2 says first-letter can't be positioned.
  style.setPosition(EPosition::kStatic);
}

void StyleAdjuster::adjustStyleForAlignment(
    ComputedStyle& style,
    const ComputedStyle& layoutParentStyle) {
  // To avoid needing to copy the RareNonInheritedData, we repurpose the 'auto'
  // flag to not just mean 'auto' prior to running the StyleAdjuster but also
  // mean 'normal' after running it.

  // If the inherited value of justify-items includes the 'legacy' keyword,
  // 'auto' computes to the the inherited value.  Otherwise, 'auto' computes to
  // 'normal'.
  if (style.justifyItemsPosition() == ItemPositionAuto) {
    if (layoutParentStyle.justifyItemsPositionType() == LegacyPosition)
      style.setJustifyItems(layoutParentStyle.justifyItems());
  }

  // The 'auto' keyword computes the computed value of justify-items on the
  // parent (minus any legacy keywords), or 'normal' if the box has no parent.
  if (style.justifySelfPosition() == ItemPositionAuto) {
    if (layoutParentStyle.justifyItemsPositionType() == LegacyPosition)
      style.setJustifySelfPosition(layoutParentStyle.justifyItemsPosition());
    else if (layoutParentStyle.justifyItemsPosition() != ItemPositionAuto)
      style.setJustifySelf(layoutParentStyle.justifyItems());
  }

  // The 'auto' keyword computes the computed value of align-items on the parent
  // or 'normal' if the box has no parent.
  if (style.alignSelfPosition() == ItemPositionAuto &&
      layoutParentStyle.alignItemsPosition() !=
          ComputedStyle::initialDefaultAlignment().position())
    style.setAlignSelf(layoutParentStyle.alignItems());
}

static void adjustStyleForHTMLElement(ComputedStyle& style,
                                      HTMLElement& element) {
  // <div> and <span> are the most common elements on the web, we skip all the
  // work for them.
  if (isHTMLDivElement(element) || isHTMLSpanElement(element))
    return;

  if (isHTMLTableCellElement(element)) {
    if (style.whiteSpace() == EWhiteSpace::kWebkitNowrap) {
      // Figure out if we are really nowrapping or if we should just
      // use normal instead. If the width of the cell is fixed, then
      // we don't actually use NOWRAP.
      if (style.width().isFixed())
        style.setWhiteSpace(EWhiteSpace::kNormal);
      else
        style.setWhiteSpace(EWhiteSpace::kNowrap);
    }
    return;
  }

  if (isHTMLImageElement(element)) {
    if (toHTMLImageElement(element).isCollapsed())
      style.setDisplay(EDisplay::None);
    return;
  }

  if (isHTMLTableElement(element)) {
    // Tables never support the -webkit-* values for text-align and will reset
    // back to the default.
    if (style.textAlign() == ETextAlign::kWebkitLeft ||
        style.textAlign() == ETextAlign::kWebkitCenter ||
        style.textAlign() == ETextAlign::kWebkitRight)
      style.setTextAlign(ETextAlign::kStart);
    return;
  }

  if (isHTMLFrameElement(element) || isHTMLFrameSetElement(element)) {
    // Frames and framesets never honor position:relative or position:absolute.
    // This is necessary to fix a crash where a site tries to position these
    // objects. They also never honor display.
    style.setPosition(EPosition::kStatic);
    style.setDisplay(EDisplay::Block);
    return;
  }

  if (isHTMLFrameElementBase(element)) {
    // Frames cannot overflow (they are always the size we ask them to be).
    // Some compositing code paths may try to draw scrollbars anyhow.
    style.setOverflowX(EOverflow::kVisible);
    style.setOverflowY(EOverflow::kVisible);
    return;
  }

  if (isHTMLRTElement(element)) {
    // Ruby text does not support float or position. This might change with
    // evolution of the specification.
    style.setPosition(EPosition::kStatic);
    style.setFloating(EFloat::kNone);
    return;
  }

  if (isHTMLLegendElement(element)) {
    style.setDisplay(EDisplay::Block);
    return;
  }

  if (isHTMLMarqueeElement(element)) {
    // For now, <marquee> requires an overflow clip to work properly.
    style.setOverflowX(EOverflow::kHidden);
    style.setOverflowY(EOverflow::kHidden);
    return;
  }

  if (isHTMLTextAreaElement(element)) {
    // Textarea considers overflow visible as auto.
    style.setOverflowX(style.overflowX() == EOverflow::kVisible
                           ? EOverflow::kAuto
                           : style.overflowX());
    style.setOverflowY(style.overflowY() == EOverflow::kVisible
                           ? EOverflow::kAuto
                           : style.overflowY());
    return;
  }

  if (isHTMLPlugInElement(element)) {
    style.setRequiresAcceleratedCompositingForExternalReasons(
        toHTMLPlugInElement(element).shouldAccelerate());
    return;
  }
}

static void adjustOverflow(ComputedStyle& style) {
  DCHECK(style.overflowX() != EOverflow::kVisible ||
         style.overflowY() != EOverflow::kVisible);

  if (style.display() == EDisplay::Table ||
      style.display() == EDisplay::InlineTable) {
    // Tables only support overflow:hidden and overflow:visible and ignore
    // anything else, see http://dev.w3.org/csswg/css2/visufx.html#overflow. As
    // a table is not a block container box the rules for resolving conflicting
    // x and y values in CSS Overflow Module Level 3 do not apply. Arguably
    // overflow-x and overflow-y aren't allowed on tables but all UAs allow it.
    if (style.overflowX() != EOverflow::kHidden)
      style.setOverflowX(EOverflow::kVisible);
    if (style.overflowY() != EOverflow::kHidden)
      style.setOverflowY(EOverflow::kVisible);
    // If we are left with conflicting overflow values for the x and y axes on a
    // table then resolve both to OverflowVisible. This is interoperable
    // behaviour but is not specced anywhere.
    if (style.overflowX() == EOverflow::kVisible)
      style.setOverflowY(EOverflow::kVisible);
    else if (style.overflowY() == EOverflow::kVisible)
      style.setOverflowX(EOverflow::kVisible);
  } else if (style.overflowX() == EOverflow::kVisible &&
             style.overflowY() != EOverflow::kVisible) {
    // If either overflow value is not visible, change to auto.
    // FIXME: Once we implement pagination controls, overflow-x should default
    // to hidden if overflow-y is set to -webkit-paged-x or -webkit-page-y. For
    // now, we'll let it default to auto so we can at least scroll through the
    // pages.
    style.setOverflowX(EOverflow::kAuto);
  } else if (style.overflowY() == EOverflow::kVisible &&
             style.overflowX() != EOverflow::kVisible) {
    style.setOverflowY(EOverflow::kAuto);
  }

  // Menulists should have visible overflow
  if (style.appearance() == MenulistPart) {
    style.setOverflowX(EOverflow::kVisible);
    style.setOverflowY(EOverflow::kVisible);
  }
}

static void adjustStyleForDisplay(ComputedStyle& style,
                                  const ComputedStyle& layoutParentStyle,
                                  Document* document) {
  if (style.display() == EDisplay::Block && !style.isFloating())
    return;

  if (style.display() == EDisplay::Contents)
    return;

  // FIXME: Don't support this mutation for pseudo styles like first-letter or
  // first-line, since it's not completely clear how that should work.
  if (style.display() == EDisplay::Inline &&
      style.styleType() == PseudoIdNone &&
      style.getWritingMode() != layoutParentStyle.getWritingMode())
    style.setDisplay(EDisplay::InlineBlock);

  // We do not honor position: relative or sticky for table rows, headers, and
  // footers. This is correct for position: relative in CSS2.1 (and caused a
  // crash in containingBlock() on some sites) and position: sticky is defined
  // as following position: relative behavior for table elements. It is
  // incorrect for CSS3.
  if ((style.display() == EDisplay::TableHeaderGroup ||
       style.display() == EDisplay::TableRowGroup ||
       style.display() == EDisplay::TableFooterGroup ||
       style.display() == EDisplay::TableRow) &&
      style.hasInFlowPosition())
    style.setPosition(EPosition::kStatic);

  // Cannot support position: sticky for table columns and column groups because
  // current code is only doing background painting through columns / column
  // groups.
  if ((style.display() == EDisplay::TableColumnGroup ||
       style.display() == EDisplay::TableColumn) &&
      style.position() == EPosition::kSticky)
    style.setPosition(EPosition::kStatic);

  // writing-mode does not apply to table row groups, table column groups, table
  // rows, and table columns.
  // FIXME: Table cells should be allowed to be perpendicular or flipped with
  // respect to the table, though.
  if (style.display() == EDisplay::TableColumn ||
      style.display() == EDisplay::TableColumnGroup ||
      style.display() == EDisplay::TableFooterGroup ||
      style.display() == EDisplay::TableHeaderGroup ||
      style.display() == EDisplay::TableRow ||
      style.display() == EDisplay::TableRowGroup ||
      style.display() == EDisplay::TableCell)
    style.setWritingMode(layoutParentStyle.getWritingMode());

  // FIXME: Since we don't support block-flow on flexible boxes yet, disallow
  // setting of block-flow to anything other than TopToBottomWritingMode.
  // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
  if (style.getWritingMode() != WritingMode::kHorizontalTb &&
      (style.display() == EDisplay::WebkitBox ||
       style.display() == EDisplay::WebkitInlineBox))
    style.setWritingMode(WritingMode::kHorizontalTb);

  if (layoutParentStyle.isDisplayFlexibleOrGridBox()) {
    style.setFloating(EFloat::kNone);
    style.setDisplay(equivalentBlockDisplay(style.display()));

    // We want to count vertical percentage paddings/margins on flex items
    // because our current behavior is different from the spec and we want to
    // gather compatibility data.
    if (style.paddingBefore().isPercentOrCalc() ||
        style.paddingAfter().isPercentOrCalc())
      UseCounter::count(document, UseCounter::FlexboxPercentagePaddingVertical);
    if (style.marginBefore().isPercentOrCalc() ||
        style.marginAfter().isPercentOrCalc())
      UseCounter::count(document, UseCounter::FlexboxPercentageMarginVertical);
  }
}

void StyleAdjuster::adjustComputedStyle(ComputedStyle& style,
                                        const ComputedStyle& parentStyle,
                                        const ComputedStyle& layoutParentStyle,
                                        Element* element) {
  if (style.display() != EDisplay::None) {
    if (element && element->isHTMLElement())
      adjustStyleForHTMLElement(style, toHTMLElement(*element));

    // Per the spec, position 'static' and 'relative' in the top layer compute
    // to 'absolute'.
    if (isInTopLayer(element, style) &&
        (style.position() == EPosition::kStatic ||
         style.position() == EPosition::kRelative))
      style.setPosition(EPosition::kAbsolute);

    // Absolute/fixed positioned elements, floating elements and the document
    // element need block-like outside display.
    if (style.display() != EDisplay::Contents &&
        (style.hasOutOfFlowPosition() || style.isFloating()))
      style.setDisplay(equivalentBlockDisplay(style.display()));

    if (element && element->document().documentElement() == element)
      style.setDisplay(equivalentBlockDisplay(style.display()));

    // We don't adjust the first letter style earlier because we may change the
    // display setting in adjustStyeForTagName() above.
    adjustStyleForFirstLetter(style);

    adjustStyleForDisplay(style, layoutParentStyle,
                          element ? &element->document() : 0);

    // Paint containment forces a block formatting context, so we must coerce
    // from inline.  https://drafts.csswg.org/css-containment/#containment-paint
    if (style.containsPaint() && style.display() == EDisplay::Inline)
      style.setDisplay(EDisplay::Block);
  } else {
    adjustStyleForFirstLetter(style);
  }

  if (element && element->hasCompositorProxy())
    style.setHasCompositorProxy(true);

  // Make sure our z-index value is only applied if the object is positioned.
  if (style.position() == EPosition::kStatic &&
      !layoutParentStyleForcesZIndexToCreateStackingContext(
          layoutParentStyle)) {
    style.setIsStackingContext(false);
    // TODO(alancutter): Avoid altering z-index here.
    if (!style.hasAutoZIndex())
      style.setZIndex(0);
  } else if (!style.hasAutoZIndex()) {
    style.setIsStackingContext(true);
  }

  if (style.overflowX() != EOverflow::kVisible ||
      style.overflowY() != EOverflow::kVisible)
    adjustOverflow(style);

  if (doesNotInheritTextDecoration(style, element))
    style.clearAppliedTextDecorations();
  else
    style.restoreParentTextDecorations(parentStyle);
  style.applyTextDecorations(
      parentStyle.visitedDependentColor(CSSPropertyTextDecorationColor),
      overridesTextDecorationColors(element));

  // Cull out any useless layers and also repeat patterns into additional
  // layers.
  style.adjustBackgroundLayers();
  style.adjustMaskLayers();

  // Let the theme also have a crack at adjusting the style.
  if (style.hasAppearance())
    LayoutTheme::theme().adjustStyle(style, element);

  // If we have first-letter pseudo style, transitions, or animations, do not
  // share this style.
  if (style.hasPseudoStyle(PseudoIdFirstLetter) || style.transitions() ||
      style.animations())
    style.setUnique();

  adjustStyleForEditing(style);

  bool isSVGElement = element && element->isSVGElement();
  if (isSVGElement) {
    // display: contents computes to inline for replaced elements and form
    // controls, and isn't specified for other kinds of SVG content[1], so let's
    // just do the same here for all other SVG elements.
    //
    // If we wouldn't do this, then we'd need to ensure that display: contents
    // doesn't prevent SVG elements from generating a LayoutObject in
    // SVGElement::layoutObjectIsNeeded.
    //
    // [1]: https://www.w3.org/TR/SVG/painting.html#DisplayProperty
    if (style.display() == EDisplay::Contents)
      style.setDisplay(EDisplay::Inline);

    // Only the root <svg> element in an SVG document fragment tree honors css
    // position.
    if (!(isSVGSVGElement(*element) && element->parentNode() &&
          !element->parentNode()->isSVGElement()))
      style.setPosition(ComputedStyle::initialPosition());

    // SVG text layout code expects us to be a block-level style element.
    if ((isSVGForeignObjectElement(*element) || isSVGTextElement(*element)) &&
        style.isDisplayInlineType())
      style.setDisplay(EDisplay::Block);

    // Columns don't apply to svg text elements.
    if (isSVGTextElement(*element))
      style.clearMultiCol();
  }
  adjustStyleForAlignment(style, layoutParentStyle);
}

}  // namespace blink
