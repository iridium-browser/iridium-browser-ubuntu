/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/layout/LayoutBoxModelObject.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLBodyElement.h"
#include "core/layout/ImageQualityController.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ShadowList.h"
#include "platform/LengthFunctions.h"
#include "platform/geometry/TransformState.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {
inline bool isOutOfFlowPositionedWithImplicitHeight(
    const LayoutBoxModelObject* child) {
  return child->isOutOfFlowPositioned() &&
         !child->style()->logicalTop().isAuto() &&
         !child->style()->logicalBottom().isAuto();
}

StickyPositionScrollingConstraints* stickyConstraintsForLayoutObject(
    const LayoutBoxModelObject* obj,
    const PaintLayer* ancestorOverflowLayer) {
  if (!obj)
    return nullptr;

  PaintLayerScrollableArea* scrollableArea =
      ancestorOverflowLayer->getScrollableArea();
  auto it = scrollableArea->stickyConstraintsMap().find(obj->layer());
  if (it == scrollableArea->stickyConstraintsMap().end())
    return nullptr;

  return &it->value;
}

// Inclusive of |from|, exclusive of |to|.
LayoutBoxModelObject* findFirstStickyBetween(LayoutObject* from,
                                             LayoutObject* to) {
  LayoutObject* maybeStickyAncestor = from;
  while (maybeStickyAncestor && maybeStickyAncestor != to) {
    if (maybeStickyAncestor->isStickyPositioned()) {
      return toLayoutBoxModelObject(maybeStickyAncestor);
    }

    maybeStickyAncestor =
        maybeStickyAncestor->isLayoutInline()
            ? maybeStickyAncestor->containingBlock()
            : toLayoutBox(maybeStickyAncestor)->locationContainer();
  }
  return nullptr;
}
}  // namespace

class FloatStateForStyleChange {
 public:
  static void setWasFloating(LayoutBoxModelObject* boxModelObject,
                             bool wasFloating) {
    s_wasFloating = wasFloating;
    s_boxModelObject = boxModelObject;
  }

  static bool wasFloating(LayoutBoxModelObject* boxModelObject) {
    ASSERT(boxModelObject == s_boxModelObject);
    return s_wasFloating;
  }

 private:
  // Used to store state between styleWillChange and styleDidChange
  static bool s_wasFloating;
  static LayoutBoxModelObject* s_boxModelObject;
};

bool FloatStateForStyleChange::s_wasFloating = false;
LayoutBoxModelObject* FloatStateForStyleChange::s_boxModelObject = nullptr;

// The HashMap for storing continuation pointers.
// The continuation chain is a singly linked list. As such, the HashMap's value
// is the next pointer associated with the key.
typedef HashMap<const LayoutBoxModelObject*, LayoutBoxModelObject*>
    ContinuationMap;
static ContinuationMap* continuationMap = nullptr;

void LayoutBoxModelObject::setSelectionState(SelectionState state) {
  if (state == SelectionInside && getSelectionState() != SelectionNone)
    return;

  if ((state == SelectionStart && getSelectionState() == SelectionEnd) ||
      (state == SelectionEnd && getSelectionState() == SelectionStart))
    LayoutObject::setSelectionState(SelectionBoth);
  else
    LayoutObject::setSelectionState(state);

  // FIXME: We should consider whether it is OK propagating to ancestor
  // LayoutInlines. This is a workaround for http://webkit.org/b/32123
  // The containing block can be null in case of an orphaned tree.
  LayoutBlock* containingBlock = this->containingBlock();
  if (containingBlock && !containingBlock->isLayoutView())
    containingBlock->setSelectionState(state);
}

void LayoutBoxModelObject::contentChanged(ContentChangeType changeType) {
  if (!hasLayer())
    return;

  layer()->contentChanged(changeType);
}

bool LayoutBoxModelObject::hasAcceleratedCompositing() const {
  return view()->compositor()->hasAcceleratedCompositing();
}

LayoutBoxModelObject::LayoutBoxModelObject(ContainerNode* node)
    : LayoutObject(node) {}

bool LayoutBoxModelObject::usesCompositedScrolling() const {
  return hasOverflowClip() && hasLayer() &&
         layer()->getScrollableArea()->usesCompositedScrolling();
}

BackgroundPaintLocation LayoutBoxModelObject::backgroundPaintLocation(
    uint32_t* reasons) const {
  bool hasCustomScrollbars = false;
  // TODO(flackr): Detect opaque custom scrollbars which would cover up a
  // border-box background.
  if (PaintLayerScrollableArea* scrollableArea = getScrollableArea()) {
    if ((scrollableArea->horizontalScrollbar() &&
         scrollableArea->horizontalScrollbar()->isCustomScrollbar()) ||
        (scrollableArea->verticalScrollbar() &&
         scrollableArea->verticalScrollbar()->isCustomScrollbar())) {
      hasCustomScrollbars = true;
    }
  }

  // TODO(flackr): When we correctly clip the scrolling contents layer we can
  // paint locally equivalent backgrounds into it. https://crbug.com/645957
  if (!style()->hasAutoClip())
    return BackgroundPaintInGraphicsLayer;

  // TODO(flackr): Remove this when box shadows are still painted correctly when
  // painting into the composited scrolling contents layer.
  // https://crbug.com/646464
  if (style()->boxShadow()) {
    if (reasons)
      *reasons |= MainThreadScrollingReason::kHasBoxShadowFromNonRootLayer;
    return BackgroundPaintInGraphicsLayer;
  }

  // Assume optimistically that the background can be painted in the scrolling
  // contents until we find otherwise.
  BackgroundPaintLocation paintLocation = BackgroundPaintInScrollingContents;
  const FillLayer* layer = &(style()->backgroundLayers());
  for (; layer; layer = layer->next()) {
    if (layer->attachment() == LocalBackgroundAttachment)
      continue;

    // Solid color layers with an effective background clip of the padding box
    // can be treated as local.
    if (!layer->image() && !layer->next() &&
        resolveColor(CSSPropertyBackgroundColor).alpha() > 0) {
      EFillBox clip = layer->clip();
      if (clip == PaddingFillBox)
        continue;
      // A border box can be treated as a padding box if the border is opaque or
      // there is no border and we don't have custom scrollbars.
      if (clip == BorderFillBox) {
        if (!hasCustomScrollbars &&
            (style()->borderTopWidth() == 0 ||
             !resolveColor(CSSPropertyBorderTopColor).hasAlpha()) &&
            (style()->borderLeftWidth() == 0 ||
             !resolveColor(CSSPropertyBorderLeftColor).hasAlpha()) &&
            (style()->borderRightWidth() == 0 ||
             !resolveColor(CSSPropertyBorderRightColor).hasAlpha()) &&
            (style()->borderBottomWidth() == 0 ||
             !resolveColor(CSSPropertyBorderBottomColor).hasAlpha())) {
          continue;
        }
        // If we have an opaque background color only, we can safely paint it
        // into both the scrolling contents layer and the graphics layer to
        // preserve LCD text.
        if (layer == (&style()->backgroundLayers()) &&
            resolveColor(CSSPropertyBackgroundColor).alpha() < 255)
          return BackgroundPaintInGraphicsLayer;
        paintLocation |= BackgroundPaintInGraphicsLayer;
        continue;
      }
      // A content fill box can be treated as a padding fill box if there is no
      // padding.
      if (clip == ContentFillBox && style()->paddingTop().isZero() &&
          style()->paddingLeft().isZero() && style()->paddingRight().isZero() &&
          style()->paddingBottom().isZero()) {
        continue;
      }
    }
    return BackgroundPaintInGraphicsLayer;
  }
  return paintLocation;
}

