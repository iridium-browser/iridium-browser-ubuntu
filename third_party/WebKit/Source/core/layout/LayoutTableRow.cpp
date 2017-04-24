/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013
*                Apple Inc.
 *               All rights reserved.
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

#include "core/layout/LayoutTableRow.h"

#include "core/HTMLNames.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutState.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutView.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/paint/TableRowPainter.h"
#include "core/style/StyleInheritedData.h"

namespace blink {

using namespace HTMLNames;

LayoutTableRow::LayoutTableRow(Element* element)
    : LayoutTableBoxComponent(element), m_rowIndex(unsetRowIndex) {
  // init LayoutObject attributes
  setInline(false);  // our object is not Inline
}

void LayoutTableRow::willBeRemovedFromTree() {
  LayoutTableBoxComponent::willBeRemovedFromTree();

  section()->setNeedsCellRecalc();
}

void LayoutTableRow::styleDidChange(StyleDifference diff,
                                    const ComputedStyle* oldStyle) {
  DCHECK_EQ(style()->display(), EDisplay::TableRow);

  LayoutTableBoxComponent::styleDidChange(diff, oldStyle);
  propagateStyleToAnonymousChildren();

  if (!oldStyle)
    return;

  if (section() && style()->logicalHeight() != oldStyle->logicalHeight())
    section()->rowLogicalHeightChanged(this);

  if (!parent())
    return;
  LayoutTable* table = this->table();
  if (!table)
    return;

  if (!table->selfNeedsLayout() && !table->normalChildNeedsLayout() &&
      oldStyle->border() != style()->border())
    table->invalidateCollapsedBorders();

  if (LayoutTableBoxComponent::doCellsHaveDirtyWidth(*this, *table, diff,
                                                     *oldStyle)) {
    // If the border width changes on a row, we need to make sure the cells in
    // the row know to lay out again.
    // This only happens when borders are collapsed, since they end up affecting
    // the border sides of the cell itself.
    for (LayoutBox* childBox = firstChildBox(); childBox;
         childBox = childBox->nextSiblingBox()) {
      if (!childBox->isTableCell())
        continue;
      // TODO(dgrogan) Add a layout test showing that setChildNeedsLayout is
      // needed instead of setNeedsLayout.
      childBox->setChildNeedsLayout();
      childBox->setPreferredLogicalWidthsDirty(MarkOnlyThis);
    }
    // Most table componenents can rely on LayoutObject::styleDidChange
    // to mark the container chain dirty. But LayoutTableSection seems
    // to never clear its dirty bit, which stops the propagation. So
    // anything under LayoutTableSection has to restart the propagation
    // at the table.
    // TODO(dgrogan): Make LayoutTableSection clear its dirty bit.
    table->setPreferredLogicalWidthsDirty();
  }
}

const BorderValue& LayoutTableRow::borderAdjoiningStartCell(
    const LayoutTableCell* cell) const {
#if DCHECK_IS_ON()
  DCHECK(cell->isFirstOrLastCellInRow());
#endif
  // FIXME: https://webkit.org/b/79272 - Add support for mixed directionality at
  // the cell level.
  return style()->borderStart();
}

const BorderValue& LayoutTableRow::borderAdjoiningEndCell(
    const LayoutTableCell* cell) const {
#if DCHECK_IS_ON()
  DCHECK(cell->isFirstOrLastCellInRow());
#endif
  // FIXME: https://webkit.org/b/79272 - Add support for mixed directionality at
  // the cell level.
  return style()->borderEnd();
}

void LayoutTableRow::addChild(LayoutObject* child, LayoutObject* beforeChild) {
  if (!child->isTableCell()) {
    LayoutObject* last = beforeChild;
    if (!last)
      last = lastCell();
    if (last && last->isAnonymous() && last->isTableCell() &&
        !last->isBeforeOrAfterContent()) {
      LayoutTableCell* lastCell = toLayoutTableCell(last);
      if (beforeChild == lastCell)
        beforeChild = lastCell->firstChild();
      lastCell->addChild(child, beforeChild);
      return;
    }

    if (beforeChild && !beforeChild->isAnonymous() &&
        beforeChild->parent() == this) {
      LayoutObject* cell = beforeChild->previousSibling();
      if (cell && cell->isTableCell() && cell->isAnonymous()) {
        cell->addChild(child);
        return;
      }
    }

    // If beforeChild is inside an anonymous cell, insert into the cell.
    if (last && !last->isTableCell() && last->parent() &&
        last->parent()->isAnonymous() &&
        !last->parent()->isBeforeOrAfterContent()) {
      last->parent()->addChild(child, beforeChild);
      return;
    }

    LayoutTableCell* cell = LayoutTableCell::createAnonymousWithParent(this);
    addChild(cell, beforeChild);
    cell->addChild(child);
    return;
  }

  if (beforeChild && beforeChild->parent() != this)
    beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

  LayoutTableCell* cell = toLayoutTableCell(child);

  ASSERT(!beforeChild || beforeChild->isTableCell());
  LayoutTableBoxComponent::addChild(cell, beforeChild);

  // Generated content can result in us having a null section so make sure to
  // null check our parent.
  if (parent()) {
    section()->addCell(cell, this);
    // When borders collapse, adding a cell can affect the the width of
    // neighboring cells.
    LayoutTable* enclosingTable = table();
    if (enclosingTable && enclosingTable->collapseBorders()) {
      if (LayoutTableCell* previousCell = cell->previousCell())
        previousCell->setNeedsLayoutAndPrefWidthsRecalc(
            LayoutInvalidationReason::TableChanged);
      if (LayoutTableCell* nextCell = cell->nextCell())
        nextCell->setNeedsLayoutAndPrefWidthsRecalc(
            LayoutInvalidationReason::TableChanged);
    }
  }

  if (beforeChild || nextRow())
    section()->setNeedsCellRecalc();
}

void LayoutTableRow::layout() {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);
  bool paginated = view()->layoutState()->isPaginated();

