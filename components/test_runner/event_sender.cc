// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/event_sender.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/test_runner/mock_spell_check.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_view_test_proxy.h"
#include "components/test_runner/web_widget_test_proxy.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebPointerProperties.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "v8/include/v8.h"

using blink::WebContextMenuData;
using blink::WebDragData;
using blink::WebDragOperationsMask;
using blink::WebFloatPoint;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebMenuItemInfo;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPagePopup;
using blink::WebPoint;
using blink::WebPointerProperties;
using blink::WebString;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebVector;
using blink::WebView;

namespace test_runner {

namespace {

const int kRawMousePointerId = -1;
const char* const kPointerTypeStringUnknown = "";
const char* const kPointerTypeStringMouse = "mouse";
const char* const kPointerTypeStringTouch = "touch";
const char* const kPointerTypeStringPen = "pen";
const char* const kPointerTypeStringEraser = "eraser";

// Assigns |pointerType| from the provided |args|. Returns false if there was
// any error.
bool getPointerType(gin::Arguments* args,
                    bool isOnlyMouseAndPenAllowed,
                    WebPointerProperties::PointerType& pointerType) {
  if (args->PeekNext().IsEmpty())
    return true;
  std::string pointer_type_string;
  if (!args->GetNext(&pointer_type_string)) {
    args->ThrowError();
    return false;
  }
  if (isOnlyMouseAndPenAllowed &&
      (pointer_type_string == kPointerTypeStringUnknown ||
       pointer_type_string == kPointerTypeStringTouch)) {
    args->ThrowError();
    return false;
  }
  if (pointer_type_string == kPointerTypeStringUnknown) {
    pointerType = WebMouseEvent::PointerType::Unknown;
  } else if (pointer_type_string == kPointerTypeStringMouse) {
    pointerType = WebMouseEvent::PointerType::Mouse;
  } else if (pointer_type_string == kPointerTypeStringTouch) {
    pointerType = WebMouseEvent::PointerType::Touch;
  } else if (pointer_type_string == kPointerTypeStringPen) {
    pointerType = WebMouseEvent::PointerType::Pen;
  } else if (pointer_type_string == kPointerTypeStringEraser) {
    pointerType = WebMouseEvent::PointerType::Eraser;
  } else {
    args->ThrowError();
    return false;
  }
  return true;
}

// Parses |pointerType|, |rawPointerId|, |pressure|, |tiltX| and |tiltY| from
// the provided |args|. Returns false if there was any error, assuming the last
// 3 of the five parsed parameters are optional.
bool getMousePenPointerProperties(
    gin::Arguments* args,
    WebPointerProperties::PointerType& pointerType,
    int& rawPointerId,
    float& pressure,
    int& tiltX,
    int& tiltY) {
  pointerType = WebPointerProperties::PointerType::Mouse;
  rawPointerId = kRawMousePointerId;
  pressure = std::numeric_limits<float>::quiet_NaN();
  tiltX = 0;
  tiltY = 0;

  // Only allow pen or mouse through this API.
  if (!getPointerType(args, false, pointerType))
    return false;
  if (!args->PeekNext().IsEmpty()) {
    if (!args->GetNext(&rawPointerId)) {
      args->ThrowError();
      return false;
    }

    // Parse optional params
    if (!args->PeekNext().IsEmpty()) {
      if (!args->GetNext(&pressure)) {
        args->ThrowError();
        return false;
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&tiltX)) {
          args->ThrowError();
          return false;
        }
        if (!args->PeekNext().IsEmpty()) {
          if (!args->GetNext(&tiltY)) {
            args->ThrowError();
            return false;
          }
        }
      }
    }
  }

  return true;
}

WebMouseEvent::Button GetButtonTypeFromButtonNumber(int button_code) {
  switch (button_code) {
    case -1:
      return WebMouseEvent::Button::NoButton;
    case 0:
      return WebMouseEvent::Button::Left;
    case 1:
      return WebMouseEvent::Button::Middle;
    case 2:
      return WebMouseEvent::Button::Right;
  }
  NOTREACHED();
  return WebMouseEvent::Button::NoButton;
}

int GetWebMouseEventModifierForButton(WebMouseEvent::Button button) {
  switch (button) {
    case WebMouseEvent::Button::NoButton:
      return 0;
    case WebMouseEvent::Button::Left:
      return WebMouseEvent::LeftButtonDown;
    case WebMouseEvent::Button::Middle:
      return WebMouseEvent::MiddleButtonDown;
    case WebMouseEvent::Button::Right:
      return WebMouseEvent::RightButtonDown;
    case WebPointerProperties::Button::X1:
    case WebPointerProperties::Button::X2:
    case WebPointerProperties::Button::Eraser:
      return 0; // Not implemented yet
  }
  NOTREACHED();
  return 0;
}

const int kButtonsInModifiers = WebMouseEvent::LeftButtonDown
    | WebMouseEvent::MiddleButtonDown | WebMouseEvent::RightButtonDown;

int modifiersWithButtons(int modifiers, int buttons) {
  return (modifiers & ~kButtonsInModifiers)
      | (buttons & kButtonsInModifiers);
}

void InitMouseEventGeneric(WebMouseEvent::Button b,
                           int current_buttons,
                           const WebPoint& pos,
                           int click_count,
                           WebPointerProperties::PointerType pointerType,
                           int pointerId,
                           float pressure,
                           int tiltX,
                           int tiltY,
                           WebMouseEvent* e) {
  e->button = b;
  e->x = pos.x;
  e->y = pos.y;
  e->globalX = pos.x;
  e->globalY = pos.y;
  e->pointerType = pointerType;
  e->id = pointerId;
  e->force = pressure;
  e->tiltX = tiltX;
  e->tiltY = tiltY;
  e->clickCount = click_count;
}

void InitMouseEvent(WebMouseEvent::Button b,
                    int current_buttons,
                    const WebPoint& pos,
                    int click_count,
                    WebMouseEvent* e) {
  InitMouseEventGeneric(b, current_buttons, pos, click_count,
                        WebPointerProperties::PointerType::Mouse, 0, 0.0, 0, 0,
                        e);
}

void InitGestureEventFromMouseWheel(const WebMouseWheelEvent& wheel_event,
                                    WebGestureEvent* gesture_event) {
  gesture_event->sourceDevice = blink::WebGestureDeviceTouchpad;
  gesture_event->x = wheel_event.x;
  gesture_event->y = wheel_event.y;
  gesture_event->globalX = wheel_event.globalX;
  gesture_event->globalY = wheel_event.globalY;
}

int GetKeyModifier(const std::string& modifier_name) {
  const char* characters = modifier_name.c_str();
  if (!strcmp(characters, "ctrlKey")
#ifndef __APPLE__
      || !strcmp(characters, "addSelectionKey")
#endif
      ) {
    return WebInputEvent::ControlKey;
  } else if (!strcmp(characters, "shiftKey") ||
             !strcmp(characters, "rangeSelectionKey")) {
    return WebInputEvent::ShiftKey;
  } else if (!strcmp(characters, "altKey")) {
    return WebInputEvent::AltKey;
#ifdef __APPLE__
  } else if (!strcmp(characters, "metaKey") ||
             !strcmp(characters, "addSelectionKey")) {
    return WebInputEvent::MetaKey;
#else
  } else if (!strcmp(characters, "metaKey")) {
    return WebInputEvent::MetaKey;
#endif
  } else if (!strcmp(characters, "autoRepeat")) {
    return WebInputEvent::IsAutoRepeat;
  } else if (!strcmp(characters, "copyKey")) {
#ifdef __APPLE__
    return WebInputEvent::AltKey;
#else
    return WebInputEvent::ControlKey;
#endif
  } else if (!strcmp(characters, "accessKey")) {
#ifdef __APPLE__
    return WebInputEvent::AltKey | WebInputEvent::ControlKey;
#else
    return WebInputEvent::AltKey;
#endif
  } else if (!strcmp(characters, "leftButton")) {
    return WebInputEvent::LeftButtonDown;
  } else if (!strcmp(characters, "middleButton")) {
    return WebInputEvent::MiddleButtonDown;
  } else if (!strcmp(characters, "rightButton")) {
    return WebInputEvent::RightButtonDown;
  } else if (!strcmp(characters, "capsLockOn")) {
    return WebInputEvent::CapsLockOn;
  } else if (!strcmp(characters, "numLockOn")) {
    return WebInputEvent::NumLockOn;
  } else if (!strcmp(characters, "locationLeft")) {
    return WebInputEvent::IsLeft;
  } else if (!strcmp(characters, "locationRight")) {
    return WebInputEvent::IsRight;
  } else if (!strcmp(characters, "locationNumpad")) {
    return WebInputEvent::IsKeyPad;
  } else if (!strcmp(characters, "isComposing")) {
    return WebInputEvent::IsComposing;
  } else if (!strcmp(characters, "altGraphKey")) {
    return WebInputEvent::AltGrKey;
  } else if (!strcmp(characters, "fnKey")) {
    return WebInputEvent::FnKey;
  } else if (!strcmp(characters, "symbolKey")) {
    return WebInputEvent::SymbolKey;
  } else if (!strcmp(characters, "scrollLockOn")) {
    return WebInputEvent::ScrollLockOn;
  }

  return 0;
}

int GetKeyModifiers(const std::vector<std::string>& modifier_names) {
  int modifiers = 0;
  for (std::vector<std::string>::const_iterator it = modifier_names.begin();
       it != modifier_names.end(); ++it) {
    modifiers |= GetKeyModifier(*it);
  }
  return modifiers;
}

int GetKeyModifiersFromV8(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  std::vector<std::string> modifier_names;
  if (value->IsString()) {
    modifier_names.push_back(gin::V8ToString(value));
  } else if (value->IsArray()) {
    gin::Converter<std::vector<std::string> >::FromV8(
        isolate, value, &modifier_names);
  }
  return GetKeyModifiers(modifier_names);
}

WebMouseWheelEvent::Phase GetMouseWheelEventPhase(
    const std::string& phase_name) {
  if (phase_name == "phaseNone") {
    return WebMouseWheelEvent::PhaseNone;
  } else if (phase_name == "phaseBegan") {
    return WebMouseWheelEvent::PhaseBegan;
  } else if (phase_name == "phaseStationary") {
    return WebMouseWheelEvent::PhaseStationary;
  } else if (phase_name == "phaseChanged") {
    return WebMouseWheelEvent::PhaseChanged;
  } else if (phase_name == "phaseEnded") {
    return WebMouseWheelEvent::PhaseEnded;
  } else if (phase_name == "phaseCancelled") {
    return WebMouseWheelEvent::PhaseCancelled;
  } else if (phase_name == "phaseMayBegin") {
    return WebMouseWheelEvent::PhaseMayBegin;
  }

  return WebMouseWheelEvent::PhaseNone;
}

WebMouseWheelEvent::Phase GetMouseWheelEventPhaseFromV8(
    v8::Local<v8::Value> value) {
  if (value->IsString())
    return GetMouseWheelEventPhase(gin::V8ToString(value));
  return WebMouseWheelEvent::PhaseNone;
}

// Maximum distance (in space and time) for a mouse click to register as a
// double or triple click.
const double kMultipleClickTimeSec = 1;
const int kMultipleClickRadiusPixels = 5;
const char kSubMenuDepthIdentifier[] = "_";
const char kSubMenuIdentifier[] = " >";
const char kSeparatorIdentifier[] = "---------";
const char kDisabledIdentifier[] = "#";
const char kCheckedIdentifier[] = "*";

bool OutsideMultiClickRadius(const WebPoint& a, const WebPoint& b) {
  return ((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)) >
         kMultipleClickRadiusPixels * kMultipleClickRadiusPixels;
}

