/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef SVGLengthTearOff_h
#define SVGLengthTearOff_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/svg/SVGLength.h"
#include "core/svg/properties/SVGPropertyTearOff.h"

namespace blink {

class SVGLengthTearOff final : public SVGPropertyTearOff<SVGLength>,
                               public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Forward declare these enums in the w3c naming scheme, for IDL generation
  enum {
    kSvgLengthtypeUnknown = 0,
    kSvgLengthtypeNumber = 1,
    kSvgLengthtypePercentage = 2,
    kSvgLengthtypeEms = 3,
    kSvgLengthtypeExs = 4,
    kSvgLengthtypePx = 5,
    kSvgLengthtypeCm = 6,
    kSvgLengthtypeMm = 7,
    kSvgLengthtypeIn = 8,
    kSvgLengthtypePt = 9,
    kSvgLengthtypePc = 10
  };

  static SVGLengthTearOff* create(
      SVGLength* target,
      SVGElement* contextElement,
      PropertyIsAnimValType propertyIsAnimVal,
      const QualifiedName& attributeName = QualifiedName::null()) {
    return new SVGLengthTearOff(target, contextElement, propertyIsAnimVal,
                                attributeName);
  }

  unsigned short unitType();
  SVGLengthMode unitMode();
  float value(ExceptionState&);
  void setValue(float value, ExceptionState&);
  float valueInSpecifiedUnits();
  void setValueInSpecifiedUnits(float value, ExceptionState&);
  String valueAsString();
  void setValueAsString(const String&, ExceptionState&);
  void newValueSpecifiedUnits(unsigned short unitType,
                              float valueInSpecifiedUnits,
                              ExceptionState&);
  void convertToSpecifiedUnits(unsigned short unitType, ExceptionState&);

  bool hasExposedLengthUnit();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  SVGLengthTearOff(SVGLength*,
                   SVGElement* contextElement,
                   PropertyIsAnimValType,
                   const QualifiedName& attributeName = QualifiedName::null());
};

}  // namespace blink

#endif  // SVGLengthTearOff_h
