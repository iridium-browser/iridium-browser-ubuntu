/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "core/paint/PaintLayerStackingNode.h"

#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/paint/PaintLayer.h"
#include "public/platform/Platform.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <memory>

namespace blink {

// FIXME: This should not require PaintLayer. There is currently a cycle where
// in order to determine if we isStacked() we have to ask the paint
// layer about some of its state.
PaintLayerStackingNode::PaintLayerStackingNode(PaintLayer* layer)
    : m_layer(layer)
#if DCHECK_IS_ON()
      ,
      m_layerListMutationAllowed(true),
      m_stackingParent(0)
#endif
{
  m_isStacked = layoutObject()->styleRef().isStacked();

  // Non-stacking contexts should have empty z-order lists. As this is already
  // the case, there is no need to dirty / recompute these lists.
  m_zOrderListsDirty = isStackingContext();
}

PaintLayerStackingNode::~PaintLayerStackingNode() {
#if DCHECK_IS_ON()
  if (!layoutObject()->documentBeingDestroyed()) {
    DCHECK(!isInStackingParentZOrderLists());

    updateStackingParentForZOrderLists(0);
  }
#endif
}

// Helper for the sorting of layers by z-index.
static inline bool compareZIndex(PaintLayerStackingNode* first,
                                 PaintLayerStackingNode* second) {
  return first->zIndex() < second->zIndex();
}

PaintLayerCompositor* PaintLayerStackingNode::compositor() const {
  DCHECK(layoutObject()->view());
  return layoutObject()->view()->compositor();
}

void PaintLayerStackingNode::dirtyZOrderLists() {
#if DCHECK_IS_ON()
  DCHECK(m_layerListMutationAllowed);
#endif
  DCHECK(isStackingContext());

#if DCHECK_IS_ON()
  updateStackingParentForZOrderLists(0);
#endif

  if (m_posZOrderList)
    m_posZOrderList->clear();
  if (m_negZOrderList)
    m_negZOrderList->clear();
  m_zOrderListsDirty = true;

  if (!layoutObject()->documentBeingDestroyed())
    compositor()->setNeedsCompositingUpdate(CompositingUpdateRebuildTree);
}

void PaintLayerStackingNode::dirtyStackingContextZOrderLists() {
  if (PaintLayerStackingNode* stackingNode = ancestorStackingContextNode())
    stackingNode->dirtyZOrderLists();
}

void PaintLayerStackingNode::rebuildZOrderLists() {
#if DCHECK_IS_ON()
  DCHECK(m_layerListMutationAllowed);
#endif
  DCHECK(isDirtyStackingContext());

  for (PaintLayer* child = layer()->firstChild(); child;
       child = child->nextSibling())
    child->stackingNode()->collectLayers(m_posZOrderList, m_negZOrderList);

  // Sort the two lists.
  if (m_posZOrderList)
    std::stable_sort(m_posZOrderList->begin(), m_posZOrderList->end(),
                     compareZIndex);

  if (m_negZOrderList)
    std::stable_sort(m_negZOrderList->begin(), m_negZOrderList->end(),
                     compareZIndex);

  // Append layers for top layer elements after normal layer collection, to
  // ensure they are on top regardless of z-indexes.  The layoutObjects of top
  // layer elements are children of the view, sorted in top layer stacking
  // order.
  if (layer()->isRootLayer()) {
    LayoutBlockFlow* rootBlock = layoutObject()->view();
    // If the viewport is paginated, everything (including "top-layer" elements)
    // gets redirected to the flow thread. So that's where we have to look, in
    // that case.
    if (LayoutBlockFlow* multiColumnFlowThread =
            rootBlock->multiColumnFlowThread())
      rootBlock = multiColumnFlowThread;
    for (LayoutObject* child = rootBlock->firstChild(); child;
         child = child->nextSibling()) {
      Element* childElement = (child->node() && child->node()->isElementNode())
                                  ? toElement(child->node())
                                  : 0;
      if (childElement && childElement->isInTopLayer()) {
        PaintLayer* layer = toLayoutBoxModelObject(child)->layer();
        // Create the buffer if it doesn't exist yet.
        if (!m_posZOrderList) {
          m_posZOrderList =
              WTF::wrapUnique(new Vector<PaintLayerStackingNode*>);
        }
        m_posZOrderList->push_back(layer->stackingNode());
      }
    }
  }

#if DCHECK_IS_ON()
  updateStackingParentForZOrderLists(this);
#endif

  m_zOrderListsDirty = false;
}

void PaintLayerStackingNode::collectLayers(
    std::unique_ptr<Vector<PaintLayerStackingNode*>>& posBuffer,
    std::unique_ptr<Vector<PaintLayerStackingNode*>>& negBuffer) {
  if (layer()->isInTopLayer())
    return;

  if (isStacked()) {
    std::unique_ptr<Vector<PaintLayerStackingNode*>>& buffer =
        (zIndex() >= 0) ? posBuffer : negBuffer;
    if (!buffer)
      buffer = WTF::wrapUnique(new Vector<PaintLayerStackingNode*>);
    buffer->push_back(this);
  }

  if (!isStackingContext()) {
    for (PaintLayer* child = layer()->firstChild(); child;
         child = child->nextSibling())
      child->stackingNode()->collectLayers(posBuffer, negBuffer);
  }
}

#if DCHECK_IS_ON()
bool PaintLayerStackingNode::isInStackingParentZOrderLists() const {
  if (!m_stackingParent || m_stackingParent->zOrderListsDirty())
    return false;

  if (m_stackingParent->posZOrderList() &&
      m_stackingParent->posZOrderList()->find(this) != kNotFound)
    return true;

  if (m_stackingParent->negZOrderList() &&
      m_stackingParent->negZOrderList()->find(this) != kNotFound)
    return true;

  return false;
}

void PaintLayerStackingNode::updateStackingParentForZOrderLists(
    PaintLayerStackingNode* stackingParent) {
  if (m_posZOrderList) {
    for (size_t i = 0; i < m_posZOrderList->size(); ++i)
      m_posZOrderList->at(i)->setStackingParent(stackingParent);
  }

  if (m_negZOrderList) {
    for (size_t i = 0; i < m_negZOrderList->size(); ++i)
      m_negZOrderList->at(i)->setStackingParent(stackingParent);
  }
}

#endif

void PaintLayerStackingNode::updateLayerListsIfNeeded() {
  updateZOrderLists();
}

void PaintLayerStackingNode::styleDidChange(const ComputedStyle* oldStyle) {
  bool wasStackingContext = oldStyle ? oldStyle->isStackingContext() : false;
  int oldZIndex = oldStyle ? oldStyle->zIndex() : 0;

  bool isStackingContext = this->isStackingContext();
  bool shouldBeStacked = layoutObject()->styleRef().isStacked();
  if (isStackingContext == wasStackingContext &&
      m_isStacked == shouldBeStacked && oldZIndex == zIndex())
    return;

  dirtyStackingContextZOrderLists();

  if (isStackingContext)
    dirtyZOrderLists();
  else
    clearZOrderLists();

  if (m_isStacked != shouldBeStacked) {
    m_isStacked = shouldBeStacked;
    if (!layoutObject()->documentBeingDestroyed() && !layer()->isRootLayer())
      compositor()->setNeedsCompositingUpdate(CompositingUpdateRebuildTree);
  }
}

PaintLayerStackingNode* PaintLayerStackingNode::ancestorStackingContextNode()
    const {
  for (PaintLayer* ancestor = layer()->parent(); ancestor;
       ancestor = ancestor->parent()) {
    PaintLayerStackingNode* stackingNode = ancestor->stackingNode();
    if (stackingNode->isStackingContext())
      return stackingNode;
  }
  return 0;
}

LayoutBoxModelObject* PaintLayerStackingNode::layoutObject() const {
  return m_layer->layoutObject();
}

}  // namespace blink
