/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009, 2010, 2013 Apple Inc.
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

#include "core/layout/LayoutTableSection.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutView.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/paint/TableSectionPainter.h"
#include "wtf/HashSet.h"
#include <algorithm>
#include <limits>

namespace blink {

using namespace HTMLNames;

// This variable is used to balance the memory consumption vs the paint
// invalidation time on big tables.
static unsigned gMinTableSizeToUseFastPaintPathWithOverflowingCell = 75 * 75;

static inline void setRowLogicalHeightToRowStyleLogicalHeight(
    LayoutTableSection::RowStruct& row) {
  ASSERT(row.rowLayoutObject);
  row.logicalHeight = row.rowLayoutObject->style()->logicalHeight();
}

static inline void updateLogicalHeightForCell(
    LayoutTableSection::RowStruct& row,
    const LayoutTableCell* cell) {
  // We ignore height settings on rowspan cells.
  if (cell->rowSpan() != 1)
    return;

  Length logicalHeight = cell->style()->logicalHeight();
  if (logicalHeight.isPositive()) {
    Length cRowLogicalHeight = row.logicalHeight;
    switch (logicalHeight.type()) {
      case Percent:
        // TODO(alancutter): Make this work correctly for calc lengths.
        if (!(cRowLogicalHeight.isPercentOrCalc()) ||
            (cRowLogicalHeight.isPercent() &&
             cRowLogicalHeight.percent() < logicalHeight.percent()))
          row.logicalHeight = logicalHeight;
        break;
      case Fixed:
        if (cRowLogicalHeight.type() < Percent ||
            (cRowLogicalHeight.isFixed() &&
             cRowLogicalHeight.value() < logicalHeight.value()))
          row.logicalHeight = logicalHeight;
        break;
      default:
        break;
    }
  }
}

void CellSpan::ensureConsistency(const unsigned maximumSpanSize) {
  static_assert(std::is_same<decltype(m_start), unsigned>::value,
                "Asserts below assume m_start is unsigned");
  static_assert(std::is_same<decltype(m_end), unsigned>::value,
                "Asserts below assume m_end is unsigned");
  RELEASE_ASSERT(m_start <= maximumSpanSize);
  RELEASE_ASSERT(m_end <= maximumSpanSize);
  RELEASE_ASSERT(m_start <= m_end);
}

LayoutTableSection::CellStruct::CellStruct() : inColSpan(false) {}

LayoutTableSection::CellStruct::~CellStruct() {}

LayoutTableSection::LayoutTableSection(Element* element)
    : LayoutTableBoxComponent(element),
      m_cCol(0),
      m_cRow(0),
      m_outerBorderStart(0),
      m_outerBorderEnd(0),
      m_outerBorderBefore(0),
      m_outerBorderAfter(0),
      m_needsCellRecalc(false),
      m_forceSlowPaintPathWithOverflowingCell(false),
      m_hasMultipleCellLevels(false) {
  // init LayoutObject attributes
  setInline(false);  // our object is not Inline
}

LayoutTableSection::~LayoutTableSection() {}

void LayoutTableSection::styleDidChange(StyleDifference diff,
                                        const ComputedStyle* oldStyle) {
  DCHECK(style()->display() == EDisplay::TableFooterGroup ||
         style()->display() == EDisplay::TableRowGroup ||
         style()->display() == EDisplay::TableHeaderGroup);

  LayoutTableBoxComponent::styleDidChange(diff, oldStyle);
  propagateStyleToAnonymousChildren();

  if (!oldStyle)
    return;

  LayoutTable* table = this->table();
  if (!table)
    return;

  if (!table->selfNeedsLayout() && !table->normalChildNeedsLayout() &&
      oldStyle->border() != style()->border())
    table->invalidateCollapsedBorders();

  if (LayoutTableBoxComponent::doCellsHaveDirtyWidth(*this, *table, diff,
                                                     *oldStyle))
    markAllCellsWidthsDirtyAndOrNeedsLayout(
        LayoutTable::MarkDirtyAndNeedsLayout);
}

void LayoutTableSection::willBeRemovedFromTree() {
  LayoutTableBoxComponent::willBeRemovedFromTree();

  // Preventively invalidate our cells as we may be re-inserted into
  // a new table which would require us to rebuild our structure.
  setNeedsCellRecalc();
}

void LayoutTableSection::addChild(LayoutObject* child,
                                  LayoutObject* beforeChild) {
  if (!child->isTableRow()) {
    LayoutObject* last = beforeChild;
    if (!last)
      last = lastRow();
    if (last && last->isAnonymous() && !last->isBeforeOrAfterContent()) {
      if (beforeChild == last)
        beforeChild = last->slowFirstChild();
      last->addChild(child, beforeChild);
      return;
    }

    if (beforeChild && !beforeChild->isAnonymous() &&
        beforeChild->parent() == this) {
      LayoutObject* row = beforeChild->previousSibling();
      if (row && row->isTableRow() && row->isAnonymous()) {
        row->addChild(child);
        return;
      }
    }

    // If beforeChild is inside an anonymous cell/row, insert into the cell or
    // into the anonymous row containing it, if there is one.
    LayoutObject* lastBox = last;
    while (lastBox && lastBox->parent()->isAnonymous() &&
           !lastBox->isTableRow())
      lastBox = lastBox->parent();
    if (lastBox && lastBox->isAnonymous() &&
        !lastBox->isBeforeOrAfterContent()) {
      lastBox->addChild(child, beforeChild);
      return;
    }

    LayoutObject* row = LayoutTableRow::createAnonymousWithParent(this);
    addChild(row, beforeChild);
    row->addChild(child);
    return;
  }

  if (beforeChild)
    setNeedsCellRecalc();

  unsigned insertionRow = m_cRow;
  ++m_cRow;
  m_cCol = 0;

  ensureRows(m_cRow);

  LayoutTableRow* row = toLayoutTableRow(child);
  m_grid[insertionRow].rowLayoutObject = row;
  row->setRowIndex(insertionRow);

  if (!beforeChild)
    setRowLogicalHeightToRowStyleLogicalHeight(m_grid[insertionRow]);

  if (beforeChild && beforeChild->parent() != this)
    beforeChild = splitAnonymousBoxesAroundChild(beforeChild);

  ASSERT(!beforeChild || beforeChild->isTableRow());
  LayoutTableBoxComponent::addChild(child, beforeChild);
}

static inline void checkThatVectorIsDOMOrdered(
    const Vector<LayoutTableCell*, 1>& cells) {
#ifndef NDEBUG
  // This function should be called on a non-empty vector.
  ASSERT(cells.size() > 0);

  const LayoutTableCell* previousCell = cells[0];
  for (size_t i = 1; i < cells.size(); ++i) {
    const LayoutTableCell* currentCell = cells[i];
    // The check assumes that all cells belong to the same row group.
    ASSERT(previousCell->section() == currentCell->section());

    // 2 overlapping cells can't be on the same row.
    ASSERT(currentCell->row() != previousCell->row());

    // Look backwards in the tree for the previousCell's row. If we are
    // DOM ordered, we should find it.
    const LayoutTableRow* row = currentCell->row();
    for (; row && row != previousCell->row(); row = row->previousRow()) {
    }
    ASSERT(row == previousCell->row());

    previousCell = currentCell;
  }
#endif  // NDEBUG
}

void LayoutTableSection::addCell(LayoutTableCell* cell, LayoutTableRow* row) {
  // We don't insert the cell if we need cell recalc as our internal columns'
  // representation will have drifted from the table's representation. Also
  // recalcCells will call addCell at a later time after sync'ing our columns'
  // with the table's.
  if (needsCellRecalc())
    return;

  unsigned rSpan = cell->rowSpan();
  unsigned cSpan = cell->colSpan();
  const Vector<LayoutTable::ColumnStruct>& columns =
      table()->effectiveColumns();
  unsigned insertionRow = row->rowIndex();

  // ### mozilla still seems to do the old HTML way, even for strict DTD
  // (see the annotation on table cell layouting in the CSS specs and the
  // testcase below:
  // <TABLE border>
  // <TR><TD>1 <TD rowspan="2">2 <TD>3 <TD>4
  // <TR><TD colspan="2">5
  // </TABLE>
  while (m_cCol < numCols(insertionRow) &&
         (cellAt(insertionRow, m_cCol).hasCells() ||
          cellAt(insertionRow, m_cCol).inColSpan))
    m_cCol++;

  updateLogicalHeightForCell(m_grid[insertionRow], cell);

  ensureRows(insertionRow + rSpan);

  m_grid[insertionRow].rowLayoutObject = row;

  unsigned col = m_cCol;
  // tell the cell where it is
  bool inColSpan = false;
  while (cSpan) {
    unsigned currentSpan;
    if (m_cCol >= columns.size()) {
      table()->appendEffectiveColumn(cSpan);
      currentSpan = cSpan;
    } else {
      if (cSpan < columns[m_cCol].span)
        table()->splitEffectiveColumn(m_cCol, cSpan);
      currentSpan = columns[m_cCol].span;
    }
    for (unsigned r = 0; r < rSpan; r++) {
      ensureCols(insertionRow + r, m_cCol + 1);
      CellStruct& c = cellAt(insertionRow + r, m_cCol);
      ASSERT(cell);
      c.cells.push_back(cell);
      checkThatVectorIsDOMOrdered(c.cells);
      // If cells overlap then we take the slow path for painting.
      if (c.cells.size() > 1)
        m_hasMultipleCellLevels = true;
      if (inColSpan)
        c.inColSpan = true;
    }
    m_cCol++;
    cSpan -= currentSpan;
    inColSpan = true;
  }
  cell->setAbsoluteColumnIndex(table()->effectiveColumnToAbsoluteColumn(col));
}

bool LayoutTableSection::rowHasOnlySpanningCells(unsigned row) {
  unsigned totalCols = m_grid[row].row.size();

  if (!totalCols)
    return false;

  for (unsigned col = 0; col < totalCols; col++) {
    const CellStruct& rowSpanCell = cellAt(row, col);

    // Empty cell is not a valid cell so it is not a rowspan cell.
    if (rowSpanCell.cells.isEmpty())
      return false;

    if (rowSpanCell.cells[0]->rowSpan() == 1)
      return false;
  }

  return true;
}

void LayoutTableSection::populateSpanningRowsHeightFromCell(
    LayoutTableCell* cell,
    struct SpanningRowsHeight& spanningRowsHeight) {
  const unsigned rowSpan = cell->rowSpan();
  const unsigned rowIndex = cell->rowIndex();

  spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing =
      cell->logicalHeightForRowSizing();

  spanningRowsHeight.rowHeight.resize(rowSpan);
  spanningRowsHeight.totalRowsHeight = 0;
  for (unsigned row = 0; row < rowSpan; row++) {
    unsigned actualRow = row + rowIndex;

    spanningRowsHeight.rowHeight[row] = m_rowPos[actualRow + 1] -
                                        m_rowPos[actualRow] -
                                        borderSpacingForRow(actualRow);
    if (!spanningRowsHeight.rowHeight[row])
      spanningRowsHeight.isAnyRowWithOnlySpanningCells |=
          rowHasOnlySpanningCells(actualRow);

    spanningRowsHeight.totalRowsHeight += spanningRowsHeight.rowHeight[row];
    spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing -=
        borderSpacingForRow(actualRow);
  }
  // We don't span the following row so its border-spacing (if any) should be
  // included.
  spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing +=
      borderSpacingForRow(rowIndex + rowSpan - 1);
}

void LayoutTableSection::distributeExtraRowSpanHeightToPercentRows(
    LayoutTableCell* cell,
    float totalPercent,
    int& extraRowSpanningHeight,
    Vector<int>& rowsHeight) {
  if (!extraRowSpanningHeight || !totalPercent)
    return;

  const unsigned rowSpan = cell->rowSpan();
  const unsigned rowIndex = cell->rowIndex();
  float percent = std::min(totalPercent, 100.0f);
  const int tableHeight = m_rowPos[m_grid.size()] + extraRowSpanningHeight;

  // Our algorithm matches Firefox. Extra spanning height would be distributed
  // Only in first percent height rows those total percent is 100. Other percent
  // rows would be uneffected even extra spanning height is remain.
  int accumulatedPositionIncrease = 0;
  for (unsigned row = rowIndex; row < (rowIndex + rowSpan); row++) {
    if (percent > 0 && extraRowSpanningHeight > 0) {
      // TODO(alancutter): Make this work correctly for calc lengths.
      if (m_grid[row].logicalHeight.isPercent()) {
        int toAdd =
            (tableHeight *
             std::min(m_grid[row].logicalHeight.percent(), percent) / 100) -
            rowsHeight[row - rowIndex];

        toAdd = std::max(std::min(toAdd, extraRowSpanningHeight), 0);
        accumulatedPositionIncrease += toAdd;
        extraRowSpanningHeight -= toAdd;
        percent -= m_grid[row].logicalHeight.percent();
      }
    }
    m_rowPos[row + 1] += accumulatedPositionIncrease;
  }
}

static void updatePositionIncreasedWithRowHeight(
    int extraHeight,
    float rowHeight,
    float totalHeight,
    int& accumulatedPositionIncrease,
    double& remainder) {
  // Without the cast we lose enough precision to cause heights to miss pixels
  // (and trigger asserts) in some layout tests.
  double proportionalPositionIncrease =
      remainder + (extraHeight * double(rowHeight)) / totalHeight;
  // The epsilon is to push any values that are close to a whole number but
  // aren't due to floating point imprecision. The epsilons are not accumulated,
  // any that aren't necessary are lost in the cast to int.
  int positionIncreaseInt = proportionalPositionIncrease + 0.000001;
  accumulatedPositionIncrease += positionIncreaseInt;
  remainder = proportionalPositionIncrease - positionIncreaseInt;
}

// This is mainly used to distribute whole extra rowspanning height in percent
// rows when all spanning rows are percent rows.
// Distributing whole extra rowspanning height in percent rows based on the
// ratios of percent because this method works same as percent distribution when
// only percent rows are present and percent is 100. Also works perfectly fine
// when percent is not equal to 100.
void LayoutTableSection::distributeWholeExtraRowSpanHeightToPercentRows(
    LayoutTableCell* cell,
    float totalPercent,
    int& extraRowSpanningHeight,
    Vector<int>& rowsHeight) {
  if (!extraRowSpanningHeight || !totalPercent)
    return;

  const unsigned rowSpan = cell->rowSpan();
  const unsigned rowIndex = cell->rowIndex();
  double remainder = 0;

  int accumulatedPositionIncrease = 0;
  for (unsigned row = rowIndex; row < (rowIndex + rowSpan); row++) {
    // TODO(alancutter): Make this work correctly for calc lengths.
    if (m_grid[row].logicalHeight.isPercent()) {
      updatePositionIncreasedWithRowHeight(
          extraRowSpanningHeight, m_grid[row].logicalHeight.percent(),
          totalPercent, accumulatedPositionIncrease, remainder);
    }
    m_rowPos[row + 1] += accumulatedPositionIncrease;
  }

  DCHECK(!round(remainder)) << "remainder was " << remainder;

  extraRowSpanningHeight -= accumulatedPositionIncrease;
}

void LayoutTableSection::distributeExtraRowSpanHeightToAutoRows(
    LayoutTableCell* cell,
    int totalAutoRowsHeight,
    int& extraRowSpanningHeight,
    Vector<int>& rowsHeight) {
  if (!extraRowSpanningHeight || !totalAutoRowsHeight)
    return;

  const unsigned rowSpan = cell->rowSpan();
  const unsigned rowIndex = cell->rowIndex();
  int accumulatedPositionIncrease = 0;
  double remainder = 0;

  // Aspect ratios of auto rows should not change otherwise table may look
  // different than user expected. So extra height distributed in auto spanning
  // rows based on their weight in spanning cell.
  for (unsigned row = rowIndex; row < (rowIndex + rowSpan); row++) {
    if (m_grid[row].logicalHeight.isAuto()) {
      updatePositionIncreasedWithRowHeight(
          extraRowSpanningHeight, rowsHeight[row - rowIndex],
          totalAutoRowsHeight, accumulatedPositionIncrease, remainder);
    }
    m_rowPos[row + 1] += accumulatedPositionIncrease;
  }

  DCHECK(!round(remainder)) << "remainder was " << remainder;

  extraRowSpanningHeight -= accumulatedPositionIncrease;
}

void LayoutTableSection::distributeExtraRowSpanHeightToRemainingRows(
    LayoutTableCell* cell,
    int totalRemainingRowsHeight,
    int& extraRowSpanningHeight,
    Vector<int>& rowsHeight) {
  if (!extraRowSpanningHeight || !totalRemainingRowsHeight)
    return;

  const unsigned rowSpan = cell->rowSpan();
  const unsigned rowIndex = cell->rowIndex();
  int accumulatedPositionIncrease = 0;
  double remainder = 0;

  // Aspect ratios of the rows should not change otherwise table may look
  // different than user expected. So extra height distribution in remaining
  // spanning rows based on their weight in spanning cell.
  for (unsigned row = rowIndex; row < (rowIndex + rowSpan); row++) {
    if (!m_grid[row].logicalHeight.isPercentOrCalc()) {
      updatePositionIncreasedWithRowHeight(
          extraRowSpanningHeight, rowsHeight[row - rowIndex],
          totalRemainingRowsHeight, accumulatedPositionIncrease, remainder);
    }
    m_rowPos[row + 1] += accumulatedPositionIncrease;
  }

  DCHECK(!round(remainder)) << "remainder was " << remainder;

  extraRowSpanningHeight -= accumulatedPositionIncrease;
}

static bool cellIsFullyIncludedInOtherCell(const LayoutTableCell* cell1,
                                           const LayoutTableCell* cell2) {
  return (cell1->rowIndex() >= cell2->rowIndex() &&
          (cell1->rowIndex() + cell1->rowSpan()) <=
              (cell2->rowIndex() + cell2->rowSpan()));
}

// To avoid unneeded extra height distributions, we apply the following sorting
// algorithm:
static bool compareRowSpanCellsInHeightDistributionOrder(
    const LayoutTableCell* cell1,
    const LayoutTableCell* cell2) {
  // Sorting bigger height cell first if cells are at same index with same span
  // because we will skip smaller height cell to distribute it's extra height.
  if (cell1->rowIndex() == cell2->rowIndex() &&
      cell1->rowSpan() == cell2->rowSpan())
    return (cell1->logicalHeightForRowSizing() >
            cell2->logicalHeightForRowSizing());
  // Sorting inner most cell first because if inner spanning cell'e extra height
  // is distributed then outer spanning cell's extra height will adjust
  // accordingly. In reverse order, there is more chances that outer spanning
  // cell's height will exceed than defined by user.
  if (cellIsFullyIncludedInOtherCell(cell1, cell2))
    return true;
  // Sorting lower row index first because first we need to apply the extra
  // height of spanning cell which comes first in the table so lower rows's
  // position would increment in sequence.
  if (!cellIsFullyIncludedInOtherCell(cell2, cell1))
    return (cell1->rowIndex() < cell2->rowIndex());

  return false;
}

unsigned LayoutTableSection::calcRowHeightHavingOnlySpanningCells(
    unsigned row,
    int& accumulatedCellPositionIncrease,
    unsigned rowToApplyExtraHeight,
    unsigned& extraTableHeightToPropgate,
    Vector<int>& rowsCountWithOnlySpanningCells) {
  ASSERT(rowHasOnlySpanningCells(row));

  unsigned totalCols = m_grid[row].row.size();

  if (!totalCols)
    return 0;

  unsigned rowHeight = 0;

  for (unsigned col = 0; col < totalCols; col++) {
    const CellStruct& rowSpanCell = cellAt(row, col);

    if (!rowSpanCell.cells.size())
      continue;

    LayoutTableCell* cell = rowSpanCell.cells[0];

    if (cell->rowSpan() < 2)
      continue;

    const unsigned cellRowIndex = cell->rowIndex();
    const unsigned cellRowSpan = cell->rowSpan();

    // As we are going from the top of the table to the bottom to calculate the
    // row heights for rows that only contain spanning cells and all previous
    // rows are processed we only need to find the number of rows with spanning
    // cells from the current cell to the end of the current cells spanning
    // height.
    unsigned startRowForSpanningCellCount = std::max(cellRowIndex, row);
    unsigned endRow = cellRowIndex + cellRowSpan;
    unsigned spanningCellsRowsCountHavingZeroHeight =
        rowsCountWithOnlySpanningCells[endRow - 1];

    if (startRowForSpanningCellCount)
      spanningCellsRowsCountHavingZeroHeight -=
          rowsCountWithOnlySpanningCells[startRowForSpanningCellCount - 1];

    int totalRowspanCellHeight = (m_rowPos[endRow] - m_rowPos[cellRowIndex]) -
                                 borderSpacingForRow(endRow - 1);

    totalRowspanCellHeight += accumulatedCellPositionIncrease;
    if (rowToApplyExtraHeight >= cellRowIndex && rowToApplyExtraHeight < endRow)
      totalRowspanCellHeight += extraTableHeightToPropgate;

    if (totalRowspanCellHeight < cell->logicalHeightForRowSizing()) {
      unsigned extraHeightRequired =
          cell->logicalHeightForRowSizing() - totalRowspanCellHeight;

      rowHeight =
          std::max(rowHeight, extraHeightRequired /
                                  spanningCellsRowsCountHavingZeroHeight);
    }
  }

  return rowHeight;
}

void LayoutTableSection::updateRowsHeightHavingOnlySpanningCells(
    LayoutTableCell* cell,
    struct SpanningRowsHeight& spanningRowsHeight,
    unsigned& extraHeightToPropagate,
    Vector<int>& rowsCountWithOnlySpanningCells) {
  ASSERT(spanningRowsHeight.rowHeight.size());

  int accumulatedPositionIncrease = 0;
  const unsigned rowSpan = cell->rowSpan();
  const unsigned rowIndex = cell->rowIndex();

  DCHECK_EQ(rowSpan, spanningRowsHeight.rowHeight.size());

  for (unsigned row = 0; row < spanningRowsHeight.rowHeight.size(); row++) {
    unsigned actualRow = row + rowIndex;
    if (!spanningRowsHeight.rowHeight[row] &&
        rowHasOnlySpanningCells(actualRow)) {
      spanningRowsHeight.rowHeight[row] = calcRowHeightHavingOnlySpanningCells(
          actualRow, accumulatedPositionIncrease, rowIndex + rowSpan,
          extraHeightToPropagate, rowsCountWithOnlySpanningCells);
      accumulatedPositionIncrease += spanningRowsHeight.rowHeight[row];
    }
    m_rowPos[actualRow + 1] += accumulatedPositionIncrease;
  }

  spanningRowsHeight.totalRowsHeight += accumulatedPositionIncrease;
}

// Distribute rowSpan cell height in rows those comes in rowSpan cell based on
// the ratio of row's height if 1 RowSpan cell height is greater than the total
// height of rows in rowSpan cell.
void LayoutTableSection::distributeRowSpanHeightToRows(
    SpanningLayoutTableCells& rowSpanCells) {
  ASSERT(rowSpanCells.size());

  // 'rowSpanCells' list is already sorted based on the cells rowIndex in
  // ascending order
  // Arrange row spanning cell in the order in which we need to process first.
  std::sort(rowSpanCells.begin(), rowSpanCells.end(),
            compareRowSpanCellsInHeightDistributionOrder);

  unsigned extraHeightToPropagate = 0;
  unsigned lastRowIndex = 0;
  unsigned lastRowSpan = 0;

  Vector<int> rowsCountWithOnlySpanningCells;

  // At this stage, Height of the rows are zero for the one containing only
  // spanning cells.
  int count = 0;
  for (unsigned row = 0; row < m_grid.size(); row++) {
    if (rowHasOnlySpanningCells(row))
      count++;
    rowsCountWithOnlySpanningCells.push_back(count);
  }

  for (unsigned i = 0; i < rowSpanCells.size(); i++) {
    LayoutTableCell* cell = rowSpanCells[i];

    unsigned rowIndex = cell->rowIndex();

    unsigned rowSpan = cell->rowSpan();

    unsigned spanningCellEndIndex = rowIndex + rowSpan;
    unsigned lastSpanningCellEndIndex = lastRowIndex + lastRowSpan;

    // Only the highest spanning cell will distribute its extra height in a row
    // if more than one spanning cell is present at the same level.
    if (rowIndex == lastRowIndex && rowSpan == lastRowSpan)
      continue;

    int originalBeforePosition = m_rowPos[spanningCellEndIndex];

    // When 2 spanning cells are ending at same row index then while extra
    // height distribution of first spanning cell updates position of the last
    // row so getting the original position of the last row in second spanning
    // cell need to reduce the height changed by first spanning cell.
    if (spanningCellEndIndex == lastSpanningCellEndIndex)
      originalBeforePosition -= extraHeightToPropagate;

    if (extraHeightToPropagate) {
      for (unsigned row = lastSpanningCellEndIndex + 1;
           row <= spanningCellEndIndex; row++)
        m_rowPos[row] += extraHeightToPropagate;
    }

    lastRowIndex = rowIndex;
    lastRowSpan = rowSpan;

    struct SpanningRowsHeight spanningRowsHeight;

    populateSpanningRowsHeightFromCell(cell, spanningRowsHeight);

    // Here we are handling only row(s) who have only rowspanning cells and do
    // not have any empty cell.
    if (spanningRowsHeight.isAnyRowWithOnlySpanningCells)
      updateRowsHeightHavingOnlySpanningCells(cell, spanningRowsHeight,
                                              extraHeightToPropagate,
                                              rowsCountWithOnlySpanningCells);

    // This code handle row(s) that have rowspanning cell(s) and at least one
    // empty cell. Such rows are not handled below and end up having a height of
    // 0. That would mean content overlapping if one of their cells has any
    // content. To avoid the problem, we add all the remaining spanning cells'
    // height to the last spanned row. This means that we could grow a row past
    // its 'height' or break percentage spreading however this is better than
    // overlapping content.
    // FIXME: Is there a better algorithm?
    if (!spanningRowsHeight.totalRowsHeight) {
      if (spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing)
        m_rowPos[spanningCellEndIndex] +=
            spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing +
            borderSpacingForRow(spanningCellEndIndex - 1);

      extraHeightToPropagate =
          m_rowPos[spanningCellEndIndex] - originalBeforePosition;
      continue;
    }

    if (spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing <=
        spanningRowsHeight.totalRowsHeight) {
      extraHeightToPropagate =
          m_rowPos[rowIndex + rowSpan] - originalBeforePosition;
      continue;
    }

    // Below we are handling only row(s) who have at least one visible cell
    // without rowspan value.
    float totalPercent = 0;
    int totalAutoRowsHeight = 0;
    int totalRemainingRowsHeight = spanningRowsHeight.totalRowsHeight;

    // FIXME: Inner spanning cell height should not change if it have fixed
    // height when it's parent spanning cell is distributing it's extra height
    // in rows.

    // Calculate total percentage, total auto rows height and total rows height
    // except percent rows.
    for (unsigned row = rowIndex; row < spanningCellEndIndex; row++) {
      // TODO(alancutter): Make this work correctly for calc lengths.
      if (m_grid[row].logicalHeight.isPercent()) {
        totalPercent += m_grid[row].logicalHeight.percent();
        totalRemainingRowsHeight -=
            spanningRowsHeight.rowHeight[row - rowIndex];
      } else if (m_grid[row].logicalHeight.isAuto()) {
        totalAutoRowsHeight += spanningRowsHeight.rowHeight[row - rowIndex];
      }
    }

    int extraRowSpanningHeight =
        spanningRowsHeight.spanningCellHeightIgnoringBorderSpacing -
        spanningRowsHeight.totalRowsHeight;

    if (totalPercent < 100 && !totalAutoRowsHeight &&
        !totalRemainingRowsHeight) {
      // Distributing whole extra rowspanning height in percent row when only
      // non-percent rows height is 0.
      distributeWholeExtraRowSpanHeightToPercentRows(
          cell, totalPercent, extraRowSpanningHeight,
          spanningRowsHeight.rowHeight);
    } else {
      distributeExtraRowSpanHeightToPercentRows(cell, totalPercent,
                                                extraRowSpanningHeight,
                                                spanningRowsHeight.rowHeight);
      distributeExtraRowSpanHeightToAutoRows(cell, totalAutoRowsHeight,
                                             extraRowSpanningHeight,
                                             spanningRowsHeight.rowHeight);
      distributeExtraRowSpanHeightToRemainingRows(
          cell, totalRemainingRowsHeight, extraRowSpanningHeight,
          spanningRowsHeight.rowHeight);
    }

    ASSERT(!extraRowSpanningHeight);

    // Getting total changed height in the table
    extraHeightToPropagate =
        m_rowPos[spanningCellEndIndex] - originalBeforePosition;
  }

  if (extraHeightToPropagate) {
    // Apply changed height by rowSpan cells to rows present at the end of the
    // table
    for (unsigned row = lastRowIndex + lastRowSpan + 1; row <= m_grid.size();
         row++)
      m_rowPos[row] += extraHeightToPropagate;
  }
}

// Find out the baseline of the cell
// If the cell's baseline is more than the row's baseline then the cell's
// baseline become the row's baseline and if the row's baseline goes out of the
// row's boundaries then adjust row height accordingly.
void LayoutTableSection::updateBaselineForCell(LayoutTableCell* cell,
                                               unsigned row,
                                               int& baselineDescent) {
  if (!cell->isBaselineAligned())
    return;

  // Ignoring the intrinsic padding as it depends on knowing the row's baseline,
  // which won't be accurate until the end of this function.
  int baselinePosition =
      cell->cellBaselinePosition() - cell->intrinsicPaddingBefore();
  if (baselinePosition >
      cell->borderBefore() +
          (cell->paddingBefore() - cell->intrinsicPaddingBefore())) {
    m_grid[row].baseline = std::max(m_grid[row].baseline, baselinePosition);

    int cellStartRowBaselineDescent = 0;
    if (cell->rowSpan() == 1) {
      baselineDescent =
          std::max(baselineDescent,
                   cell->logicalHeightForRowSizing() - baselinePosition);
      cellStartRowBaselineDescent = baselineDescent;
    }
    m_rowPos[row + 1] =
        std::max<int>(m_rowPos[row + 1], m_rowPos[row] + m_grid[row].baseline +
                                             cellStartRowBaselineDescent);
  }
}

int LayoutTableSection::calcRowLogicalHeight() {
#if DCHECK_IS_ON()
  SetLayoutNeededForbiddenScope layoutForbiddenScope(*this);
#endif

  ASSERT(!needsLayout());

  LayoutTableCell* cell;

  // We may have to forcefully lay out cells here, in which case we need a
  // layout state.
  LayoutState state(*this);

  m_rowPos.resize(m_grid.size() + 1);

  // We ignore the border-spacing on any non-top section as it is already
  // included in the previous section's last row position.
  if (this == table()->topSection())
    m_rowPos[0] = table()->vBorderSpacing();
  else
    m_rowPos[0] = 0;

  SpanningLayoutTableCells rowSpanCells;

  // At fragmentainer breaks we need to prevent rowspanned cells (and whatever
  // else) from distributing their extra height requirements over the rows that
  // it spans. Otherwise we'd need to refragment afterwards.
  unsigned indexOfFirstStretchableRow = 0;

  for (unsigned r = 0; r < m_grid.size(); r++) {
    m_grid[r].baseline = -1;
    int baselineDescent = 0;

    if (state.isPaginated() && m_grid[r].rowLayoutObject)
      m_rowPos[r] += m_grid[r].rowLayoutObject->paginationStrut().ceil();

    if (m_grid[r].logicalHeight.isSpecified()) {
      // Our base size is the biggest logical height from our cells' styles
      // (excluding row spanning cells).
      m_rowPos[r + 1] = std::max(
          m_rowPos[r] +
              minimumValueForLength(m_grid[r].logicalHeight, LayoutUnit())
                  .round(),
          0);
    } else {
      // Non-specified lengths are ignored because the row already accounts for
      // the cells intrinsic logical height.
      m_rowPos[r + 1] = std::max(m_rowPos[r], 0);
    }

    Row& row = m_grid[r].row;
    unsigned totalCols = row.size();

    for (unsigned c = 0; c < totalCols; c++) {
      CellStruct& current = cellAt(r, c);
      if (current.inColSpan)
        continue;
      for (unsigned i = 0; i < current.cells.size(); i++) {
        cell = current.cells[i];

        // For row spanning cells, we only handle them for the first row they
        // span. This ensures we take their baseline into account.
        if (cell->rowIndex() != r)
          continue;

        if (r < indexOfFirstStretchableRow ||
            (state.isPaginated() &&
             crossesPageBoundary(
                 LayoutUnit(m_rowPos[r]),
                 LayoutUnit(cell->logicalHeightForRowSizing())))) {
          // Entering or extending a range of unstretchable rows. We enter this
          // mode when a cell in a row crosses a fragmentainer boundary, and
          // we'll stay in this mode until we get to a row where we're past all
          // rowspanned cells that we encountered while in this mode.
          DCHECK(state.isPaginated());
          unsigned rowIndexBelowCell = r + cell->rowSpan();
          indexOfFirstStretchableRow =
              std::max(indexOfFirstStretchableRow, rowIndexBelowCell);
        } else if (cell->rowSpan() > 1) {
          DCHECK(!rowSpanCells.contains(cell));
          rowSpanCells.push_back(cell);
        }

        if (cell->hasOverrideLogicalContentHeight()) {
          cell->clearIntrinsicPadding();
          cell->clearOverrideSize();
          cell->forceChildLayout();
        }

        if (cell->rowSpan() == 1)
          m_rowPos[r + 1] = std::max(
              m_rowPos[r + 1], m_rowPos[r] + cell->logicalHeightForRowSizing());

        // Find out the baseline. The baseline is set on the first row in a
        // rowSpan.
        updateBaselineForCell(cell, r, baselineDescent);
      }
    }

    if (r < indexOfFirstStretchableRow && m_grid[r].rowLayoutObject) {
      // We're not allowed to resize this row. Just scratch what we've
      // calculated so far, and use the height that we got during initial
      // layout instead.
      m_rowPos[r + 1] =
          m_rowPos[r] + m_grid[r].rowLayoutObject->logicalHeight().toInt();
    }

    // Add the border-spacing to our final position.
    m_rowPos[r + 1] += borderSpacingForRow(r);
    m_rowPos[r + 1] = std::max(m_rowPos[r + 1], m_rowPos[r]);
  }

  if (!rowSpanCells.isEmpty())
    distributeRowSpanHeightToRows(rowSpanCells);

  ASSERT(!needsLayout());

  return m_rowPos[m_grid.size()];
}

void LayoutTableSection::layout() {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);
  RELEASE_ASSERT(!needsCellRecalc());
  ASSERT(!table()->needsSectionRecalc());

