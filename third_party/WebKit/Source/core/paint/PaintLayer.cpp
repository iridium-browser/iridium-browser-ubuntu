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

#include "core/paint/PaintLayer.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/css/PseudoStyleRequest.h"
#include "core/dom/Document.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/FragmentainerIterator.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/HitTestingTransformState.h"
#include "core/layout/LayoutFlowThread.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/page/Page.h"
#include "core/page/scrolling/RootScrollerController.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/BoxReflectionUtils.h"
#include "core/paint/FilterEffectBuilder.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "platform/LengthFunctions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/CompositorFilterOperations.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include "wtf/allocator/Partitions.h"
#include "wtf/text/CString.h"

namespace blink {

namespace {

static CompositingQueryMode gCompositingQueryMode =
    CompositingQueriesAreOnlyAllowedInCertainDocumentLifecyclePhases;

struct SameSizeAsPaintLayer : DisplayItemClient {
  int bitFields;
  void* pointers[11];
  LayoutUnit layoutUnits[4];
  IntSize size;
  Persistent<PaintLayerScrollableArea> scrollableArea;
  struct {
    IntSize size;
    void* pointer;
    LayoutRect rect;
  } previousPaintStatus;
};

static_assert(sizeof(PaintLayer) == sizeof(SameSizeAsPaintLayer),
              "PaintLayer should stay small");

bool isReferenceClipPath(const ClipPathOperation* clipOperation) {
  return clipOperation && clipOperation->type() == ClipPathOperation::REFERENCE;
}

}  // namespace

using namespace HTMLNames;

PaintLayerRareData::PaintLayerRareData()
    : enclosingPaginationLayer(nullptr),
      potentialCompositingReasonsFromStyle(CompositingReasonNone),
      compositingReasons(CompositingReasonNone),
      squashingDisallowedReasons(SquashingDisallowedReasonsNone),
      groupedMapping(nullptr) {}

PaintLayerRareData::~PaintLayerRareData() {}

PaintLayer::PaintLayer(LayoutBoxModelObject& layoutObject)
    : m_hasSelfPaintingLayerDescendant(false),
      m_hasSelfPaintingLayerDescendantDirty(false),
      m_isRootLayer(layoutObject.isLayoutView()),
      m_hasVisibleContent(false),
      m_needsDescendantDependentFlagsUpdate(true),
      m_hasVisibleDescendant(false),
#if DCHECK_IS_ON()
      m_needsPositionUpdate(true),
#endif
      m_has3DTransformedDescendant(false),
      m_containsDirtyOverlayScrollbars(false),
      m_needsAncestorDependentCompositingInputsUpdate(true),
      m_childNeedsCompositingInputsUpdate(true),
      m_hasCompositingDescendant(false),
      m_isAllScrollingContentComposited(false),
      m_shouldIsolateCompositedDescendants(false),
      m_lostGroupedMapping(false),
      m_needsRepaint(false),
      m_previousPaintResult(FullyPainted),
      m_needsPaintPhaseDescendantOutlines(false),
      m_previousPaintPhaseDescendantOutlinesWasEmpty(false),
      m_needsPaintPhaseFloat(false),
      m_previousPaintPhaseFloatWasEmpty(false),
      m_needsPaintPhaseDescendantBlockBackgrounds(false),
      m_previousPaintPhaseDescendantBlockBackgroundsWasEmpty(false),
      m_hasDescendantWithClipPath(false),
      m_hasNonIsolatedDescendantWithBlendMode(false),
      m_hasAncestorWithClipPath(false),
      m_hasRootScrollerAsDescendant(false),
      m_selfPaintingStatusChanged(false),
      m_layoutObject(layoutObject),
      m_parent(0),
      m_previous(0),
      m_next(0),
      m_first(0),
      m_last(0),
      m_staticInlinePosition(0),
      m_staticBlockPosition(0),
      m_ancestorOverflowLayer(nullptr) {
  updateStackingNode();

  m_isSelfPaintingLayer = shouldBeSelfPaintingLayer();

  updateScrollableArea();
}

PaintLayer::~PaintLayer() {
  if (m_rareData && m_rareData->resourceInfo) {
    const ComputedStyle& style = layoutObject().styleRef();
    if (style.hasFilter())
      style.filter().removeClient(m_rareData->resourceInfo);
    if (isReferenceClipPath(style.clipPath())) {
      toReferenceClipPathOperation(style.clipPath())
          ->removeClient(m_rareData->resourceInfo);
    }
    m_rareData->resourceInfo->clearLayer();
  }
  if (layoutObject().frame() && layoutObject().frame()->page()) {
    if (ScrollingCoordinator* scrollingCoordinator =
            layoutObject().frame()->page()->scrollingCoordinator())
      scrollingCoordinator->willDestroyLayer(this);
  }

  if (groupedMapping()) {
    DisableCompositingQueryAsserts disabler;
    setGroupedMapping(0, InvalidateLayerAndRemoveFromMapping);
  }

  // Child layers will be deleted by their corresponding layout objects, so
  // we don't need to delete them ourselves.

  clearCompositedLayerMapping(true);

  if (m_scrollableArea)
    m_scrollableArea->dispose();
}

String PaintLayer::debugName() const {
  return layoutObject().debugName();
}

LayoutRect PaintLayer::visualRect() const {
  return m_layoutObject.visualRect();
}

PaintLayerCompositor* PaintLayer::compositor() const {
  if (!layoutObject().view())
    return 0;
  return layoutObject().view()->compositor();
}

void PaintLayer::contentChanged(ContentChangeType changeType) {
  // updateLayerCompositingState will query compositingReasons for accelerated
  // overflow scrolling.  This is tripped by
  // LayoutTests/compositing/content-changed-chicken-egg.html
  DisableCompositingQueryAsserts disabler;

  if (changeType == CanvasChanged)
    compositor()->setNeedsCompositingUpdate(
        CompositingUpdateAfterCompositingInputChange);

  if (changeType == CanvasContextChanged) {
    compositor()->setNeedsCompositingUpdate(
        CompositingUpdateAfterCompositingInputChange);

    // Although we're missing test coverage, we need to call
    // GraphicsLayer::setContentsToPlatformLayer with the new platform
    // layer for this canvas.
    // See http://crbug.com/349195
    if (hasCompositedLayerMapping())
      compositedLayerMapping()->setNeedsGraphicsLayerUpdate(
          GraphicsLayerUpdateSubtree);
  }

  if (CompositedLayerMapping* compositedLayerMapping =
          this->compositedLayerMapping())
    compositedLayerMapping->contentChanged(changeType);
}

bool PaintLayer::paintsWithFilters() const {
  if (!layoutObject().hasFilterInducingProperty())
    return false;

  // https://code.google.com/p/chromium/issues/detail?id=343759
  DisableCompositingQueryAsserts disabler;
  return !compositedLayerMapping() ||
         compositingState() != PaintsIntoOwnBacking;
}

bool PaintLayer::paintsWithBackdropFilters() const {
  if (!layoutObject().hasBackdropFilter())
    return false;

  // https://code.google.com/p/chromium/issues/detail?id=343759
  DisableCompositingQueryAsserts disabler;
  return !compositedLayerMapping() ||
         compositingState() != PaintsIntoOwnBacking;
}

LayoutSize PaintLayer::subpixelAccumulation() const {
  return m_rareData ? m_rareData->subpixelAccumulation : LayoutSize();
}

void PaintLayer::setSubpixelAccumulation(const LayoutSize& size) {
  if (m_rareData || !size.isZero())
    ensureRareData().subpixelAccumulation = size;
}

void PaintLayer::updateLayerPositionsAfterLayout() {
  TRACE_EVENT0("blink,benchmark",
               "PaintLayer::updateLayerPositionsAfterLayout");

  clipper(PaintLayer::DoNotUseGeometryMapper)
      .clearClipRectsIncludingDescendants();
  updateLayerPositionRecursive();

  {
    // FIXME: Remove incremental compositing updates after fixing the
    // chicken/egg issues, https://crbug.com/343756
    DisableCompositingQueryAsserts disabler;
    updatePaginationRecursive(enclosingPaginationLayer());
  }
}

void PaintLayer::updateLayerPositionRecursive() {
  updateLayerPosition();

  for (PaintLayer* child = firstChild(); child; child = child->nextSibling())
    child->updateLayerPositionRecursive();
}

void PaintLayer::updateHasSelfPaintingLayerDescendant() const {
  DCHECK(m_hasSelfPaintingLayerDescendantDirty);

  m_hasSelfPaintingLayerDescendant = false;

  for (PaintLayer* child = firstChild(); child; child = child->nextSibling()) {
    if (child->isSelfPaintingLayer() ||
        child->hasSelfPaintingLayerDescendant()) {
      m_hasSelfPaintingLayerDescendant = true;
      break;
    }
  }

  m_hasSelfPaintingLayerDescendantDirty = false;
}

void PaintLayer::dirtyAncestorChainHasSelfPaintingLayerDescendantStatus() {
  for (PaintLayer* layer = this; layer; layer = layer->parent()) {
    layer->m_hasSelfPaintingLayerDescendantDirty = true;
    // If we have reached a self-painting layer, we know our parent should have
    // a self-painting descendant in this case, there is no need to dirty our
    // ancestors further.
    if (layer->isSelfPaintingLayer()) {
      DCHECK(!parent() || parent()->m_hasSelfPaintingLayerDescendantDirty ||
             parent()->m_hasSelfPaintingLayerDescendant);
      break;
    }
  }
}

bool PaintLayer::sticksToViewport() const {
  if (layoutObject().style()->position() != EPosition::kFixed &&
      layoutObject().style()->position() != EPosition::kSticky)
    return false;

  // TODO(pdr): This approach of calculating the nearest scroll node is O(n).
  // An option for improving this is to cache the nearest scroll node in
  // the local border box properties.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    const auto* viewProperties = layoutObject().view()->paintProperties();
    const ScrollPaintPropertyNode* ancestorTargetScrollNode;
    if (layoutObject().style()->position() == EPosition::kFixed) {
      ancestorTargetScrollNode = viewProperties->localBorderBoxProperties()
                                     ->transform()
                                     ->findEnclosingScrollNode();
    } else {
      ancestorTargetScrollNode = viewProperties->contentsProperties()
                                     ->transform()
                                     ->findEnclosingScrollNode();
    }

    const auto* properties = layoutObject().paintProperties();
    const auto* transform = properties->localBorderBoxProperties()->transform();
    return transform->findEnclosingScrollNode() == ancestorTargetScrollNode;
  }

  return (layoutObject().style()->position() == EPosition::kFixed &&
          layoutObject().containerForFixedPosition() ==
              layoutObject().view()) ||
         (layoutObject().style()->position() == EPosition::kSticky &&
          (!ancestorScrollingLayer() || ancestorScrollingLayer() == root()));
}

bool PaintLayer::scrollsWithRespectTo(const PaintLayer* other) const {
  if (sticksToViewport() != other->sticksToViewport())
    return true;
  return ancestorScrollingLayer() != other->ancestorScrollingLayer();
}

void PaintLayer::updateLayerPositionsAfterOverflowScroll() {
  clipper(PaintLayer::DoNotUseGeometryMapper)
      .clearClipRectsIncludingDescendants();
  updateLayerPositionRecursive();
}

void PaintLayer::updateTransformationMatrix() {
  if (TransformationMatrix* transform = this->transform()) {
    LayoutBox* box = layoutBox();
    DCHECK(box);
    transform->makeIdentity();
    box->style()->applyTransform(
        *transform, box->size(), ComputedStyle::IncludeTransformOrigin,
        ComputedStyle::IncludeMotionPath,
        ComputedStyle::IncludeIndependentTransformProperties);
    makeMatrixRenderable(*transform, compositor()->hasAcceleratedCompositing());
  }
}

void PaintLayer::updateTransform(const ComputedStyle* oldStyle,
                                 const ComputedStyle& newStyle) {
  if (oldStyle && newStyle.transformDataEquivalent(*oldStyle))
    return;

  // hasTransform() on the layoutObject is also true when there is
  // transform-style: preserve-3d or perspective set, so check style too.
  bool hasTransform =
      layoutObject().hasTransformRelatedProperty() && newStyle.hasTransform();
  bool had3DTransform = has3DTransform();

  bool hadTransform = transform();
  if (hasTransform != hadTransform) {
    if (hasTransform)
      ensureRareData().transform = TransformationMatrix::create();
    else
      m_rareData->transform.reset();

    // PaintLayers with transforms act as clip rects roots, so clear the cached
    // clip rects here.
    clipper(PaintLayer::DoNotUseGeometryMapper)
        .clearClipRectsIncludingDescendants();
  } else if (hasTransform) {
    clipper(PaintLayer::DoNotUseGeometryMapper)
        .clearClipRectsIncludingDescendants(AbsoluteClipRects);
  }

  updateTransformationMatrix();

  if (had3DTransform != has3DTransform())
    markAncestorChainForDescendantDependentFlagsUpdate();

  if (FrameView* frameView = layoutObject().document().view())
    frameView->setNeedsUpdateWidgetGeometries();
}

static PaintLayer* enclosingLayerForContainingBlock(PaintLayer* layer) {
  if (LayoutObject* containingBlock = layer->layoutObject().containingBlock())
    return containingBlock->enclosingLayer();
  return 0;
}

static const PaintLayer* enclosingLayerForContainingBlock(
    const PaintLayer* layer) {
  if (const LayoutObject* containingBlock =
          layer->layoutObject().containingBlock())
    return containingBlock->enclosingLayer();
  return 0;
}

PaintLayer* PaintLayer::renderingContextRoot() {
  PaintLayer* renderingContext = 0;

  if (shouldPreserve3D())
    renderingContext = this;

  for (PaintLayer* current = enclosingLayerForContainingBlock(this);
       current && current->shouldPreserve3D();
       current = enclosingLayerForContainingBlock(current))
    renderingContext = current;

  return renderingContext;
}

const PaintLayer* PaintLayer::renderingContextRoot() const {
  const PaintLayer* renderingContext = 0;

  if (shouldPreserve3D())
    renderingContext = this;

  for (const PaintLayer* current = enclosingLayerForContainingBlock(this);
       current && current->shouldPreserve3D();
       current = enclosingLayerForContainingBlock(current))
    renderingContext = current;

  return renderingContext;
}

TransformationMatrix PaintLayer::currentTransform() const {
  if (TransformationMatrix* transform = this->transform())
    return *transform;
  return TransformationMatrix();
}

TransformationMatrix PaintLayer::renderableTransform(
    GlobalPaintFlags globalPaintFlags) const {
  TransformationMatrix* transform = this->transform();
  if (!transform)
    return TransformationMatrix();

  if (globalPaintFlags & GlobalPaintFlattenCompositingLayers) {
    TransformationMatrix matrix = *transform;
    makeMatrixRenderable(matrix, false /* flatten 3d */);
    return matrix;
  }

  return *transform;
}