void PopulateCustomItems(const WebVector<WebMenuItemInfo>& customItems,
    const std::string& prefix, std::vector<std::string>* strings) {
  for (size_t i = 0; i < customItems.size(); ++i) {
    std::string prefixCopy = prefix;
    if (!customItems[i].enabled)
        prefixCopy = kDisabledIdentifier + prefix;
    if (customItems[i].checked)
        prefixCopy = kCheckedIdentifier + prefix;
    if (customItems[i].type == blink::WebMenuItemInfo::Separator) {
      strings->push_back(prefixCopy + kSeparatorIdentifier);
    } else if (customItems[i].type == blink::WebMenuItemInfo::SubMenu) {
      strings->push_back(prefixCopy + customItems[i].label.utf8() +
          customItems[i].icon.utf8() + kSubMenuIdentifier);
      PopulateCustomItems(customItems[i].subMenuItems, prefixCopy +
          kSubMenuDepthIdentifier, strings);
    } else {
      strings->push_back(prefixCopy + customItems[i].label.utf8() +
          customItems[i].icon.utf8());
    }
  }
}

// Because actual context menu is implemented by the browser side,
// this function does only what LayoutTests are expecting:
// - Many test checks the count of items. So returning non-zero value makes
// sense.
// - Some test compares the count before and after some action. So changing the
// count based on flags also makes sense. This function is doing such for some
// flags.
// - Some test even checks actual string content. So providing it would be also
// helpful.
std::vector<std::string> MakeMenuItemStringsFor(
    WebContextMenuData* context_menu,
    WebTestDelegate* delegate) {
  // These constants are based on Safari's context menu because tests are made
  // for it.
  static const char* kNonEditableMenuStrings[] = {
    "Back",
    "Reload Page",
    "Open in Dashbaord",
    "<separator>",
    "View Source",
    "Save Page As",
    "Print Page",
    "Inspect Element",
    0
  };
  static const char* kEditableMenuStrings[] = {
    "Cut",
    "Copy",
    "<separator>",
    "Paste",
    "Spelling and Grammar",
    "Substitutions, Transformations",
    "Font",
    "Speech",
    "Paragraph Direction",
    "<separator>",
    0
  };

  // This is possible because mouse events are cancelleable.
  if (!context_menu)
    return std::vector<std::string>();

  std::vector<std::string> strings;

  // Populate custom menu items if provided by blink.
  PopulateCustomItems(context_menu->customItems, "", &strings);

  if (context_menu->isEditable) {
    for (const char** item = kEditableMenuStrings; *item; ++item) {
      strings.push_back(*item);
    }
    WebVector<WebString> suggestions;
    MockSpellCheck::FillSuggestionList(context_menu->misspelledWord,
                                       &suggestions);
    for (size_t i = 0; i < suggestions.size(); ++i) {
      strings.push_back(suggestions[i].utf8());
    }
  } else {
    for (const char** item = kNonEditableMenuStrings; *item; ++item) {
      strings.push_back(*item);
    }
  }

  return strings;
}

// How much we should scroll per event - the value here is chosen to match the
// WebKit impl and layout test results.
const float kScrollbarPixelsPerTick = 40.0f;

// Get the edit command corresponding to a keyboard event.
// Returns true if the specified event corresponds to an edit command, the name
// of the edit command will be stored in |*name|.
bool GetEditCommand(const WebKeyboardEvent& event, std::string* name) {
#if defined(OS_MACOSX)
// We only cares about Left,Right,Up,Down keys with Command or Command+Shift
// modifiers. These key events correspond to some special movement and
// selection editor commands. These keys will be marked as system key, which
// prevents them from being handled. Thus they must be handled specially.
  if ((event.modifiers() & ~WebKeyboardEvent::ShiftKey) !=
      WebKeyboardEvent::MetaKey)
    return false;

  switch (event.windowsKeyCode) {
    case ui::VKEY_LEFT:
      *name = "MoveToBeginningOfLine";
      break;
    case ui::VKEY_RIGHT:
      *name = "MoveToEndOfLine";
      break;
    case ui::VKEY_UP:
      *name = "MoveToBeginningOfDocument";
      break;
    case ui::VKEY_DOWN:
      *name = "MoveToEndOfDocument";
      break;
    default:
      return false;
  }

  if (event.modifiers() & WebKeyboardEvent::ShiftKey)
    name->append("AndModifySelection");

  return true;
#else
  return false;
#endif
}

bool IsSystemKeyEvent(const WebKeyboardEvent& event) {
#if defined(OS_MACOSX)
  return event.modifiers() & WebInputEvent::MetaKey &&
         event.windowsKeyCode != ui::VKEY_B &&
         event.windowsKeyCode != ui::VKEY_I;
#else
  return !!(event.modifiers() & WebInputEvent::AltKey);
#endif
}

bool GetScrollUnits(gin::Arguments* args, WebGestureEvent::ScrollUnits* units) {
  std::string units_string;
  if (!args->PeekNext().IsEmpty()) {
    if (args->PeekNext()->IsString())
      args->GetNext(&units_string);
    if (units_string == "Page") {
      *units = WebGestureEvent::Page;
      return true;
    } else if (units_string == "Pixels") {
      *units = WebGestureEvent::Pixels;
      return true;
    } else if (units_string == "PrecisePixels") {
      *units = WebGestureEvent::PrecisePixels;
      return true;
    } else {
      args->ThrowError();
      return false;
    }
  } else {
    *units = WebGestureEvent::PrecisePixels;
    return true;
  }
}

const char* kSourceDeviceStringTouchpad = "touchpad";
const char* kSourceDeviceStringTouchscreen = "touchscreen";

}  // namespace

class EventSenderBindings : public gin::Wrappable<EventSenderBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<EventSender> sender,
                      blink::WebLocalFrame* frame);

 private:
  explicit EventSenderBindings(base::WeakPtr<EventSender> sender);
  ~EventSenderBindings() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Bound methods:
  void EnableDOMUIEventLogging();
  void FireKeyboardEventsToElement();
  void ClearKillRing();
  std::vector<std::string> ContextClick();
  void TextZoomIn();
  void TextZoomOut();
  void ZoomPageIn();
  void ZoomPageOut();
  void SetPageZoomFactor(double factor);
  void ClearTouchPoints();
  void ReleaseTouchPoint(unsigned index);
  void UpdateTouchPoint(unsigned index,
                        double x,
                        double y,
                        gin::Arguments* args);
  void CancelTouchPoint(unsigned index);
  void SetTouchModifier(const std::string& key_name, bool set_mask);
  void SetTouchCancelable(bool cancelable);
  void DumpFilenameBeingDragged();
  void GestureFlingCancel();
  void GestureFlingStart(float x,
                         float y,
                         float velocity_x,
                         float velocity_y,
                         gin::Arguments* args);
  bool IsFlinging() const;
  void GestureScrollFirstPoint(int x, int y);
  void TouchStart(gin::Arguments* args);
  void TouchMove(gin::Arguments* args);
  void TouchCancel(gin::Arguments* args);
  void TouchEnd(gin::Arguments* args);
  void NotifyStartOfTouchScroll();
  void LeapForward(int milliseconds);
  double LastEventTimestamp();
  void BeginDragWithFiles(const std::vector<std::string>& files);
  void AddTouchPoint(double x, double y, gin::Arguments* args);
  void GestureScrollBegin(gin::Arguments* args);
  void GestureScrollEnd(gin::Arguments* args);
  void GestureScrollUpdate(gin::Arguments* args);
  void GesturePinchBegin(gin::Arguments* args);
  void GesturePinchEnd(gin::Arguments* args);
  void GesturePinchUpdate(gin::Arguments* args);
  void GestureTap(gin::Arguments* args);
  void GestureTapDown(gin::Arguments* args);
  void GestureShowPress(gin::Arguments* args);
  void GestureTapCancel(gin::Arguments* args);
  void GestureLongPress(gin::Arguments* args);
  void GestureLongTap(gin::Arguments* args);
  void GestureTwoFingerTap(gin::Arguments* args);
  void ContinuousMouseScrollBy(gin::Arguments* args);
  void MouseMoveTo(gin::Arguments* args);
  void MouseLeave();
  void MouseScrollBy(gin::Arguments* args);
  void ScheduleAsynchronousClick(gin::Arguments* args);
  void ScheduleAsynchronousKeyDown(gin::Arguments* args);
  void MouseDown(gin::Arguments* args);
  void MouseUp(gin::Arguments* args);
  void SetMouseButtonState(gin::Arguments* args);
  void KeyDown(gin::Arguments* args);

  // Binding properties:
  bool ForceLayoutOnEvents() const;
  void SetForceLayoutOnEvents(bool force);
  bool IsDragMode() const;
  void SetIsDragMode(bool drag_mode);

#if defined(OS_WIN)
  int WmKeyDown() const;
  void SetWmKeyDown(int key_down);

  int WmKeyUp() const;
  void SetWmKeyUp(int key_up);

  int WmChar() const;
  void SetWmChar(int wm_char);

  int WmDeadChar() const;
  void SetWmDeadChar(int dead_char);

  int WmSysKeyDown() const;
  void SetWmSysKeyDown(int key_down);

  int WmSysKeyUp() const;
  void SetWmSysKeyUp(int key_up);

  int WmSysChar() const;
  void SetWmSysChar(int sys_char);

  int WmSysDeadChar() const;
  void SetWmSysDeadChar(int sys_dead_char);
#endif

  base::WeakPtr<EventSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(EventSenderBindings);
};

gin::WrapperInfo EventSenderBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

EventSenderBindings::EventSenderBindings(base::WeakPtr<EventSender> sender)
    : sender_(sender) {
}

EventSenderBindings::~EventSenderBindings() {}

// static
void EventSenderBindings::Install(base::WeakPtr<EventSender> sender,
                                  WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<EventSenderBindings> bindings =
      gin::CreateHandle(isolate, new EventSenderBindings(sender));
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "eventSender"), bindings.ToV8());
}

