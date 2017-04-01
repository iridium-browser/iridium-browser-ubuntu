/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) CO. Ltd. All rights reserved.
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

#include "core/layout/svg/line/SVGRootInlineBox.h"

#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGTextLayoutEngine.h"
#include "core/layout/svg/line/SVGInlineFlowBox.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "core/paint/SVGRootInlineBoxPainter.h"

namespace blink {

void SVGRootInlineBox::paint(const PaintInfo& paintInfo,
                             const LayoutPoint& paintOffset,
                             LayoutUnit,
                             LayoutUnit) const {
  SVGRootInlineBoxPainter(*this).paint(paintInfo, paintOffset);
}

void SVGRootInlineBox::markDirty() {
  for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
    child->markDirty();
  RootInlineBox::markDirty();
}

void SVGRootInlineBox::computePerCharacterLayoutInformation() {
  LayoutSVGText& textRoot =
      toLayoutSVGText(*LineLayoutAPIShim::layoutObjectFrom(block()));

  const Vector<LayoutSVGInlineText*>& descendantTextNodes =
      textRoot.descendantTextNodes();
  if (descendantTextNodes.isEmpty())
    return;

  if (textRoot.needsReordering())
    reorderValueLists();

  // Perform SVG text layout phase two (see SVGTextLayoutEngine for details).
  SVGTextLayoutEngine characterLayout(descendantTextNodes);
  characterLayout.layoutCharactersInTextBoxes(this);

  // Perform SVG text layout phase three (see SVGTextChunkBuilder for details).
  characterLayout.finishLayout();

  // Perform SVG text layout phase four
  // Position & resize all SVGInlineText/FlowBoxes in the inline box tree,
  // resize the root box as well as the LayoutSVGText parent block.
  layoutInlineBoxes(*this);

  // Let the HTML block space originate from the local SVG coordinate space.
  LineLayoutBlockFlow parentBlock = block();
  parentBlock.setLocation(LayoutPoint());
  // The width could be any value, but set it so that a line box will mirror
  // within the childRect when its coordinates are converted between physical
  // block direction and flipped block direction, for ease of understanding of
  // flipped coordinates. The height doesn't matter.
  parentBlock.setSize(LayoutSize(x() * 2 + width(), LayoutUnit()));

  setLineTopBottomPositions(logicalTop(), logicalBottom(), logicalTop(),
                            logicalBottom());
}

LayoutRect SVGRootInlineBox::layoutInlineBoxes(InlineBox& box) {
  LayoutRect rect;
  if (box.isSVGInlineTextBox()) {
    rect = toSVGInlineTextBox(box).calculateBoundaries();
  } else {
    for (InlineBox* child = toInlineFlowBox(box).firstChild(); child;
         child = child->nextOnLine())
      rect.unite(layoutInlineBoxes(*child));
  }

  box.setX(rect.x());
  box.setY(rect.y());
  box.setLogicalWidth(box.isHorizontal() ? rect.width() : rect.height());
  LayoutUnit logicalHeight = box.isHorizontal() ? rect.height() : rect.width();
  if (box.isSVGInlineTextBox())
    toSVGInlineTextBox(box).setLogicalHeight(logicalHeight);
  else if (box.isSVGInlineFlowBox())
    toSVGInlineFlowBox(box).setLogicalHeight(logicalHeight);
  else
    toSVGRootInlineBox(box).setLogicalHeight(logicalHeight);

  return rect;
}

InlineBox* SVGRootInlineBox::closestLeafChildForPosition(
    const LayoutPoint& point) {
  InlineBox* firstLeaf = firstLeafChild();
  InlineBox* lastLeaf = lastLeafChild();
  if (firstLeaf == lastLeaf)
    return firstLeaf;

  // FIXME: Check for vertical text!
  InlineBox* closestLeaf = nullptr;
  for (InlineBox* leaf = firstLeaf; leaf; leaf = leaf->nextLeafChild()) {
    if (!leaf->isSVGInlineTextBox())
      continue;
    if (point.y() < leaf->y())
      continue;
    if (point.y() > leaf->y() + leaf->virtualLogicalHeight())
      continue;

    closestLeaf = leaf;
    if (point.x() < leaf->x() + leaf->logicalWidth())
      return leaf;
  }

  return closestLeaf ? closestLeaf : lastLeaf;
}

static inline void swapPositioningValuesInTextBoxes(
    SVGInlineTextBox* firstTextBox,
    SVGInlineTextBox* lastTextBox) {
  LineLayoutSVGInlineText firstTextNode =
      LineLayoutSVGInlineText(firstTextBox->getLineLayoutItem());
  SVGCharacterDataMap& firstCharacterDataMap = firstTextNode.characterDataMap();
  SVGCharacterDataMap::iterator itFirst =
      firstCharacterDataMap.find(firstTextBox->start() + 1);
  if (itFirst == firstCharacterDataMap.end())
    return;
  LineLayoutSVGInlineText lastTextNode =
      LineLayoutSVGInlineText(lastTextBox->getLineLayoutItem());
  SVGCharacterDataMap& lastCharacterDataMap = lastTextNode.characterDataMap();
  SVGCharacterDataMap::iterator itLast =
      lastCharacterDataMap.find(lastTextBox->start() + 1);
  if (itLast == lastCharacterDataMap.end())
    return;
  // We only want to perform the swap if both inline boxes are absolutely
  // positioned.
  std::swap(itFirst->value, itLast->value);
}

static inline void reverseInlineBoxRangeAndValueListsIfNeeded(
    Vector<InlineBox*>::iterator first,
    Vector<InlineBox*>::iterator last) {
  // This is a copy of std::reverse(first, last). It additionally assures
  // that the metrics map within the layoutObjects belonging to the
  // InlineBoxes are reordered as well.
  while (true) {
    if (first == last || first == --last)
      return;

    if ((*last)->isSVGInlineTextBox() && (*first)->isSVGInlineTextBox()) {
      SVGInlineTextBox* firstTextBox = toSVGInlineTextBox(*first);
      SVGInlineTextBox* lastTextBox = toSVGInlineTextBox(*last);

      // Reordering is only necessary for BiDi text that is _absolutely_
      // positioned.
      if (firstTextBox->len() == 1 && firstTextBox->len() == lastTextBox->len())
        swapPositioningValuesInTextBoxes(firstTextBox, lastTextBox);
    }

    InlineBox* temp = *first;
    *first = *last;
    *last = temp;
    ++first;
  }
}

void SVGRootInlineBox::reorderValueLists() {
  Vector<InlineBox*> leafBoxesInLogicalOrder;
  collectLeafBoxesInLogicalOrder(leafBoxesInLogicalOrder,
                                 reverseInlineBoxRangeAndValueListsIfNeeded);
}

bool SVGRootInlineBox::nodeAtPoint(HitTestResult& result,
                                   const HitTestLocation& locationInContainer,
                                   const LayoutPoint& accumulatedOffset,
                                   LayoutUnit lineTop,
                                   LayoutUnit lineBottom) {
  for (InlineBox* leaf = firstLeafChild(); leaf; leaf = leaf->nextLeafChild()) {
    if (!leaf->isSVGInlineTextBox())
      continue;
    if (leaf->nodeAtPoint(result, locationInContainer, accumulatedOffset,
                          lineTop, lineBottom))
      return true;
  }

  return false;
}

}  // namespace blink
