// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableParser_h
#define CSSVariableParser_h

#include "core/CoreExport.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class CSSCustomPropertyDeclaration;

class CORE_EXPORT CSSVariableParser {
public:
    static bool containsValidVariableReferences(CSSParserTokenRange);

    static CSSCustomPropertyDeclaration* parseDeclarationValue(const AtomicString&, CSSParserTokenRange);

    static bool isValidVariableName(const CSSParserToken&);
    static bool isValidVariableName(const String&);
};

} // namespace blink

#endif // CSSVariableParser_h
