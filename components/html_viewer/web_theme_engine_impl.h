// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_THEME_ENGINE_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_THEME_ENGINE_IMPL_H_

#include "third_party/WebKit/public/platform/WebThemeEngine.h"

namespace html_viewer {

class WebThemeEngineImpl : public blink::WebThemeEngine {
 public:
  // WebThemeEngine methods:
  virtual blink::WebSize getSize(blink::WebThemeEngine::Part);
  virtual void paint(blink::WebCanvas* canvas,
                     blink::WebThemeEngine::Part part,
                     blink::WebThemeEngine::State state,
                     const blink::WebRect& rect,
                     const blink::WebThemeEngine::ExtraParams* extra_params);
  virtual void paintStateTransition(blink::WebCanvas* canvas,
                                    blink::WebThemeEngine::Part part,
                                    blink::WebThemeEngine::State startState,
                                    blink::WebThemeEngine::State endState,
                                    double progress,
                                    const blink::WebRect& rect);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_THEME_ENGINE_IMPL_H_