void PaintLayer::convertFromFlowThreadToVisualBoundingBoxInAncestor(
    const PaintLayer* ancestorLayer,
    LayoutRect& rect) const {
  PaintLayer* paginationLayer = enclosingPaginationLayer();
  DCHECK(paginationLayer);
  LayoutFlowThread& flowThread =
      toLayoutFlowThread(paginationLayer->layoutObject());

  // First make the flow thread rectangle relative to the flow thread, not to
  // |layer|.
  LayoutPoint offsetWithinPaginationLayer;
  convertToLayerCoords(paginationLayer, offsetWithinPaginationLayer);
  rect.moveBy(offsetWithinPaginationLayer);

  // Then make the rectangle visual, relative to the fragmentation context.
  // Split our box up into the actual fragment boxes that layout in the
  // columns/pages and unite those together to get our true bounding box.
  rect = flowThread.fragmentsBoundingBox(rect);

  // Finally, make the visual rectangle relative to |ancestorLayer|.
  if (ancestorLayer->enclosingPaginationLayer() != paginationLayer) {
    rect.moveBy(paginationLayer->visualOffsetFromAncestor(ancestorLayer));
    return;
  }
  // The ancestor layer is inside the same pagination layer as |layer|, so we
  // need to subtract the visual distance from the ancestor layer to the
  // pagination layer.
  rect.moveBy(-ancestorLayer->visualOffsetFromAncestor(paginationLayer));
}

void PaintLayer::updatePaginationRecursive(bool needsPaginationUpdate) {
  if (m_rareData)
    m_rareData->enclosingPaginationLayer = nullptr;

  if (layoutObject().isLayoutFlowThread())
    needsPaginationUpdate = true;

  if (needsPaginationUpdate) {
    // Each paginated layer has to paint on its own. There is no recurring into
    // child layers. Each layer has to be checked individually and genuinely
    // know if it is going to have to split itself up when painting only its
    // contents (and not any other descendant layers). We track an
    // enclosingPaginationLayer instead of using a simple bit, since we want to
    // be able to get back to that layer easily.
    if (LayoutFlowThread* containingFlowThread =
            layoutObject().flowThreadContainingBlock())
      ensureRareData().enclosingPaginationLayer = containingFlowThread->layer();
  }

  for (PaintLayer* child = firstChild(); child; child = child->nextSibling())
    child->updatePaginationRecursive(needsPaginationUpdate);
}

void PaintLayer::clearPaginationRecursive() {
  if (m_rareData)
    m_rareData->enclosingPaginationLayer = nullptr;
  for (PaintLayer* child = firstChild(); child; child = child->nextSibling())
    child->clearPaginationRecursive();
}

void PaintLayer::mapPointInPaintInvalidationContainerToBacking(
    const LayoutBoxModelObject& paintInvalidationContainer,
    FloatPoint& point) {
  // TODO(chrishtr): this should be
  // PaintLayer*  transformedAncestorPaintLayer =
  //    paintInvalidationLayer->enclosingTransformedAncestor();
  // if (!transformedAncestorPaintLayer)
  //  return;
  PaintLayer* paintInvalidationLayer = paintInvalidationContainer.layer();
  if (!paintInvalidationLayer->groupedMapping())
    return;

  LayoutBoxModelObject& transformedAncestor =
      paintInvalidationLayer->enclosingTransformedAncestor()->layoutObject();

  // |paintInvalidationContainer| may have a local 2D transform on it, so take
  // that into account when mapping into the space of the transformed ancestor.
  point = paintInvalidationContainer.localToAncestorPoint(point,
                                                          &transformedAncestor);

  point.moveBy(-paintInvalidationLayer->groupedMapping()
                    ->squashingOffsetFromTransformedAncestor());
}

void PaintLayer::mapRectInPaintInvalidationContainerToBacking(
    const LayoutBoxModelObject& paintInvalidationContainer,
    LayoutRect& rect) {
  // TODO(chrishtr): this should be
  // PaintLayer*  transformedAncestorPaintLayer =
  //    paintInvalidationLayer->enclosingTransformedAncestor();
  // if (!transformedAncestorPaintLayer)
  //  return;
  PaintLayer* paintInvalidationLayer = paintInvalidationContainer.layer();
  if (!paintInvalidationLayer->groupedMapping())
    return;

  LayoutBoxModelObject& transformedAncestor =
      paintInvalidationLayer->enclosingTransformedAncestor()->layoutObject();

  // |paintInvalidationContainer| may have a local 2D transform on it, so take
  // that into account when mapping into the space of the transformed ancestor.
  rect =
      LayoutRect(paintInvalidationContainer
                     .localToAncestorQuad(FloatRect(rect), &transformedAncestor)
                     .boundingBox());

  rect.moveBy(-paintInvalidationLayer->groupedMapping()
                   ->squashingOffsetFromTransformedAncestor());
}

void PaintLayer::mapRectToPaintInvalidationBacking(
    const LayoutObject& layoutObject,
    const LayoutBoxModelObject& paintInvalidationContainer,
    LayoutRect& rect) {
  if (!paintInvalidationContainer.layer()->groupedMapping()) {
    layoutObject.mapToVisualRectInAncestorSpace(&paintInvalidationContainer,
                                                rect);
    return;
  }

  // This code adjusts the visual rect to be in the space of the transformed
  // ancestor of the grouped (i.e. squashed) layer. This is because all layers
  // that squash together need to issue paint invalidations w.r.t. a single
  // container that is an ancestor of all of them, in order to properly take
  // into account any local transforms etc.
  // FIXME: remove this special-case code that works around the paint
  // invalidation code structure.
  layoutObject.mapToVisualRectInAncestorSpace(&paintInvalidationContainer,
                                              rect);

  mapRectInPaintInvalidationContainerToBacking(paintInvalidationContainer,
                                               rect);
}

void PaintLayer::dirtyVisibleContentStatus() {
  markAncestorChainForDescendantDependentFlagsUpdate();
  // Non-self-painting layers paint into their ancestor layer, and count as part
  // of the "visible contents" of the parent, so we need to dirty it.
  if (!isSelfPaintingLayer())
    parent()->dirtyVisibleContentStatus();
}

void PaintLayer::markAncestorChainForDescendantDependentFlagsUpdate() {
  for (PaintLayer* layer = this; layer; layer = layer->parent()) {
    if (layer->m_needsDescendantDependentFlagsUpdate)
      break;
    layer->m_needsDescendantDependentFlagsUpdate = true;

    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
      layer->layoutObject().setNeedsPaintPropertyUpdate();
  }
}

// FIXME: this is quite brute-force. We could be more efficient if we were to
// track state and update it as appropriate as changes are made in the layout
// tree.
void PaintLayer::updateScrollingStateAfterCompositingChange() {
  TRACE_EVENT0("blink",
               "PaintLayer::updateScrollingStateAfterCompositingChange");
  m_isAllScrollingContentComposited = true;
  for (LayoutObject* r = layoutObject().slowFirstChild(); r;
       r = r->nextSibling()) {
    if (!r->hasLayer()) {
      m_isAllScrollingContentComposited = false;
      return;
    }
  }

  for (PaintLayer* child = firstChild(); child; child = child->nextSibling()) {
    if (child->compositingState() == NotComposited) {
      m_isAllScrollingContentComposited = false;
      return;
    } else if (!child->stackingNode()->isStackingContext()) {
      // If the child is composited, but not a stacking context, it may paint
      // negative z-index descendants into an ancestor's GraphicsLayer.
      m_isAllScrollingContentComposited = false;
      return;
    }
  }
}

void PaintLayer::updateDescendantDependentFlags() {
  if (m_needsDescendantDependentFlagsUpdate) {
    bool oldHasNonIsolatedDescendantWithBlendMode =
        m_hasNonIsolatedDescendantWithBlendMode;
    m_hasVisibleDescendant = false;
    m_hasNonIsolatedDescendantWithBlendMode = false;
    m_hasDescendantWithClipPath = false;
    m_hasRootScrollerAsDescendant = false;

    for (PaintLayer* child = firstChild(); child;
         child = child->nextSibling()) {
      child->updateDescendantDependentFlags();

      if (child->m_hasVisibleContent || child->m_hasVisibleDescendant)
        m_hasVisibleDescendant = true;

      m_hasNonIsolatedDescendantWithBlendMode |=
          (!child->stackingNode()->isStackingContext() &&
           child->hasNonIsolatedDescendantWithBlendMode()) ||
          child->layoutObject().styleRef().hasBlendMode();

      m_hasDescendantWithClipPath |= child->hasDescendantWithClipPath() ||
                                     child->layoutObject().hasClipPath();

      m_hasRootScrollerAsDescendant |= child->hasRootScrollerAsDescendant() ||
                                       (child ==
                                        child->layoutObject()
                                            .document()
                                            .rootScrollerController()
                                            .rootScrollerPaintLayer());
    }

    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() &&
        oldHasNonIsolatedDescendantWithBlendMode !=
            static_cast<bool>(m_hasNonIsolatedDescendantWithBlendMode))
      layoutObject().setNeedsPaintPropertyUpdate();
    m_needsDescendantDependentFlagsUpdate = false;
  }

  bool previouslyHasVisibleContent = m_hasVisibleContent;
  if (layoutObject().style()->visibility() == EVisibility::kVisible) {
    m_hasVisibleContent = true;
  } else {
    // layer may be hidden but still have some visible content, check for this
    m_hasVisibleContent = false;
    LayoutObject* r = layoutObject().slowFirstChild();
    while (r) {
      if (r->style()->visibility() == EVisibility::kVisible &&
          (!r->hasLayer() || !r->enclosingLayer()->isSelfPaintingLayer())) {
        m_hasVisibleContent = true;
        break;
      }
      LayoutObject* layoutObjectFirstChild = r->slowFirstChild();
      if (layoutObjectFirstChild &&
          (!r->hasLayer() || !r->enclosingLayer()->isSelfPaintingLayer())) {
        r = layoutObjectFirstChild;
      } else if (r->nextSibling()) {
        r = r->nextSibling();
      } else {
        do {
          r = r->parent();
          if (r == &layoutObject())
            r = 0;
        } while (r && !r->nextSibling());
        if (r)
          r = r->nextSibling();
      }
    }
  }

  if (hasVisibleContent() != previouslyHasVisibleContent) {
    setNeedsCompositingInputsUpdateInternal();
    // We need to tell m_layoutObject to recheck its rect because we
    // pretend that invisible LayoutObjects have 0x0 rects. Changing
    // visibility therefore changes our rect and we need to visit
    // this LayoutObject during the invalidateTreeIfNeeded walk.
    m_layoutObject.setMayNeedPaintInvalidation();
  }

  update3DTransformedDescendantStatus();
}

void PaintLayer::update3DTransformedDescendantStatus() {
  m_has3DTransformedDescendant = false;

  m_stackingNode->updateZOrderLists();

  // Transformed or preserve-3d descendants can only be in the z-order lists,
  // not in the normal flow list, so we only need to check those.
  PaintLayerStackingNodeIterator iterator(
      *m_stackingNode.get(), PositiveZOrderChildren | NegativeZOrderChildren);
  while (PaintLayerStackingNode* node = iterator.next()) {
    const PaintLayer& childLayer = *node->layer();
    bool childHas3D = false;
    // If the child lives in a 3d hierarchy, then the layer at the root of
    // that hierarchy needs the m_has3DTransformedDescendant set.
    if (childLayer.preserves3D() && (childLayer.has3DTransform() ||
                                     childLayer.has3DTransformedDescendant()))
      childHas3D = true;
    else if (childLayer.has3DTransform())
      childHas3D = true;

    if (childHas3D) {
      m_has3DTransformedDescendant = true;
      break;
    }
  }
}

void PaintLayer::updateLayerPosition() {
  LayoutPoint localPoint;

  bool didResize = false;
  if (layoutObject().isInline() && layoutObject().isLayoutInline()) {
    LayoutInline& inlineFlow = toLayoutInline(layoutObject());
    IntRect lineBox = enclosingIntRect(inlineFlow.linesBoundingBox());
    m_size = lineBox.size();
  } else if (LayoutBox* box = layoutBox()) {
    IntSize newSize = pixelSnappedIntSize(box->size(), box->location());
    didResize = newSize != m_size;
    m_size = newSize;
    localPoint.moveBy(box->physicalLocation());
  }

  if (!layoutObject().isOutOfFlowPositioned() &&
      !layoutObject().isColumnSpanAll()) {
    // We must adjust our position by walking up the layout tree looking for the
    // nearest enclosing object with a layer.
    LayoutObject* curr = layoutObject().container();
    while (curr && !curr->hasLayer()) {
      if (curr->isBox() && !curr->isTableRow()) {
        // Rows and cells share the same coordinate space (that of the section).
        // Omit them when computing our xpos/ypos.
        localPoint.moveBy(toLayoutBox(curr)->physicalLocation());
      }
      curr = curr->container();
    }
    if (curr && curr->isTableRow()) {
      // Put ourselves into the row coordinate space.
      localPoint.moveBy(-toLayoutBox(curr)->physicalLocation());
    }
  }

  if (PaintLayer* containingLayer = this->containingLayer()) {
    if (containingLayer->layoutObject().hasOverflowClip()) {
      // Subtract our container's scroll offset.
      IntSize offset = containingLayer->layoutBox()->scrolledContentOffset();
      localPoint -= offset;
    } else if (layoutObject().isAbsolutePositioned() &&
               containingLayer->layoutObject().isInFlowPositioned() &&
               containingLayer->layoutObject().isLayoutInline()) {
      // Adjust offset for absolute under in-flow positioned inline.
      LayoutSize offset =
          toLayoutInline(containingLayer->layoutObject())
              .offsetForInFlowPositionedInline(toLayoutBox(layoutObject()));
      localPoint += offset;
    }
  }

  if (layoutObject().isInFlowPositioned()) {
    LayoutSize newOffset = layoutObject().offsetForInFlowPosition();
    if (m_rareData || !newOffset.isZero())
      ensureRareData().offsetForInFlowPosition = newOffset;
    localPoint.move(newOffset);
  } else if (m_rareData) {
    m_rareData->offsetForInFlowPosition = LayoutSize();
  }

  m_location = localPoint;

  if (m_scrollableArea && didResize)
    m_scrollableArea->visibleSizeChanged();

#if DCHECK_IS_ON()
  m_needsPositionUpdate = false;
#endif
}

TransformationMatrix PaintLayer::perspectiveTransform() const {
  if (!layoutObject().hasTransformRelatedProperty())
    return TransformationMatrix();

  const ComputedStyle& style = layoutObject().styleRef();
  if (!style.hasPerspective())
    return TransformationMatrix();

  TransformationMatrix t;
  t.applyPerspective(style.perspective());
  return t;
}

FloatPoint PaintLayer::perspectiveOrigin() const {
  if (!layoutObject().hasTransformRelatedProperty())
    return FloatPoint();

  const LayoutRect borderBox = toLayoutBox(layoutObject()).borderBoxRect();
  const ComputedStyle& style = layoutObject().styleRef();

  return FloatPoint(floatValueForLength(style.perspectiveOriginX(),
                                        borderBox.width().toFloat()),
                    floatValueForLength(style.perspectiveOriginY(),
                                        borderBox.height().toFloat()));
}

