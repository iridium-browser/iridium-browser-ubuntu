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
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/Page.h"
#include "platform/geometry/IntSize.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/web/WebFrame.h"
#include "public/web/WebSettings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

KeyboardEvent* createKeyboardEventWithLocation(
    KeyboardEvent::KeyLocationCode location) {
  KeyboardEventInit keyEventInit;
  keyEventInit.setBubbles(true);
  keyEventInit.setCancelable(true);
  keyEventInit.setLocation(location);
  return new KeyboardEvent("keydown", keyEventInit);
}

int getModifiersForKeyLocationCode(KeyboardEvent::KeyLocationCode location) {
  KeyboardEvent* event = createKeyboardEventWithLocation(location);
  WebKeyboardEventBuilder convertedEvent(*event);
  return convertedEvent.modifiers();
}

TEST(WebInputEventConversionTest, WebKeyboardEventBuilder) {
  // Test key location conversion.
  int modifiers =
      getModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationStandard);
  EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad ||
               modifiers & WebInputEvent::IsLeft ||
               modifiers & WebInputEvent::IsRight);

  modifiers =
      getModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationLeft);
  EXPECT_TRUE(modifiers & WebInputEvent::IsLeft);
  EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad ||
               modifiers & WebInputEvent::IsRight);

  modifiers =
      getModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationRight);
  EXPECT_TRUE(modifiers & WebInputEvent::IsRight);
  EXPECT_FALSE(modifiers & WebInputEvent::IsKeyPad ||
               modifiers & WebInputEvent::IsLeft);

  modifiers =
      getModifiersForKeyLocationCode(KeyboardEvent::kDomKeyLocationNumpad);
  EXPECT_TRUE(modifiers & WebInputEvent::IsKeyPad);
  EXPECT_FALSE(modifiers & WebInputEvent::IsLeft ||
               modifiers & WebInputEvent::IsRight);
}

TEST(WebInputEventConversionTest, WebMouseEventBuilder) {
  TouchEvent* event = TouchEvent::create();
  WebMouseEventBuilder mouse(0, 0, *event);
  EXPECT_EQ(WebInputEvent::Undefined, mouse.type());
}