LayoutBoxModelObject::~LayoutBoxModelObject() {
  // Our layer should have been destroyed and cleared by now
  ASSERT(!hasLayer());
  ASSERT(!m_layer);
}

void LayoutBoxModelObject::willBeDestroyed() {
  ImageQualityController::remove(*this);

  // A continuation of this LayoutObject should be destroyed at subclasses.
  ASSERT(!continuation());

  if (isPositioned()) {
    // Don't use this->view() because the document's layoutView has been set to
    // 0 during destruction.
    if (LocalFrame* frame = this->frame()) {
      if (FrameView* frameView = frame->view()) {
        if (style()->hasViewportConstrainedPosition())
          frameView->removeViewportConstrainedObject(*this);
      }
    }
  }

  LayoutObject::willBeDestroyed();

  destroyLayer();
}

void LayoutBoxModelObject::styleWillChange(StyleDifference diff,
                                           const ComputedStyle& newStyle) {
  // This object's layer may begin or cease to be a stacking context, in which
  // case the paint invalidation container of this object and descendants may
  // change. Thus we need to invalidate paint eagerly for all such children.
  // PaintLayerCompositor::paintInvalidationOnCompositingChange() doesn't work
  // for the case because we can only see the new paintInvalidationContainer
  // during compositing update.
  if (style() &&
      (style()->isStackingContext() != newStyle.isStackingContext())) {
    // The following disablers are valid because we need to invalidate based on
    // the current status.
    DisableCompositingQueryAsserts compositingDisabler;
    DisablePaintInvalidationStateAsserts paintDisabler;
    ObjectPaintInvalidator(*this)
        .invalidatePaintIncludingNonCompositingDescendants();
  }

  FloatStateForStyleChange::setWasFloating(this, isFloating());

  if (hasLayer() && diff.cssClipChanged()) {
    layer()
        ->clipper(PaintLayer::DoNotUseGeometryMapper)
        .clearClipRectsIncludingDescendants();
  }

  LayoutObject::styleWillChange(diff, newStyle);
}

DISABLE_CFI_PERF
void LayoutBoxModelObject::styleDidChange(StyleDifference diff,
                                          const ComputedStyle* oldStyle) {
  bool hadTransformRelatedProperty = hasTransformRelatedProperty();
  bool hadLayer = hasLayer();
  bool layerWasSelfPainting = hadLayer && layer()->isSelfPaintingLayer();
  bool wasFloatingBeforeStyleChanged =
      FloatStateForStyleChange::wasFloating(this);
  bool wasHorizontalWritingMode = isHorizontalWritingMode();

  LayoutObject::styleDidChange(diff, oldStyle);
  updateFromStyle();

  // When an out-of-flow-positioned element changes its display between block
  // and inline-block, then an incremental layout on the element's containing
  // block lays out the element through LayoutPositionedObjects, which skips
  // laying out the element's parent.
  // The element's parent needs to relayout so that it calls LayoutBlockFlow::
  // setStaticInlinePositionForChild with the out-of-flow-positioned child, so
  // that when it's laid out, its LayoutBox::computePositionedLogicalWidth/
  // Height takes into account its new inline/block position rather than its old
  // block/inline position.
  // Position changes and other types of display changes are handled elsewhere.
  if (oldStyle && isOutOfFlowPositioned() && parent() &&
      (parent() != containingBlock()) &&
      (styleRef().position() == oldStyle->position()) &&
      (styleRef().originalDisplay() != oldStyle->originalDisplay()) &&
      ((styleRef().originalDisplay() == EDisplay::Block) ||
       (styleRef().originalDisplay() == EDisplay::InlineBlock)) &&
      ((oldStyle->originalDisplay() == EDisplay::Block) ||
       (oldStyle->originalDisplay() == EDisplay::InlineBlock)))
    parent()->setNeedsLayout(LayoutInvalidationReason::ChildChanged,
                             MarkContainerChain);

  PaintLayerType type = layerTypeRequired();
  if (type != NoPaintLayer) {
    if (!layer() && layerCreationAllowedForSubtree()) {
      if (wasFloatingBeforeStyleChanged && isFloating())
        setChildNeedsLayout();
      createLayer();
      if (parent() && !needsLayout()) {
        // FIXME: We should call a specialized version of this function.
        layer()->updateLayerPositionsAfterLayout();
      }
    }
  } else if (layer() && layer()->parent()) {
    PaintLayer* parentLayer = layer()->parent();
    // Either a transform wasn't specified or the object doesn't support
    // transforms, so just null out the bit.
    setHasTransformRelatedProperty(false);
    setHasReflection(false);
    layer()->updateFilters(oldStyle, styleRef());
    layer()->updateClipPath(oldStyle, styleRef());
    // Calls destroyLayer() which clears m_layer.
    layer()->removeOnlyThisLayerAfterStyleChange();
    if (wasFloatingBeforeStyleChanged && isFloating())
      setChildNeedsLayout();
    if (hadTransformRelatedProperty) {
      setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::StyleChange);
    }
    if (!needsLayout()) {
      // FIXME: We should call a specialized version of this function.
      parentLayer->updateLayerPositionsAfterLayout();
    }
  }

  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    if ((oldStyle && oldStyle->position() != styleRef().position()) ||
        hadLayer != hasLayer()) {
      // This may affect paint properties of the current object, and descendants
      // even if paint properties of the current object won't change. E.g. the
      // stacking context and/or containing block of descendants may change.
      setSubtreeNeedsPaintPropertyUpdate();
    } else if (hadTransformRelatedProperty != hasTransformRelatedProperty()) {
      // This affects whether to create transform node.
      setNeedsPaintPropertyUpdate();
    }
  }

  if (layer()) {
    layer()->styleDidChange(diff, oldStyle);
    if (hadLayer && layer()->isSelfPaintingLayer() != layerWasSelfPainting)
      setChildNeedsLayout();
  }

  if (oldStyle && wasHorizontalWritingMode != isHorizontalWritingMode()) {
    // Changing the getWritingMode() may change isOrthogonalWritingModeRoot()
    // of children. Make sure all children are marked/unmarked as orthogonal
    // writing-mode roots.
    bool newHorizontalWritingMode = isHorizontalWritingMode();
    for (LayoutObject* child = slowFirstChild(); child;
         child = child->nextSibling()) {
      if (!child->isBox())
        continue;
      if (newHorizontalWritingMode != child->isHorizontalWritingMode())
        toLayoutBox(child)->markOrthogonalWritingModeRoot();
      else
        toLayoutBox(child)->unmarkOrthogonalWritingModeRoot();
    }
  }

  // Fixed-position is painted using transform. In the case that the object
  // gets the same layout after changing position property, although no
  // re-raster (rect-based invalidation) is needed, display items should
  // still update their paint offset.
  if (oldStyle) {
    bool newStyleIsFixedPosition = style()->position() == EPosition::kFixed;
    bool oldStyleIsFixedPosition = oldStyle->position() == EPosition::kFixed;
    if (newStyleIsFixedPosition != oldStyleIsFixedPosition)
      ObjectPaintInvalidator(*this)
          .invalidateDisplayItemClientsIncludingNonCompositingDescendants(
              PaintInvalidationStyleChange);
  }

  // The used style for body background may change due to computed style change
  // on the document element because of background stealing.
  // Refer to backgroundStolenForBeingBody() and
  // http://www.w3.org/TR/css3-background/#body-background for more info.
  if (isDocumentElement()) {
    HTMLBodyElement* body = document().firstBodyElement();
    LayoutObject* bodyLayout = body ? body->layoutObject() : nullptr;
    if (bodyLayout && bodyLayout->isBoxModelObject()) {
      bool newStoleBodyBackground = toLayoutBoxModelObject(bodyLayout)
                                        ->backgroundStolenForBeingBody(style());
      bool oldStoleBodyBackground =
          oldStyle &&
          toLayoutBoxModelObject(bodyLayout)
              ->backgroundStolenForBeingBody(oldStyle);
      if (newStoleBodyBackground != oldStoleBodyBackground &&
          bodyLayout->style() && bodyLayout->style()->hasBackground()) {
        bodyLayout->setShouldDoFullPaintInvalidation();
      }
    }
  }

  if (FrameView* frameView = view()->frameView()) {
    bool newStyleIsViewportConstained =
        style()->position() == EPosition::kFixed;
    bool oldStyleIsViewportConstrained =
        oldStyle && oldStyle->position() == EPosition::kFixed;
    bool newStyleIsSticky = style()->position() == EPosition::kSticky;
    bool oldStyleIsSticky =
        oldStyle && oldStyle->position() == EPosition::kSticky;

    if (newStyleIsSticky != oldStyleIsSticky) {
      if (newStyleIsSticky) {
        // During compositing inputs update we'll have the scroll ancestor
        // without having to walk up the tree and can compute the sticky
        // position constraints then.
        if (layer())
          layer()->setNeedsCompositingInputsUpdate();

        // TODO(pdr): When slimming paint v2 is enabled, we will need to
        // invalidate the scroll paint property subtree for this so main thread
        // scroll reasons are recomputed.
      } else {
        // This may get re-added to viewport constrained objects if the object
        // went from sticky to fixed.
        frameView->removeViewportConstrainedObject(*this);

        // Remove sticky constraints for this layer.
        if (layer()) {
          DisableCompositingQueryAsserts disabler;
          if (const PaintLayer* ancestorOverflowLayer =
                  layer()->ancestorOverflowLayer()) {
            if (PaintLayerScrollableArea* scrollableArea =
                    ancestorOverflowLayer->getScrollableArea())
              scrollableArea->invalidateStickyConstraintsFor(layer());
          }
        }

        // TODO(pdr): When slimming paint v2 is enabled, we will need to
        // invalidate the scroll paint property subtree for this so main thread
        // scroll reasons are recomputed.
      }
    }

    if (newStyleIsViewportConstained != oldStyleIsViewportConstrained) {
      if (newStyleIsViewportConstained && layer())
        frameView->addViewportConstrainedObject(*this);
      else
        frameView->removeViewportConstrainedObject(*this);
    }
  }
}

