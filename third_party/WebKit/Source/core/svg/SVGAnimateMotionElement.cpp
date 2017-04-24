/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "core/svg/SVGAnimateMotionElement.h"

#include "core/SVGNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/layout/LayoutObject.h"
#include "core/svg/SVGMPathElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGPathUtilities.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"

namespace blink {

namespace {

bool targetCanHaveMotionTransform(const SVGElement& target) {
  // We don't have a special attribute name to verify the animation type. Check
  // the element name instead.
  if (!target.isSVGGraphicsElement())
    return false;
  // Spec: SVG 1.1 section 19.2.15
  // FIXME: svgTag is missing. Needs to be checked, if transforming <svg> could
  // cause problems.
  return isSVGGElement(target) || isSVGDefsElement(target) ||
         isSVGUseElement(target) || isSVGImageElement(target) ||
         isSVGSwitchElement(target) || isSVGPathElement(target) ||
         isSVGRectElement(target) || isSVGCircleElement(target) ||
         isSVGEllipseElement(target) || isSVGLineElement(target) ||
         isSVGPolylineElement(target) || isSVGPolygonElement(target) ||
         isSVGTextElement(target) || isSVGClipPathElement(target) ||
         isSVGMaskElement(target) || isSVGAElement(target) ||
         isSVGForeignObjectElement(target);
}
}

inline SVGAnimateMotionElement::SVGAnimateMotionElement(Document& document)
    : SVGAnimationElement(SVGNames::animateMotionTag, document),
      m_hasToPointAtEndOfDuration(false) {
  setCalcMode(CalcModePaced);
}

DEFINE_NODE_FACTORY(SVGAnimateMotionElement)

SVGAnimateMotionElement::~SVGAnimateMotionElement() {}

bool SVGAnimateMotionElement::hasValidTarget() {
  return SVGAnimationElement::hasValidTarget() &&
         targetCanHaveMotionTransform(*targetElement());
}

void SVGAnimateMotionElement::parseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == SVGNames::pathAttr) {
    m_path = Path();
    buildPathFromString(params.newValue, m_path);
    updateAnimationPath();
    return;
  }

  SVGAnimationElement::parseAttribute(params);
}

SVGAnimateMotionElement::RotateMode SVGAnimateMotionElement::getRotateMode()
    const {
  DEFINE_STATIC_LOCAL(const AtomicString, autoVal, ("auto"));
  DEFINE_STATIC_LOCAL(const AtomicString, autoReverse, ("auto-reverse"));
  const AtomicString& rotate = getAttribute(SVGNames::rotateAttr);
  if (rotate == autoVal)
    return RotateAuto;
  if (rotate == autoReverse)
    return RotateAutoReverse;
  return RotateAngle;
}

void SVGAnimateMotionElement::updateAnimationPath() {
  m_animationPath = Path();
  bool foundMPath = false;

  for (SVGMPathElement* mpath = Traversal<SVGMPathElement>::firstChild(*this);
       mpath; mpath = Traversal<SVGMPathElement>::nextSibling(*mpath)) {
    if (SVGPathElement* pathElement = mpath->pathElement()) {
      m_animationPath = pathElement->attributePath();
      foundMPath = true;
      break;
    }
  }

  if (!foundMPath && fastHasAttribute(SVGNames::pathAttr))
    m_animationPath = m_path;

  updateAnimationMode();
}

template <typename CharType>
static bool parsePointInternal(const String& string, FloatPoint& point) {
  const CharType* ptr = string.getCharacters<CharType>();
  const CharType* end = ptr + string.length();

  if (!skipOptionalSVGSpaces(ptr, end))
    return false;

  float x = 0;
  if (!parseNumber(ptr, end, x))
    return false;

  float y = 0;
  if (!parseNumber(ptr, end, y))
    return false;

  point = FloatPoint(x, y);

  // disallow anything except spaces at the end
  return !skipOptionalSVGSpaces(ptr, end);
}

static bool parsePoint(const String& string, FloatPoint& point) {
  if (string.isEmpty())
    return false;
  if (string.is8Bit())
    return parsePointInternal<LChar>(string, point);
  return parsePointInternal<UChar>(string, point);
}

void SVGAnimateMotionElement::resetAnimatedType() {
  SVGElement* targetElement = this->targetElement();
  if (!targetElement || !targetCanHaveMotionTransform(*targetElement))
    return;
  if (AffineTransform* transform = targetElement->animateMotionTransform())
    transform->makeIdentity();
}

void SVGAnimateMotionElement::clearAnimatedType() {
  SVGElement* targetElement = this->targetElement();
  if (!targetElement)
    return;

  AffineTransform* transform = targetElement->animateMotionTransform();
  if (!transform)
    return;

  transform->makeIdentity();

  if (LayoutObject* targetLayoutObject = targetElement->layoutObject())
    invalidateForAnimateMotionTransformChange(*targetLayoutObject);
}

