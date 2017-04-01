/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGPatternElement.h"

#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/layout/svg/LayoutSVGResourcePattern.h"
#include "core/svg/PatternAttributes.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

inline SVGPatternElement::SVGPatternElement(Document& document)
    : SVGElement(SVGNames::patternTag, document),
      SVGURIReference(this),
      SVGTests(this),
      SVGFitToViewBox(this),
      m_x(SVGAnimatedLength::create(this,
                                    SVGNames::xAttr,
                                    SVGLength::create(SVGLengthMode::Width))),
      m_y(SVGAnimatedLength::create(this,
                                    SVGNames::yAttr,
                                    SVGLength::create(SVGLengthMode::Height))),
      m_width(
          SVGAnimatedLength::create(this,
                                    SVGNames::widthAttr,
                                    SVGLength::create(SVGLengthMode::Width))),
      m_height(
          SVGAnimatedLength::create(this,
                                    SVGNames::heightAttr,
                                    SVGLength::create(SVGLengthMode::Height))),
      m_patternTransform(
          SVGAnimatedTransformList::create(this,
                                           SVGNames::patternTransformAttr,
                                           CSSPropertyTransform)),
      m_patternUnits(SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::create(
          this,
          SVGNames::patternUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)),
      m_patternContentUnits(
          SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>::create(
              this,
              SVGNames::patternContentUnitsAttr,
              SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {
  addToPropertyMap(m_x);
  addToPropertyMap(m_y);
  addToPropertyMap(m_width);
  addToPropertyMap(m_height);
  addToPropertyMap(m_patternTransform);
  addToPropertyMap(m_patternUnits);
  addToPropertyMap(m_patternContentUnits);
}

DEFINE_TRACE(SVGPatternElement) {
  visitor->trace(m_x);
  visitor->trace(m_y);
  visitor->trace(m_width);
  visitor->trace(m_height);
  visitor->trace(m_patternTransform);
  visitor->trace(m_patternUnits);
  visitor->trace(m_patternContentUnits);
  SVGElement::trace(visitor);
  SVGURIReference::trace(visitor);
  SVGTests::trace(visitor);
  SVGFitToViewBox::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGPatternElement)

void SVGPatternElement::collectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == SVGNames::patternTransformAttr) {
    addPropertyToPresentationAttributeStyle(
        style, CSSPropertyTransform,
        m_patternTransform->currentValue()->cssValue());
    return;
  }
  SVGElement::collectStyleForPresentationAttribute(name, value, style);
}

void SVGPatternElement::svgAttributeChanged(const QualifiedName& attrName) {
  bool isLengthAttr =
      attrName == SVGNames::xAttr || attrName == SVGNames::yAttr ||
      attrName == SVGNames::widthAttr || attrName == SVGNames::heightAttr;

  if (attrName == SVGNames::patternTransformAttr) {
    invalidateSVGPresentationAttributeStyle();
    setNeedsStyleRecalc(LocalStyleChange,
                        StyleChangeReasonForTracing::fromAttribute(attrName));
  }

  if (isLengthAttr || attrName == SVGNames::patternUnitsAttr ||
      attrName == SVGNames::patternContentUnitsAttr ||
      attrName == SVGNames::patternTransformAttr ||
      SVGFitToViewBox::isKnownAttribute(attrName) ||
      SVGURIReference::isKnownAttribute(attrName) ||
      SVGTests::isKnownAttribute(attrName)) {
    SVGElement::InvalidationGuard invalidationGuard(this);

    if (isLengthAttr)
      updateRelativeLengthsInformation();

    LayoutSVGResourceContainer* layoutObject =
        toLayoutSVGResourceContainer(this->layoutObject());
    if (layoutObject)
      layoutObject->invalidateCacheAndMarkForLayout();

    return;
  }

  SVGElement::svgAttributeChanged(attrName);
}

void SVGPatternElement::childrenChanged(const ChildrenChange& change) {
  SVGElement::childrenChanged(change);

  if (change.byParser)
    return;

  if (LayoutObject* object = layoutObject())
    object->setNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::ChildChanged);
}

LayoutObject* SVGPatternElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutSVGResourcePattern(this);
}

static void setPatternAttributes(const SVGPatternElement* element,
                                 PatternAttributes& attributes) {
  if (!attributes.hasX() && element->x()->isSpecified())
    attributes.setX(element->x()->currentValue());

  if (!attributes.hasY() && element->y()->isSpecified())
    attributes.setY(element->y()->currentValue());

  if (!attributes.hasWidth() && element->width()->isSpecified())
    attributes.setWidth(element->width()->currentValue());

  if (!attributes.hasHeight() && element->height()->isSpecified())
    attributes.setHeight(element->height()->currentValue());

  if (!attributes.hasViewBox() && element->viewBox()->isSpecified() &&
      element->viewBox()->currentValue()->isValid())
    attributes.setViewBox(element->viewBox()->currentValue()->value());

  if (!attributes.hasPreserveAspectRatio() &&
      element->preserveAspectRatio()->isSpecified())
    attributes.setPreserveAspectRatio(
        element->preserveAspectRatio()->currentValue());

  if (!attributes.hasPatternUnits() && element->patternUnits()->isSpecified())
    attributes.setPatternUnits(
        element->patternUnits()->currentValue()->enumValue());

  if (!attributes.hasPatternContentUnits() &&
      element->patternContentUnits()->isSpecified())
    attributes.setPatternContentUnits(
        element->patternContentUnits()->currentValue()->enumValue());

  if (!attributes.hasPatternTransform() &&
      element->hasTransform(SVGElement::ExcludeMotionTransform)) {
    attributes.setPatternTransform(
        element->calculateTransform(SVGElement::ExcludeMotionTransform));
  }

  if (!attributes.hasPatternContentElement() &&
      ElementTraversal::firstWithin(*element))
    attributes.setPatternContentElement(element);
}

void SVGPatternElement::collectPatternAttributes(
    PatternAttributes& attributes) const {
  HeapHashSet<Member<const SVGPatternElement>> processedPatterns;
  const SVGPatternElement* current = this;

  while (true) {
    setPatternAttributes(current, attributes);
    processedPatterns.add(current);

    // Respect xlink:href, take attributes from referenced element
    Node* refNode = SVGURIReference::targetElementFromIRIString(
        current->hrefString(), treeScope());

    // Only consider attached SVG pattern elements.
    if (!isSVGPatternElement(refNode) || !refNode->layoutObject())
      break;

    current = toSVGPatternElement(refNode);

    // Cycle detection
    if (processedPatterns.contains(current))
      break;
  }
}

AffineTransform SVGPatternElement::localCoordinateSpaceTransform(
    SVGElement::CTMScope) const {
  return calculateTransform(SVGElement::ExcludeMotionTransform);
}

bool SVGPatternElement::selfHasRelativeLengths() const {
  return m_x->currentValue()->isRelative() ||
         m_y->currentValue()->isRelative() ||
         m_width->currentValue()->isRelative() ||
         m_height->currentValue()->isRelative();
}

}  // namespace blink