gin::ObjectTemplateBuilder
EventSenderBindings::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<EventSenderBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("enableDOMUIEventLogging",
                 &EventSenderBindings::EnableDOMUIEventLogging)
      .SetMethod("fireKeyboardEventsToElement",
                 &EventSenderBindings::FireKeyboardEventsToElement)
      .SetMethod("clearKillRing", &EventSenderBindings::ClearKillRing)
      .SetMethod("contextClick", &EventSenderBindings::ContextClick)
      .SetMethod("textZoomIn", &EventSenderBindings::TextZoomIn)
      .SetMethod("textZoomOut", &EventSenderBindings::TextZoomOut)
      .SetMethod("zoomPageIn", &EventSenderBindings::ZoomPageIn)
      .SetMethod("zoomPageOut", &EventSenderBindings::ZoomPageOut)
      .SetMethod("setPageZoomFactor", &EventSenderBindings::SetPageZoomFactor)
      .SetMethod("clearTouchPoints", &EventSenderBindings::ClearTouchPoints)
      .SetMethod("releaseTouchPoint", &EventSenderBindings::ReleaseTouchPoint)
      .SetMethod("updateTouchPoint", &EventSenderBindings::UpdateTouchPoint)
      .SetMethod("cancelTouchPoint", &EventSenderBindings::CancelTouchPoint)
      .SetMethod("setTouchModifier", &EventSenderBindings::SetTouchModifier)
      .SetMethod("setTouchCancelable", &EventSenderBindings::SetTouchCancelable)
      .SetMethod("dumpFilenameBeingDragged",
                 &EventSenderBindings::DumpFilenameBeingDragged)
      .SetMethod("gestureFlingCancel", &EventSenderBindings::GestureFlingCancel)
      .SetMethod("gestureFlingStart", &EventSenderBindings::GestureFlingStart)
      .SetMethod("isFlinging", &EventSenderBindings::IsFlinging)
      .SetMethod("gestureScrollFirstPoint",
                 &EventSenderBindings::GestureScrollFirstPoint)
      .SetMethod("touchStart", &EventSenderBindings::TouchStart)
      .SetMethod("touchMove", &EventSenderBindings::TouchMove)
      .SetMethod("touchCancel", &EventSenderBindings::TouchCancel)
      .SetMethod("touchEnd", &EventSenderBindings::TouchEnd)
      .SetMethod("notifyStartOfTouchScroll",
                 &EventSenderBindings::NotifyStartOfTouchScroll)
      .SetMethod("leapForward", &EventSenderBindings::LeapForward)
      .SetMethod("lastEventTimestamp", &EventSenderBindings::LastEventTimestamp)
      .SetMethod("beginDragWithFiles", &EventSenderBindings::BeginDragWithFiles)
      .SetMethod("addTouchPoint", &EventSenderBindings::AddTouchPoint)
      .SetMethod("gestureScrollBegin", &EventSenderBindings::GestureScrollBegin)
      .SetMethod("gestureScrollEnd", &EventSenderBindings::GestureScrollEnd)
      .SetMethod("gestureScrollUpdate",
                 &EventSenderBindings::GestureScrollUpdate)
      .SetMethod("gesturePinchBegin", &EventSenderBindings::GesturePinchBegin)
      .SetMethod("gesturePinchEnd", &EventSenderBindings::GesturePinchEnd)
      .SetMethod("gesturePinchUpdate", &EventSenderBindings::GesturePinchUpdate)
      .SetMethod("gestureTap", &EventSenderBindings::GestureTap)
      .SetMethod("gestureTapDown", &EventSenderBindings::GestureTapDown)
      .SetMethod("gestureShowPress", &EventSenderBindings::GestureShowPress)
      .SetMethod("gestureTapCancel", &EventSenderBindings::GestureTapCancel)
      .SetMethod("gestureLongPress", &EventSenderBindings::GestureLongPress)
      .SetMethod("gestureLongTap", &EventSenderBindings::GestureLongTap)
      .SetMethod("gestureTwoFingerTap",
                 &EventSenderBindings::GestureTwoFingerTap)
      .SetMethod("continuousMouseScrollBy",
                 &EventSenderBindings::ContinuousMouseScrollBy)
      .SetMethod("keyDown", &EventSenderBindings::KeyDown)
      .SetMethod("mouseDown", &EventSenderBindings::MouseDown)
      .SetMethod("mouseMoveTo", &EventSenderBindings::MouseMoveTo)
      .SetMethod("mouseLeave", &EventSenderBindings::MouseLeave)
      .SetMethod("mouseScrollBy", &EventSenderBindings::MouseScrollBy)
      .SetMethod("mouseUp", &EventSenderBindings::MouseUp)
      .SetMethod("setMouseButtonState",
                 &EventSenderBindings::SetMouseButtonState)
      .SetMethod("scheduleAsynchronousClick",
                 &EventSenderBindings::ScheduleAsynchronousClick)
      .SetMethod("scheduleAsynchronousKeyDown",
                 &EventSenderBindings::ScheduleAsynchronousKeyDown)
      .SetProperty("forceLayoutOnEvents",
                   &EventSenderBindings::ForceLayoutOnEvents,
                   &EventSenderBindings::SetForceLayoutOnEvents)
#if defined(OS_WIN)
      .SetProperty("WM_KEYDOWN", &EventSenderBindings::WmKeyDown,
                   &EventSenderBindings::SetWmKeyDown)
      .SetProperty("WM_KEYUP", &EventSenderBindings::WmKeyUp,
                   &EventSenderBindings::SetWmKeyUp)
      .SetProperty("WM_CHAR", &EventSenderBindings::WmChar,
                   &EventSenderBindings::SetWmChar)
      .SetProperty("WM_DEADCHAR", &EventSenderBindings::WmDeadChar,
                   &EventSenderBindings::SetWmDeadChar)
      .SetProperty("WM_SYSKEYDOWN", &EventSenderBindings::WmSysKeyDown,
                   &EventSenderBindings::SetWmSysKeyDown)
      .SetProperty("WM_SYSKEYUP", &EventSenderBindings::WmSysKeyUp,
                   &EventSenderBindings::SetWmSysKeyUp)
      .SetProperty("WM_SYSCHAR", &EventSenderBindings::WmSysChar,
                   &EventSenderBindings::SetWmSysChar)
      .SetProperty("WM_SYSDEADCHAR", &EventSenderBindings::WmSysDeadChar,
                   &EventSenderBindings::SetWmSysDeadChar)
#endif
      .SetProperty("dragMode", &EventSenderBindings::IsDragMode,
                   &EventSenderBindings::SetIsDragMode);
}

void EventSenderBindings::EnableDOMUIEventLogging() {
  if (sender_)
    sender_->EnableDOMUIEventLogging();
}

void EventSenderBindings::FireKeyboardEventsToElement() {
  if (sender_)
    sender_->FireKeyboardEventsToElement();
}

void EventSenderBindings::ClearKillRing() {
  if (sender_)
    sender_->ClearKillRing();
}

std::vector<std::string> EventSenderBindings::ContextClick() {
  if (sender_)
    return sender_->ContextClick();
  return std::vector<std::string>();
}

void EventSenderBindings::TextZoomIn() {
  if (sender_)
    sender_->TextZoomIn();
}

void EventSenderBindings::TextZoomOut() {
  if (sender_)
    sender_->TextZoomOut();
}

void EventSenderBindings::ZoomPageIn() {
  if (sender_)
    sender_->ZoomPageIn();
}

void EventSenderBindings::ZoomPageOut() {
  if (sender_)
    sender_->ZoomPageOut();
}

void EventSenderBindings::SetPageZoomFactor(double factor) {
  if (sender_)
    sender_->SetPageZoomFactor(factor);
}

void EventSenderBindings::ClearTouchPoints() {
  if (sender_)
    sender_->ClearTouchPoints();
}

void EventSenderBindings::ReleaseTouchPoint(unsigned index) {
  if (sender_)
    sender_->ReleaseTouchPoint(index);
}

void EventSenderBindings::UpdateTouchPoint(unsigned index,
                                           double x,
                                           double y,
                                           gin::Arguments* args) {
  if (sender_) {
    sender_->UpdateTouchPoint(index, static_cast<float>(x),
                              static_cast<float>(y), args);
  }
}

void EventSenderBindings::CancelTouchPoint(unsigned index) {
  if (sender_)
    sender_->CancelTouchPoint(index);
}

void EventSenderBindings::SetTouchModifier(const std::string& key_name,
                                           bool set_mask) {
  if (sender_)
    sender_->SetTouchModifier(key_name, set_mask);
}

void EventSenderBindings::SetTouchCancelable(bool cancelable) {
  if (sender_)
    sender_->SetTouchCancelable(cancelable);
}

void EventSenderBindings::DumpFilenameBeingDragged() {
  if (sender_)
    sender_->DumpFilenameBeingDragged();
}

void EventSenderBindings::GestureFlingCancel() {
  if (sender_)
    sender_->GestureFlingCancel();
}

void EventSenderBindings::GestureFlingStart(float x,
                                            float y,
                                            float velocity_x,
                                            float velocity_y,
                                            gin::Arguments* args) {
  if (sender_)
    sender_->GestureFlingStart(x, y, velocity_x, velocity_y, args);
}

bool EventSenderBindings::IsFlinging() const {
  if (sender_)
    return sender_->IsFlinging();
  return false;
}

void EventSenderBindings::GestureScrollFirstPoint(int x, int y) {
  if (sender_)
    sender_->GestureScrollFirstPoint(x, y);
}

void EventSenderBindings::TouchStart(gin::Arguments* args) {
  if (sender_)
    sender_->TouchStart(args);
}

void EventSenderBindings::TouchMove(gin::Arguments* args) {
  if (sender_)
    sender_->TouchMove(args);
}

void EventSenderBindings::TouchCancel(gin::Arguments* args) {
  if (sender_)
    sender_->TouchCancel(args);
}

void EventSenderBindings::TouchEnd(gin::Arguments* args) {
  if (sender_)
    sender_->TouchEnd(args);
}

void EventSenderBindings::NotifyStartOfTouchScroll() {
  if (sender_)
    sender_->NotifyStartOfTouchScroll();
}

void EventSenderBindings::LeapForward(int milliseconds) {
  if (sender_)
    sender_->LeapForward(milliseconds);
}

double EventSenderBindings::LastEventTimestamp() {
  if (sender_)
    return sender_->last_event_timestamp();
  return 0;
}

void EventSenderBindings::BeginDragWithFiles(
    const std::vector<std::string>& files) {
  if (sender_)
    sender_->BeginDragWithFiles(files);
}

void EventSenderBindings::AddTouchPoint(double x,
                                        double y,
                                        gin::Arguments* args) {
  if (sender_)
    sender_->AddTouchPoint(static_cast<float>(x), static_cast<float>(y), args);
}

void EventSenderBindings::GestureScrollBegin(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollBegin(args);
}

void EventSenderBindings::GestureScrollEnd(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollEnd(args);
}

void EventSenderBindings::GestureScrollUpdate(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollUpdate(args);
}

void EventSenderBindings::GesturePinchBegin(gin::Arguments* args) {
  if (sender_)
    sender_->GesturePinchBegin(args);
}

void EventSenderBindings::GesturePinchEnd(gin::Arguments* args) {
  if (sender_)
    sender_->GesturePinchEnd(args);
}

void EventSenderBindings::GesturePinchUpdate(gin::Arguments* args) {
  if (sender_)
    sender_->GesturePinchUpdate(args);
}

void EventSenderBindings::GestureTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTap(args);
}

void EventSenderBindings::GestureTapDown(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTapDown(args);
}

void EventSenderBindings::GestureShowPress(gin::Arguments* args) {
  if (sender_)
    sender_->GestureShowPress(args);
}

void EventSenderBindings::GestureTapCancel(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTapCancel(args);
}

void EventSenderBindings::GestureLongPress(gin::Arguments* args) {
  if (sender_)
    sender_->GestureLongPress(args);
}

void EventSenderBindings::GestureLongTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureLongTap(args);
}

void EventSenderBindings::GestureTwoFingerTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTwoFingerTap(args);
}

void EventSenderBindings::ContinuousMouseScrollBy(gin::Arguments* args) {
  if (sender_)
    sender_->MouseScrollBy(args, EventSender::MouseScrollType::PIXEL);
}

void EventSenderBindings::MouseMoveTo(gin::Arguments* args) {
  if (sender_)
    sender_->MouseMoveTo(args);
}

void EventSenderBindings::MouseLeave() {
  if (sender_)
    sender_->MouseLeave();
}

void EventSenderBindings::MouseScrollBy(gin::Arguments* args) {
  if (sender_)
    sender_->MouseScrollBy(args, EventSender::MouseScrollType::TICK);
}

void EventSenderBindings::ScheduleAsynchronousClick(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    args->GetNext(&button_number);
    if (!args->PeekNext().IsEmpty())
      modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
  }
  sender_->ScheduleAsynchronousClick(button_number, modifiers);
}

void EventSenderBindings::ScheduleAsynchronousKeyDown(gin::Arguments* args) {
  if (!sender_)
    return;

  std::string code_str;
  int modifiers = 0;
  int location = DOMKeyLocationStandard;
  args->GetNext(&code_str);
  if (!args->PeekNext().IsEmpty()) {
    v8::Local<v8::Value> value;
    args->GetNext(&value);
    modifiers = GetKeyModifiersFromV8(args->isolate(), value);
    if (!args->PeekNext().IsEmpty())
      args->GetNext(&location);
  }
  sender_->ScheduleAsynchronousKeyDown(code_str, modifiers,
                                       static_cast<KeyLocationCode>(location));
}

void EventSenderBindings::MouseDown(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    if (!args->GetNext(&button_number)) {
      args->ThrowError();
      return;
    }
    if (!args->PeekNext().IsEmpty()) {
      modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
      args->Skip();
    }
  }

  WebPointerProperties::PointerType pointerType =
      WebPointerProperties::PointerType::Mouse;
  int pointerId = 0;
  float pressure = 0;
  int tiltX = 0;
  int tiltY = 0;
  if (!getMousePenPointerProperties(args, pointerType, pointerId, pressure,
                                    tiltX, tiltY))
    return;

  sender_->PointerDown(button_number, modifiers, pointerType, pointerId,
                       pressure, tiltX, tiltY);
}

