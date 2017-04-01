/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "web/WebInputEventConversion.h"

#include "core/dom/Touch.h"
#include "core/dom/TouchList.h"
#include "core/events/GestureEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/TouchEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/api/LayoutItem.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/KeyboardCodes.h"
#include "platform/Widget.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {
float frameScale(const Widget* widget) {
  float scale = 1;
  if (widget) {
    FrameView* rootView = toFrameView(widget->root());
    if (rootView)
      scale = rootView->inputEventsScaleFactor();
  }
  return scale;
}

FloatPoint frameTranslation(const Widget* widget) {
  float scale = 1;
  FloatSize offset;
  IntPoint visualViewport;
  FloatSize overscrollOffset;
  if (widget) {
    FrameView* rootView = toFrameView(widget->root());
    if (rootView) {
      scale = rootView->inputEventsScaleFactor();
      offset = FloatSize(rootView->inputEventsOffsetForEmulation());
      visualViewport = flooredIntPoint(rootView->page()
                                           ->frameHost()
                                           .visualViewport()
                                           .visibleRect()
                                           .location());
      overscrollOffset =
          rootView->page()->frameHost().chromeClient().elasticOverscroll();
    }
  }
  return FloatPoint(
      -offset.width() / scale + visualViewport.x() + overscrollOffset.width(),
      -offset.height() / scale + visualViewport.y() +
          overscrollOffset.height());
}

float scaleDeltaToWindow(const Widget* widget, float delta) {
  return delta / frameScale(widget);
}

FloatSize scaleSizeToWindow(const Widget* widget, FloatSize size) {
  return FloatSize(scaleDeltaToWindow(widget, size.width()),
                   scaleDeltaToWindow(widget, size.height()));
}

// This method converts from the renderer's coordinate space into Blink's root
// frame coordinate space.  It's somewhat unique in that it takes into account
// DevTools emulation, which applies a scale and offset in the root layer (see
// updateRootLayerTransform in WebViewImpl) as well as the overscroll effect on
// OSX.  This is in addition to the visual viewport "pinch-zoom" transformation
// and is one of the few cases where the visual viewport is not equal to the
// renderer's coordinate-space.
FloatPoint convertHitPointToRootFrame(const Widget* widget,
                                      FloatPoint pointInRendererViewport) {
  float scale = 1;
  IntSize offset;
  IntPoint visualViewport;
  FloatSize overscrollOffset;
  if (widget) {
    FrameView* rootView = toFrameView(widget->root());
    if (rootView) {
      scale = rootView->inputEventsScaleFactor();
      offset = rootView->inputEventsOffsetForEmulation();
      visualViewport = flooredIntPoint(rootView->page()
                                           ->frameHost()
                                           .visualViewport()
                                           .visibleRect()
                                           .location());
      overscrollOffset =
          rootView->page()->frameHost().chromeClient().elasticOverscroll();
    }
  }
  return FloatPoint((pointInRendererViewport.x() - offset.width()) / scale +
                        visualViewport.x() + overscrollOffset.width(),
                    (pointInRendererViewport.y() - offset.height()) / scale +
                        visualViewport.y() + overscrollOffset.height());
}

PlatformEvent::DispatchType toPlatformDispatchType(
    WebInputEvent::DispatchType type) {
  static_assert(PlatformEvent::DispatchType::Blocking ==
                    static_cast<PlatformEvent::DispatchType>(
                        WebInputEvent::DispatchType::Blocking),
                "Dispatch Types not equal");
  static_assert(PlatformEvent::DispatchType::EventNonBlocking ==
                    static_cast<PlatformEvent::DispatchType>(
                        WebInputEvent::DispatchType::EventNonBlocking),
                "Dispatch Types not equal");
  static_assert(
      PlatformEvent::DispatchType::ListenersNonBlockingPassive ==
          static_cast<PlatformEvent::DispatchType>(
              WebInputEvent::DispatchType::ListenersNonBlockingPassive),
      "Dispatch Types not equal");
  static_assert(
      PlatformEvent::DispatchType::ListenersForcedNonBlockingDueToFling ==
          static_cast<PlatformEvent::DispatchType>(
              WebInputEvent::DispatchType::
                  ListenersForcedNonBlockingDueToFling),
      "Dispatch Types not equal");

  return static_cast<PlatformEvent::DispatchType>(type);
}

