/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009, 2013 Apple Inc.
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

#ifndef LayoutTableCell_h
#define LayoutTableCell_h

#include "core/CoreExport.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTableSection.h"
#include "platform/LengthFunctions.h"
#include <memory>

namespace blink {

static const unsigned unsetColumnIndex = 0x1FFFFFFF;
static const unsigned maxColumnIndex = 0x1FFFFFFE;  // 536,870,910

enum IncludeBorderColorOrNot { DoNotIncludeBorderColor, IncludeBorderColor };

class SubtreeLayoutScope;

// LayoutTableCell is used to represent a table cell (display: table-cell).
//
// Because rows are as tall as the tallest cell, cells need to be aligned into
// the enclosing row space. To achieve this, LayoutTableCell introduces the
// concept of 'intrinsic padding'. Those 2 paddings are used to shift the box
// into the row as follows:
//
//        --------------------------------
//        ^  ^
//        |  |
//        |  |    cell's border before
//        |  |
//        |  v
//        |  ^
//        |  |
//        |  | m_intrinsicPaddingBefore
//        |  |
//        |  v
//        |  -----------------------------
//        |  |                           |
// row    |  |   cell's padding box      |
// height |  |                           |
//        |  -----------------------------
//        |  ^
//        |  |
//        |  | m_intrinsicPaddingAfter
//        |  |
//        |  v
//        |  ^
//        |  |
//        |  |    cell's border after
//        |  |
//        v  v
//        ---------------------------------
//
// Note that this diagram is not impacted by collapsing or separate borders
// (see 'border-collapse').
// Also there is no margin on table cell (or any internal table element).
//
// LayoutTableCell is positioned with respect to the enclosing
// LayoutTableSection. See callers of
// LayoutTableSection::setLogicalPositionForCell() for when it is placed.
class CORE_EXPORT LayoutTableCell final : public LayoutBlockFlow {
 public:
  explicit LayoutTableCell(Element*);

  unsigned colSpan() const {
    if (!m_hasColSpan)
      return 1;
    return parseColSpanFromDOM();
  }
  unsigned rowSpan() const {
    if (!m_hasRowSpan)
      return 1;
    return parseRowSpanFromDOM();
  }

  // Called from HTMLTableCellElement.
  void colSpanOrRowSpanChanged();

  void setAbsoluteColumnIndex(unsigned column) {
    if (UNLIKELY(column > maxColumnIndex))
      CRASH();

    m_absoluteColumnIndex = column;
  }

  bool hasSetAbsoluteColumnIndex() const {
    return m_absoluteColumnIndex != unsetColumnIndex;
  }

  unsigned absoluteColumnIndex() const {
    ASSERT(hasSetAbsoluteColumnIndex());
    return m_absoluteColumnIndex;
  }

  LayoutTableRow* row() const { return toLayoutTableRow(parent()); }
  LayoutTableSection* section() const {
    return toLayoutTableSection(parent()->parent());
  }
  LayoutTable* table() const {
    return toLayoutTable(parent()->parent()->parent());
  }

  LayoutTableCell* previousCell() const;
  LayoutTableCell* nextCell() const;

  unsigned rowIndex() const {
    // This function shouldn't be called on a detached cell.
    ASSERT(row());
    return row()->rowIndex();
  }

  Length styleOrColLogicalWidth() const {
    Length styleWidth = style()->logicalWidth();
    if (!styleWidth.isAuto())
      return styleWidth;
    if (LayoutTableCol* firstColumn =
            table()
                ->colElementAtAbsoluteColumn(absoluteColumnIndex())
                .innermostColOrColGroup())
      return logicalWidthFromColumns(firstColumn, styleWidth);
    return styleWidth;
  }

  int logicalHeightFromStyle() const {
    Length height = style()->logicalHeight();
    int styleLogicalHeight = height.isIntrinsicOrAuto()
                                 ? 0
                                 : valueForLength(height, LayoutUnit()).toInt();

    // In strict mode, box-sizing: content-box do the right thing and actually
    // add in the border and padding.
    // Call computedCSSPadding* directly to avoid including implicitPadding.
    if (!document().inQuirksMode() &&
        style()->boxSizing() != EBoxSizing::kBorderBox) {
      styleLogicalHeight +=
          (computedCSSPaddingBefore() + computedCSSPaddingAfter()).floor() +
          (borderBefore() + borderAfter()).floor();
    }
    return styleLogicalHeight;
  }