PaintLayer* PaintLayer::containingLayer(const PaintLayer* ancestor,
                                        bool* skippedAncestor) const {
  // If we have specified an ancestor, surely the caller needs to know whether
  // we skipped it.
  DCHECK(!ancestor || skippedAncestor);
  if (skippedAncestor)
    *skippedAncestor = false;

  LayoutObject& layoutObject = this->layoutObject();
  // Column span need to find the containing layer through its containing block.
  // TODO(wangxianzhu): This can be combined with the loop handing possible
  // floating objects.
  if (layoutObject.isColumnSpanAll()) {
    Optional<LayoutObject::AncestorSkipInfo> skipInfo;
    if (skippedAncestor)
      skipInfo.emplace(&ancestor->layoutObject());
    if (auto containingBlock = layoutObject.containingBlock(
            skippedAncestor ? &*skipInfo : nullptr)) {
      if (skippedAncestor && skipInfo->ancestorSkipped())
        *skippedAncestor = true;
      return containingBlock->enclosingLayer();
    }
    return nullptr;
  }

  if (layoutObject.isOutOfFlowPositioned()) {
    auto canContainThisLayer =
        layoutObject.isFixedPositioned()
            ? &LayoutObject::canContainFixedPositionObjects
            : &LayoutObject::canContainAbsolutePositionObjects;

    PaintLayer* curr = parent();
    while (curr && !((&curr->layoutObject())->*canContainThisLayer)()) {
      if (skippedAncestor && curr == ancestor)
        *skippedAncestor = true;
      curr = curr->parent();
    }
    return curr;
  }

  // If the parent layer is not a block, there might be floating objects
  // between this layer (included) and parent layer which need to escape the
  // inline parent to find the actual containing layer through the containing
  // block chain.
  if (!parent() || parent()->layoutObject().isLayoutBlock())
    return parent();

  // This is a universal approach to find containing layer, but is slower than
  // the earlier code.
  Optional<LayoutObject::AncestorSkipInfo> skipInfo;
  if (skippedAncestor)
    skipInfo.emplace(&ancestor->layoutObject());
  auto* object = &layoutObject;
  while (auto* container =
             object->container(skippedAncestor ? &*skipInfo : nullptr)) {
    if (skippedAncestor && skipInfo->ancestorSkipped())
      *skippedAncestor = true;
    if (container->hasLayer())
      return toLayoutBoxModelObject(container)->layer();
    object = container;
  }
  return nullptr;
}

PaintLayer* PaintLayer::enclosingTransformedAncestor() const {
  PaintLayer* curr = parent();
  while (curr && !curr->isRootLayer() && !curr->transform())
    curr = curr->parent();

  return curr;
}

LayoutPoint PaintLayer::computeOffsetFromTransformedAncestor() const {
  TransformState transformState(TransformState::ApplyTransformDirection,
                                FloatPoint());
  layoutObject().mapLocalToAncestor(
      transformAncestor() ? &transformAncestor()->layoutObject() : nullptr,
      transformState, 0);
  transformState.flatten();
  return LayoutPoint(transformState.lastPlanarPoint());
}

PaintLayer* PaintLayer::compositingContainer() const {
  if (!stackingNode()->isStacked())
    return isSelfPaintingLayer() ? parent() : containingLayer();
  if (PaintLayerStackingNode* ancestorStackingNode =
          stackingNode()->ancestorStackingContextNode())
    return ancestorStackingNode->layer();
  return nullptr;
}

bool PaintLayer::isPaintInvalidationContainer() const {
  return compositingState() == PaintsIntoOwnBacking ||
         compositingState() == PaintsIntoGroupedBacking;
}

// Note: enclosingCompositingLayer does not include squashed layers. Compositing
// stacking children of squashed layers receive graphics layers that are
// parented to the compositing ancestor of the squashed layer.
PaintLayer* PaintLayer::enclosingLayerWithCompositedLayerMapping(
    IncludeSelfOrNot includeSelf) const {
  DCHECK(isAllowedToQueryCompositingState());

  if ((includeSelf == IncludeSelf) && compositingState() != NotComposited &&
      compositingState() != PaintsIntoGroupedBacking)
    return const_cast<PaintLayer*>(this);

  for (PaintLayer* curr = compositingContainer(); curr;
       curr = curr->compositingContainer()) {
    if (curr->compositingState() != NotComposited &&
        curr->compositingState() != PaintsIntoGroupedBacking)
      return curr;
  }

  return nullptr;
}

// Return the enclosingCompositedLayerForPaintInvalidation for the given Layer
// including crossing frame boundaries.
PaintLayer*
PaintLayer::enclosingLayerForPaintInvalidationCrossingFrameBoundaries() const {
  const PaintLayer* layer = this;
  PaintLayer* compositedLayer = 0;
  while (!compositedLayer) {
    compositedLayer = layer->enclosingLayerForPaintInvalidation();
    if (!compositedLayer) {
      CHECK(layer->layoutObject().frame());
      LayoutItem owner = layer->layoutObject().frame()->ownerLayoutItem();
      if (owner.isNull())
        break;
      layer = owner.enclosingLayer();
    }
  }
  return compositedLayer;
}

PaintLayer* PaintLayer::enclosingLayerForPaintInvalidation() const {
  DCHECK(isAllowedToQueryCompositingState());

  if (isPaintInvalidationContainer())
    return const_cast<PaintLayer*>(this);

  for (PaintLayer* curr = compositingContainer(); curr;
       curr = curr->compositingContainer()) {
    if (curr->isPaintInvalidationContainer())
      return curr;
  }

  return nullptr;
}

void PaintLayer::setNeedsCompositingInputsUpdate() {
  setNeedsCompositingInputsUpdateInternal();

  // TODO(chrishtr): These are a bit of a heavy hammer, because not all
  // things which require compositing inputs update require a descendant-
  // dependent flags udpate. Reduce call sites after SPv2 launch allows
  /// removal of CompositingInputsUpdater.
  markAncestorChainForDescendantDependentFlagsUpdate();
}

void PaintLayer::setNeedsCompositingInputsUpdateInternal() {
  m_needsAncestorDependentCompositingInputsUpdate = true;

  for (PaintLayer* current = this;
       current && !current->m_childNeedsCompositingInputsUpdate;
       current = current->parent())
    current->m_childNeedsCompositingInputsUpdate = true;

  compositor()->setNeedsCompositingUpdate(
      CompositingUpdateAfterCompositingInputChange);
}

void PaintLayer::updateAncestorDependentCompositingInputs(
    const AncestorDependentCompositingInputs& compositingInputs,
    bool hasAncestorWithClipPath) {
  m_ancestorDependentCompositingInputs =
      WTF::makeUnique<AncestorDependentCompositingInputs>(compositingInputs);
  m_hasAncestorWithClipPath = hasAncestorWithClipPath;
  m_needsAncestorDependentCompositingInputsUpdate = false;
}

void PaintLayer::didUpdateCompositingInputs() {
  DCHECK(!needsCompositingInputsUpdate());
  m_childNeedsCompositingInputsUpdate = false;
  if (m_scrollableArea)
    m_scrollableArea->updateNeedsCompositedScrolling();
}

bool PaintLayer::hasNonIsolatedDescendantWithBlendMode() const {
  if (m_hasNonIsolatedDescendantWithBlendMode)
    return true;
  if (layoutObject().isSVGRoot())
    return toLayoutSVGRoot(layoutObject()).hasNonIsolatedBlendingDescendants();
  return false;
}

void PaintLayer::setCompositingReasons(CompositingReasons reasons,
                                       CompositingReasons mask) {
  CompositingReasons oldReasons =
      m_rareData ? m_rareData->compositingReasons : CompositingReasonNone;
  if ((oldReasons & mask) == (reasons & mask))
    return;
  CompositingReasons newReasons = (reasons & mask) | (oldReasons & ~mask);
  if (m_rareData || newReasons != CompositingReasonNone)
    ensureRareData().compositingReasons = newReasons;
}

void PaintLayer::setSquashingDisallowedReasons(
    SquashingDisallowedReasons reasons) {
  SquashingDisallowedReasons oldReasons =
      m_rareData ? m_rareData->squashingDisallowedReasons
                 : SquashingDisallowedReasonsNone;
  if (oldReasons == reasons)
    return;
  if (m_rareData || reasons != SquashingDisallowedReasonsNone)
    ensureRareData().squashingDisallowedReasons = reasons;
}

void PaintLayer::setHasCompositingDescendant(bool hasCompositingDescendant) {
  if (m_hasCompositingDescendant ==
      static_cast<unsigned>(hasCompositingDescendant))
    return;

  m_hasCompositingDescendant = hasCompositingDescendant;

  if (hasCompositedLayerMapping())
    compositedLayerMapping()->setNeedsGraphicsLayerUpdate(
        GraphicsLayerUpdateLocal);
}

void PaintLayer::setShouldIsolateCompositedDescendants(
    bool shouldIsolateCompositedDescendants) {
  if (m_shouldIsolateCompositedDescendants ==
      static_cast<unsigned>(shouldIsolateCompositedDescendants))
    return;

  m_shouldIsolateCompositedDescendants = shouldIsolateCompositedDescendants;

  if (hasCompositedLayerMapping())
    compositedLayerMapping()->setNeedsGraphicsLayerUpdate(
        GraphicsLayerUpdateLocal);
}

bool PaintLayer::hasAncestorWithFilterThatMovesPixels() const {
  for (const PaintLayer* curr = this; curr; curr = curr->parent()) {
    if (curr->hasFilterThatMovesPixels())
      return true;
  }
  return false;
}

static void expandClipRectForDescendants(
    LayoutRect& clipRect,
    const PaintLayer* layer,
    const PaintLayer* rootLayer,
    PaintLayer::TransparencyClipBoxBehavior transparencyBehavior,
    const LayoutSize& subPixelAccumulation,
    GlobalPaintFlags globalPaintFlags) {
  // If we have a mask, then the clip is limited to the border box area (and
  // there is no need to examine child layers).
  if (!layer->layoutObject().hasMask()) {
    // Note: we don't have to walk z-order lists since transparent elements
    // always establish a stacking container. This means we can just walk the
    // layer tree directly.
    for (PaintLayer* curr = layer->firstChild(); curr;
         curr = curr->nextSibling())
      clipRect.unite(PaintLayer::transparencyClipBox(
          curr, rootLayer, transparencyBehavior,
          PaintLayer::DescendantsOfTransparencyClipBox, subPixelAccumulation,
          globalPaintFlags));
  }
}

LayoutRect PaintLayer::transparencyClipBox(
    const PaintLayer* layer,
    const PaintLayer* rootLayer,
    TransparencyClipBoxBehavior transparencyBehavior,
    TransparencyClipBoxMode transparencyMode,
    const LayoutSize& subPixelAccumulation,
    GlobalPaintFlags globalPaintFlags) {
  // FIXME: Although this function completely ignores CSS-imposed clipping, we
  // did already intersect with the paintDirtyRect, and that should cut down on
  // the amount we have to paint.  Still it would be better to respect clips.

  if (rootLayer != layer &&
      ((transparencyBehavior == PaintingTransparencyClipBox &&
        layer->paintsWithTransform(globalPaintFlags)) ||
       (transparencyBehavior == HitTestingTransparencyClipBox &&
        layer->hasTransformRelatedProperty()))) {
    // The best we can do here is to use enclosed bounding boxes to establish a
    // "fuzzy" enough clip to encompass the transformed layer and all of its
    // children.
    const PaintLayer* paginationLayer =
        transparencyMode == DescendantsOfTransparencyClipBox
            ? layer->enclosingPaginationLayer()
            : 0;
    const PaintLayer* rootLayerForTransform =
        paginationLayer ? paginationLayer : rootLayer;
    LayoutPoint delta;
    layer->convertToLayerCoords(rootLayerForTransform, delta);

    delta.move(subPixelAccumulation);
    IntPoint pixelSnappedDelta = roundedIntPoint(delta);
    TransformationMatrix transform;
    transform.translate(pixelSnappedDelta.x(), pixelSnappedDelta.y());
    if (layer->transform())
      transform = transform * *layer->transform();

    // We don't use fragment boxes when collecting a transformed layer's
    // bounding box, since it always paints unfragmented.
    LayoutRect clipRect = layer->physicalBoundingBox(LayoutPoint());
    expandClipRectForDescendants(clipRect, layer, layer, transparencyBehavior,
                                 subPixelAccumulation, globalPaintFlags);
    LayoutRect result = enclosingLayoutRect(
        transform.mapRect(layer->mapRectForFilter(FloatRect(clipRect))));
    if (!paginationLayer)
      return result;

    // We have to break up the transformed extent across our columns.
    // Split our box up into the actual fragment boxes that layout in the
    // columns/pages and unite those together to get our true bounding box.
    LayoutFlowThread& enclosingFlowThread =
        toLayoutFlowThread(paginationLayer->layoutObject());
    result = enclosingFlowThread.fragmentsBoundingBox(result);

    LayoutPoint rootLayerDelta;
    paginationLayer->convertToLayerCoords(rootLayer, rootLayerDelta);
    result.moveBy(rootLayerDelta);
    return result;
  }

  LayoutRect clipRect = layer->shouldFragmentCompositedBounds(rootLayer)
                            ? layer->fragmentsBoundingBox(rootLayer)
                            : layer->physicalBoundingBox(rootLayer);
  expandClipRectForDescendants(clipRect, layer, rootLayer, transparencyBehavior,
                               subPixelAccumulation, globalPaintFlags);

  // Convert clipRect into local coordinates for mapLayerRectForFilter(), and
  // convert back after.
  LayoutPoint delta;
  layer->convertToLayerCoords(rootLayer, delta);
  clipRect.moveBy(-delta);
  clipRect = layer->mapLayoutRectForFilter(clipRect);
  clipRect.moveBy(delta);

  clipRect.move(subPixelAccumulation);
  return clipRect;
}

LayoutRect PaintLayer::paintingExtent(const PaintLayer* rootLayer,
                                      const LayoutSize& subPixelAccumulation,
                                      GlobalPaintFlags globalPaintFlags) {
  return transparencyClipBox(this, rootLayer, PaintingTransparencyClipBox,
                             RootOfTransparencyClipBox, subPixelAccumulation,
                             globalPaintFlags);
}

void* PaintLayer::operator new(size_t sz) {
  return PartitionAlloc(WTF::Partitions::layoutPartition(), sz,
                        WTF_HEAP_PROFILER_TYPE_NAME(PaintLayer));
}

void PaintLayer::operator delete(void* ptr) {
  WTF::PartitionFree(ptr);
}

void PaintLayer::addChild(PaintLayer* child, PaintLayer* beforeChild) {
  PaintLayer* prevSibling =
      beforeChild ? beforeChild->previousSibling() : lastChild();
  if (prevSibling) {
    child->setPreviousSibling(prevSibling);
    prevSibling->setNextSibling(child);
    DCHECK(prevSibling != child);
  } else {
    setFirstChild(child);
  }

  if (beforeChild) {
    beforeChild->setPreviousSibling(child);
    child->setNextSibling(beforeChild);
    DCHECK(beforeChild != child);
  } else {
    setLastChild(child);
  }

  child->m_parent = this;

  // The ancestor overflow layer is calculated during compositing inputs update
  // and should not be set yet.
  CHECK(!child->ancestorOverflowLayer());

  setNeedsCompositingInputsUpdate();

  if (!child->stackingNode()->isStacked() &&
      !layoutObject().documentBeingDestroyed())
    compositor()->setNeedsCompositingUpdate(CompositingUpdateRebuildTree);

  if (child->stackingNode()->isStacked() || child->firstChild()) {
    // Dirty the z-order list in which we are contained. The
    // ancestorStackingContextNode() can be null in the case where we're
    // building up generated content layers. This is ok, since the lists will
    // start off dirty in that case anyway.
    child->stackingNode()->dirtyStackingContextZOrderLists();
  }

  // Non-self-painting children paint into this layer, so the visible contents
  // status of this layer is affected.
  if (!child->isSelfPaintingLayer())
    dirtyVisibleContentStatus();

  markAncestorChainForDescendantDependentFlagsUpdate();
  dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

  child->setNeedsRepaint();
}

