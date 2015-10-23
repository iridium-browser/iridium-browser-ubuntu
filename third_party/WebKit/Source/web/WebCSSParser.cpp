// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/web/WebCSSParser.h"

#include "core/css/parser/CSSParser.h"
#include "public/platform/WebString.h"

namespace blink {

bool WebCSSParser::parseColor(WebColor* color, const WebString& colorString)
{
    return CSSParser::parseColor(*color, colorString, true);
}

} // namespace blink
