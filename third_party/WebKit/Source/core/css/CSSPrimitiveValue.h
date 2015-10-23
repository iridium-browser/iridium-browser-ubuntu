/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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

#ifndef CSSPrimitiveValue_h
#define CSSPrimitiveValue_h

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "platform/graphics/Color.h"
#include "wtf/BitVector.h"
#include "wtf/Forward.h"
#include "wtf/MathExtras.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/StringHash.h"

namespace blink {

class CSSBasicShape;
class CSSCalcValue;
class CSSToLengthConversionData;
class Counter;
class Length;
class LengthSize;
class Pair;
class Quad;
class RGBColor;
class Rect;
class ComputedStyle;

// Dimension calculations are imprecise, often resulting in values of e.g.
// 44.99998. We need to go ahead and round if we're really close to the next
// integer value.
template<typename T> inline T roundForImpreciseConversion(double value)
{
    value += (value < 0) ? -0.01 : +0.01;
    return ((value > std::numeric_limits<T>::max()) || (value < std::numeric_limits<T>::min())) ? 0 : static_cast<T>(value);
}

template<> inline float roundForImpreciseConversion(double value)
{
    double ceiledValue = ceil(value);
    double proximityToNextInt = ceiledValue - value;
    if (proximityToNextInt <= 0.01 && value > 0)
        return static_cast<float>(ceiledValue);
    if (proximityToNextInt >= 0.99 && value < 0)
        return static_cast<float>(floor(value));
    return static_cast<float>(value);
}

// CSSPrimitiveValues are immutable. This class has manual ref-counting
// of unioned types and does not have the code necessary
// to handle any kind of mutations.
class CORE_EXPORT CSSPrimitiveValue : public CSSValue {
public:
    enum class UnitType {
        Unknown,
        Number,
        Percentage,
        Ems,
        Exs,
        Pixels,
        Centimeters,
        Millimeters,
        Inches,
        Points,
        Picas,
        Degrees,
        Radians,
        Gradians,
        Turns,
        Milliseconds,
        Seconds,
        Hertz,
        Kilohertz,
        CustomIdentifier,
        URI,
        Attribute,
        Counter,
        Rect,
        RGBColor,
        ViewportWidth,
        ViewportHeight,
        ViewportMin,
        ViewportMax,
        DotsPerPixel,
        DotsPerInch,
        DotsPerCentimeter,
        Fraction,
        Integer,
        Pair,
        Rems,
        Chs,
        Shape,
        Quad,
        Calc,
        CalcPercentageWithNumber,
        CalcPercentageWithLength,
        String,
        PropertyID,
        ValueID,
        QuirkyEms,
    };

    enum LengthUnitType {
        UnitTypePixels = 0,
        UnitTypePercentage,
        UnitTypeFontSize,
        UnitTypeFontXSize,
        UnitTypeRootFontSize,
        UnitTypeZeroCharacterWidth,
        UnitTypeViewportWidth,
        UnitTypeViewportHeight,
        UnitTypeViewportMin,
        UnitTypeViewportMax,

        // This value must come after the last length unit type to enable iteration over the length unit types.
        LengthUnitTypeCount,
    };

    typedef Vector<double, CSSPrimitiveValue::LengthUnitTypeCount> CSSLengthArray;
    typedef BitVector CSSLengthTypeArray;

    void accumulateLengthArray(CSSLengthArray&, double multiplier = 1) const;
    void accumulateLengthArray(CSSLengthArray&, CSSLengthTypeArray&, double multiplier = 1) const;

    enum UnitCategory {
        UNumber,
        UPercent,
        ULength,
        UAngle,
        UTime,
        UFrequency,
        UResolution,
        UOther
    };
    static UnitCategory unitCategory(UnitType);
    static float clampToCSSLengthRange(double);

    static void initUnitTable();

    static UnitType fromName(const String& unit);

