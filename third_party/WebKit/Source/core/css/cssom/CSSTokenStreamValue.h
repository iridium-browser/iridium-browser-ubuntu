// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTokenStreamValue_h
#define CSSTokenStreamValue_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/StringOrCSSVariableReferenceValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT CSSTokenStreamValue final : public CSSStyleValue, public ValueIterable<StringOrCSSVariableReferenceValue> {
    WTF_MAKE_NONCOPYABLE(CSSTokenStreamValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSTokenStreamValue* create(const HeapVector<StringOrCSSVariableReferenceValue>& fragments)
    {
        return new CSSTokenStreamValue(fragments);
    }

    CSSValue* toCSSValue() const override;

    StyleValueType type() const override { return TokenStreamType; }

    StringOrCSSVariableReferenceValue fragmentAtIndex(int index) const { return m_fragments.at(index); }

    size_t size() const { return m_fragments.size(); }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_fragments);
        CSSStyleValue::trace(visitor);
    }

protected:
    CSSTokenStreamValue(const HeapVector<StringOrCSSVariableReferenceValue>& fragments)
        : CSSStyleValue()
        , m_fragments(fragments)
    {
    }

private:
    IterationSource* startIteration(ScriptState*, ExceptionState&) override;

    HeapVector<StringOrCSSVariableReferenceValue> m_fragments;
};

} // namespace blink

#endif // CSSTokenStreamValue_h
