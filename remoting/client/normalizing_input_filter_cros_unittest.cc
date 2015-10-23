// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/normalizing_input_filter_cros.h"

#include "remoting/proto/event.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/test_event_matchers.h"
#include "remoting/protocol/usb_key_codes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using remoting::protocol::InputStub;
using remoting::protocol::KeyEvent;
using remoting::protocol::MockInputStub;
using remoting::protocol::MouseEvent;
using remoting::protocol::test::EqualsKeyEventWithNumLock;
using remoting::protocol::test::EqualsMouseButtonEvent;
using remoting::protocol::test::EqualsMouseMoveEvent;

namespace remoting {

namespace {

const unsigned int kUsbFunctionKey = 0x07003a;  // F1
const unsigned int kUsbExtendedKey = kUsbInsert;
const unsigned int kUsbOtherKey    = kUsbTab;

KeyEvent MakeKeyEvent(uint32 keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(keycode);
  event.set_pressed(pressed);
  event.set_lock_states(protocol::KeyEvent::LOCK_STATES_NUMLOCK);
  return event;
}

void PressAndReleaseKey(InputStub* input_stub, uint32 keycode) {
  input_stub->InjectKeyEvent(MakeKeyEvent(keycode, true));
  input_stub->InjectKeyEvent(MakeKeyEvent(keycode, false));
}

static MouseEvent MakeMouseMoveEvent(int x, int y) {
  MouseEvent event;
  event.set_x(x);
  event.set_y(y);
  return event;
}

static MouseEvent MakeMouseButtonEvent(MouseEvent::MouseButton button,
                                       bool button_down) {
  MouseEvent event;
  event.set_button(button);
  event.set_button_down(button_down);
  return event;
}

}  // namespace

// Test OSKey press/release.
TEST(NormalizingInputFilterCrosTest, PressReleaseOsKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, false)));

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, true)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, false)));
  }

  // Inject press & release events for left & right OSKeys.
  PressAndReleaseKey(processor.get(), kUsbLeftOs);
  PressAndReleaseKey(processor.get(), kUsbRightOs);
}

// Test OSKey key repeat switches it to "modifying" mode.
TEST(NormalizingInputFilterCrosTest, OSKeyRepeats) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
  }

  // Inject a press and repeats for the left OSKey, but don't release it, and
  // verify that the repeats result in press events.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
}

// Test OSKey press followed by function key press and release results in
// just the function key events.
TEST(NormalizingInputFilterCrosTest, FunctionKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbFunctionKey, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(kUsbFunctionKey, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  PressAndReleaseKey(processor.get(), kUsbFunctionKey);
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));
}

// Test OSKey press followed by extended key press and release results in
// just the function key events.
TEST(NormalizingInputFilterCrosTest, ExtendedKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbExtendedKey, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(kUsbExtendedKey, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  PressAndReleaseKey(processor.get(), kUsbExtendedKey);
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));
}

// Test OSKey press followed by non-function, non-extended key press and release
// results in normal-looking sequence.
TEST(NormalizingInputFilterCrosTest, OtherKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbOtherKey, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbOtherKey, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  PressAndReleaseKey(processor.get(), kUsbOtherKey);
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));
}

// Test OSKey press followed by extended key press, then normal key press
// results in OSKey switching to modifying mode for the normal key.
TEST(NormalizingInputFilterCrosTest, ExtendedThenOtherKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbExtendedKey, true)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(kUsbExtendedKey, false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbOtherKey, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbOtherKey, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  PressAndReleaseKey(processor.get(), kUsbExtendedKey);
  PressAndReleaseKey(processor.get(), kUsbOtherKey);
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));
}

// Test OSKey press followed by mouse event puts the OSKey into modifying mode.
TEST(NormalizingInputFilterCrosTest, MouseEvent) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseMoveEvent(0, 0)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, false)));
  }

  // Hold the left OSKey while pressing & releasing the function key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  processor->InjectMouseEvent(MakeMouseMoveEvent(0, 0));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));
}

// Test left alt + right click is remapped to left alt + left click.
TEST(NormalizingInputFilterCrosTest, LeftAltClick) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftAlt, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_LEFT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_LEFT, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftAlt, false)));
  }

  // Hold the left alt key while left-clicking. ChromeOS will rewrite this as
  // Alt+RightClick
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftAlt, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, false));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftAlt, false));
}

// Test that right alt + right click is unchanged.
TEST(NormalizingInputFilterCrosTest, RightAltClick) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterCros(&stub));

  {
    InSequence s;

    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightAlt, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_RIGHT, true)));
    EXPECT_CALL(stub, InjectMouseEvent(EqualsMouseButtonEvent(
                          MouseEvent::BUTTON_RIGHT, false)));
    EXPECT_CALL(stub, InjectKeyEvent(
                          EqualsKeyEventWithNumLock(kUsbRightAlt, false)));
  }

  // Hold the right alt key while left-clicking. ChromeOS will rewrite this as
  // Alt+RightClick
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, true));
  processor->InjectMouseEvent(
      MakeMouseButtonEvent(MouseEvent::BUTTON_RIGHT, false));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightAlt, false));
}

}  // namespace remoting