bool SVGAnimateMotionElement::calculateToAtEndOfDurationValue(
    const String& toAtEndOfDurationString) {
  parsePoint(toAtEndOfDurationString, m_toPointAtEndOfDuration);
  m_hasToPointAtEndOfDuration = true;
  return true;
}

bool SVGAnimateMotionElement::calculateFromAndToValues(const String& fromString,
                                                       const String& toString) {
  m_hasToPointAtEndOfDuration = false;
  parsePoint(fromString, m_fromPoint);
  parsePoint(toString, m_toPoint);
  return true;
}

bool SVGAnimateMotionElement::calculateFromAndByValues(const String& fromString,
                                                       const String& byString) {
  m_hasToPointAtEndOfDuration = false;
  if (getAnimationMode() == ByAnimation && !isAdditive())
    return false;
  parsePoint(fromString, m_fromPoint);
  FloatPoint byPoint;
  parsePoint(byString, byPoint);
  m_toPoint =
      FloatPoint(m_fromPoint.x() + byPoint.x(), m_fromPoint.y() + byPoint.y());
  return true;
}

void SVGAnimateMotionElement::calculateAnimatedValue(float percentage,
                                                     unsigned repeatCount,
                                                     SVGSMILElement*) {
  SVGElement* targetElement = this->targetElement();
  DCHECK(targetElement);
  AffineTransform* transform = targetElement->animateMotionTransform();
  if (!transform)
    return;

  if (LayoutObject* targetLayoutObject = targetElement->layoutObject())
    invalidateForAnimateMotionTransformChange(*targetLayoutObject);

  if (!isAdditive())
    transform->makeIdentity();

  if (getAnimationMode() != PathAnimation) {
    FloatPoint toPointAtEndOfDuration = m_toPoint;
    if (isAccumulated() && repeatCount && m_hasToPointAtEndOfDuration)
      toPointAtEndOfDuration = m_toPointAtEndOfDuration;

    float animatedX = 0;
    animateAdditiveNumber(percentage, repeatCount, m_fromPoint.x(),
                          m_toPoint.x(), toPointAtEndOfDuration.x(), animatedX);

    float animatedY = 0;
    animateAdditiveNumber(percentage, repeatCount, m_fromPoint.y(),
                          m_toPoint.y(), toPointAtEndOfDuration.y(), animatedY);

    transform->translate(animatedX, animatedY);
    return;
  }

  ASSERT(!m_animationPath.isEmpty());

  float positionOnPath = m_animationPath.length() * percentage;
  FloatPoint position;
  float angle;
  m_animationPath.pointAndNormalAtLength(positionOnPath, position, angle);

  // Handle accumulate="sum".
  if (isAccumulated() && repeatCount) {
    FloatPoint positionAtEndOfDuration =
        m_animationPath.pointAtLength(m_animationPath.length());
    position.move(positionAtEndOfDuration.x() * repeatCount,
                  positionAtEndOfDuration.y() * repeatCount);
  }

  transform->translate(position.x(), position.y());
  RotateMode rotateMode = this->getRotateMode();
  if (rotateMode != RotateAuto && rotateMode != RotateAutoReverse)
    return;
  if (rotateMode == RotateAutoReverse)
    angle += 180;
  transform->rotate(angle);
}

void SVGAnimateMotionElement::applyResultsToTarget() {
  // We accumulate to the target element transform list so there is not much to
  // do here.
  SVGElement* targetElement = this->targetElement();
  if (!targetElement)
    return;

  AffineTransform* t = targetElement->animateMotionTransform();
  if (!t)
    return;

  // ...except in case where we have additional instances in <use> trees.
  const HeapHashSet<WeakMember<SVGElement>>& instances =
      targetElement->instancesForElement();
  for (SVGElement* shadowTreeElement : instances) {
    ASSERT(shadowTreeElement);
    AffineTransform* transform = shadowTreeElement->animateMotionTransform();
    if (!transform)
      continue;
    transform->setMatrix(t->a(), t->b(), t->c(), t->d(), t->e(), t->f());
    if (LayoutObject* layoutObject = shadowTreeElement->layoutObject())
      invalidateForAnimateMotionTransformChange(*layoutObject);
  }
}

float SVGAnimateMotionElement::calculateDistance(const String& fromString,
                                                 const String& toString) {
  FloatPoint from;
  FloatPoint to;
  if (!parsePoint(fromString, from))
    return -1;
  if (!parsePoint(toString, to))
    return -1;
  FloatSize diff = to - from;
  return sqrtf(diff.width() * diff.width() + diff.height() * diff.height());
}

void SVGAnimateMotionElement::updateAnimationMode() {
  if (!m_animationPath.isEmpty())
    setAnimationMode(PathAnimation);
  else
    SVGAnimationElement::updateAnimationMode();
}

void SVGAnimateMotionElement::invalidateForAnimateMotionTransformChange(
    LayoutObject& object) {
  object.setNeedsTransformUpdate();
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    // The transform paint property relies on the SVG transform value.
    object.setNeedsPaintPropertyUpdate();
  }
  markForLayoutAndParentResourceInvalidation(&object);
}

}  // namespace blink
