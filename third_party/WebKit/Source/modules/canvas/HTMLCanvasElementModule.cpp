// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/HTMLCanvasElementModule.h"

#include "core/dom/DOMNodeIds.h"
#include "core/html/canvas/CanvasContextCreationAttributes.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/offscreencanvas/OffscreenCanvas.h"

namespace blink {

void HTMLCanvasElementModule::getContext(
    HTMLCanvasElement& canvas,
    const String& type,
    const CanvasContextCreationAttributes& attributes,
    ExceptionState& exceptionState,
    RenderingContext& result) {
  if (canvas.surfaceLayerBridge()) {
    // The existence of canvas surfaceLayerBridge indicates that
    // HTMLCanvasElement.transferControlToOffscreen() has been called.
    exceptionState.throwDOMException(InvalidStateError,
                                     "Cannot get context from a canvas that "
                                     "has transferred its control to "
                                     "offscreen.");
    return;
  }

  CanvasRenderingContext* context =
      canvas.getCanvasRenderingContext(type, attributes);
  if (context) {
    context->setCanvasGetContextResult(result);
  }
}

OffscreenCanvas* HTMLCanvasElementModule::transferControlToOffscreen(
    HTMLCanvasElement& canvas,
    ExceptionState& exceptionState) {
  if (canvas.surfaceLayerBridge()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Cannot transfer control from a canvas for more than one time.");
    return nullptr;
  }

  canvas.createLayer();

  return transferControlToOffscreenInternal(canvas, exceptionState);
}

OffscreenCanvas* HTMLCanvasElementModule::transferControlToOffscreenInternal(
    HTMLCanvasElement& canvas,
    ExceptionState& exceptionState) {
  if (canvas.renderingContext()) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Cannot transfer control from a canvas that has a rendering context.");
    return nullptr;
  }
  OffscreenCanvas* offscreenCanvas =
      OffscreenCanvas::create(canvas.width(), canvas.height());

  int canvasId = DOMNodeIds::idForNode(&canvas);
  offscreenCanvas->setPlaceholderCanvasId(canvasId);
  canvas.registerPlaceholder(canvasId);

  CanvasSurfaceLayerBridge* bridge = canvas.surfaceLayerBridge();
  if (bridge) {
    offscreenCanvas->setFrameSinkId(bridge->getFrameSinkId().client_id(),
                                    bridge->getFrameSinkId().sink_id());
  }
  return offscreenCanvas;
}
}
