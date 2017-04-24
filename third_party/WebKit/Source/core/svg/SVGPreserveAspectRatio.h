/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGPreserveAspectRatio_h
#define SVGPreserveAspectRatio_h

#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGPropertyHelper.h"

namespace blink {

class AffineTransform;
class FloatRect;
class SVGPreserveAspectRatioTearOff;

class SVGPreserveAspectRatio final
    : public SVGPropertyHelper<SVGPreserveAspectRatio> {
 public:
  enum SVGPreserveAspectRatioType {
    kSvgPreserveaspectratioUnknown = 0,
    kSvgPreserveaspectratioNone = 1,
    kSvgPreserveaspectratioXminymin = 2,
    kSvgPreserveaspectratioXmidymin = 3,
    kSvgPreserveaspectratioXmaxymin = 4,
    kSvgPreserveaspectratioXminymid = 5,
    kSvgPreserveaspectratioXmidymid = 6,
    kSvgPreserveaspectratioXmaxymid = 7,
    kSvgPreserveaspectratioXminymax = 8,
    kSvgPreserveaspectratioXmidymax = 9,
    kSvgPreserveaspectratioXmaxymax = 10
  };

  enum SVGMeetOrSliceType {
    kSvgMeetorsliceUnknown = 0,
    kSvgMeetorsliceMeet = 1,
    kSvgMeetorsliceSlice = 2
  };

  typedef SVGPreserveAspectRatioTearOff TearOffType;

  static SVGPreserveAspectRatio* create() {
    return new SVGPreserveAspectRatio();
  }

  virtual SVGPreserveAspectRatio* clone() const;

  bool operator==(const SVGPreserveAspectRatio&) const;
  bool operator!=(const SVGPreserveAspectRatio& other) const {
    return !operator==(other);
  }

  void setAlign(SVGPreserveAspectRatioType align) { m_align = align; }
  SVGPreserveAspectRatioType align() const { return m_align; }

  void setMeetOrSlice(SVGMeetOrSliceType meetOrSlice) {
    m_meetOrSlice = meetOrSlice;
  }
  SVGMeetOrSliceType meetOrSlice() const { return m_meetOrSlice; }

  void transformRect(FloatRect& destRect, FloatRect& srcRect);

  AffineTransform getCTM(float logicX,
                         float logicY,
                         float logicWidth,
                         float logicHeight,
                         float physWidth,
                         float physHeight) const;

  String valueAsString() const override;
  SVGParsingError setValueAsString(const String&);
  bool parse(const UChar*& ptr, const UChar* end, bool validate);
  bool parse(const LChar*& ptr, const LChar* end, bool validate);

  void add(SVGPropertyBase*, SVGElement*) override;
  void calculateAnimatedValue(SVGAnimationElement*,
                              float percentage,
                              unsigned repeatCount,
                              SVGPropertyBase* from,
                              SVGPropertyBase* to,
                              SVGPropertyBase* toAtEndOfDurationValue,
                              SVGElement* contextElement) override;
  float calculateDistance(SVGPropertyBase* to,
                          SVGElement* contextElement) override;

  static AnimatedPropertyType classType() {
    return AnimatedPreserveAspectRatio;
  }

  void setDefault();

 private:
  SVGPreserveAspectRatio();

  template <typename CharType>
  SVGParsingError parseInternal(const CharType*& ptr,
                                const CharType* end,
                                bool validate);

  SVGPreserveAspectRatioType m_align;
  SVGMeetOrSliceType m_meetOrSlice;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGPreserveAspectRatio);

}  // namespace blink

#endif  // SVGPreserveAspectRatio_h