  // addChild may over-grow m_grid but we don't want to throw away the memory
  // too early as addChild can be called in a loop (e.g during parsing). Doing
  // it now ensures we have a stable-enough structure.
  m_grid.shrinkToFit();

  LayoutState state(*this);

  const Vector<int>& columnPos = table()->effectiveColumnPositions();
  LayoutUnit rowLogicalTop;

  SubtreeLayoutScope layouter(*this);
  for (unsigned r = 0; r < m_grid.size(); ++r) {
    Row& row = m_grid[r].row;
    unsigned cols = row.size();
    // First, propagate our table layout's information to the cells. This will
    // mark the row as needing layout if there was a column logical width
    // change.
    for (unsigned startColumn = 0; startColumn < cols; ++startColumn) {
      CellStruct& current = row[startColumn];
      LayoutTableCell* cell = current.primaryCell();
      if (!cell || current.inColSpan)
        continue;

      unsigned endCol = startColumn;
      unsigned cspan = cell->colSpan();
      while (cspan && endCol < cols) {
        ASSERT(endCol < table()->effectiveColumns().size());
        cspan -= table()->effectiveColumns()[endCol].span;
        endCol++;
      }
      int tableLayoutLogicalWidth = columnPos[endCol] - columnPos[startColumn] -
                                    table()->hBorderSpacing();
      cell->setCellLogicalWidth(tableLayoutLogicalWidth, layouter);
    }

    if (LayoutTableRow* rowLayoutObject = m_grid[r].rowLayoutObject) {
      if (state.isPaginated())
        rowLayoutObject->setLogicalTop(rowLogicalTop);
      if (!rowLayoutObject->needsLayout())
        markChildForPaginationRelayoutIfNeeded(*rowLayoutObject, layouter);
      rowLayoutObject->layoutIfNeeded();
      if (state.isPaginated()) {
        adjustRowForPagination(*rowLayoutObject, layouter);
        updateFragmentationInfoForChild(*rowLayoutObject);
        rowLogicalTop = rowLayoutObject->logicalBottom();
        rowLogicalTop += LayoutUnit(table()->vBorderSpacing());
      }
    }
  }

