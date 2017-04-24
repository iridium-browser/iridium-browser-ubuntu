/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "core/layout/line/InlineBox.h"

#include "core/layout/HitTestLocation.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/fonts/FontMetrics.h"
#include "wtf/allocator/Partitions.h"

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace blink {

class LayoutObject;

struct SameSizeAsInlineBox : DisplayItemClient {
  virtual ~SameSizeAsInlineBox() {}
  uint32_t bitfields;
  void* a[4];
  LayoutPoint b;
  LayoutUnit c;
#if DCHECK_IS_ON()
  bool f;
#endif
};

static_assert(sizeof(InlineBox) == sizeof(SameSizeAsInlineBox),
              "InlineBox should stay small");

#if DCHECK_IS_ON()
InlineBox::~InlineBox() {
  if (!m_hasBadParent && m_parent)
    m_parent->setHasBadChildList();
}
#endif

DISABLE_CFI_PERF
void InlineBox::destroy() {
  // We do not need to issue invalidations if the page is being destroyed
  // since these objects will never be repainted.
  if (!m_lineLayoutItem.documentBeingDestroyed()) {
    setLineLayoutItemShouldDoFullPaintInvalidationIfNeeded();

    // TODO(crbug.com/619630): Make this fast.
    m_lineLayoutItem.slowSetPaintingLayerNeedsRepaint();
  }

  delete this;
}

void InlineBox::remove(MarkLineBoxes markLineBoxes) {
  if (parent())
    parent()->removeChild(this, markLineBoxes);
}

void* InlineBox::operator new(size_t sz) {
  return PartitionAlloc(WTF::Partitions::layoutPartition(), sz,
                        WTF_HEAP_PROFILER_TYPE_NAME(InlineBox));
}

void InlineBox::operator delete(void* ptr) {
  WTF::PartitionFree(ptr);
}

const char* InlineBox::boxName() const {
  return "InlineBox";
}

String InlineBox::debugName() const {
  return boxName();
}

LayoutRect InlineBox::visualRect() const {
  // TODO(chrishtr): tighten these bounds.
  return getLineLayoutItem().visualRect();
}

#ifndef NDEBUG
void InlineBox::showTreeForThis() const {
  getLineLayoutItem().showTreeForThis();
}

void InlineBox::showLineTreeForThis() const {
  getLineLayoutItem().containingBlock().showLineTreeAndMark(this, "*");
}

void InlineBox::showLineTreeAndMark(const InlineBox* markedBox1,
                                    const char* markedLabel1,
                                    const InlineBox* markedBox2,
                                    const char* markedLabel2,
                                    const LayoutObject* obj,
                                    int depth) const {
  int printedCharacters = 0;
  if (this == markedBox1)
    printedCharacters += fprintf(stderr, "%s", markedLabel1);
  if (this == markedBox2)
    printedCharacters += fprintf(stderr, "%s", markedLabel2);
  if (getLineLayoutItem().isEqual(obj))
    printedCharacters += fprintf(stderr, "*");
  for (; printedCharacters < depth * 2; printedCharacters++)
    fputc(' ', stderr);

  showBox(printedCharacters);
}

void InlineBox::showBox(int printedCharacters) const {
  printedCharacters += fprintf(stderr, "%s %p", boxName(), this);
  for (; printedCharacters < showTreeCharacterOffset; printedCharacters++)
    fputc(' ', stderr);
  fprintf(stderr, "\t%s %p {pos=%g,%g size=%g,%g} baseline=%i/%i\n",
          getLineLayoutItem().decoratedName().ascii().data(),
          getLineLayoutItem().debugPointer(), x().toFloat(), y().toFloat(),
          width().toFloat(), height().toFloat(),
          baselinePosition(AlphabeticBaseline),
          baselinePosition(IdeographicBaseline));
}
#endif

LayoutUnit InlineBox::logicalHeight() const {
  if (hasVirtualLogicalHeight())
    return virtualLogicalHeight();

  const SimpleFontData* fontData =
      getLineLayoutItem().style(isFirstLineStyle())->font().primaryFont();
  if (getLineLayoutItem().isText()) {
    DCHECK(fontData);
    return m_bitfields.isText() && fontData
               ? LayoutUnit(fontData->getFontMetrics().height())
               : LayoutUnit();
  }
  if (getLineLayoutItem().isBox() && parent())
    return isHorizontal() ? LineLayoutBox(getLineLayoutItem()).size().height()
                          : LineLayoutBox(getLineLayoutItem()).size().width();

  ASSERT(isInlineFlowBox());
  LineLayoutBoxModel flowObject = boxModelObject();
  DCHECK(fontData);
  LayoutUnit result(fontData ? fontData->getFontMetrics().height() : 0);
  if (parent())
    result += flowObject.borderAndPaddingLogicalHeight();
  return result;
}

int InlineBox::baselinePosition(FontBaseline baselineType) const {
  return boxModelObject().baselinePosition(
      baselineType, m_bitfields.firstLine(),
      isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineBox::lineHeight() const {
  return boxModelObject().lineHeight(
      m_bitfields.firstLine(), isHorizontal() ? HorizontalLine : VerticalLine,
      PositionOnContainingLine);
}

int InlineBox::caretMinOffset() const {
  return getLineLayoutItem().caretMinOffset();
}

int InlineBox::caretMaxOffset() const {
  return getLineLayoutItem().caretMaxOffset();
}

void InlineBox::dirtyLineBoxes() {
  markDirty();
  for (InlineFlowBox* curr = parent(); curr && !curr->isDirty();
       curr = curr->parent())
    curr->markDirty();
}

void InlineBox::deleteLine() {
  if (!m_bitfields.extracted() && getLineLayoutItem().isBox())
    LineLayoutBox(getLineLayoutItem()).setInlineBoxWrapper(nullptr);
  destroy();
}

void InlineBox::extractLine() {
  m_bitfields.setExtracted(true);
  if (getLineLayoutItem().isBox())
    LineLayoutBox(getLineLayoutItem()).setInlineBoxWrapper(nullptr);
}

void InlineBox::attachLine() {
  m_bitfields.setExtracted(false);
  if (getLineLayoutItem().isBox())
    LineLayoutBox(getLineLayoutItem()).setInlineBoxWrapper(this);
}

void InlineBox::move(const LayoutSize& delta) {
  m_location.move(delta);

  if (getLineLayoutItem().isAtomicInlineLevel())
    LineLayoutBox(getLineLayoutItem()).move(delta.width(), delta.height());

  setLineLayoutItemShouldDoFullPaintInvalidationIfNeeded();
}

void InlineBox::paint(const PaintInfo& paintInfo,
                      const LayoutPoint& paintOffset,
                      LayoutUnit /* lineTop */,
                      LayoutUnit /* lineBottom */) const {
  BlockPainter::paintInlineBox(*this, paintInfo, paintOffset);
}

bool InlineBox::nodeAtPoint(HitTestResult& result,
                            const HitTestLocation& locationInContainer,
                            const LayoutPoint& accumulatedOffset,
                            LayoutUnit /* lineTop */,
                            LayoutUnit /* lineBottom */) {
  // Hit test all phases of replaced elements atomically, as though the replaced
  // element established its own stacking context. (See Appendix E.2, section
  // 6.4 on inline block/table elements in the CSS2.1 specification.)
  LayoutPoint childPoint = accumulatedOffset;
  // Faster than calling containingBlock().
  if (parent()->getLineLayoutItem().hasFlippedBlocksWritingMode())
    childPoint =
        getLineLayoutItem().containingBlock().flipForWritingModeForChild(
            LineLayoutBox(getLineLayoutItem()), childPoint);

  return getLineLayoutItem().hitTest(result, locationInContainer, childPoint);
}

const RootInlineBox& InlineBox::root() const {
  if (m_parent)
    return m_parent->root();
  ASSERT(isRootInlineBox());
  return static_cast<const RootInlineBox&>(*this);
}

RootInlineBox& InlineBox::root() {
  if (m_parent)
    return m_parent->root();
  ASSERT(isRootInlineBox());
  return static_cast<RootInlineBox&>(*this);
}

bool InlineBox::nextOnLineExists() const {
  if (!m_bitfields.determinedIfNextOnLineExists()) {
    m_bitfields.setDeterminedIfNextOnLineExists(true);

    if (!parent())
      m_bitfields.setNextOnLineExists(false);
    else if (nextOnLine())
      m_bitfields.setNextOnLineExists(true);
    else
      m_bitfields.setNextOnLineExists(parent()->nextOnLineExists());
  }
  return m_bitfields.nextOnLineExists();
}

InlineBox* InlineBox::nextLeafChild() const {
  InlineBox* leaf = nullptr;
  for (InlineBox* box = nextOnLine(); box && !leaf; box = box->nextOnLine())
    leaf = box->isLeaf() ? box : toInlineFlowBox(box)->firstLeafChild();
  if (!leaf && parent())
    leaf = parent()->nextLeafChild();
  return leaf;
}

InlineBox* InlineBox::prevLeafChild() const {
  InlineBox* leaf = nullptr;
  for (InlineBox* box = prevOnLine(); box && !leaf; box = box->prevOnLine())
    leaf = box->isLeaf() ? box : toInlineFlowBox(box)->lastLeafChild();
  if (!leaf && parent())
    leaf = parent()->prevLeafChild();
  return leaf;
}

InlineBox* InlineBox::nextLeafChildIgnoringLineBreak() const {
  InlineBox* leaf = nextLeafChild();
  return (leaf && leaf->isLineBreak()) ? nullptr : leaf;
}

InlineBox* InlineBox::prevLeafChildIgnoringLineBreak() const {
  InlineBox* leaf = prevLeafChild();
  return (leaf && leaf->isLineBreak()) ? nullptr : leaf;
}

SelectionState InlineBox::getSelectionState() const {
  return getLineLayoutItem().getSelectionState();
}

bool InlineBox::canAccommodateEllipsis(bool ltr,
                                       LayoutUnit blockEdge,
                                       LayoutUnit ellipsisWidth) const {
  // Non-atomic inline-level elements can always accommodate an ellipsis.
  // Skip list markers and try the next box.
  if (!getLineLayoutItem().isAtomicInlineLevel() ||
      getLineLayoutItem().isListMarker())
    return true;

  LayoutRect boxRect(x(), LayoutUnit(), m_logicalWidth, LayoutUnit(10));
  LayoutRect ellipsisRect(ltr ? blockEdge - ellipsisWidth : blockEdge,
                          LayoutUnit(), ellipsisWidth, LayoutUnit(10));
  return !(boxRect.intersects(ellipsisRect));
}

LayoutUnit InlineBox::placeEllipsisBox(bool,
                                       LayoutUnit,
                                       LayoutUnit,
                                       LayoutUnit,
                                       LayoutUnit& truncatedWidth,
                                       bool&,
                                       LayoutUnit) {
  // Use -1 to mean "we didn't set the position."
  truncatedWidth += logicalWidth();
  return LayoutUnit(-1);
}

void InlineBox::clearKnownToHaveNoOverflow() {
  m_bitfields.setKnownToHaveNoOverflow(false);
  if (parent() && parent()->knownToHaveNoOverflow())
    parent()->clearKnownToHaveNoOverflow();
}

LayoutPoint InlineBox::physicalLocation() const {
  LayoutRect rect(location(), size());
  flipForWritingMode(rect);
  return rect.location();
}

void InlineBox::logicalRectToPhysicalRect(LayoutRect& rect) const {
  if (!isHorizontal())
    rect = rect.transposedRect();
  flipForWritingMode(rect);
}

void InlineBox::flipForWritingMode(FloatRect& rect) const {
  if (!UNLIKELY(getLineLayoutItem().hasFlippedBlocksWritingMode()))
    return;
  root().block().flipForWritingMode(rect);
}

FloatPoint InlineBox::flipForWritingMode(const FloatPoint& point) const {
  if (!UNLIKELY(getLineLayoutItem().hasFlippedBlocksWritingMode()))
    return point;
  return root().block().flipForWritingMode(point);
}

void InlineBox::flipForWritingMode(LayoutRect& rect) const {
  if (!UNLIKELY(getLineLayoutItem().hasFlippedBlocksWritingMode()))
    return;
  root().block().flipForWritingMode(rect);
}

LayoutPoint InlineBox::flipForWritingMode(const LayoutPoint& point) const {
  if (!UNLIKELY(getLineLayoutItem().hasFlippedBlocksWritingMode()))
    return point;
  return root().block().flipForWritingMode(point);
}

void InlineBox::setShouldDoFullPaintInvalidationRecursively() {
  getLineLayoutItem().setShouldDoFullPaintInvalidation();
  if (!isInlineFlowBox())
    return;
  for (InlineBox* child = toInlineFlowBox(this)->firstChild(); child;
       child = child->nextOnLine())
    child->setShouldDoFullPaintInvalidationRecursively();
}

void InlineBox::setLineLayoutItemShouldDoFullPaintInvalidationIfNeeded() {
  // For RootInlineBox, we only need to invalidate if it's using the first line
  // style. Otherwise it paints nothing so we don't need to invalidate it.
  if (!isRootInlineBox() || isFirstLineStyle())
    m_lineLayoutItem.setShouldDoFullPaintInvalidation();
}

}  // namespace blink

#ifndef NDEBUG

void showTree(const blink::InlineBox* b) {
  if (b)
    b->showTreeForThis();
  else
    fprintf(stderr, "Cannot showTree for (nil) InlineBox.\n");
}

void showLineTree(const blink::InlineBox* b) {
  if (b)
    b->showLineTreeForThis();
  else
    fprintf(stderr, "Cannot showLineTree for (nil) InlineBox.\n");
}

#endif
