// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLContextAttributeHelpers.h"

#include "core/frame/Settings.h"

namespace blink {

WebGLContextAttributes toWebGLContextAttributes(
    const CanvasContextCreationAttributes& attrs) {
  WebGLContextAttributes result;
  result.setAlpha(attrs.alpha());
  result.setDepth(attrs.depth());
  result.setStencil(attrs.stencil());
  result.setAntialias(attrs.antialias());
  result.setPremultipliedAlpha(attrs.premultipliedAlpha());
  result.setPreserveDrawingBuffer(attrs.preserveDrawingBuffer());
  result.setFailIfMajorPerformanceCaveat(attrs.failIfMajorPerformanceCaveat());
  return result;
}

Platform::ContextAttributes toPlatformContextAttributes(
    const CanvasContextCreationAttributes& attrs,
    unsigned webGLVersion,
    bool supportOwnOffscreenSurface) {
  Platform::ContextAttributes result;
  result.failIfMajorPerformanceCaveat = attrs.failIfMajorPerformanceCaveat();
  result.webGLVersion = webGLVersion;
  if (supportOwnOffscreenSurface) {
    // Only ask for alpha/depth/stencil/antialias if we may be using the default
    // framebuffer. They are not needed for standard offscreen rendering.
    result.supportAlpha = attrs.alpha();
    result.supportDepth = attrs.depth();
    result.supportStencil = attrs.stencil();
    result.supportAntialias = attrs.antialias();
  }
  return result;
}

}  // namespace blink