  for (LayoutTableCell* cell = firstCell(); cell; cell = cell->nextCell()) {
    SubtreeLayoutScope layouter(*cell);
    cell->setLogicalTop(logicalTop());
    if (!cell->needsLayout())
      section()->markChildForPaginationRelayoutIfNeeded(*cell, layouter);
    if (cell->needsLayout())
      cell->layout();
    if (paginated)
      section()->updateFragmentationInfoForChild(*cell);
  }

  m_overflow.reset();
  addVisualEffectOverflow();
  // We do not call addOverflowFromCell here. The cell are laid out to be
  // measured above and will be sized correctly in a follow-up phase.

  // We only ever need to issue paint invalidations if our cells didn't, which
  // means that they didn't need layout, so we know that our bounds didn't
  // change. This code is just making up for the fact that we did not invalidate
  // paints in setStyle() because we had a layout hint.
  if (selfNeedsLayout()) {
    for (LayoutTableCell* cell = firstCell(); cell; cell = cell->nextCell()) {
      // FIXME: Is this needed when issuing paint invalidations after layout?
      cell->setShouldDoFullPaintInvalidation();
    }
  }

  // LayoutTableSection::layoutRows will set our logical height and width later,
  // so it calls updateLayerTransform().
  clearNeedsLayout();
}

// Hit Testing
bool LayoutTableRow::nodeAtPoint(HitTestResult& result,
                                 const HitTestLocation& locationInContainer,
                                 const LayoutPoint& accumulatedOffset,
                                 HitTestAction action) {
  // Table rows cannot ever be hit tested.  Effectively they do not exist.
  // Just forward to our children always.
  for (LayoutTableCell* cell = lastCell(); cell; cell = cell->previousCell()) {
    // FIXME: We have to skip over inline flows, since they can show up inside
    // table rows at the moment (a demoted inline <form> for example). If we
    // ever implement a table-specific hit-test method (which we should do for
    // performance reasons anyway), then we can remove this check.
    if (!cell->hasSelfPaintingLayer()) {
      LayoutPoint cellPoint =
          flipForWritingModeForChild(cell, accumulatedOffset);
      if (cell->nodeAtPoint(result, locationInContainer, cellPoint, action)) {
        updateHitTestResult(
            result, locationInContainer.point() - toLayoutSize(cellPoint));
        return true;
      }
    }
  }

  return false;
}

LayoutBox::PaginationBreakability LayoutTableRow::getPaginationBreakability()
    const {
  PaginationBreakability breakability =
      LayoutTableBoxComponent::getPaginationBreakability();
  if (breakability == AllowAnyBreaks) {
    // Even if the row allows us to break inside, we will want to prevent that
    // if we have a header group that wants to appear at the top of each page.
    if (const LayoutTableSection* header = table()->header())
      breakability = header->getPaginationBreakability();
  }
  return breakability;
}

void LayoutTableRow::paint(const PaintInfo& paintInfo,
                           const LayoutPoint& paintOffset) const {
  TableRowPainter(*this).paint(paintInfo, paintOffset);
}

LayoutTableRow* LayoutTableRow::createAnonymous(Document* document) {
  LayoutTableRow* layoutObject = new LayoutTableRow(nullptr);
  layoutObject->setDocumentForAnonymous(document);
  return layoutObject;
}

LayoutTableRow* LayoutTableRow::createAnonymousWithParent(
    const LayoutObject* parent) {
  LayoutTableRow* newRow = LayoutTableRow::createAnonymous(&parent->document());
  RefPtr<ComputedStyle> newStyle =
      ComputedStyle::createAnonymousStyleWithDisplay(parent->styleRef(),
                                                     EDisplay::TableRow);
  newRow->setStyle(std::move(newStyle));
  return newRow;
}

void LayoutTableRow::computeOverflow() {
  clearAllOverflows();
  addVisualEffectOverflow();
  for (LayoutTableCell* cell = firstCell(); cell; cell = cell->nextCell())
    addOverflowFromCell(cell);
}

void LayoutTableRow::addOverflowFromCell(const LayoutTableCell* cell) {
  // Non-row-spanning-cells don't create overflow (they are fully contained
  // within this row).
  // TODO(crbug.com/603993): This seems incorrect because cell may have visual
  // effect overflow that should be included in this row.
  if (cell->rowSpan() == 1)
    return;

  // Cells only generates visual overflow.
  LayoutRect cellVisualOverflowRect =
      cell->visualOverflowRectForPropagation(styleRef());

  // The cell and the row share the section's coordinate system. However
  // the visual overflow should be determined in the coordinate system of
  // the row, that's why we shift it below.
  LayoutUnit cellOffsetLogicalTopDifference =
      cell->location().y() - location().y();
  cellVisualOverflowRect.move(LayoutUnit(), cellOffsetLogicalTopDifference);

  addContentsVisualOverflow(cellVisualOverflowRect);
}

bool LayoutTableRow::isFirstRowInSectionAfterHeader() const {
  // If there isn't room on the page for at least one content row after the
  // header group, then we won't repeat the header on each page.
  // https://drafts.csswg.org/css-tables-3/#repeated-headers reads like
  // it wants us to drop headers on only the pages that a single row
  // won't fit but we avoid the complexity of that reading until it
  // is clarified. Tracked by crbug.com/675904
  if (rowIndex())
    return false;
  LayoutTableSection* header = table()->header();
  return header && table()->sectionAbove(section()) == header &&
         header->getPaginationBreakability() != AllowAnyBreaks;
}

}  // namespace blink