  clearNeedsLayout();
}

void LayoutTableSection::distributeExtraLogicalHeightToPercentRows(
    int& extraLogicalHeight,
    int totalPercent) {
  if (!totalPercent)
    return;

  unsigned totalRows = m_grid.size();
  int totalHeight = m_rowPos[totalRows] + extraLogicalHeight;
  int totalLogicalHeightAdded = 0;
  totalPercent = std::min(totalPercent, 100);
  int rowHeight = m_rowPos[1] - m_rowPos[0];
  for (unsigned r = 0; r < totalRows; ++r) {
    // TODO(alancutter): Make this work correctly for calc lengths.
    if (totalPercent > 0 && m_grid[r].logicalHeight.isPercent()) {
      int toAdd = std::min<int>(
          extraLogicalHeight,
          (totalHeight * m_grid[r].logicalHeight.percent() / 100) - rowHeight);
      // If toAdd is negative, then we don't want to shrink the row (this bug
      // affected Outlook Web Access).
      toAdd = std::max(0, toAdd);
      totalLogicalHeightAdded += toAdd;
      extraLogicalHeight -= toAdd;
      totalPercent -= m_grid[r].logicalHeight.percent();
    }
    ASSERT(totalRows >= 1);
    if (r < totalRows - 1)
      rowHeight = m_rowPos[r + 2] - m_rowPos[r + 1];
    m_rowPos[r + 1] += totalLogicalHeightAdded;
  }
}

void LayoutTableSection::distributeExtraLogicalHeightToAutoRows(
    int& extraLogicalHeight,
    unsigned autoRowsCount) {
  if (!autoRowsCount)
    return;

  int totalLogicalHeightAdded = 0;
  for (unsigned r = 0; r < m_grid.size(); ++r) {
    if (autoRowsCount > 0 && m_grid[r].logicalHeight.isAuto()) {
      // Recomputing |extraLogicalHeightForRow| guarantees that we properly
      // ditribute round |extraLogicalHeight|.
      int extraLogicalHeightForRow = extraLogicalHeight / autoRowsCount;
      totalLogicalHeightAdded += extraLogicalHeightForRow;
      extraLogicalHeight -= extraLogicalHeightForRow;
      --autoRowsCount;
    }
    m_rowPos[r + 1] += totalLogicalHeightAdded;
  }
}

void LayoutTableSection::distributeRemainingExtraLogicalHeight(
    int& extraLogicalHeight) {
  unsigned totalRows = m_grid.size();

  if (extraLogicalHeight <= 0 || !m_rowPos[totalRows])
    return;

  // FIXME: m_rowPos[totalRows] - m_rowPos[0] is the total rows' size.
  int totalRowSize = m_rowPos[totalRows];
  int totalLogicalHeightAdded = 0;
  int previousRowPosition = m_rowPos[0];
  for (unsigned r = 0; r < totalRows; r++) {
    // weight with the original height
    totalLogicalHeightAdded += extraLogicalHeight *
                               (m_rowPos[r + 1] - previousRowPosition) /
                               totalRowSize;
    previousRowPosition = m_rowPos[r + 1];
    m_rowPos[r + 1] += totalLogicalHeightAdded;
  }

  extraLogicalHeight -= totalLogicalHeightAdded;
}

