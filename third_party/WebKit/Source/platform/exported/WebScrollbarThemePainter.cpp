/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "public/platform/WebScrollbarThemePainter.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemListContextRecorder.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/WebRect.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace blink {

void WebScrollbarThemePainter::assign(const WebScrollbarThemePainter& painter)
{
    // This is a pointer to a static object, so no ownership transferral.
    m_theme = painter.m_theme;
    m_scrollbar = painter.m_scrollbar;
}

void WebScrollbarThemePainter::paintScrollbarBackground(WebCanvas* canvas, const WebRect& rect)
{
    SkRect clip = SkRect::MakeXYWH(rect.x, rect.y, rect.width, rect.height);
    canvas->clipRect(clip);

    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintScrollbarBackground(&contextRecorder.context(), m_scrollbar);
}

void WebScrollbarThemePainter::paintTrackBackground(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintTrackBackground(&contextRecorder.context(), m_scrollbar, IntRect(rect));
}

void WebScrollbarThemePainter::paintBackTrackPart(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintTrackPiece(&contextRecorder.context(), m_scrollbar, IntRect(rect), BackTrackPart);
}

void WebScrollbarThemePainter::paintForwardTrackPart(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintTrackPiece(&contextRecorder.context(), m_scrollbar, IntRect(rect), ForwardTrackPart);
}

void WebScrollbarThemePainter::paintBackButtonStart(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintButton(&contextRecorder.context(), m_scrollbar, IntRect(rect), BackButtonStartPart);
}

void WebScrollbarThemePainter::paintBackButtonEnd(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintButton(&contextRecorder.context(), m_scrollbar, IntRect(rect), BackButtonEndPart);
}

void WebScrollbarThemePainter::paintForwardButtonStart(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintButton(&contextRecorder.context(), m_scrollbar, IntRect(rect), ForwardButtonStartPart);
}

void WebScrollbarThemePainter::paintForwardButtonEnd(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintButton(&contextRecorder.context(), m_scrollbar, IntRect(rect), ForwardButtonEndPart);
}

void WebScrollbarThemePainter::paintTickmarks(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintTickmarks(&contextRecorder.context(), m_scrollbar, IntRect(rect));
}

void WebScrollbarThemePainter::paintThumb(WebCanvas* canvas, const WebRect& rect)
{
    OwnPtr<GraphicsContext> context = GraphicsContext::deprecatedCreateWithCanvas(canvas);
    DisplayItemListContextRecorder contextRecorder(*context);
    m_theme->paintThumb(&contextRecorder.context(), m_scrollbar, IntRect(rect));
}

WebScrollbarThemePainter::WebScrollbarThemePainter(ScrollbarTheme* theme, Scrollbar* scrollbar)
    : m_theme(theme)
    , m_scrollbar(scrollbar)
{
}

} // namespace blink
