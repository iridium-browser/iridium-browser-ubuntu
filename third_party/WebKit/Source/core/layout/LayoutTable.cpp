/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "core/layout/LayoutTable.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLTableElement.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutTableCaption.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableSection.h"
#include "core/layout/LayoutView.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/layout/TableLayoutAlgorithmAuto.h"
#include "core/layout/TableLayoutAlgorithmFixed.h"
#include "core/layout/TextAutosizer.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/TablePaintInvalidator.h"
#include "core/paint/TablePainter.h"
#include "core/style/StyleInheritedData.h"
#include "wtf/PtrUtil.h"

namespace blink {

using namespace HTMLNames;

LayoutTable::LayoutTable(Element* element)
    : LayoutBlock(element),
      m_head(nullptr),
      m_foot(nullptr),
      m_firstBody(nullptr),
      m_collapsedBordersValid(false),
      m_hasColElements(false),
      m_needsSectionRecalc(false),
      m_columnLogicalWidthChanged(false),
      m_columnLayoutObjectsValid(false),
      m_noCellColspanAtLeast(0),
      m_hSpacing(0),
      m_vSpacing(0),
      m_borderStart(0),
      m_borderEnd(0) {
  ASSERT(!childrenInline());
  m_effectiveColumnPositions.fill(0, 1);
}

LayoutTable::~LayoutTable() {}

void LayoutTable::styleDidChange(StyleDifference diff,
                                 const ComputedStyle* oldStyle) {
  LayoutBlock::styleDidChange(diff, oldStyle);

  bool oldFixedTableLayout = oldStyle ? oldStyle->isFixedTableLayout() : false;

  // In the collapsed border model, there is no cell spacing.
  m_hSpacing = collapseBorders() ? 0 : style()->horizontalBorderSpacing();
  m_vSpacing = collapseBorders() ? 0 : style()->verticalBorderSpacing();
  m_effectiveColumnPositions[0] = m_hSpacing;

  if (!m_tableLayout || style()->isFixedTableLayout() != oldFixedTableLayout) {
    if (m_tableLayout)
      m_tableLayout->willChangeTableLayout();

    // According to the CSS2 spec, you only use fixed table layout if an
    // explicit width is specified on the table. Auto width implies auto table
    // layout.
    if (style()->isFixedTableLayout())
      m_tableLayout = WTF::makeUnique<TableLayoutAlgorithmFixed>(this);
    else
      m_tableLayout = WTF::makeUnique<TableLayoutAlgorithmAuto>(this);
  }

  // If border was changed, invalidate collapsed borders cache.
  if (!needsLayout() && oldStyle && oldStyle->border() != style()->border())
    invalidateCollapsedBorders();
  if (LayoutTableBoxComponent::doCellsHaveDirtyWidth(*this, *this, diff,
                                                     *oldStyle))
    markAllCellsWidthsDirtyAndOrNeedsLayout(MarkDirtyAndNeedsLayout);
}

static inline void resetSectionPointerIfNotBefore(LayoutTableSection*& ptr,
                                                  LayoutObject* before) {
  if (!before || !ptr)
    return;
  LayoutObject* o = before->previousSibling();
  while (o && o != ptr)
    o = o->previousSibling();
  if (!o)
    ptr = 0;
}

static inline bool needsTableSection(LayoutObject* object) {
  // Return true if 'object' can't exist in an anonymous table without being
  // wrapped in a table section box.
  EDisplay display = object->style()->display();
  return display != EDisplay::TableCaption &&
         display != EDisplay::TableColumnGroup &&
         display != EDisplay::TableColumn;
}

void LayoutTable::addChild(LayoutObject* child, LayoutObject* beforeChild) {
  bool wrapInAnonymousSection = !child->isOutOfFlowPositioned();

  if (child->isTableCaption()) {
    wrapInAnonymousSection = false;
  } else if (child->isLayoutTableCol()) {
    m_hasColElements = true;
    wrapInAnonymousSection = false;
  } else if (child->isTableSection()) {
    switch (child->style()->display()) {
      case EDisplay::TableHeaderGroup:
        resetSectionPointerIfNotBefore(m_head, beforeChild);
        if (!m_head) {
          m_head = toLayoutTableSection(child);
        } else {
          resetSectionPointerIfNotBefore(m_firstBody, beforeChild);
          if (!m_firstBody)
            m_firstBody = toLayoutTableSection(child);
        }
        wrapInAnonymousSection = false;
        break;
      case EDisplay::TableFooterGroup:
        resetSectionPointerIfNotBefore(m_foot, beforeChild);
        if (!m_foot) {
          m_foot = toLayoutTableSection(child);
          wrapInAnonymousSection = false;
          break;
        }
      // Fall through.
      case EDisplay::TableRowGroup:
        resetSectionPointerIfNotBefore(m_firstBody, beforeChild);
        if (!m_firstBody)
          m_firstBody = toLayoutTableSection(child);
        wrapInAnonymousSection = false;
        break;
      default:
        ASSERT_NOT_REACHED();
    }
  } else {
    wrapInAnonymousSection = true;
  }

  if (child->isTableSection())
    setNeedsSectionRecalc();

  if (!wrapInAnonymousSection) {
    if (beforeChild && beforeChild->parent() != this)
      beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

    LayoutBox::addChild(child, beforeChild);
    return;
  }

  if (!beforeChild && lastChild() && lastChild()->isTableSection() &&
      lastChild()->isAnonymous() && !lastChild()->isBeforeContent()) {
    lastChild()->addChild(child);
    return;
  }

  if (beforeChild && !beforeChild->isAnonymous() &&
      beforeChild->parent() == this) {
    LayoutObject* section = beforeChild->previousSibling();
    if (section && section->isTableSection() && section->isAnonymous()) {
      section->addChild(child);
      return;
    }
  }

  LayoutObject* lastBox = beforeChild;
  while (lastBox && lastBox->parent()->isAnonymous() &&
         !lastBox->isTableSection() && needsTableSection(lastBox))
    lastBox = lastBox->parent();
  if (lastBox && lastBox->isAnonymous() && !isAfterContent(lastBox)) {
    if (beforeChild == lastBox)
      beforeChild = lastBox->slowFirstChild();
    lastBox->addChild(child, beforeChild);
    return;
  }

  if (beforeChild && !beforeChild->isTableSection() &&
      needsTableSection(beforeChild))
    beforeChild = 0;

  LayoutTableSection* section =
      LayoutTableSection::createAnonymousWithParent(this);
  addChild(section, beforeChild);
  section->addChild(child);
}

void LayoutTable::addCaption(const LayoutTableCaption* caption) {
  ASSERT(m_captions.find(caption) == kNotFound);
  m_captions.push_back(const_cast<LayoutTableCaption*>(caption));
}

void LayoutTable::removeCaption(const LayoutTableCaption* oldCaption) {
  size_t index = m_captions.find(oldCaption);
  ASSERT(index != kNotFound);
  if (index == kNotFound)
    return;

  m_captions.remove(index);
}

void LayoutTable::invalidateCachedColumns() {
  m_columnLayoutObjectsValid = false;
  m_columnLayoutObjects.resize(0);
}

void LayoutTable::addColumn(const LayoutTableCol*) {
  invalidateCachedColumns();
}

void LayoutTable::removeColumn(const LayoutTableCol*) {
  invalidateCachedColumns();
  // We don't really need to recompute our sections, but we need to update our
  // column count and whether we have a column. Currently, we only have one
  // size-fit-all flag but we may have to consider splitting it.
  setNeedsSectionRecalc();
}

bool LayoutTable::isLogicalWidthAuto() const {
  Length styleLogicalWidth = style()->logicalWidth();
  return (!styleLogicalWidth.isSpecified() ||
          !styleLogicalWidth.isPositive()) &&
         !styleLogicalWidth.isIntrinsic();
}

void LayoutTable::updateLogicalWidth() {
  recalcSectionsIfNeeded();

  if (isOutOfFlowPositioned()) {
    LogicalExtentComputedValues computedValues;
    computePositionedLogicalWidth(computedValues);
    setLogicalWidth(computedValues.m_extent);
    setLogicalLeft(computedValues.m_position);
    setMarginStart(computedValues.m_margins.m_start);
    setMarginEnd(computedValues.m_margins.m_end);
  }

  LayoutBlock* cb = containingBlock();

  LayoutUnit availableLogicalWidth = containingBlockLogicalWidthForContent();
  bool hasPerpendicularContainingBlock =
      cb->style()->isHorizontalWritingMode() !=
      style()->isHorizontalWritingMode();
  LayoutUnit containerWidthInInlineDirection =
      hasPerpendicularContainingBlock
          ? perpendicularContainingBlockLogicalHeight()
          : availableLogicalWidth;

  Length styleLogicalWidth = style()->logicalWidth();
  if (!isLogicalWidthAuto()) {
    setLogicalWidth(convertStyleLogicalWidthToComputedWidth(
        styleLogicalWidth, containerWidthInInlineDirection));
  } else {
    // Subtract out any fixed margins from our available width for auto width
    // tables.
    LayoutUnit marginStart =
        minimumValueForLength(style()->marginStart(), availableLogicalWidth);
    LayoutUnit marginEnd =
        minimumValueForLength(style()->marginEnd(), availableLogicalWidth);
    LayoutUnit marginTotal = marginStart + marginEnd;

    // Subtract out our margins to get the available content width.
    LayoutUnit availableContentLogicalWidth =
        (containerWidthInInlineDirection - marginTotal).clampNegativeToZero();
    if (shrinkToAvoidFloats() && cb->isLayoutBlockFlow() &&
        toLayoutBlockFlow(cb)->containsFloats() &&
        !hasPerpendicularContainingBlock)
      availableContentLogicalWidth = shrinkLogicalWidthToAvoidFloats(
          marginStart, marginEnd, toLayoutBlockFlow(cb));

    // Ensure we aren't bigger than our available width.
    LayoutUnit maxWidth = maxPreferredLogicalWidth();
    // scaledWidthFromPercentColumns depends on m_layoutStruct in
    // TableLayoutAlgorithmAuto, which maxPreferredLogicalWidth fills in. So
    // scaledWidthFromPercentColumns has to be called after
    // maxPreferredLogicalWidth.
    LayoutUnit scaledWidth = m_tableLayout->scaledWidthFromPercentColumns() +
                             bordersPaddingAndSpacingInRowDirection();
    maxWidth = std::max(scaledWidth, maxWidth);
    setLogicalWidth(
        LayoutUnit(std::min(availableContentLogicalWidth, maxWidth).floor()));
  }

  // Ensure we aren't bigger than our max-width style.
  Length styleMaxLogicalWidth = style()->logicalMaxWidth();
  if ((styleMaxLogicalWidth.isSpecified() &&
       !styleMaxLogicalWidth.isNegative()) ||
      styleMaxLogicalWidth.isIntrinsic()) {
    LayoutUnit computedMaxLogicalWidth =
        convertStyleLogicalWidthToComputedWidth(styleMaxLogicalWidth,
                                                availableLogicalWidth);
    setLogicalWidth(
        LayoutUnit(std::min(logicalWidth(), computedMaxLogicalWidth).floor()));
  }

  // Ensure we aren't smaller than our min preferred width. This MUST be done
  // after 'max-width' as we ignore it if it means we wouldn't accommodate our
  // content.
  setLogicalWidth(
      LayoutUnit(std::max(logicalWidth(), minPreferredLogicalWidth()).floor()));

  // Ensure we aren't smaller than our min-width style.
  Length styleMinLogicalWidth = style()->logicalMinWidth();
  if ((styleMinLogicalWidth.isSpecified() &&
       !styleMinLogicalWidth.isNegative()) ||
      styleMinLogicalWidth.isIntrinsic()) {
    LayoutUnit computedMinLogicalWidth =
        convertStyleLogicalWidthToComputedWidth(styleMinLogicalWidth,
                                                availableLogicalWidth);
    setLogicalWidth(
        LayoutUnit(std::max(logicalWidth(), computedMinLogicalWidth).floor()));
  }

  // Finally, with our true width determined, compute our margins for real.
  ComputedMarginValues marginValues;
  computeMarginsForDirection(InlineDirection, cb, availableLogicalWidth,
                             logicalWidth(), marginValues.m_start,
                             marginValues.m_end, style()->marginStart(),
                             style()->marginEnd());
  setMarginStart(marginValues.m_start);
  setMarginEnd(marginValues.m_end);

  // We should NEVER shrink the table below the min-content logical width, or
  // else the table can't accommodate its own content which doesn't match CSS
  // nor what authors expect.
  // FIXME: When we convert to sub-pixel layout for tables we can remove the int
  // conversion. http://crbug.com/241198
  ASSERT(logicalWidth().floor() >= minPreferredLogicalWidth().floor());
}

// This method takes a ComputedStyle's logical width, min-width, or max-width
// length and computes its actual value.
LayoutUnit LayoutTable::convertStyleLogicalWidthToComputedWidth(
    const Length& styleLogicalWidth,
    LayoutUnit availableWidth) const {
  if (styleLogicalWidth.isIntrinsic())
    return computeIntrinsicLogicalWidthUsing(
        styleLogicalWidth, availableWidth,
        bordersPaddingAndSpacingInRowDirection());

  // HTML tables' width styles already include borders and paddings, but CSS
  // tables' width styles do not.
  LayoutUnit borders;
  bool isCSSTable = !isHTMLTableElement(node());
  if (isCSSTable && styleLogicalWidth.isSpecified() &&
      styleLogicalWidth.isPositive() &&
      style()->boxSizing() == EBoxSizing::kContentBox)
    borders =
        borderStart() + borderEnd() +
        (collapseBorders() ? LayoutUnit() : paddingStart() + paddingEnd());

  return minimumValueForLength(styleLogicalWidth, availableWidth) + borders;
}

LayoutUnit LayoutTable::convertStyleLogicalHeightToComputedHeight(
    const Length& styleLogicalHeight) const {
  LayoutUnit borderAndPaddingBefore =
      borderBefore() + (collapseBorders() ? LayoutUnit() : paddingBefore());
  LayoutUnit borderAndPaddingAfter =
      borderAfter() + (collapseBorders() ? LayoutUnit() : paddingAfter());
  LayoutUnit borderAndPadding = borderAndPaddingBefore + borderAndPaddingAfter;
  LayoutUnit computedLogicalHeight;
  if (styleLogicalHeight.isFixed()) {
    // HTML tables size as though CSS height includes border/padding, CSS tables
    // do not.
    LayoutUnit borders = LayoutUnit();
    // FIXME: We cannot apply box-sizing: content-box on <table> which other
    // browsers allow.
    if (isHTMLTableElement(node()) ||
        style()->boxSizing() == EBoxSizing::kBorderBox) {
      borders = borderAndPadding;
    }
    computedLogicalHeight = LayoutUnit(styleLogicalHeight.value() - borders);
  } else if (styleLogicalHeight.isPercentOrCalc()) {
    computedLogicalHeight = computePercentageLogicalHeight(styleLogicalHeight);
  } else if (styleLogicalHeight.isIntrinsic()) {
    computedLogicalHeight = computeIntrinsicLogicalContentHeightUsing(
        styleLogicalHeight, logicalHeight() - borderAndPadding,
        borderAndPadding);
  } else {
    ASSERT_NOT_REACHED();
  }
  return computedLogicalHeight.clampNegativeToZero();
}

void LayoutTable::layoutCaption(LayoutTableCaption& caption,
                                SubtreeLayoutScope& layouter) {
  if (!caption.needsLayout())
    markChildForPaginationRelayoutIfNeeded(caption, layouter);
  if (caption.needsLayout()) {
    // The margins may not be available but ensure the caption is at least
    // located beneath any previous sibling caption so that it does not
    // mistakenly think any floats in the previous caption intrude into it.
    caption.setLogicalLocation(
        LayoutPoint(caption.marginStart(),
                    collapsedMarginBeforeForChild(caption) + logicalHeight()));
    // If LayoutTableCaption ever gets a layout() function, use it here.
    caption.layoutIfNeeded();
  }
  // Apply the margins to the location now that they are definitely available
  // from layout
  LayoutUnit captionLogicalTop =
      collapsedMarginBeforeForChild(caption) + logicalHeight();
  caption.setLogicalLocation(
      LayoutPoint(caption.marginStart(), captionLogicalTop));
  if (view()->layoutState()->isPaginated())
    updateFragmentationInfoForChild(caption);

  if (!selfNeedsLayout())
    caption.setMayNeedPaintInvalidation();

  setLogicalHeight(logicalHeight() + caption.logicalHeight() +
                   collapsedMarginBeforeForChild(caption) +
                   collapsedMarginAfterForChild(caption));
}

void LayoutTable::layoutSection(LayoutTableSection& section,
                                SubtreeLayoutScope& layouter,
                                LayoutUnit logicalLeft) {
  section.setLogicalLocation(LayoutPoint(logicalLeft, logicalHeight()));
  if (m_columnLogicalWidthChanged)
    layouter.setChildNeedsLayout(&section);
  if (!section.needsLayout())
    markChildForPaginationRelayoutIfNeeded(section, layouter);
  section.layoutIfNeeded();
  int sectionLogicalHeight = section.calcRowLogicalHeight();
  section.setLogicalHeight(LayoutUnit(sectionLogicalHeight));
  if (view()->layoutState()->isPaginated())
    updateFragmentationInfoForChild(section);
  setLogicalHeight(logicalHeight() + sectionLogicalHeight);
}

LayoutUnit LayoutTable::logicalHeightFromStyle() const {
  LayoutUnit computedLogicalHeight;
  Length logicalHeightLength = style()->logicalHeight();
  if (logicalHeightLength.isIntrinsic() ||
      (logicalHeightLength.isSpecified() && logicalHeightLength.isPositive())) {
    computedLogicalHeight =
        convertStyleLogicalHeightToComputedHeight(logicalHeightLength);
  }

  Length logicalMaxHeightLength = style()->logicalMaxHeight();
  if (logicalMaxHeightLength.isIntrinsic() ||
      (logicalMaxHeightLength.isSpecified() &&
       !logicalMaxHeightLength.isNegative())) {
    LayoutUnit computedMaxLogicalHeight =
        convertStyleLogicalHeightToComputedHeight(logicalMaxHeightLength);
    computedLogicalHeight =
        std::min(computedLogicalHeight, computedMaxLogicalHeight);
  }

  Length logicalMinHeightLength = style()->logicalMinHeight();
  if (logicalMinHeightLength.isIntrinsic() ||
      (logicalMinHeightLength.isSpecified() &&
       !logicalMinHeightLength.isNegative())) {
    LayoutUnit computedMinLogicalHeight =
        convertStyleLogicalHeightToComputedHeight(logicalMinHeightLength);
    computedLogicalHeight =
        std::max(computedLogicalHeight, computedMinLogicalHeight);
  }

  return computedLogicalHeight;
}

void LayoutTable::distributeExtraLogicalHeight(int extraLogicalHeight) {
  if (extraLogicalHeight <= 0)
    return;

  // FIXME: Distribute the extra logical height between all table sections
  // instead of giving it all to the first one.
  if (LayoutTableSection* section = firstBody())
    extraLogicalHeight -=
        section->distributeExtraLogicalHeightToRows(extraLogicalHeight);

  // FIXME: We really would like to enable this ASSERT to ensure that all the
  // extra space has been distributed.
  // However our current distribution algorithm does not round properly and thus
  // we can have some remaining height.
  // ASSERT(!topSection() || !extraLogicalHeight);
}

void LayoutTable::simplifiedNormalFlowLayout() {
  // FIXME: We should walk through the items in the tree in tree order to do the
  // layout here instead of walking through individual parts of the tree.
  // crbug.com/442737
  for (auto& caption : m_captions)
    caption->layoutIfNeeded();

  for (LayoutTableSection* section = topSection(); section;
       section = sectionBelow(section)) {
    section->layoutIfNeeded();
    section->layoutRows();
    section->computeOverflowFromCells();
    section->updateLayerTransformAfterLayout();
    section->addVisualEffectOverflow();
  }
}

bool LayoutTable::recalcChildOverflowAfterStyleChange() {
  ASSERT(childNeedsOverflowRecalcAfterStyleChange());
  clearChildNeedsOverflowRecalcAfterStyleChange();

  // If the table sections we keep pointers to have gone away then the table
  // will be rebuilt and overflow will get recalculated anyway so return early.
  if (needsSectionRecalc())
    return false;

  bool childrenOverflowChanged = false;
  for (LayoutTableSection* section = topSection(); section;
       section = sectionBelow(section)) {
    if (!section->childNeedsOverflowRecalcAfterStyleChange())
      continue;
    childrenOverflowChanged = section->recalcChildOverflowAfterStyleChange() ||
                              childrenOverflowChanged;
  }
  return recalcPositionedDescendantsOverflowAfterStyleChange() ||
         childrenOverflowChanged;
}

void LayoutTable::layout() {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  if (simplifiedLayout())
    return;

  // Note: LayoutTable is handled differently than other LayoutBlocks and the
  // LayoutScope
  //       must be created before the table begins laying out.
  TextAutosizer::LayoutScope textAutosizerLayoutScope(this);

  recalcSectionsIfNeeded();
  // FIXME: We should do this recalc lazily in borderStart/borderEnd so that we
  // don't have to make sure to call this before we call borderStart/borderEnd
  // to avoid getting a stale value.
  recalcBordersInRowDirection();

  SubtreeLayoutScope layouter(*this);

  {
    LayoutState state(*this);
    LayoutUnit oldLogicalWidth = logicalWidth();
    LayoutUnit oldLogicalHeight = logicalHeight();

    setLogicalHeight(LayoutUnit());
    updateLogicalWidth();

    if (logicalWidth() != oldLogicalWidth) {
      for (unsigned i = 0; i < m_captions.size(); i++)
        layouter.setNeedsLayout(m_captions[i],
                                LayoutInvalidationReason::TableChanged);
    }
    // FIXME: The optimisation below doesn't work since the internal table
    // layout could have changed. We need to add a flag to the table
    // layout that tells us if something has changed in the min max
    // calculations to do it correctly.
    // if ( oldWidth != width() || columns.size() + 1 != columnPos.size() )
    m_tableLayout->layout();

    // Lay out top captions.
    // FIXME: Collapse caption margin.
    for (unsigned i = 0; i < m_captions.size(); i++) {
      if (m_captions[i]->style()->captionSide() == ECaptionSide::kBottom)
        continue;
      layoutCaption(*m_captions[i], layouter);
    }

    LayoutTableSection* topSection = this->topSection();
    LayoutTableSection* bottomSection = this->bottomSection();

    // This is the border-before edge of the "table box", relative to the "table
    // wrapper box", i.e. right after all top captions.
    // https://www.w3.org/TR/2011/REC-CSS2-20110607/tables.html#model
    LayoutUnit tableBoxLogicalTop = logicalHeight();

    bool collapsing = collapseBorders();
    if (collapsing) {
      // Need to set up the table borders before we can position the sections.
      for (LayoutTableSection* section = topSection; section;
           section = sectionBelow(section))
        section->recalcOuterBorder();
    }

    LayoutUnit borderAndPaddingBefore =
        borderBefore() + (collapsing ? LayoutUnit() : paddingBefore());
    LayoutUnit borderAndPaddingAfter =
        borderAfter() + (collapsing ? LayoutUnit() : paddingAfter());

    setLogicalHeight(tableBoxLogicalTop + borderAndPaddingBefore);

    LayoutUnit sectionLogicalLeft = LayoutUnit(
        style()->isLeftToRightDirection() ? borderStart() : borderEnd());
    if (!collapsing) {
      sectionLogicalLeft +=
          style()->isLeftToRightDirection() ? paddingStart() : paddingEnd();
    }

    // Lay out table header group.
    if (LayoutTableSection* section = header()) {
      layoutSection(*section, layouter, sectionLogicalLeft);
      if (state.isPaginated()) {
        // If the repeating header group allows at least one row of content,
        // then store the offset for other sections to offset their rows
        // against.
        LayoutUnit sectionLogicalHeight = section->logicalHeight();
        if (sectionLogicalHeight <
                section->pageLogicalHeightForOffset(section->logicalTop()) &&
            section->getPaginationBreakability() != AllowAnyBreaks) {
          // Don't include any strut in the header group - we only want the
          // height from its content.
          LayoutUnit offsetForTableHeaders = sectionLogicalHeight;
          if (LayoutTableRow* row = section->firstRow())
            offsetForTableHeaders -= row->paginationStrut();
          setRowOffsetFromRepeatingHeader(offsetForTableHeaders);
        }
      }
    }

    // Lay out table body groups, and column groups.
    for (LayoutObject* child = firstChild(); child;
         child = child->nextSibling()) {
      if (child->isTableSection()) {
        if (child != header() && child != footer()) {
          LayoutTableSection& section = *toLayoutTableSection(child);
          layoutSection(section, layouter, sectionLogicalLeft);
        }
      } else if (child->isLayoutTableCol()) {
        child->layoutIfNeeded();
      } else {
        DCHECK(child->isTableCaption());
      }
    }

    // Lay out table footer.
    if (LayoutTableSection* section = footer())
      layoutSection(*section, layouter, sectionLogicalLeft);

    setLogicalHeight(tableBoxLogicalTop + borderAndPaddingBefore);

    LayoutUnit computedLogicalHeight = logicalHeightFromStyle();
    LayoutUnit totalSectionLogicalHeight;
    if (topSection) {
      totalSectionLogicalHeight =
          bottomSection->logicalBottom() - topSection->logicalTop();
    }

    if (!state.isPaginated() ||
        !crossesPageBoundary(tableBoxLogicalTop, computedLogicalHeight)) {
      distributeExtraLogicalHeight(
          floorToInt(computedLogicalHeight - totalSectionLogicalHeight));
    }

    LayoutUnit logicalOffset =
        topSection ? topSection->logicalTop() : LayoutUnit();
    for (LayoutTableSection* section = topSection; section;
         section = sectionBelow(section)) {
      section->setLogicalTop(logicalOffset);
      section->layoutRows();
      logicalOffset += section->logicalHeight();
    }

    if (!topSection && computedLogicalHeight > totalSectionLogicalHeight &&
        !document().inQuirksMode()) {
      // Completely empty tables (with no sections or anything) should at least
      // honor specified height in strict mode.
      setLogicalHeight(logicalHeight() + computedLogicalHeight);
    }

    // position the table sections
    LayoutTableSection* section = topSection;
    while (section) {
      section->setLogicalLocation(
          LayoutPoint(sectionLogicalLeft, logicalHeight()));

      setLogicalHeight(logicalHeight() + section->logicalHeight());

      section->updateLayerTransformAfterLayout();
      section->addVisualEffectOverflow();

      section = sectionBelow(section);
    }

    setLogicalHeight(logicalHeight() + borderAndPaddingAfter);

    // Lay out bottom captions.
    for (unsigned i = 0; i < m_captions.size(); i++) {
      if (m_captions[i]->style()->captionSide() != ECaptionSide::kBottom)
        continue;
      layoutCaption(*m_captions[i], layouter);
    }

    updateLogicalHeight();

    // table can be containing block of positioned elements.
    bool dimensionChanged = oldLogicalWidth != logicalWidth() ||
                            oldLogicalHeight != logicalHeight();
    layoutPositionedObjects(dimensionChanged);

    updateLayerTransformAfterLayout();

    // Layout was changed, so probably borders too.
    invalidateCollapsedBorders();

    computeOverflow(clientLogicalBottom());
    updateAfterLayout();

    if (state.isPaginated() && isPageLogicalHeightKnown()) {
      m_blockOffsetToFirstRepeatableHeader = state.pageLogicalOffset(
          *this, topSection ? topSection->logicalTop() : LayoutUnit());
    }
  }

  // FIXME: This value isn't the intrinsic content logical height, but we need
  // to update the value as its used by flexbox layout. crbug.com/367324
  setIntrinsicContentLogicalHeight(contentLogicalHeight());

  m_columnLogicalWidthChanged = false;
  clearNeedsLayout();
}

void LayoutTable::invalidateCollapsedBorders() {
  m_collapsedBorders.clear();
  if (!collapseBorders())
    return;

  m_collapsedBordersValid = false;
  setMayNeedPaintInvalidation();
}

// Collect all the unique border values that we want to paint in a sorted list.
// During the collection, each cell saves its recalculated borders into the
// cache of its containing section, and invalidates itself if any border
// changes. This method doesn't affect layout.
void LayoutTable::recalcCollapsedBordersIfNeeded() {
  if (m_collapsedBordersValid || !collapseBorders())
    return;
  m_collapsedBordersValid = true;
  m_collapsedBorders.clear();
  for (LayoutObject* section = firstChild(); section;
       section = section->nextSibling()) {
    if (!section->isTableSection())
      continue;
    for (LayoutTableRow* row = toLayoutTableSection(section)->firstRow(); row;
         row = row->nextRow()) {
      for (LayoutTableCell* cell = row->firstCell(); cell;
           cell = cell->nextCell()) {
        ASSERT(cell->table() == this);
        cell->collectBorderValues(m_collapsedBorders);
      }
    }
  }
  LayoutTableCell::sortBorderValues(m_collapsedBorders);
}

void LayoutTable::addOverflowFromChildren() {
  // Add overflow from borders.
  // Technically it's odd that we are incorporating the borders into layout
  // overflow, which is only supposed to be about overflow from our
  // descendant objects, but since tables don't support overflow:auto, this
  // works out fine.
  if (collapseBorders()) {
    int rightBorderOverflow =
        (size().width() + outerBorderRight() - borderRight()).toInt();
    int leftBorderOverflow = borderLeft() - outerBorderLeft();
    int bottomBorderOverflow =
        (size().height() + outerBorderBottom() - borderBottom()).toInt();
    int topBorderOverflow = borderTop() - outerBorderTop();
    IntRect borderOverflowRect(leftBorderOverflow, topBorderOverflow,
                               rightBorderOverflow - leftBorderOverflow,
                               bottomBorderOverflow - topBorderOverflow);
    if (borderOverflowRect != pixelSnappedBorderBoxRect()) {
      LayoutRect borderLayoutRect(borderOverflowRect);
      addLayoutOverflow(borderLayoutRect);
      addContentsVisualOverflow(borderLayoutRect);
    }
  }

  // Add overflow from our caption.
  for (unsigned i = 0; i < m_captions.size(); i++)
    addOverflowFromChild(m_captions[i]);

  // Add overflow from our sections.
  for (LayoutTableSection* section = topSection(); section;
       section = sectionBelow(section))
    addOverflowFromChild(section);
}

void LayoutTable::paintObject(const PaintInfo& paintInfo,
                              const LayoutPoint& paintOffset) const {
  TablePainter(*this).paintObject(paintInfo, paintOffset);
}

void LayoutTable::subtractCaptionRect(LayoutRect& rect) const {
  for (unsigned i = 0; i < m_captions.size(); i++) {
    LayoutUnit captionLogicalHeight = m_captions[i]->logicalHeight() +
                                      m_captions[i]->marginBefore() +
                                      m_captions[i]->marginAfter();
    bool captionIsBefore =
        (m_captions[i]->style()->captionSide() != ECaptionSide::kBottom) ^
        style()->isFlippedBlocksWritingMode();
    if (style()->isHorizontalWritingMode()) {
      rect.setHeight(rect.height() - captionLogicalHeight);
      if (captionIsBefore)
        rect.move(LayoutUnit(), captionLogicalHeight);
    } else {
      rect.setWidth(rect.width() - captionLogicalHeight);
      if (captionIsBefore)
        rect.move(captionLogicalHeight, LayoutUnit());
    }
  }
}

void LayoutTable::markAllCellsWidthsDirtyAndOrNeedsLayout(
    WhatToMarkAllCells whatToMark) {
  for (LayoutObject* child = children()->firstChild(); child;
       child = child->nextSibling()) {
    if (!child->isTableSection())
      continue;
    LayoutTableSection* section = toLayoutTableSection(child);
    section->markAllCellsWidthsDirtyAndOrNeedsLayout(whatToMark);
  }
}

void LayoutTable::paintBoxDecorationBackground(
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset) const {
  TablePainter(*this).paintBoxDecorationBackground(paintInfo, paintOffset);
}

void LayoutTable::paintMask(const PaintInfo& paintInfo,
                            const LayoutPoint& paintOffset) const {
  TablePainter(*this).paintMask(paintInfo, paintOffset);
}

void LayoutTable::computeIntrinsicLogicalWidths(LayoutUnit& minWidth,
                                                LayoutUnit& maxWidth) const {
  recalcSectionsIfNeeded();
  // FIXME: Do the recalc in borderStart/borderEnd and make those const_cast
  // this call.
  // Then m_borderStart/m_borderEnd will be transparent a cache and it removes
  // the possibility of reading out stale values.
  const_cast<LayoutTable*>(this)->recalcBordersInRowDirection();
  // FIXME: Restructure the table layout code so that we can make this method
  // const.
  const_cast<LayoutTable*>(this)->m_tableLayout->computeIntrinsicLogicalWidths(
      minWidth, maxWidth);

  // FIXME: We should include captions widths here like we do in
  // computePreferredLogicalWidths.
}

void LayoutTable::computePreferredLogicalWidths() {
  ASSERT(preferredLogicalWidthsDirty());

  computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth,
                                m_maxPreferredLogicalWidth);

