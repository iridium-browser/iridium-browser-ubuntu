// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSURIValue_h
#define CSSURIValue_h

#include "core/css/CSSValue.h"
#include "core/fetch/DocumentResource.h"

namespace blink {

class Document;

class CSSURIValue : public CSSValue {
public:
    static CSSURIValue* create(const String& str)
    {
        return new CSSURIValue(str);
    }
    ~CSSURIValue();

    DocumentResource* cachedDocument() const { return m_document; }
    DocumentResource* load(Document&) const;

    const String& value() const { return m_url; }
    const String& url() const { return m_url; }

    String customCSSText() const;

    bool loadRequested() const { return m_loadRequested; }
    bool equals(const CSSURIValue&) const;

    DECLARE_TRACE_AFTER_DISPATCH();

private:
    CSSURIValue(const String&);

    String m_url;

    // Document cache.
    mutable Member<DocumentResource> m_document;
    mutable bool m_loadRequested;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSURIValue, isURIValue());

} // namespace blink

#endif // CSSURIValue_h