void EventSenderBindings::MouseUp(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    if (!args->GetNext(&button_number)) {
      args->ThrowError();
      return;
    }
    if (!args->PeekNext().IsEmpty()) {
      modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
      args->Skip();
    }
  }

  WebPointerProperties::PointerType pointerType =
      WebPointerProperties::PointerType::Mouse;
  int pointerId = 0;
  float pressure = 0;
  int tiltX = 0;
  int tiltY = 0;
  if (!getMousePenPointerProperties(args, pointerType, pointerId, pressure,
                                    tiltX, tiltY))
    return;

  sender_->PointerUp(button_number, modifiers, pointerType, pointerId, pressure,
                     tiltX, tiltY);
}

void EventSenderBindings::SetMouseButtonState(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number;
  if (!args->GetNext(&button_number)) {
    args->ThrowError();
    return;
  }

  int modifiers = -1;  // Default to the modifier implied by button_number
  if (!args->PeekNext().IsEmpty()) {
    modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
  }

  sender_->SetMouseButtonState(button_number, modifiers);
}

void EventSenderBindings::KeyDown(gin::Arguments* args) {
  if (!sender_)
    return;

  std::string code_str;
  int modifiers = 0;
  int location = DOMKeyLocationStandard;
  args->GetNext(&code_str);
  if (!args->PeekNext().IsEmpty()) {
    v8::Local<v8::Value> value;
    args->GetNext(&value);
    modifiers = GetKeyModifiersFromV8(args->isolate(), value);
    if (!args->PeekNext().IsEmpty())
      args->GetNext(&location);
  }
  sender_->KeyDown(code_str, modifiers, static_cast<KeyLocationCode>(location));
}

bool EventSenderBindings::ForceLayoutOnEvents() const {
  if (sender_)
    return sender_->force_layout_on_events();
  return false;
}

void EventSenderBindings::SetForceLayoutOnEvents(bool force) {
  if (sender_)
    sender_->set_force_layout_on_events(force);
}

bool EventSenderBindings::IsDragMode() const {
  if (sender_)
    return sender_->is_drag_mode();
  return true;
}

void EventSenderBindings::SetIsDragMode(bool drag_mode) {
  if (sender_)
    sender_->set_is_drag_mode(drag_mode);
}

#if defined(OS_WIN)
int EventSenderBindings::WmKeyDown() const {
  if (sender_)
    return sender_->wm_key_down();
  return 0;
}

void EventSenderBindings::SetWmKeyDown(int key_down) {
  if (sender_)
    sender_->set_wm_key_down(key_down);
}

int EventSenderBindings::WmKeyUp() const {
  if (sender_)
    return sender_->wm_key_up();
  return 0;
}

void EventSenderBindings::SetWmKeyUp(int key_up) {
  if (sender_)
    sender_->set_wm_key_up(key_up);
}

int EventSenderBindings::WmChar() const {
  if (sender_)
    return sender_->wm_char();
  return 0;
}

void EventSenderBindings::SetWmChar(int wm_char) {
  if (sender_)
    sender_->set_wm_char(wm_char);
}

int EventSenderBindings::WmDeadChar() const {
  if (sender_)
    return sender_->wm_dead_char();
  return 0;
}

void EventSenderBindings::SetWmDeadChar(int dead_char) {
  if (sender_)
    sender_->set_wm_dead_char(dead_char);
}

int EventSenderBindings::WmSysKeyDown() const {
  if (sender_)
    return sender_->wm_sys_key_down();
  return 0;
}

void EventSenderBindings::SetWmSysKeyDown(int key_down) {
  if (sender_)
    sender_->set_wm_sys_key_down(key_down);
}

int EventSenderBindings::WmSysKeyUp() const {
  if (sender_)
    return sender_->wm_sys_key_up();
  return 0;
}

void EventSenderBindings::SetWmSysKeyUp(int key_up) {
  if (sender_)
    sender_->set_wm_sys_key_up(key_up);
}

int EventSenderBindings::WmSysChar() const {
  if (sender_)
    return sender_->wm_sys_char();
  return 0;
}

void EventSenderBindings::SetWmSysChar(int sys_char) {
  if (sender_)
    sender_->set_wm_sys_char(sys_char);
}

int EventSenderBindings::WmSysDeadChar() const {
  if (sender_)
    return sender_->wm_sys_dead_char();
  return 0;
}

void EventSenderBindings::SetWmSysDeadChar(int sys_dead_char) {
  if (sender_)
    sender_->set_wm_sys_dead_char(sys_dead_char);
}
#endif

// EventSender -----------------------------------------------------------------

WebMouseEvent::Button EventSender::last_button_type_ =
    WebMouseEvent::Button::NoButton;

EventSender::SavedEvent::SavedEvent()
    : type(TYPE_UNSPECIFIED),
      button_type(WebMouseEvent::Button::NoButton),
      milliseconds(0),
      modifiers(0) {}

EventSender::EventSender(WebWidgetTestProxyBase* web_widget_test_proxy_base)
    : web_widget_test_proxy_base_(web_widget_test_proxy_base),
      replaying_saved_events_(false),
      weak_factory_(this) {
  Reset();
}

EventSender::~EventSender() {}

void EventSender::Reset() {
  DCHECK(current_drag_data_.isNull());
  current_drag_data_.reset();
  current_drag_effect_ = blink::WebDragOperationNone;
  current_drag_effects_allowed_ = blink::WebDragOperationNone;
  if (widget() &&
      current_pointer_state_[kRawMousePointerId].pressed_button_ !=
          WebMouseEvent::Button::NoButton)
    widget()->mouseCaptureLost();
  current_pointer_state_.clear();
  is_drag_mode_ = true;
  force_layout_on_events_ = true;

#if defined(OS_WIN)
  wm_key_down_ = WM_KEYDOWN;
  wm_key_up_ = WM_KEYUP;
  wm_char_ = WM_CHAR;
  wm_dead_char_ = WM_DEADCHAR;
  wm_sys_key_down_ = WM_SYSKEYDOWN;
  wm_sys_key_up_ = WM_SYSKEYUP;
  wm_sys_char_ = WM_SYSCHAR;
  wm_sys_dead_char_ = WM_SYSDEADCHAR;
#endif

  last_click_time_sec_ = 0;
  last_click_pos_ = WebPoint(0, 0);
  last_button_type_ = WebMouseEvent::Button::NoButton;
  touch_points_.clear();
  last_context_menu_data_.reset();
  weak_factory_.InvalidateWeakPtrs();
  current_gesture_location_ = WebPoint(0, 0);
  mouse_event_queue_.clear();

  time_offset_ms_ = 0;
  click_count_ = 0;

  touch_modifiers_ = 0;
  touch_cancelable_ = true;
  touch_points_.clear();
}

void EventSender::Install(WebLocalFrame* frame) {
  EventSenderBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void EventSender::SetContextMenuData(const WebContextMenuData& data) {
  last_context_menu_data_.reset(new WebContextMenuData(data));
}

int EventSender::ModifiersForPointer(int pointer_id) {
  return modifiersWithButtons(
      current_pointer_state_[pointer_id].modifiers_,
      current_pointer_state_[pointer_id].current_buttons_);
}

void EventSender::DoDragDrop(const WebDragData& drag_data,
                              WebDragOperationsMask mask) {
  WebMouseEvent raw_event(WebInputEvent::MouseDown,
                          ModifiersForPointer(kRawMousePointerId),
                          GetCurrentEventTimeSec());
  InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &raw_event);

  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebMouseEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;

  WebPoint client_point(event->x, event->y);
  WebPoint screen_point(event->globalX, event->globalY);
  current_drag_data_ = drag_data;
  current_drag_effects_allowed_ = mask;
  current_drag_effect_ = mainFrameWidget()->dragTargetDragEnter(
      drag_data, client_point, screen_point, current_drag_effects_allowed_,
      modifiersWithButtons(
          current_pointer_state_[kRawMousePointerId].modifiers_,
          current_pointer_state_[kRawMousePointerId].current_buttons_));

  // Finish processing events.
  ReplaySavedEvents();
}

void EventSender::MouseDown(int button_number, int modifiers) {
  PointerDown(button_number, modifiers,
              WebPointerProperties::PointerType::Mouse, kRawMousePointerId, 0.0,
              0, 0);
}

void EventSender::MouseUp(int button_number, int modifiers) {
  PointerUp(button_number, modifiers, WebPointerProperties::PointerType::Mouse,
            kRawMousePointerId, 0.0, 0, 0);
}

void EventSender::PointerDown(int button_number,
                              int modifiers,
                              WebPointerProperties::PointerType pointerType,
                              int pointerId,
                              float pressure,
                              int tiltX,
                              int tiltY) {
  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  DCHECK_NE(-1, button_number);

  WebMouseEvent::Button button_type =
      GetButtonTypeFromButtonNumber(button_number);

  int click_count = 0;
  current_pointer_state_[pointerId].pressed_button_ = button_type;
  current_pointer_state_[pointerId].current_buttons_ |=
      GetWebMouseEventModifierForButton(button_type);
  current_pointer_state_[pointerId].modifiers_ = modifiers;

  if (pointerType == WebPointerProperties::PointerType::Mouse) {
    UpdateClickCountForButton(button_type);
    click_count = click_count_;
  }
  WebMouseEvent event(WebInputEvent::MouseDown, ModifiersForPointer(pointerId),
                      GetCurrentEventTimeSec());
  InitMouseEventGeneric(current_pointer_state_[pointerId].pressed_button_,
                        current_pointer_state_[pointerId].current_buttons_,
                        current_pointer_state_[pointerId].last_pos_,
                        click_count, pointerType, pointerId, pressure, tiltX,
                        tiltY, &event);

  HandleInputEventOnViewOrPopup(event);
}

void EventSender::PointerUp(int button_number,
                            int modifiers,
                            WebPointerProperties::PointerType pointerType,
                            int pointerId,
                            float pressure,
                            int tiltX,
                            int tiltY) {
  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  DCHECK_NE(-1, button_number);

  WebMouseEvent::Button button_type =
      GetButtonTypeFromButtonNumber(button_number);

  if (pointerType == WebPointerProperties::PointerType::Mouse &&
      is_drag_mode_ && !replaying_saved_events_) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_MOUSE_UP;
    saved_event.button_type = button_type;
    saved_event.modifiers = modifiers;
    mouse_event_queue_.push_back(saved_event);
    ReplaySavedEvents();
  } else {
    current_pointer_state_[pointerId].modifiers_ = modifiers;
    current_pointer_state_[pointerId].current_buttons_ &=
        ~GetWebMouseEventModifierForButton(button_type);
    current_pointer_state_[pointerId].pressed_button_ =
        WebMouseEvent::Button::NoButton;

    WebMouseEvent event(WebInputEvent::MouseUp, ModifiersForPointer(pointerId),
                        GetCurrentEventTimeSec());
    int click_count = pointerType == WebPointerProperties::PointerType::Mouse
                          ? click_count_
                          : 0;
    InitMouseEventGeneric(
        button_type, current_pointer_state_[pointerId].current_buttons_,
        current_pointer_state_[pointerId].last_pos_, click_count, pointerType,
        pointerId, pressure, tiltX, tiltY, &event);
    HandleInputEventOnViewOrPopup(event);
    if (pointerType == WebPointerProperties::PointerType::Mouse)
      DoDragAfterMouseUp(event);
  }
}

void EventSender::SetMouseButtonState(int button_number, int modifiers) {
  current_pointer_state_[kRawMousePointerId].pressed_button_ =
      GetButtonTypeFromButtonNumber(button_number);
  current_pointer_state_[kRawMousePointerId].current_buttons_ =
      (modifiers == -1)
          ? GetWebMouseEventModifierForButton(
                current_pointer_state_[kRawMousePointerId].pressed_button_)
          : modifiers & kButtonsInModifiers;
}