void LayoutBoxModelObject::invalidateStickyConstraints() {
  PaintLayer* enclosing = enclosingLayer();

  if (PaintLayerScrollableArea* scrollableArea =
          enclosing->getScrollableArea()) {
    scrollableArea->invalidateAllStickyConstraints();
    // If this object doesn't have a layer and its enclosing layer is a scroller
    // then we don't need to invalidate the sticky constraints on the ancestor
    // scroller because the enclosing scroller won't have changed size.
    if (!layer())
      return;
  }

  // This intentionally uses the stale ancestor overflow layer compositing input
  // as if we have saved constraints for this layer they were saved in the
  // previous frame.
  DisableCompositingQueryAsserts disabler;
  if (const PaintLayer* ancestorOverflowLayer =
          enclosing->ancestorOverflowLayer())
    ancestorOverflowLayer->getScrollableArea()
        ->invalidateAllStickyConstraints();
}

void LayoutBoxModelObject::createLayer() {
  ASSERT(!m_layer);
  m_layer = WTF::makeUnique<PaintLayer>(*this);
  setHasLayer(true);
  m_layer->insertOnlyThisLayerAfterStyleChange();
}

void LayoutBoxModelObject::destroyLayer() {
  setHasLayer(false);
  m_layer = nullptr;
}

bool LayoutBoxModelObject::hasSelfPaintingLayer() const {
  return m_layer && m_layer->isSelfPaintingLayer();
}

PaintLayerScrollableArea* LayoutBoxModelObject::getScrollableArea() const {
  return m_layer ? m_layer->getScrollableArea() : 0;
}

void LayoutBoxModelObject::addLayerHitTestRects(
    LayerHitTestRects& rects,
    const PaintLayer* currentLayer,
    const LayoutPoint& layerOffset,
    const LayoutRect& containerRect) const {
  if (hasLayer()) {
    if (isLayoutView()) {
      // LayoutView is handled with a special fast-path, but it needs to know
      // the current layer.
      LayoutObject::addLayerHitTestRects(rects, layer(), LayoutPoint(),
                                         LayoutRect());
    } else {
      // Since a LayoutObject never lives outside it's container Layer, we can
      // switch to marking entire layers instead. This may sometimes mark more
      // than necessary (when a layer is made of disjoint objects) but in
      // practice is a significant performance savings.
      layer()->addLayerHitTestRects(rects);
    }
  } else {
    LayoutObject::addLayerHitTestRects(rects, currentLayer, layerOffset,
                                       containerRect);
  }
}

DISABLE_CFI_PERF
void LayoutBoxModelObject::invalidateTreeIfNeeded(
    const PaintInvalidationState& paintInvalidationState) {
  DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
  ensureIsReadyForPaintInvalidation();

  PaintInvalidationState newPaintInvalidationState(paintInvalidationState,
                                                   *this);
  if (!shouldCheckForPaintInvalidation(newPaintInvalidationState))
    return;

  if (mayNeedPaintInvalidationSubtree())
    newPaintInvalidationState
        .setForceSubtreeInvalidationCheckingWithinContainer();

  ObjectPaintInvalidator paintInvalidator(*this);
  LayoutRect previousVisualRect = visualRect();
  LayoutPoint previousLocation = paintInvalidator.locationInBacking();
  PaintInvalidationReason reason =
      invalidatePaintIfNeeded(newPaintInvalidationState);

  if (previousLocation != paintInvalidator.locationInBacking()) {
    newPaintInvalidationState
        .setForceSubtreeInvalidationCheckingWithinContainer();
  }

  // TODO(wangxianzhu): This is a workaround for crbug.com/490725. We don't have
  // enough saved information to do accurate check of clipping change. Will
  // remove when we remove rect-based paint invalidation.
  if (previousVisualRect != visualRect() &&
      !usesCompositedScrolling()
      // Note that isLayoutView() below becomes unnecessary after the launch of
      // root layer scrolling.
      && (hasOverflowClip() || isLayoutView())) {
    newPaintInvalidationState
        .setForceSubtreeInvalidationRectUpdateWithinContainer();
  }

  newPaintInvalidationState.updateForChildren(reason);
  invalidatePaintOfSubtreesIfNeeded(newPaintInvalidationState);

  clearPaintInvalidationFlags();
}

