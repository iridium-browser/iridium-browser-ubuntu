// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSColorInterpolationType.h"

#include "core/animation/ColorPropertyFunctions.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/layout/LayoutTheme.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

enum InterpolableColorIndex : unsigned {
  Red,
  Green,
  Blue,
  Alpha,
  Currentcolor,
  WebkitActivelink,
  WebkitLink,
  QuirkInherit,
  InterpolableColorIndexCount,
};

static std::unique_ptr<InterpolableValue> createInterpolableColorForIndex(
    InterpolableColorIndex index) {
  DCHECK_LT(index, InterpolableColorIndexCount);
  std::unique_ptr<InterpolableList> list =
      InterpolableList::create(InterpolableColorIndexCount);
  for (unsigned i = 0; i < InterpolableColorIndexCount; i++)
    list->set(i, InterpolableNumber::create(i == index));
  return std::move(list);
}

std::unique_ptr<InterpolableValue>
CSSColorInterpolationType::createInterpolableColor(const Color& color) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::create(InterpolableColorIndexCount);
  list->set(Red, InterpolableNumber::create(color.red() * color.alpha()));
  list->set(Green, InterpolableNumber::create(color.green() * color.alpha()));
  list->set(Blue, InterpolableNumber::create(color.blue() * color.alpha()));
  list->set(Alpha, InterpolableNumber::create(color.alpha()));
  list->set(Currentcolor, InterpolableNumber::create(0));
  list->set(WebkitActivelink, InterpolableNumber::create(0));
  list->set(WebkitLink, InterpolableNumber::create(0));
  list->set(QuirkInherit, InterpolableNumber::create(0));
  return std::move(list);
}

std::unique_ptr<InterpolableValue>
CSSColorInterpolationType::createInterpolableColor(CSSValueID keyword) {
  switch (keyword) {
    case CSSValueCurrentcolor:
      return createInterpolableColorForIndex(Currentcolor);
    case CSSValueWebkitActivelink:
      return createInterpolableColorForIndex(WebkitActivelink);
    case CSSValueWebkitLink:
      return createInterpolableColorForIndex(WebkitLink);
    case CSSValueInternalQuirkInherit:
      return createInterpolableColorForIndex(QuirkInherit);
    case CSSValueWebkitFocusRingColor:
      return createInterpolableColor(LayoutTheme::theme().focusRingColor());
    default:
      DCHECK(StyleColor::isColorKeyword(keyword));
      return createInterpolableColor(StyleColor::colorFromKeyword(keyword));
  }
}

std::unique_ptr<InterpolableValue>
CSSColorInterpolationType::createInterpolableColor(const StyleColor& color) {
  if (color.isCurrentColor())
    return createInterpolableColorForIndex(Currentcolor);
  return createInterpolableColor(color.getColor());
}

std::unique_ptr<InterpolableValue>
CSSColorInterpolationType::maybeCreateInterpolableColor(const CSSValue& value) {
  if (value.isColorValue())
    return createInterpolableColor(toCSSColorValue(value).value());
  if (!value.isIdentifierValue())
    return nullptr;
  const CSSIdentifierValue& identifierValue = toCSSIdentifierValue(value);
  if (!StyleColor::isColorKeyword(identifierValue.getValueID()))
    return nullptr;
  return createInterpolableColor(identifierValue.getValueID());
}

static void addPremultipliedColor(double& red,
                                  double& green,
                                  double& blue,
                                  double& alpha,
                                  double fraction,
                                  const Color& color) {
  double colorAlpha = color.alpha();
  red += fraction * color.red() * colorAlpha;
  green += fraction * color.green() * colorAlpha;
  blue += fraction * color.blue() * colorAlpha;
  alpha += fraction * colorAlpha;
}

Color CSSColorInterpolationType::resolveInterpolableColor(
    const InterpolableValue& interpolableColor,
    const StyleResolverState& state,
    bool isVisited,
    bool isTextDecoration) {
  const InterpolableList& list = toInterpolableList(interpolableColor);
  DCHECK_EQ(list.length(), InterpolableColorIndexCount);

  double red = toInterpolableNumber(list.get(Red))->value();
  double green = toInterpolableNumber(list.get(Green))->value();
  double blue = toInterpolableNumber(list.get(Blue))->value();
  double alpha = toInterpolableNumber(list.get(Alpha))->value();

  if (double currentcolorFraction =
          toInterpolableNumber(list.get(Currentcolor))->value()) {
    auto currentColorGetter = isVisited
                                  ? ColorPropertyFunctions::getVisitedColor
                                  : ColorPropertyFunctions::getUnvisitedColor;
    StyleColor currentStyleColor = StyleColor::currentColor();
    if (isTextDecoration)
      currentStyleColor =
          currentColorGetter(CSSPropertyWebkitTextFillColor, *state.style());
    if (currentStyleColor.isCurrentColor())
      currentStyleColor = currentColorGetter(CSSPropertyColor, *state.style());
    addPremultipliedColor(red, green, blue, alpha, currentcolorFraction,
                          currentStyleColor.getColor());
  }
  const TextLinkColors& colors = state.document().textLinkColors();
  if (double webkitActivelinkFraction =
          toInterpolableNumber(list.get(WebkitActivelink))->value())
    addPremultipliedColor(red, green, blue, alpha, webkitActivelinkFraction,
                          colors.activeLinkColor());
  if (double webkitLinkFraction =
          toInterpolableNumber(list.get(WebkitLink))->value())
    addPremultipliedColor(
        red, green, blue, alpha, webkitLinkFraction,
        isVisited ? colors.visitedLinkColor() : colors.linkColor());
  if (double quirkInheritFraction =
          toInterpolableNumber(list.get(QuirkInherit))->value())
    addPremultipliedColor(red, green, blue, alpha, quirkInheritFraction,
                          colors.textColor());

  alpha = clampTo<double>(alpha, 0, 255);
  if (alpha == 0)
    return Color::transparent;

  return makeRGBA(round(red / alpha), round(green / alpha), round(blue / alpha),
                  round(alpha));
}

class InheritedColorChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedColorChecker> create(
      CSSPropertyID property,
      const StyleColor& color) {
    return WTF::wrapUnique(new InheritedColorChecker(property, color));
  }

 private:
  InheritedColorChecker(CSSPropertyID property, const StyleColor& color)
      : m_property(property), m_color(color) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    return m_color == ColorPropertyFunctions::getUnvisitedColor(
                          m_property, *environment.state().parentStyle());
  }

  const CSSPropertyID m_property;
  const StyleColor m_color;
};

InterpolationValue CSSColorInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return convertStyleColorPair(StyleColor(Color::transparent),
                               StyleColor(Color::transparent));
}

InterpolationValue CSSColorInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversionCheckers) const {
  StyleColor initialColor;
  if (ColorPropertyFunctions::getInitialColor(cssProperty(), initialColor))
    return convertStyleColorPair(initialColor, initialColor);
  return nullptr;
}

InterpolationValue CSSColorInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  if (!state.parentStyle())
    return nullptr;
  // Visited color can never explicitly inherit from parent visited color so
  // only use the unvisited color.
  const StyleColor inheritedColor = ColorPropertyFunctions::getUnvisitedColor(
      cssProperty(), *state.parentStyle());
  conversionCheckers.push_back(
      InheritedColorChecker::create(cssProperty(), inheritedColor));
  return convertStyleColorPair(inheritedColor, inheritedColor);
}

enum InterpolableColorPairIndex : unsigned {
  Unvisited,
  Visited,
  InterpolableColorPairIndexCount,
};

InterpolationValue CSSColorInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  if (cssProperty() == CSSPropertyColor && value.isIdentifierValue() &&
      toCSSIdentifierValue(value).getValueID() == CSSValueCurrentcolor)
    return maybeConvertInherit(state, conversionCheckers);

  std::unique_ptr<InterpolableValue> interpolableColor =
      maybeCreateInterpolableColor(value);
  if (!interpolableColor)
    return nullptr;
  std::unique_ptr<InterpolableList> colorPair =
      InterpolableList::create(InterpolableColorPairIndexCount);
  colorPair->set(Unvisited, interpolableColor->clone());
  colorPair->set(Visited, std::move(interpolableColor));
  return InterpolationValue(std::move(colorPair));
}

InterpolationValue CSSColorInterpolationType::convertStyleColorPair(
    const StyleColor& unvisitedColor,
    const StyleColor& visitedColor) const {
  std::unique_ptr<InterpolableList> colorPair =
      InterpolableList::create(InterpolableColorPairIndexCount);
  colorPair->set(Unvisited, createInterpolableColor(unvisitedColor));
  colorPair->set(Visited, createInterpolableColor(visitedColor));
  return InterpolationValue(std::move(colorPair));
}

InterpolationValue
CSSColorInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const StyleResolverState& state) const {
  return convertStyleColorPair(
      ColorPropertyFunctions::getUnvisitedColor(cssProperty(), *state.style()),
      ColorPropertyFunctions::getVisitedColor(cssProperty(), *state.style()));
}

void CSSColorInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  const InterpolableList& colorPair = toInterpolableList(interpolableValue);
  DCHECK_EQ(colorPair.length(), InterpolableColorPairIndexCount);
  ColorPropertyFunctions::setUnvisitedColor(
      cssProperty(), *state.style(),
      resolveInterpolableColor(
          *colorPair.get(Unvisited), state, false,
          cssProperty() == CSSPropertyTextDecorationColor));
  ColorPropertyFunctions::setVisitedColor(
      cssProperty(), *state.style(),
      resolveInterpolableColor(
          *colorPair.get(Visited), state, true,
          cssProperty() == CSSPropertyTextDecorationColor));
}

}  // namespace blink
