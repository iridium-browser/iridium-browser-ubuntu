/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGDocumentExtensions.h"

#include "core/dom/Document.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/animation/SMILTimeContainer.h"
#include "wtf/AutoReset.h"
#include "wtf/text/AtomicString.h"

namespace blink {

SVGDocumentExtensions::SVGDocumentExtensions(Document* document)
    : m_document(document)
{
}

SVGDocumentExtensions::~SVGDocumentExtensions() {}

void SVGDocumentExtensions::addTimeContainer(SVGSVGElement* element) {
  m_timeContainers.add(element);
}

void SVGDocumentExtensions::removeTimeContainer(SVGSVGElement* element) {
  m_timeContainers.remove(element);
}

void SVGDocumentExtensions::addWebAnimationsPendingSVGElement(
    SVGElement& element) {
  ASSERT(RuntimeEnabledFeatures::webAnimationsSVGEnabled());
  m_webAnimationsPendingSVGElements.add(&element);
}

void SVGDocumentExtensions::addResource(const AtomicString& id,
                                        LayoutSVGResourceContainer* resource) {
  ASSERT(resource);

  if (id.isEmpty())
    return;

  // Replaces resource if already present, to handle potential id changes
  m_resources.set(id, resource);
}

void SVGDocumentExtensions::removeResource(const AtomicString& id) {
  if (id.isEmpty())
    return;

  m_resources.remove(id);
}

LayoutSVGResourceContainer* SVGDocumentExtensions::resourceById(
    const AtomicString& id) const {
  if (id.isEmpty())
    return nullptr;

  return m_resources.get(id);
}

void SVGDocumentExtensions::serviceOnAnimationFrame(Document& document) {
  if (!document.svgExtensions())
    return;
  document.accessSVGExtensions().serviceAnimations();
}

void SVGDocumentExtensions::serviceAnimations() {
  if (RuntimeEnabledFeatures::smilEnabled()) {
    HeapVector<Member<SVGSVGElement>> timeContainers;
    copyToVector(m_timeContainers, timeContainers);
    for (const auto& container : timeContainers)
      container->timeContainer()->serviceAnimations();
  }

  SVGElementSet webAnimationsPendingSVGElements;
  webAnimationsPendingSVGElements.swap(m_webAnimationsPendingSVGElements);

  // TODO(alancutter): Make SVG animation effect application a separate document
  // lifecycle phase from servicing animations to be responsive to Javascript
  // manipulation of exposed animation objects.
  for (auto& svgElement : webAnimationsPendingSVGElements)
    svgElement->applyActiveWebAnimations();

  ASSERT(m_webAnimationsPendingSVGElements.isEmpty());
}

void SVGDocumentExtensions::startAnimations() {
  // FIXME: Eventually every "Time Container" will need a way to latch on to
  // some global timer starting animations for a document will do this
  // "latching"
  // FIXME: We hold a ref pointers to prevent a shadow tree from getting removed
  // out from underneath us.  In the future we should refactor the use-element
  // to avoid this. See https://webkit.org/b/53704
  HeapVector<Member<SVGSVGElement>> timeContainers;
  copyToVector(m_timeContainers, timeContainers);
  for (const auto& container : timeContainers) {
    SMILTimeContainer* timeContainer = container->timeContainer();
    if (!timeContainer->isStarted())
      timeContainer->start();
  }
}

void SVGDocumentExtensions::pauseAnimations() {
  for (SVGSVGElement* element : m_timeContainers)
    element->pauseAnimations();
}

void SVGDocumentExtensions::dispatchSVGLoadEventToOutermostSVGElements() {
  HeapVector<Member<SVGSVGElement>> timeContainers;
  copyToVector(m_timeContainers, timeContainers);
  for (const auto& container : timeContainers) {
    SVGSVGElement* outerSVG = container.get();
    if (!outerSVG->isOutermostSVGSVGElement())
      continue;

    // Don't dispatch the load event document is not wellformed (for
    // XML/standalone svg).
    if (outerSVG->document().wellFormed() ||
        !outerSVG->document().isSVGDocument())
      outerSVG->sendSVGLoadEventIfPossible();
  }
}

void SVGDocumentExtensions::reportError(const String& message) {
  ConsoleMessage* consoleMessage = ConsoleMessage::create(
      RenderingMessageSource, ErrorMessageLevel, "Error: " + message);
  m_document->addConsoleMessage(consoleMessage);
}

void SVGDocumentExtensions::addPendingResource(const AtomicString& id,
                                               Element* element) {
  ASSERT(element);
  ASSERT(element->isConnected());

  if (id.isEmpty())
    return;

  HeapHashMap<AtomicString, Member<SVGPendingElements>>::AddResult result =
      m_pendingResources.add(id, nullptr);
  if (result.isNewEntry)
    result.storedValue->value = new SVGPendingElements;
  result.storedValue->value->add(element);

  element->setHasPendingResources();
}

bool SVGDocumentExtensions::hasPendingResource(const AtomicString& id) const {
  if (id.isEmpty())
    return false;

  return m_pendingResources.contains(id);
}

bool SVGDocumentExtensions::isElementPendingResources(Element* element) const {
  // This algorithm takes time proportional to the number of pending resources
  // and need not.  If performance becomes an issue we can keep a counted set of
  // elements and answer the question efficiently.

  ASSERT(element);

  for (const auto& entry : m_pendingResources) {
    SVGPendingElements* elements = entry.value.get();
    ASSERT(elements);

    if (elements->contains(element))
      return true;
  }
  return false;
}

bool SVGDocumentExtensions::isElementPendingResource(
    Element* element,
    const AtomicString& id) const {
  ASSERT(element);

  if (!hasPendingResource(id))
    return false;

  return m_pendingResources.get(id)->contains(element);
}

void SVGDocumentExtensions::clearHasPendingResourcesIfPossible(
    Element* element) {
  if (!isElementPendingResources(element))
    element->clearHasPendingResources();
}

void SVGDocumentExtensions::removeElementFromPendingResources(
    Element* element) {
  DCHECK(element);

  // Remove the element from pending resources.
  if (m_pendingResources.isEmpty() || !element->hasPendingResources())
    return;

  Vector<AtomicString> toBeRemoved;
  for (const auto& entry : m_pendingResources) {
    SVGPendingElements* elements = entry.value.get();
    DCHECK(elements);
    DCHECK(!elements->isEmpty());

    elements->remove(element);
    if (elements->isEmpty())
      toBeRemoved.push_back(entry.key);
  }

  clearHasPendingResourcesIfPossible(element);

  m_pendingResources.removeAll(toBeRemoved);
}

SVGDocumentExtensions::SVGPendingElements*
SVGDocumentExtensions::removePendingResource(const AtomicString& id) {
  ASSERT(m_pendingResources.contains(id));
  return m_pendingResources.take(id);
}

void SVGDocumentExtensions::addSVGRootWithRelativeLengthDescendents(
    SVGSVGElement* svgRoot) {
  ASSERT(!m_inRelativeLengthSVGRootsInvalidation);
  m_relativeLengthSVGRoots.add(svgRoot);
}

void SVGDocumentExtensions::removeSVGRootWithRelativeLengthDescendents(
    SVGSVGElement* svgRoot) {
  ASSERT(!m_inRelativeLengthSVGRootsInvalidation);
  m_relativeLengthSVGRoots.remove(svgRoot);
}

bool SVGDocumentExtensions::isSVGRootWithRelativeLengthDescendents(
    SVGSVGElement* svgRoot) const {
  return m_relativeLengthSVGRoots.contains(svgRoot);
}

void SVGDocumentExtensions::invalidateSVGRootsWithRelativeLengthDescendents(
    SubtreeLayoutScope* scope) {
  ASSERT(!m_inRelativeLengthSVGRootsInvalidation);
#if DCHECK_IS_ON()
  AutoReset<bool> inRelativeLengthSVGRootsChange(
      &m_inRelativeLengthSVGRootsInvalidation, true);
#endif

  for (SVGSVGElement* element : m_relativeLengthSVGRoots)
    element->invalidateRelativeLengthClients(scope);
}

bool SVGDocumentExtensions::zoomAndPanEnabled() const {
  if (SVGSVGElement* svg = rootElement(*m_document))
    return svg->zoomAndPanEnabled();
  return false;
}

void SVGDocumentExtensions::startPan(const FloatPoint& start) {
  if (SVGSVGElement* svg = rootElement(*m_document))
    m_translate = FloatPoint(start.x() - svg->currentTranslate().x(),
                             start.y() - svg->currentTranslate().y());
}

void SVGDocumentExtensions::updatePan(const FloatPoint& pos) const {
  if (SVGSVGElement* svg = rootElement(*m_document))
    svg->setCurrentTranslate(
        FloatPoint(pos.x() - m_translate.x(), pos.y() - m_translate.y()));
}

SVGSVGElement* SVGDocumentExtensions::rootElement(const Document& document) {
  Element* elem = document.documentElement();
  return isSVGSVGElement(elem) ? toSVGSVGElement(elem) : 0;
}

SVGSVGElement* SVGDocumentExtensions::rootElement() const {
  ASSERT(m_document);
  return rootElement(*m_document);
}

DEFINE_TRACE(SVGDocumentExtensions) {
  visitor->trace(m_document);
  visitor->trace(m_timeContainers);
  visitor->trace(m_webAnimationsPendingSVGElements);
  visitor->trace(m_relativeLengthSVGRoots);
  visitor->trace(m_pendingResources);
}

}  // namespace blink