PaintLayer* PaintLayer::removeChild(PaintLayer* oldChild) {
  if (oldChild->previousSibling())
    oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
  if (oldChild->nextSibling())
    oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

  if (m_first == oldChild)
    m_first = oldChild->nextSibling();
  if (m_last == oldChild)
    m_last = oldChild->previousSibling();

  if (!oldChild->stackingNode()->isStacked() &&
      !layoutObject().documentBeingDestroyed())
    compositor()->setNeedsCompositingUpdate(CompositingUpdateRebuildTree);

  if (oldChild->stackingNode()->isStacked() || oldChild->firstChild()) {
    // Dirty the z-order list in which we are contained.  When called via the
    // reattachment process in removeOnlyThisLayer, the layer may already be
    // disconnected from the main layer tree, so we need to null-check the
    // |stackingContext| value.
    oldChild->stackingNode()->dirtyStackingContextZOrderLists();
  }

  if (layoutObject().style()->visibility() != EVisibility::kVisible)
    dirtyVisibleContentStatus();

  oldChild->setPreviousSibling(0);
  oldChild->setNextSibling(0);
  oldChild->m_parent = 0;

  // Remove any ancestor overflow layers which descended into the removed child.
  if (oldChild->ancestorOverflowLayer())
    oldChild->removeAncestorOverflowLayer(oldChild->ancestorOverflowLayer());

  dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

  if (oldChild->m_hasVisibleContent || oldChild->m_hasVisibleDescendant)
    markAncestorChainForDescendantDependentFlagsUpdate();

  if (oldChild->enclosingPaginationLayer())
    oldChild->clearPaginationRecursive();

  setNeedsRepaint();

  return oldChild;
}

void PaintLayer::removeOnlyThisLayerAfterStyleChange() {
  if (!m_parent)
    return;

  bool didSetPaintInvalidation = false;
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    DisableCompositingQueryAsserts
        disabler;  // We need the current compositing status.
    if (isPaintInvalidationContainer()) {
      // Our children will be reparented and contained by a new paint
      // invalidation container, so need paint invalidation. CompositingUpdate
      // can't see this layer (which has been removed) so won't do this for us.
      DisablePaintInvalidationStateAsserts disabler;
      ObjectPaintInvalidator(layoutObject())
          .invalidatePaintIncludingNonCompositingDescendants();
      layoutObject()
          .setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
      didSetPaintInvalidation = true;
    }
  }

  if (!didSetPaintInvalidation && isSelfPaintingLayer()) {
    if (PaintLayer* enclosingSelfPaintingLayer =
            m_parent->enclosingSelfPaintingLayer())
      enclosingSelfPaintingLayer->mergeNeedsPaintPhaseFlagsFrom(*this);
  }

  clipper(PaintLayer::DoNotUseGeometryMapper)
      .clearClipRectsIncludingDescendants();

  PaintLayer* nextSib = nextSibling();

  // Now walk our kids and reattach them to our parent.
  PaintLayer* current = m_first;
  while (current) {
    PaintLayer* next = current->nextSibling();
    removeChild(current);
    m_parent->addChild(current, nextSib);

    // FIXME: We should call a specialized version of this function.
    current->updateLayerPositionsAfterLayout();
    current = next;
  }

  // Remove us from the parent.
  m_parent->removeChild(this);
  m_layoutObject.destroyLayer();
}

void PaintLayer::insertOnlyThisLayerAfterStyleChange() {
  if (!m_parent && layoutObject().parent()) {
    // We need to connect ourselves when our layoutObject() has a parent.
    // Find our enclosingLayer and add ourselves.
    PaintLayer* parentLayer = layoutObject().parent()->enclosingLayer();
    DCHECK(parentLayer);
    PaintLayer* beforeChild =
        layoutObject().parent()->findNextLayer(parentLayer, &layoutObject());
    parentLayer->addChild(this, beforeChild);
  }

  // Remove all descendant layers from the hierarchy and add them to the new
  // position.
  for (LayoutObject* curr = layoutObject().slowFirstChild(); curr;
       curr = curr->nextSibling())
    curr->moveLayers(m_parent, this);

  // If the previous paint invalidation container is not a stacking context and
  // this object is stacked content, creating this layer may cause this object
  // and its descendants to change paint invalidation container.
  bool didSetPaintInvalidation = false;
  if (!RuntimeEnabledFeatures::slimmingPaintV2Enabled() &&
      !layoutObject().isLayoutView() && layoutObject().isRooted() &&
      layoutObject().styleRef().isStacked()) {
    const LayoutBoxModelObject& previousPaintInvalidationContainer =
        layoutObject().parent()->containerForPaintInvalidation();
    if (!previousPaintInvalidationContainer.styleRef().isStackingContext()) {
      ObjectPaintInvalidator(layoutObject())
          .invalidatePaintIncludingNonSelfPaintingLayerDescendants(
              previousPaintInvalidationContainer);
      // Set needsRepaint along the original compositingContainer chain.
      layoutObject().parent()->enclosingLayer()->setNeedsRepaint();
      didSetPaintInvalidation = true;
    }
  }

  if (!didSetPaintInvalidation && isSelfPaintingLayer() && m_parent) {
    if (PaintLayer* enclosingSelfPaintingLayer =
            m_parent->enclosingSelfPaintingLayer())
      mergeNeedsPaintPhaseFlagsFrom(*enclosingSelfPaintingLayer);
  }

  // Clear out all the clip rects.
  clipper(PaintLayer::DoNotUseGeometryMapper)
      .clearClipRectsIncludingDescendants();
}

// Returns the layer reached on the walk up towards the ancestor.
static inline const PaintLayer* accumulateOffsetTowardsAncestor(
    const PaintLayer* layer,
    const PaintLayer* ancestorLayer,
    LayoutPoint& location) {
  DCHECK(ancestorLayer != layer);

  const LayoutBoxModelObject& layoutObject = layer->layoutObject();

  if (layoutObject.isFixedPositioned() &&
      (!ancestorLayer || ancestorLayer == layoutObject.view()->layer())) {
    // If the fixed layer's container is the root, just add in the offset of the
    // view. We can obtain this by calling localToAbsolute() on the LayoutView.
    FloatPoint absPos = layoutObject.localToAbsolute();
    location += LayoutSize(absPos.x(), absPos.y());
    return ancestorLayer;
  }

  bool foundAncestorFirst;
  PaintLayer* containingLayer =
      layer->containingLayer(ancestorLayer, &foundAncestorFirst);

  if (foundAncestorFirst) {
    // Found ancestorLayer before the containing layer, so compute offset of
    // both relative to the container and subtract.
    LayoutPoint thisCoords;
    layer->convertToLayerCoords(containingLayer, thisCoords);

    LayoutPoint ancestorCoords;
    ancestorLayer->convertToLayerCoords(containingLayer, ancestorCoords);

    location += (thisCoords - ancestorCoords);
    return ancestorLayer;
  }

  if (!containingLayer)
    return nullptr;

  location += layer->location();
  return containingLayer;
}

void PaintLayer::convertToLayerCoords(const PaintLayer* ancestorLayer,
                                      LayoutPoint& location) const {
  if (ancestorLayer == this)
    return;

  const PaintLayer* currLayer = this;
  while (currLayer && currLayer != ancestorLayer)
    currLayer =
        accumulateOffsetTowardsAncestor(currLayer, ancestorLayer, location);
}

void PaintLayer::convertToLayerCoords(const PaintLayer* ancestorLayer,
                                      LayoutRect& rect) const {
  LayoutPoint delta;
  convertToLayerCoords(ancestorLayer, delta);
  rect.moveBy(delta);
}

LayoutPoint PaintLayer::visualOffsetFromAncestor(
    const PaintLayer* ancestorLayer) const {
  LayoutPoint offset;
  if (ancestorLayer == this)
    return offset;
  PaintLayer* paginationLayer = enclosingPaginationLayer();
  if (paginationLayer == this)
    paginationLayer = parent()->enclosingPaginationLayer();
  if (!paginationLayer) {
    convertToLayerCoords(ancestorLayer, offset);
    return offset;
  }

  LayoutFlowThread& flowThread =
      toLayoutFlowThread(paginationLayer->layoutObject());
  convertToLayerCoords(paginationLayer, offset);
  offset = flowThread.flowThreadPointToVisualPoint(offset);
  if (ancestorLayer == paginationLayer)
    return offset;

  if (ancestorLayer->enclosingPaginationLayer() != paginationLayer) {
    offset.moveBy(paginationLayer->visualOffsetFromAncestor(ancestorLayer));
  } else {
    // The ancestor layer is also inside the pagination layer, so we need to
    // subtract the visual distance from the ancestor layer to the pagination
    // layer.
    offset.moveBy(-ancestorLayer->visualOffsetFromAncestor(paginationLayer));
  }
  return offset;
}

void PaintLayer::didUpdateNeedsCompositedScrolling() {
  updateSelfPaintingLayer();
}

void PaintLayer::updateStackingNode() {
  DCHECK(!m_stackingNode);
  if (requiresStackingNode())
    m_stackingNode = WTF::makeUnique<PaintLayerStackingNode>(this);
  else
    m_stackingNode = nullptr;
}

void PaintLayer::updateScrollableArea() {
  DCHECK(!m_scrollableArea);
  if (requiresScrollableArea())
    m_scrollableArea = PaintLayerScrollableArea::create(*this);
}

bool PaintLayer::hasOverflowControls() const {
  return m_scrollableArea && (m_scrollableArea->hasScrollbar() ||
                              m_scrollableArea->scrollCorner() ||
                              layoutObject().style()->resize() != RESIZE_NONE);
}

void PaintLayer::appendSingleFragmentIgnoringPagination(
    PaintLayerFragments& fragments,
    const PaintLayer* rootLayer,
    const LayoutRect& dirtyRect,
    ClipRectsCacheSlot clipRectsCacheSlot,
    PaintLayer::GeometryMapperOption geometryMapperOption,
    OverlayScrollbarClipBehavior overlayScrollbarClipBehavior,
    ShouldRespectOverflowClipType respectOverflowClip,
    const LayoutPoint* offsetFromRoot,
    const LayoutSize& subPixelAccumulation) {
  PaintLayerFragment fragment;
  ClipRectsContext clipRectsContext(rootLayer, clipRectsCacheSlot,
                                    overlayScrollbarClipBehavior,
                                    subPixelAccumulation);
  if (respectOverflowClip == IgnoreOverflowClip)
    clipRectsContext.setIgnoreOverflowClip();
  clipper(geometryMapperOption)
      .calculateRects(clipRectsContext, dirtyRect, fragment.layerBounds,
                      fragment.backgroundRect, fragment.foregroundRect,
                      offsetFromRoot);
  fragments.push_back(fragment);
}

bool PaintLayer::shouldFragmentCompositedBounds(
    const PaintLayer* compositingLayer) const {
  // Composited layers may not be fragmented.
  return enclosingPaginationLayer() &&
         !compositingLayer->enclosingPaginationLayer();
}

void PaintLayer::collectFragments(
    PaintLayerFragments& fragments,
    const PaintLayer* rootLayer,
    const LayoutRect& dirtyRect,
    ClipRectsCacheSlot clipRectsCacheSlot,
    PaintLayer::GeometryMapperOption geometryMapperOption,
    OverlayScrollbarClipBehavior overlayScrollbarClipBehavior,
    ShouldRespectOverflowClipType respectOverflowClip,
    const LayoutPoint* offsetFromRoot,
    const LayoutSize& subPixelAccumulation,
    const LayoutRect* layerBoundingBox) {
  // For unpaginated layers, there is only one fragment. We also avoid
  // fragmentation when compositing, due to implementation limitations.
  if (!enclosingPaginationLayer() ||
      !shouldFragmentCompositedBounds(rootLayer)) {
    appendSingleFragmentIgnoringPagination(
        fragments, rootLayer, dirtyRect, clipRectsCacheSlot,
        geometryMapperOption, overlayScrollbarClipBehavior, respectOverflowClip,
        offsetFromRoot, subPixelAccumulation);
    return;
  }

  // Compute our offset within the enclosing pagination layer.
  LayoutPoint offsetWithinPaginatedLayer;
  convertToLayerCoords(enclosingPaginationLayer(), offsetWithinPaginatedLayer);

  // Calculate clip rects relative to the enclosingPaginationLayer. The purpose
  // of this call is to determine our bounds clipped to intermediate layers
  // between us and the pagination context. It's important to minimize the
  // number of fragments we need to create and this helps with that.
  ClipRectsContext paginationClipRectsContext(enclosingPaginationLayer(),
                                              clipRectsCacheSlot,
                                              overlayScrollbarClipBehavior);
  if (respectOverflowClip == IgnoreOverflowClip)
    paginationClipRectsContext.setIgnoreOverflowClip();
  LayoutRect layerBoundsInFlowThread;
  ClipRect backgroundRectInFlowThread;
  ClipRect foregroundRectInFlowThread;
  clipper(geometryMapperOption)
      .calculateRects(paginationClipRectsContext,
                      LayoutRect(LayoutRect::infiniteIntRect()),
                      layerBoundsInFlowThread, backgroundRectInFlowThread,
                      foregroundRectInFlowThread, &offsetWithinPaginatedLayer);

  // Take our bounding box within the flow thread and clip it.
  LayoutRect layerBoundingBoxInFlowThread =
      layerBoundingBox ? *layerBoundingBox
                       : physicalBoundingBox(offsetWithinPaginatedLayer);
  layerBoundingBoxInFlowThread.intersect(backgroundRectInFlowThread.rect());

  LayoutFlowThread& enclosingFlowThread =
      toLayoutFlowThread(enclosingPaginationLayer()->layoutObject());
  // Visual offset from the root layer to the nearest fragmentation context.
  LayoutPoint offsetOfPaginationLayerFromRoot;
  bool rootLayerIsInsidePaginationLayer =
      rootLayer->enclosingPaginationLayer() == enclosingPaginationLayer();
  if (rootLayerIsInsidePaginationLayer) {
    // The root layer is in the same fragmentation context as this layer, so we
    // need to look inside it and subtract the offset between the fragmentation
    // context and the root layer.
    offsetOfPaginationLayerFromRoot =
        -rootLayer->visualOffsetFromAncestor(enclosingPaginationLayer());
  } else {
    offsetOfPaginationLayerFromRoot =
        enclosingPaginationLayer()->visualOffsetFromAncestor(rootLayer);
  }
  // Make the dirty rect relative to the fragmentation context (multicol
  // container, etc.).
  LayoutRect dirtyRectInMulticolContainer(dirtyRect);
  dirtyRectInMulticolContainer.move(enclosingFlowThread.physicalLocation() -
                                    offsetOfPaginationLayerFromRoot);

  // Slice the layer into fragments. Each fragment needs to be processed (e.g.
  // painted) separately. We pass enough information to walk a minimal number of
  // fragments based on the pages/columns that intersect the actual dirtyRect as
  // well as the pages/columns that intersect our layer's bounding box.
  FragmentainerIterator iterator(enclosingFlowThread,
                                 layerBoundingBoxInFlowThread,
                                 dirtyRectInMulticolContainer);
  if (iterator.atEnd())
    return;

  // Get the parent clip rects of the pagination layer, since we need to
  // intersect with that when painting column contents.
  ClipRect ancestorClipRect = dirtyRect;
  if (const PaintLayer* paginationParentLayer =
          enclosingPaginationLayer()->parent()) {
    const PaintLayer* ancestorLayer =
        rootLayerIsInsidePaginationLayer ? paginationParentLayer : rootLayer;
    ClipRectsContext clipRectsContext(ancestorLayer, clipRectsCacheSlot,
                                      overlayScrollbarClipBehavior);
    if (respectOverflowClip == IgnoreOverflowClip)
      clipRectsContext.setIgnoreOverflowClip();
    ancestorClipRect = enclosingPaginationLayer()
                           ->clipper(geometryMapperOption)
                           .backgroundClipRect(clipRectsContext);
    if (rootLayerIsInsidePaginationLayer)
      ancestorClipRect.moveBy(
          -rootLayer->visualOffsetFromAncestor(ancestorLayer));
    ancestorClipRect.intersect(dirtyRect);
  }

  const LayoutSize subPixelAccumulationIfNeeded =
      offsetFromRoot ? subPixelAccumulation : LayoutSize();
  for (; !iterator.atEnd(); iterator.advance()) {
    PaintLayerFragment fragment;
    fragment.paginationOffset = toLayoutPoint(iterator.paginationOffset());
    fragment.paginationClip = iterator.clipRectInFlowThread();

    // Set our four rects with all clipping applied that was internal to the
    // flow thread.
    fragment.setRects(layerBoundsInFlowThread, backgroundRectInFlowThread,
                      foregroundRectInFlowThread);

    // Shift to the root-relative physical position used when painting the flow
    // thread in this fragment.
    fragment.moveBy(fragment.paginationOffset +
                    offsetOfPaginationLayerFromRoot +
                    subPixelAccumulationIfNeeded);

    // Intersect the fragment with our ancestor's background clip so that e.g.,
    // columns in an overflow:hidden block are properly clipped by the overflow.
    fragment.intersect(ancestorClipRect.rect());

    // Now intersect with our pagination clip. This will typically mean we're
    // just intersecting the dirty rect with the column clip, so the column clip
    // ends up being all we apply.
    fragment.intersect(fragment.paginationClip);

    // TODO(mstensho): Don't add empty fragments. We've always done that in some
    // cases, but there should be no reason to do so. Either filter them out
    // here, or, even better: pass a better clip rectangle to the fragmentainer
    // iterator, so that we won't end up with empty fragments here.
    fragments.push_back(fragment);
  }
}

