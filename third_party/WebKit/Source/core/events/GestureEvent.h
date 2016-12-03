/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GestureEvent_h
#define GestureEvent_h

#include "core/CoreExport.h"
#include "core/events/EventDispatcher.h"
#include "core/events/MouseRelatedEvent.h"
#include "platform/PlatformGestureEvent.h"

namespace blink {

enum GestureSource {
    GestureSourceUninitialized,
    GestureSourceTouchpad,
    GestureSourceTouchscreen
};

class CORE_EXPORT GestureEvent final : public MouseRelatedEvent {
public:
    ~GestureEvent() override { }

    static GestureEvent* create(AbstractView*, const PlatformGestureEvent&);

    bool isGestureEvent() const override;

    const AtomicString& interfaceName() const override;

    float deltaX() const { return m_deltaX; }
    float deltaY() const { return m_deltaY; }
    float velocityX() const { return m_velocityX; }
    float velocityY() const { return m_velocityY; }
    ScrollInertialPhase inertialPhase() const { return m_inertialPhase; }

    GestureSource source() const { return m_source; }
    int resendingPluginId() const { return m_resendingPluginId; }
    bool synthetic() const { return m_synthetic; }
    ScrollGranularity deltaUnits() const { return m_deltaUnits; }

    DECLARE_VIRTUAL_TRACE();

private:
    GestureEvent(const AtomicString& type, AbstractView*, int screenX, int screenY, int clientX, int clientY, PlatformEvent::Modifiers, float deltaX, float deltaY, float velocityX, float velocityY, ScrollInertialPhase, bool synthetic, ScrollGranularity deltaUnits, double platformTimeStamp, int resendingPluginId, GestureSource);

    float m_deltaX;
    float m_deltaY;
    float m_velocityX;
    float m_velocityY;
    ScrollInertialPhase m_inertialPhase;
    bool m_synthetic;
    ScrollGranularity m_deltaUnits;
    GestureSource m_source;
    int m_resendingPluginId;
};

DEFINE_EVENT_TYPE_CASTS(GestureEvent);

} // namespace blink

#endif // GestureEvent_h