void LayoutBoxModelObject::addOutlineRectsForNormalChildren(
    Vector<LayoutRect>& rects,
    const LayoutPoint& additionalOffset,
    IncludeBlockVisualOverflowOrNot includeBlockOverflows) const {
  for (LayoutObject* child = slowFirstChild(); child;
       child = child->nextSibling()) {
    // Outlines of out-of-flow positioned descendants are handled in
    // LayoutBlock::addOutlineRects().
    if (child->isOutOfFlowPositioned())
      continue;

    // Outline of an element continuation or anonymous block continuation is
    // added when we iterate the continuation chain.
    // See LayoutBlock::addOutlineRects() and LayoutInline::addOutlineRects().
    if (child->isElementContinuation() ||
        (child->isLayoutBlockFlow() &&
         toLayoutBlockFlow(child)->isAnonymousBlockContinuation()))
      continue;

    addOutlineRectsForDescendant(*child, rects, additionalOffset,
                                 includeBlockOverflows);
  }
}

void LayoutBoxModelObject::addOutlineRectsForDescendant(
    const LayoutObject& descendant,
    Vector<LayoutRect>& rects,
    const LayoutPoint& additionalOffset,
    IncludeBlockVisualOverflowOrNot includeBlockOverflows) const {
  if (descendant.isText() || descendant.isListMarker())
    return;

  if (descendant.hasLayer()) {
    Vector<LayoutRect> layerOutlineRects;
    descendant.addOutlineRects(layerOutlineRects, LayoutPoint(),
                               includeBlockOverflows);
    descendant.localToAncestorRects(layerOutlineRects, this, LayoutPoint(),
                                    additionalOffset);
    rects.appendVector(layerOutlineRects);
    return;
  }

  if (descendant.isBox()) {
    descendant.addOutlineRects(
        rects, additionalOffset + toLayoutBox(descendant).locationOffset(),
        includeBlockOverflows);
    return;
  }

  if (descendant.isLayoutInline()) {
    // As an optimization, an ancestor has added rects for its line boxes
    // covering descendants' line boxes, so descendants don't need to add line
    // boxes again. For example, if the parent is a LayoutBlock, it adds rects
    // for its RootOutlineBoxes which cover the line boxes of this LayoutInline.
    // So the LayoutInline needs to add rects for children and continuations
    // only.
    toLayoutInline(descendant)
        .addOutlineRectsForChildrenAndContinuations(rects, additionalOffset,
                                                    includeBlockOverflows);
    return;
  }

  descendant.addOutlineRects(rects, additionalOffset, includeBlockOverflows);
}

bool LayoutBoxModelObject::hasNonEmptyLayoutSize() const {
  for (const LayoutBoxModelObject* root = this; root;
       root = root->continuation()) {
    for (const LayoutObject* object = root; object;
         object = object->nextInPreOrder(object)) {
      if (object->isBox()) {
        const LayoutBox& box = toLayoutBox(*object);
        if (box.logicalHeight() && box.logicalWidth())
          return true;
      } else if (object->isLayoutInline()) {
        const LayoutInline& layoutInline = toLayoutInline(*object);
        if (!layoutInline.linesBoundingBox().isEmpty())
          return true;
      } else {
        ASSERT(object->isText());
      }
    }
  }
  return false;
}

void LayoutBoxModelObject::absoluteQuadsForSelf(
    Vector<FloatQuad>& quads,
    MapCoordinatesFlags mode) const {
  NOTREACHED();
}

void LayoutBoxModelObject::absoluteQuads(Vector<FloatQuad>& quads,
                                         MapCoordinatesFlags mode) const {
  absoluteQuadsForSelf(quads, mode);

  // Iterate over continuations, avoiding recursion in case there are
  // many of them. See crbug.com/653767.
  for (const LayoutBoxModelObject* continuationObject = this->continuation();
       continuationObject;
       continuationObject = continuationObject->continuation()) {
    DCHECK(continuationObject->isLayoutInline() ||
           (continuationObject->isLayoutBlockFlow() &&
            toLayoutBlockFlow(continuationObject)
                ->isAnonymousBlockContinuation()));
    continuationObject->absoluteQuadsForSelf(quads, mode);
  }
}

void LayoutBoxModelObject::updateFromStyle() {
  const ComputedStyle& styleToUse = styleRef();
  setHasBoxDecorationBackground(styleToUse.hasBoxDecorationBackground());
  setInline(styleToUse.isDisplayInlineType());
  setPositionState(styleToUse.position());
  setHorizontalWritingMode(styleToUse.isHorizontalWritingMode());
}

LayoutBlock* LayoutBoxModelObject::containingBlockForAutoHeightDetection(
    Length logicalHeight) const {
  // For percentage heights: The percentage is calculated with respect to the
  // height of the generated box's containing block. If the height of the
  // containing block is not specified explicitly (i.e., it depends on content
  // height), and this element is not absolutely positioned, the used height is
  // calculated as if 'auto' was specified.
  if (!logicalHeight.isPercentOrCalc() || isOutOfFlowPositioned())
    return nullptr;

  // Anonymous block boxes are ignored when resolving percentage values that
  // would refer to it: the closest non-anonymous ancestor box is used instead.
  LayoutBlock* cb = containingBlock();
  while (cb->isAnonymous())
    cb = cb->containingBlock();

  // Matching LayoutBox::percentageLogicalHeightIsResolvableFromBlock() by
  // ignoring table cell's attribute value, where it says that table cells
  // violate what the CSS spec says to do with heights. Basically we don't care
  // if the cell specified a height or not.
  if (cb->isTableCell())
    return nullptr;

  // Match LayoutBox::availableLogicalHeightUsing by special casing the layout
  // view. The available height is taken from the frame.
  if (cb->isLayoutView())
    return nullptr;

  if (isOutOfFlowPositionedWithImplicitHeight(cb))
    return nullptr;

  return cb;
}

bool LayoutBoxModelObject::hasAutoHeightOrContainingBlockWithAutoHeight()
    const {
  // TODO(rego): Check if we can somehow reuse LayoutBlock::
  // availableLogicalHeightForPercentageComputation() (see crbug.com/635655).
  const LayoutBox* thisBox = isBox() ? toLayoutBox(this) : nullptr;
  Length logicalHeightLength = style()->logicalHeight();
  LayoutBlock* cb = containingBlockForAutoHeightDetection(logicalHeightLength);
  if (logicalHeightLength.isPercentOrCalc() && cb && isBox())
    cb->addPercentHeightDescendant(const_cast<LayoutBox*>(toLayoutBox(this)));
  if (thisBox && thisBox->isFlexItem()) {
    LayoutFlexibleBox& flexBox = toLayoutFlexibleBox(*parent());
    if (flexBox.childLogicalHeightForPercentageResolution(*thisBox) !=
        LayoutUnit(-1))
      return false;
  }
  if (thisBox && thisBox->isGridItem() &&
      thisBox->hasOverrideContainingBlockLogicalHeight())
    return false;
  if (logicalHeightLength.isAuto() &&
      !isOutOfFlowPositionedWithImplicitHeight(this))
    return true;

  if (document().inQuirksMode())
    return false;

  if (cb)
    return !cb->hasDefiniteLogicalHeight();

  return false;
}