void EventSender::KeyDown(const std::string& code_str,
                          int modifiers,
                          KeyLocationCode location) {
  // FIXME: I'm not exactly sure how we should convert the string to a key
  // event. This seems to work in the cases I tested.
  // FIXME: Should we also generate a KEY_UP?

  bool generate_char = false;

  // Convert \n -> VK_RETURN. Some layout tests use \n to mean "Enter", when
  // Windows uses \r for "Enter".
  int code = 0;
  int text = 0;
  bool needs_shift_key_modifier = false;
  std::string domKeyString;
  std::string domCodeString;

  if ("Enter" == code_str) {
    generate_char = true;
    text = code = ui::VKEY_RETURN;
    domKeyString.assign("Enter");
    domCodeString.assign("Enter");
  } else if ("ArrowRight" == code_str) {
    code = ui::VKEY_RIGHT;
    domKeyString.assign("ArrowRight");
    domCodeString.assign("ArrowRight");
  } else if ("ArrowDown" == code_str) {
    code = ui::VKEY_DOWN;
    domKeyString.assign("ArrowDown");
    domCodeString.assign("ArrowDown");
  } else if ("ArrowLeft" == code_str) {
    code = ui::VKEY_LEFT;
    domKeyString.assign("ArrowLeft");
    domCodeString.assign("ArrowLeft");
  } else if ("ArrowUp" == code_str) {
    code = ui::VKEY_UP;
    domKeyString.assign("ArrowUp");
    domCodeString.assign("ArrowUp");
  } else if ("Insert" == code_str) {
    code = ui::VKEY_INSERT;
    domKeyString.assign("Insert");
    domCodeString.assign("Insert");
  } else if ("Delete" == code_str) {
    code = ui::VKEY_DELETE;
    domKeyString.assign("Delete");
    domCodeString.assign("Delete");
  } else if ("PageUp" == code_str) {
    code = ui::VKEY_PRIOR;
    domKeyString.assign("PageUp");
    domCodeString.assign("PageUp");
  } else if ("PageDown" == code_str) {
    code = ui::VKEY_NEXT;
    domKeyString.assign("PageDown");
    domCodeString.assign("PageDown");
  } else if ("Home" == code_str) {
    code = ui::VKEY_HOME;
    domKeyString.assign("Home");
    domCodeString.assign("Home");
  } else if ("End" == code_str) {
    code = ui::VKEY_END;
    domKeyString.assign("End");
    domCodeString.assign("End");
  } else if ("PrintScreen" == code_str) {
    code = ui::VKEY_SNAPSHOT;
    domKeyString.assign("PrintScreen");
    domCodeString.assign("PrintScreen");
  } else if ("ContextMenu" == code_str) {
    code = ui::VKEY_APPS;
    domKeyString.assign("ContextMenu");
    domCodeString.assign("ContextMenu");
  } else if ("ControlLeft" == code_str) {
    code = ui::VKEY_CONTROL;
    domKeyString.assign("Control");
    domCodeString.assign("ControlLeft");
    location = DOMKeyLocationLeft;
  } else if ("ControlRight" == code_str) {
    code = ui::VKEY_CONTROL;
    domKeyString.assign("Control");
    domCodeString.assign("ControlRight");
    location = DOMKeyLocationRight;
  } else if ("ShiftLeft" == code_str) {
    code = ui::VKEY_SHIFT;
    domKeyString.assign("Shift");
    domCodeString.assign("ShiftLeft");
    location = DOMKeyLocationLeft;
  } else if ("ShiftRight" == code_str) {
    code = ui::VKEY_SHIFT;
    domKeyString.assign("Shift");
    domCodeString.assign("ShiftRight");
    location = DOMKeyLocationRight;
  } else if ("AltLeft" == code_str) {
    code = ui::VKEY_MENU;
    domKeyString.assign("Alt");
    domCodeString.assign("AltLeft");
    location = DOMKeyLocationLeft;
  } else if ("AltRight" == code_str) {
    code = ui::VKEY_MENU;
    domKeyString.assign("Alt");
    domCodeString.assign("AltRight");
    location = DOMKeyLocationRight;
  } else if ("NumLock" == code_str) {
    code = ui::VKEY_NUMLOCK;
    domKeyString.assign("NumLock");
    domCodeString.assign("NumLock");
  } else if ("Backspace" == code_str) {
    code = ui::VKEY_BACK;
    domKeyString.assign("Backspace");
    domCodeString.assign("Backspace");
  } else if ("Escape" == code_str) {
    code = ui::VKEY_ESCAPE;
    domKeyString.assign("Escape");
    domCodeString.assign("Escape");
  } else if ("Tab" == code_str) {
    code = ui::VKEY_TAB;
    domKeyString.assign("Tab");
    domCodeString.assign("Tab");
  } else if ("Cut" == code_str || "Copy" == code_str || "Paste" == code_str) {
    // No valid KeyboardCode for Cut/Copy/Paste.
    code = 0;
    domKeyString.assign(code_str);
    // It's OK to assign the same string as the DomCode strings happens to be
    // the same for these keys.
    domCodeString.assign(code_str);
  } else {
    // Compare the input string with the function-key names defined by the
    // DOM spec (i.e. "F1",...,"F24"). If the input string is a function-key
    // name, set its key code.
    for (int i = 1; i <= 24; ++i) {
      std::string function_key_name = base::StringPrintf("F%d", i);
      if (function_key_name == code_str) {
        code = ui::VKEY_F1 + (i - 1);
        domKeyString = function_key_name;
        domCodeString = function_key_name;
        break;
      }
    }
    if (!code) {
      base::string16 code_str16 = base::UTF8ToUTF16(code_str);
      if (code_str16.size() != 1u) {
        v8::Isolate* isolate = blink::mainThreadIsolate();
        isolate->ThrowException(v8::Exception::TypeError(
            gin::StringToV8(isolate, "Invalid web code.")));
        return;
      }
      text = code = code_str16[0];
      needs_shift_key_modifier = base::IsAsciiUpper(code & 0xFF);
      if (base::IsAsciiLower(code & 0xFF))
        code -= 'a' - 'A';
      if (base::IsAsciiAlpha(code)) {
        domKeyString.assign(code_str);
        domCodeString.assign("Key");
        domCodeString.push_back(
            base::ToUpperASCII(static_cast<base::char16>(code)));
      } else if (base::IsAsciiDigit(code)) {
        domKeyString.assign(code_str);
        domCodeString.assign("Digit");
        domCodeString.push_back(code);
      } else if (code == ' ') {
        domKeyString.assign(code_str);
        domCodeString.assign("Space");
      } else if (code == 9) {
        domKeyString.assign("Tab");
        domCodeString.assign("Tab");
      }
      generate_char = true;
    }

    if ("(" == code_str) {
      code = '9';
      needs_shift_key_modifier = true;
      domKeyString.assign("(");
      domCodeString.assign("Digit9");
    }
  }

  if (needs_shift_key_modifier)
    modifiers |= WebInputEvent::ShiftKey;

  // See if KeyLocation argument is given.
  switch (location) {
    case DOMKeyLocationStandard:
      break;
    case DOMKeyLocationLeft:
      modifiers |= WebInputEvent::IsLeft;
      break;
    case DOMKeyLocationRight:
      modifiers |= WebInputEvent::IsRight;
      break;
    case DOMKeyLocationNumpad:
      modifiers |= WebInputEvent::IsKeyPad;
      break;
  }

  // For one generated keyboard event, we need to generate a keyDown/keyUp
  // pair;
  // On Windows, we might also need to generate a char event to mimic the
  // Windows event flow; on other platforms we create a merged event and test
  // the event flow that that platform provides.
  WebKeyboardEvent event_down(WebInputEvent::RawKeyDown, modifiers,
                              GetCurrentEventTimeSec());
  event_down.windowsKeyCode = code;
  event_down.domKey = static_cast<int>(
      ui::KeycodeConverter::KeyStringToDomKey(domKeyString));
  event_down.domCode = static_cast<int>(
      ui::KeycodeConverter::CodeStringToDomCode(domCodeString));

  if (generate_char) {
    event_down.text[0] = text;
    event_down.unmodifiedText[0] = text;
  }

  if (event_down.modifiers() != 0)
    event_down.isSystemKey = IsSystemKeyEvent(event_down);

  WebKeyboardEvent event_up = event_down;
  event_up.setType(WebInputEvent::KeyUp);
  // EventSender.m forces a layout here, with at least one
  // test (fast/forms/focus-control-to-page.html) relying on this.
  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  // In the browser, if a keyboard event corresponds to an editor command,
  // the command will be dispatched to the renderer just before dispatching
  // the keyboard event, and then it will be executed in the
  // RenderView::handleCurrentKeyboardEvent() method.
  // We just simulate the same behavior here.
  std::string edit_command;
  if (GetEditCommand(event_down, &edit_command))
    delegate()->SetEditCommand(edit_command, "");

  HandleInputEventOnViewOrPopup(event_down);

  if (code == ui::VKEY_ESCAPE && !current_drag_data_.isNull()) {
    WebMouseEvent event(WebInputEvent::MouseDown,
                        ModifiersForPointer(kRawMousePointerId),
                        GetCurrentEventTimeSec());
    InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                   current_pointer_state_[kRawMousePointerId].current_buttons_,
                   current_pointer_state_[kRawMousePointerId].last_pos_,
                   click_count_, &event);
    FinishDragAndDrop(event, blink::WebDragOperationNone);
  }

  delegate()->ClearEditCommand();

  if (generate_char) {
    WebKeyboardEvent event_char = event_up;
    event_char.setType(WebInputEvent::Char);
    HandleInputEventOnViewOrPopup(event_char);
  }

  HandleInputEventOnViewOrPopup(event_up);
}

void EventSender::EnableDOMUIEventLogging() {}

void EventSender::FireKeyboardEventsToElement() {}

void EventSender::ClearKillRing() {}

std::vector<std::string> EventSender::ContextClick() {
  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  UpdateClickCountForButton(WebMouseEvent::Button::Right);

  // Clears last context menu data because we need to know if the context menu
  // be requested after following mouse events.
  last_context_menu_data_.reset();

  // Generate right mouse down and up.
  // This is a hack to work around only allowing a single pressed button since
  // we want to test the case where both the left and right mouse buttons are
  // pressed.
  // TODO(mustaq): This hack seems unused here! But do we need this hack at all
  //   after adding current_buttons_.
  if (current_pointer_state_[kRawMousePointerId].pressed_button_ ==
      WebMouseEvent::Button::NoButton) {
    current_pointer_state_[kRawMousePointerId].pressed_button_ =
        WebMouseEvent::Button::Right;
    current_pointer_state_[kRawMousePointerId].current_buttons_ |=
        GetWebMouseEventModifierForButton(
            current_pointer_state_[kRawMousePointerId].pressed_button_);
  }
  WebMouseEvent event(WebInputEvent::MouseDown,
                      ModifiersForPointer(kRawMousePointerId),
                      GetCurrentEventTimeSec());
  InitMouseEvent(WebMouseEvent::Button::Right,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &event);
  HandleInputEventOnViewOrPopup(event);

#if defined(OS_WIN)
  current_pointer_state_[kRawMousePointerId].current_buttons_ &=
      ~GetWebMouseEventModifierForButton(WebMouseEvent::Button::Right);
  current_pointer_state_[kRawMousePointerId].pressed_button_ =
      WebMouseEvent::Button::NoButton;

  WebMouseEvent mouseUpEvent(WebInputEvent::MouseUp,
                             ModifiersForPointer(kRawMousePointerId),
                             GetCurrentEventTimeSec());
  InitMouseEvent(WebMouseEvent::Button::Right,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &mouseUpEvent);
  HandleInputEventOnViewOrPopup(mouseUpEvent);
#endif

  std::vector<std::string> menu_items =
      MakeMenuItemStringsFor(last_context_menu_data_.get(), delegate());
  last_context_menu_data_.reset();
  return menu_items;
}