  int bordersPaddingAndSpacing =
      bordersPaddingAndSpacingInRowDirection().toInt();
  m_minPreferredLogicalWidth += bordersPaddingAndSpacing;
  m_maxPreferredLogicalWidth += bordersPaddingAndSpacing;

  m_tableLayout->applyPreferredLogicalWidthQuirks(m_minPreferredLogicalWidth,
                                                  m_maxPreferredLogicalWidth);

  for (unsigned i = 0; i < m_captions.size(); i++)
    m_minPreferredLogicalWidth = std::max(
        m_minPreferredLogicalWidth, m_captions[i]->minPreferredLogicalWidth());

  const ComputedStyle& styleToUse = styleRef();
  // FIXME: This should probably be checking for isSpecified since you should be
  // able to use percentage or calc values for min-width.
  if (styleToUse.logicalMinWidth().isFixed() &&
      styleToUse.logicalMinWidth().value() > 0) {
    m_maxPreferredLogicalWidth = std::max(
        m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(
                                        styleToUse.logicalMinWidth().value()));
    m_minPreferredLogicalWidth = std::max(
        m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(
                                        styleToUse.logicalMinWidth().value()));
  }

  // FIXME: This should probably be checking for isSpecified since you should be
  // able to use percentage or calc values for maxWidth.
  if (styleToUse.logicalMaxWidth().isFixed()) {
    // We don't constrain m_minPreferredLogicalWidth as the table should be at
    // least the size of its min-content, regardless of 'max-width'.
    m_maxPreferredLogicalWidth = std::min(
        m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(
                                        styleToUse.logicalMaxWidth().value()));
    m_maxPreferredLogicalWidth =
        std::max(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
  }

  // FIXME: We should be adding borderAndPaddingLogicalWidth here, but
  // m_tableLayout->computePreferredLogicalWidths already does, so a bunch of
  // tests break doing this naively.
  clearPreferredLogicalWidthsDirty();
}

LayoutTableSection* LayoutTable::topNonEmptySection() const {
  LayoutTableSection* section = topSection();
  if (section && !section->numRows())
    section = sectionBelow(section, SkipEmptySections);
  return section;
}

void LayoutTable::splitEffectiveColumn(unsigned index, unsigned firstSpan) {
  // We split the column at |index|, taking |firstSpan| cells from the span.
  ASSERT(m_effectiveColumns[index].span > firstSpan);
  m_effectiveColumns.insert(index, firstSpan);
  m_effectiveColumns[index + 1].span -= firstSpan;

  // Propagate the change in our columns representation to the sections that
  // don't need cell recalc. If they do, they will be synced up directly with
  // m_columns later.
  for (LayoutObject* child = firstChild(); child;
       child = child->nextSibling()) {
    if (!child->isTableSection())
      continue;

    LayoutTableSection* section = toLayoutTableSection(child);
    if (section->needsCellRecalc())
      continue;

    section->splitEffectiveColumn(index, firstSpan);
  }

  m_effectiveColumnPositions.grow(numEffectiveColumns() + 1);
}

void LayoutTable::appendEffectiveColumn(unsigned span) {
  unsigned newColumnIndex = m_effectiveColumns.size();
  m_effectiveColumns.push_back(span);

  // Unless the table has cell(s) with colspan that exceed the number of columns
  // afforded by the other rows in the table we can use the fast path when
  // mapping columns to effective columns.
  if (span == 1 && m_noCellColspanAtLeast + 1 == numEffectiveColumns()) {
    m_noCellColspanAtLeast++;
  }

  // Propagate the change in our columns representation to the sections that
  // don't need cell recalc. If they do, they will be synced up directly with
  // m_columns later.
  for (LayoutObject* child = firstChild(); child;
       child = child->nextSibling()) {
    if (!child->isTableSection())
      continue;

    LayoutTableSection* section = toLayoutTableSection(child);
    if (section->needsCellRecalc())
      continue;

    section->appendEffectiveColumn(newColumnIndex);
  }

  m_effectiveColumnPositions.grow(numEffectiveColumns() + 1);
}

LayoutTableCol* LayoutTable::firstColumn() const {
  for (LayoutObject* child = firstChild(); child;
       child = child->nextSibling()) {
    if (child->isLayoutTableCol())
      return toLayoutTableCol(child);
  }

  return nullptr;
}

void LayoutTable::updateColumnCache() const {
  ASSERT(m_hasColElements);
  ASSERT(m_columnLayoutObjects.isEmpty());
  ASSERT(!m_columnLayoutObjectsValid);

  for (LayoutTableCol* columnLayoutObject = firstColumn(); columnLayoutObject;
       columnLayoutObject = columnLayoutObject->nextColumn()) {
    if (columnLayoutObject->isTableColumnGroupWithColumnChildren())
      continue;
    m_columnLayoutObjects.push_back(columnLayoutObject);
  }
  m_columnLayoutObjectsValid = true;
}

LayoutTable::ColAndColGroup LayoutTable::slowColElementAtAbsoluteColumn(
    unsigned absoluteColumnIndex) const {
  ASSERT(m_hasColElements);

  if (!m_columnLayoutObjectsValid)
    updateColumnCache();

  unsigned columnCount = 0;
  for (unsigned i = 0; i < m_columnLayoutObjects.size(); i++) {
    LayoutTableCol* columnLayoutObject = m_columnLayoutObjects[i];
    ASSERT(!columnLayoutObject->isTableColumnGroupWithColumnChildren());
    unsigned span = columnLayoutObject->span();
    unsigned startCol = columnCount;
    ASSERT(span >= 1);
    unsigned endCol = columnCount + span - 1;
    columnCount += span;
    if (columnCount > absoluteColumnIndex) {
      ColAndColGroup colAndColGroup;
      bool isAtStartEdge = startCol == absoluteColumnIndex;
      bool isAtEndEdge = endCol == absoluteColumnIndex;
      if (columnLayoutObject->isTableColumnGroup()) {
        colAndColGroup.colgroup = columnLayoutObject;
        colAndColGroup.adjoinsStartBorderOfColGroup = isAtStartEdge;
        colAndColGroup.adjoinsEndBorderOfColGroup = isAtEndEdge;
      } else {
        colAndColGroup.col = columnLayoutObject;
        colAndColGroup.colgroup = columnLayoutObject->enclosingColumnGroup();
        if (colAndColGroup.colgroup) {
          colAndColGroup.adjoinsStartBorderOfColGroup =
              isAtStartEdge && !colAndColGroup.col->previousSibling();
          colAndColGroup.adjoinsEndBorderOfColGroup =
              isAtEndEdge && !colAndColGroup.col->nextSibling();
        }
      }
      return colAndColGroup;
    }
  }
  return ColAndColGroup();
}

void LayoutTable::recalcSections() const {
  ASSERT(m_needsSectionRecalc);

  m_head = nullptr;
  m_foot = nullptr;
  m_firstBody = nullptr;
  m_hasColElements = false;

  // We need to get valid pointers to caption, head, foot and first body again
  LayoutObject* nextSibling;
  for (LayoutObject* child = firstChild(); child; child = nextSibling) {
    nextSibling = child->nextSibling();
    switch (child->style()->display()) {
      case EDisplay::TableColumn:
      case EDisplay::TableColumnGroup:
        m_hasColElements = true;
        break;
      case EDisplay::TableHeaderGroup:
        if (child->isTableSection()) {
          LayoutTableSection* section = toLayoutTableSection(child);
          if (!m_head)
            m_head = section;
          else if (!m_firstBody)
            m_firstBody = section;
          section->recalcCellsIfNeeded();
        }
        break;
      case EDisplay::TableFooterGroup:
        if (child->isTableSection()) {
          LayoutTableSection* section = toLayoutTableSection(child);
          if (!m_foot)
            m_foot = section;
          else if (!m_firstBody)
            m_firstBody = section;
          section->recalcCellsIfNeeded();
        }
        break;
      case EDisplay::TableRowGroup:
        if (child->isTableSection()) {
          LayoutTableSection* section = toLayoutTableSection(child);
          if (!m_firstBody)
            m_firstBody = section;
          section->recalcCellsIfNeeded();
        }
        break;
      default:
        break;
    }
  }

  // repair column count (addChild can grow it too much, because it always adds
  // elements to the last row of a section)
  unsigned maxCols = 0;
  for (LayoutObject* child = firstChild(); child;
       child = child->nextSibling()) {
    if (child->isTableSection()) {
      LayoutTableSection* section = toLayoutTableSection(child);
      unsigned sectionCols = section->numEffectiveColumns();
      if (sectionCols > maxCols)
        maxCols = sectionCols;
    }
  }

  m_effectiveColumns.resize(maxCols);
  m_effectiveColumnPositions.resize(maxCols + 1);
  m_noCellColspanAtLeast = calcNoCellColspanAtLeast();

  ASSERT(selfNeedsLayout());

  m_needsSectionRecalc = false;
}

int LayoutTable::calcBorderStart() const {
  if (!collapseBorders())
    return LayoutBlock::borderStart();

  // Determined by the first cell of the first row. See the CSS 2.1 spec,
  // section 17.6.2.
  if (!numEffectiveColumns())
    return 0;

  int borderWidth = 0;

  const BorderValue& tableStartBorder = style()->borderStart();
  if (tableStartBorder.style() == BorderStyleHidden)
    return 0;
  if (tableStartBorder.style() > BorderStyleHidden)
    borderWidth = tableStartBorder.width();

  // TODO(dgrogan): This logic doesn't properly account for the first column in
  // the first column-group case.
  if (LayoutTableCol* column =
          colElementAtAbsoluteColumn(0).innermostColOrColGroup()) {
    // FIXME: We don't account for direction on columns and column groups.
    const BorderValue& columnAdjoiningBorder = column->style()->borderStart();
    if (columnAdjoiningBorder.style() == BorderStyleHidden)
      return 0;
    if (columnAdjoiningBorder.style() > BorderStyleHidden)
      borderWidth = std::max(borderWidth, columnAdjoiningBorder.width());
  }

  if (const LayoutTableSection* topNonEmptySection =
          this->topNonEmptySection()) {
    const BorderValue& sectionAdjoiningBorder =
        topNonEmptySection->borderAdjoiningTableStart();
    if (sectionAdjoiningBorder.style() == BorderStyleHidden)
      return 0;

    if (sectionAdjoiningBorder.style() > BorderStyleHidden)
      borderWidth = std::max(borderWidth, sectionAdjoiningBorder.width());

    if (const LayoutTableCell* adjoiningStartCell =
            topNonEmptySection->firstRowCellAdjoiningTableStart()) {
      // FIXME: Make this work with perpendicular and flipped cells.
      const BorderValue& startCellAdjoiningBorder =
          adjoiningStartCell->borderAdjoiningTableStart();
      if (startCellAdjoiningBorder.style() == BorderStyleHidden)
        return 0;

      const BorderValue& firstRowAdjoiningBorder =
          adjoiningStartCell->row()->borderAdjoiningTableStart();
      if (firstRowAdjoiningBorder.style() == BorderStyleHidden)
        return 0;

      if (startCellAdjoiningBorder.style() > BorderStyleHidden)
        borderWidth = std::max(borderWidth, startCellAdjoiningBorder.width());
      if (firstRowAdjoiningBorder.style() > BorderStyleHidden)
        borderWidth = std::max(borderWidth, firstRowAdjoiningBorder.width());
    }
  }
  return (borderWidth + (style()->isLeftToRightDirection() ? 0 : 1)) / 2;
}

int LayoutTable::calcBorderEnd() const {
  if (!collapseBorders())
    return LayoutBlock::borderEnd();

  // Determined by the last cell of the first row. See the CSS 2.1 spec, section
  // 17.6.2.
  if (!numEffectiveColumns())
    return 0;

  int borderWidth = 0;

  const BorderValue& tableEndBorder = style()->borderEnd();
  if (tableEndBorder.style() == BorderStyleHidden)
    return 0;
  if (tableEndBorder.style() > BorderStyleHidden)
    borderWidth = tableEndBorder.width();

  unsigned endColumn = numEffectiveColumns() - 1;

  // TODO(dgrogan): This logic doesn't properly account for the last column in
  // the last column-group case.
  if (LayoutTableCol* column =
          colElementAtAbsoluteColumn(endColumn).innermostColOrColGroup()) {
    // FIXME: We don't account for direction on columns and column groups.
    const BorderValue& columnAdjoiningBorder = column->style()->borderEnd();
    if (columnAdjoiningBorder.style() == BorderStyleHidden)
      return 0;
    if (columnAdjoiningBorder.style() > BorderStyleHidden)
      borderWidth = std::max(borderWidth, columnAdjoiningBorder.width());
  }

  if (const LayoutTableSection* topNonEmptySection =
          this->topNonEmptySection()) {
    const BorderValue& sectionAdjoiningBorder =
        topNonEmptySection->borderAdjoiningTableEnd();
    if (sectionAdjoiningBorder.style() == BorderStyleHidden)
      return 0;

    if (sectionAdjoiningBorder.style() > BorderStyleHidden)
      borderWidth = std::max(borderWidth, sectionAdjoiningBorder.width());

    if (const LayoutTableCell* adjoiningEndCell =
            topNonEmptySection->firstRowCellAdjoiningTableEnd()) {
      // FIXME: Make this work with perpendicular and flipped cells.
      const BorderValue& endCellAdjoiningBorder =
          adjoiningEndCell->borderAdjoiningTableEnd();
      if (endCellAdjoiningBorder.style() == BorderStyleHidden)
        return 0;

      const BorderValue& firstRowAdjoiningBorder =
          adjoiningEndCell->row()->borderAdjoiningTableEnd();
      if (firstRowAdjoiningBorder.style() == BorderStyleHidden)
        return 0;

      if (endCellAdjoiningBorder.style() > BorderStyleHidden)
        borderWidth = std::max(borderWidth, endCellAdjoiningBorder.width());
      if (firstRowAdjoiningBorder.style() > BorderStyleHidden)
        borderWidth = std::max(borderWidth, firstRowAdjoiningBorder.width());
    }
  }
  return (borderWidth + (style()->isLeftToRightDirection() ? 1 : 0)) / 2;
}

void LayoutTable::recalcBordersInRowDirection() {
  // FIXME: We need to compute the collapsed before / after borders in the same
  // fashion.
  m_borderStart = calcBorderStart();
  m_borderEnd = calcBorderEnd();
}

int LayoutTable::borderBefore() const {
  if (collapseBorders()) {
    recalcSectionsIfNeeded();
    return outerBorderBefore();
  }
  return LayoutBlock::borderBefore();
}

int LayoutTable::borderAfter() const {
  if (collapseBorders()) {
    recalcSectionsIfNeeded();
    return outerBorderAfter();
  }
  return LayoutBlock::borderAfter();
}

int LayoutTable::outerBorderBefore() const {
  if (!collapseBorders())
    return 0;
  int borderWidth = 0;
  if (LayoutTableSection* topSection = this->topSection()) {
    borderWidth = topSection->outerBorderBefore();
    if (borderWidth < 0)
      return 0;  // Overridden by hidden
  }
  const BorderValue& tb = style()->borderBefore();
  if (tb.style() == BorderStyleHidden)
    return 0;
  if (tb.style() > BorderStyleHidden)
    borderWidth = std::max<int>(borderWidth, tb.width() / 2);
  return borderWidth;
}

int LayoutTable::outerBorderAfter() const {
  if (!collapseBorders())
    return 0;
  int borderWidth = 0;

  if (LayoutTableSection* section = bottomSection()) {
    borderWidth = section->outerBorderAfter();
    if (borderWidth < 0)
      return 0;  // Overridden by hidden
  }
  const BorderValue& tb = style()->borderAfter();
  if (tb.style() == BorderStyleHidden)
    return 0;
  if (tb.style() > BorderStyleHidden)
    borderWidth = std::max<int>(borderWidth, (tb.width() + 1) / 2);
  return borderWidth;
}

int LayoutTable::outerBorderStart() const {
  if (!collapseBorders())
    return 0;

  int borderWidth = 0;

  const BorderValue& tb = style()->borderStart();
  if (tb.style() == BorderStyleHidden)
    return 0;
  if (tb.style() > BorderStyleHidden)
    borderWidth =
        (tb.width() + (style()->isLeftToRightDirection() ? 0 : 1)) / 2;

  bool allHidden = true;
  for (LayoutTableSection* section = topSection(); section;
       section = sectionBelow(section)) {
    int sw = section->outerBorderStart();
    if (sw < 0)
      continue;
    allHidden = false;
    borderWidth = std::max(borderWidth, sw);
  }
  if (allHidden)
    return 0;

  return borderWidth;
}

int LayoutTable::outerBorderEnd() const {
  if (!collapseBorders())
    return 0;

  int borderWidth = 0;

  const BorderValue& tb = style()->borderEnd();
  if (tb.style() == BorderStyleHidden)
    return 0;
  if (tb.style() > BorderStyleHidden)
    borderWidth =
        (tb.width() + (style()->isLeftToRightDirection() ? 1 : 0)) / 2;

  bool allHidden = true;
  for (LayoutTableSection* section = topSection(); section;
       section = sectionBelow(section)) {
    int sw = section->outerBorderEnd();
    if (sw < 0)
      continue;
    allHidden = false;
    borderWidth = std::max(borderWidth, sw);
  }
  if (allHidden)
    return 0;

  return borderWidth;
}

LayoutTableSection* LayoutTable::sectionAbove(
    const LayoutTableSection* section,
    SkipEmptySectionsValue skipEmptySections) const {
  recalcSectionsIfNeeded();

  if (section == m_head)
    return 0;

  LayoutObject* prevSection =
      section == m_foot ? lastChild() : section->previousSibling();
  while (prevSection) {
    if (prevSection->isTableSection() && prevSection != m_head &&
        prevSection != m_foot && (skipEmptySections == DoNotSkipEmptySections ||
                                  toLayoutTableSection(prevSection)->numRows()))
      break;
    prevSection = prevSection->previousSibling();
  }
  if (!prevSection && m_head &&
      (skipEmptySections == DoNotSkipEmptySections || m_head->numRows()))
    prevSection = m_head;
  return toLayoutTableSection(prevSection);
}

LayoutTableSection* LayoutTable::sectionBelow(
    const LayoutTableSection* section,
    SkipEmptySectionsValue skipEmptySections) const {
  recalcSectionsIfNeeded();

  if (section == m_foot)
    return nullptr;

  LayoutObject* nextSection =
      section == m_head ? firstChild() : section->nextSibling();
  while (nextSection) {
    if (nextSection->isTableSection() && nextSection != m_head &&
        nextSection != m_foot && (skipEmptySections == DoNotSkipEmptySections ||
                                  toLayoutTableSection(nextSection)->numRows()))
      break;
    nextSection = nextSection->nextSibling();
  }
  if (!nextSection && m_foot &&
      (skipEmptySections == DoNotSkipEmptySections || m_foot->numRows()))
    nextSection = m_foot;
  return toLayoutTableSection(nextSection);
}

LayoutTableSection* LayoutTable::bottomSection() const {
  recalcSectionsIfNeeded();

  if (m_foot)
    return m_foot;

  for (LayoutObject* child = lastChild(); child;
       child = child->previousSibling()) {
    if (child->isTableSection())
      return toLayoutTableSection(child);
  }

  return nullptr;
}

LayoutTableCell* LayoutTable::cellAbove(const LayoutTableCell* cell) const {
  recalcSectionsIfNeeded();

  // Find the section and row to look in
  unsigned r = cell->rowIndex();
  LayoutTableSection* section = nullptr;
  unsigned rAbove = 0;
  if (r > 0) {
    // cell is not in the first row, so use the above row in its own section
    section = cell->section();
    rAbove = r - 1;
  } else {
    section = sectionAbove(cell->section(), SkipEmptySections);
    if (section) {
      ASSERT(section->numRows());
      rAbove = section->numRows() - 1;
    }
  }

  // Look up the cell in the section's grid, which requires effective col index
  if (section) {
    unsigned effCol =
        absoluteColumnToEffectiveColumn(cell->absoluteColumnIndex());
    return section->primaryCellAt(rAbove, effCol);
  }
  return nullptr;
}

LayoutTableCell* LayoutTable::cellBelow(const LayoutTableCell* cell) const {
  recalcSectionsIfNeeded();

  // Find the section and row to look in
  unsigned r = cell->rowIndex() + cell->rowSpan() - 1;
  LayoutTableSection* section = nullptr;
  unsigned rBelow = 0;
  if (r < cell->section()->numRows() - 1) {
    // The cell is not in the last row, so use the next row in the section.
    section = cell->section();
    rBelow = r + 1;
  } else {
    section = sectionBelow(cell->section(), SkipEmptySections);
    if (section)
      rBelow = 0;
  }

  // Look up the cell in the section's grid, which requires effective col index
  if (section) {
    unsigned effCol =
        absoluteColumnToEffectiveColumn(cell->absoluteColumnIndex());
    return section->primaryCellAt(rBelow, effCol);
  }
  return nullptr;
}

LayoutTableCell* LayoutTable::cellBefore(const LayoutTableCell* cell) const {
  recalcSectionsIfNeeded();

  LayoutTableSection* section = cell->section();
  unsigned effCol =
      absoluteColumnToEffectiveColumn(cell->absoluteColumnIndex());
  if (!effCol)
    return nullptr;

  // If we hit a colspan back up to a real cell.
  LayoutTableSection::CellStruct& prevCell =
      section->cellAt(cell->rowIndex(), effCol - 1);
  return prevCell.primaryCell();
}

LayoutTableCell* LayoutTable::cellAfter(const LayoutTableCell* cell) const {
  recalcSectionsIfNeeded();

  unsigned effCol = absoluteColumnToEffectiveColumn(
      cell->absoluteColumnIndex() + cell->colSpan());
  return cell->section()->primaryCellAt(cell->rowIndex(), effCol);
}

int LayoutTable::baselinePosition(FontBaseline baselineType,
                                  bool firstLine,
                                  LineDirectionMode direction,
                                  LinePositionMode linePositionMode) const {
  ASSERT(linePositionMode == PositionOnContainingLine);
  int baseline = firstLineBoxBaseline();
  if (baseline != -1) {
    if (isInline())
      return beforeMarginInLineDirection(direction) + baseline;
    return baseline;
  }

  return LayoutBox::baselinePosition(baselineType, firstLine, direction,
                                     linePositionMode);
}

int LayoutTable::inlineBlockBaseline(LineDirectionMode) const {
  // Tables are skipped when computing an inline-block's baseline.
  return -1;
}

int LayoutTable::firstLineBoxBaseline() const {
  // The baseline of a 'table' is the same as the 'inline-table' baseline per
  // CSS 3 Flexbox (CSS 2.1 doesn't define the baseline of a 'table' only an
  // 'inline-table'). This is also needed to properly determine the baseline of
  // a cell if it has a table child.

  if (isWritingModeRoot())
    return -1;

  recalcSectionsIfNeeded();

  const LayoutTableSection* topNonEmptySection = this->topNonEmptySection();
  if (!topNonEmptySection)
    return -1;

  int baseline = topNonEmptySection->firstLineBoxBaseline();
  if (baseline >= 0)
    return (topNonEmptySection->logicalTop() + baseline).toInt();

  // FF, Presto and IE use the top of the section as the baseline if its first
  // row is empty of cells or content.
  // The baseline of an empty row isn't specified by CSS 2.1.
  if (topNonEmptySection->firstRow() &&
      !topNonEmptySection->firstRow()->firstCell())
    return topNonEmptySection->logicalTop().toInt();

  return -1;
}

LayoutRect LayoutTable::overflowClipRect(
    const LayoutPoint& location,
    OverlayScrollbarClipBehavior overlayScrollbarClipBehavior) const {
  LayoutRect rect =
      LayoutBlock::overflowClipRect(location, overlayScrollbarClipBehavior);

  // If we have a caption, expand the clip to include the caption.
  // FIXME: Technically this is wrong, but it's virtually impossible to fix this
  // for real until captions have been re-written.
  // FIXME: This code assumes (like all our other caption code) that only
  // top/bottom are supported.  When we actually support left/right and stop
  // mapping them to top/bottom, we might have to hack this code first
  // (depending on what order we do these bug fixes in).
  if (!m_captions.isEmpty()) {
    if (style()->isHorizontalWritingMode()) {
      rect.setHeight(size().height());
      rect.setY(location.y());
    } else {
      rect.setWidth(size().width());
      rect.setX(location.x());
    }
  }

  return rect;
}

bool LayoutTable::nodeAtPoint(HitTestResult& result,
                              const HitTestLocation& locationInContainer,
                              const LayoutPoint& accumulatedOffset,
                              HitTestAction action) {
  LayoutPoint adjustedLocation = accumulatedOffset + location();

  // Check kids first.
  if (!hasOverflowClip() ||
      locationInContainer.intersects(overflowClipRect(adjustedLocation))) {
    for (LayoutObject* child = lastChild(); child;
         child = child->previousSibling()) {
      if (child->isBox() && !toLayoutBox(child)->hasSelfPaintingLayer() &&
          (child->isTableSection() || child->isTableCaption())) {
        LayoutPoint childPoint =
            flipForWritingModeForChild(toLayoutBox(child), adjustedLocation);
        if (child->nodeAtPoint(result, locationInContainer, childPoint,
                               action)) {
          updateHitTestResult(
              result, toLayoutPoint(locationInContainer.point() - childPoint));
          return true;
        }
      }
    }
  }

  // Check our bounds next.
  LayoutRect boundsRect(adjustedLocation, size());
  if (visibleToHitTestRequest(result.hitTestRequest()) &&
      (action == HitTestBlockBackground ||
       action == HitTestChildBlockBackground) &&
      locationInContainer.intersects(boundsRect)) {
    updateHitTestResult(result,
                        flipForWritingMode(locationInContainer.point() -
                                           toLayoutSize(adjustedLocation)));
    if (result.addNodeToListBasedTestResult(node(), locationInContainer,
                                            boundsRect) == StopHitTesting)
      return true;
  }

  return false;
}

LayoutTable* LayoutTable::createAnonymousWithParent(
    const LayoutObject* parent) {
  RefPtr<ComputedStyle> newStyle =
      ComputedStyle::createAnonymousStyleWithDisplay(
          parent->styleRef(),
          parent->isLayoutInline() ? EDisplay::InlineTable : EDisplay::Table);
  LayoutTable* newTable = new LayoutTable(nullptr);
  newTable->setDocumentForAnonymous(&parent->document());
  newTable->setStyle(std::move(newStyle));
  return newTable;
}

const BorderValue& LayoutTable::tableStartBorderAdjoiningCell(
    const LayoutTableCell* cell) const {
  ASSERT(cell->isFirstOrLastCellInRow());
  if (hasSameDirectionAs(cell->row()))
    return style()->borderStart();

  return style()->borderEnd();
}

const BorderValue& LayoutTable::tableEndBorderAdjoiningCell(
    const LayoutTableCell* cell) const {
  ASSERT(cell->isFirstOrLastCellInRow());
  if (hasSameDirectionAs(cell->row()))
    return style()->borderEnd();

  return style()->borderStart();
}

void LayoutTable::ensureIsReadyForPaintInvalidation() {
  LayoutBlock::ensureIsReadyForPaintInvalidation();
  recalcCollapsedBordersIfNeeded();
}

PaintInvalidationReason LayoutTable::invalidatePaintIfNeeded(
    const PaintInvalidationState& paintInvalidationState) {
  if (collapseBorders() && !m_collapsedBorders.isEmpty())
    paintInvalidationState.paintingLayer()
        .setNeedsPaintPhaseDescendantBlockBackgrounds();

  return LayoutBlock::invalidatePaintIfNeeded(paintInvalidationState);
}

PaintInvalidationReason LayoutTable::invalidatePaintIfNeeded(
    const PaintInvalidatorContext& context) const {
  return TablePaintInvalidator(*this, context).invalidatePaintIfNeeded();
}

LayoutUnit LayoutTable::paddingTop() const {
  if (collapseBorders())
    return LayoutUnit();

  return LayoutBlock::paddingTop();
}

LayoutUnit LayoutTable::paddingBottom() const {
  if (collapseBorders())
    return LayoutUnit();

  return LayoutBlock::paddingBottom();
}

LayoutUnit LayoutTable::paddingLeft() const {
  if (collapseBorders())
    return LayoutUnit();

  return LayoutBlock::paddingLeft();
}

LayoutUnit LayoutTable::paddingRight() const {
  if (collapseBorders())
    return LayoutUnit();

  return LayoutBlock::paddingRight();
}

}  // namespace blink