int LayoutTableSection::distributeExtraLogicalHeightToRows(
    int extraLogicalHeight) {
  if (!extraLogicalHeight)
    return extraLogicalHeight;

  unsigned totalRows = m_grid.size();
  if (!totalRows)
    return extraLogicalHeight;

  if (!m_rowPos[totalRows] && nextSibling())
    return extraLogicalHeight;

  unsigned autoRowsCount = 0;
  int totalPercent = 0;
  for (unsigned r = 0; r < totalRows; r++) {
    if (m_grid[r].logicalHeight.isAuto())
      ++autoRowsCount;
    else if (m_grid[r].logicalHeight.isPercent())
      totalPercent += m_grid[r].logicalHeight.percent();
  }

  int remainingExtraLogicalHeight = extraLogicalHeight;
  distributeExtraLogicalHeightToPercentRows(remainingExtraLogicalHeight,
                                            totalPercent);
  distributeExtraLogicalHeightToAutoRows(remainingExtraLogicalHeight,
                                         autoRowsCount);
  distributeRemainingExtraLogicalHeight(remainingExtraLogicalHeight);
  return extraLogicalHeight - remainingExtraLogicalHeight;
}

static bool shouldFlexCellChild(const LayoutTableCell& cell,
                                LayoutObject* cellDescendant) {
  if (!cell.style()->logicalHeight().isSpecified())
    return false;
  if (cellDescendant->style()->overflowY() == EOverflow::Visible ||
      cellDescendant->style()->overflowY() == EOverflow::Hidden)
    return true;
  return cellDescendant->isBox() &&
         toLayoutBox(cellDescendant)->shouldBeConsideredAsReplaced();
}

void LayoutTableSection::layoutRows() {
#if DCHECK_IS_ON()
  SetLayoutNeededForbiddenScope layoutForbiddenScope(*this);
#endif

  ASSERT(!needsLayout());

  LayoutAnalyzer::Scope analyzer(*this);

  // FIXME: Changing the height without a layout can change the overflow so it
  // seems wrong.

  unsigned totalRows = m_grid.size();

  // Set the width of our section now.  The rows will also be this width.
  setLogicalWidth(table()->contentLogicalWidth());

  int vspacing = table()->vBorderSpacing();
  LayoutState state(*this);

  // Set the rows' location and size.
  for (unsigned r = 0; r < totalRows; r++) {
    LayoutTableRow* rowLayoutObject = m_grid[r].rowLayoutObject;
    if (rowLayoutObject) {
      rowLayoutObject->setLogicalLocation(LayoutPoint(0, m_rowPos[r]));
      rowLayoutObject->setLogicalWidth(logicalWidth());
      LayoutUnit rowLogicalHeight(m_rowPos[r + 1] - m_rowPos[r] - vspacing);
      if (state.isPaginated() && r + 1 < totalRows) {
        // If the next row has a pagination strut, we need to subtract it. It
        // should not be included in this row's height.
        if (LayoutTableRow* nextRowObject = m_grid[r + 1].rowLayoutObject)
          rowLogicalHeight -= nextRowObject->paginationStrut();
      }
      rowLayoutObject->setLogicalHeight(rowLogicalHeight);
      rowLayoutObject->updateLayerTransformAfterLayout();
    }
  }

  // Vertically align and flex the cells in each row.
  for (unsigned r = 0; r < totalRows; r++) {
    LayoutTableRow* rowLayoutObject = m_grid[r].rowLayoutObject;

    for (unsigned c = 0; c < numCols(r); c++) {
      CellStruct& cs = cellAt(r, c);
      LayoutTableCell* cell = cs.primaryCell();

      if (!cell || cs.inColSpan)
        continue;

      if (cell->rowIndex() != r)
        continue;  // Rowspanned cells are handled in the first row they occur.

      int rHeight;
      int rowLogicalTop;
      unsigned rowSpan = std::max(1U, cell->rowSpan());
      unsigned endRowIndex = std::min(r + rowSpan, totalRows) - 1;
      LayoutTableRow* lastRowObject = m_grid[endRowIndex].rowLayoutObject;
      if (lastRowObject && rowLayoutObject) {
        rowLogicalTop = rowLayoutObject->logicalTop().toInt();
        rHeight = lastRowObject->logicalBottom().toInt() - rowLogicalTop;
      } else {
        rHeight = m_rowPos[endRowIndex + 1] - m_rowPos[r] - vspacing;
        rowLogicalTop = m_rowPos[r];
      }

      relayoutCellIfFlexed(*cell, r, rHeight);

      SubtreeLayoutScope layouter(*cell);
      EVerticalAlign cellVerticalAlign;
      // If the cell crosses a fragmentainer boundary, just align it at the
      // top. That's how it was laid out initially, before we knew the final
      // row height, and re-aligning it now could result in the cell being
      // fragmented differently, which could change its height and thus violate
      // the requested alignment. Give up instead of risking circular
      // dependencies and unstable layout.
      if (state.isPaginated() &&
          crossesPageBoundary(LayoutUnit(rowLogicalTop), LayoutUnit(rHeight)))
        cellVerticalAlign = EVerticalAlign::Top;
      else
        cellVerticalAlign = cell->style()->verticalAlign();
      cell->computeIntrinsicPadding(rHeight, cellVerticalAlign, layouter);

      LayoutRect oldCellRect = cell->frameRect();

      setLogicalPositionForCell(cell, c);

      cell->layoutIfNeeded();

      LayoutSize childOffset(cell->location() - oldCellRect.location());
      if (childOffset.width() || childOffset.height()) {
        // If the child moved, we have to issue paint invalidations to it as
        // well as any floating/positioned descendants. An exception is if we
        // need a layout. In this case, we know we're going to issue paint
        // invalidations ourselves (and the child) anyway.
        if (!table()->selfNeedsLayout())
          cell->setMayNeedPaintInvalidation();
      }
    }
    if (rowLayoutObject)
      rowLayoutObject->computeOverflow();
  }

  ASSERT(!needsLayout());

  setLogicalHeight(LayoutUnit(m_rowPos[totalRows]));

  computeOverflowFromCells(totalRows, table()->numEffectiveColumns());
}

int LayoutTableSection::paginationStrutForRow(LayoutTableRow* row,
                                              LayoutUnit logicalOffset) const {
  DCHECK(row);
  if (row->getPaginationBreakability() == AllowAnyBreaks)
    return 0;
  LayoutUnit pageLogicalHeight = pageLogicalHeightForOffset(logicalOffset);
  if (!pageLogicalHeight)
    return 0;
  // If the row is too tall for the page don't insert a strut.
  LayoutUnit rowLogicalHeight = row->logicalHeight();
  if (rowLogicalHeight > pageLogicalHeight)
    return 0;

  LayoutUnit remainingLogicalHeight = pageRemainingLogicalHeightForOffset(
      logicalOffset, LayoutBlock::AssociateWithLatterPage);
  if (remainingLogicalHeight >= rowLogicalHeight)
    return 0;  // It fits fine where it is. No need to break.
  LayoutUnit paginationStrut = calculatePaginationStrutToFitContent(
      logicalOffset, remainingLogicalHeight, rowLogicalHeight);
  if (paginationStrut == remainingLogicalHeight &&
      remainingLogicalHeight == pageLogicalHeight) {
    // Don't break if we were at the top of a page, and we failed to fit the
    // content completely. No point in leaving a page completely blank.
    return 0;
  }
  // Table layout parts only work on integers, so we have to round. Round up, to
  // make sure that no fraction ever gets left behind in the previous
  // fragmentainer.
  return paginationStrut.ceil();
}

void LayoutTableSection::computeOverflowFromCells() {
  unsigned totalRows = m_grid.size();
  unsigned nEffCols = table()->numEffectiveColumns();
  computeOverflowFromCells(totalRows, nEffCols);
}

void LayoutTableSection::computeOverflowFromCells(unsigned totalRows,
                                                  unsigned nEffCols) {
  unsigned totalCellsCount = nEffCols * totalRows;
  unsigned maxAllowedOverflowingCellsCount =
      totalCellsCount < gMinTableSizeToUseFastPaintPathWithOverflowingCell
          ? 0
          : gMaxAllowedOverflowingCellRatioForFastPaintPath * totalCellsCount;

  m_overflow.reset();
  m_overflowingCells.clear();
  m_forceSlowPaintPathWithOverflowingCell = false;
#if DCHECK_IS_ON()
  bool hasOverflowingCell = false;
#endif
  // Now that our height has been determined, add in overflow from cells.
  for (unsigned r = 0; r < totalRows; r++) {
    for (unsigned c = 0; c < numCols(r); c++) {
      CellStruct& cs = cellAt(r, c);
      LayoutTableCell* cell = cs.primaryCell();
      if (!cell || cs.inColSpan)
        continue;
      if (r < totalRows - 1 && cell == primaryCellAt(r + 1, c))
        continue;
      addOverflowFromChild(cell);
#if DCHECK_IS_ON()
      hasOverflowingCell |= cell->hasVisualOverflow();
#endif
      if (cell->hasVisualOverflow() &&
          !m_forceSlowPaintPathWithOverflowingCell) {
        m_overflowingCells.add(cell);
        if (m_overflowingCells.size() > maxAllowedOverflowingCellsCount) {
          // We need to set m_forcesSlowPaintPath only if there is a least one
          // overflowing cells as the hit testing code rely on this information.
          m_forceSlowPaintPathWithOverflowingCell = true;
          // The slow path does not make any use of the overflowing cells info,
          // don't hold on to the memory.
          m_overflowingCells.clear();
        }
      }
    }
  }

  ASSERT(hasOverflowingCell == this->hasOverflowingCell());
}

