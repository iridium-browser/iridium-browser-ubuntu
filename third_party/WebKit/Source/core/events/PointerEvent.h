// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerEvent_h
#define PointerEvent_h

#include "core/events/MouseEvent.h"
#include "core/events/PointerEventInit.h"
#include "platform/PlatformTouchPoint.h"

namespace blink {

class CORE_EXPORT PointerEvent final : public MouseEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PointerEvent* create(const AtomicString& type,
                              const PointerEventInit& initializer) {
    return new PointerEvent(type, initializer);
  }

  int pointerId() const { return m_pointerId; }
  double width() const { return m_width; }
  double height() const { return m_height; }
  float pressure() const { return m_pressure; }
  long tiltX() const { return m_tiltX; }
  long tiltY() const { return m_tiltY; }
  float tangentialPressure() const { return m_tangentialPressure; }
  long twist() const { return m_twist; }
  const String& pointerType() const { return m_pointerType; }
  bool isPrimary() const { return m_isPrimary; }

  short button() const override { return rawButton(); }
  bool isMouseEvent() const override;
  bool isPointerEvent() const override;

  EventDispatchMediator* createMediator() override;

  HeapVector<Member<PointerEvent>> getCoalescedEvents() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  PointerEvent(const AtomicString&, const PointerEventInit&);

  int m_pointerId;
  double m_width;
  double m_height;
  float m_pressure;
  long m_tiltX;
  long m_tiltY;
  float m_tangentialPressure;
  long m_twist;
  String m_pointerType;
  bool m_isPrimary;

  HeapVector<Member<PointerEvent>> m_coalescedEvents;
};

class PointerEventDispatchMediator final : public EventDispatchMediator {
 public:
  static PointerEventDispatchMediator* create(PointerEvent*);

 private:
  explicit PointerEventDispatchMediator(PointerEvent*);
  PointerEvent& event() const;
  DispatchEventResult dispatchEvent(EventDispatcher&) const override;
};

DEFINE_EVENT_TYPE_CASTS(PointerEvent);

}  // namespace blink

#endif  // PointerEvent_h