    bool isAngle() const
    {
        return type() == UnitType::Degrees
            || type() == UnitType::Radians
            || type() == UnitType::Gradians
            || type() == UnitType::Turns;
    }
    bool isAttr() const { return type() == UnitType::Attribute; }
    bool isCounter() const { return type() == UnitType::Counter; }
    bool isCustomIdent() const { return type() == UnitType::CustomIdentifier; }
    bool isFontRelativeLength() const
    {
        return type() == UnitType::Ems
            || type() == UnitType::Exs
            || type() == UnitType::Rems
            || type() == UnitType::Chs;
    }
    bool isViewportPercentageLength() const { return isViewportPercentageLength(type()); }
    static bool isViewportPercentageLength(UnitType type) { return type >= UnitType::ViewportWidth && type <= UnitType::ViewportMax; }
    static bool isLength(UnitType type)
    {
        return (type >= UnitType::Ems && type <= UnitType::Picas) || type == UnitType::Rems || type == UnitType::Chs || isViewportPercentageLength(type);
    }
    bool isLength() const { return isLength(typeWithCalcResolved()); }
    bool isNumber() const { return typeWithCalcResolved() == UnitType::Number || typeWithCalcResolved() == UnitType::Integer; }
    bool isPercentage() const { return typeWithCalcResolved() == UnitType::Percentage; }
    bool isPropertyID() const { return type() == UnitType::PropertyID; }
    bool isPx() const { return typeWithCalcResolved() == UnitType::Pixels; }
    bool isQuad() const { return type() == UnitType::Quad; }
    bool isRect() const { return type() == UnitType::Rect; }
    bool isRGBColor() const { return type() == UnitType::RGBColor; }
    bool isShape() const { return type() == UnitType::Shape; }
    bool isString() const { return type() == UnitType::String; }
    bool isTime() const { return type() == UnitType::Seconds || type() == UnitType::Milliseconds; }
    bool isURI() const { return type() == UnitType::URI; }
    bool isCalculated() const { return type() == UnitType::Calc; }
    bool isCalculatedPercentageWithNumber() const { return typeWithCalcResolved() == UnitType::CalcPercentageWithNumber; }
    bool isCalculatedPercentageWithLength() const { return typeWithCalcResolved() == UnitType::CalcPercentageWithLength; }
    static bool isDotsPerInch(UnitType type) { return type == UnitType::DotsPerInch; }
    static bool isDotsPerPixel(UnitType type) { return type == UnitType::DotsPerPixel; }
    static bool isDotsPerCentimeter(UnitType type) { return type == UnitType::DotsPerCentimeter; }
    static bool isResolution(UnitType type) { return type >= UnitType::DotsPerPixel && type <= UnitType::DotsPerCentimeter; }
    bool isFlex() const { return typeWithCalcResolved() == UnitType::Fraction; }
    bool isValueID() const { return type() == UnitType::ValueID; }
    bool colorIsDerivedFromElement() const;

    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> createIdentifier(CSSValueID valueID)
    {
        return adoptRefWillBeNoop(new CSSPrimitiveValue(valueID));
    }
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> createIdentifier(CSSPropertyID propertyID)
    {
        return adoptRefWillBeNoop(new CSSPrimitiveValue(propertyID));
    }
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> createColor(RGBA32 rgbValue)
    {
        return adoptRefWillBeNoop(new CSSPrimitiveValue(rgbValue));
    }
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> create(double value, UnitType type)
    {
        return adoptRefWillBeNoop(new CSSPrimitiveValue(value, type));
    }
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> create(const String& value, UnitType type)
    {
        return adoptRefWillBeNoop(new CSSPrimitiveValue(value, type));
    }
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> create(const Length& value, float zoom)
    {
        return adoptRefWillBeNoop(new CSSPrimitiveValue(value, zoom));
    }
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> create(const LengthSize& value, const ComputedStyle& style)
    {
        return adoptRefWillBeNoop(new CSSPrimitiveValue(value, style));
    }
    template<typename T> static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> create(T value)
    {
        return adoptRefWillBeNoop(new CSSPrimitiveValue(value));
    }

    // This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
    // The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em.
    // When the quirky value is used, if you're in quirks mode, the margin will collapse away
    // inside a table cell.
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> createAllowingMarginQuirk(double value, UnitType type)
    {
        CSSPrimitiveValue* quirkValue = new CSSPrimitiveValue(value, type);
        quirkValue->m_isQuirkValue = true;
        return adoptRefWillBeNoop(quirkValue);
    }

    ~CSSPrimitiveValue();

    void cleanup();

    UnitType typeWithCalcResolved() const;

    double computeDegrees() const;
    double computeSeconds();

    // Computes a length in pixels, resolving relative lengths
    template<typename T> T computeLength(const CSSToLengthConversionData&);

    // Converts to a Length (Fixed, Percent or Calculated)
    Length convertToLength(const CSSToLengthConversionData&);

    double getDoubleValue() const;
    float getFloatValue() const { return getValue<float>(); }
    int getIntValue() const { return getValue<int>(); }
    template<typename T> inline T getValue() const { return clampTo<T>(getDoubleValue()); }

