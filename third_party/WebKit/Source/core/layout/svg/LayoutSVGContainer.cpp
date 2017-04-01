/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "core/layout/svg/LayoutSVGContainer.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGContainerPainter.h"

namespace blink {

LayoutSVGContainer::LayoutSVGContainer(SVGElement* node)
    : LayoutSVGModelObject(node),
      m_objectBoundingBoxValid(false),
      m_needsBoundariesUpdate(true),
      m_didScreenScaleFactorChange(false),
      m_hasNonIsolatedBlendingDescendants(false),
      m_hasNonIsolatedBlendingDescendantsDirty(false) {}

LayoutSVGContainer::~LayoutSVGContainer() {}

void LayoutSVGContainer::layout() {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  // Update the local transform in subclasses.
  SVGTransformChange transformChange = calculateLocalTransform();
  m_didScreenScaleFactorChange =
      transformChange == SVGTransformChange::Full ||
      SVGLayoutSupport::screenScaleFactorChanged(parent());

  // When hasRelativeLengths() is false, no descendants have relative lengths
  // (hence no one is interested in viewport size changes).
  bool layoutSizeChanged =
      element()->hasRelativeLengths() &&
      SVGLayoutSupport::layoutSizeOfNearestViewportChanged(this);

  SVGLayoutSupport::layoutChildren(
      firstChild(), false, m_didScreenScaleFactorChange, layoutSizeChanged);

  // Invalidate all resources of this client if our layout changed.
  if (everHadLayout() && needsLayout())
    SVGResourcesCache::clientLayoutChanged(this);

  if (m_needsBoundariesUpdate || transformChange != SVGTransformChange::None) {
    updateCachedBoundaries();
    m_needsBoundariesUpdate = false;

    // If our bounds changed, notify the parents.
    LayoutSVGModelObject::setNeedsBoundariesUpdate();
  }

  ASSERT(!m_needsBoundariesUpdate);
  clearNeedsLayout();
}

void LayoutSVGContainer::addChild(LayoutObject* child,
                                  LayoutObject* beforeChild) {
  LayoutSVGModelObject::addChild(child, beforeChild);
  SVGResourcesCache::clientWasAddedToTree(child, child->styleRef());

  bool shouldIsolateDescendants =
      (child->isBlendingAllowed() && child->style()->hasBlendMode()) ||
      child->hasNonIsolatedBlendingDescendants();
  if (shouldIsolateDescendants)
    descendantIsolationRequirementsChanged(DescendantIsolationRequired);
}

void LayoutSVGContainer::removeChild(LayoutObject* child) {
  SVGResourcesCache::clientWillBeRemovedFromTree(child);
  LayoutSVGModelObject::removeChild(child);

  bool hadNonIsolatedDescendants =
      (child->isBlendingAllowed() && child->style()->hasBlendMode()) ||
      child->hasNonIsolatedBlendingDescendants();
  if (hadNonIsolatedDescendants)
    descendantIsolationRequirementsChanged(DescendantIsolationNeedsUpdate);
}

bool LayoutSVGContainer::selfWillPaint() const {
  return SVGLayoutSupport::hasFilterResource(*this);
}

void LayoutSVGContainer::styleDidChange(StyleDifference diff,
                                        const ComputedStyle* oldStyle) {
  LayoutSVGModelObject::styleDidChange(diff, oldStyle);

  bool hadIsolation =
      oldStyle && !isSVGHiddenContainer() &&
      SVGLayoutSupport::willIsolateBlendingDescendantsForStyle(*oldStyle);

  bool willIsolateBlendingDescendants =
      SVGLayoutSupport::willIsolateBlendingDescendantsForObject(this);

  bool isolationChanged = hadIsolation != willIsolateBlendingDescendants;

  if (isolationChanged)
    setNeedsPaintPropertyUpdate();

  if (!parent() || !isolationChanged)
    return;

  if (hasNonIsolatedBlendingDescendants()) {
    parent()->descendantIsolationRequirementsChanged(
        willIsolateBlendingDescendants ? DescendantIsolationNeedsUpdate
                                       : DescendantIsolationRequired);
  }
}

bool LayoutSVGContainer::hasNonIsolatedBlendingDescendants() const {
  if (m_hasNonIsolatedBlendingDescendantsDirty) {
    m_hasNonIsolatedBlendingDescendants =
        SVGLayoutSupport::computeHasNonIsolatedBlendingDescendants(this);
    m_hasNonIsolatedBlendingDescendantsDirty = false;
  }
  return m_hasNonIsolatedBlendingDescendants;
}

void LayoutSVGContainer::descendantIsolationRequirementsChanged(
    DescendantIsolationState state) {
  switch (state) {
    case DescendantIsolationRequired:
      m_hasNonIsolatedBlendingDescendants = true;
      m_hasNonIsolatedBlendingDescendantsDirty = false;
      break;
    case DescendantIsolationNeedsUpdate:
      if (m_hasNonIsolatedBlendingDescendantsDirty)
        return;
      m_hasNonIsolatedBlendingDescendantsDirty = true;
      break;
  }
  if (SVGLayoutSupport::willIsolateBlendingDescendantsForObject(this)) {
    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
      setNeedsPaintPropertyUpdate();
    return;
  }
  if (parent())
    parent()->descendantIsolationRequirementsChanged(state);
}

void LayoutSVGContainer::paint(const PaintInfo& paintInfo,
                               const LayoutPoint&) const {
  SVGContainerPainter(*this).paint(paintInfo);
}

void LayoutSVGContainer::addOutlineRects(
    Vector<LayoutRect>& rects,
    const LayoutPoint&,
    IncludeBlockVisualOverflowOrNot) const {
  rects.push_back(LayoutRect(visualRectInLocalSVGCoordinates()));
}

void LayoutSVGContainer::updateCachedBoundaries() {
  SVGLayoutSupport::computeContainerBoundingBoxes(
      this, m_objectBoundingBox, m_objectBoundingBoxValid, m_strokeBoundingBox,
      m_localVisualRect);
  if (element())
    element()->setNeedsResizeObserverUpdate();
}

bool LayoutSVGContainer::nodeAtFloatPoint(HitTestResult& result,
                                          const FloatPoint& pointInParent,
                                          HitTestAction hitTestAction) {
  FloatPoint localPoint;
  if (!SVGLayoutSupport::transformToUserSpaceAndCheckClipping(
          *this, localToSVGParentTransform(), pointInParent, localPoint))
    return false;

  for (LayoutObject* child = lastChild(); child;
       child = child->previousSibling()) {
    if (child->nodeAtFloatPoint(result, localPoint, hitTestAction)) {
      const LayoutPoint& localLayoutPoint = LayoutPoint(localPoint);
      updateHitTestResult(result, localLayoutPoint);
      if (result.addNodeToListBasedTestResult(
              child->node(), localLayoutPoint) == StopHitTesting)
        return true;
    }
  }

  // pointer-events: bounding-box makes it possible for containers to be direct
  // targets.
  if (style()->pointerEvents() == EPointerEvents::kBoundingBox) {
    // Check for a valid bounding box because it will be invalid for empty
    // containers.
    if (isObjectBoundingBoxValid() &&
        objectBoundingBox().contains(localPoint)) {
      const LayoutPoint& localLayoutPoint = LayoutPoint(localPoint);
      updateHitTestResult(result, localLayoutPoint);
      if (result.addNodeToListBasedTestResult(element(), localLayoutPoint) ==
          StopHitTesting)
        return true;
    }
  }
  // 16.4: "If there are no graphics elements whose relevant graphics content is
  // under the pointer (i.e., there is no target element), the event is not
  // dispatched."
  return false;
}

SVGTransformChange LayoutSVGContainer::calculateLocalTransform() {
  return SVGTransformChange::None;
}

}  // namespace blink