static inline LayoutRect frameVisibleRect(LayoutObject& layoutObject) {
  FrameView* frameView = layoutObject.document().view();
  if (!frameView)
    return LayoutRect();

  return LayoutRect(frameView->visibleContentRect());
}

bool PaintLayer::hitTest(HitTestResult& result) {
  DCHECK(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

  // LayoutView should make sure to update layout before entering hit testing
  DCHECK(!layoutObject().frame()->view()->layoutPending());
  DCHECK(!layoutObject().document().layoutViewItem().needsLayout());

  const HitTestRequest& request = result.hitTestRequest();
  const HitTestLocation& hitTestLocation = result.hitTestLocation();

  // Start with frameVisibleRect to ensure we include the scrollbars.
  LayoutRect hitTestArea = frameVisibleRect(layoutObject());
  if (request.ignoreClipping())
    hitTestArea.unite(LayoutRect(layoutObject().view()->documentRect()));

  PaintLayer* insideLayer =
      hitTestLayer(this, 0, result, hitTestArea, hitTestLocation, false);
  if (!insideLayer && isRootLayer()) {
    IntRect hitRect = hitTestLocation.boundingBox();
    bool fallback = false;
    // If we didn't hit any layers but are still inside the document
    // bounds, then we should fallback to hitting the document.
    // For rect-based hit test, we do the fallback only when the hit-rect
    // is totally within the document bounds.
    if (hitTestArea.contains(LayoutRect(hitRect))) {
      fallback = true;

      // Mouse dragging outside the main document should also be
      // delivered to the document.
      // TODO(miletus): Capture behavior inconsistent with iframes
      // crbug.com/522109.
      // TODO(majidvp): This should apply more consistently across different
      // event types and we should not use RequestType for it. Perhaps best for
      // it to be done at a higher level. See http://crbug.com/505825
    } else if ((request.active() || request.release()) &&
               !request.isChildFrameHitTest()) {
      fallback = true;
    }
    if (fallback) {
      layoutObject().updateHitTestResult(
          result, toLayoutView(layoutObject())
                      .flipForWritingMode(hitTestLocation.point()));
      insideLayer = this;

      // Don't cache this result since it really wasn't a true hit.
      result.setCacheable(false);
    }
  }

  // Now determine if the result is inside an anchor - if the urlElement isn't
  // already set.
  Node* node = result.innerNode();
  if (node && !result.URLElement())
    result.setURLElement(node->enclosingLinkEventParentOrSelf());

  // Now return whether we were inside this layer (this will always be true for
  // the root layer).
  return insideLayer;
}

Node* PaintLayer::enclosingNode() const {
  for (LayoutObject* r = &layoutObject(); r; r = r->parent()) {
    if (Node* e = r->node())
      return e;
  }
  ASSERT_NOT_REACHED();
  return 0;
}

bool PaintLayer::isInTopLayer() const {
  Node* node = layoutObject().node();
  return node && node->isElementNode() && toElement(node)->isInTopLayer();
}

// Compute the z-offset of the point in the transformState.
// This is effectively projecting a ray normal to the plane of ancestor, finding
// where that ray intersects target, and computing the z delta between those two
// points.
static double computeZOffset(const HitTestingTransformState& transformState) {
  // We got an affine transform, so no z-offset
  if (transformState.m_accumulatedTransform.isAffine())
    return 0;

  // Flatten the point into the target plane
  FloatPoint targetPoint = transformState.mappedPoint();

  // Now map the point back through the transform, which computes Z.
  FloatPoint3D backmappedPoint =
      transformState.m_accumulatedTransform.mapPoint(FloatPoint3D(targetPoint));
  return backmappedPoint.z();
}

PassRefPtr<HitTestingTransformState> PaintLayer::createLocalTransformState(
    PaintLayer* rootLayer,
    PaintLayer* containerLayer,
    const LayoutRect& hitTestRect,
    const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* containerTransformState,
    const LayoutPoint& translationOffset) const {
  RefPtr<HitTestingTransformState> transformState;
  LayoutPoint offset;
  if (containerTransformState) {
    // If we're already computing transform state, then it's relative to the
    // container (which we know is non-null).
    transformState = HitTestingTransformState::create(*containerTransformState);
    convertToLayerCoords(containerLayer, offset);
  } else {
    // If this is the first time we need to make transform state, then base it
    // off of hitTestLocation, which is relative to rootLayer.
    transformState = HitTestingTransformState::create(
        hitTestLocation.transformedPoint(), hitTestLocation.transformedRect(),
        FloatQuad(FloatRect(hitTestRect)));
    convertToLayerCoords(rootLayer, offset);
  }
  offset.moveBy(translationOffset);

  LayoutObject* containerLayoutObject =
      containerLayer ? &containerLayer->layoutObject() : 0;
  if (layoutObject().shouldUseTransformFromContainer(containerLayoutObject)) {
    TransformationMatrix containerTransform;
    layoutObject().getTransformFromContainer(
        containerLayoutObject, toLayoutSize(offset), containerTransform);
    transformState->applyTransform(
        containerTransform, HitTestingTransformState::AccumulateTransform);
  } else {
    transformState->translate(offset.x().toInt(), offset.y().toInt(),
                              HitTestingTransformState::AccumulateTransform);
  }

  return transformState;
}

static bool isHitCandidate(const PaintLayer* hitLayer,
                           bool canDepthSort,
                           double* zOffset,
                           const HitTestingTransformState* transformState) {
  if (!hitLayer)
    return false;

  // The hit layer is depth-sorting with other layers, so just say that it was
  // hit.
  if (canDepthSort)
    return true;

  // We need to look at z-depth to decide if this layer was hit.
  if (zOffset) {
    DCHECK(transformState);
    // This is actually computing our z, but that's OK because the hitLayer is
    // coplanar with us.
    double childZOffset = computeZOffset(*transformState);
    if (childZOffset > *zOffset) {
      *zOffset = childZOffset;
      return true;
    }
    return false;
  }

  return true;
}

// hitTestLocation and hitTestRect are relative to rootLayer.
// A 'flattening' layer is one preserves3D() == false.
// transformState.m_accumulatedTransform holds the transform from the containing
// flattening layer.
// transformState.m_lastPlanarPoint is the hitTestLocation in the plane of the
// containing flattening layer.
// transformState.m_lastPlanarQuad is the hitTestRect as a quad in the plane of
// the containing flattening layer.
//
// If zOffset is non-null (which indicates that the caller wants z offset
// information), *zOffset on return is the z offset of the hit point relative to
// the containing flattening layer.
PaintLayer* PaintLayer::hitTestLayer(
    PaintLayer* rootLayer,
    PaintLayer* containerLayer,
    HitTestResult& result,
    const LayoutRect& hitTestRect,
    const HitTestLocation& hitTestLocation,
    bool appliedTransform,
    const HitTestingTransformState* transformState,
    double* zOffset) {
  DCHECK(layoutObject().document().lifecycle().state() >=
         DocumentLifecycle::CompositingClean);

  if (!isSelfPaintingLayer() && !hasSelfPaintingLayerDescendant())
    return nullptr;

  ClipRectsCacheSlot clipRectsCacheSlot =
      result.hitTestRequest().ignoreClipping()
          ? RootRelativeClipRectsIgnoringViewportClip
          : RootRelativeClipRects;

  // Apply a transform if we have one.
  if (transform() && !appliedTransform) {
    if (enclosingPaginationLayer())
      return hitTestTransformedLayerInFragments(
          rootLayer, containerLayer, result, hitTestRect, hitTestLocation,
          transformState, zOffset, clipRectsCacheSlot);

    // Make sure the parent's clip rects have been calculated.
    if (parent()) {
      ClipRect clipRect = clipper(PaintLayer::DoNotUseGeometryMapper)
                              .backgroundClipRect(ClipRectsContext(
                                  rootLayer, clipRectsCacheSlot,
                                  ExcludeOverlayScrollbarSizeForHitTesting));
      // Go ahead and test the enclosing clip now.
      if (!clipRect.intersects(hitTestLocation))
        return nullptr;
    }

    return hitTestLayerByApplyingTransform(rootLayer, containerLayer, result,
                                           hitTestRect, hitTestLocation,
                                           transformState, zOffset);
  }

  if (hitTestClippedOutByClipPath(rootLayer, hitTestLocation))
    return nullptr;

  // The natural thing would be to keep HitTestingTransformState on the stack,
  // but it's big, so we heap-allocate.
  RefPtr<HitTestingTransformState> localTransformState;
  if (appliedTransform) {
    // We computed the correct state in the caller (above code), so just
    // reference it.
    DCHECK(transformState);
    localTransformState = const_cast<HitTestingTransformState*>(transformState);
  } else if (transformState || m_has3DTransformedDescendant || preserves3D()) {
    // We need transform state for the first time, or to offset the container
    // state, so create it here.
    localTransformState =
        createLocalTransformState(rootLayer, containerLayer, hitTestRect,
                                  hitTestLocation, transformState);
  }

  // Check for hit test on backface if backface-visibility is 'hidden'
  if (localTransformState &&
      layoutObject().style()->backfaceVisibility() ==
          BackfaceVisibilityHidden) {
    TransformationMatrix invertedMatrix =
        localTransformState->m_accumulatedTransform.inverse();
    // If the z-vector of the matrix is negative, the back is facing towards the
    // viewer.
    if (invertedMatrix.m33() < 0)
      return nullptr;
  }

  RefPtr<HitTestingTransformState> unflattenedTransformState =
      localTransformState;
  if (localTransformState && !preserves3D()) {
    // Keep a copy of the pre-flattening state, for computing z-offsets for the
    // container
    unflattenedTransformState =
        HitTestingTransformState::create(*localTransformState);
    // This layer is flattening, so flatten the state passed to descendants.
    localTransformState->flatten();
  }

  // The following are used for keeping track of the z-depth of the hit point of
  // 3d-transformed descendants.
  double localZOffset = -std::numeric_limits<double>::infinity();
  double* zOffsetForDescendantsPtr = 0;
  double* zOffsetForContentsPtr = 0;

  bool depthSortDescendants = false;
  if (preserves3D()) {
    depthSortDescendants = true;
    // Our layers can depth-test with our container, so share the z depth
    // pointer with the container, if it passed one down.
    zOffsetForDescendantsPtr = zOffset ? zOffset : &localZOffset;
    zOffsetForContentsPtr = zOffset ? zOffset : &localZOffset;
  } else if (zOffset) {
    zOffsetForDescendantsPtr = 0;
    // Container needs us to give back a z offset for the hit layer.
    zOffsetForContentsPtr = zOffset;
  }

  // This variable tracks which layer the mouse ends up being inside.
  PaintLayer* candidateLayer = 0;

  // Begin by walking our list of positive layers from highest z-index down to
  // the lowest z-index.
  PaintLayer* hitLayer = hitTestChildren(
      PositiveZOrderChildren, rootLayer, result, hitTestRect, hitTestLocation,
      localTransformState.get(), zOffsetForDescendantsPtr, zOffset,
      unflattenedTransformState.get(), depthSortDescendants);
  if (hitLayer) {
    if (!depthSortDescendants)
      return hitLayer;
    candidateLayer = hitLayer;
  }

  // Now check our overflow objects.
  hitLayer = hitTestChildren(
      NormalFlowChildren, rootLayer, result, hitTestRect, hitTestLocation,
      localTransformState.get(), zOffsetForDescendantsPtr, zOffset,
      unflattenedTransformState.get(), depthSortDescendants);
  if (hitLayer) {
    if (!depthSortDescendants)
      return hitLayer;
    candidateLayer = hitLayer;
  }

  // Collect the fragments. This will compute the clip rectangles for each layer
  // fragment.
  PaintLayerFragments layerFragments;
  if (appliedTransform)
    appendSingleFragmentIgnoringPagination(
        layerFragments, rootLayer, hitTestRect, clipRectsCacheSlot,
        PaintLayer::DoNotUseGeometryMapper,
        ExcludeOverlayScrollbarSizeForHitTesting);
  else
    collectFragments(layerFragments, rootLayer, hitTestRect, clipRectsCacheSlot,
                     PaintLayer::DoNotUseGeometryMapper,
                     ExcludeOverlayScrollbarSizeForHitTesting);

  if (m_scrollableArea &&
      m_scrollableArea->hitTestResizerInFragments(layerFragments,
                                                  hitTestLocation)) {
    layoutObject().updateHitTestResult(result, hitTestLocation.point());
    return this;
  }

  // Next we want to see if the mouse pos is inside the child LayoutObjects of
  // the layer. Check every fragment in reverse order.
  if (isSelfPaintingLayer()) {
    // Hit test with a temporary HitTestResult, because we only want to commit
    // to 'result' if we know we're frontmost.
    HitTestResult tempResult(result.hitTestRequest(), result.hitTestLocation());
    bool insideFragmentForegroundRect = false;
    if (hitTestContentsForFragments(layerFragments, tempResult, hitTestLocation,
                                    HitTestDescendants,
                                    insideFragmentForegroundRect) &&
        isHitCandidate(this, false, zOffsetForContentsPtr,
                       unflattenedTransformState.get())) {
      if (result.hitTestRequest().listBased())
        result.append(tempResult);
      else
        result = tempResult;
      if (!depthSortDescendants)
        return this;
      // Foreground can depth-sort with descendant layers, so keep this as a
      // candidate.
      candidateLayer = this;
    } else if (insideFragmentForegroundRect &&
               result.hitTestRequest().listBased()) {
      result.append(tempResult);
    }
  }

  // Now check our negative z-index children.
  hitLayer = hitTestChildren(
      NegativeZOrderChildren, rootLayer, result, hitTestRect, hitTestLocation,
      localTransformState.get(), zOffsetForDescendantsPtr, zOffset,
      unflattenedTransformState.get(), depthSortDescendants);
  if (hitLayer) {
    if (!depthSortDescendants)
      return hitLayer;
    candidateLayer = hitLayer;
  }

  // If we found a layer, return. Child layers, and foreground always render in
  // front of background.
  if (candidateLayer)
    return candidateLayer;

  if (isSelfPaintingLayer()) {
    HitTestResult tempResult(result.hitTestRequest(), result.hitTestLocation());
    bool insideFragmentBackgroundRect = false;
    if (hitTestContentsForFragments(layerFragments, tempResult, hitTestLocation,
                                    HitTestSelf,
                                    insideFragmentBackgroundRect) &&
        isHitCandidate(this, false, zOffsetForContentsPtr,
                       unflattenedTransformState.get())) {
      if (result.isRectBasedTest())
        result.append(tempResult);
      else
        result = tempResult;
      return this;
    }
    if (insideFragmentBackgroundRect && result.hitTestRequest().listBased())
      result.append(tempResult);
  }

  return nullptr;
}

bool PaintLayer::hitTestContentsForFragments(
    const PaintLayerFragments& layerFragments,
    HitTestResult& result,
    const HitTestLocation& hitTestLocation,
    HitTestFilter hitTestFilter,
    bool& insideClipRect) const {
  if (layerFragments.isEmpty())
    return false;

  for (int i = layerFragments.size() - 1; i >= 0; --i) {
    const PaintLayerFragment& fragment = layerFragments.at(i);
    if ((hitTestFilter == HitTestSelf &&
         !fragment.backgroundRect.intersects(hitTestLocation)) ||
        (hitTestFilter == HitTestDescendants &&
         !fragment.foregroundRect.intersects(hitTestLocation)))
      continue;
    insideClipRect = true;
    if (hitTestContents(result, fragment.layerBounds, hitTestLocation,
                        hitTestFilter))
      return true;
  }

  return false;
}

PaintLayer* PaintLayer::hitTestTransformedLayerInFragments(
    PaintLayer* rootLayer,
    PaintLayer* containerLayer,
    HitTestResult& result,
    const LayoutRect& hitTestRect,
    const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState,
    double* zOffset,
    ClipRectsCacheSlot clipRectsCacheSlot) {
  PaintLayerFragments enclosingPaginationFragments;
  LayoutPoint offsetOfPaginationLayerFromRoot;
  // FIXME: We're missing a sub-pixel offset here crbug.com/348728
  LayoutRect transformedExtent = transparencyClipBox(
      this, enclosingPaginationLayer(), HitTestingTransparencyClipBox,
      PaintLayer::RootOfTransparencyClipBox, LayoutSize());
  enclosingPaginationLayer()->collectFragments(
      enclosingPaginationFragments, rootLayer, hitTestRect, clipRectsCacheSlot,
      PaintLayer::DoNotUseGeometryMapper,
      ExcludeOverlayScrollbarSizeForHitTesting, RespectOverflowClip,
      &offsetOfPaginationLayerFromRoot, LayoutSize(), &transformedExtent);

  for (int i = enclosingPaginationFragments.size() - 1; i >= 0; --i) {
    const PaintLayerFragment& fragment = enclosingPaginationFragments.at(i);

    // Apply the page/column clip for this fragment, as well as any clips
    // established by layers in between us and the enclosing pagination layer.
    LayoutRect clipRect = fragment.backgroundRect.rect();

    // Now compute the clips within a given fragment
    if (parent() != enclosingPaginationLayer()) {
      enclosingPaginationLayer()->convertToLayerCoords(
          rootLayer, offsetOfPaginationLayerFromRoot);
      LayoutRect parentClipRect =
          clipper(PaintLayer::DoNotUseGeometryMapper)
              .backgroundClipRect(ClipRectsContext(
                  enclosingPaginationLayer(), clipRectsCacheSlot,
                  ExcludeOverlayScrollbarSizeForHitTesting))
              .rect();
      parentClipRect.moveBy(fragment.paginationOffset +
                            offsetOfPaginationLayerFromRoot);
      clipRect.intersect(parentClipRect);
    }

    if (!hitTestLocation.intersects(clipRect))
      continue;

    PaintLayer* hitLayer = hitTestLayerByApplyingTransform(
        rootLayer, containerLayer, result, hitTestRect, hitTestLocation,
        transformState, zOffset, fragment.paginationOffset);
    if (hitLayer)
      return hitLayer;
  }

  return 0;
}

PaintLayer* PaintLayer::hitTestLayerByApplyingTransform(
    PaintLayer* rootLayer,
    PaintLayer* containerLayer,
    HitTestResult& result,
    const LayoutRect& hitTestRect,
    const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState,
    double* zOffset,
    const LayoutPoint& translationOffset) {
  // Create a transform state to accumulate this transform.
  RefPtr<HitTestingTransformState> newTransformState =
      createLocalTransformState(rootLayer, containerLayer, hitTestRect,
                                hitTestLocation, transformState,
                                translationOffset);

  // If the transform can't be inverted, then don't hit test this layer at all.
  if (!newTransformState->m_accumulatedTransform.isInvertible())
    return 0;

  // Compute the point and the hit test rect in the coords of this layer by
  // using the values from the transformState, which store the point and quad in
  // the coords of the last flattened layer, and the accumulated transform which
  // lets up map through preserve-3d layers.
  //
  // We can't just map hitTestLocation and hitTestRect because they may have
  // been flattened (losing z) by our container.
  FloatPoint localPoint = newTransformState->mappedPoint();
  FloatQuad localPointQuad = newTransformState->mappedQuad();
  LayoutRect localHitTestRect = newTransformState->boundsOfMappedArea();
  HitTestLocation newHitTestLocation;
  if (hitTestLocation.isRectBasedTest())
    newHitTestLocation = HitTestLocation(localPoint, localPointQuad);
  else
    newHitTestLocation = HitTestLocation(localPoint);

  // Now do a hit test with the root layer shifted to be us.
  return hitTestLayer(this, containerLayer, result, localHitTestRect,
                      newHitTestLocation, true, newTransformState.get(),
                      zOffset);
}

bool PaintLayer::hitTestContents(HitTestResult& result,
                                 const LayoutRect& layerBounds,
                                 const HitTestLocation& hitTestLocation,
                                 HitTestFilter hitTestFilter) const {
  DCHECK(isSelfPaintingLayer() || hasSelfPaintingLayerDescendant());

  if (!layoutObject().hitTest(
          result, hitTestLocation,
          toLayoutPoint(layerBounds.location() - layoutBoxLocation()),
          hitTestFilter)) {
    // It's wrong to set innerNode, but then claim that you didn't hit anything,
    // unless it is a rect-based test.
    DCHECK(!result.innerNode() || (result.hitTestRequest().listBased() &&
                                   result.listBasedTestResult().size()));
    return false;
  }

  if (!result.innerNode()) {
    // We hit something anonymous, and we didn't find a DOM node ancestor in
    // this layer.

    if (layoutObject().isLayoutFlowThread()) {
      // For a flow thread it's safe to just say that we didn't hit anything.
      // That means that we'll continue as normally, and eventually hit a column
      // set sibling instead. Column sets are also anonymous, but, unlike flow
      // threads, they don't establish layers, so we'll fall back and hit the
      // multicol container parent (which should have a DOM node).
      return false;
    }

    Node* e = enclosingNode();
    // FIXME: should be a call to result.setNodeAndPosition. What we would
    // really want to do here is to return and look for the nearest
    // non-anonymous ancestor, and ignore aunts and uncles on our way. It's bad
    // to look for it manually like we do here, and give up on setting a local
    // point in the result, because that has bad implications for text selection
    // and caretRangeFromPoint(). See crbug.com/461791
    if (!result.innerNode())
      result.setInnerNode(e);
  }
  return true;
}

PaintLayer* PaintLayer::hitTestChildren(
    ChildrenIteration childrentoVisit,
    PaintLayer* rootLayer,
    HitTestResult& result,
    const LayoutRect& hitTestRect,
    const HitTestLocation& hitTestLocation,
    const HitTestingTransformState* transformState,
    double* zOffsetForDescendants,
    double* zOffset,
    const HitTestingTransformState* unflattenedTransformState,
    bool depthSortDescendants) {
  if (!hasSelfPaintingLayerDescendant())
    return 0;

  PaintLayer* resultLayer = 0;
  PaintLayerStackingNodeReverseIterator iterator(*m_stackingNode,
                                                 childrentoVisit);
  while (PaintLayerStackingNode* child = iterator.next()) {
    PaintLayer* childLayer = child->layer();
    PaintLayer* hitLayer = 0;
    HitTestResult tempResult(result.hitTestRequest(), result.hitTestLocation());
    hitLayer = childLayer->hitTestLayer(rootLayer, this, tempResult,
                                        hitTestRect, hitTestLocation, false,
                                        transformState, zOffsetForDescendants);

    // If it is a list-based test, we can safely append the temporary result
    // since it might had hit nodes but not necesserily had hitLayer set.
    DCHECK(!result.isRectBasedTest() || result.hitTestRequest().listBased());
    if (result.hitTestRequest().listBased())
      result.append(tempResult);

    if (isHitCandidate(hitLayer, depthSortDescendants, zOffset,
                       unflattenedTransformState)) {
      resultLayer = hitLayer;
      if (!result.hitTestRequest().listBased())
        result = tempResult;
      if (!depthSortDescendants)
        break;
    }
  }

  return resultLayer;
}

FloatRect PaintLayer::boxForFilterOrMask() const {
  return FloatRect(physicalBoundingBoxIncludingStackingChildren(
      LayoutPoint(), PaintLayer::CalculateBoundsOptions::
                         IncludeTransformsAndCompositedChildLayers));
}

LayoutRect PaintLayer::boxForClipPath() const {
  if (!layoutObject().isBox()) {
    SECURITY_DCHECK(layoutObject().isLayoutInline());
    const LayoutInline& layoutInline = toLayoutInline(layoutObject());
    // This somewhat convoluted computation matches what Gecko does.
    // See crbug.com/641907.
    LayoutRect inlineBBox = layoutInline.linesBoundingBox();
    const InlineFlowBox* flowBox = layoutInline.firstLineBox();
    inlineBBox.setHeight(flowBox ? flowBox->frameRect().height()
                                 : LayoutUnit(0));
    return inlineBBox;
  }
  return toLayoutBox(layoutObject()).borderBoxRect();
}

bool PaintLayer::hitTestClippedOutByClipPath(
    PaintLayer* rootLayer,
    const HitTestLocation& hitTestLocation) const {
  if (!layoutObject().hasClipPath())
    return false;
  DCHECK(isSelfPaintingLayer());
  DCHECK(rootLayer);

  LayoutRect referenceBox(boxForClipPath());
  if (enclosingPaginationLayer())
    convertFromFlowThreadToVisualBoundingBoxInAncestor(rootLayer, referenceBox);
  else
    convertToLayerCoords(rootLayer, referenceBox);

  FloatPoint point(hitTestLocation.point());

  ClipPathOperation* clipPathOperation = layoutObject().style()->clipPath();
  DCHECK(clipPathOperation);
  if (clipPathOperation->type() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation* clipPath =
        toShapeClipPathOperation(clipPathOperation);
    return !clipPath->path(FloatRect(referenceBox)).contains(point);
  }
  DCHECK_EQ(clipPathOperation->type(), ClipPathOperation::REFERENCE);
  Node* targetNode = layoutObject().node();
  if (!targetNode)
    return false;
  const ReferenceClipPathOperation& referenceClipPathOperation =
      toReferenceClipPathOperation(*clipPathOperation);
  SVGElement* element =
      referenceClipPathOperation.findElement(targetNode->treeScope());
  if (!isSVGClipPathElement(element) || !element->layoutObject())
    return false;
  LayoutSVGResourceClipper* clipper = toLayoutSVGResourceClipper(
      toLayoutSVGResourceContainer(element->layoutObject()));
  // If the clipPath is using "userspace on use" units, then the origin of
  // the coordinate system is the top-left of the reference box, so adjust
  // the point accordingly.
  if (clipper->clipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse)
    point.moveBy(-referenceBox.location());
  return !clipper->hitTestClipContent(FloatRect(referenceBox), point);
}

bool PaintLayer::intersectsDamageRect(const LayoutRect& layerBounds,
                                      const LayoutRect& damageRect,
                                      const LayoutPoint& offsetFromRoot) const {
  // Always examine the canvas and the root.
  // FIXME: Could eliminate the isDocumentElement() check if we fix background
  // painting so that the LayoutView paints the root's background.
  if (isRootLayer() || layoutObject().isDocumentElement())
    return true;

  // If we aren't an inline flow, and our layer bounds do intersect the damage
  // rect, then we can go ahead and return true.
  LayoutView* view = layoutObject().view();
  DCHECK(view);
  if (view && !layoutObject().isLayoutInline()) {
    if (layerBounds.intersects(damageRect))
      return true;
  }

  // Otherwise we need to compute the bounding box of this single layer and see
  // if it intersects the damage rect.
  return physicalBoundingBox(offsetFromRoot).intersects(damageRect);
}

LayoutRect PaintLayer::logicalBoundingBox() const {
  LayoutRect rect = layoutObject().visualOverflowRect();

  if (isRootLayer()) {
    rect.unite(
        LayoutRect(rect.location(), layoutObject().view()->viewRect().size()));
  }

  return rect;
}

static inline LayoutRect flippedLogicalBoundingBox(LayoutRect boundingBox,
                                                   LayoutObject& layoutObject) {
  LayoutRect result = boundingBox;
  if (layoutObject.isBox())
    toLayoutBox(layoutObject).flipForWritingMode(result);
  else
    layoutObject.containingBlock()->flipForWritingMode(result);
  return result;
}

LayoutRect PaintLayer::physicalBoundingBox(
    const PaintLayer* ancestorLayer) const {
  LayoutPoint offsetFromRoot;
  convertToLayerCoords(ancestorLayer, offsetFromRoot);
  return physicalBoundingBox(offsetFromRoot);
}

LayoutRect PaintLayer::physicalBoundingBox(
    const LayoutPoint& offsetFromRoot) const {
  LayoutRect result =
      flippedLogicalBoundingBox(logicalBoundingBox(), layoutObject());
  result.moveBy(offsetFromRoot);
  return result;
}

LayoutRect PaintLayer::fragmentsBoundingBox(
    const PaintLayer* ancestorLayer) const {
  if (!enclosingPaginationLayer())
    return physicalBoundingBox(ancestorLayer);

  LayoutRect result =
      flippedLogicalBoundingBox(logicalBoundingBox(), layoutObject());
  convertFromFlowThreadToVisualBoundingBoxInAncestor(ancestorLayer, result);
  return result;
}

LayoutRect PaintLayer::boundingBoxForCompositingOverlapTest() const {
  // Apply NeverIncludeTransformForAncestorLayer, because the geometry map in
  // CompositingInputsUpdater will take care of applying the transform of |this|
  // (== the ancestorLayer argument to boundingBoxForCompositing).
  // TODO(trchen): Layer fragmentation is inhibited across compositing boundary.
  // Should we return the unfragmented bounds for overlap testing? Or perhaps
  // assume fragmented layers always overlap?
  return overlapBoundsIncludeChildren()
             ? boundingBoxForCompositingInternal(
                   *this, nullptr, NeverIncludeTransformForAncestorLayer)
             : fragmentsBoundingBox(this);
}

bool PaintLayer::overlapBoundsIncludeChildren() const {
  return hasFilterThatMovesPixels();
}

void PaintLayer::expandRectForStackingChildren(
    const PaintLayer& compositedLayer,
    LayoutRect& result,
    PaintLayer::CalculateBoundsOptions options) const {
  DCHECK(stackingNode()->isStackingContext() ||
         !stackingNode()->hasPositiveZOrderList());

#if DCHECK_IS_ON()
  LayerListMutationDetector mutationChecker(
      const_cast<PaintLayer*>(this)->stackingNode());
#endif

  PaintLayerStackingNodeIterator iterator(*this->stackingNode(), AllChildren);
  while (PaintLayerStackingNode* node = iterator.next()) {
    // Here we exclude both directly composited layers and squashing layers
    // because those Layers don't paint into the graphics layer
    // for this Layer. For example, the bounds of squashed Layers
    // will be included in the computation of the appropriate squashing
    // GraphicsLayer.
    if (options != PaintLayer::CalculateBoundsOptions::
                       IncludeTransformsAndCompositedChildLayers &&
        node->layer()->compositingState() != NotComposited)
      continue;
    result.unite(node->layer()->boundingBoxForCompositingInternal(
        compositedLayer, this, options));
  }
}

LayoutRect PaintLayer::physicalBoundingBoxIncludingStackingChildren(
    const LayoutPoint& offsetFromRoot,
    CalculateBoundsOptions options) const {
  LayoutRect result = physicalBoundingBox(LayoutPoint());

  const_cast<PaintLayer*>(this)->stackingNode()->updateLayerListsIfNeeded();

  expandRectForStackingChildren(*this, result, options);

  result.moveBy(offsetFromRoot);
  return result;
}

LayoutRect PaintLayer::boundingBoxForCompositing() const {
  return boundingBoxForCompositingInternal(
      *this, nullptr, MaybeIncludeTransformForAncestorLayer);
}

LayoutRect PaintLayer::boundingBoxForCompositingInternal(
    const PaintLayer& compositedLayer,
    const PaintLayer* stackingParent,
    CalculateBoundsOptions options) const {
  if (!isSelfPaintingLayer())
    return LayoutRect();

  // FIXME: This could be improved to do a check like
  // hasVisibleNonCompositingDescendantLayers() (bug 92580).
  if (this != &compositedLayer && !hasVisibleContent() &&
      !hasVisibleDescendant())
    return LayoutRect();

  // The root layer is the size of the document, plus any additional area due
  // to layout viewport being different than initial containing block.
  if (isRootLayer()) {
    IntRect documentRect = layoutObject().view()->documentRect();

    if (FrameView* frameView = layoutObject().document().view()) {
      documentRect.unite(IntRect(IntPoint(), frameView->visibleContentSize()));
    }

    return LayoutRect(documentRect);
  }

  // The layer created for the LayoutFlowThread is just a helper for painting
  // and hit-testing, and should not contribute to the bounding box. The
  // LayoutMultiColumnSets will contribute the correct size for the layout
  // content of the multicol container.
  if (layoutObject().isLayoutFlowThread())
    return LayoutRect();

  const_cast<PaintLayer*>(this)->stackingNode()->updateLayerListsIfNeeded();

  // If there is a clip applied by an ancestor to this PaintLayer but below or
  // equal to |ancestorLayer|, apply that clip.
  LayoutRect result = clipper(PaintLayer::DoNotUseGeometryMapper)
                          .localClipRect(compositedLayer);

  result.intersect(physicalBoundingBox(LayoutPoint()));

  expandRectForStackingChildren(compositedLayer, result, options);

  // Only enlarge by the filter outsets if we know the filter is going to be
  // rendered in software.  Accelerated filters will handle their own outsets.
  if (paintsWithFilters())
    result = mapLayoutRectForFilter(result);

  if (transform() && (options == IncludeTransformsAndCompositedChildLayers ||
                      ((paintsWithTransform(GlobalPaintNormalPhase) &&
                        (this != &compositedLayer ||
                         options == MaybeIncludeTransformForAncestorLayer)))))
    result = transform()->mapRect(result);

  if (shouldFragmentCompositedBounds(&compositedLayer)) {
    convertFromFlowThreadToVisualBoundingBoxInAncestor(&compositedLayer,
                                                       result);
    return result;
  }

  if (stackingParent) {
    LayoutPoint delta;
    convertToLayerCoords(stackingParent, delta);
    result.moveBy(delta);
  }
  return result;
}

CompositingState PaintLayer::compositingState() const {
  DCHECK(isAllowedToQueryCompositingState());

  // This is computed procedurally so there is no redundant state variable that
  // can get out of sync from the real actual compositing state.

  if (groupedMapping()) {
    DCHECK(!compositedLayerMapping());
    return PaintsIntoGroupedBacking;
  }

  if (!compositedLayerMapping())
    return NotComposited;

  return PaintsIntoOwnBacking;
}

bool PaintLayer::isAllowedToQueryCompositingState() const {
  if (gCompositingQueryMode == CompositingQueriesAreAllowed ||
      RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return true;
  return layoutObject().document().lifecycle().state() >=
         DocumentLifecycle::InCompositingUpdate;
}

CompositedLayerMapping* PaintLayer::compositedLayerMapping() const {
  DCHECK(isAllowedToQueryCompositingState());
  return m_rareData ? m_rareData->compositedLayerMapping.get() : nullptr;
}

GraphicsLayer* PaintLayer::graphicsLayerBacking(const LayoutObject* obj) const {
  switch (compositingState()) {
    case NotComposited:
      return nullptr;
    case PaintsIntoGroupedBacking:
      return groupedMapping()->squashingLayer();
    default:
      return (obj != &layoutObject() &&
              compositedLayerMapping()->scrollingContentsLayer())
                 ? compositedLayerMapping()->scrollingContentsLayer()
                 : compositedLayerMapping()->mainGraphicsLayer();
  }
}

BackgroundPaintLocation PaintLayer::backgroundPaintLocation(
    uint32_t* reasons) const {
  BackgroundPaintLocation location;
  if (!scrollsOverflow()) {
    location = BackgroundPaintInGraphicsLayer;
  } else if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    location = layoutObject().backgroundPaintLocation(reasons);
  } else {
    location = isRootLayer() ? BackgroundPaintInGraphicsLayer
                             : layoutObject().backgroundPaintLocation(reasons);
  }
  m_stackingNode->updateLayerListsIfNeeded();
  if (m_stackingNode->hasNegativeZOrderList())
    location = BackgroundPaintInGraphicsLayer;
  return location;
}

void PaintLayer::ensureCompositedLayerMapping() {
  if (m_rareData && m_rareData->compositedLayerMapping)
    return;

  ensureRareData().compositedLayerMapping =
      WTF::wrapUnique(new CompositedLayerMapping(*this));
  m_rareData->compositedLayerMapping->setNeedsGraphicsLayerUpdate(
      GraphicsLayerUpdateSubtree);

  if (PaintLayerResourceInfo* resourceInfo = this->resourceInfo())
    resourceInfo->invalidateFilterChain();
}

void PaintLayer::clearCompositedLayerMapping(bool layerBeingDestroyed) {
  if (!layerBeingDestroyed) {
    // We need to make sure our decendants get a geometry update. In principle,
    // we could call setNeedsGraphicsLayerUpdate on our children, but that would
    // require walking the z-order lists to find them. Instead, we
    // over-invalidate by marking our parent as needing a geometry update.
    if (PaintLayer* compositingParent =
            enclosingLayerWithCompositedLayerMapping(ExcludeSelf))
      compositingParent->compositedLayerMapping()->setNeedsGraphicsLayerUpdate(
          GraphicsLayerUpdateSubtree);
  }

  if (m_rareData)
    m_rareData->compositedLayerMapping.reset();

  if (layerBeingDestroyed)
    return;

  if (PaintLayerResourceInfo* resourceInfo = this->resourceInfo())
    resourceInfo->invalidateFilterChain();
}

void PaintLayer::setGroupedMapping(CompositedLayerMapping* groupedMapping,
                                   SetGroupMappingOptions options) {
  CompositedLayerMapping* oldGroupedMapping = this->groupedMapping();
  if (groupedMapping == oldGroupedMapping)
    return;

  if (options == InvalidateLayerAndRemoveFromMapping && oldGroupedMapping) {
    oldGroupedMapping->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
    oldGroupedMapping->removeLayerFromSquashingGraphicsLayer(this);
  }
  if (m_rareData || groupedMapping)
    ensureRareData().groupedMapping = groupedMapping;
#if DCHECK_IS_ON()
  DCHECK(!groupedMapping || groupedMapping->verifyLayerInSquashingVector(this));
#endif
  if (options == InvalidateLayerAndRemoveFromMapping && groupedMapping)
    groupedMapping->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
}

bool PaintLayer::maskBlendingAppliedByCompositor() const {
  DCHECK(m_layoutObject.hasMask());
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled())
    return true;
  return m_rareData && m_rareData->compositedLayerMapping &&
         m_rareData->compositedLayerMapping->hasMaskLayer();
}