TEST(WebInputEventConversionTest, WebTouchEventBuilder) {
  const std::string baseURL("http://www.test0.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  Document* document =
      toLocalFrame(webViewImpl->page()->mainFrame())->document();
  LocalDOMWindow* domWindow = document->domWindow();
  LayoutViewItem documentLayoutView = document->layoutViewItem();

  WebTouchPoint p0, p1;
  p0.id = 1;
  p1.id = 2;
  p0.screenPosition = WebFloatPoint(100.f, 50.f);
  p1.screenPosition = WebFloatPoint(150.f, 25.f);
  p0.position = WebFloatPoint(10.f, 10.f);
  p1.position = WebFloatPoint(5.f, 5.f);
  p0.radiusX = p1.radiusY = 10.f;
  p0.radiusY = p1.radiusX = 5.f;
  p0.rotationAngle = p1.rotationAngle = 1.f;
  p0.force = p1.force = 25.f;

  Touch* touch0 = Touch::create(toLocalFrame(webViewImpl->page()->mainFrame()),
                                document, p0.id, p0.screenPosition, p0.position,
                                FloatSize(p0.radiusX, p0.radiusY),
                                p0.rotationAngle, p0.force, String());
  Touch* touch1 = Touch::create(toLocalFrame(webViewImpl->page()->mainFrame()),
                                document, p1.id, p1.screenPosition, p1.position,
                                FloatSize(p1.radiusX, p1.radiusY),
                                p1.rotationAngle, p1.force, String());

  // Test touchstart.
  {
    TouchList* touchList = TouchList::create();
    touchList->append(touch0);
    TouchEvent* touchEvent = TouchEvent::create(
        touchList, touchList, touchList, EventTypeNames::touchstart, domWindow,
        PlatformEvent::NoModifiers, false, false, true, TimeTicks(),
        TouchActionAuto, WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(1u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchStart, webTouchBuilder.type());
    EXPECT_EQ(WebTouchPoint::StatePressed, webTouchBuilder.touches[0].state);
    EXPECT_FLOAT_EQ(p0.screenPosition.x,
                    webTouchBuilder.touches[0].screenPosition.x);
    EXPECT_FLOAT_EQ(p0.screenPosition.y,
                    webTouchBuilder.touches[0].screenPosition.y);
    EXPECT_FLOAT_EQ(p0.position.x, webTouchBuilder.touches[0].position.x);
    EXPECT_FLOAT_EQ(p0.position.y, webTouchBuilder.touches[0].position.y);
    EXPECT_FLOAT_EQ(p0.radiusX, webTouchBuilder.touches[0].radiusX);
    EXPECT_FLOAT_EQ(p0.radiusY, webTouchBuilder.touches[0].radiusY);
    EXPECT_FLOAT_EQ(p0.rotationAngle, webTouchBuilder.touches[0].rotationAngle);
    EXPECT_FLOAT_EQ(p0.force, webTouchBuilder.touches[0].force);
    EXPECT_EQ(WebPointerProperties::PointerType::Touch,
              webTouchBuilder.touches[0].pointerType);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test cancelable touchstart.
  {
    TouchList* touchList = TouchList::create();
    touchList->append(touch0);
    TouchEvent* touchEvent = TouchEvent::create(
        touchList, touchList, touchList, EventTypeNames::touchstart, domWindow,
        PlatformEvent::NoModifiers, true, false, true, TimeTicks(),
        TouchActionAuto, WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    EXPECT_EQ(WebInputEvent::Blocking, webTouchBuilder.dispatchType);
  }

  // Test touchmove.
  {
    TouchList* activeTouchList = TouchList::create();
    TouchList* movedTouchList = TouchList::create();
    activeTouchList->append(touch0);
    activeTouchList->append(touch1);
    movedTouchList->append(touch0);
    TouchEvent* touchEvent = TouchEvent::create(
        activeTouchList, activeTouchList, movedTouchList,
        EventTypeNames::touchmove, domWindow, PlatformEvent::NoModifiers, false,
        false, true, TimeTicks(), TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(2u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchMove, webTouchBuilder.type());
    EXPECT_EQ(WebTouchPoint::StateMoved, webTouchBuilder.touches[0].state);
    EXPECT_EQ(WebTouchPoint::StateStationary, webTouchBuilder.touches[1].state);
    EXPECT_EQ(p0.id, webTouchBuilder.touches[0].id);
    EXPECT_EQ(p1.id, webTouchBuilder.touches[1].id);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test touchmove, different point yields same ordering.
  {
    TouchList* activeTouchList = TouchList::create();
    TouchList* movedTouchList = TouchList::create();
    activeTouchList->append(touch0);
    activeTouchList->append(touch1);
    movedTouchList->append(touch1);
    TouchEvent* touchEvent = TouchEvent::create(
        activeTouchList, activeTouchList, movedTouchList,
        EventTypeNames::touchmove, domWindow, PlatformEvent::NoModifiers, false,
        false, true, TimeTicks(), TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(2u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchMove, webTouchBuilder.type());
    EXPECT_EQ(WebTouchPoint::StateStationary, webTouchBuilder.touches[0].state);
    EXPECT_EQ(WebTouchPoint::StateMoved, webTouchBuilder.touches[1].state);
    EXPECT_EQ(p0.id, webTouchBuilder.touches[0].id);
    EXPECT_EQ(p1.id, webTouchBuilder.touches[1].id);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test touchend.
  {
    TouchList* activeTouchList = TouchList::create();
    TouchList* releasedTouchList = TouchList::create();
    activeTouchList->append(touch0);
    releasedTouchList->append(touch1);
    TouchEvent* touchEvent = TouchEvent::create(
        activeTouchList, activeTouchList, releasedTouchList,
        EventTypeNames::touchend, domWindow, PlatformEvent::NoModifiers, false,
        false, false, TimeTicks(), TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(2u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchEnd, webTouchBuilder.type());
    EXPECT_EQ(WebTouchPoint::StateStationary, webTouchBuilder.touches[0].state);
    EXPECT_EQ(WebTouchPoint::StateReleased, webTouchBuilder.touches[1].state);
    EXPECT_EQ(p0.id, webTouchBuilder.touches[0].id);
    EXPECT_EQ(p1.id, webTouchBuilder.touches[1].id);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test touchcancel.
  {
    TouchList* activeTouchList = TouchList::create();
    TouchList* cancelledTouchList = TouchList::create();
    cancelledTouchList->append(touch0);
    cancelledTouchList->append(touch1);
    TouchEvent* touchEvent = TouchEvent::create(
        activeTouchList, activeTouchList, cancelledTouchList,
        EventTypeNames::touchcancel, domWindow, PlatformEvent::NoModifiers,
        false, false, false, TimeTicks(), TouchActionAuto,
        WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(2u, webTouchBuilder.touchesLength);
    EXPECT_EQ(WebInputEvent::TouchCancel, webTouchBuilder.type());
    EXPECT_EQ(WebTouchPoint::StateCancelled, webTouchBuilder.touches[0].state);
    EXPECT_EQ(WebTouchPoint::StateCancelled, webTouchBuilder.touches[1].state);
    EXPECT_EQ(p0.id, webTouchBuilder.touches[0].id);
    EXPECT_EQ(p1.id, webTouchBuilder.touches[1].id);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }

  // Test max point limit.
  {
    TouchList* touchList = TouchList::create();
    TouchList* changedTouchList = TouchList::create();
    for (int i = 0; i <= static_cast<int>(WebTouchEvent::kTouchesLengthCap) * 2;
         ++i) {
      Touch* touch = Touch::create(
          toLocalFrame(webViewImpl->page()->mainFrame()), document, i,
          p0.screenPosition, p0.position, FloatSize(p0.radiusX, p0.radiusY),
          p0.rotationAngle, p0.force, String());
      touchList->append(touch);
      changedTouchList->append(touch);
    }
    TouchEvent* touchEvent = TouchEvent::create(
        touchList, touchList, touchList, EventTypeNames::touchstart, domWindow,
        PlatformEvent::NoModifiers, false, false, true, TimeTicks(),
        TouchActionAuto, WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(static_cast<unsigned>(WebTouchEvent::kTouchesLengthCap),
              webTouchBuilder.touchesLength);
  }
}

TEST(WebInputEventConversionTest, InputEventsScaling) {
  const std::string baseURL("http://www.test1.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  webViewImpl->settings()->setViewportEnabled(true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  webViewImpl->setPageScaleFactor(2);

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();
  Document* document =
      toLocalFrame(webViewImpl->page()->mainFrame())->document();
  LocalDOMWindow* domWindow = document->domWindow();
  LayoutViewItem documentLayoutView = document->layoutViewItem();

  {
    WebMouseEvent webMouseEvent(WebInputEvent::MouseMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webMouseEvent.x = 10;
    webMouseEvent.y = 10;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 10;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 10;
    webMouseEvent.movementX = 10;
    webMouseEvent.movementY = 10;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(5, platformMouseBuilder.position().x());
    EXPECT_EQ(5, platformMouseBuilder.position().y());
    EXPECT_EQ(10, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(10, platformMouseBuilder.globalPosition().y());
    EXPECT_EQ(5, platformMouseBuilder.movementDelta().x());
    EXPECT_EQ(5, platformMouseBuilder.movementDelta().y());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureScrollUpdate,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 10;
    webGestureEvent.y = 12;
    webGestureEvent.globalX = 20;
    webGestureEvent.globalY = 22;
    webGestureEvent.data.scrollUpdate.deltaX = 30;
    webGestureEvent.data.scrollUpdate.deltaY = 32;
    webGestureEvent.data.scrollUpdate.velocityX = 40;
    webGestureEvent.data.scrollUpdate.velocityY = 42;
    webGestureEvent.data.scrollUpdate.inertialPhase =
        WebGestureEvent::MomentumPhase;
    webGestureEvent.data.scrollUpdate.preventPropagation = true;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntPoint position =
        flooredIntPoint(scaledGestureEvent.positionInRootFrame());
    EXPECT_EQ(5, position.x());
    EXPECT_EQ(6, position.y());
    EXPECT_EQ(20, scaledGestureEvent.globalX);
    EXPECT_EQ(22, scaledGestureEvent.globalY);
    EXPECT_EQ(15, scaledGestureEvent.deltaXInRootFrame());
    EXPECT_EQ(16, scaledGestureEvent.deltaYInRootFrame());
    // TODO: The velocity values may need to be scaled to page scale in
    // order to remain consist with delta values.
    EXPECT_EQ(40, scaledGestureEvent.velocityX());
    EXPECT_EQ(42, scaledGestureEvent.velocityY());
    EXPECT_EQ(WebGestureEvent::MomentumPhase,
              scaledGestureEvent.inertialPhase());
    EXPECT_TRUE(scaledGestureEvent.preventPropagation());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureScrollEnd,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 10;
    webGestureEvent.y = 12;
    webGestureEvent.globalX = 20;
    webGestureEvent.globalY = 22;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntPoint position =
        flooredIntPoint(scaledGestureEvent.positionInRootFrame());
    EXPECT_EQ(5, position.x());
    EXPECT_EQ(6, position.y());
    EXPECT_EQ(20, scaledGestureEvent.globalX);
    EXPECT_EQ(22, scaledGestureEvent.globalY);
    EXPECT_EQ(WebGestureEvent::UnknownMomentumPhase,
              scaledGestureEvent.inertialPhase());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTap,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tap.width = 10;
    webGestureEvent.data.tap.height = 10;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(5, area.width());
    EXPECT_EQ(5, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTapUnconfirmed,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tap.width = 10;
    webGestureEvent.data.tap.height = 10;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(5, area.width());
    EXPECT_EQ(5, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTapDown,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tapDown.width = 10;
    webGestureEvent.data.tapDown.height = 10;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(5, area.width());
    EXPECT_EQ(5, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureShowPress,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.showPress.width = 10;
    webGestureEvent.data.showPress.height = 10;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(5, area.width());
    EXPECT_EQ(5, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureLongPress,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.longPress.width = 10;
    webGestureEvent.data.longPress.height = 10;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(5, area.width());
    EXPECT_EQ(5, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTwoFingerTap,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.twoFingerTap.firstFingerWidth = 10;
    webGestureEvent.data.twoFingerTap.firstFingerHeight = 10;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(5, area.width());
    EXPECT_EQ(5, area.height());
  }

  {
    WebTouchEvent webTouchEvent(WebInputEvent::TouchMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webTouchEvent.touchesLength = 1;
    webTouchEvent.touches[0].state = WebTouchPoint::StateMoved;
    webTouchEvent.touches[0].screenPosition.x = 10.6f;
    webTouchEvent.touches[0].screenPosition.y = 10.4f;
    webTouchEvent.touches[0].position.x = 10.6f;
    webTouchEvent.touches[0].position.y = 10.4f;
    webTouchEvent.touches[0].radiusX = 10.6f;
    webTouchEvent.touches[0].radiusY = 10.4f;

    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].screenPosition.x);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].screenPosition.y);
    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].position.x);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].position.y);
    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].radiusX);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].radiusY);

    PlatformTouchEventBuilder platformTouchBuilder(view, webTouchEvent);
    EXPECT_FLOAT_EQ(10.6f,
                    platformTouchBuilder.touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(10.4f,
                    platformTouchBuilder.touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(5.3f, platformTouchBuilder.touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(5.2f, platformTouchBuilder.touchPoints()[0].pos().y());
    EXPECT_FLOAT_EQ(5.3f,
                    platformTouchBuilder.touchPoints()[0].radius().width());
    EXPECT_FLOAT_EQ(5.2f,
                    platformTouchBuilder.touchPoints()[0].radius().height());
  }

  // Reverse builders should *not* go back to physical pixels, as they are used
  // for plugins which expect CSS pixel coordinates.
  {
    PlatformMouseEvent platformMouseEvent(
        IntPoint(10, 10), IntPoint(10, 10), WebPointerProperties::Button::Left,
        PlatformEvent::MouseMoved, 1, PlatformEvent::NoModifiers,
        PlatformMouseEvent::RealOrIndistinguishable, TimeTicks());
    MouseEvent* mouseEvent = MouseEvent::create(
        EventTypeNames::mousemove, domWindow, platformMouseEvent, 0, document);
    WebMouseEventBuilder webMouseBuilder(view, documentLayoutView, *mouseEvent);

    EXPECT_EQ(10, webMouseBuilder.x);
    EXPECT_EQ(10, webMouseBuilder.y);
    EXPECT_EQ(10, webMouseBuilder.globalX);
    EXPECT_EQ(10, webMouseBuilder.globalY);
    EXPECT_EQ(10, webMouseBuilder.windowX);
    EXPECT_EQ(10, webMouseBuilder.windowY);
  }

  {
    PlatformMouseEvent platformMouseEvent(
        IntPoint(10, 10), IntPoint(10, 10),
        WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved, 1,
        PlatformEvent::NoModifiers, PlatformMouseEvent::RealOrIndistinguishable,
        TimeTicks());
    MouseEvent* mouseEvent = MouseEvent::create(
        EventTypeNames::mousemove, domWindow, platformMouseEvent, 0, document);
    WebMouseEventBuilder webMouseBuilder(view, documentLayoutView, *mouseEvent);
    EXPECT_EQ(WebMouseEvent::Button::NoButton, webMouseBuilder.button);
  }

  {
    Touch* touch =
        Touch::create(toLocalFrame(webViewImpl->page()->mainFrame()), document,
                      0, FloatPoint(10, 9.5), FloatPoint(3.5, 2),
                      FloatSize(4, 4.5), 0, 0, String());
    TouchList* touchList = TouchList::create();
    touchList->append(touch);
    TouchEvent* touchEvent = TouchEvent::create(
        touchList, touchList, touchList, EventTypeNames::touchmove, domWindow,
        PlatformEvent::NoModifiers, false, false, true, TimeTicks(),
        TouchActionAuto, WebPointerProperties::PointerType::Touch);

    WebTouchEventBuilder webTouchBuilder(documentLayoutView, *touchEvent);
    ASSERT_EQ(1u, webTouchBuilder.touchesLength);
    EXPECT_EQ(10, webTouchBuilder.touches[0].screenPosition.x);
    EXPECT_FLOAT_EQ(9.5, webTouchBuilder.touches[0].screenPosition.y);
    EXPECT_FLOAT_EQ(3.5, webTouchBuilder.touches[0].position.x);
    EXPECT_FLOAT_EQ(2, webTouchBuilder.touches[0].position.y);
    EXPECT_FLOAT_EQ(4, webTouchBuilder.touches[0].radiusX);
    EXPECT_FLOAT_EQ(4.5, webTouchBuilder.touches[0].radiusY);
    EXPECT_EQ(WebInputEvent::EventNonBlocking, webTouchBuilder.dispatchType);
  }
}

TEST(WebInputEventConversionTest, InputEventsTransform) {
  const std::string baseURL("http://www.test2.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  webViewImpl->settings()->setViewportEnabled(true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  webViewImpl->setPageScaleFactor(2);
  webViewImpl->mainFrameImpl()->setInputEventsTransformForEmulation(
      IntSize(10, 20), 1.5);

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  {
    WebMouseEvent webMouseEvent(WebInputEvent::MouseMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webMouseEvent.x = 100;
    webMouseEvent.y = 110;
    webMouseEvent.windowX = 100;
    webMouseEvent.windowY = 110;
    webMouseEvent.globalX = 100;
    webMouseEvent.globalY = 110;
    webMouseEvent.movementX = 60;
    webMouseEvent.movementY = 60;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(30, platformMouseBuilder.position().x());
    EXPECT_EQ(30, platformMouseBuilder.position().y());
    EXPECT_EQ(100, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(110, platformMouseBuilder.globalPosition().y());
    EXPECT_EQ(20, platformMouseBuilder.movementDelta().x());
    EXPECT_EQ(20, platformMouseBuilder.movementDelta().y());
  }

  {
    WebMouseEvent webMouseEvent1(WebInputEvent::MouseMove,
                                 WebInputEvent::NoModifiers,
                                 WebInputEvent::TimeStampForTesting);
    webMouseEvent1.x = 100;
    webMouseEvent1.y = 110;
    webMouseEvent1.windowX = 100;
    webMouseEvent1.windowY = 110;
    webMouseEvent1.globalX = 100;
    webMouseEvent1.globalY = 110;
    webMouseEvent1.movementX = 60;
    webMouseEvent1.movementY = 60;

    WebMouseEvent webMouseEvent2 = webMouseEvent1;
    webMouseEvent2.y = 140;
    webMouseEvent2.windowY = 140;
    webMouseEvent2.globalY = 140;
    webMouseEvent2.movementY = 30;

    std::vector<const WebInputEvent*> events;
    events.push_back(&webMouseEvent1);
    events.push_back(&webMouseEvent2);

    Vector<PlatformMouseEvent> coalescedevents =
        createPlatformMouseEventVector(view, events);
    EXPECT_EQ(events.size(), coalescedevents.size());

    EXPECT_EQ(30, coalescedevents[0].position().x());
    EXPECT_EQ(30, coalescedevents[0].position().y());
    EXPECT_EQ(100, coalescedevents[0].globalPosition().x());
    EXPECT_EQ(110, coalescedevents[0].globalPosition().y());
    EXPECT_EQ(20, coalescedevents[0].movementDelta().x());
    EXPECT_EQ(20, coalescedevents[0].movementDelta().y());

    EXPECT_EQ(30, coalescedevents[1].position().x());
    EXPECT_EQ(40, coalescedevents[1].position().y());
    EXPECT_EQ(100, coalescedevents[1].globalPosition().x());
    EXPECT_EQ(140, coalescedevents[1].globalPosition().y());
    EXPECT_EQ(20, coalescedevents[1].movementDelta().x());
    EXPECT_EQ(10, coalescedevents[1].movementDelta().y());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureScrollUpdate,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 100;
    webGestureEvent.y = 110;
    webGestureEvent.globalX = 100;
    webGestureEvent.globalY = 110;
    webGestureEvent.data.scrollUpdate.deltaX = 60;
    webGestureEvent.data.scrollUpdate.deltaY = 60;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    FloatPoint position = scaledGestureEvent.positionInRootFrame();

    EXPECT_FLOAT_EQ(30, position.x());
    EXPECT_FLOAT_EQ(30, position.y());
    EXPECT_EQ(100, scaledGestureEvent.globalX);
    EXPECT_EQ(110, scaledGestureEvent.globalY);
    EXPECT_EQ(20, scaledGestureEvent.deltaXInRootFrame());
    EXPECT_EQ(20, scaledGestureEvent.deltaYInRootFrame());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTap,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tap.width = 30;
    webGestureEvent.data.tap.height = 30;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(10, area.width());
    EXPECT_EQ(10, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTapUnconfirmed,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tap.width = 30;
    webGestureEvent.data.tap.height = 30;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(10, area.width());
    EXPECT_EQ(10, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTapDown,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.tapDown.width = 30;
    webGestureEvent.data.tapDown.height = 30;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(10, area.width());
    EXPECT_EQ(10, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureShowPress,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.showPress.width = 30;
    webGestureEvent.data.showPress.height = 30;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(10, area.width());
    EXPECT_EQ(10, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureLongPress,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.longPress.width = 30;
    webGestureEvent.data.longPress.height = 30;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(10, area.width());
    EXPECT_EQ(10, area.height());
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTwoFingerTap,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.data.twoFingerTap.firstFingerWidth = 30;
    webGestureEvent.data.twoFingerTap.firstFingerHeight = 30;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntSize area = flooredIntSize(scaledGestureEvent.tapAreaInRootFrame());
    EXPECT_EQ(10, area.width());
    EXPECT_EQ(10, area.height());
  }

  {
    WebTouchEvent webTouchEvent(WebInputEvent::TouchMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webTouchEvent.touchesLength = 1;
    webTouchEvent.touches[0].state = WebTouchPoint::StateMoved;
    webTouchEvent.touches[0].screenPosition.x = 100;
    webTouchEvent.touches[0].screenPosition.y = 110;
    webTouchEvent.touches[0].position.x = 100;
    webTouchEvent.touches[0].position.y = 110;
    webTouchEvent.touches[0].radiusX = 30;
    webTouchEvent.touches[0].radiusY = 30;

    PlatformTouchEventBuilder platformTouchBuilder(view, webTouchEvent);
    EXPECT_FLOAT_EQ(100, platformTouchBuilder.touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(110, platformTouchBuilder.touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(30, platformTouchBuilder.touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(30, platformTouchBuilder.touchPoints()[0].pos().y());
    EXPECT_FLOAT_EQ(10, platformTouchBuilder.touchPoints()[0].radius().width());
    EXPECT_FLOAT_EQ(10,
                    platformTouchBuilder.touchPoints()[0].radius().height());
  }

  {
    WebTouchEvent webTouchEvent1(WebInputEvent::TouchMove,
                                 WebInputEvent::NoModifiers,
                                 WebInputEvent::TimeStampForTesting);
    webTouchEvent1.touchesLength = 1;
    webTouchEvent1.touches[0].state = WebTouchPoint::StateMoved;
    webTouchEvent1.touches[0].screenPosition.x = 100;
    webTouchEvent1.touches[0].screenPosition.y = 110;
    webTouchEvent1.touches[0].position.x = 100;
    webTouchEvent1.touches[0].position.y = 110;
    webTouchEvent1.touches[0].radiusX = 30;
    webTouchEvent1.touches[0].radiusY = 30;

    WebTouchEvent webTouchEvent2 = webTouchEvent1;
    webTouchEvent2.touches[0].screenPosition.x = 130;
    webTouchEvent2.touches[0].position.x = 130;
    webTouchEvent2.touches[0].radiusX = 60;

    std::vector<const WebInputEvent*> events;
    events.push_back(&webTouchEvent1);
    events.push_back(&webTouchEvent2);

    Vector<PlatformTouchEvent> coalescedevents =
        createPlatformTouchEventVector(view, events);
    EXPECT_EQ(events.size(), coalescedevents.size());

    EXPECT_FLOAT_EQ(100, coalescedevents[0].touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(110, coalescedevents[0].touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(30, coalescedevents[0].touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(30, coalescedevents[0].touchPoints()[0].pos().y());
    EXPECT_FLOAT_EQ(10, coalescedevents[0].touchPoints()[0].radius().width());
    EXPECT_FLOAT_EQ(10, coalescedevents[0].touchPoints()[0].radius().height());

    EXPECT_FLOAT_EQ(130, coalescedevents[1].touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(110, coalescedevents[1].touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(40, coalescedevents[1].touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(30, coalescedevents[1].touchPoints()[0].pos().y());
    EXPECT_FLOAT_EQ(20, coalescedevents[1].touchPoints()[0].radius().width());
    EXPECT_FLOAT_EQ(10, coalescedevents[1].touchPoints()[0].radius().height());
  }
}

TEST(WebInputEventConversionTest, InputEventsConversions) {
  const std::string baseURL("http://www.test3.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();
  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureTap,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 10;
    webGestureEvent.y = 10;
    webGestureEvent.globalX = 10;
    webGestureEvent.globalY = 10;
    webGestureEvent.data.tap.tapCount = 1;
    webGestureEvent.data.tap.width = 10;
    webGestureEvent.data.tap.height = 10;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntPoint position =
        flooredIntPoint(scaledGestureEvent.positionInRootFrame());
    EXPECT_EQ(10.f, position.x());
    EXPECT_EQ(10.f, position.y());
    EXPECT_EQ(10.f, scaledGestureEvent.globalX);
    EXPECT_EQ(10.f, scaledGestureEvent.globalY);
    EXPECT_EQ(1, scaledGestureEvent.tapCount());
  }
}

TEST(WebInputEventConversionTest, VisualViewportOffset) {
  const std::string baseURL("http://www.test4.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  webViewImpl->setPageScaleFactor(2);

  IntPoint visualOffset(35, 60);
  webViewImpl->page()->frameHost().visualViewport().setLocation(visualOffset);

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  {
    WebMouseEvent webMouseEvent(WebInputEvent::MouseMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webMouseEvent.x = 10;
    webMouseEvent.y = 10;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 10;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 10;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(5 + visualOffset.x(), platformMouseBuilder.position().x());
    EXPECT_EQ(5 + visualOffset.y(), platformMouseBuilder.position().y());
    EXPECT_EQ(10, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(10, platformMouseBuilder.globalPosition().y());
  }

  {
    WebMouseWheelEvent webMouseWheelEvent(WebInputEvent::MouseWheel,
                                          WebInputEvent::NoModifiers,
                                          WebInputEvent::TimeStampForTesting);
    webMouseWheelEvent.x = 10;
    webMouseWheelEvent.y = 10;
    webMouseWheelEvent.windowX = 10;
    webMouseWheelEvent.windowY = 10;
    webMouseWheelEvent.globalX = 10;
    webMouseWheelEvent.globalY = 10;

    WebMouseWheelEvent scaledMouseWheelEvent =
        TransformWebMouseWheelEvent(view, webMouseWheelEvent);
    IntPoint position =
        flooredIntPoint(scaledMouseWheelEvent.positionInRootFrame());
    EXPECT_EQ(5 + visualOffset.x(), position.x());
    EXPECT_EQ(5 + visualOffset.y(), position.y());
    EXPECT_EQ(10, scaledMouseWheelEvent.globalX);
    EXPECT_EQ(10, scaledMouseWheelEvent.globalY);
  }

  {
    WebGestureEvent webGestureEvent(WebInputEvent::GestureScrollUpdate,
                                    WebInputEvent::NoModifiers,
                                    WebInputEvent::TimeStampForTesting);
    webGestureEvent.sourceDevice = WebGestureDeviceTouchscreen;
    webGestureEvent.x = 10;
    webGestureEvent.y = 10;
    webGestureEvent.globalX = 10;
    webGestureEvent.globalY = 10;

    WebGestureEvent scaledGestureEvent =
        TransformWebGestureEvent(view, webGestureEvent);
    IntPoint position =
        flooredIntPoint(scaledGestureEvent.positionInRootFrame());
    EXPECT_EQ(5 + visualOffset.x(), position.x());
    EXPECT_EQ(5 + visualOffset.y(), position.y());
    EXPECT_EQ(10, scaledGestureEvent.globalX);
    EXPECT_EQ(10, scaledGestureEvent.globalY);
  }

  {
    WebTouchEvent webTouchEvent(WebInputEvent::TouchMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webTouchEvent.touchesLength = 1;
    webTouchEvent.touches[0].state = WebTouchPoint::StateMoved;
    webTouchEvent.touches[0].screenPosition.x = 10.6f;
    webTouchEvent.touches[0].screenPosition.y = 10.4f;
    webTouchEvent.touches[0].position.x = 10.6f;
    webTouchEvent.touches[0].position.y = 10.4f;

    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].screenPosition.x);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].screenPosition.y);
    EXPECT_FLOAT_EQ(10.6f, webTouchEvent.touches[0].position.x);
    EXPECT_FLOAT_EQ(10.4f, webTouchEvent.touches[0].position.y);

    PlatformTouchEventBuilder platformTouchBuilder(view, webTouchEvent);
    EXPECT_FLOAT_EQ(10.6f,
                    platformTouchBuilder.touchPoints()[0].screenPos().x());
    EXPECT_FLOAT_EQ(10.4f,
                    platformTouchBuilder.touchPoints()[0].screenPos().y());
    EXPECT_FLOAT_EQ(5.3f + visualOffset.x(),
                    platformTouchBuilder.touchPoints()[0].pos().x());
    EXPECT_FLOAT_EQ(5.2f + visualOffset.y(),
                    platformTouchBuilder.touchPoints()[0].pos().y());
  }
}

TEST(WebInputEventConversionTest, ElasticOverscroll) {
  const std::string baseURL("http://www.test5.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  FloatSize elasticOverscroll(10, -20);
  webViewImpl->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                   elasticOverscroll, 1.0f, 0.0f);

  // Just elastic overscroll.
  {
    WebMouseEvent webMouseEvent(WebInputEvent::MouseMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webMouseEvent.x = 10;
    webMouseEvent.y = 50;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 50;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 50;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(webMouseEvent.x + elasticOverscroll.width(),
              platformMouseBuilder.position().x());
    EXPECT_EQ(webMouseEvent.y + elasticOverscroll.height(),
              platformMouseBuilder.position().y());
    EXPECT_EQ(webMouseEvent.globalX, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(webMouseEvent.globalY, platformMouseBuilder.globalPosition().y());
  }

  // Elastic overscroll and pinch-zoom (this doesn't actually ever happen,
  // but ensure that if it were to, the overscroll would be applied after the
  // pinch-zoom).
  float pageScale = 2;
  webViewImpl->setPageScaleFactor(pageScale);
  IntPoint visualOffset(35, 60);
  webViewImpl->page()->frameHost().visualViewport().setLocation(visualOffset);
  {
    WebMouseEvent webMouseEvent(WebInputEvent::MouseMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webMouseEvent.x = 10;
    webMouseEvent.y = 10;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 10;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 10;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(webMouseEvent.x / pageScale + visualOffset.x() +
                  elasticOverscroll.width(),
              platformMouseBuilder.position().x());
    EXPECT_EQ(webMouseEvent.y / pageScale + visualOffset.y() +
                  elasticOverscroll.height(),
              platformMouseBuilder.position().y());
    EXPECT_EQ(webMouseEvent.globalX, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(webMouseEvent.globalY, platformMouseBuilder.globalPosition().y());
  }
}

// Page reload/navigation should not reset elastic overscroll.
TEST(WebInputEventConversionTest, ElasticOverscrollWithPageReload) {
  const std::string baseURL("http://www.test6.com/");
  const std::string fileName("fixed_layout.html");

  URLTestHelpers::registerMockedURLFromBaseURL(
      WebString::fromUTF8(baseURL.c_str()),
      WebString::fromUTF8("fixed_layout.html"));
  FrameTestHelpers::WebViewHelper webViewHelper;
  WebViewImpl* webViewImpl =
      webViewHelper.initializeAndLoad(baseURL + fileName, true);
  int pageWidth = 640;
  int pageHeight = 480;
  webViewImpl->resize(WebSize(pageWidth, pageHeight));
  webViewImpl->updateAllLifecyclePhases();

  FloatSize elasticOverscroll(10, -20);
  webViewImpl->applyViewportDeltas(WebFloatSize(), WebFloatSize(),
                                   elasticOverscroll, 1.0f, 0.0f);
  FrameTestHelpers::reloadFrame(webViewHelper.webView()->mainFrame());
  FrameView* view = toLocalFrame(webViewImpl->page()->mainFrame())->view();

  // Just elastic overscroll.
  {
    WebMouseEvent webMouseEvent(WebInputEvent::MouseMove,
                                WebInputEvent::NoModifiers,
                                WebInputEvent::TimeStampForTesting);
    webMouseEvent.x = 10;
    webMouseEvent.y = 50;
    webMouseEvent.windowX = 10;
    webMouseEvent.windowY = 50;
    webMouseEvent.globalX = 10;
    webMouseEvent.globalY = 50;

    PlatformMouseEventBuilder platformMouseBuilder(view, webMouseEvent);
    EXPECT_EQ(webMouseEvent.x + elasticOverscroll.width(),
              platformMouseBuilder.position().x());
    EXPECT_EQ(webMouseEvent.y + elasticOverscroll.height(),
              platformMouseBuilder.position().y());
    EXPECT_EQ(webMouseEvent.globalX, platformMouseBuilder.globalPosition().x());
    EXPECT_EQ(webMouseEvent.globalY, platformMouseBuilder.globalPosition().y());
  }
}

}  // namespace blink
