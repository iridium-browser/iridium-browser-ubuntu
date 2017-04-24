/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "core/frame/Screen.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "public/platform/WebScreenInfo.h"

namespace blink {

Screen::Screen(LocalFrame* frame) : DOMWindowClient(frame) {}

int Screen::height() const {
  if (!frame())
    return 0;
  Page* page = frame()->page();
  if (!page)
    return 0;
  if (page->settings().getReportScreenSizeInPhysicalPixelsQuirk()) {
    WebScreenInfo screenInfo = page->chromeClient().screenInfo();
    return lroundf(screenInfo.rect.height * screenInfo.deviceScaleFactor);
  }
  return page->chromeClient().screenInfo().rect.height;
}

int Screen::width() const {
  if (!frame())
    return 0;
  Page* page = frame()->page();
  if (!page)
    return 0;
  if (page->settings().getReportScreenSizeInPhysicalPixelsQuirk()) {
    WebScreenInfo screenInfo = page->chromeClient().screenInfo();
    return lroundf(screenInfo.rect.width * screenInfo.deviceScaleFactor);
  }
  return page->chromeClient().screenInfo().rect.width;
}

unsigned Screen::colorDepth() const {
  if (!frame() || !frame()->page())
    return 0;
  return static_cast<unsigned>(
      frame()->page()->chromeClient().screenInfo().depth);
}

unsigned Screen::pixelDepth() const {
  if (!frame())
    return 0;
  return static_cast<unsigned>(
      frame()->page()->chromeClient().screenInfo().depth);
}

int Screen::availLeft() const {
  if (!frame())
    return 0;
  Page* page = frame()->page();
  if (!page)
    return 0;
  if (page->settings().getReportScreenSizeInPhysicalPixelsQuirk()) {
    WebScreenInfo screenInfo = page->chromeClient().screenInfo();
    return lroundf(screenInfo.availableRect.x * screenInfo.deviceScaleFactor);
  }
  return static_cast<int>(page->chromeClient().screenInfo().availableRect.x);
}

int Screen::availTop() const {
  if (!frame())
    return 0;
  Page* page = frame()->page();
  if (!page)
    return 0;
  if (page->settings().getReportScreenSizeInPhysicalPixelsQuirk()) {
    WebScreenInfo screenInfo = page->chromeClient().screenInfo();
    return lroundf(screenInfo.availableRect.y * screenInfo.deviceScaleFactor);
  }
  return static_cast<int>(page->chromeClient().screenInfo().availableRect.y);
}

int Screen::availHeight() const {
  if (!frame())
    return 0;
  Page* page = frame()->page();
  if (!page)
    return 0;
  if (page->settings().getReportScreenSizeInPhysicalPixelsQuirk()) {
    WebScreenInfo screenInfo = page->chromeClient().screenInfo();
    return lroundf(screenInfo.availableRect.height *
                   screenInfo.deviceScaleFactor);
  }
  return page->chromeClient().screenInfo().availableRect.height;
}

int Screen::availWidth() const {
  if (!frame())
    return 0;
  Page* page = frame()->page();
  if (!page)
    return 0;
  if (page->settings().getReportScreenSizeInPhysicalPixelsQuirk()) {
    WebScreenInfo screenInfo = page->chromeClient().screenInfo();
    return lroundf(screenInfo.availableRect.width *
                   screenInfo.deviceScaleFactor);
  }
  return page->chromeClient().screenInfo().availableRect.width;
}

DEFINE_TRACE(Screen) {
  DOMWindowClient::trace(visitor);
  Supplementable<Screen>::trace(visitor);
}

}  // namespace blink