void EventSender::TextZoomIn() {
  view()->setTextZoomFactor(view()->textZoomFactor() * 1.2f);
}

void EventSender::TextZoomOut() {
  view()->setTextZoomFactor(view()->textZoomFactor() / 1.2f);
}

void EventSender::ZoomPageIn() {
  const std::vector<WebViewTestProxyBase*>& window_list =
      interfaces()->GetWindowList();

  for (size_t i = 0; i < window_list.size(); ++i) {
    window_list.at(i)->web_view()->setZoomLevel(
        window_list.at(i)->web_view()->zoomLevel() + 1);
  }
}

void EventSender::ZoomPageOut() {
  const std::vector<WebViewTestProxyBase*>& window_list =
      interfaces()->GetWindowList();

  for (size_t i = 0; i < window_list.size(); ++i) {
    window_list.at(i)->web_view()->setZoomLevel(
        window_list.at(i)->web_view()->zoomLevel() - 1);
  }
}

void EventSender::SetPageZoomFactor(double zoom_factor) {
  const std::vector<WebViewTestProxyBase*>& window_list =
      interfaces()->GetWindowList();

  for (size_t i = 0; i < window_list.size(); ++i) {
    window_list.at(i)->web_view()->setZoomLevel(std::log(zoom_factor) /
                                                std::log(1.2));
  }
}

void EventSender::ClearTouchPoints() {
  touch_points_.clear();
}

void EventSender::ThrowTouchPointError() {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  isolate->ThrowException(v8::Exception::TypeError(
      gin::StringToV8(isolate, "Invalid touch point.")));
}

void EventSender::ReleaseTouchPoint(unsigned index) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::StateReleased;
}

void EventSender::UpdateTouchPoint(unsigned index,
                                   float x,
                                   float y,
                                   gin::Arguments* args) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::StateMoved;
  touch_point->position = WebFloatPoint(x, y);
  touch_point->screenPosition = touch_point->position;

  InitPointerProperties(args, touch_point, &touch_point->radiusX,
                        &touch_point->radiusY);
}

void EventSender::CancelTouchPoint(unsigned index) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::StateCancelled;
}

void EventSender::SetTouchModifier(const std::string& key_name,
                                    bool set_mask) {
  int mask = GetKeyModifier(key_name);

  if (set_mask)
    touch_modifiers_ |= mask;
  else
    touch_modifiers_ &= ~mask;
}

void EventSender::SetTouchCancelable(bool cancelable) {
  touch_cancelable_ = cancelable;
}

void EventSender::DumpFilenameBeingDragged() {
  if (current_drag_data_.isNull())
    return;

  WebString filename;
  WebVector<WebDragData::Item> items = current_drag_data_.items();
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].storageType == WebDragData::Item::StorageTypeBinaryData) {
      filename = items[i].title;
      break;
    }
  }
  delegate()->PrintMessage(std::string("Filename being dragged: ") +
                           filename.utf8().data() + "\n");
}

void EventSender::GestureFlingCancel() {
  WebGestureEvent event(WebInputEvent::GestureFlingCancel,
                        WebInputEvent::NoModifiers, GetCurrentEventTimeSec());
  // Generally it won't matter what device we use here, and since it might
  // be cumbersome to expect all callers to specify a device, we'll just
  // choose Touchpad here.
  event.sourceDevice = blink::WebGestureDeviceTouchpad;

  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  HandleInputEventOnViewOrPopup(event);
}

void EventSender::GestureFlingStart(float x,
                                    float y,
                                    float velocity_x,
                                    float velocity_y,
                                    gin::Arguments* args) {
  WebGestureEvent event(WebInputEvent::GestureFlingStart,
                        WebInputEvent::NoModifiers, GetCurrentEventTimeSec());

  std::string device_string;
  if (!args->PeekNext().IsEmpty() && args->PeekNext()->IsString())
    args->GetNext(&device_string);

  if (device_string == kSourceDeviceStringTouchpad) {
    event.sourceDevice = blink::WebGestureDeviceTouchpad;
  } else if (device_string == kSourceDeviceStringTouchscreen) {
    event.sourceDevice = blink::WebGestureDeviceTouchscreen;
  } else {
    args->ThrowError();
    return;
  }

  float max_start_velocity = std::max(fabs(velocity_x), fabs(velocity_y));
  if (!max_start_velocity) {
    v8::Isolate* isolate = blink::mainThreadIsolate();
    isolate->ThrowException(v8::Exception::TypeError(
      gin::StringToV8(isolate, "Invalid max start velocity.")));
    return;
  }

  event.x = x;
  event.y = y;
  event.globalX = event.x;
  event.globalY = event.y;

  event.data.flingStart.velocityX = velocity_x;
  event.data.flingStart.velocityY = velocity_y;

  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  HandleInputEventOnViewOrPopup(event);
}

bool EventSender::IsFlinging() const {
  return view()->isFlinging();
}

void EventSender::GestureScrollFirstPoint(int x, int y) {
  current_gesture_location_ = WebPoint(x, y);
}

void EventSender::TouchStart(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::TouchStart, args);
}

void EventSender::TouchMove(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::TouchMove, args);
}

void EventSender::TouchCancel(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::TouchCancel, args);
}

void EventSender::TouchEnd(gin::Arguments* args) {
  SendCurrentTouchEvent(WebInputEvent::TouchEnd, args);
}

void EventSender::NotifyStartOfTouchScroll() {
  WebTouchEvent event(WebInputEvent::TouchScrollStarted,
                      WebInputEvent::NoModifiers, GetCurrentEventTimeSec());
  HandleInputEventOnViewOrPopup(event);
}

void EventSender::LeapForward(int milliseconds) {
  if (is_drag_mode_ &&
      current_pointer_state_[kRawMousePointerId].pressed_button_ ==
          WebMouseEvent::Button::Left &&
      !replaying_saved_events_) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_LEAP_FORWARD;
    saved_event.milliseconds = milliseconds;
    mouse_event_queue_.push_back(saved_event);
  } else {
    DoLeapForward(milliseconds);
  }
}

void EventSender::BeginDragWithFiles(const std::vector<std::string>& files) {
  if (!current_drag_data_.isNull()) {
    // Nested dragging not supported, fuzzer code a likely culprit.
    // Cancel the current drag operation and throw an error.
    KeyDown("Escape", 0, DOMKeyLocationStandard);
    v8::Isolate* isolate = blink::mainThreadIsolate();
    isolate->ThrowException(v8::Exception::Error(
        gin::StringToV8(isolate,
                        "Nested beginDragWithFiles() not supported.")));
    return;
  }
  current_drag_data_.initialize();
  WebVector<WebString> absolute_filenames(files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeFilename;
    item.filenameData = delegate()->GetAbsoluteWebStringFromUTF8Path(files[i]);
    current_drag_data_.addItem(item);
    absolute_filenames[i] = item.filenameData;
  }
  current_drag_data_.setFilesystemId(
      delegate()->RegisterIsolatedFileSystem(absolute_filenames));
  current_drag_effects_allowed_ = blink::WebDragOperationCopy;

  const WebPoint& last_pos =
      current_pointer_state_[kRawMousePointerId].last_pos_;
  float scale = delegate()->GetWindowToViewportScale();
  WebPoint scaled_last_pos(last_pos.x * scale, last_pos.y * scale);

  // Provide a drag source.
  mainFrameWidget()->dragTargetDragEnter(
      current_drag_data_, scaled_last_pos, scaled_last_pos,
      current_drag_effects_allowed_, 0);
  // |is_drag_mode_| saves events and then replays them later. We don't
  // need/want that.
  is_drag_mode_ = false;

  // Make the rest of eventSender think a drag is in progress.
  current_pointer_state_[kRawMousePointerId].pressed_button_ =
      WebMouseEvent::Button::Left;
  current_pointer_state_[kRawMousePointerId].current_buttons_ |=
      GetWebMouseEventModifierForButton(
          current_pointer_state_[kRawMousePointerId].pressed_button_);
}

void EventSender::AddTouchPoint(float x, float y, gin::Arguments* args) {
  WebTouchPoint touch_point;
  touch_point.pointerType = WebPointerProperties::PointerType::Touch;
  touch_point.state = WebTouchPoint::StatePressed;
  touch_point.position = WebFloatPoint(x, y);
  touch_point.screenPosition = touch_point.position;

  int highest_id = -1;
  for (size_t i = 0; i < touch_points_.size(); i++) {
    if (touch_points_[i].id > highest_id)
      highest_id = touch_points_[i].id;
  }
  touch_point.id = highest_id + 1;

  InitPointerProperties(args, &touch_point, &touch_point.radiusX,
                        &touch_point.radiusY);

  // Set the touch point pressure to zero if it was not set by the caller
  if (std::isnan(touch_point.force))
      touch_point.force = 0.0;

  touch_points_.push_back(touch_point);
}

void EventSender::GestureScrollBegin(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureScrollBegin, args);
}

void EventSender::GestureScrollEnd(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureScrollEnd, args);
}

void EventSender::GestureScrollUpdate(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureScrollUpdate, args);
}

void EventSender::GesturePinchBegin(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GesturePinchBegin, args);
}

void EventSender::GesturePinchEnd(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GesturePinchEnd, args);
}

void EventSender::GesturePinchUpdate(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GesturePinchUpdate, args);
}

void EventSender::GestureTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureTap, args);
}

void EventSender::GestureTapDown(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureTapDown, args);
}

void EventSender::GestureShowPress(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureShowPress, args);
}

void EventSender::GestureTapCancel(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureTapCancel, args);
}

void EventSender::GestureLongPress(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureLongPress, args);
}

void EventSender::GestureLongTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureLongTap, args);
}

void EventSender::GestureTwoFingerTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureTwoFingerTap, args);
}

void EventSender::MouseScrollBy(gin::Arguments* args,
                                MouseScrollType scroll_type) {
  // TODO(dtapuska): Gestures really should be sent by the MouseWheelEventQueue
  // class in the browser. But since the event doesn't propogate up into
  // the browser generate the events here. See crbug.com/596095.
  bool send_gestures = true;
  WebMouseWheelEvent wheel_event =
      GetMouseWheelEvent(args, scroll_type, &send_gestures);
  if (wheel_event.type() != WebInputEvent::Undefined &&
      HandleInputEventOnViewOrPopup(wheel_event) ==
          WebInputEventResult::NotHandled &&
      send_gestures) {
    SendGesturesForMouseWheelEvent(wheel_event);
  }
}

void EventSender::MouseMoveTo(gin::Arguments* args) {
  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  double x;
  double y;
  if (!args->GetNext(&x) || !args->GetNext(&y)) {
    args->ThrowError();
    return;
  }
  WebPoint mouse_pos(static_cast<int>(x), static_cast<int>(y));

  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    modifiers = GetKeyModifiersFromV8(args->isolate(), args->PeekNext());
    args->Skip();
  }

  WebPointerProperties::PointerType pointerType =
      WebPointerProperties::PointerType::Mouse;
  int pointerId = 0;
  float pressure = 0;
  int tiltX = 0;
  int tiltY = 0;
  if (!getMousePenPointerProperties(args, pointerType, pointerId, pressure,
                                    tiltX, tiltY))
    return;

  if (pointerType == WebPointerProperties::PointerType::Mouse &&
      is_drag_mode_ && !replaying_saved_events_ &&
      current_pointer_state_[kRawMousePointerId].pressed_button_ ==
          WebMouseEvent::Button::Left) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_MOUSE_MOVE;
    saved_event.pos = mouse_pos;
    saved_event.modifiers = modifiers;
    mouse_event_queue_.push_back(saved_event);
  } else {
    current_pointer_state_[pointerId].last_pos_ = mouse_pos;
    current_pointer_state_[pointerId].modifiers_ = modifiers;
    WebMouseEvent event(WebInputEvent::MouseMove,
                        ModifiersForPointer(pointerId),
                        GetCurrentEventTimeSec());
    int click_count = pointerType == WebPointerProperties::PointerType::Mouse
                          ? click_count_
                          : 0;
    InitMouseEventGeneric(
        current_pointer_state_[kRawMousePointerId].pressed_button_,
        current_pointer_state_[kRawMousePointerId].current_buttons_, mouse_pos,
        click_count, pointerType, pointerId, pressure, tiltX, tiltY, &event);
    HandleInputEventOnViewOrPopup(event);
    if (pointerType == WebPointerProperties::PointerType::Mouse)
      DoDragAfterMouseMove(event);
  }
}