bool LayoutTableSection::recalcChildOverflowAfterStyleChange() {
  ASSERT(childNeedsOverflowRecalcAfterStyleChange());
  clearChildNeedsOverflowRecalcAfterStyleChange();
  unsigned totalRows = m_grid.size();
  bool childrenOverflowChanged = false;
  for (unsigned r = 0; r < totalRows; r++) {
    LayoutTableRow* rowLayouter = rowLayoutObjectAt(r);
    if (!rowLayouter ||
        !rowLayouter->childNeedsOverflowRecalcAfterStyleChange())
      continue;
    rowLayouter->clearChildNeedsOverflowRecalcAfterStyleChange();
    bool rowChildrenOverflowChanged = false;
    for (unsigned c = 0; c < numCols(r); c++) {
      CellStruct& cs = cellAt(r, c);
      LayoutTableCell* cell = cs.primaryCell();
      if (!cell || cs.inColSpan || !cell->needsOverflowRecalcAfterStyleChange())
        continue;
      rowChildrenOverflowChanged |= cell->recalcOverflowAfterStyleChange();
    }
    if (rowChildrenOverflowChanged)
      rowLayouter->computeOverflow();
    childrenOverflowChanged |= rowChildrenOverflowChanged;
  }
  // TODO(crbug.com/604136): Add visual overflow from rows too.
  if (childrenOverflowChanged)
    computeOverflowFromCells(totalRows, table()->numEffectiveColumns());
  return childrenOverflowChanged;
}

void LayoutTableSection::markAllCellsWidthsDirtyAndOrNeedsLayout(
    LayoutTable::WhatToMarkAllCells whatToMark) {
  for (LayoutTableRow* row = firstRow(); row; row = row->nextRow()) {
    for (LayoutTableCell* cell = row->firstCell(); cell;
         cell = cell->nextCell()) {
      cell->setPreferredLogicalWidthsDirty();
      if (whatToMark == LayoutTable::MarkDirtyAndNeedsLayout)
        cell->setChildNeedsLayout();
    }
  }
}

int LayoutTableSection::calcBlockDirectionOuterBorder(
    BlockBorderSide side) const {
  if (!m_grid.size() || !table()->numEffectiveColumns())
    return 0;

  int borderWidth = 0;

  const BorderValue& sb =
      side == BorderBefore ? style()->borderBefore() : style()->borderAfter();
  if (sb.style() == BorderStyleHidden)
    return -1;
  if (sb.style() > BorderStyleHidden)
    borderWidth = sb.width();

  const BorderValue& rb = side == BorderBefore
                              ? firstRow()->style()->borderBefore()
                              : lastRow()->style()->borderAfter();
  if (rb.style() == BorderStyleHidden)
    return -1;
  if (rb.style() > BorderStyleHidden && rb.width() > borderWidth)
    borderWidth = rb.width();

  bool allHidden = true;
  unsigned r = side == BorderBefore ? 0 : m_grid.size() - 1;
  for (unsigned c = 0; c < numCols(r); c++) {
    const CellStruct& current = cellAt(r, c);
    if (current.inColSpan || !current.hasCells())
      continue;
    const ComputedStyle& primaryCellStyle = current.primaryCell()->styleRef();
    // FIXME: Make this work with perpendicular and flipped cells.
    const BorderValue& cb = side == BorderBefore
                                ? primaryCellStyle.borderBefore()
                                : primaryCellStyle.borderAfter();
    // FIXME: Don't repeat for the same col group
    LayoutTableCol* col =
        table()->colElementAtAbsoluteColumn(c).innermostColOrColGroup();
    if (col) {
      const BorderValue& gb = side == BorderBefore
                                  ? col->style()->borderBefore()
                                  : col->style()->borderAfter();
      if (gb.style() == BorderStyleHidden || cb.style() == BorderStyleHidden)
        continue;
      allHidden = false;
      if (gb.style() > BorderStyleHidden && gb.width() > borderWidth)
        borderWidth = gb.width();
      if (cb.style() > BorderStyleHidden && cb.width() > borderWidth)
        borderWidth = cb.width();
    } else {
      if (cb.style() == BorderStyleHidden)
        continue;
      allHidden = false;
      if (cb.style() > BorderStyleHidden && cb.width() > borderWidth)
        borderWidth = cb.width();
    }
  }
  if (allHidden)
    return -1;

  if (side == BorderAfter)
    borderWidth++;  // Distribute rounding error
  return borderWidth / 2;
}

int LayoutTableSection::calcInlineDirectionOuterBorder(
    InlineBorderSide side) const {
  unsigned totalCols = table()->numEffectiveColumns();
  if (!m_grid.size() || !totalCols)
    return 0;
  unsigned colIndex = side == BorderStart ? 0 : totalCols - 1;

  int borderWidth = 0;

  const BorderValue& sb =
      side == BorderStart ? style()->borderStart() : style()->borderEnd();
  if (sb.style() == BorderStyleHidden)
    return -1;
  if (sb.style() > BorderStyleHidden)
    borderWidth = sb.width();

  if (LayoutTableCol* col = table()
                                ->colElementAtAbsoluteColumn(colIndex)
                                .innermostColOrColGroup()) {
    const BorderValue& gb = side == BorderStart ? col->style()->borderStart()
                                                : col->style()->borderEnd();
    if (gb.style() == BorderStyleHidden)
      return -1;
    if (gb.style() > BorderStyleHidden && gb.width() > borderWidth)
      borderWidth = gb.width();
  }

  bool allHidden = true;
  for (unsigned r = 0; r < m_grid.size(); r++) {
    if (colIndex >= numCols(r))
      continue;
    const CellStruct& current = cellAt(r, colIndex);
    if (!current.hasCells())
      continue;
    // FIXME: Don't repeat for the same cell
    const ComputedStyle& primaryCellStyle = current.primaryCell()->styleRef();
    const ComputedStyle& primaryCellParentStyle =
        current.primaryCell()->parent()->styleRef();
    // FIXME: Make this work with perpendicular and flipped cells.
    const BorderValue& cb = side == BorderStart ? primaryCellStyle.borderStart()
                                                : primaryCellStyle.borderEnd();
    const BorderValue& rb = side == BorderStart
                                ? primaryCellParentStyle.borderStart()
                                : primaryCellParentStyle.borderEnd();
    if (cb.style() == BorderStyleHidden || rb.style() == BorderStyleHidden)
      continue;
    allHidden = false;
    if (cb.style() > BorderStyleHidden && cb.width() > borderWidth)
      borderWidth = cb.width();
    if (rb.style() > BorderStyleHidden && rb.width() > borderWidth)
      borderWidth = rb.width();
  }
  if (allHidden)
    return -1;

  if ((side == BorderStart) != table()->style()->isLeftToRightDirection())
    borderWidth++;  // Distribute rounding error
  return borderWidth / 2;
}

void LayoutTableSection::recalcOuterBorder() {
  m_outerBorderBefore = calcBlockDirectionOuterBorder(BorderBefore);
  m_outerBorderAfter = calcBlockDirectionOuterBorder(BorderAfter);
  m_outerBorderStart = calcInlineDirectionOuterBorder(BorderStart);
  m_outerBorderEnd = calcInlineDirectionOuterBorder(BorderEnd);
}

int LayoutTableSection::firstLineBoxBaseline() const {
  if (!m_grid.size())
    return -1;

  int firstLineBaseline = m_grid[0].baseline;
  if (firstLineBaseline >= 0)
    return firstLineBaseline + m_rowPos[0];

  const Row& firstRow = m_grid[0].row;
  for (size_t i = 0; i < firstRow.size(); ++i) {
    const CellStruct& cs = firstRow.at(i);
    const LayoutTableCell* cell = cs.primaryCell();
    if (cell)
      firstLineBaseline =
          std::max<int>(firstLineBaseline,
                        (cell->logicalTop() + cell->borderBefore() +
                         cell->paddingBefore() + cell->contentLogicalHeight())
                            .toInt());
  }

  return firstLineBaseline;
}

void LayoutTableSection::paint(const PaintInfo& paintInfo,
                               const LayoutPoint& paintOffset) const {
  TableSectionPainter(*this).paint(paintInfo, paintOffset);
}

LayoutRect LayoutTableSection::logicalRectForWritingModeAndDirection(
    const LayoutRect& rect) const {
  LayoutRect tableAlignedRect(rect);

  flipForWritingMode(tableAlignedRect);

  if (!style()->isHorizontalWritingMode())
    tableAlignedRect = tableAlignedRect.transposedRect();

  const Vector<int>& columnPos = table()->effectiveColumnPositions();
  // FIXME: The table's direction should determine our row's direction, not the
  // section's (see bug 96691).
  if (!style()->isLeftToRightDirection())
    tableAlignedRect.setX(columnPos[columnPos.size() - 1] -
                          tableAlignedRect.maxX());

  return tableAlignedRect;
}

CellSpan LayoutTableSection::dirtiedRows(const LayoutRect& damageRect) const {
  if (m_forceSlowPaintPathWithOverflowingCell)
    return fullTableRowSpan();

  if (!m_grid.size())
    return CellSpan(0, 0);

  CellSpan coveredRows = spannedRows(damageRect);

  // To issue paint invalidations for the border we might need to paint
  // invalidate the first or last row even if they are not spanned themselves.
  RELEASE_ASSERT(coveredRows.start() < m_rowPos.size());
  if (coveredRows.start() == m_rowPos.size() - 1 &&
      m_rowPos[m_rowPos.size() - 1] + table()->outerBorderAfter() >=
          damageRect.y())
    coveredRows.decreaseStart();

  if (!coveredRows.end() &&
      m_rowPos[0] - table()->outerBorderBefore() <= damageRect.maxY())
    coveredRows.increaseEnd();

  coveredRows.ensureConsistency(m_grid.size());

  return coveredRows;
}

CellSpan LayoutTableSection::dirtiedEffectiveColumns(
    const LayoutRect& damageRect) const {
  if (m_forceSlowPaintPathWithOverflowingCell)
    return fullTableEffectiveColumnSpan();

  RELEASE_ASSERT(table()->numEffectiveColumns());
  CellSpan coveredColumns = spannedEffectiveColumns(damageRect);

  const Vector<int>& columnPos = table()->effectiveColumnPositions();
  // To issue paint invalidations for the border we might need to paint
  // invalidate the first or last column even if they are not spanned
  // themselves.
  RELEASE_ASSERT(coveredColumns.start() < columnPos.size());
  if (coveredColumns.start() == columnPos.size() - 1 &&
      columnPos[columnPos.size() - 1] + table()->outerBorderEnd() >=
          damageRect.x())
    coveredColumns.decreaseStart();

  if (!coveredColumns.end() &&
      columnPos[0] - table()->outerBorderStart() <= damageRect.maxX())
    coveredColumns.increaseEnd();

  coveredColumns.ensureConsistency(table()->numEffectiveColumns());

  return coveredColumns;
}