  int logicalHeightForRowSizing() const {
    // FIXME: This function does too much work, and is very hot during table
    // layout!
    int adjustedLogicalHeight =
        pixelSnappedLogicalHeight() -
        (intrinsicPaddingBefore() + intrinsicPaddingAfter());
    int styleLogicalHeight = logicalHeightFromStyle();
    return max(styleLogicalHeight, adjustedLogicalHeight);
  }

  void setCellLogicalWidth(int constrainedLogicalWidth, SubtreeLayoutScope&);

  LayoutUnit borderLeft() const override;
  LayoutUnit borderRight() const override;
  LayoutUnit borderTop() const override;
  LayoutUnit borderBottom() const override;
  LayoutUnit borderStart() const override;
  LayoutUnit borderEnd() const override;
  LayoutUnit borderBefore() const override;
  LayoutUnit borderAfter() const override;

  void collectBorderValues(LayoutTable::CollapsedBorderValues&);
  static void sortBorderValues(LayoutTable::CollapsedBorderValues&);

  void layout() override;

  void paint(const PaintInfo&, const LayoutPoint&) const override;

  int cellBaselinePosition() const;
  bool isBaselineAligned() const {
    EVerticalAlign va = style()->verticalAlign();
    return va == EVerticalAlign::kBaseline ||
           va == EVerticalAlign::kTextBottom ||
           va == EVerticalAlign::kTextTop || va == EVerticalAlign::kSuper ||
           va == EVerticalAlign::kSub || va == EVerticalAlign::kLength;
  }

  // Align the cell in the block direction. This is done by calculating an
  // intrinsic padding before and after the cell contents, so that all cells in
  // the row get the same logical height.
  void computeIntrinsicPadding(int rowHeight,
                               EVerticalAlign,
                               SubtreeLayoutScope&);

  void clearIntrinsicPadding() { setIntrinsicPadding(0, 0); }

  int intrinsicPaddingBefore() const { return m_intrinsicPaddingBefore; }
  int intrinsicPaddingAfter() const { return m_intrinsicPaddingAfter; }

  LayoutUnit paddingTop() const override;
  LayoutUnit paddingBottom() const override;
  LayoutUnit paddingLeft() const override;
  LayoutUnit paddingRight() const override;

  // FIXME: For now we just assume the cell has the same block flow direction as
  // the table. It's likely we'll create an extra anonymous LayoutBlock to
  // handle mixing directionality anyway, in which case we can lock the block
  // flow directionality of the cells to the table's directionality.
  LayoutUnit paddingBefore() const override;
  LayoutUnit paddingAfter() const override;

  void setOverrideLogicalContentHeightFromRowHeight(LayoutUnit);

  void scrollbarsChanged(bool horizontalScrollbarChanged,
                         bool verticalScrollbarChanged,
                         ScrollbarChangeContext = Layout) override;

  bool cellWidthChanged() const { return m_cellWidthChanged; }
  void setCellWidthChanged(bool b = true) { m_cellWidthChanged = b; }

  static LayoutTableCell* createAnonymous(Document*);
  static LayoutTableCell* createAnonymousWithParent(const LayoutObject*);
  LayoutBox* createAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override {
    return createAnonymousWithParent(parent);
  }

  // This function is used to unify which table part's style we use for
  // computing direction and writing mode. Writing modes are not allowed on row
  // group and row but direction is. This means we can safely use the same style
  // in all cases to simplify our code.
  // FIXME: Eventually this function should replaced by style() once we support
  // direction on all table parts and writing-mode on cells.
  const ComputedStyle& styleForCellFlow() const { return row()->styleRef(); }

  const BorderValue& borderAdjoiningTableStart() const {
    ASSERT(isFirstOrLastCellInRow());
    if (section()->hasSameDirectionAs(table()))
      return style()->borderStart();

    return style()->borderEnd();
  }

