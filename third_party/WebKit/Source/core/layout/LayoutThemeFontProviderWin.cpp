/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutThemeFontProvider.h"

#include "core/CSSValueKeywords.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "wtf/text/WTFString.h"

namespace blink {

// Converts |points| to pixels. One point is 1/72 of an inch.
static float pointsToPixels(float points)
{
    float pixelsPerInch = 96.0f * FontCache::deviceScaleFactor();
    static const float pointsPerInch = 72.0f;
    return points / pointsPerInch * pixelsPerInch;
}

// static
void LayoutThemeFontProvider::systemFont(CSSValueID systemFontID, FontStyle& fontStyle, FontWeight& fontWeight, float& fontSize, AtomicString& fontFamily)
{
    fontStyle = FontStyleNormal;
    fontWeight = FontWeightNormal;
    fontSize = s_defaultFontSize;
    fontFamily = defaultGUIFont();

    switch (systemFontID) {
    case CSSValueSmallCaption:
        fontSize = FontCache::smallCaptionFontHeight();
        fontFamily = FontCache::smallCaptionFontFamily();
        break;
    case CSSValueMenu:
        fontSize = FontCache::menuFontHeight();
        fontFamily = FontCache::menuFontFamily();
        break;
    case CSSValueStatusBar:
        fontSize = FontCache::statusFontHeight();
        fontFamily = FontCache::statusFontFamily();
        break;
    case CSSValueWebkitMiniControl:
    case CSSValueWebkitSmallControl:
    case CSSValueWebkitControl:
        // Why 2 points smaller? Because that's what Gecko does.
        fontSize = s_defaultFontSize - pointsToPixels(2);
        fontFamily = defaultGUIFont();
        break;
    default:
        fontSize = s_defaultFontSize;
        fontFamily = defaultGUIFont();
        break;
    }
}

// static
void LayoutThemeFontProvider::setDefaultFontSize(int fontSize)
{
    s_defaultFontSize = static_cast<float>(fontSize);
}

} // namespace blink
