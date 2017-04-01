// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAlignmentUtils_h
#define CSSPropertyAlignmentUtils_h

#include "wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyAlignmentUtils {
  STATIC_ONLY(CSSPropertyAlignmentUtils);

  static CSSValue* consumeSelfPositionOverflowPosition(CSSParserTokenRange& range);
};

}  // namespace blink

#endif  // CSSPropertyAlignmentUtils_h
