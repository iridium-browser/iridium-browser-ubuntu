// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSRotation.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"

namespace blink {

namespace {

bool isNumberValue(const CSSValue& value) {
  return value.isPrimitiveValue() && toCSSPrimitiveValue(value).isNumber();
}

CSSRotation* fromCSSRotate(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);
  const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value.item(0));
  if (!primitiveValue.isAngle())
    return nullptr;
  return CSSRotation::create(CSSAngleValue::fromCSSValue(primitiveValue));
}

CSSRotation* fromCSSRotate3d(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 4UL);
  DCHECK(isNumberValue(value.item(0)));
  DCHECK(isNumberValue(value.item(1)));
  DCHECK(isNumberValue(value.item(2)));
  const CSSPrimitiveValue& angle = toCSSPrimitiveValue(value.item(3));
  if (!angle.isAngle())
    return nullptr;

  double x = toCSSPrimitiveValue(value.item(0)).getDoubleValue();
  double y = toCSSPrimitiveValue(value.item(1)).getDoubleValue();
  double z = toCSSPrimitiveValue(value.item(2)).getDoubleValue();

  return CSSRotation::create(x, y, z, CSSAngleValue::fromCSSValue(angle));
}

CSSRotation* fromCSSRotateXYZ(const CSSFunctionValue& value) {
  DCHECK_EQ(value.length(), 1UL);

  CSSAngleValue* angle =
      CSSAngleValue::fromCSSValue(toCSSPrimitiveValue(value.item(0)));
  switch (value.functionType()) {
    case CSSValueRotateX:
      return CSSRotation::create(1, 0, 0, angle);
    case CSSValueRotateY:
      return CSSRotation::create(0, 1, 0, angle);
    case CSSValueRotateZ:
      return CSSRotation::create(0, 0, 1, angle);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

CSSRotation* CSSRotation::fromCSSValue(const CSSFunctionValue& value) {
  switch (value.functionType()) {
    case CSSValueRotate:
      return fromCSSRotate(value);
    case CSSValueRotate3d:
      return fromCSSRotate3d(value);
    case CSSValueRotateX:
    case CSSValueRotateY:
    case CSSValueRotateZ:
      return fromCSSRotateXYZ(value);
    default:
      NOTREACHED();
      return nullptr;
  }
}

CSSFunctionValue* CSSRotation::toCSSValue() const {
  CSSFunctionValue* result =
      CSSFunctionValue::create(m_is2D ? CSSValueRotate : CSSValueRotate3d);
  if (!m_is2D) {
    result->append(
        *CSSPrimitiveValue::create(m_x, CSSPrimitiveValue::UnitType::Number));
    result->append(
        *CSSPrimitiveValue::create(m_y, CSSPrimitiveValue::UnitType::Number));
    result->append(
        *CSSPrimitiveValue::create(m_z, CSSPrimitiveValue::UnitType::Number));
  }
  result->append(*CSSPrimitiveValue::create(m_angle->value(), m_angle->unit()));
  return result;
}

}  // namespace blink
