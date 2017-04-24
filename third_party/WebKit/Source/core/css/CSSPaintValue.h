// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPaintValue_h
#define CSSPaintValue_h

#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/CSSPaintImageGenerator.h"
#include "core/css/CSSVariableData.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class CSSPaintValue : public CSSImageGeneratorValue {
 public:
  static CSSPaintValue* create(CSSCustomIdentValue* name) {
    return new CSSPaintValue(name);
  }

  static CSSPaintValue* create(CSSCustomIdentValue* name,
                               Vector<RefPtr<CSSVariableData>>& variableData) {
    return new CSSPaintValue(name, variableData);
  }

  ~CSSPaintValue();

  String customCSSText() const;

  String name() const;

  PassRefPtr<Image> image(const LayoutObject&, const IntSize&, float zoom);
  bool isFixedSize() const { return false; }
  IntSize fixedSize(const LayoutObject&) { return IntSize(); }

  bool isPending() const { return true; }
  bool knownToBeOpaque(const LayoutObject&) const;

  void loadSubimages(const Document&) {}

  bool equals(const CSSPaintValue&) const;

  const Vector<CSSPropertyID>* nativeInvalidationProperties() const {
    return m_generator ? &m_generator->nativeInvalidationProperties() : nullptr;
  }
  const Vector<AtomicString>* customInvalidationProperties() const {
    return m_generator ? &m_generator->customInvalidationProperties() : nullptr;
  }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  explicit CSSPaintValue(CSSCustomIdentValue* name);

  CSSPaintValue(CSSCustomIdentValue* name, Vector<RefPtr<CSSVariableData>>&);

  class Observer final : public CSSPaintImageGenerator::Observer {
    WTF_MAKE_NONCOPYABLE(Observer);

   public:
    explicit Observer(CSSPaintValue* ownerValue) : m_ownerValue(ownerValue) {}

    ~Observer() override {}
    DEFINE_INLINE_VIRTUAL_TRACE() {
      visitor->trace(m_ownerValue);
      CSSPaintImageGenerator::Observer::trace(visitor);
    }

    void paintImageGeneratorReady() final;

   private:
    Member<CSSPaintValue> m_ownerValue;
  };

  void paintImageGeneratorReady();

  bool parseInputArguments();

  bool m_inputArgumentsInvalid = false;

  Member<CSSCustomIdentValue> m_name;
  Member<CSSPaintImageGenerator> m_generator;
  Member<Observer> m_paintImageGeneratorObserver;
  Member<CSSStyleValueVector> m_parsedInputArguments;
  Vector<RefPtr<CSSVariableData>> m_argumentVariableData;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSPaintValue, isPaintValue());

}  // namespace blink

#endif  // CSSPaintValue_h