LayoutSize LayoutBoxModelObject::relativePositionOffset() const {
  LayoutSize offset = accumulateInFlowPositionOffsets();

  LayoutBlock* containingBlock = this->containingBlock();

  // Objects that shrink to avoid floats normally use available line width when
  // computing containing block width. However in the case of relative
  // positioning using percentages, we can't do this. The offset should always
  // be resolved using the available width of the containing block. Therefore we
  // don't use containingBlockLogicalWidthForContent() here, but instead
  // explicitly call availableWidth on our containing block.
  // https://drafts.csswg.org/css-position-3/#rel-pos
  Optional<LayoutUnit> left;
  Optional<LayoutUnit> right;
  if (!style()->left().isAuto())
    left = valueForLength(style()->left(), containingBlock->availableWidth());
  if (!style()->right().isAuto())
    right = valueForLength(style()->right(), containingBlock->availableWidth());
  if (!left && !right) {
    left = LayoutUnit();
    right = LayoutUnit();
  }
  if (!left)
    left = -right.value();
  if (!right)
    right = -left.value();
  bool isLtr = containingBlock->style()->isLeftToRightDirection();
  WritingMode writingMode = containingBlock->style()->getWritingMode();
  switch (writingMode) {
    case WritingMode::kHorizontalTb:
      if (isLtr)
        offset.expand(left.value(), LayoutUnit());
      else
        offset.setWidth(-right.value());
      break;
    case WritingMode::kVerticalRl:
      offset.setWidth(-right.value());
      break;
    case WritingMode::kVerticalLr:
      offset.expand(left.value(), LayoutUnit());
      break;
  }

  // If the containing block of a relatively positioned element does not specify
  // a height, a percentage top or bottom offset should be resolved as auto.
  // An exception to this is if the containing block has the WinIE quirk where
  // <html> and <body> assume the size of the viewport. In this case, calculate
  // the percent offset based on this height.
  // See <https://bugs.webkit.org/show_bug.cgi?id=26396>.

  Optional<LayoutUnit> top;
  Optional<LayoutUnit> bottom;
  if (!style()->top().isAuto() &&
      (!containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight() ||
       !style()->top().isPercentOrCalc() ||
       containingBlock->stretchesToViewport())) {
    top = valueForLength(style()->top(), containingBlock->availableHeight());
  }
  if (!style()->bottom().isAuto() &&
      (!containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight() ||
       !style()->bottom().isPercentOrCalc() ||
       containingBlock->stretchesToViewport())) {
    bottom =
        valueForLength(style()->bottom(), containingBlock->availableHeight());
  }
  if (!top && !bottom) {
    top = LayoutUnit();
    bottom = LayoutUnit();
  }
  if (!top)
    top = -bottom.value();
  if (!bottom)
    bottom = -top.value();
  switch (writingMode) {
    case WritingMode::kHorizontalTb:
      offset.expand(LayoutUnit(), top.value());
      break;
    case WritingMode::kVerticalRl:
      if (isLtr)
        offset.expand(LayoutUnit(), top.value());
      else
        offset.setHeight(-bottom.value());
      break;
    case WritingMode::kVerticalLr:
      if (isLtr)
        offset.expand(LayoutUnit(), top.value());
      else
        offset.setHeight(-bottom.value());
      break;
  }
  return offset;
}