void EventSender::MouseLeave() {
  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  WebMouseEvent event(WebInputEvent::MouseLeave,
                      ModifiersForPointer(kRawMousePointerId),
                      GetCurrentEventTimeSec());
  InitMouseEvent(WebMouseEvent::Button::NoButton, 0,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &event);
  HandleInputEventOnViewOrPopup(event);
}


void EventSender::ScheduleAsynchronousClick(int button_number, int modifiers) {
  delegate()->PostTask(base::Bind(&EventSender::MouseDown,
                                  weak_factory_.GetWeakPtr(), button_number,
                                  modifiers));
  delegate()->PostTask(base::Bind(&EventSender::MouseUp,
                                  weak_factory_.GetWeakPtr(), button_number,
                                  modifiers));
}

void EventSender::ScheduleAsynchronousKeyDown(const std::string& code_str,
                                              int modifiers,
                                              KeyLocationCode location) {
  delegate()->PostTask(base::Bind(&EventSender::KeyDown,
                                  weak_factory_.GetWeakPtr(), code_str,
                                  modifiers, location));
}

double EventSender::GetCurrentEventTimeSec() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF() +
         time_offset_ms_ / 1000.0;
}

void EventSender::DoLeapForward(int milliseconds) {
  time_offset_ms_ += milliseconds;
}

void EventSender::GetOptionalTouchArgs(gin::Arguments* args,
                                       bool& moved_beyond_slop_region,
                                       uint32_t& unique_touch_event_id) {
  moved_beyond_slop_region = false;
  if(!args->PeekNext().IsEmpty() && args->PeekNext()->IsString()) {
    std::string arg;
    if (args->GetNext(&arg) && arg == "movedBeyondSlopRegion")
      moved_beyond_slop_region = true;
    else
      args->ThrowError();
  }

  unique_touch_event_id = GetUniqueTouchEventId(args);
  return;
}

uint32_t EventSender::GetUniqueTouchEventId(gin::Arguments* args) {
  uint32_t unique_touch_event_id;
  if(!args->PeekNext().IsEmpty() && args->GetNext(&unique_touch_event_id))
    return unique_touch_event_id;

  return 0;
}

void EventSender::SendCurrentTouchEvent(WebInputEvent::Type type,
                                        gin::Arguments* args) {
  bool moved_beyond_slop_region;
  uint32_t unique_touch_event_id;
  GetOptionalTouchArgs(args, moved_beyond_slop_region, unique_touch_event_id);

  DCHECK_GT(static_cast<unsigned>(WebTouchEvent::kTouchesLengthCap),
            touch_points_.size());
  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  WebTouchEvent touch_event(type, touch_modifiers_, GetCurrentEventTimeSec());
  touch_event.dispatchType = touch_cancelable_
                                 ? WebInputEvent::Blocking
                                 : WebInputEvent::EventNonBlocking;
  touch_event.movedBeyondSlopRegion = moved_beyond_slop_region;
  touch_event.uniqueTouchEventId = unique_touch_event_id;
  touch_event.touchesLength = touch_points_.size();
  for (size_t i = 0; i < touch_points_.size(); ++i)
    touch_event.touches[i] = touch_points_[i];
  HandleInputEventOnViewOrPopup(touch_event);

  for (size_t i = 0; i < touch_points_.size(); ++i) {
    WebTouchPoint* touch_point = &touch_points_[i];
    if (touch_point->state == WebTouchPoint::StateReleased
        || touch_point->state == WebTouchPoint::StateCancelled) {
      touch_points_.erase(touch_points_.begin() + i);
      --i;
    } else {
      touch_point->state = WebTouchPoint::StateStationary;
    }
  }
}

void EventSender::GestureEvent(WebInputEvent::Type type,
                               gin::Arguments* args) {
  WebGestureEvent event(type, WebInputEvent::NoModifiers,
                        GetCurrentEventTimeSec());

  // If the first argument is a string, it is to specify the device, otherwise
  // the device is assumed to be a touchscreen (since most tests were written
  // assuming this).
  event.sourceDevice = blink::WebGestureDeviceTouchscreen;
  if (!args->PeekNext().IsEmpty() && args->PeekNext()->IsString()) {
    std::string device_string;
    if (!args->GetNext(&device_string)) {
      args->ThrowError();
      return;
    }
    if (device_string == kSourceDeviceStringTouchpad) {
      event.sourceDevice = blink::WebGestureDeviceTouchpad;
    } else if (device_string == kSourceDeviceStringTouchscreen) {
      event.sourceDevice = blink::WebGestureDeviceTouchscreen;
    } else {
      args->ThrowError();
      return;
    }
  }

  double x;
  double y;
  if (!args->GetNext(&x) || !args->GetNext(&y)) {
    args->ThrowError();
    return;
  }

  switch (type) {
    case WebInputEvent::GestureScrollUpdate:
    {
      bool preventPropagation = false;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&preventPropagation)) {
          args->ThrowError();
          return;
        }
      }
      if (!GetScrollUnits(args, &event.data.scrollUpdate.deltaUnits))
        return;

      event.data.scrollUpdate.deltaX = static_cast<float>(x);
      event.data.scrollUpdate.deltaY = static_cast<float>(y);
      event.data.scrollUpdate.preventPropagation = preventPropagation;
      event.x = current_gesture_location_.x;
      event.y = current_gesture_location_.y;
      current_gesture_location_.x =
          current_gesture_location_.x + event.data.scrollUpdate.deltaX;
      current_gesture_location_.y =
          current_gesture_location_.y + event.data.scrollUpdate.deltaY;
      break;
    }
    case WebInputEvent::GestureScrollBegin:
      current_gesture_location_ = WebPoint(x, y);
      event.x = current_gesture_location_.x;
      event.y = current_gesture_location_.y;
      break;
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureFlingStart:
      event.x = current_gesture_location_.x;
      event.y = current_gesture_location_.y;
      break;
    case WebInputEvent::GesturePinchBegin:
    case WebInputEvent::GesturePinchEnd:
      current_gesture_location_ = WebPoint(x, y);
      event.x = current_gesture_location_.x;
      event.y = current_gesture_location_.y;
      break;
    case WebInputEvent::GesturePinchUpdate:
    {
      float scale = 1;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&scale)) {
          args->ThrowError();
          return;
        }
      }
      event.data.pinchUpdate.scale = scale;
      current_gesture_location_ = WebPoint(x, y);
      event.x = current_gesture_location_.x;
      event.y = current_gesture_location_.y;
      break;
    }
    case WebInputEvent::GestureTap:
    {
      float tap_count = 1;
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&tap_count)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&height)) {
          args->ThrowError();
          return;
        }
      }
      event.data.tap.tapCount = tap_count;
      event.data.tap.width = width;
      event.data.tap.height = height;
      event.x = x;
      event.y = y;
      break;
    }
    case WebInputEvent::GestureTapUnconfirmed:
      if (!args->PeekNext().IsEmpty()) {
        float tap_count;
        if (!args->GetNext(&tap_count)) {
          args->ThrowError();
          return;
        }
        event.data.tap.tapCount = tap_count;
      } else {
        event.data.tap.tapCount = 1;
      }
      event.x = x;
      event.y = y;
      break;
    case WebInputEvent::GestureTapDown:
    {
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&height)) {
          args->ThrowError();
          return;
        }
      }
      event.x = x;
      event.y = y;
      event.data.tapDown.width = width;
      event.data.tapDown.height = height;
      break;
    }
    case WebInputEvent::GestureShowPress:
    {
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
        if (!args->PeekNext().IsEmpty()) {
          if (!args->GetNext(&height)) {
            args->ThrowError();
            return;
          }
        }
      }
      event.x = x;
      event.y = y;
      event.data.showPress.width = width;
      event.data.showPress.height = height;
      break;
    }
    case WebInputEvent::GestureTapCancel:
      event.x = x;
      event.y = y;
      break;
    case WebInputEvent::GestureLongPress:
    case WebInputEvent::GestureLongTap:
      event.x = x;
      event.y = y;
      if (!args->PeekNext().IsEmpty()) {
        float width;
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
        event.data.longPress.width = width;
        if (!args->PeekNext().IsEmpty()) {
          float height;
          if (!args->GetNext(&height)) {
            args->ThrowError();
            return;
          }
          event.data.longPress.height = height;
        }
      }
      break;
    case WebInputEvent::GestureTwoFingerTap:
      event.x = x;
      event.y = y;
      if (!args->PeekNext().IsEmpty()) {
        float first_finger_width;
        if (!args->GetNext(&first_finger_width)) {
          args->ThrowError();
          return;
        }
        event.data.twoFingerTap.firstFingerWidth = first_finger_width;
        if (!args->PeekNext().IsEmpty()) {
          float first_finger_height;
          if (!args->GetNext(&first_finger_height)) {
            args->ThrowError();
            return;
          }
          event.data.twoFingerTap.firstFingerHeight = first_finger_height;
        }
      }
      break;
    default:
      NOTREACHED();
  }

  event.uniqueTouchEventId = GetUniqueTouchEventId(args);

  event.globalX = event.x;
  event.globalY = event.y;

  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  WebInputEventResult result = HandleInputEventOnViewOrPopup(event);

  // Long press might start a drag drop session. Complete it if so.
  if (type == WebInputEvent::GestureLongPress && !current_drag_data_.isNull()) {
    WebMouseEvent mouse_event(WebInputEvent::MouseDown,
                              ModifiersForPointer(kRawMousePointerId),
                              GetCurrentEventTimeSec());

    InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                   current_pointer_state_[kRawMousePointerId].current_buttons_,
                   WebPoint(x, y), click_count_, &mouse_event);

    FinishDragAndDrop(mouse_event, blink::WebDragOperationNone);
  }
  args->Return(result != WebInputEventResult::NotHandled);
}

void EventSender::UpdateClickCountForButton(
    WebMouseEvent::Button button_type) {
  if ((GetCurrentEventTimeSec() - last_click_time_sec_ <
       kMultipleClickTimeSec) &&
      (!OutsideMultiClickRadius(
          current_pointer_state_[kRawMousePointerId].last_pos_,
          last_click_pos_)) &&
      (button_type == last_button_type_)) {
    ++click_count_;
  } else {
    click_count_ = 1;
    last_button_type_ = button_type;
  }
}

