/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/svg/SVGGeometryElement.h"

#include "core/SVGNames.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/svg/LayoutSVGPath.h"
#include "core/layout/svg/LayoutSVGShape.h"
#include "core/svg/SVGPointTearOff.h"

namespace blink {

class SVGAnimatedPathLength final : public SVGAnimatedNumber {
 public:
  static SVGAnimatedPathLength* create(SVGGeometryElement* contextElement) {
    return new SVGAnimatedPathLength(contextElement);
  }

  SVGParsingError setBaseValueAsString(const String& value) override {
    SVGParsingError parseStatus =
        SVGAnimatedNumber::setBaseValueAsString(value);
    if (parseStatus == SVGParseStatus::NoError && baseValue()->value() < 0)
      parseStatus = SVGParseStatus::NegativeValue;
    return parseStatus;
  }

 private:
  explicit SVGAnimatedPathLength(SVGGeometryElement* contextElement)
      : SVGAnimatedNumber(contextElement,
                          SVGNames::pathLengthAttr,
                          SVGNumber::create()) {}
};

SVGGeometryElement::SVGGeometryElement(const QualifiedName& tagName,
                                       Document& document,
                                       ConstructionType constructionType)
    : SVGGraphicsElement(tagName, document, constructionType),
      m_pathLength(SVGAnimatedPathLength::create(this)) {
  addToPropertyMap(m_pathLength);
}

DEFINE_TRACE(SVGGeometryElement) {
  visitor->trace(m_pathLength);
  SVGGraphicsElement::trace(visitor);
}

bool SVGGeometryElement::isPointInFill(SVGPointTearOff* point) const {
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // FIXME: Eventually we should support isPointInFill for display:none
  // elements.
  if (!layoutObject() || !layoutObject()->isSVGShape())
    return false;

  HitTestRequest request(HitTestRequest::ReadOnly);
  PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_GEOMETRY_HITTESTING,
                                 request,
                                 layoutObject()->style()->pointerEvents());
  hitRules.canHitStroke = false;
  return toLayoutSVGShape(layoutObject())
      ->nodeAtFloatPointInternal(request, point->target()->value(), hitRules);
}

bool SVGGeometryElement::isPointInStroke(SVGPointTearOff* point) const {
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  // FIXME: Eventually we should support isPointInStroke for display:none
  // elements.
  if (!layoutObject() || !layoutObject()->isSVGShape())
    return false;

  HitTestRequest request(HitTestRequest::ReadOnly);
  PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_GEOMETRY_HITTESTING,
                                 request,
                                 layoutObject()->style()->pointerEvents());
  hitRules.canHitFill = false;
  return toLayoutSVGShape(layoutObject())
      ->nodeAtFloatPointInternal(request, point->target()->value(), hitRules);
}

void SVGGeometryElement::toClipPath(Path& path) const {
  path = asPath();
  path.transform(calculateTransform(SVGElement::IncludeMotionTransform));

  ASSERT(layoutObject());
  ASSERT(layoutObject()->style());
  path.setWindRule(layoutObject()->style()->svgStyle().clipRule());
}

float SVGGeometryElement::getTotalLength() {
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  if (!layoutObject())
    return 0;
  return asPath().length();
}

SVGPointTearOff* SVGGeometryElement::getPointAtLength(float length) {
  document().updateStyleAndLayoutIgnorePendingStylesheets();

  FloatPoint point;
  if (layoutObject())
    point = asPath().pointAtLength(length);
  return SVGPointTearOff::create(SVGPoint::create(point), 0,
                                 PropertyIsNotAnimVal);
}

float SVGGeometryElement::computePathLength() const {
  return asPath().length();
}

float SVGGeometryElement::pathLengthScaleFactor() const {
  if (!pathLength()->isSpecified())
    return 1;
  float authorPathLength = pathLength()->currentValue()->value();
  if (authorPathLength < 0)
    return 1;
  if (!authorPathLength)
    return 0;
  DCHECK(layoutObject());
  float computedPathLength = computePathLength();
  if (!computedPathLength)
    return 1;
  return computedPathLength / authorPathLength;
}

LayoutObject* SVGGeometryElement::createLayoutObject(const ComputedStyle&) {
  // By default, any subclass is expected to do path-based drawing.
  return new LayoutSVGPath(this);
}

}  // namespace blink