void LayoutBoxModelObject::updateStickyPositionConstraints() const {
  const FloatSize constrainingSize = computeStickyConstrainingRect().size();

  PaintLayerScrollableArea* scrollableArea =
      layer()->ancestorOverflowLayer()->getScrollableArea();
  StickyPositionScrollingConstraints constraints;
  FloatSize skippedContainersOffset;
  LayoutBlock* containingBlock = this->containingBlock();
  // The location container for boxes is not always the containing block.
  LayoutObject* locationContainer =
      isLayoutInline() ? container() : toLayoutBox(this)->locationContainer();
  // Skip anonymous containing blocks.
  while (containingBlock->isAnonymous()) {
    containingBlock = containingBlock->containingBlock();
  }
  MapCoordinatesFlags flags = IgnoreStickyOffset;
  skippedContainersOffset =
      toFloatSize(locationContainer
                      ->localToAncestorQuadWithoutTransforms(
                          FloatQuad(), containingBlock, flags)
                      .boundingBox()
                      .location());
  LayoutBox* scrollAncestor =
      layer()->ancestorOverflowLayer()->isRootLayer()
          ? nullptr
          : &toLayoutBox(layer()->ancestorOverflowLayer()->layoutObject());

  LayoutUnit maxContainerWidth =
      containingBlock->isLayoutView()
          ? containingBlock->logicalWidth()
          : containingBlock->containingBlockLogicalWidthForContent();
  // Sticky positioned element ignore any override logical width on the
  // containing block, as they don't call containingBlockLogicalWidthForContent.
  // It's unclear whether this is totally fine.
  // Compute the container-relative area within which the sticky element is
  // allowed to move.
  LayoutUnit maxWidth = containingBlock->availableLogicalWidth();

  // Map the containing block to the inner corner of the scroll ancestor without
  // transforms.
  FloatRect scrollContainerRelativePaddingBoxRect(
      containingBlock->layoutOverflowRect());
  FloatSize scrollContainerBorderOffset;
  if (scrollAncestor) {
    scrollContainerBorderOffset =
        FloatSize(scrollAncestor->borderLeft(), scrollAncestor->borderTop());
  }
  if (containingBlock != scrollAncestor) {
    FloatQuad localQuad(FloatRect(containingBlock->paddingBoxRect()));
    scrollContainerRelativePaddingBoxRect =
        containingBlock
            ->localToAncestorQuadWithoutTransforms(localQuad, scrollAncestor,
                                                   flags)
            .boundingBox();

    // The sticky position constraint rects should be independent of the current
    // scroll position, so after mapping we add in the scroll position to get
    // the container's position within the ancestor scroller's unscrolled layout
    // overflow.
    ScrollOffset scrollOffset(
        scrollAncestor
            ? toFloatSize(scrollAncestor->getScrollableArea()->scrollPosition())
            : FloatSize());
    scrollContainerRelativePaddingBoxRect.move(scrollOffset);
  }
  // Remove top-left border offset from overflow scroller.
  scrollContainerRelativePaddingBoxRect.move(-scrollContainerBorderOffset);

  LayoutRect scrollContainerRelativeContainingBlockRect(
      scrollContainerRelativePaddingBoxRect);
  // This is removing the padding of the containing block's overflow rect to get
  // the flow box rectangle and removing the margin of the sticky element to
  // ensure that space between the sticky element and its containing flow box.
  // It is an open issue whether the margin should collapse.
  // See https://www.w3.org/TR/css-position-3/#sticky-pos
  scrollContainerRelativeContainingBlockRect.contractEdges(
      minimumValueForLength(containingBlock->style()->paddingTop(),
                            maxContainerWidth) +
          minimumValueForLength(style()->marginTop(), maxWidth),
      minimumValueForLength(containingBlock->style()->paddingRight(),
                            maxContainerWidth) +
          minimumValueForLength(style()->marginRight(), maxWidth),
      minimumValueForLength(containingBlock->style()->paddingBottom(),
                            maxContainerWidth) +
          minimumValueForLength(style()->marginBottom(), maxWidth),
      minimumValueForLength(containingBlock->style()->paddingLeft(),
                            maxContainerWidth) +
          minimumValueForLength(style()->marginLeft(), maxWidth));

  constraints.setScrollContainerRelativeContainingBlockRect(
      FloatRect(scrollContainerRelativeContainingBlockRect));

  FloatRect stickyBoxRect =
      isLayoutInline() ? FloatRect(toLayoutInline(this)->linesBoundingBox())
                       : FloatRect(toLayoutBox(this)->frameRect());

  FloatRect flippedStickyBoxRect = stickyBoxRect;
  containingBlock->flipForWritingMode(flippedStickyBoxRect);
  FloatPoint stickyLocation =
      flippedStickyBoxRect.location() + skippedContainersOffset;

  // The scrollContainerRelativePaddingBoxRect's position is the padding box so
  // we need to remove the border when finding the position of the sticky box
  // within the scroll ancestor if the container is not our scroll ancestor. If
  // the container is our scroll ancestor, we also need to remove the border
  // box because we want the position from within the scroller border.
  FloatSize containerBorderOffset(containingBlock->borderLeft(),
                                  containingBlock->borderTop());
  stickyLocation -= containerBorderOffset;
  constraints.setScrollContainerRelativeStickyBoxRect(
      FloatRect(scrollContainerRelativePaddingBoxRect.location() +
                    toFloatSize(stickyLocation),
                flippedStickyBoxRect.size()));

  // To correctly compute the offsets, the constraints need to know about any
  // nested position:sticky elements between themselves and their
  // containingBlock, and between the containingBlock and their scrollAncestor.
  //
  // The respective search ranges are [container, containingBlock) and
  // [containingBlock, scrollAncestor).
  constraints.setNearestStickyBoxShiftingStickyBox(
      findFirstStickyBetween(locationContainer, containingBlock));
  // We cannot use |scrollAncestor| here as it disregards the root
  // ancestorOverflowLayer(), which we should include.
  constraints.setNearestStickyBoxShiftingContainingBlock(findFirstStickyBetween(
      containingBlock, &layer()->ancestorOverflowLayer()->layoutObject()));

  // We skip the right or top sticky offset if there is not enough space to
  // honor both the left/right or top/bottom offsets.
  LayoutUnit horizontalOffsets =
      minimumValueForLength(style()->right(),
                            LayoutUnit(constrainingSize.width())) +
      minimumValueForLength(style()->left(),
                            LayoutUnit(constrainingSize.width()));
  bool skipRight = false;
  bool skipLeft = false;
  if (!style()->left().isAuto() && !style()->right().isAuto()) {
    if (horizontalOffsets >
            scrollContainerRelativeContainingBlockRect.width() ||
        horizontalOffsets + scrollContainerRelativeContainingBlockRect.width() >
            constrainingSize.width()) {
      skipRight = style()->isLeftToRightDirection();
      skipLeft = !skipRight;
    }
  }

  if (!style()->left().isAuto() && !skipLeft) {
    constraints.setLeftOffset(minimumValueForLength(
        style()->left(), LayoutUnit(constrainingSize.width())));
    constraints.addAnchorEdge(
        StickyPositionScrollingConstraints::AnchorEdgeLeft);
  }

  if (!style()->right().isAuto() && !skipRight) {
    constraints.setRightOffset(minimumValueForLength(
        style()->right(), LayoutUnit(constrainingSize.width())));
    constraints.addAnchorEdge(
        StickyPositionScrollingConstraints::AnchorEdgeRight);
  }

  bool skipBottom = false;
  // TODO(flackr): Exclude top or bottom edge offset depending on the writing
  // mode when related sections are fixed in spec.
  // See http://lists.w3.org/Archives/Public/www-style/2014May/0286.html
  LayoutUnit verticalOffsets =
      minimumValueForLength(style()->top(),
                            LayoutUnit(constrainingSize.height())) +
      minimumValueForLength(style()->bottom(),
                            LayoutUnit(constrainingSize.height()));
  if (!style()->top().isAuto() && !style()->bottom().isAuto()) {
    if (verticalOffsets > scrollContainerRelativeContainingBlockRect.height() ||
        verticalOffsets + scrollContainerRelativeContainingBlockRect.height() >
            constrainingSize.height()) {
      skipBottom = true;
    }
  }

  if (!style()->top().isAuto()) {
    constraints.setTopOffset(minimumValueForLength(
        style()->top(), LayoutUnit(constrainingSize.height())));
    constraints.addAnchorEdge(
        StickyPositionScrollingConstraints::AnchorEdgeTop);
  }

  if (!style()->bottom().isAuto() && !skipBottom) {
    constraints.setBottomOffset(minimumValueForLength(
        style()->bottom(), LayoutUnit(constrainingSize.height())));
    constraints.addAnchorEdge(
        StickyPositionScrollingConstraints::AnchorEdgeBottom);
  }
  scrollableArea->stickyConstraintsMap().set(layer(), constraints);
}

FloatRect LayoutBoxModelObject::computeStickyConstrainingRect() const {
  if (layer()->ancestorOverflowLayer()->isRootLayer())
    return view()->frameView()->visibleContentRect();

  LayoutBox* enclosingClippingBox =
      layer()->ancestorOverflowLayer()->layoutBox();
  DCHECK(enclosingClippingBox);
  FloatRect constrainingRect;
  constrainingRect =
      FloatRect(enclosingClippingBox->overflowClipRect(LayoutPoint(DoublePoint(
          enclosingClippingBox->getScrollableArea()->scrollPosition()))));
  constrainingRect.move(
      -enclosingClippingBox->borderLeft() + enclosingClippingBox->paddingLeft(),
      -enclosingClippingBox->borderTop() + enclosingClippingBox->paddingTop());
  constrainingRect.contract(
      FloatSize(enclosingClippingBox->paddingLeft() +
                    enclosingClippingBox->paddingRight(),
                enclosingClippingBox->paddingTop() +
                    enclosingClippingBox->paddingBottom()));
  return constrainingRect;
}

LayoutSize LayoutBoxModelObject::stickyPositionOffset() const {
  const PaintLayer* ancestorOverflowLayer = layer()->ancestorOverflowLayer();
  // TODO: Force compositing input update if we ask for offset before
  // compositing inputs have been computed?
  if (!ancestorOverflowLayer)
    return LayoutSize();

  StickyPositionScrollingConstraints* constraints =
      stickyConstraintsForLayoutObject(this, ancestorOverflowLayer);
  if (!constraints)
    return LayoutSize();

  StickyPositionScrollingConstraints* shiftingStickyBoxConstraints =
      stickyConstraintsForLayoutObject(
          constraints->nearestStickyBoxShiftingStickyBox(),
          ancestorOverflowLayer);

  StickyPositionScrollingConstraints* shiftingContainingBlockConstraints =
      stickyConstraintsForLayoutObject(
          constraints->nearestStickyBoxShiftingContainingBlock(),
          ancestorOverflowLayer);

  // The sticky offset is physical, so we can just return the delta computed in
  // absolute coords (though it may be wrong with transforms).
  FloatRect constrainingRect = computeStickyConstrainingRect();
  return LayoutSize(constraints->computeStickyOffset(
      constrainingRect, shiftingStickyBoxConstraints,
      shiftingContainingBlockConstraints));
}

