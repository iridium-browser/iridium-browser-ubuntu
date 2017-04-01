/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "platform/scroll/ScrollbarThemeOverlay.h"

#include "platform/PlatformMouseEvent.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/scroll/Scrollbar.h"
#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/Platform.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebThemeEngine.h"
#include "wtf/MathExtras.h"

#include <algorithm>

namespace blink {

ScrollbarThemeOverlay::ScrollbarThemeOverlay(int thumbThickness,
                                             int scrollbarMargin,
                                             HitTestBehavior allowHitTest,
                                             Color color)
    : ScrollbarTheme(),
      m_thumbThickness(thumbThickness),
      m_scrollbarMargin(scrollbarMargin),
      m_allowHitTest(allowHitTest),
      m_color(color),
      m_useSolidColor(true) {}

ScrollbarThemeOverlay::ScrollbarThemeOverlay(int thumbThickness,
                                             int scrollbarMargin,
                                             HitTestBehavior allowHitTest)
    : ScrollbarTheme(),
      m_thumbThickness(thumbThickness),
      m_scrollbarMargin(scrollbarMargin),
      m_allowHitTest(allowHitTest),
      m_useSolidColor(false) {}

bool ScrollbarThemeOverlay::shouldRepaintAllPartsOnInvalidation() const {
  return false;
}

ScrollbarPart ScrollbarThemeOverlay::invalidateOnThumbPositionChange(
    const ScrollbarThemeClient&,
    float oldPosition,
    float newPosition) const {
  return NoPart;
}

ScrollbarPart ScrollbarThemeOverlay::invalidateOnEnabledChange() const {
  return NoPart;
}

int ScrollbarThemeOverlay::scrollbarThickness(
    ScrollbarControlSize controlSize) {
  return m_thumbThickness + m_scrollbarMargin;
}

int ScrollbarThemeOverlay::scrollbarMargin() const {
  return m_scrollbarMargin;
}

bool ScrollbarThemeOverlay::usesOverlayScrollbars() const {
  return true;
}

double ScrollbarThemeOverlay::overlayScrollbarFadeOutDelaySeconds() const {
  // TODO(bokan): Unit tests run without a theme engine. This is normally fine
  // because they expect to use ScrollbarThemeMock which doesn't use a theme
  // engine.  If overlays are turned on though, this class is used even if mock
  // scrollbars are on. We should either provide mock out a web theme engine for
  // unit tests or provide a mock version of this class.
  if (!Platform::current()->themeEngine())
    return 0.0;
  WebThemeEngine::ScrollbarStyle style;
  Platform::current()->themeEngine()->getOverlayScrollbarStyle(&style);
  return style.fadeOutDelaySeconds;
}

double ScrollbarThemeOverlay::overlayScrollbarFadeOutDurationSeconds() const {
  if (!Platform::current()->themeEngine())
    return 0.0;
  WebThemeEngine::ScrollbarStyle style;
  Platform::current()->themeEngine()->getOverlayScrollbarStyle(&style);
  return style.fadeOutDurationSeconds;
}

int ScrollbarThemeOverlay::thumbPosition(const ScrollbarThemeClient& scrollbar,
                                         float scrollPosition) {
  if (!scrollbar.totalSize())
    return 0;

  int trackLen = trackLength(scrollbar);
  float proportion = static_cast<float>(scrollPosition) / scrollbar.totalSize();
  return round(proportion * trackLen);
}

int ScrollbarThemeOverlay::thumbLength(const ScrollbarThemeClient& scrollbar) {
  int trackLen = trackLength(scrollbar);

  if (!scrollbar.totalSize())
    return trackLen;

  float proportion =
      static_cast<float>(scrollbar.visibleSize()) / scrollbar.totalSize();
  int length = round(proportion * trackLen);
  int minLen = std::min(minimumThumbLength(scrollbar), trackLen);
  length = clampTo(length, minLen, trackLen);
  return length;
}

bool ScrollbarThemeOverlay::hasThumb(const ScrollbarThemeClient& scrollbar) {
  return true;
}

IntRect ScrollbarThemeOverlay::backButtonRect(const ScrollbarThemeClient&,
                                              ScrollbarPart,
                                              bool) {
  return IntRect();
}

IntRect ScrollbarThemeOverlay::forwardButtonRect(const ScrollbarThemeClient&,
                                                 ScrollbarPart,
                                                 bool) {
  return IntRect();
}

IntRect ScrollbarThemeOverlay::trackRect(const ScrollbarThemeClient& scrollbar,
                                         bool) {
  IntRect rect = scrollbar.frameRect();
  if (scrollbar.orientation() == HorizontalScrollbar)
    rect.inflateX(-m_scrollbarMargin);
  else
    rect.inflateY(-m_scrollbarMargin);
  return rect;
}

int ScrollbarThemeOverlay::thumbThickness(const ScrollbarThemeClient&) {
  return m_thumbThickness;
}

void ScrollbarThemeOverlay::paintThumb(GraphicsContext& context,
                                       const Scrollbar& scrollbar,
                                       const IntRect& rect) {
  if (DrawingRecorder::useCachedDrawingIfPossible(context, scrollbar,
                                                  DisplayItem::kScrollbarThumb))
    return;

  DrawingRecorder recorder(context, scrollbar, DisplayItem::kScrollbarThumb,
                           rect);

  IntRect thumbRect = rect;
  if (scrollbar.orientation() == HorizontalScrollbar) {
    thumbRect.setHeight(thumbRect.height() - m_scrollbarMargin);
  } else {
    thumbRect.setWidth(thumbRect.width() - m_scrollbarMargin);
    if (scrollbar.isLeftSideVerticalScrollbar())
      thumbRect.setX(thumbRect.x() + m_scrollbarMargin);
  }

  if (m_useSolidColor || !Platform::current()->themeEngine()) {
    context.fillRect(thumbRect, m_color);
    return;
  }

  WebThemeEngine::State state = WebThemeEngine::StateNormal;

  if (!scrollbar.enabled())
    state = WebThemeEngine::StateDisabled;
  else if (scrollbar.pressedPart() == ThumbPart)
    state = WebThemeEngine::StatePressed;
  else if (scrollbar.hoveredPart() == ThumbPart)
    state = WebThemeEngine::StateHover;

  WebCanvas* canvas = context.canvas();

  WebThemeEngine::Part part = WebThemeEngine::PartScrollbarHorizontalThumb;
  if (scrollbar.orientation() == VerticalScrollbar)
    part = WebThemeEngine::PartScrollbarVerticalThumb;

  blink::WebThemeEngine::ExtraParams params;
  params.scrollbarThumb.scrollbarTheme =
      static_cast<WebScrollbarOverlayColorTheme>(
          scrollbar.getScrollbarOverlayColorTheme());

  Platform::current()->themeEngine()->paint(canvas, part, state, WebRect(rect),
                                            &params);
}

ScrollbarPart ScrollbarThemeOverlay::hitTest(
    const ScrollbarThemeClient& scrollbar,
    const IntPoint& position) {
  if (m_allowHitTest == DisallowHitTest)
    return NoPart;

  ScrollbarPart part = ScrollbarTheme::hitTest(scrollbar, position);
  if (part != ThumbPart)
    return NoPart;

  return ThumbPart;
}

ScrollbarThemeOverlay& ScrollbarThemeOverlay::mobileTheme() {
  static ScrollbarThemeOverlay* theme;
  if (!theme) {
    WebThemeEngine::ScrollbarStyle style = {3, 3, 0x80808080};  // default style
    if (Platform::current()->themeEngine()) {
      Platform::current()->themeEngine()->getOverlayScrollbarStyle(&style);
    }
    theme = new ScrollbarThemeOverlay(
        style.thumbThickness, style.scrollbarMargin,
        ScrollbarThemeOverlay::DisallowHitTest, Color(style.color));
  }
  return *theme;
}

}  // namespace blink