bool PaintLayer::hasCompositedClippingMask() const {
  return m_rareData && m_rareData->compositedLayerMapping &&
         m_rareData->compositedLayerMapping->hasChildClippingMaskLayer();
}

bool PaintLayer::paintsWithTransform(GlobalPaintFlags globalPaintFlags) const {
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    return transform() &&
           ((globalPaintFlags & GlobalPaintFlattenCompositingLayers) ||
            compositingState() != PaintsIntoOwnBacking);
  }

  return (transform() ||
          layoutObject().style()->position() == EPosition::kFixed) &&
         ((globalPaintFlags & GlobalPaintFlattenCompositingLayers) ||
          compositingState() != PaintsIntoOwnBacking);
}

bool PaintLayer::compositesWithTransform() const {
  return transformAncestor() || transform();
}

bool PaintLayer::compositesWithOpacity() const {
  return opacityAncestor() || layoutObject().style()->hasOpacity();
}

bool PaintLayer::backgroundIsKnownToBeOpaqueInRect(
    const LayoutRect& localRect) const {
  if (paintsWithTransparency(GlobalPaintNormalPhase))
    return false;

  // We can't use hasVisibleContent(), because that will be true if our
  // layoutObject is hidden, but some child is visible and that child doesn't
  // cover the entire rect.
  if (layoutObject().style()->visibility() != EVisibility::kVisible)
    return false;

  if (paintsWithFilters() &&
      layoutObject().style()->filter().hasFilterThatAffectsOpacity())
    return false;

  // FIXME: Handle simple transforms.
  if (transform() && compositingState() != PaintsIntoOwnBacking)
    return false;

  if (!RuntimeEnabledFeatures::compositeOpaqueFixedPositionEnabled() &&
      layoutObject().style()->position() == EPosition::kFixed &&
      compositingState() != PaintsIntoOwnBacking)
    return false;

  // This function should not be called when layer-lists are dirty.
  // TODO(schenney) This check never hits in layout tests or most platforms, but
  // does hit in PopupBlockerBrowserTest.AllowPopupThroughContentSetting on
  // Win 7 Test Builder.
  if (m_stackingNode->zOrderListsDirty())
    return false;

  // FIXME: We currently only check the immediate layoutObject,
  // which will miss many cases where additional layout objects paint
  // into this layer.
  if (layoutObject().backgroundIsKnownToBeOpaqueInRect(localRect))
    return true;

  // We can't consult child layers if we clip, since they might cover
  // parts of the rect that are clipped out.
  if (layoutObject().hasClipRelatedProperty())
    return false;

  // TODO(schenney): This could be improved by unioning the opaque regions of
  // all the children.  That would require a refactoring because currently
  // children just check they at least cover the given rect, but a unioning
  // method would require children to compute and report their rects.
  return childBackgroundIsKnownToBeOpaqueInRect(localRect);
}

