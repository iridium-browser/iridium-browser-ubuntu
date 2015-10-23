// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/normalizing_input_filter_mac.h"

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

namespace remoting {

namespace {

KeyEvent MakeKeyEvent(uint32 keycode, bool pressed) {
  KeyEvent event;
  event.set_usb_keycode(keycode);
  event.set_pressed(pressed);
  event.set_lock_states(KeyEvent::LOCK_STATES_NUMLOCK);
  return event;
}

}  // namespace

// Test CapsLock press/release.
TEST(NormalizingInputFilterMacTest, CapsLock) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Verifies the generated CapsLock up/down events.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbCapsLock, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbCapsLock, false)));
  }

  // Injecting a CapsLock down event with NumLock on.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbCapsLock, true));
}

// Test without pressing command key.
TEST(NormalizingInputFilterMacTest, NoInjection) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
  }

  // C Down and C Up.
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent('C', false));
}

// Test pressing command key and other normal keys.
TEST(NormalizingInputFilterMacTest, CmdKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Left command key.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, false)));

    // Right command key.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, false)));

    // More than one keys after CMD.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('V', true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('V', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, false)));
  }

  // Left command key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));

  // Right command key.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightOs, true));
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightOs, false));

  // More than one keys after CMD.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightOs, true));
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent('V', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightOs, false));
}

// Test pressing command and special keys.
TEST(NormalizingInputFilterMacTest, SpecialKeys) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    // Command + Shift.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftShift, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftShift, false)));

    // Command + Option.
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftAlt, true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, false)));
    EXPECT_CALL(
        stub, InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftAlt, false)));
  }

  // Command + Shift.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftShift, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftShift, false));

  // Command + Option.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftAlt, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftAlt, false));
}

// Test pressing multiple command keys.
TEST(NormalizingInputFilterMacTest, MultipleCmdKeys) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbLeftOs, false)));
  }

  // Test multiple CMD keys at the same time.
  // L CMD Down, C Down, R CMD Down, L CMD Up.
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, true));
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightOs, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbLeftOs, false));
}

// Test press C key before command key.
TEST(NormalizingInputFilterMacTest, BeforeCmdKey) {
  MockInputStub stub;
  scoped_ptr<protocol::InputFilter> processor(
      new NormalizingInputFilterMac(&stub));

  {
    InSequence s;

    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', true)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, true)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
    EXPECT_CALL(stub,
                InjectKeyEvent(EqualsKeyEventWithNumLock(kUsbRightOs, false)));
    EXPECT_CALL(stub, InjectKeyEvent(EqualsKeyEventWithNumLock('C', false)));
  }

  // Press C before command key.
  processor->InjectKeyEvent(MakeKeyEvent('C', true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightOs, true));
  processor->InjectKeyEvent(MakeKeyEvent(kUsbRightOs, false));
  processor->InjectKeyEvent(MakeKeyEvent('C', false));
}

}  // namespace remoting