unsigned toPlatformModifierFrom(WebMouseEvent::Button button) {
  if (button == WebMouseEvent::Button::NoButton)
    return 0;

  unsigned webMouseButtonToPlatformModifier[] = {
      PlatformEvent::LeftButtonDown, PlatformEvent::MiddleButtonDown,
      PlatformEvent::RightButtonDown};

  return webMouseButtonToPlatformModifier[static_cast<int>(button)];
}

}  // namespace

// MakePlatformMouseEvent -----------------------------------------------------

// TODO(mustaq): Add tests for this.
PlatformMouseEventBuilder::PlatformMouseEventBuilder(Widget* widget,
                                                     const WebMouseEvent& e) {
  // FIXME: Widget is always toplevel, unless it's a popup. We may be able
  // to get rid of this once we abstract popups into a WebKit API.
  m_position = widget->convertFromRootFrame(
      flooredIntPoint(convertHitPointToRootFrame(widget, IntPoint(e.x, e.y))));
  m_globalPosition = IntPoint(e.globalX, e.globalY);
  m_movementDelta = IntPoint(scaleDeltaToWindow(widget, e.movementX),
                             scaleDeltaToWindow(widget, e.movementY));
  m_modifiers = e.modifiers();

  m_timestamp = TimeTicks::FromSeconds(e.timeStampSeconds());
  m_clickCount = e.clickCount;

  m_pointerProperties = static_cast<WebPointerProperties>(e);

  switch (e.type()) {
    case WebInputEvent::MouseMove:
    case WebInputEvent::MouseEnter:  // synthesize a move event
    case WebInputEvent::MouseLeave:  // synthesize a move event
      m_type = PlatformEvent::MouseMoved;
      break;

    case WebInputEvent::MouseDown:
      m_type = PlatformEvent::MousePressed;
      break;

    case WebInputEvent::MouseUp:
      m_type = PlatformEvent::MouseReleased;

      // The MouseEvent spec requires that buttons indicates the state
      // immediately after the event takes place. To ensure consistency
      // between platforms here, we explicitly clear the button that is
      // in the process of being released.
      m_modifiers &= ~toPlatformModifierFrom(e.button);
      break;

    default:
      NOTREACHED();
  }
}

WebMouseWheelEvent TransformWebMouseWheelEvent(
    Widget* widget,
    const WebMouseWheelEvent& event) {
  WebMouseWheelEvent result = event;
  result.setFrameScale(frameScale(widget));
  result.setFrameTranslate(frameTranslation(widget));
  return result;
}

WebGestureEvent TransformWebGestureEvent(Widget* widget,
                                         const WebGestureEvent& event) {
  WebGestureEvent result = event;
  result.setFrameScale(frameScale(widget));
  result.setFrameTranslate(frameTranslation(widget));
  return result;
}

inline PlatformEvent::EventType toPlatformTouchEventType(
    const WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::TouchStart:
      return PlatformEvent::TouchStart;
    case WebInputEvent::TouchMove:
      return PlatformEvent::TouchMove;
    case WebInputEvent::TouchEnd:
      return PlatformEvent::TouchEnd;
    case WebInputEvent::TouchCancel:
      return PlatformEvent::TouchCancel;
    case WebInputEvent::TouchScrollStarted:
      return PlatformEvent::TouchScrollStarted;
    default:
      NOTREACHED();
  }
  return PlatformEvent::TouchStart;
}

inline PlatformTouchPoint::TouchState toPlatformTouchPointState(
    const WebTouchPoint::State state) {
  switch (state) {
    case WebTouchPoint::StateReleased:
      return PlatformTouchPoint::TouchReleased;
    case WebTouchPoint::StatePressed:
      return PlatformTouchPoint::TouchPressed;
    case WebTouchPoint::StateMoved:
      return PlatformTouchPoint::TouchMoved;
    case WebTouchPoint::StateStationary:
      return PlatformTouchPoint::TouchStationary;
    case WebTouchPoint::StateCancelled:
      return PlatformTouchPoint::TouchCancelled;
    case WebTouchPoint::StateUndefined:
      NOTREACHED();
  }
  return PlatformTouchPoint::TouchReleased;
}

inline WebTouchPoint::State toWebTouchPointState(const AtomicString& type) {
  if (type == EventTypeNames::touchend)
    return WebTouchPoint::StateReleased;
  if (type == EventTypeNames::touchcancel)
    return WebTouchPoint::StateCancelled;
  if (type == EventTypeNames::touchstart)
    return WebTouchPoint::StatePressed;
  if (type == EventTypeNames::touchmove)
    return WebTouchPoint::StateMoved;
  return WebTouchPoint::StateUndefined;
}