WebMouseWheelEvent EventSender::GetMouseWheelEvent(gin::Arguments* args,
                                                   MouseScrollType scroll_type,
                                                   bool* send_gestures) {
  // Force a layout here just to make sure every position has been
  // determined before we send events (as well as all the other methods
  // that send an event do).
  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  double horizontal;
  double vertical;
  if (!args->GetNext(&horizontal) || !args->GetNext(&vertical)) {
    args->ThrowError();
    return WebMouseWheelEvent();
  }

  bool paged = false;
  bool has_precise_scrolling_deltas = false;
  int modifiers = 0;
  WebMouseWheelEvent::Phase phase = WebMouseWheelEvent::PhaseNone;
  if (!args->PeekNext().IsEmpty()) {
    args->GetNext(&paged);
    if (!args->PeekNext().IsEmpty()) {
      args->GetNext(&has_precise_scrolling_deltas);
      if (!args->PeekNext().IsEmpty()) {
        v8::Local<v8::Value> value;
        args->GetNext(&value);
        modifiers = GetKeyModifiersFromV8(args->isolate(), value);
        if (!args->PeekNext().IsEmpty()) {
          args->GetNext(send_gestures);
          if (!args->PeekNext().IsEmpty()) {
            v8::Local<v8::Value> phase_value;
            args->GetNext(&phase_value);
            phase = GetMouseWheelEventPhaseFromV8(phase_value);
          }
        }
      }
    }
  }

  current_pointer_state_[kRawMousePointerId].modifiers_ = modifiers;
  WebMouseWheelEvent event(WebInputEvent::MouseWheel,
                           ModifiersForPointer(kRawMousePointerId),
                           GetCurrentEventTimeSec());
  InitMouseEvent(current_pointer_state_[kRawMousePointerId].pressed_button_,
                 current_pointer_state_[kRawMousePointerId].current_buttons_,
                 current_pointer_state_[kRawMousePointerId].last_pos_,
                 click_count_, &event);
  event.wheelTicksX = static_cast<float>(horizontal);
  event.wheelTicksY = static_cast<float>(vertical);
  event.deltaX = event.wheelTicksX;
  event.deltaY = event.wheelTicksY;
  event.scrollByPage = paged;
  event.hasPreciseScrollingDeltas = has_precise_scrolling_deltas;
  event.phase = phase;
  if (scroll_type == MouseScrollType::PIXEL) {
    event.wheelTicksX /= kScrollbarPixelsPerTick;
    event.wheelTicksY /= kScrollbarPixelsPerTick;
  } else {
    event.deltaX *= kScrollbarPixelsPerTick;
    event.deltaY *= kScrollbarPixelsPerTick;
  }
  return event;
}

// Radius fields radius_x and radius_y should eventually be moved to
// WebPointerProperties.
// TODO(e_hakkinen): Drop radius_{x,y}_pointer parameters once that happens.
void EventSender::InitPointerProperties(gin::Arguments* args,
                                        WebPointerProperties* e,
                                        float* radius_x_pointer,
                                        float* radius_y_pointer) {
  if (!args->PeekNext().IsEmpty()) {
    double radius_x;
    if (!args->GetNext(&radius_x)) {
      args->ThrowError();
      return;
    }

    double radius_y = radius_x;
    if (!args->PeekNext().IsEmpty()) {
      if (!args->GetNext(&radius_y)) {
        args->ThrowError();
        return;
      }
    }

    *radius_x_pointer = static_cast<float>(radius_x);
    *radius_y_pointer = static_cast<float>(radius_y);
  }

  if (!args->PeekNext().IsEmpty()) {
    double force;
    if (!args->GetNext(&force)) {
      args->ThrowError();
      return;
    }
    e->force = static_cast<float>(force);
  }

  if (!args->PeekNext().IsEmpty()) {
    int tiltX, tiltY;
    if (!args->GetNext(&tiltX) || !args->GetNext(&tiltY)) {
      args->ThrowError();
      return;
    }
    e->tiltX = tiltX;
    e->tiltY = tiltY;
  }

  if (!getPointerType(args, false, e->pointerType))
    return;
}

void EventSender::FinishDragAndDrop(const WebMouseEvent& raw_event,
                                    blink::WebDragOperation drag_effect) {
  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebMouseEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;

  WebPoint client_point(event->x, event->y);
  WebPoint screen_point(event->globalX, event->globalY);
  current_drag_effect_ = drag_effect;
  if (current_drag_effect_) {
    // Specifically pass any keyboard modifiers to the drop method. This allows
    // tests to control the drop type (i.e. copy or move).
    mainFrameWidget()->dragTargetDrop(current_drag_data_, client_point,
                                      screen_point, event->modifiers());
  } else {
    mainFrameWidget()->dragTargetDragLeave();
  }
  current_drag_data_.reset();
  mainFrameWidget()->dragSourceEndedAt(client_point, screen_point,
                                       current_drag_effect_);
  mainFrameWidget()->dragSourceSystemDragEnded();
}

void EventSender::DoDragAfterMouseUp(const WebMouseEvent& raw_event) {
  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebMouseEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;

  last_click_time_sec_ = event->timeStampSeconds();
  last_click_pos_ = current_pointer_state_[kRawMousePointerId].last_pos_;

  // If we're in a drag operation, complete it.
  if (current_drag_data_.isNull())
    return;

  WebPoint client_point(event->x, event->y);
  WebPoint screen_point(event->globalX, event->globalY);
  blink::WebDragOperation drag_effect = mainFrameWidget()->dragTargetDragOver(
      client_point, screen_point, current_drag_effects_allowed_,
      event->modifiers());

  // Bail if dragover caused cancellation.
  if (current_drag_data_.isNull())
    return;

  FinishDragAndDrop(raw_event, drag_effect);
}

void EventSender::DoDragAfterMouseMove(const WebMouseEvent& raw_event) {
  if (current_pointer_state_[kRawMousePointerId].pressed_button_ ==
          WebMouseEvent::Button::NoButton ||
      current_drag_data_.isNull()) {
    return;
  }

  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebMouseEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;

  WebPoint client_point(event->x, event->y);
  WebPoint screen_point(event->globalX, event->globalY);
  current_drag_effect_ = mainFrameWidget()->dragTargetDragOver(
      client_point, screen_point, current_drag_effects_allowed_,
      event->modifiers());
}

void EventSender::ReplaySavedEvents() {
  replaying_saved_events_ = true;
  while (!mouse_event_queue_.empty()) {
    SavedEvent e = mouse_event_queue_.front();
    mouse_event_queue_.pop_front();

    switch (e.type) {
      case SavedEvent::TYPE_MOUSE_MOVE: {
        current_pointer_state_[kRawMousePointerId].modifiers_ = e.modifiers;
        WebMouseEvent event(WebInputEvent::MouseMove,
                            ModifiersForPointer(kRawMousePointerId),
                            GetCurrentEventTimeSec());
        InitMouseEvent(
            current_pointer_state_[kRawMousePointerId].pressed_button_,
            current_pointer_state_[kRawMousePointerId].current_buttons_, e.pos,
            click_count_, &event);
        current_pointer_state_[kRawMousePointerId].last_pos_ =
            WebPoint(event.x, event.y);
        HandleInputEventOnViewOrPopup(event);
        DoDragAfterMouseMove(event);
        break;
      }
      case SavedEvent::TYPE_LEAP_FORWARD:
        DoLeapForward(e.milliseconds);
        break;
      case SavedEvent::TYPE_MOUSE_UP: {
        current_pointer_state_[kRawMousePointerId].current_buttons_ &=
            ~GetWebMouseEventModifierForButton(e.button_type);
        current_pointer_state_[kRawMousePointerId].pressed_button_ =
            WebMouseEvent::Button::NoButton;
        current_pointer_state_[kRawMousePointerId].modifiers_ = e.modifiers;

        WebMouseEvent event(WebInputEvent::MouseUp,
                            ModifiersForPointer(kRawMousePointerId),
                            GetCurrentEventTimeSec());
        InitMouseEvent(
            e.button_type,
            current_pointer_state_[kRawMousePointerId].current_buttons_,
            current_pointer_state_[kRawMousePointerId].last_pos_, click_count_,
            &event);
        HandleInputEventOnViewOrPopup(event);
        DoDragAfterMouseUp(event);
        break;
      }
      default:
        NOTREACHED();
    }
  }

  replaying_saved_events_ = false;
}

WebInputEventResult EventSender::HandleInputEventOnViewOrPopup(
    const WebInputEvent& raw_event) {
  last_event_timestamp_ = raw_event.timeStampSeconds();

  WebPagePopup* popup = widget()->pagePopup();
  if (popup && !WebInputEvent::isKeyboardEventType(raw_event.type())) {
    // ui::ScaleWebInputEvent returns nullptr when the scale is 1.0f as the
    // event does not have to be converted.
    std::unique_ptr<WebInputEvent> scaled_event = ui::ScaleWebInputEvent(
        raw_event, delegate()->GetWindowToViewportScale());
    const WebInputEvent* popup_friendly_event =
        scaled_event.get() ? scaled_event.get() : &raw_event;
    return popup->handleInputEvent(*popup_friendly_event);
  }

  std::unique_ptr<WebInputEvent> widget_event =
      TransformScreenToWidgetCoordinates(raw_event);
  const WebInputEvent* event =
      widget_event.get() ? static_cast<WebMouseEvent*>(widget_event.get())
                         : &raw_event;
  return widget()->handleInputEvent(*event);
}

void EventSender::SendGesturesForMouseWheelEvent(
    const WebMouseWheelEvent wheel_event) {
  WebGestureEvent begin_event(WebInputEvent::GestureScrollBegin,
                              wheel_event.modifiers(),
                              GetCurrentEventTimeSec());
  InitGestureEventFromMouseWheel(wheel_event, &begin_event);
  begin_event.data.scrollBegin.deltaXHint = wheel_event.deltaX;
  begin_event.data.scrollBegin.deltaYHint = wheel_event.deltaY;
  if (wheel_event.scrollByPage) {
    begin_event.data.scrollBegin.deltaHintUnits = blink::WebGestureEvent::Page;
    if (begin_event.data.scrollBegin.deltaXHint) {
      begin_event.data.scrollBegin.deltaXHint =
          begin_event.data.scrollBegin.deltaXHint > 0 ? 1 : -1;
    }
    if (begin_event.data.scrollBegin.deltaYHint) {
      begin_event.data.scrollBegin.deltaYHint =
          begin_event.data.scrollBegin.deltaYHint > 0 ? 1 : -1;
    }
  } else {
    begin_event.data.scrollBegin.deltaHintUnits =
        wheel_event.hasPreciseScrollingDeltas
            ? blink::WebGestureEvent::PrecisePixels
            : blink::WebGestureEvent::Pixels;
  }

  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();

  HandleInputEventOnViewOrPopup(begin_event);

  WebGestureEvent update_event(WebInputEvent::GestureScrollUpdate,
                               wheel_event.modifiers(),
                               GetCurrentEventTimeSec());
  InitGestureEventFromMouseWheel(wheel_event, &update_event);
  update_event.data.scrollUpdate.deltaX =
      begin_event.data.scrollBegin.deltaXHint;
  update_event.data.scrollUpdate.deltaY =
      begin_event.data.scrollBegin.deltaYHint;
  update_event.data.scrollUpdate.deltaUnits =
      begin_event.data.scrollBegin.deltaHintUnits;

  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();
  HandleInputEventOnViewOrPopup(update_event);

  WebGestureEvent end_event(WebInputEvent::GestureScrollEnd,
                            wheel_event.modifiers(), GetCurrentEventTimeSec());
  InitGestureEventFromMouseWheel(wheel_event, &end_event);
  end_event.data.scrollEnd.deltaUnits =
      begin_event.data.scrollBegin.deltaHintUnits;

  if (force_layout_on_events_)
    widget()->updateAllLifecyclePhases();
  HandleInputEventOnViewOrPopup(end_event);
}

TestInterfaces* EventSender::interfaces() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()
      ->test_interfaces();
}

WebTestDelegate* EventSender::delegate() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()->delegate();
}

const blink::WebView* EventSender::view() const {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()->web_view();
}

blink::WebView* EventSender::view() {
  return web_widget_test_proxy_base_->web_view_test_proxy_base()->web_view();
}

blink::WebWidget* EventSender::widget() {
  return web_widget_test_proxy_base_->web_widget();
}

blink::WebFrameWidget* EventSender::mainFrameWidget() {
  return view()->mainFrame()->toWebLocalFrame()->frameWidget();
}

std::unique_ptr<WebInputEvent> EventSender::TransformScreenToWidgetCoordinates(
    const WebInputEvent& event) {
  return delegate()->TransformScreenToWidgetCoordinates(
      web_widget_test_proxy_base_, event);
}

}  // namespace test_runner
