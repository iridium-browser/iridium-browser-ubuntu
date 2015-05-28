// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBVIEW_COLOR_OVERLAY_H_
#define CHROME_RENDERER_WEBVIEW_COLOR_OVERLAY_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/public/web/WebPageOverlay.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
class RenderView;
}

// This class draws the given color on a PageOverlay of a WebView.
class WebViewColorOverlay : public blink::WebPageOverlay {
 public:
  WebViewColorOverlay(content::RenderView* render_view, SkColor color);
  virtual ~WebViewColorOverlay();

 private:
  // blink::WebPageOverlay implementation:
  virtual void paintPageOverlay(blink::WebGraphicsContext* context,
                                const blink::WebSize& webViewSize);

  content::RenderView* render_view_;
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(WebViewColorOverlay);
};

#endif  // CHROME_RENDERER_WEBVIEW_COLOR_OVERLAY_H_
