/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/layout/svg/LayoutSVGResourceContainer.h"

#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/svg/SVGElementProxy.h"
#include "wtf/AutoReset.h"

namespace blink {

static inline SVGDocumentExtensions& svgExtensionsFromElement(
    Element* element) {
  ASSERT(element);
  return element->document().accessSVGExtensions();
}

LayoutSVGResourceContainer::LayoutSVGResourceContainer(SVGElement* node)
    : LayoutSVGHiddenContainer(node),
      m_isInLayout(false),
      m_id(node->getIdAttribute()),
      m_invalidationMask(0),
      m_registered(false),
      m_isInvalidating(false) {}

LayoutSVGResourceContainer::~LayoutSVGResourceContainer() {}

void LayoutSVGResourceContainer::layout() {
  // FIXME: Investigate a way to detect and break resource layout dependency
  // cycles early. Then we can remove this method altogether, and fall back onto
  // LayoutSVGHiddenContainer::layout().
  ASSERT(needsLayout());
  if (m_isInLayout)
    return;

  AutoReset<bool> inLayoutChange(&m_isInLayout, true);

  LayoutSVGHiddenContainer::layout();

  clearInvalidationMask();
}

SVGElementProxySet* LayoutSVGResourceContainer::elementProxySet() {
  return element()->elementProxySet();
}

void LayoutSVGResourceContainer::notifyContentChanged() {
  if (SVGElementProxySet* proxySet = elementProxySet())
    proxySet->notifyContentChanged(element()->treeScope());
}

void LayoutSVGResourceContainer::willBeDestroyed() {
  // Detach all clients referring to this resource. If the resource itself is
  // a client, it will be detached from any such resources by the call to
  // LayoutSVGHiddenContainer::willBeDestroyed() below.
  detachAllClients();

  LayoutSVGHiddenContainer::willBeDestroyed();
  if (m_registered)
    svgExtensionsFromElement(element()).removeResource(m_id);
}

void LayoutSVGResourceContainer::styleDidChange(StyleDifference diff,
                                                const ComputedStyle* oldStyle) {
  LayoutSVGHiddenContainer::styleDidChange(diff, oldStyle);

  if (!m_registered) {
    m_registered = true;
    registerResource();
  }
}

void LayoutSVGResourceContainer::detachAllClients() {
  for (auto* client : m_clients) {
    // Unlink the resource from the client's SVGResources. (The actual
    // removal will be signaled after processing all the clients.)
    SVGResources* resources =
        SVGResourcesCache::cachedResourcesForLayoutObject(client);
    // Or else the client wouldn't be in the list in the first place.
    DCHECK(resources);
    resources->resourceDestroyed(this);

    // Add a pending resolution based on the id of the old resource.
    Element* clientElement = toElement(client->node());
    svgExtensionsFromElement(clientElement)
        .addPendingResource(m_id, clientElement);
  }

  removeAllClientsFromCache();
}

void LayoutSVGResourceContainer::idChanged() {
  // Invalidate all our current clients.
  removeAllClientsFromCache();

  // Remove old id, that is guaranteed to be present in cache.
  SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
  extensions.removeResource(m_id);
  m_id = element()->getIdAttribute();

  registerResource();
}

void LayoutSVGResourceContainer::markAllClientsForInvalidation(
    InvalidationMode mode) {
  if (m_isInvalidating)
    return;
  SVGElementProxySet* proxySet = elementProxySet();
  if (m_clients.isEmpty() && (!proxySet || proxySet->isEmpty()))
    return;
  if (m_invalidationMask & mode)
    return;

  m_invalidationMask |= mode;
  m_isInvalidating = true;
  bool needsLayout = mode == LayoutAndBoundariesInvalidation;
  bool markForInvalidation = mode != ParentOnlyInvalidation;

  // Invalidate clients registered on the this object (via SVGResources).
  for (auto* client : m_clients) {
    DCHECK(client->isSVG());
    if (client->isSVGResourceContainer()) {
      toLayoutSVGResourceContainer(client)->removeAllClientsFromCache(
          markForInvalidation);
      continue;
    }

    if (markForInvalidation)
      markClientForInvalidation(client, mode);

    LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(
        client, needsLayout);
  }

  // Invalidate clients registered via an SVGElementProxy.
  notifyContentChanged();

  m_isInvalidating = false;
}

void LayoutSVGResourceContainer::markClientForInvalidation(
    LayoutObject* client,
    InvalidationMode mode) {
  ASSERT(client);
  ASSERT(!m_clients.isEmpty());

  switch (mode) {
    case LayoutAndBoundariesInvalidation:
    case BoundariesInvalidation:
      client->setNeedsBoundariesUpdate();
      break;
    case PaintInvalidation:
      // Since LayoutSVGInlineTexts don't have SVGResources (they use their
      // parent's), they will not be notified of changes to paint servers. So
      // if the client is one that could have a LayoutSVGInlineText use a
      // paint invalidation reason that will force paint invalidation of the
      // entire <text>/<tspan>/... subtree.
      client->setShouldDoFullPaintInvalidation(
          PaintInvalidationSVGResourceChange);
      // Invalidate paint properties to update effects if any.
      client->setNeedsPaintPropertyUpdate();
      break;
    case ParentOnlyInvalidation:
      break;
  }
}

void LayoutSVGResourceContainer::addClient(LayoutObject* client) {
  ASSERT(client);
  m_clients.add(client);
  clearInvalidationMask();
}

void LayoutSVGResourceContainer::removeClient(LayoutObject* client) {
  ASSERT(client);
  removeClientFromCache(client, false);
  m_clients.remove(client);
}

void LayoutSVGResourceContainer::invalidateCacheAndMarkForLayout(
    SubtreeLayoutScope* layoutScope) {
  if (selfNeedsLayout())
    return;

  setNeedsLayoutAndFullPaintInvalidation(
      LayoutInvalidationReason::SvgResourceInvalidated, MarkContainerChain,
      layoutScope);

  if (everHadLayout())
    removeAllClientsFromCache();
}

void LayoutSVGResourceContainer::registerResource() {
  SVGDocumentExtensions& extensions = svgExtensionsFromElement(element());
  if (!extensions.hasPendingResource(m_id)) {
    extensions.addResource(m_id, this);
    return;
  }

  SVGDocumentExtensions::SVGPendingElements* clients(
      extensions.removePendingResource(m_id));

  // Cache us with the new id.
  extensions.addResource(m_id, this);

  // Update cached resources of pending clients.
  for (const auto& pendingClient : *clients) {
    DCHECK(pendingClient->hasPendingResources());
    extensions.clearHasPendingResourcesIfPossible(pendingClient);
    LayoutObject* layoutObject = pendingClient->layoutObject();
    if (!layoutObject)
      continue;
    DCHECK(layoutObject->isSVG() && (resourceType() != FilterResourceType ||
                                     !layoutObject->isSVGRoot()));

    StyleDifference diff;
    diff.setNeedsFullLayout();
    SVGResourcesCache::clientStyleChanged(layoutObject, diff,
                                          layoutObject->styleRef());
    layoutObject->setNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::SvgResourceInvalidated);
  }
}