CellSpan LayoutTableSection::spannedRows(const LayoutRect& flippedRect) const {
  // Find the first row that starts after rect top.
  unsigned nextRow =
      std::upper_bound(m_rowPos.begin(), m_rowPos.end(), flippedRect.y()) -
      m_rowPos.begin();

  // After all rows.
  if (nextRow == m_rowPos.size())
    return CellSpan(m_rowPos.size() - 1, m_rowPos.size() - 1);

  unsigned startRow = nextRow > 0 ? nextRow - 1 : 0;

  // Find the first row that starts after rect bottom.
  unsigned endRow;
  if (m_rowPos[nextRow] >= flippedRect.maxY()) {
    endRow = nextRow;
  } else {
    endRow = std::upper_bound(m_rowPos.begin() + nextRow, m_rowPos.end(),
                              flippedRect.maxY()) -
             m_rowPos.begin();
    if (endRow == m_rowPos.size())
      endRow = m_rowPos.size() - 1;
  }

  return CellSpan(startRow, endRow);
}

CellSpan LayoutTableSection::spannedEffectiveColumns(
    const LayoutRect& flippedRect) const {
  const Vector<int>& columnPos = table()->effectiveColumnPositions();

  // Find the first column that starts after rect left.
  // lower_bound doesn't handle the edge between two cells properly as it would
  // wrongly return the cell on the logical top/left.
  // upper_bound on the other hand properly returns the cell on the logical
  // bottom/right, which also matches the behavior of other browsers.
  unsigned nextColumn =
      std::upper_bound(columnPos.begin(), columnPos.end(), flippedRect.x()) -
      columnPos.begin();

  if (nextColumn == columnPos.size())
    return CellSpan(columnPos.size() - 1,
                    columnPos.size() - 1);  // After all columns.

  unsigned startColumn = nextColumn > 0 ? nextColumn - 1 : 0;

  // Find the first column that starts after rect right.
  unsigned endColumn;
  if (columnPos[nextColumn] >= flippedRect.maxX()) {
    endColumn = nextColumn;
  } else {
    endColumn = std::upper_bound(columnPos.begin() + nextColumn,
                                 columnPos.end(), flippedRect.maxX()) -
                columnPos.begin();
    if (endColumn == columnPos.size())
      endColumn = columnPos.size() - 1;
  }

  return CellSpan(startColumn, endColumn);
}

void LayoutTableSection::recalcCells() {
  ASSERT(m_needsCellRecalc);
  // We reset the flag here to ensure that |addCell| works. This is safe to do
  // as fillRowsWithDefaultStartingAtPosition makes sure we match the table's
  // columns representation.
  m_needsCellRecalc = false;

  m_cCol = 0;
  m_cRow = 0;
  m_grid.clear();

  for (LayoutTableRow* row = firstRow(); row; row = row->nextRow()) {
    unsigned insertionRow = m_cRow;
    ++m_cRow;
    m_cCol = 0;
    ensureRows(m_cRow);

    m_grid[insertionRow].rowLayoutObject = row;
    row->setRowIndex(insertionRow);
    setRowLogicalHeightToRowStyleLogicalHeight(m_grid[insertionRow]);

    for (LayoutTableCell* cell = row->firstCell(); cell;
         cell = cell->nextCell())
      addCell(cell, row);
  }

  m_grid.shrinkToFit();
  setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::Unknown);
}

// FIXME: This function could be made O(1) in certain cases (like for the
// non-most-constrainive cells' case).
void LayoutTableSection::rowLogicalHeightChanged(LayoutTableRow* row) {
  if (needsCellRecalc())
    return;

  unsigned rowIndex = row->rowIndex();
  setRowLogicalHeightToRowStyleLogicalHeight(m_grid[rowIndex]);

  for (LayoutTableCell* cell = m_grid[rowIndex].rowLayoutObject->firstCell();
       cell; cell = cell->nextCell())
    updateLogicalHeightForCell(m_grid[rowIndex], cell);
}

void LayoutTableSection::setNeedsCellRecalc() {
  m_needsCellRecalc = true;
  if (LayoutTable* t = table())
    t->setNeedsSectionRecalc();
}

unsigned LayoutTableSection::numEffectiveColumns() const {
  unsigned result = 0;

  for (unsigned r = 0; r < m_grid.size(); ++r) {
    for (unsigned c = result; c < numCols(r); ++c) {
      const CellStruct& cell = cellAt(r, c);
      if (cell.hasCells() || cell.inColSpan)
        result = c;
    }
  }

  return result + 1;
}

const BorderValue& LayoutTableSection::borderAdjoiningStartCell(
    const LayoutTableCell* cell) const {
  ASSERT(cell->isFirstOrLastCellInRow());
  return hasSameDirectionAs(cell) ? style()->borderStart()
                                  : style()->borderEnd();
}

const BorderValue& LayoutTableSection::borderAdjoiningEndCell(
    const LayoutTableCell* cell) const {
  ASSERT(cell->isFirstOrLastCellInRow());
  return hasSameDirectionAs(cell) ? style()->borderEnd()
                                  : style()->borderStart();
}

const LayoutTableCell* LayoutTableSection::firstRowCellAdjoiningTableStart()
    const {
  unsigned adjoiningStartCellColumnIndex =
      hasSameDirectionAs(table()) ? 0 : table()->lastEffectiveColumnIndex();
  return primaryCellAt(0, adjoiningStartCellColumnIndex);
}

const LayoutTableCell* LayoutTableSection::firstRowCellAdjoiningTableEnd()
    const {
  unsigned adjoiningEndCellColumnIndex =
      hasSameDirectionAs(table()) ? table()->lastEffectiveColumnIndex() : 0;
  return primaryCellAt(0, adjoiningEndCellColumnIndex);
}

void LayoutTableSection::appendEffectiveColumn(unsigned pos) {
  ASSERT(!m_needsCellRecalc);

  for (unsigned row = 0; row < m_grid.size(); ++row)
    m_grid[row].row.resize(pos + 1);
}

void LayoutTableSection::splitEffectiveColumn(unsigned pos, unsigned first) {
  ASSERT(!m_needsCellRecalc);

  if (m_cCol > pos)
    m_cCol++;
  for (unsigned row = 0; row < m_grid.size(); ++row) {
    Row& r = m_grid[row].row;
    ensureCols(row, pos + 2);
    r.insert(pos + 1, CellStruct());
    if (r[pos].hasCells()) {
      r[pos + 1].cells.appendVector(r[pos].cells);
      LayoutTableCell* cell = r[pos].primaryCell();
      ASSERT(cell);
      ASSERT(cell->colSpan() >= (r[pos].inColSpan ? 1u : 0));
      unsigned colleft = cell->colSpan() - r[pos].inColSpan;
      if (first > colleft)
        r[pos + 1].inColSpan = 0;
      else
        r[pos + 1].inColSpan = first + r[pos].inColSpan;
    } else {
      r[pos + 1].inColSpan = 0;
    }
  }
}

// Hit Testing
bool LayoutTableSection::nodeAtPoint(HitTestResult& result,
                                     const HitTestLocation& locationInContainer,
                                     const LayoutPoint& accumulatedOffset,
                                     HitTestAction action) {
  // If we have no children then we have nothing to do.
  if (!firstRow())
    return false;

  // Table sections cannot ever be hit tested.  Effectively they do not exist.
  // Just forward to our children always.
  LayoutPoint adjustedLocation = accumulatedOffset + location();

  if (hasOverflowClip() &&
      !locationInContainer.intersects(overflowClipRect(adjustedLocation)))
    return false;

  if (hasOverflowingCell()) {
    for (LayoutTableRow* row = lastRow(); row; row = row->previousRow()) {
      // FIXME: We have to skip over inline flows, since they can show up inside
      // table rows at the moment (a demoted inline <form> for example). If we
      // ever implement a table-specific hit-test method (which we should do for
      // performance reasons anyway), then we can remove this check.
      if (!row->hasSelfPaintingLayer()) {
        LayoutPoint childPoint =
            flipForWritingModeForChild(row, adjustedLocation);
        if (row->nodeAtPoint(result, locationInContainer, childPoint, action)) {
          updateHitTestResult(
              result, toLayoutPoint(locationInContainer.point() - childPoint));
          return true;
        }
      }
    }
    return false;
  }

  recalcCellsIfNeeded();

  LayoutRect hitTestRect = LayoutRect(locationInContainer.boundingBox());
  hitTestRect.moveBy(-adjustedLocation);

  LayoutRect tableAlignedRect =
      logicalRectForWritingModeAndDirection(hitTestRect);
  CellSpan rowSpan = spannedRows(tableAlignedRect);
  CellSpan columnSpan = spannedEffectiveColumns(tableAlignedRect);

  // Now iterate over the spanned rows and columns.
  for (unsigned hitRow = rowSpan.start(); hitRow < rowSpan.end(); ++hitRow) {
    for (unsigned hitColumn = columnSpan.start(); hitColumn < columnSpan.end();
         ++hitColumn) {
      if (hitColumn >= numCols(hitRow))
        break;

      CellStruct& current = cellAt(hitRow, hitColumn);

      // If the cell is empty, there's nothing to do
      if (!current.hasCells())
        continue;

      for (unsigned i = current.cells.size(); i;) {
        --i;
        LayoutTableCell* cell = current.cells[i];
        LayoutPoint cellPoint =
            flipForWritingModeForChild(cell, adjustedLocation);
        if (static_cast<LayoutObject*>(cell)->nodeAtPoint(
                result, locationInContainer, cellPoint, action)) {
          updateHitTestResult(
              result, locationInContainer.point() - toLayoutSize(cellPoint));
          return true;
        }
      }
      if (!result.hitTestRequest().listBased())
        break;
    }
    if (!result.hitTestRequest().listBased())
      break;
  }

  return false;
}

