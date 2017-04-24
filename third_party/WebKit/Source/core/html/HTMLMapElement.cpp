/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "core/html/HTMLMapElement.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLImageElement.h"
#include "core/layout/HitTestResult.h"

namespace blink {

using namespace HTMLNames;

inline HTMLMapElement::HTMLMapElement(Document& document)
    : HTMLElement(mapTag, document) {
  UseCounter::count(document, UseCounter::MapElement);
}

DEFINE_NODE_FACTORY(HTMLMapElement)

HTMLMapElement::~HTMLMapElement() {}

HTMLAreaElement* HTMLMapElement::areaForPoint(
    const LayoutPoint& location,
    const LayoutObject* containerObject) {
  HTMLAreaElement* defaultArea = nullptr;
  for (HTMLAreaElement& area :
       Traversal<HTMLAreaElement>::descendantsOf(*this)) {
    if (area.isDefault() && !defaultArea)
      defaultArea = &area;
    else if (area.pointInArea(location, containerObject))
      return &area;
  }

  return defaultArea;
}

HTMLImageElement* HTMLMapElement::imageElement() {
  HTMLCollection* images = document().images();
  for (unsigned i = 0; Element* curr = images->item(i); ++i) {
    DCHECK(isHTMLImageElement(curr));

    // The HTMLImageElement's useMap() value includes the '#' symbol at the
    // beginning, which has to be stripped off.
    HTMLImageElement& imageElement = toHTMLImageElement(*curr);
    String useMapName =
        imageElement.getAttribute(usemapAttr).getString().substring(1);
    if (useMapName == m_name)
      return &imageElement;
  }

  return nullptr;
}

void HTMLMapElement::parseAttribute(const AttributeModificationParams& params) {
  // FIXME: This logic seems wrong for XML documents.
  // Either the id or name will be used depending on the order the attributes
  // are parsed.

  if (params.name == idAttr || params.name == nameAttr) {
    if (params.name == idAttr) {
      // Call base class so that hasID bit gets set.
      HTMLElement::parseAttribute(params);
      if (document().isHTMLDocument())
        return;
    }
    if (isConnected())
      treeScope().removeImageMap(this);
    String mapName = params.newValue;
    if (mapName[0] == '#')
      mapName = mapName.substring(1);
    m_name = AtomicString(mapName);
    if (isConnected())
      treeScope().addImageMap(this);

    return;
  }

  HTMLElement::parseAttribute(params);
}

HTMLCollection* HTMLMapElement::areas() {
  return ensureCachedCollection<HTMLCollection>(MapAreas);
}

Node::InsertionNotificationRequest HTMLMapElement::insertedInto(
    ContainerNode* insertionPoint) {
  if (insertionPoint->isConnected())
    treeScope().addImageMap(this);
  return HTMLElement::insertedInto(insertionPoint);
}

void HTMLMapElement::removedFrom(ContainerNode* insertionPoint) {
  if (insertionPoint->isConnected())
    treeScope().removeImageMap(this);
  HTMLElement::removedFrom(insertionPoint);
}

}  // namespace blink
