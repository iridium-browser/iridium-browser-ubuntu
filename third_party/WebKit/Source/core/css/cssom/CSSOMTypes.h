// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSOMTypes_h
#define CSSOMTypes_h

#include "core/CSSPropertyNames.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSStyleValue;

class CSSOMTypes {
    STATIC_ONLY(CSSOMTypes);

public:
    static bool propertyCanTake(CSSPropertyID, const CSSStyleValue&);
};

} // namespace blink

#endif
