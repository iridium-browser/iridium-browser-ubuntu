// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSLengthValue_h
#define CSSLengthValue_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CSSCalcDictionary;
class ExceptionState;

class CORE_EXPORT CSSLengthValue : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSLengthValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const int kNumSupportedUnits = 15;

  static CSSLengthValue* from(const String& cssText, ExceptionState&);
  static CSSLengthValue* from(double value,
                              const String& typeStr,
                              ExceptionState&);
  static CSSLengthValue* from(const CSSCalcDictionary&, ExceptionState&);

  static bool isSupportedLengthUnit(CSSPrimitiveValue::UnitType unit) {
    return (CSSPrimitiveValue::isLength(unit) ||
            unit == CSSPrimitiveValue::UnitType::Percentage) &&
           unit != CSSPrimitiveValue::UnitType::QuirkyEms &&
           unit != CSSPrimitiveValue::UnitType::UserUnits;
  }
  static CSSPrimitiveValue::UnitType unitFromName(const String& name);
  static CSSLengthValue* fromCSSValue(const CSSPrimitiveValue&);

  CSSLengthValue* add(const CSSLengthValue* other);
  CSSLengthValue* subtract(const CSSLengthValue* other);
  CSSLengthValue* multiply(double);
  CSSLengthValue* divide(double, ExceptionState&);

  virtual bool containsPercent() const = 0;

 protected:
  CSSLengthValue() {}

  virtual CSSLengthValue* addInternal(const CSSLengthValue* other);
  virtual CSSLengthValue* subtractInternal(const CSSLengthValue* other);
  virtual CSSLengthValue* multiplyInternal(double);
  virtual CSSLengthValue* divideInternal(double);
};

DEFINE_TYPE_CASTS(CSSLengthValue,
                  CSSStyleValue,
                  value,
                  (value->type() == CSSStyleValue::SimpleLengthType ||
                   value->type() == CSSStyleValue::CalcLengthType),
                  (value.type() == CSSStyleValue::SimpleLengthType ||
                   value.type() == CSSStyleValue::CalcLengthType));

}  // namespace blink

#endif