bool PaintLayer::childBackgroundIsKnownToBeOpaqueInRect(
    const LayoutRect& localRect) const {
  PaintLayerStackingNodeReverseIterator reverseIterator(
      *m_stackingNode,
      PositiveZOrderChildren | NormalFlowChildren | NegativeZOrderChildren);
  while (PaintLayerStackingNode* child = reverseIterator.next()) {
    const PaintLayer* childLayer = child->layer();
    // Stop at composited paint boundaries and non-self-painting layers.
    if (childLayer->isPaintInvalidationContainer())
      continue;

    if (!childLayer->canUseConvertToLayerCoords())
      continue;

    LayoutPoint childOffset;
    LayoutRect childLocalRect(localRect);
    childLayer->convertToLayerCoords(this, childOffset);
    childLocalRect.moveBy(-childOffset);

    if (childLayer->backgroundIsKnownToBeOpaqueInRect(childLocalRect))
      return true;
  }
  return false;
}

bool PaintLayer::shouldBeSelfPaintingLayer() const {
  if (layoutObject().isLayoutPart() &&
      toLayoutPart(layoutObject()).requiresAcceleratedCompositing())
    return true;

  return layoutObject().layerTypeRequired() == NormalPaintLayer ||
         (m_scrollableArea && m_scrollableArea->hasOverlayScrollbars()) ||
         needsCompositedScrolling();
}

void PaintLayer::updateSelfPaintingLayer() {
  bool isSelfPaintingLayer = shouldBeSelfPaintingLayer();
  if (this->isSelfPaintingLayer() == isSelfPaintingLayer)
    return;

  m_isSelfPaintingLayer = isSelfPaintingLayer;
  m_selfPaintingStatusChanged = true;

  if (PaintLayer* parent = this->parent()) {
    parent->dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    if (PaintLayer* enclosingSelfPaintingLayer =
            parent->enclosingSelfPaintingLayer()) {
      if (isSelfPaintingLayer)
        mergeNeedsPaintPhaseFlagsFrom(*enclosingSelfPaintingLayer);
      else
        enclosingSelfPaintingLayer->mergeNeedsPaintPhaseFlagsFrom(*this);
    }
  }
}

PaintLayer* PaintLayer::enclosingSelfPaintingLayer() {
  PaintLayer* layer = this;
  while (layer && !layer->isSelfPaintingLayer())
    layer = layer->parent();
  return layer;
}