// TODO(mustaq): Add tests for this.
PlatformTouchPointBuilder::PlatformTouchPointBuilder(
    Widget* widget,
    const WebTouchPoint& point) {
  m_pointerProperties = point;
  m_state = toPlatformTouchPointState(point.state);

  FloatPoint floatPos = convertHitPointToRootFrame(widget, point.position);
  IntPoint flooredPoint = flooredIntPoint(floatPos);
  m_pos =
      widget->convertFromRootFrame(flooredPoint) + (floatPos - flooredPoint);

  m_screenPos = FloatPoint(point.screenPosition.x, point.screenPosition.y);
  m_radius = scaleSizeToWindow(widget, FloatSize(point.radiusX, point.radiusY));
  m_rotationAngle = point.rotationAngle;
}

PlatformTouchEventBuilder::PlatformTouchEventBuilder(
    Widget* widget,
    const WebTouchEvent& event) {
  m_type = toPlatformTouchEventType(event.type());
  m_modifiers = event.modifiers();
  m_timestamp = TimeTicks::FromSeconds(event.timeStampSeconds());
  m_causesScrollingIfUncanceled = event.movedBeyondSlopRegion;
  m_touchStartOrFirstTouchMove = event.touchStartOrFirstTouchMove;

  for (unsigned i = 0; i < event.touchesLength; ++i)
    m_touchPoints.push_back(
        PlatformTouchPointBuilder(widget, event.touches[i]));

  m_dispatchType = toPlatformDispatchType(event.dispatchType);
  m_uniqueTouchEventId = event.uniqueTouchEventId;
}

static FloatPoint convertAbsoluteLocationForLayoutObjectFloat(
    const DoublePoint& location,
    const LayoutItem layoutItem) {
  return layoutItem.absoluteToLocal(FloatPoint(location), UseTransforms);
}

static IntPoint convertAbsoluteLocationForLayoutObjectInt(
    const DoublePoint& location,
    const LayoutItem layoutItem) {
  return roundedIntPoint(
      convertAbsoluteLocationForLayoutObjectFloat(location, layoutItem));
}

// FIXME: Change |widget| to const Widget& after RemoteFrames get
// RemoteFrameViews.
static void updateWebMouseEventFromCoreMouseEvent(const MouseEvent& event,
                                                  const Widget* widget,
                                                  const LayoutItem layoutItem,
                                                  WebMouseEvent& webEvent) {
  webEvent.setTimeStampSeconds(event.platformTimeStamp().InSeconds());
  webEvent.setModifiers(event.modifiers());

  FrameView* view = widget ? toFrameView(widget->parent()) : 0;
  // TODO(bokan): If view == nullptr, pointInRootFrame will really be
  // pointInRootContent.
  IntPoint pointInRootFrame(event.absoluteLocation().x(),
                            event.absoluteLocation().y());
  if (view)
    pointInRootFrame = view->contentsToRootFrame(pointInRootFrame);
  webEvent.globalX = event.screenX();
  webEvent.globalY = event.screenY();
  webEvent.windowX = pointInRootFrame.x();
  webEvent.windowY = pointInRootFrame.y();
  IntPoint localPoint = convertAbsoluteLocationForLayoutObjectInt(
      event.absoluteLocation(), layoutItem);
  webEvent.x = localPoint.x();
  webEvent.y = localPoint.y();
}

WebMouseEventBuilder::WebMouseEventBuilder(const Widget* widget,
                                           const LayoutItem layoutItem,
                                           const MouseEvent& event) {
  if (event.type() == EventTypeNames::mousemove)
    m_type = WebInputEvent::MouseMove;
  else if (event.type() == EventTypeNames::mouseout)
    m_type = WebInputEvent::MouseLeave;
  else if (event.type() == EventTypeNames::mouseover)
    m_type = WebInputEvent::MouseEnter;
  else if (event.type() == EventTypeNames::mousedown)
    m_type = WebInputEvent::MouseDown;
  else if (event.type() == EventTypeNames::mouseup)
    m_type = WebInputEvent::MouseUp;
  else if (event.type() == EventTypeNames::contextmenu)
    m_type = WebInputEvent::ContextMenu;
  else
    return;  // Skip all other mouse events.

  m_timeStampSeconds = event.platformTimeStamp().InSeconds();
  m_modifiers = event.modifiers();
  updateWebMouseEventFromCoreMouseEvent(event, widget, layoutItem, *this);

  switch (event.button()) {
    case short(WebPointerProperties::Button::Left):
      button = WebMouseEvent::Button::Left;
      break;
    case short(WebPointerProperties::Button::Middle):
      button = WebMouseEvent::Button::Middle;
      break;
    case short(WebPointerProperties::Button::Right):
      button = WebMouseEvent::Button::Right;
      break;
  }
  if (event.buttonDown()) {
    switch (event.button()) {
      case short(WebPointerProperties::Button::Left):
        m_modifiers |= WebInputEvent::LeftButtonDown;
        break;
      case short(WebPointerProperties::Button::Middle):
        m_modifiers |= WebInputEvent::MiddleButtonDown;
        break;
      case short(WebPointerProperties::Button::Right):
        m_modifiers |= WebInputEvent::RightButtonDown;
        break;
    }
  } else {
    button = WebMouseEvent::Button::NoButton;
  }
  movementX = event.movementX();
  movementY = event.movementY();
  clickCount = event.detail();

  pointerType = WebPointerProperties::PointerType::Mouse;
  if (event.mouseEvent())
    pointerType = event.mouseEvent()->pointerProperties().pointerType;
}

