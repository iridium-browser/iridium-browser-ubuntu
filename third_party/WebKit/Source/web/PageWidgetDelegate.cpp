/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/PageWidgetDelegate.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/input/EventHandler.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Page.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/CullRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebInputEvent.h"
#include "web/WebInputEventConversion.h"
#include "wtf/CurrentTime.h"

namespace blink {

void PageWidgetDelegate::animate(Page& page, double monotonicFrameBeginTime) {
  page.autoscrollController().animate(monotonicFrameBeginTime);
  page.animator().serviceScriptedAnimations(monotonicFrameBeginTime);
}

void PageWidgetDelegate::updateAllLifecyclePhases(Page& page,
                                                  LocalFrame& root) {
  page.animator().updateAllLifecyclePhases(root);
}

static void paintInternal(Page& page,
                          WebCanvas* canvas,
                          const WebRect& rect,
                          LocalFrame& root,
                          const GlobalPaintFlags globalPaintFlags) {
  if (rect.isEmpty())
    return;

  IntRect intRect(rect);
  SkPictureBuilder pictureBuilder(intRect);
  {
    GraphicsContext& paintContext = pictureBuilder.context();

    // FIXME: device scale factor settings are layering violations and should
    // not be used within Blink paint code.
    float scaleFactor = page.deviceScaleFactor();
    paintContext.setDeviceScaleFactor(scaleFactor);

    AffineTransform scale;
    scale.scale(scaleFactor);
    TransformRecorder scaleRecorder(paintContext, pictureBuilder, scale);

    IntRect dirtyRect(rect);
    FrameView* view = root.view();
    if (view) {
      ClipRecorder clipRecorder(paintContext, pictureBuilder,
                                DisplayItem::kPageWidgetDelegateClip,
                                dirtyRect);
      view->paint(paintContext, globalPaintFlags, CullRect(dirtyRect));
    } else {
      DrawingRecorder drawingRecorder(
          paintContext, pictureBuilder,
          DisplayItem::kPageWidgetDelegateBackgroundFallback, dirtyRect);
      paintContext.fillRect(dirtyRect, Color::white);
    }
  }
  pictureBuilder.endRecording()->playback(canvas);
}

void PageWidgetDelegate::paint(Page& page,
                               WebCanvas* canvas,
                               const WebRect& rect,
                               LocalFrame& root) {
  paintInternal(page, canvas, rect, root, GlobalPaintNormalPhase);
}

void PageWidgetDelegate::paintIgnoringCompositing(Page& page,
                                                  WebCanvas* canvas,
                                                  const WebRect& rect,
                                                  LocalFrame& root) {
  paintInternal(page, canvas, rect, root, GlobalPaintFlattenCompositingLayers);
}

WebInputEventResult PageWidgetDelegate::handleInputEvent(
    PageWidgetEventHandler& handler,
    const WebCoalescedInputEvent& coalescedEvent,
    LocalFrame* root) {
  const WebInputEvent& event = coalescedEvent.event();
  if (event.modifiers() & WebInputEvent::IsTouchAccessibility &&
      WebInputEvent::isMouseEventType(event.type())) {
    PlatformMouseEventBuilder pme(root->view(),
                                  static_cast<const WebMouseEvent&>(event));

    IntPoint docPoint(root->view()->rootFrameToContents(pme.position()));
    HitTestResult result = root->eventHandler().hitTestResultAtPoint(
        docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
    result.setToShadowHostIfInUserAgentShadowRoot();
    if (result.innerNodeFrame()) {
      Document* document = result.innerNodeFrame()->document();
      if (document) {
        AXObjectCache* cache = document->existingAXObjectCache();
        if (cache)
          cache->onTouchAccessibilityHover(
              result.roundedPointInInnerNodeFrame());
      }
    }
  }

  switch (event.type()) {
    // FIXME: WebKit seems to always return false on mouse events processing
    // methods. For now we'll assume it has processed them (as we are only
    // interested in whether keyboard events are processed).
    // FIXME: Why do we return HandleSuppressed when there is no root or
    // the root is detached?
    case WebInputEvent::MouseMove:
      if (!root || !root->view())
        return WebInputEventResult::HandledSuppressed;
      handler.handleMouseMove(*root, static_cast<const WebMouseEvent&>(event),
                              coalescedEvent.getCoalescedEventsPointers());
      return WebInputEventResult::HandledSystem;
    case WebInputEvent::MouseLeave:
      if (!root || !root->view())
        return WebInputEventResult::HandledSuppressed;
      handler.handleMouseLeave(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::HandledSystem;
    case WebInputEvent::MouseDown:
      if (!root || !root->view())
        return WebInputEventResult::HandledSuppressed;
      handler.handleMouseDown(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::HandledSystem;
    case WebInputEvent::MouseUp:
      if (!root || !root->view())
        return WebInputEventResult::HandledSuppressed;
      handler.handleMouseUp(*root, static_cast<const WebMouseEvent&>(event));
      return WebInputEventResult::HandledSystem;
    case WebInputEvent::MouseWheel:
      if (!root || !root->view())
        return WebInputEventResult::NotHandled;
      return handler.handleMouseWheel(
          *root, static_cast<const WebMouseWheelEvent&>(event));

    case WebInputEvent::RawKeyDown:
    case WebInputEvent::KeyDown:
    case WebInputEvent::KeyUp:
      return handler.handleKeyEvent(
          static_cast<const WebKeyboardEvent&>(event));

    case WebInputEvent::Char:
      return handler.handleCharEvent(
          static_cast<const WebKeyboardEvent&>(event));
    case WebInputEvent::GestureScrollBegin:
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureFlingStart:
    case WebInputEvent::GestureFlingCancel:
    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureTapUnconfirmed:
    case WebInputEvent::GestureTapDown:
    case WebInputEvent::GestureShowPress:
    case WebInputEvent::GestureTapCancel:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GestureTwoFingerTap:
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
      return handler.handleGestureEvent(
          static_cast<const WebGestureEvent&>(event));

    case WebInputEvent::TouchStart:
    case WebInputEvent::TouchMove:
    case WebInputEvent::TouchEnd:
    case WebInputEvent::TouchCancel:
    case WebInputEvent::TouchScrollStarted:
      if (!root || !root->view())
        return WebInputEventResult::NotHandled;
      return handler.handleTouchEvent(
          *root, static_cast<const WebTouchEvent&>(event),
          coalescedEvent.getCoalescedEventsPointers());
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
    case WebInputEvent::GesturePinchUpdate:
      // Touchscreen pinch events are currently not handled in main thread.
      // Once they are, these should be passed to |handleGestureEvent| similar
      // to gesture scroll events.
      return WebInputEventResult::NotHandled;
    default:
      return WebInputEventResult::NotHandled;
  }
}

// ----------------------------------------------------------------
// Default handlers for PageWidgetEventHandler

void PageWidgetEventHandler::handleMouseMove(
    LocalFrame& mainFrame,
    const WebMouseEvent& event,
    const std::vector<const WebInputEvent*>& coalescedEvents) {
  mainFrame.eventHandler().handleMouseMoveEvent(
      PlatformMouseEventBuilder(mainFrame.view(), event),
      createPlatformMouseEventVector(mainFrame.view(), coalescedEvents));
}

void PageWidgetEventHandler::handleMouseLeave(LocalFrame& mainFrame,
                                              const WebMouseEvent& event) {
  mainFrame.eventHandler().handleMouseLeaveEvent(
      PlatformMouseEventBuilder(mainFrame.view(), event));
}

void PageWidgetEventHandler::handleMouseDown(LocalFrame& mainFrame,
                                             const WebMouseEvent& event) {
  mainFrame.eventHandler().handleMousePressEvent(
      PlatformMouseEventBuilder(mainFrame.view(), event));
}

void PageWidgetEventHandler::handleMouseUp(LocalFrame& mainFrame,
                                           const WebMouseEvent& event) {
  mainFrame.eventHandler().handleMouseReleaseEvent(
      PlatformMouseEventBuilder(mainFrame.view(), event));
}

WebInputEventResult PageWidgetEventHandler::handleMouseWheel(
    LocalFrame& mainFrame,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent transformedEvent =
      TransformWebMouseWheelEvent(mainFrame.view(), event);
  return mainFrame.eventHandler().handleWheelEvent(transformedEvent);
}

WebInputEventResult PageWidgetEventHandler::handleTouchEvent(
    LocalFrame& mainFrame,
    const WebTouchEvent& event,
    const std::vector<const WebInputEvent*>& coalescedEvents) {
  return mainFrame.eventHandler().handleTouchEvent(
      PlatformTouchEventBuilder(mainFrame.view(), event),
      createPlatformTouchEventVector(mainFrame.view(), coalescedEvents));
}

}  // namespace blink
