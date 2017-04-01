/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGFEBlendElement.h"

#include "core/SVGNames.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/graphics/filters/FEBlend.h"

namespace blink {

static WebBlendMode toWebBlendMode(SVGFEBlendElement::Mode mode) {
#define MAP_BLEND_MODE(MODENAME)          \
  case SVGFEBlendElement::Mode##MODENAME: \
    return WebBlendMode##MODENAME

  switch (mode) {
    MAP_BLEND_MODE(Normal);
    MAP_BLEND_MODE(Multiply);
    MAP_BLEND_MODE(Screen);
    MAP_BLEND_MODE(Darken);
    MAP_BLEND_MODE(Lighten);
    MAP_BLEND_MODE(Overlay);
    MAP_BLEND_MODE(ColorDodge);
    MAP_BLEND_MODE(ColorBurn);
    MAP_BLEND_MODE(HardLight);
    MAP_BLEND_MODE(SoftLight);
    MAP_BLEND_MODE(Difference);
    MAP_BLEND_MODE(Exclusion);
    MAP_BLEND_MODE(Hue);
    MAP_BLEND_MODE(Saturation);
    MAP_BLEND_MODE(Color);
    MAP_BLEND_MODE(Luminosity);
    default:
      ASSERT_NOT_REACHED();
      return WebBlendModeNormal;
  }
#undef MAP_BLEND_MODE
}

template <>
const SVGEnumerationStringEntries&
getStaticStringEntries<SVGFEBlendElement::Mode>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.isEmpty()) {
    entries.push_back(std::make_pair(SVGFEBlendElement::ModeNormal, "normal"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeMultiply, "multiply"));
    entries.push_back(std::make_pair(SVGFEBlendElement::ModeScreen, "screen"));
    entries.push_back(std::make_pair(SVGFEBlendElement::ModeDarken, "darken"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeLighten, "lighten"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeOverlay, "overlay"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeColorDodge, "color-dodge"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeColorBurn, "color-burn"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeHardLight, "hard-light"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeSoftLight, "soft-light"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeDifference, "difference"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeExclusion, "exclusion"));
    entries.push_back(std::make_pair(SVGFEBlendElement::ModeHue, "hue"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeSaturation, "saturation"));
    entries.push_back(std::make_pair(SVGFEBlendElement::ModeColor, "color"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::ModeLuminosity, "luminosity"));
  }
  return entries;
}

template <>
unsigned short getMaxExposedEnumValue<SVGFEBlendElement::Mode>() {
  return SVGFEBlendElement::ModeLighten;
}

inline SVGFEBlendElement::SVGFEBlendElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(SVGNames::feBlendTag, document),
      m_in1(SVGAnimatedString::create(this, SVGNames::inAttr)),
      m_in2(SVGAnimatedString::create(this, SVGNames::in2Attr)),
      m_mode(
          SVGAnimatedEnumeration<Mode>::create(this,
                                               SVGNames::modeAttr,
                                               SVGFEBlendElement::ModeNormal)) {
  addToPropertyMap(m_in1);
  addToPropertyMap(m_in2);
  addToPropertyMap(m_mode);
}

DEFINE_TRACE(SVGFEBlendElement) {
  visitor->trace(m_in1);
  visitor->trace(m_in2);
  visitor->trace(m_mode);
  SVGFilterPrimitiveStandardAttributes::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGFEBlendElement)

bool SVGFEBlendElement::setFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attrName) {
  FEBlend* blend = static_cast<FEBlend*>(effect);
  if (attrName == SVGNames::modeAttr)
    return blend->setBlendMode(
        toWebBlendMode(m_mode->currentValue()->enumValue()));

  return SVGFilterPrimitiveStandardAttributes::setFilterEffectAttribute(
      effect, attrName);
}

void SVGFEBlendElement::svgAttributeChanged(const QualifiedName& attrName) {
  if (attrName == SVGNames::modeAttr) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    primitiveAttributeChanged(attrName);
    return;
  }

  if (attrName == SVGNames::inAttr || attrName == SVGNames::in2Attr) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::svgAttributeChanged(attrName);
}

FilterEffect* SVGFEBlendElement::build(SVGFilterBuilder* filterBuilder,
                                       Filter* filter) {
  FilterEffect* input1 = filterBuilder->getEffectById(
      AtomicString(m_in1->currentValue()->value()));
  FilterEffect* input2 = filterBuilder->getEffectById(
      AtomicString(m_in2->currentValue()->value()));
  ASSERT(input1 && input2);

  FilterEffect* effect = FEBlend::create(
      filter, toWebBlendMode(m_mode->currentValue()->enumValue()));
  FilterEffectVector& inputEffects = effect->inputEffects();
  inputEffects.reserveCapacity(2);
  inputEffects.push_back(input1);
  inputEffects.push_back(input2);
  return effect;
}

}  // namespace blink