LayoutPoint LayoutBoxModelObject::adjustedPositionRelativeTo(
    const LayoutPoint& startPoint,
    const Element* offsetParent) const {
  // If the element is the HTML body element or doesn't have a parent
  // return 0 and stop this algorithm.
  if (isBody() || !parent())
    return LayoutPoint();

  LayoutPoint referencePoint = startPoint;

  // If the offsetParent is null, return the distance between the canvas origin
  // and the left/top border edge of the element and stop this algorithm.
  if (!offsetParent)
    return referencePoint;

  if (const LayoutBoxModelObject* offsetParentObject =
          offsetParent->layoutBoxModelObject()) {
    if (!isOutOfFlowPositioned()) {
      if (isInFlowPositioned())
        referencePoint.move(offsetForInFlowPosition());

      // Note that we may fail to find |offsetParent| while walking the
      // container chain, if |offsetParent| is an inline split into
      // continuations: <body style="display:inline;" id="offsetParent">
      // <div id="this">
      // This is why we have to do a nullptr check here.
      for (const LayoutObject* current = container();
           current && current->node() != offsetParent;
           current = current->container()) {
        // FIXME: What are we supposed to do inside SVG content?
        referencePoint.move(current->columnOffset(referencePoint));
        if (current->isBox() && !current->isTableRow())
          referencePoint.moveBy(toLayoutBox(current)->physicalLocation());
      }

      if (offsetParentObject->isBox() && offsetParentObject->isBody() &&
          !offsetParentObject->isPositioned()) {
        referencePoint.moveBy(
            toLayoutBox(offsetParentObject)->physicalLocation());
      }
    }

    if (offsetParentObject->isLayoutInline()) {
      const LayoutInline* inlineParent = toLayoutInline(offsetParentObject);

      if (isBox() && style()->position() == EPosition::kAbsolute &&
          inlineParent->isInFlowPositioned()) {
        // Offset for absolute elements with inline parent is a special
        // case in the CSS spec
        referencePoint +=
            inlineParent->offsetForInFlowPositionedInline(*toLayoutBox(this));
      }

      referencePoint -= inlineParent->firstLineBoxTopLeft();
    }

    if (offsetParentObject->isBox() && !offsetParentObject->isBody()) {
      referencePoint.move(-toLayoutBox(offsetParentObject)->borderLeft(),
                          -toLayoutBox(offsetParentObject)->borderTop());
    }
  }

  return referencePoint;
}

LayoutSize LayoutBoxModelObject::offsetForInFlowPosition() const {
  if (isRelPositioned())
    return relativePositionOffset();

  if (isStickyPositioned())
    return stickyPositionOffset();

  return LayoutSize();
}

LayoutUnit LayoutBoxModelObject::offsetLeft(const Element* parent) const {
  // Note that LayoutInline and LayoutBox override this to pass a different
  // startPoint to adjustedPositionRelativeTo.
  return adjustedPositionRelativeTo(LayoutPoint(), parent).x();
}

LayoutUnit LayoutBoxModelObject::offsetTop(const Element* parent) const {
  // Note that LayoutInline and LayoutBox override this to pass a different
  // startPoint to adjustedPositionRelativeTo.
  return adjustedPositionRelativeTo(LayoutPoint(), parent).y();
}

int LayoutBoxModelObject::pixelSnappedOffsetWidth(const Element* parent) const {
  return snapSizeToPixel(offsetWidth(), offsetLeft(parent));
}

int LayoutBoxModelObject::pixelSnappedOffsetHeight(
    const Element* parent) const {
  return snapSizeToPixel(offsetHeight(), offsetTop(parent));
}

LayoutUnit LayoutBoxModelObject::computedCSSPadding(
    const Length& padding) const {
  LayoutUnit w;
  if (padding.isPercentOrCalc())
    w = containingBlockLogicalWidthForContent();
  return minimumValueForLength(padding, w);
}

LayoutUnit LayoutBoxModelObject::containingBlockLogicalWidthForContent() const {
  return containingBlock()->availableLogicalWidth();
}

LayoutBoxModelObject* LayoutBoxModelObject::continuation() const {
  return (!continuationMap) ? nullptr : continuationMap->at(this);
}

void LayoutBoxModelObject::setContinuation(LayoutBoxModelObject* continuation) {
  if (continuation) {
    ASSERT(continuation->isLayoutInline() || continuation->isLayoutBlockFlow());
    if (!continuationMap)
      continuationMap = new ContinuationMap;
    continuationMap->set(this, continuation);
  } else {
    if (continuationMap)
      continuationMap->erase(this);
  }
}

void LayoutBoxModelObject::computeLayerHitTestRects(
    LayerHitTestRects& rects) const {
  LayoutObject::computeLayerHitTestRects(rects);

  // If there is a continuation then we need to consult it here, since this is
  // the root of the tree walk and it wouldn't otherwise get picked up.
  // Continuations should always be siblings in the tree, so any others should
  // get picked up already by the tree walk.
  if (continuation())
    continuation()->computeLayerHitTestRects(rects);
}

LayoutRect LayoutBoxModelObject::localCaretRectForEmptyElement(
    LayoutUnit width,
    LayoutUnit textIndentOffset) {
  DCHECK(!slowFirstChild() || slowFirstChild()->isPseudoElement());

  // FIXME: This does not take into account either :first-line or :first-letter
  // However, as soon as some content is entered, the line boxes will be
  // constructed and this kludge is not called any more. So only the caret size
  // of an empty :first-line'd block is wrong. I think we can live with that.
  const ComputedStyle& currentStyle = firstLineStyleRef();

  enum CaretAlignment { AlignLeft, AlignRight, AlignCenter };

  CaretAlignment alignment = AlignLeft;

  switch (currentStyle.textAlign()) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      break;
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      alignment = AlignCenter;
      break;
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      alignment = AlignRight;
      break;
    case ETextAlign::kJustify:
    case ETextAlign::kStart:
      if (!currentStyle.isLeftToRightDirection())
        alignment = AlignRight;
      break;
    case ETextAlign::kEnd:
      if (currentStyle.isLeftToRightDirection())
        alignment = AlignRight;
      break;
  }

  LayoutUnit x = borderLeft() + paddingLeft();
  LayoutUnit maxX = width - borderRight() - paddingRight();
  LayoutUnit caretWidth = frameView()->caretWidth();

  switch (alignment) {
    case AlignLeft:
      if (currentStyle.isLeftToRightDirection())
        x += textIndentOffset;
      break;
    case AlignCenter:
      x = (x + maxX) / 2;
      if (currentStyle.isLeftToRightDirection())
        x += textIndentOffset / 2;
      else
        x -= textIndentOffset / 2;
      break;
    case AlignRight:
      x = maxX - caretWidth;
      if (!currentStyle.isLeftToRightDirection())
        x -= textIndentOffset;
      break;
  }
  x = std::min(x, (maxX - caretWidth).clampNegativeToZero());

  const Font& font = style()->font();
  const SimpleFontData* fontData = font.primaryFont();
  LayoutUnit height;
  // crbug.com/595692 This check should not be needed but sometimes
  // primaryFont is null.
  if (fontData)
    height = LayoutUnit(fontData->getFontMetrics().height());
  LayoutUnit verticalSpace =
      lineHeight(true, currentStyle.isHorizontalWritingMode() ? HorizontalLine
                                                              : VerticalLine,
                 PositionOfInteriorLineBoxes) -
      height;
  LayoutUnit y = paddingTop() + borderTop() + (verticalSpace / 2);
  return currentStyle.isHorizontalWritingMode()
             ? LayoutRect(x, y, caretWidth, height)
             : LayoutRect(y, x, height, caretWidth);
}

