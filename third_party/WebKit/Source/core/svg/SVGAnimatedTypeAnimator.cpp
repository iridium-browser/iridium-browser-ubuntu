/*
 * Copyright (C) Research In Motion Limited 2011-2012. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#include "core/svg/SVGAnimatedTypeAnimator.h"

#include "core/svg/SVGAnimateTransformElement.h"
#include "core/svg/SVGAnimatedColor.h"
#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthList.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/SVGPointList.h"
#include "core/svg/SVGString.h"
#include "core/svg/SVGTransformList.h"

namespace blink {

SVGAnimatedTypeAnimator::SVGAnimatedTypeAnimator(SVGAnimationElement* animationElement)
    : m_animationElement(animationElement)
    , m_contextElement(nullptr)
    , m_type(AnimatedUnknown)
{
    ASSERT(m_animationElement);
}

void SVGAnimatedTypeAnimator::clear()
{
    m_contextElement = nullptr;
    m_animatedProperty = nullptr;
    m_type = AnimatedUnknown;
}

void SVGAnimatedTypeAnimator::reset(SVGElement* contextElement)
{
    ASSERT(contextElement);
    m_contextElement = contextElement;

    const QualifiedName& attributeName = m_animationElement->attributeName();
    m_animatedProperty = m_contextElement->propertyFromAttribute(attributeName);
    m_type = m_animatedProperty ? m_animatedProperty->type()
        : SVGElement::animatedPropertyTypeForCSSAttribute(attributeName);

    // Only <animateTransform> is allowed to animate AnimatedTransformList.
    // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
    if (m_type == AnimatedTransformList && !isSVGAnimateTransformElement(*m_animationElement))
        m_type = AnimatedUnknown;

    ASSERT(m_type != AnimatedPoint
        && m_type != AnimatedStringList
        && m_type != AnimatedTransform);
}

SVGPropertyBase* SVGAnimatedTypeAnimator::createPropertyForAnimation(const String& value)
{
    ASSERT(m_contextElement);

    if (isAnimatingSVGDom()) {
        // SVG DOM animVal animation code-path.

        if (m_type == AnimatedTransformList) {
            // TransformList must be animated via <animateTransform>,
            // and its {from,by,to} attribute values needs to be parsed w.r.t. its "type" attribute.
            // Spec: http://www.w3.org/TR/SVG/single-page.html#animate-AnimateTransformElement
            ASSERT(m_animationElement);
            SVGTransformType transformType = toSVGAnimateTransformElement(m_animationElement)->transformType();
            return SVGTransformList::create(transformType, value);
        }

        ASSERT(m_animatedProperty);
        return m_animatedProperty->currentValueBase()->cloneForAnimation(value);
    }

    ASSERT(isAnimatingCSSProperty());

    // CSS properties animation code-path.
    // Create a basic instance of the corresponding SVG property.
    // The instance will not have full context info. (e.g. SVGLengthMode)

    switch (m_type) {
    case AnimatedColor:
        return SVGColorProperty::create(value);
    case AnimatedNumber: {
        SVGNumber* property = SVGNumber::create();
        property->setValueAsString(value);
        return property;
    }
    case AnimatedLength: {
        SVGLength* property = SVGLength::create();
        property->setValueAsString(value);
        return property;
    }
    case AnimatedLengthList: {
        SVGLengthList* property = SVGLengthList::create();
        property->setValueAsString(value);
        return property;
    }
    case AnimatedString: {
        SVGString* property = SVGString::create();
        property->setValueAsString(value);
        return property;
    }

    // These types don't appear in the table in SVGElement::animatedPropertyTypeForCSSAttribute() and thus don't need support.
    case AnimatedAngle:
    case AnimatedBoolean:
    case AnimatedEnumeration:
    case AnimatedInteger:
    case AnimatedIntegerOptionalInteger:
    case AnimatedNumberList:
    case AnimatedNumberOptionalNumber:
    case AnimatedPath:
    case AnimatedPoint:
    case AnimatedPoints:
    case AnimatedPreserveAspectRatio:
    case AnimatedRect:
    case AnimatedStringList:
    case AnimatedTransform:
    case AnimatedTransformList:
        ASSERT_NOT_REACHED();

    case AnimatedUnknown:
        ASSERT_NOT_REACHED();
    };

    ASSERT_NOT_REACHED();
    return nullptr;
}

SVGPropertyBase* SVGAnimatedTypeAnimator::constructFromString(const String& value)
{
    return createPropertyForAnimation(value);
}

void SVGAnimatedTypeAnimator::calculateFromAndToValues(Member<SVGPropertyBase>& from, Member<SVGPropertyBase>& to, const String& fromString, const String& toString)
{
    from = constructFromString(fromString);
    to = constructFromString(toString);
}

void SVGAnimatedTypeAnimator::calculateFromAndByValues(Member<SVGPropertyBase>& from, Member<SVGPropertyBase>& to, const String& fromString, const String& byString)
{
    from = constructFromString(fromString);
    to = constructFromString(byString);
    to->add(from, m_contextElement);
}

SVGPropertyBase* SVGAnimatedTypeAnimator::resetAnimation()
{
    DCHECK(isAnimatingSVGDom());
    DCHECK(m_contextElement);
    SVGPropertyBase* animatedValue = m_animatedProperty->createAnimatedValue();
    DCHECK_EQ(animatedValue->type(), m_type);
    m_contextElement->setAnimatedAttribute(m_animatedProperty->attributeName(), animatedValue);
    return animatedValue;
}

SVGPropertyBase* SVGAnimatedTypeAnimator::startAnimValAnimation()
{
    return resetAnimation();
}

void SVGAnimatedTypeAnimator::stopAnimValAnimation()
{
    if (!isAnimatingSVGDom())
        return;
    DCHECK(m_contextElement);
    m_contextElement->clearAnimatedAttribute(m_animatedProperty->attributeName());
}

SVGPropertyBase* SVGAnimatedTypeAnimator::resetAnimValToBaseVal()
{
    return resetAnimation();
}

class ParsePropertyFromString {
    STACK_ALLOCATED();
public:
    explicit ParsePropertyFromString(SVGAnimatedTypeAnimator* animator)
        : m_animator(animator)
    {
    }

    SVGPropertyBase* operator()(SVGAnimationElement*, const String& value)
    {
        return m_animator->createPropertyForAnimation(value);
    }

private:
    SVGAnimatedTypeAnimator* m_animator;
};

void SVGAnimatedTypeAnimator::calculateAnimatedValue(float percentage, unsigned repeatCount, SVGPropertyBase* from, SVGPropertyBase* to, SVGPropertyBase* toAtEndOfDuration, SVGPropertyBase* animated)
{
    ASSERT(m_animationElement);
    ASSERT(m_contextElement);

    SVGPropertyBase* fromValue = m_animationElement->getAnimationMode() == ToAnimation ? animated : from;
    SVGPropertyBase* toValue = to;
    SVGPropertyBase* toAtEndOfDurationValue = toAtEndOfDuration;
    SVGPropertyBase* animatedValue = animated;

    // Apply CSS inheritance rules.
    ParsePropertyFromString parsePropertyFromString(this);
    m_animationElement->adjustForInheritance<SVGPropertyBase*, ParsePropertyFromString>(parsePropertyFromString, m_animationElement->fromPropertyValueType(), fromValue, m_contextElement);
    m_animationElement->adjustForInheritance<SVGPropertyBase*, ParsePropertyFromString>(parsePropertyFromString, m_animationElement->toPropertyValueType(), toValue, m_contextElement);

    animatedValue->calculateAnimatedValue(m_animationElement, percentage, repeatCount, fromValue, toValue, toAtEndOfDurationValue, m_contextElement);
}

float SVGAnimatedTypeAnimator::calculateDistance(const String& fromString, const String& toString)
{
    ASSERT(m_contextElement);
    SVGPropertyBase* fromValue = createPropertyForAnimation(fromString);
    SVGPropertyBase* toValue = createPropertyForAnimation(toString);
    return fromValue->calculateDistance(toValue, m_contextElement);
}

DEFINE_TRACE(SVGAnimatedTypeAnimator)
{
    visitor->trace(m_animationElement);
    visitor->trace(m_contextElement);
    visitor->trace(m_animatedProperty);
}

} // namespace blink
