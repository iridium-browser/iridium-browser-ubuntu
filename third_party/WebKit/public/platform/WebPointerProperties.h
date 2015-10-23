// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPointerProperties_h
#define WebPointerProperties_h

namespace blink {

// This class encapsulates the properties that are common between mouse and
// pointer events and touch points as we transition towards the unified pointer
// event model.
// TODO(e_hakkinen): Replace WebTouchEvent with WebPointerEvent, remove
// WebTouchEvent and WebTouchPoint and merge this into WebPointerEvent.
class WebPointerProperties {
public:
    WebPointerProperties()
        : button(ButtonNone)
        , id(0)
        , force(0.f)
        , tiltX(0)
        , tiltY(0)
        , pointerType(PointerTypeUnknown)
    {
    }

    enum Button {
        ButtonNone = -1,
        ButtonLeft,
        ButtonMiddle,
        ButtonRight
    };

    enum PointerType {
        PointerTypeUnknown,
        PointerTypeMouse,
        PointerTypePen,
        PointerTypeTouch
    };

    Button button;

    int id;
    float force;

    // Tilt of a pen stylus from surface normal as plane angles in degrees,
    // Values lie in [-90,90]. A positive tiltX is to the right and a positive
    // tiltY is towards the user.
    int tiltX;
    int tiltY;

    PointerType pointerType;
};

} // namespace blink

#endif