const LayoutObject* LayoutBoxModelObject::pushMappingToContainer(
    const LayoutBoxModelObject* ancestorToStopAt,
    LayoutGeometryMap& geometryMap) const {
  ASSERT(ancestorToStopAt != this);

  AncestorSkipInfo skipInfo(ancestorToStopAt);
  LayoutObject* container = this->container(&skipInfo);
  if (!container)
    return nullptr;

  bool isInline = isLayoutInline();
  bool isFixedPos = !isInline && style()->position() == EPosition::kFixed;
  bool containsFixedPosition = canContainFixedPositionObjects();

  LayoutSize adjustmentForSkippedAncestor;
  if (skipInfo.ancestorSkipped()) {
    // There can't be a transform between paintInvalidationContainer and
    // ancestorToStopAt, because transforms create containers, so it should be
    // safe to just subtract the delta between the ancestor and
    // ancestorToStopAt.
    adjustmentForSkippedAncestor =
        -ancestorToStopAt->offsetFromAncestorContainer(container);
  }

  LayoutSize containerOffset = offsetFromContainer(container);
  bool offsetDependsOnPoint;
  if (isLayoutFlowThread()) {
    containerOffset += columnOffset(LayoutPoint());
    offsetDependsOnPoint = true;
  } else {
    offsetDependsOnPoint =
        container->style()->isFlippedBlocksWritingMode() && container->isBox();
  }

  bool preserve3D = container->style()->preserves3D() || style()->preserves3D();
  GeometryInfoFlags flags = 0;
  if (preserve3D)
    flags |= AccumulatingTransform;
  if (offsetDependsOnPoint)
    flags |= IsNonUniform;
  if (isFixedPos)
    flags |= IsFixedPosition;
  if (containsFixedPosition)
    flags |= ContainsFixedPosition;
  if (shouldUseTransformFromContainer(container)) {
    TransformationMatrix t;
    getTransformFromContainer(container, containerOffset, t);
    t.translateRight(adjustmentForSkippedAncestor.width().toFloat(),
                     adjustmentForSkippedAncestor.height().toFloat());
    geometryMap.push(this, t, flags, LayoutSize());
  } else {
    containerOffset += adjustmentForSkippedAncestor;
    geometryMap.push(this, containerOffset, flags, LayoutSize());
  }

  return skipInfo.ancestorSkipped() ? ancestorToStopAt : container;
}

void LayoutBoxModelObject::moveChildTo(LayoutBoxModelObject* toBoxModelObject,
                                       LayoutObject* child,
                                       LayoutObject* beforeChild,
                                       bool fullRemoveInsert) {
  // We assume that callers have cleared their positioned objects list for child
  // moves (!fullRemoveInsert) so the positioned layoutObject maps don't become
  // stale. It would be too slow to do the map lookup on each call.
  ASSERT(!fullRemoveInsert || !isLayoutBlock() ||
         !toLayoutBlock(this)->hasPositionedObjects());

  ASSERT(this == child->parent());
  ASSERT(!beforeChild || toBoxModelObject == beforeChild->parent());

  // If a child is moving from a block-flow to an inline-flow parent then any
  // floats currently intruding into the child can no longer do so. This can
  // happen if a block becomes floating or out-of-flow and is moved to an
  // anonymous block. Remove all floats from their float-lists immediately as
  // markAllDescendantsWithFloatsForLayout won't attempt to remove floats from
  // parents that have inline-flow if we try later.
  if (child->isLayoutBlockFlow() && toBoxModelObject->childrenInline() &&
      !childrenInline()) {
    toLayoutBlockFlow(child)->removeFloatingObjectsFromDescendants();
    ASSERT(!toLayoutBlockFlow(child)->containsFloats());
  }

  if (fullRemoveInsert && isLayoutBlock() && child->isBox())
    toLayoutBox(child)->removeFromPercentHeightContainer();

  if (fullRemoveInsert && (toBoxModelObject->isLayoutBlock() ||
                           toBoxModelObject->isLayoutInline())) {
    // Takes care of adding the new child correctly if toBlock and fromBlock
    // have different kind of children (block vs inline).
    toBoxModelObject->addChild(virtualChildren()->removeChildNode(this, child),
                               beforeChild);
  } else {
    toBoxModelObject->virtualChildren()->insertChildNode(
        toBoxModelObject,
        virtualChildren()->removeChildNode(this, child, fullRemoveInsert),
        beforeChild, fullRemoveInsert);
  }
}

void LayoutBoxModelObject::moveChildrenTo(
    LayoutBoxModelObject* toBoxModelObject,
    LayoutObject* startChild,
    LayoutObject* endChild,
    LayoutObject* beforeChild,
    bool fullRemoveInsert) {
  // This condition is rarely hit since this function is usually called on
  // anonymous blocks which can no longer carry positioned objects (see r120761)
  // or when fullRemoveInsert is false.
  if (fullRemoveInsert && isLayoutBlock()) {
    LayoutBlock* block = toLayoutBlock(this);
    block->removePositionedObjects(nullptr);
    block->removeFromPercentHeightContainer();
    if (block->isLayoutBlockFlow())
      toLayoutBlockFlow(block)->removeFloatingObjects();
  }

  ASSERT(!beforeChild || toBoxModelObject == beforeChild->parent());
  for (LayoutObject* child = startChild; child && child != endChild;) {
    // Save our next sibling as moveChildTo will clear it.
    LayoutObject* nextSibling = child->nextSibling();
    moveChildTo(toBoxModelObject, child, beforeChild, fullRemoveInsert);
    child = nextSibling;
  }
}

bool LayoutBoxModelObject::backgroundStolenForBeingBody(
    const ComputedStyle* rootElementStyle) const {
  // http://www.w3.org/TR/css3-background/#body-background
  // If the root element is <html> with no background, and a <body> child
  // element exists, the root element steals the first <body> child element's
  // background.
  if (!isBody())
    return false;

  Element* rootElement = document().documentElement();
  if (!isHTMLHtmlElement(rootElement))
    return false;

  if (!rootElementStyle)
    rootElementStyle = rootElement->ensureComputedStyle();
  if (rootElementStyle->hasBackground())
    return false;

  if (node() != document().firstBodyElement())
    return false;

  return true;
}

}  // namespace blink
