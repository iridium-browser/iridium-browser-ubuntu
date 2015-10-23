// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/web_input_event_builders_android.h"

#include "base/logging.h"
#include "content/browser/renderer_host/input/motion_event_android.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "content/browser/renderer_host/input/web_input_event_util_posix.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

WebKeyboardEvent WebKeyboardEventBuilder::Build(WebInputEvent::Type type,
                                                int modifiers,
                                                double time_sec,
                                                int keycode,
                                                int unicode_character,
                                                bool is_system_key) {
  DCHECK(WebInputEvent::isKeyboardEventType(type));
  WebKeyboardEvent result;

  result.type = type;
  result.modifiers = modifiers;
  result.timeStampSeconds = time_sec;
  ui::KeyboardCode windows_key_code =
      ui::KeyboardCodeFromAndroidKeyCode(keycode);
  UpdateWindowsKeyCodeAndKeyIdentifier(&result, windows_key_code);
  result.modifiers |= GetLocationModifiersFromWindowsKeyCode(windows_key_code);
  result.nativeKeyCode = keycode;
  result.unmodifiedText[0] = unicode_character;
  if (result.windowsKeyCode == ui::VKEY_RETURN) {
    // This is the same behavior as GTK:
    // We need to treat the enter key as a key press of character \r. This
    // is apparently just how webkit handles it and what it expects.
    result.unmodifiedText[0] = '\r';
  }
  result.text[0] = result.unmodifiedText[0];
  result.isSystemKey = is_system_key;

  return result;
}

WebMouseEvent WebMouseEventBuilder::Build(blink::WebInputEvent::Type type,
                                          WebMouseEvent::Button button,
                                          double time_sec,
                                          int window_x,
                                          int window_y,
                                          int modifiers,
                                          int click_count) {
  DCHECK(WebInputEvent::isMouseEventType(type));
  WebMouseEvent result;

  result.type = type;
  result.x = window_x;
  result.y = window_y;
  result.windowX = window_x;
  result.windowY = window_y;
  result.timeStampSeconds = time_sec;
  result.clickCount = click_count;
  result.modifiers = modifiers;

  if (type == WebInputEvent::MouseDown || type == WebInputEvent::MouseUp)
    result.button = button;
  else
    result.button = WebMouseEvent::ButtonNone;

  return result;
}

WebMouseWheelEvent WebMouseWheelEventBuilder::Build(float ticks_x,
                                                    float ticks_y,
                                                    float tick_multiplier,
                                                    double time_sec,
                                                    int window_x,
                                                    int window_y) {
  WebMouseWheelEvent result;

  result.type = WebInputEvent::MouseWheel;
  result.x = window_x;
  result.y = window_y;
  result.windowX = window_x;
  result.windowY = window_y;
  result.timeStampSeconds = time_sec;
  result.button = WebMouseEvent::ButtonNone;
  result.hasPreciseScrollingDeltas = true;
  result.deltaX = ticks_x * tick_multiplier;
  result.deltaY = ticks_y * tick_multiplier;
  result.wheelTicksX = ticks_x;
  result.wheelTicksY = ticks_y;

  return result;
}

WebGestureEvent WebGestureEventBuilder::Build(WebInputEvent::Type type,
                                              double time_sec,
                                              int x,
                                              int y) {
  DCHECK(WebInputEvent::isGestureEventType(type));
  WebGestureEvent result;

  result.type = type;
  result.x = x;
  result.y = y;
  result.timeStampSeconds = time_sec;
  result.sourceDevice = blink::WebGestureDeviceTouchscreen;

  return result;
}

}  // namespace content