static inline void removeFromCacheAndInvalidateDependencies(
    LayoutObject* object,
    bool needsLayout) {
  ASSERT(object);
  if (SVGResources* resources =
          SVGResourcesCache::cachedResourcesForLayoutObject(object)) {
    resources->removeClientFromCacheAffectingObjectBounds(object);
  }

  if (!object->node() || !object->node()->isSVGElement())
    return;

  SVGElementSet* dependencies =
      toSVGElement(object->node())->setOfIncomingReferences();
  if (!dependencies)
    return;

  // We allow cycles in SVGDocumentExtensions reference sets in order to avoid
  // expensive reference graph adjustments on changes, so we need to break
  // possible cycles here.
  // This strong reference is safe, as it is guaranteed that this set will be
  // emptied at the end of recursion.
  DEFINE_STATIC_LOCAL(SVGElementSet, invalidatingDependencies,
                      (new SVGElementSet));

  for (SVGElement* element : *dependencies) {
    if (LayoutObject* layoutObject = element->layoutObject()) {
      if (UNLIKELY(!invalidatingDependencies.add(element).isNewEntry)) {
        // Reference cycle: we are in process of invalidating this dependant.
        continue;
      }

      LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(
          layoutObject, needsLayout);
      invalidatingDependencies.remove(element);
    }
  }
}

void LayoutSVGResourceContainer::markForLayoutAndParentResourceInvalidation(
    LayoutObject* object,
    bool needsLayout) {
  ASSERT(object);
  ASSERT(object->node());

  if (needsLayout && !object->documentBeingDestroyed())
    object->setNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::SvgResourceInvalidated);

  removeFromCacheAndInvalidateDependencies(object, needsLayout);

  // Invalidate resources in ancestor chain, if needed.
  LayoutObject* current = object->parent();
  while (current) {
    removeFromCacheAndInvalidateDependencies(current, needsLayout);

    if (current->isSVGResourceContainer()) {
      // This will process the rest of the ancestors.
      toLayoutSVGResourceContainer(current)->removeAllClientsFromCache();
      break;
    }

    current = current->parent();
  }
}

}  // namespace blink