// Generate a synthetic WebMouseEvent given a TouchEvent (eg. for emulating a
// mouse with touch input for plugins that don't support touch input).
WebMouseEventBuilder::WebMouseEventBuilder(const Widget* widget,
                                           const LayoutItem layoutItem,
                                           const TouchEvent& event) {
  if (!event.touches())
    return;
  if (event.touches()->length() != 1) {
    if (event.touches()->length() || event.type() != EventTypeNames::touchend ||
        !event.changedTouches() || event.changedTouches()->length() != 1)
      return;
  }

  const Touch* touch = event.touches()->length() == 1
                           ? event.touches()->item(0)
                           : event.changedTouches()->item(0);
  if (touch->identifier())
    return;

  if (event.type() == EventTypeNames::touchstart)
    m_type = MouseDown;
  else if (event.type() == EventTypeNames::touchmove)
    m_type = MouseMove;
  else if (event.type() == EventTypeNames::touchend)
    m_type = MouseUp;
  else
    return;

  m_timeStampSeconds = event.platformTimeStamp().InSeconds();
  m_modifiers = event.modifiers();

  // The mouse event co-ordinates should be generated from the co-ordinates of
  // the touch point.
  FrameView* view = toFrameView(widget->parent());
  // FIXME: if view == nullptr, pointInRootFrame will really be
  // pointInRootContent.
  IntPoint pointInRootFrame = roundedIntPoint(touch->absoluteLocation());
  if (view)
    pointInRootFrame = view->contentsToRootFrame(pointInRootFrame);
  IntPoint screenPoint = roundedIntPoint(touch->screenLocation());
  globalX = screenPoint.x();
  globalY = screenPoint.y();
  windowX = pointInRootFrame.x();
  windowY = pointInRootFrame.y();

  button = WebMouseEvent::Button::Left;
  m_modifiers |= WebInputEvent::LeftButtonDown;
  clickCount = (m_type == MouseDown || m_type == MouseUp);

  IntPoint localPoint = convertAbsoluteLocationForLayoutObjectInt(
      DoublePoint(touch->absoluteLocation()), layoutItem);
  x = localPoint.x();
  y = localPoint.y();

  pointerType = WebPointerProperties::PointerType::Touch;
}

WebKeyboardEventBuilder::WebKeyboardEventBuilder(const KeyboardEvent& event) {
  if (const WebKeyboardEvent* webEvent = event.keyEvent()) {
    *static_cast<WebKeyboardEvent*>(this) = *webEvent;

    // TODO(dtapuska): DOM KeyboardEvents converted back to WebInputEvents
    // drop the Raw behaviour. Figure out if this is actually really needed.
    if (m_type == RawKeyDown)
      m_type = KeyDown;
    return;
  }

  if (event.type() == EventTypeNames::keydown)
    m_type = KeyDown;
  else if (event.type() == EventTypeNames::keyup)
    m_type = WebInputEvent::KeyUp;
  else if (event.type() == EventTypeNames::keypress)
    m_type = WebInputEvent::Char;
  else
    return;  // Skip all other keyboard events.

  m_modifiers = event.modifiers();
  m_timeStampSeconds = event.platformTimeStamp().InSeconds();
  windowsKeyCode = event.keyCode();
}

