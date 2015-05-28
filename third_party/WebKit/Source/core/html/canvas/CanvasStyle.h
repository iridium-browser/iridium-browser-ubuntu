/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef CanvasStyle_h
#define CanvasStyle_h

#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "wtf/Assertions.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

class SkShader;

namespace blink {

    class CanvasGradient;
    class CanvasPattern;
    class GraphicsContext;
    class HTMLCanvasElement;

    class CanvasStyle final : public RefCountedWillBeGarbageCollected<CanvasStyle> {
    public:
        static PassRefPtrWillBeRawPtr<CanvasStyle> createFromRGBA(RGBA32 rgba) { return adoptRefWillBeNoop(new CanvasStyle(rgba)); }
        static PassRefPtrWillBeRawPtr<CanvasStyle> createFromGradient(PassRefPtrWillBeRawPtr<CanvasGradient>);
        static PassRefPtrWillBeRawPtr<CanvasStyle> createFromPattern(PassRefPtrWillBeRawPtr<CanvasPattern>);

        String color() const { ASSERT(m_type == ColorRGBA); return Color(m_rgba).serialized(); }
        CanvasGradient* canvasGradient() const { return m_gradient.get(); }
        CanvasPattern* canvasPattern() const { return m_pattern.get(); }

        void applyFillColor(GraphicsContext*); // Deprecated
        void applyStrokeColor(GraphicsContext*); // Deprecated
        SkShader* shader() const;
        RGBA32 paintColor() const;

        bool isEquivalentRGBA(RGBA32 rgba) const { return m_type == ColorRGBA && m_rgba == rgba; }

        DECLARE_TRACE();

    private:
        enum Type { ColorRGBA, Gradient, ImagePattern };

        CanvasStyle(RGBA32 rgba);
        CanvasStyle(PassRefPtrWillBeRawPtr<CanvasGradient>);
        CanvasStyle(PassRefPtrWillBeRawPtr<CanvasPattern>);

        Type m_type;
        RGBA32 m_rgba;

        RefPtrWillBeMember<CanvasGradient> m_gradient;
        RefPtrWillBeMember<CanvasPattern> m_pattern;
    };

    bool parseColorOrCurrentColor(RGBA32& parsedColor, const String& colorString, HTMLCanvasElement*);

} // namespace blink

#endif