    String getStringValue() const;

    Counter* getCounterValue() const { ASSERT(isCounter()); return m_value.counter; }
    Rect* getRectValue() const { ASSERT(isRect()); return m_value.rect; }
    Quad* getQuadValue() const { ASSERT(isQuad()); return m_value.quad; }
    RGBA32 getRGBA32Value() const { ASSERT(isRGBColor()); return m_value.rgbcolor; }

    // TODO(timloh): Add isPair() and update callers so we can ASSERT(isPair())
    Pair* getPairValue() const { return type() != UnitType::Pair ? 0 : m_value.pair; }

    CSSBasicShape* getShapeValue() const { ASSERT(isShape()); return m_value.shape; }
    CSSCalcValue* cssCalcValue() const { ASSERT(isCalculated()); return m_value.calc; }
    CSSPropertyID getPropertyID() const { ASSERT(isPropertyID()); return m_value.propertyID; }

    CSSValueID getValueID() const { return type() == UnitType::ValueID ? m_value.valueID : CSSValueInvalid; }

    template<typename T> inline operator T() const; // Defined in CSSPrimitiveValueMappings.h

    static const char* unitTypeToString(UnitType);
    String customCSSText() const;

    bool isQuirkValue() { return m_isQuirkValue; }

    bool equals(const CSSPrimitiveValue&) const;

    DECLARE_TRACE_AFTER_DISPATCH();

    static UnitType canonicalUnitTypeForCategory(UnitCategory);
    static double conversionToCanonicalUnitsScaleFactor(UnitType);

    // Returns true and populates lengthUnitType, if unitType is a length unit. Otherwise, returns false.
    static bool unitTypeToLengthUnitType(UnitType, LengthUnitType&);
    static UnitType lengthUnitTypeToUnitType(LengthUnitType);

private:
    CSSPrimitiveValue(CSSValueID);
    CSSPrimitiveValue(CSSPropertyID);
    CSSPrimitiveValue(RGBA32 color);
    CSSPrimitiveValue(const Length&, float zoom);
    CSSPrimitiveValue(const LengthSize&, const ComputedStyle&);
    CSSPrimitiveValue(const String&, UnitType);
    CSSPrimitiveValue(double, UnitType);

    template<typename T> CSSPrimitiveValue(T); // Defined in CSSPrimitiveValueMappings.h
    template<typename T> CSSPrimitiveValue(T* val)
        : CSSValue(PrimitiveClass)
    {
        init(PassRefPtrWillBeRawPtr<T>(val));
    }

    template<typename T> CSSPrimitiveValue(PassRefPtrWillBeRawPtr<T> val)
        : CSSValue(PrimitiveClass)
    {
        init(val);
    }

    static void create(int); // compile-time guard
    static void create(unsigned); // compile-time guard
    template<typename T> operator T*(); // compile-time guard

    void init(UnitType);
    void init(const Length&);
    void init(const LengthSize&, const ComputedStyle&);
    void init(PassRefPtrWillBeRawPtr<Counter>);
    void init(PassRefPtrWillBeRawPtr<Rect>);
    void init(PassRefPtrWillBeRawPtr<Pair>);
    void init(PassRefPtrWillBeRawPtr<Quad>);
    void init(PassRefPtrWillBeRawPtr<CSSBasicShape>);
    void init(PassRefPtrWillBeRawPtr<CSSCalcValue>);

    double computeLengthDouble(const CSSToLengthConversionData&);

    inline UnitType type() const { return static_cast<UnitType>(m_primitiveUnitType); }

    union {
        CSSPropertyID propertyID;
        CSSValueID valueID;
        double num;
        StringImpl* string;
        RGBA32 rgbcolor;
        // FIXME: oilpan: Should be members, but no support for members in unions. Just trace the raw ptr for now.
        CSSBasicShape* shape;
        CSSCalcValue* calc;
        Counter* counter;
        Pair* pair;
        Rect* rect;
        Quad* quad;
    } m_value;
};

typedef CSSPrimitiveValue::CSSLengthArray CSSLengthArray;
typedef CSSPrimitiveValue::CSSLengthTypeArray CSSLengthTypeArray;

DEFINE_CSS_VALUE_TYPE_CASTS(CSSPrimitiveValue, isPrimitiveValue());

} // namespace blink

#endif // CSSPrimitiveValue_h