  const BorderValue& borderAdjoiningTableEnd() const {
    ASSERT(isFirstOrLastCellInRow());
    if (section()->hasSameDirectionAs(table()))
      return style()->borderEnd();

    return style()->borderStart();
  }

  const BorderValue& borderAdjoiningCellBefore(const LayoutTableCell* cell) {
    DCHECK_EQ(table()->cellAfter(cell), this);
    // FIXME: https://webkit.org/b/79272 - Add support for mixed directionality
    // at the cell level.
    return style()->borderStart();
  }

  const BorderValue& borderAdjoiningCellAfter(const LayoutTableCell* cell) {
    DCHECK_EQ(table()->cellBefore(cell), this);
    // FIXME: https://webkit.org/b/79272 - Add support for mixed directionality
    // at the cell level.
    return style()->borderEnd();
  }

#if DCHECK_IS_ON()
  bool isFirstOrLastCellInRow() const {
    return !table()->cellAfter(this) || !table()->cellBefore(this);
  }
#endif

  const char* name() const override { return "LayoutTableCell"; }

  bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const override;
  void invalidateDisplayItemClients(PaintInvalidationReason) const override;

  // TODO(wkorman): Consider renaming to more clearly differentiate from
  // CollapsedBorderValue.
  class CollapsedBorderValues : public DisplayItemClient {
   public:
    CollapsedBorderValues(const LayoutTableCell&,
                          const CollapsedBorderValue& startBorder,
                          const CollapsedBorderValue& endBorder,
                          const CollapsedBorderValue& beforeBorder,
                          const CollapsedBorderValue& afterBorder);

    const CollapsedBorderValue& startBorder() const { return m_startBorder; }
    const CollapsedBorderValue& endBorder() const { return m_endBorder; }
    const CollapsedBorderValue& beforeBorder() const { return m_beforeBorder; }
    const CollapsedBorderValue& afterBorder() const { return m_afterBorder; }

    void setCollapsedBorderValues(const CollapsedBorderValues& other);

    // DisplayItemClient methods.
    String debugName() const;
    LayoutRect visualRect() const;

   private:
    const LayoutTableCell& m_layoutTableCell;
    CollapsedBorderValue m_startBorder;
    CollapsedBorderValue m_endBorder;
    CollapsedBorderValue m_beforeBorder;
    CollapsedBorderValue m_afterBorder;
  };

  class RowBackgroundDisplayItemClient : public DisplayItemClient {
   public:
    RowBackgroundDisplayItemClient(const LayoutTableCell&);

    // DisplayItemClient methods.
    String debugName() const;
    LayoutRect visualRect() const;

   private:
    const LayoutTableCell& m_layoutTableCell;
  };

  bool usesCompositedCellDisplayItemClients() const;
  const CollapsedBorderValues* collapsedBorderValues() const {
    return m_collapsedBorderValues.get();
  }
  const DisplayItemClient& backgroundDisplayItemClient() const {
    return m_rowBackgroundDisplayItemClient
               ? static_cast<const DisplayItemClient&>(
                     *m_rowBackgroundDisplayItemClient)
               : *this;
  }

  LayoutRect debugRect() const override;

  void adjustChildDebugRect(LayoutRect&) const override;

  // A table cell's location is relative to its containing section.
  LayoutBox* locationContainer() const override { return section(); }

  void ensureIsReadyForPaintInvalidation() override;

  bool hasLineIfEmpty() const override;

 protected:
  void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
  void computePreferredLogicalWidths() override;

  void addLayerHitTestRects(LayerHitTestRects&,
                            const PaintLayer* currentCompositedLayer,
                            const LayoutPoint& layerOffset,
                            const LayoutRect& containerRect) const override;

 private:
  bool isOfType(LayoutObjectType type) const override {
    return type == LayoutObjectTableCell || LayoutBlockFlow::isOfType(type);
  }

  void willBeRemovedFromTree() override;

  void updateLogicalWidth() override;

  void paintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint&) const override;
  void paintMask(const PaintInfo&, const LayoutPoint&) const override;