bool PaintLayer::hasNonEmptyChildLayoutObjects() const {
  // Some HTML can cause whitespace text nodes to have layoutObjects, like:
  // <div>
  // <img src=...>
  // </div>
  // so test for 0x0 LayoutTexts here
  for (LayoutObject* child = layoutObject().slowFirstChild(); child;
       child = child->nextSibling()) {
    if (!child->hasLayer()) {
      if (child->isLayoutInline() || !child->isBox())
        return true;

      if (toLayoutBox(child)->size().width() > 0 ||
          toLayoutBox(child)->size().height() > 0)
        return true;
    }
  }
  return false;
}

bool PaintLayer::hasBoxDecorationsOrBackground() const {
  return layoutObject().style()->hasBoxDecorations() ||
         layoutObject().style()->hasBackground();
}

bool PaintLayer::hasVisibleBoxDecorations() const {
  if (!hasVisibleContent())
    return false;

  return hasBoxDecorationsOrBackground() || hasOverflowControls();
}

void PaintLayer::updateFilters(const ComputedStyle* oldStyle,
                               const ComputedStyle& newStyle) {
  if (!newStyle.hasFilterInducingProperty() &&
      (!oldStyle || !oldStyle->hasFilterInducingProperty()))
    return;
  const bool hadResourceInfo = resourceInfo();
  if (newStyle.hasFilterInducingProperty())
    newStyle.filter().addClient(&ensureResourceInfo());
  if (hadResourceInfo && oldStyle)
    oldStyle->filter().removeClient(resourceInfo());
  if (PaintLayerResourceInfo* resourceInfo = this->resourceInfo())
    resourceInfo->invalidateFilterChain();
}

void PaintLayer::updateClipPath(const ComputedStyle* oldStyle,
                                const ComputedStyle& newStyle) {
  ClipPathOperation* newClipOperation = newStyle.clipPath();
  ClipPathOperation* oldClipOperation =
      oldStyle ? oldStyle->clipPath() : nullptr;
  if (!newClipOperation && !oldClipOperation)
    return;
  const bool hadResourceInfo = resourceInfo();
  if (isReferenceClipPath(newClipOperation)) {
    toReferenceClipPathOperation(newClipOperation)
        ->addClient(&ensureResourceInfo());
  }
  if (hadResourceInfo && isReferenceClipPath(oldClipOperation)) {
    toReferenceClipPathOperation(oldClipOperation)
        ->removeClient(resourceInfo());
  }
}

bool PaintLayer::attemptDirectCompositingUpdate(StyleDifference diff,
                                                const ComputedStyle* oldStyle) {
  CompositingReasons oldPotentialCompositingReasonsFromStyle =
      potentialCompositingReasonsFromStyle();
  compositor()->updatePotentialCompositingReasonsFromStyle(this);

  // This function implements an optimization for transforms and opacity.
  // A common pattern is for a touchmove handler to update the transform
  // and/or an opacity of an element every frame while the user moves their
  // finger across the screen. The conditions below recognize when the
  // compositing state is set up to receive a direct transform or opacity
  // update.

  if (!diff.hasAtMostPropertySpecificDifferences(
          StyleDifference::TransformChanged | StyleDifference::OpacityChanged))
    return false;
  // The potentialCompositingReasonsFromStyle could have changed without
  // a corresponding StyleDifference if an animation started or ended.
  if (potentialCompositingReasonsFromStyle() !=
      oldPotentialCompositingReasonsFromStyle)
    return false;
  if (!m_rareData || !m_rareData->compositedLayerMapping)
    return false;

  // To cut off almost all the work in the compositing update for
  // this case, we treat inline transforms has having assumed overlap
  // (similar to how we treat animated transforms). Notice that we read
  // CompositingReasonInlineTransform from the m_compositingReasons, which
  // means that the inline transform actually triggered assumed overlap in
  // the overlap map.
  if (diff.transformChanged() &&
      (!m_rareData ||
       !(m_rareData->compositingReasons & CompositingReasonInlineTransform)))
    return false;

  // We composite transparent Layers differently from non-transparent
  // Layers even when the non-transparent Layers are already a
  // stacking context.
  if (diff.opacityChanged() &&
      m_layoutObject.style()->hasOpacity() != oldStyle->hasOpacity())
    return false;

  // Changes in pointer-events affect hit test visibility of the scrollable
  // area and its |m_scrollsOverflow| value which determines if the layer
  // requires composited scrolling or not.
  if (m_scrollableArea &&
      m_layoutObject.style()->pointerEvents() != oldStyle->pointerEvents())
    return false;

  updateTransform(oldStyle, layoutObject().styleRef());

  // FIXME: Consider introducing a smaller graphics layer update scope
  // that just handles transforms and opacity. GraphicsLayerUpdateLocal
  // will also program bounds, clips, and many other properties that could
  // not possibly have changed.
  m_rareData->compositedLayerMapping->setNeedsGraphicsLayerUpdate(
      GraphicsLayerUpdateLocal);
  compositor()->setNeedsCompositingUpdate(CompositingUpdateAfterGeometryChange);

  if (m_scrollableArea)
    m_scrollableArea->updateAfterStyleChange(oldStyle);

  return true;
}

void PaintLayer::styleDidChange(StyleDifference diff,
                                const ComputedStyle* oldStyle) {
  if (attemptDirectCompositingUpdate(diff, oldStyle))
    return;

  m_stackingNode->styleDidChange(oldStyle);

  if (m_scrollableArea)
    m_scrollableArea->updateAfterStyleChange(oldStyle);

  // Overlay scrollbars can make this layer self-painting so we need
  // to recompute the bit once scrollbars have been updated.
  updateSelfPaintingLayer();

  updateTransform(oldStyle, layoutObject().styleRef());
  updateFilters(oldStyle, layoutObject().styleRef());
  updateClipPath(oldStyle, layoutObject().styleRef());

  setNeedsCompositingInputsUpdate();
  layoutObject().setNeedsPaintPropertyUpdate();
}

PaintLayerClipper PaintLayer::clipper(
    GeometryMapperOption geometryMapperOption) const {
  GeometryMapper* geometryMapper = nullptr;
  if (geometryMapperOption == UseGeometryMapper) {
    DCHECK(RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled());
    if (FrameView* frameView = m_layoutObject.document().view())
      geometryMapper = &frameView->geometryMapper();
  }
  return PaintLayerClipper(*this, geometryMapper);
}

bool PaintLayer::scrollsOverflow() const {
  if (PaintLayerScrollableArea* scrollableArea = this->getScrollableArea())
    return scrollableArea->scrollsOverflow();

  return false;
}

FilterOperations PaintLayer::addReflectionToFilterOperations(
    const ComputedStyle& style) const {
  FilterOperations filterOperations = style.filter();
  if (layoutObject().hasReflection() && layoutObject().isBox()) {
    BoxReflection reflection = boxReflectionForPaintLayer(*this, style);
    filterOperations.operations().push_back(
        BoxReflectFilterOperation::create(reflection));
  }
  return filterOperations;
}

CompositorFilterOperations
PaintLayer::createCompositorFilterOperationsForFilter(
    const ComputedStyle& style) {
  FloatRect zoomedReferenceBox;
  if (style.filter().hasReferenceFilter())
    zoomedReferenceBox = boxForFilterOrMask();
  FilterEffectBuilder builder(enclosingNode(), zoomedReferenceBox,
                              style.effectiveZoom());
  return builder.buildFilterOperations(addReflectionToFilterOperations(style));
}

CompositorFilterOperations
PaintLayer::createCompositorFilterOperationsForBackdropFilter(
    const ComputedStyle& style) {
  FloatRect zoomedReferenceBox;
  if (style.backdropFilter().hasReferenceFilter())
    zoomedReferenceBox = boxForFilterOrMask();
  FilterEffectBuilder builder(enclosingNode(), zoomedReferenceBox,
                              style.effectiveZoom());
  return builder.buildFilterOperations(style.backdropFilter());
}

PaintLayerResourceInfo& PaintLayer::ensureResourceInfo() {
  PaintLayerRareData& rareData = ensureRareData();
  if (!rareData.resourceInfo)
    rareData.resourceInfo = new PaintLayerResourceInfo(this);
  return *rareData.resourceInfo;
}

void PaintLayer::removeAncestorOverflowLayer(const PaintLayer* removedLayer) {
  // If the current ancestor overflow layer does not match the removed layer
  // the ancestor overflow layer has changed so we can stop searching.
  if (ancestorOverflowLayer() && ancestorOverflowLayer() != removedLayer)
    return;

  if (ancestorOverflowLayer()) {
    // TODO(pdr): When slimming paint v2 is enabled, we will need to
    // invalidate the scroll paint property subtree for this so main
    // thread scroll reasons are recomputed.
    ancestorOverflowLayer()
        ->getScrollableArea()
        ->invalidateStickyConstraintsFor(this);
  }
  updateAncestorOverflowLayer(nullptr);
  PaintLayer* current = m_first;
  while (current) {
    current->removeAncestorOverflowLayer(removedLayer);
    current = current->nextSibling();
  }
}

FilterEffect* PaintLayer::lastFilterEffect() const {
  // TODO(chrishtr): ensure (and assert) that compositing is clean here.
  if (!paintsWithFilters())
    return nullptr;
  PaintLayerResourceInfo* resourceInfo = this->resourceInfo();
  DCHECK(resourceInfo);

  if (resourceInfo->lastEffect())
    return resourceInfo->lastEffect();

  const ComputedStyle& style = layoutObject().styleRef();
  FloatRect zoomedReferenceBox;
  if (style.filter().hasReferenceFilter())
    zoomedReferenceBox = boxForFilterOrMask();
  FilterEffectBuilder builder(enclosingNode(), zoomedReferenceBox,
                              style.effectiveZoom());
  resourceInfo->setLastEffect(
      builder.buildFilterEffect(addReflectionToFilterOperations(style)));
  return resourceInfo->lastEffect();
}

FloatRect PaintLayer::mapRectForFilter(const FloatRect& rect) const {
  if (!hasFilterThatMovesPixels())
    return rect;

  // Ensure the filter-chain is refreshed wrt reference filters.
  // TODO(fs): Avoid having this side-effect inducing call.
  lastFilterEffect();

  FilterOperations filterOperations =
      addReflectionToFilterOperations(layoutObject().styleRef());
  return filterOperations.mapRect(rect);
}

LayoutRect PaintLayer::mapLayoutRectForFilter(const LayoutRect& rect) const {
  if (!hasFilterThatMovesPixels())
    return rect;
  return enclosingLayoutRect(mapRectForFilter(FloatRect(rect)));
}

bool PaintLayer::hasFilterThatMovesPixels() const {
  if (!hasFilterInducingProperty())
    return false;
  const ComputedStyle& style = layoutObject().styleRef();
  if (style.hasFilter() && style.filter().hasFilterThatMovesPixels())
    return true;
  if (style.hasBoxReflect())
    return true;
  return false;
}

void PaintLayer::addLayerHitTestRects(LayerHitTestRects& rects) const {
  computeSelfHitTestRects(rects);
  for (PaintLayer* child = firstChild(); child; child = child->nextSibling())
    child->addLayerHitTestRects(rects);
}

void PaintLayer::computeSelfHitTestRects(LayerHitTestRects& rects) const {
  if (!size().isEmpty()) {
    Vector<LayoutRect> rect;

    if (layoutBox() && layoutBox()->scrollsOverflow()) {
      // For scrolling layers, rects are taken to be in the space of the
      // contents.  We need to include the bounding box of the layer in the
      // space of its parent (eg. for border / scroll bars) and if it's
      // composited then the entire contents as well as they may be on another
      // composited layer. Skip reporting contents for non-composited layers as
      // they'll get projected to the same layer as the bounding box.
      if (compositingState() != NotComposited)
        rect.push_back(m_scrollableArea->overflowRect());

      rects.set(this, rect);
      if (const PaintLayer* parentLayer = parent()) {
        LayerHitTestRects::iterator iter = rects.find(parentLayer);
        if (iter == rects.end()) {
          rects.insert(parentLayer, Vector<LayoutRect>())
              .storedValue->value.push_back(physicalBoundingBox(parentLayer));
        } else {
          iter->value.push_back(physicalBoundingBox(parentLayer));
        }
      }
    } else {
      rect.push_back(logicalBoundingBox());
      rects.set(this, rect);
    }
  }
}

void PaintLayer::setNeedsRepaint() {
  setNeedsRepaintInternal();

  // Do this unconditionally to ensure container chain is marked when
  // compositing status of the layer changes.
  markCompositingContainerChainForNeedsRepaint();
}

void PaintLayer::setNeedsRepaintInternal() {
  m_needsRepaint = true;
  setDisplayItemsUncached();  // Invalidate as a display item client.
}

void PaintLayer::markCompositingContainerChainForNeedsRepaint() {
  // Need to access compositingState(). We've ensured correct flag setting when
  // compositingState() changes.
  DisableCompositingQueryAsserts disabler;

  PaintLayer* layer = this;
  while (true) {
    if (layer->compositingState() == PaintsIntoOwnBacking)
      return;
    if (CompositedLayerMapping* groupedMapping = layer->groupedMapping()) {
      // TODO(wkorman): As we clean up the CompositedLayerMapping needsRepaint
      // logic to delegate to scrollbars, we may be able to remove the line
      // below as well.
      groupedMapping->owningLayer().setNeedsRepaint();
      return;
    }

    PaintLayer* container = layer->compositingContainer();
    if (!container) {
      LayoutItem owner = layer->layoutObject().frame()->ownerLayoutItem();
      if (owner.isNull())
        break;
      container = owner.enclosingLayer();
    }

    if (container->m_needsRepaint)
      break;

    container->setNeedsRepaintInternal();
    layer = container;
  }
}

void PaintLayer::clearNeedsRepaintRecursively() {
  for (PaintLayer* child = firstChild(); child; child = child->nextSibling())
    child->clearNeedsRepaintRecursively();
  m_needsRepaint = false;
}

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
void PaintLayer::endShouldKeepAliveAllClientsRecursive() {
  for (PaintLayer* child = firstChild(); child; child = child->nextSibling())
    child->endShouldKeepAliveAllClientsRecursive();
  DisplayItemClient::endShouldKeepAliveAllClients(this);
}
#endif

DisableCompositingQueryAsserts::DisableCompositingQueryAsserts()
    : m_disabler(&gCompositingQueryMode, CompositingQueriesAreAllowed) {}

}  // namespace blink

#ifndef NDEBUG
// FIXME: Rename?
void showLayerTree(const blink::PaintLayer* layer) {
  blink::DisableCompositingQueryAsserts disabler;
  if (!layer) {
    LOG(INFO) << "Cannot showLayerTree. Root is (nil)";
    return;
  }

  if (blink::LocalFrame* frame = layer->layoutObject().frame()) {
    WTF::String output = externalRepresentation(
        frame,
        blink::LayoutAsTextShowAllLayers | blink::LayoutAsTextShowLayerNesting |
            blink::LayoutAsTextShowCompositedLayers |
            blink::LayoutAsTextShowAddresses |
            blink::LayoutAsTextShowIDAndClass |
            blink::LayoutAsTextDontUpdateLayout |
            blink::LayoutAsTextShowLayoutState,
        layer);
    LOG(INFO) << output.utf8().data();
  }
}

void showLayerTree(const blink::LayoutObject* layoutObject) {
  if (!layoutObject) {
    LOG(INFO) << "Cannot showLayerTree. Root is (nil)";
    return;
  }
  showLayerTree(layoutObject->enclosingLayer());
}
#endif