static WebTouchPoint toWebTouchPoint(const Touch* touch,
                                     const LayoutItem layoutItem,
                                     WebTouchPoint::State state,
                                     WebPointerProperties::PointerType type) {
  WebTouchPoint point;
  point.pointerType = type;
  point.id = touch->identifier();
  point.screenPosition = touch->screenLocation();
  point.position = convertAbsoluteLocationForLayoutObjectFloat(
      DoublePoint(touch->absoluteLocation()), layoutItem);
  point.radiusX = touch->radiusX();
  point.radiusY = touch->radiusY();
  point.rotationAngle = touch->rotationAngle();
  point.force = touch->force();
  point.state = state;
  return point;
}

static unsigned indexOfTouchPointWithId(const WebTouchPoint* touchPoints,
                                        unsigned touchPointsLength,
                                        unsigned id) {
  for (unsigned i = 0; i < touchPointsLength; ++i) {
    if (touchPoints[i].id == static_cast<int>(id))
      return i;
  }
  return std::numeric_limits<unsigned>::max();
}

static void addTouchPointsUpdateStateIfNecessary(
    WebTouchPoint::State state,
    TouchList* touches,
    WebTouchPoint* touchPoints,
    unsigned* touchPointsLength,
    const LayoutItem layoutItem,
    WebPointerProperties::PointerType pointerType) {
  unsigned initialTouchPointsLength = *touchPointsLength;
  for (unsigned i = 0; i < touches->length(); ++i) {
    const unsigned pointIndex = *touchPointsLength;
    if (pointIndex >= static_cast<unsigned>(WebTouchEvent::kTouchesLengthCap))
      return;

    const Touch* touch = touches->item(i);
    unsigned existingPointIndex = indexOfTouchPointWithId(
        touchPoints, initialTouchPointsLength, touch->identifier());
    if (existingPointIndex != std::numeric_limits<unsigned>::max()) {
      touchPoints[existingPointIndex].state = state;
    } else {
      touchPoints[pointIndex] =
          toWebTouchPoint(touch, layoutItem, state, pointerType);
      ++(*touchPointsLength);
    }
  }
}

WebTouchEventBuilder::WebTouchEventBuilder(const LayoutItem layoutItem,
                                           const TouchEvent& event) {
  if (event.type() == EventTypeNames::touchstart)
    m_type = TouchStart;
  else if (event.type() == EventTypeNames::touchmove)
    m_type = TouchMove;
  else if (event.type() == EventTypeNames::touchend)
    m_type = TouchEnd;
  else if (event.type() == EventTypeNames::touchcancel)
    m_type = TouchCancel;
  else {
    NOTREACHED();
    m_type = Undefined;
    return;
  }

  m_timeStampSeconds = event.platformTimeStamp().InSeconds();
  m_modifiers = event.modifiers();
  dispatchType = event.cancelable() ? WebInputEvent::Blocking
                                    : WebInputEvent::EventNonBlocking;
  movedBeyondSlopRegion = event.causesScrollingIfUncanceled();

  // Currently touches[] is empty, add stationary points as-is.
  for (unsigned i = 0;
       i < event.touches()->length() &&
       i < static_cast<unsigned>(WebTouchEvent::kTouchesLengthCap);
       ++i) {
    touches[i] =
        toWebTouchPoint(event.touches()->item(i), layoutItem,
                        WebTouchPoint::StateStationary, event.pointerType());
    ++touchesLength;
  }
  // If any existing points are also in the change list, we should update
  // their state, otherwise just add the new points.
  addTouchPointsUpdateStateIfNecessary(
      toWebTouchPointState(event.type()), event.changedTouches(), touches,
      &touchesLength, layoutItem, event.pointerType());
}

Vector<PlatformMouseEvent> createPlatformMouseEventVector(
    Widget* widget,
    const std::vector<const WebInputEvent*>& coalescedEvents) {
  Vector<PlatformMouseEvent> result;
  for (const auto& event : coalescedEvents) {
    DCHECK(WebInputEvent::isMouseEventType(event->type()));
    result.push_back(PlatformMouseEventBuilder(
        widget, static_cast<const WebMouseEvent&>(*event)));
  }
  return result;
}

Vector<PlatformTouchEvent> createPlatformTouchEventVector(
    Widget* widget,
    const std::vector<const WebInputEvent*>& coalescedEvents) {
  Vector<PlatformTouchEvent> result;
  for (const auto& event : coalescedEvents) {
    DCHECK(WebInputEvent::isTouchEventType(event->type()));
    result.push_back(PlatformTouchEventBuilder(
        widget, static_cast<const WebTouchEvent&>(*event)));
  }
  return result;
}

}  // namespace blink