  LayoutSize offsetFromContainer(const LayoutObject*) const override;
  LayoutRect localVisualRect() const override;

  LayoutUnit borderHalfLeft(bool outer) const;
  LayoutUnit borderHalfRight(bool outer) const;
  LayoutUnit borderHalfTop(bool outer) const;
  LayoutUnit borderHalfBottom(bool outer) const;

  LayoutUnit borderHalfStart(bool outer) const;
  LayoutUnit borderHalfEnd(bool outer) const;
  LayoutUnit borderHalfBefore(bool outer) const;
  LayoutUnit borderHalfAfter(bool outer) const;

  void setIntrinsicPaddingBefore(int p) { m_intrinsicPaddingBefore = p; }
  void setIntrinsicPaddingAfter(int p) { m_intrinsicPaddingAfter = p; }
  void setIntrinsicPadding(int before, int after) {
    setIntrinsicPaddingBefore(before);
    setIntrinsicPaddingAfter(after);
  }

  bool hasStartBorderAdjoiningTable() const;
  bool hasEndBorderAdjoiningTable() const;

  // Those functions implement the CSS collapsing border conflict
  // resolution algorithm.
  // http://www.w3.org/TR/CSS2/tables.html#border-conflict-resolution
  //
  // The code is pretty complicated as it needs to handle mixed directionality
  // between the table and the different table parts (cell, row, row group,
  // column, column group).
  // TODO(jchaffraix): It should be easier to compute all the borders in
  // physical coordinates. However this is not the design of the current code.
  //
  // Blink's support for mixed directionality is currently partial. We only
  // support the directionality up to |styleForCellFlow|. See comment on the
  // function above for more details.
  // See also https://code.google.com/p/chromium/issues/detail?id=128227 for
  // some history.
  //
  // Those functions are called when the cache (m_collapsedBorders) is
  // invalidated on LayoutTable.
  CollapsedBorderValue computeCollapsedStartBorder(
      IncludeBorderColorOrNot = IncludeBorderColor) const;
  CollapsedBorderValue computeCollapsedEndBorder(
      IncludeBorderColorOrNot = IncludeBorderColor) const;
  CollapsedBorderValue computeCollapsedBeforeBorder(
      IncludeBorderColorOrNot = IncludeBorderColor) const;
  CollapsedBorderValue computeCollapsedAfterBorder(
      IncludeBorderColorOrNot = IncludeBorderColor) const;

  Length logicalWidthFromColumns(LayoutTableCol* firstColForThisCell,
                                 Length widthFromStyle) const;

  void updateColAndRowSpanFlags();

  unsigned parseRowSpanFromDOM() const;
  unsigned parseColSpanFromDOM() const;

  void nextSibling() const = delete;
  void previousSibling() const = delete;

  // Note MSVC will only pack members if they have identical types, hence we use
  // unsigned instead of bool here.
  unsigned m_absoluteColumnIndex : 29;
  unsigned m_cellWidthChanged : 1;
  unsigned m_hasColSpan : 1;
  unsigned m_hasRowSpan : 1;

  // The intrinsic padding.
  // See class comment for what they are.
  //
  // Note: Those fields are using non-subpixel units (int)
  // because we don't do fractional arithmetic on tables.
  int m_intrinsicPaddingBefore;
  int m_intrinsicPaddingAfter;

  std::unique_ptr<CollapsedBorderValues> m_collapsedBorderValues;
  std::unique_ptr<RowBackgroundDisplayItemClient>
      m_rowBackgroundDisplayItemClient;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTableCell, isTableCell());

inline LayoutTableCell* LayoutTableCell::previousCell() const {
  return toLayoutTableCell(LayoutObject::previousSibling());
}

inline LayoutTableCell* LayoutTableCell::nextCell() const {
  return toLayoutTableCell(LayoutObject::nextSibling());
}

inline LayoutTableCell* LayoutTableRow::firstCell() const {
  return toLayoutTableCell(firstChild());
}

inline LayoutTableCell* LayoutTableRow::lastCell() const {
  return toLayoutTableCell(lastChild());
}

}  // namespace blink

#endif  // LayoutTableCell_h