LayoutTableSection* LayoutTableSection::createAnonymousWithParent(
    const LayoutObject* parent) {
  RefPtr<ComputedStyle> newStyle =
      ComputedStyle::createAnonymousStyleWithDisplay(parent->styleRef(),
                                                     EDisplay::TableRowGroup);
  LayoutTableSection* newSection = new LayoutTableSection(nullptr);
  newSection->setDocumentForAnonymous(&parent->document());
  newSection->setStyle(std::move(newStyle));
  return newSection;
}

void LayoutTableSection::setLogicalPositionForCell(
    LayoutTableCell* cell,
    unsigned effectiveColumn) const {
  LayoutPoint cellLocation(0, m_rowPos[cell->rowIndex()]);
  int horizontalBorderSpacing = table()->hBorderSpacing();

  // FIXME: The table's direction should determine our row's direction, not the
  // section's (see bug 96691).
  if (!style()->isLeftToRightDirection())
    cellLocation.setX(LayoutUnit(
        table()->effectiveColumnPositions()[table()->numEffectiveColumns()] -
        table()->effectiveColumnPositions()
            [table()->absoluteColumnToEffectiveColumn(
                cell->absoluteColumnIndex() + cell->colSpan())] +
        horizontalBorderSpacing));
  else
    cellLocation.setX(
        LayoutUnit(table()->effectiveColumnPositions()[effectiveColumn] +
                   horizontalBorderSpacing));

  cell->setLogicalLocation(cellLocation);
}

void LayoutTableSection::relayoutCellIfFlexed(LayoutTableCell& cell,
                                              int rowIndex,
                                              int rowHeight) {
  // Force percent height children to lay themselves out again.
  // This will cause these children to grow to fill the cell.
  // FIXME: There is still more work to do here to fully match WinIE (should
  // it become necessary to do so).  In quirks mode, WinIE behaves like we
  // do, but it will clip the cells that spill out of the table section.
  // strict mode, Mozilla and WinIE both regrow the table to accommodate the
  // new height of the cell (thus letting the percentages cause growth one
  // time only). We may also not be handling row-spanning cells correctly.
  //
  // Note also the oddity where replaced elements always flex, and yet blocks/
  // tables do not necessarily flex. WinIE is crazy and inconsistent, and we
  // can't hope to match the behavior perfectly, but we'll continue to refine it
  // as we discover new bugs. :)
  bool cellChildrenFlex = false;
  bool flexAllChildren = cell.style()->logicalHeight().isSpecified() ||
                         (!table()->style()->logicalHeight().isAuto() &&
                          rowHeight != cell.logicalHeight());

  for (LayoutObject* child = cell.firstChild(); child;
       child = child->nextSibling()) {
    if (!child->isText() && child->style()->logicalHeight().isPercentOrCalc() &&
        (flexAllChildren || shouldFlexCellChild(cell, child)) &&
        (!child->isTable() || toLayoutTable(child)->hasSections())) {
      cellChildrenFlex = true;
      break;
    }
  }

  if (!cellChildrenFlex) {
    if (TrackedLayoutBoxListHashSet* percentHeightDescendants =
            cell.percentHeightDescendants()) {
      for (auto* descendant : *percentHeightDescendants) {
        if (flexAllChildren || shouldFlexCellChild(cell, descendant)) {
          cellChildrenFlex = true;
          break;
        }
      }
    }
  }

  if (!cellChildrenFlex)
    return;

  // Alignment within a cell is based off the calculated height, which becomes
  // irrelevant once the cell has been resized based off its percentage.
  cell.setOverrideLogicalContentHeightFromRowHeight(LayoutUnit(rowHeight));
  cell.forceChildLayout();

  // If the baseline moved, we may have to update the data for our row. Find
  // out the new baseline.
  if (cell.isBaselineAligned()) {
    int baseline = cell.cellBaselinePosition();
    if (baseline > cell.borderBefore() + cell.paddingBefore())
      m_grid[rowIndex].baseline = std::max(m_grid[rowIndex].baseline, baseline);
  }
}

int LayoutTableSection::logicalHeightForRow(
    const LayoutTableRow& rowObject) const {
  unsigned rowIndex = rowObject.rowIndex();
  DCHECK(rowIndex < m_grid.size());
  int logicalHeight = 0;
  const Row& row = m_grid[rowIndex].row;
  unsigned cols = row.size();
  for (unsigned colIndex = 0; colIndex < cols; colIndex++) {
    const CellStruct& cellStruct = cellAt(rowIndex, colIndex);
    const LayoutTableCell* cell = cellStruct.primaryCell();
    if (!cell || cellStruct.inColSpan)
      continue;
    unsigned rowSpan = cell->rowSpan();
    if (rowSpan == 1) {
      logicalHeight =
          std::max(logicalHeight, cell->logicalHeightForRowSizing());
      continue;
    }
    unsigned rowIndexForCell = cell->rowIndex();
    if (rowIndex == m_grid.size() - 1 ||
        (rowSpan > 1 && rowIndex - rowIndexForCell == rowSpan - 1)) {
      // This is the last row of the rowspanned cell. Add extra height if
      // needed.
      if (LayoutTableRow* firstRowForCell =
              m_grid[rowIndexForCell].rowLayoutObject) {
        int minLogicalHeight = cell->logicalHeightForRowSizing();
        // Subtract space provided by previous rows.
        minLogicalHeight -= rowObject.logicalTop().toInt() -
                            firstRowForCell->logicalTop().toInt();

        logicalHeight = std::max(logicalHeight, minLogicalHeight);
      }
    }
  }

  if (m_grid[rowIndex].logicalHeight.isSpecified()) {
    LayoutUnit specifiedLogicalHeight =
        minimumValueForLength(m_grid[rowIndex].logicalHeight, LayoutUnit());
    logicalHeight = std::max(logicalHeight, specifiedLogicalHeight.toInt());
  }
  return logicalHeight;
}

void LayoutTableSection::adjustRowForPagination(LayoutTableRow& rowObject,
                                                SubtreeLayoutScope& layouter) {
  rowObject.setPaginationStrut(LayoutUnit());
  rowObject.setLogicalHeight(LayoutUnit(logicalHeightForRow(rowObject)));
  int paginationStrut =
      paginationStrutForRow(&rowObject, rowObject.logicalTop());
  bool rowIsAtTopOfColumn = false;
  LayoutUnit offsetFromTopOfPage;
  if (!paginationStrut) {
    LayoutUnit pageLogicalHeight =
        pageLogicalHeightForOffset(rowObject.logicalTop());
    if (pageLogicalHeight && table()->header() &&
        table()->rowOffsetFromRepeatingHeader()) {
      offsetFromTopOfPage =
          pageLogicalHeight -
          pageRemainingLogicalHeightForOffset(rowObject.logicalTop(),
                                              AssociateWithLatterPage);
      rowIsAtTopOfColumn = !offsetFromTopOfPage ||
                           offsetFromTopOfPage <= table()->vBorderSpacing();
    }

    if (!rowIsAtTopOfColumn)
      return;
  }
  // We need to push this row to the next fragmentainer. If there are repeated
  // table headers, we need to make room for those at the top of the next
  // fragmentainer, above this row. Otherwise, this row will just go at the top
  // of the next fragmentainer.

  LayoutTableSection* header = table()->header();
  if (rowObject.isFirstRowInSectionAfterHeader())
    table()->setRowOffsetFromRepeatingHeader(LayoutUnit());
  // Border spacing from the previous row has pushed this row just past the top
  // of the page, so we must reposition it to the top of the page and avoid any
  // repeating header.
  if (rowIsAtTopOfColumn && offsetFromTopOfPage)
    paginationStrut -= offsetFromTopOfPage.toInt();

  // If we have a header group we will paint it at the top of each page,
  // move the rows down to accomodate it.
  if (header)
    paginationStrut += table()->rowOffsetFromRepeatingHeader().toInt();
  rowObject.setPaginationStrut(LayoutUnit(paginationStrut));

  // We have inserted a pagination strut before the row. Adjust the logical top
  // and re-lay out. We no longer want to break inside the row, but rather
  // *before* it. From the previous layout pass, there are most likely
  // pagination struts inside some cell in this row that we need to get rid of.
  rowObject.setLogicalTop(rowObject.logicalTop() + paginationStrut);
  layouter.setChildNeedsLayout(&rowObject);
  rowObject.layoutIfNeeded();

  // It's very likely that re-laying out (and nuking pagination struts inside
  // cells) gave us a new height.
  rowObject.setLogicalHeight(LayoutUnit(logicalHeightForRow(rowObject)));
}

bool LayoutTableSection::isRepeatingHeaderGroup() const {
  if (getPaginationBreakability() == LayoutBox::AllowAnyBreaks)
    return false;
  // TODO(rhogan): Should we paint a header repeatedly if it's self-painting?
  if (hasSelfPaintingLayer())
    return false;
  LayoutUnit pageHeight = table()->pageLogicalHeightForOffset(LayoutUnit());
  if (!pageHeight)
    return false;

  if (logicalHeight() > pageHeight)
    return false;

  // If the first row of the section after the header group doesn't fit on the
  // page, then don't repeat the header on each page.
  // See https://drafts.csswg.org/css-tables-3/#repeated-headers
  LayoutTableSection* sectionBelow = table()->sectionBelow(this);
  if (!sectionBelow)
    return true;
  if (LayoutTableRow* firstRow = sectionBelow->firstRow()) {
    if (firstRow->paginationStrut() || firstRow->logicalHeight() > pageHeight)
      return false;
  }

  return true;
}

bool LayoutTableSection::mapToVisualRectInAncestorSpace(
    const LayoutBoxModelObject* ancestor,
    LayoutRect& rect,
    VisualRectFlags flags) const {
  if (ancestor == this)
    return true;
  // Repeating table headers are painted once per fragmentation page/column.
  // This does not go through the regular fragmentation machinery, so we need
  // special code to expand the invalidation rect to contain all positions of
  // the header in all columns.
  // Note that this is in flow thread coordinates, not visual coordinates. The
  // enclosing LayoutFlowThread will convert to visual coordinates.
  if (table()->header() == this && isRepeatingHeaderGroup())
    rect.setHeight(table()->logicalHeight());
  return LayoutTableBoxComponent::mapToVisualRectInAncestorSpace(ancestor, rect,
                                                                 flags);
}

}  // namespace blink
