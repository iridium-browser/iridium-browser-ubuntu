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

#include "core/events/GestureEvent.h"

#include "core/dom/Element.h"
#include "wtf/text/AtomicString.h"

namespace blink {

GestureEvent* GestureEvent::create(AbstractView* view, const PlatformGestureEvent& event)
{
    AtomicString eventType;
    float deltaX = 0;
    float deltaY = 0;
    float velocityX = 0;
    float velocityY = 0;
    ScrollInertialPhase inertialPhase = ScrollInertialPhase::ScrollInertialPhaseUnknown;
    bool synthetic = false;
    ScrollGranularity deltaUnits = ScrollGranularity::ScrollByPrecisePixel;

    GestureSource source = GestureSourceUninitialized;
    switch (event.source()) {
    case PlatformGestureSourceTouchpad:
        source = GestureSourceTouchpad;
        break;
    case PlatformGestureSourceTouchscreen:
        source = GestureSourceTouchscreen;
        break;
    default:
        NOTREACHED();
    }

    switch (event.type()) {
    case PlatformEvent::GestureScrollBegin:
        eventType = EventTypeNames::gesturescrollstart;
        synthetic = event.synthetic();
        deltaUnits = event.deltaUnits();
        inertialPhase = event.inertialPhase();
        break;
    case PlatformEvent::GestureScrollEnd:
        eventType = EventTypeNames::gesturescrollend;
        synthetic = event.synthetic();
        deltaUnits = event.deltaUnits();
        inertialPhase = event.inertialPhase();
        break;
    case PlatformEvent::GestureScrollUpdate:
        // Only deltaX/Y are used when converting this
        // back to a PlatformGestureEvent.
        eventType = EventTypeNames::gesturescrollupdate;
        deltaX = event.deltaX();
        deltaY = event.deltaY();
        inertialPhase = event.inertialPhase();
        deltaUnits = event.deltaUnits();
        break;
    case PlatformEvent::GestureTap:
        eventType = EventTypeNames::gesturetap; break;
    case PlatformEvent::GestureTapUnconfirmed:
        eventType = EventTypeNames::gesturetapunconfirmed; break;
    case PlatformEvent::GestureTapDown:
        eventType = EventTypeNames::gesturetapdown; break;
    case PlatformEvent::GestureShowPress:
        eventType = EventTypeNames::gestureshowpress; break;
    case PlatformEvent::GestureLongPress:
        eventType = EventTypeNames::gesturelongpress; break;
    case PlatformEvent::GestureFlingStart:
        eventType = EventTypeNames::gestureflingstart;
        velocityX = event.velocityX();
        velocityY = event.velocityY();
        break;
    case PlatformEvent::GestureTwoFingerTap:
    case PlatformEvent::GesturePinchBegin:
    case PlatformEvent::GesturePinchEnd:
    case PlatformEvent::GesturePinchUpdate:
    case PlatformEvent::GestureTapDownCancel:
    default:
        return nullptr;
    }
    return new GestureEvent(eventType, view, event.globalPosition().x(), event.globalPosition().y(), event.position().x(), event.position().y(), event.getModifiers(), deltaX, deltaY, velocityX, velocityY, inertialPhase, synthetic, deltaUnits, event.timestamp(), event.resendingPluginId(), source);
}

const AtomicString& GestureEvent::interfaceName() const
{
    // FIXME: when a GestureEvent.idl interface is defined, return the string "GestureEvent".
    // Until that happens, do not advertise an interface that does not exist, since it will
    // trip up the bindings integrity checks.
    return UIEvent::interfaceName();
}

bool GestureEvent::isGestureEvent() const
{
    return true;
}

GestureEvent::GestureEvent(const AtomicString& type, AbstractView* view, int screenX, int screenY, int clientX, int clientY, PlatformEvent::Modifiers modifiers, float deltaX, float deltaY, float velocityX, float velocityY, ScrollInertialPhase inertialPhase, bool synthetic, ScrollGranularity deltaUnits, double platformTimeStamp, int resendingPluginId, GestureSource source)
    : MouseRelatedEvent(type, true, true, view, 0, IntPoint(screenX, screenY), IntPoint(clientX, clientY), IntPoint(0, 0), modifiers, platformTimeStamp, PositionType::Position)
    , m_deltaX(deltaX)
    , m_deltaY(deltaY)
    , m_velocityX(velocityX)
    , m_velocityY(velocityY)
    , m_inertialPhase(inertialPhase)
    , m_synthetic(synthetic)
    , m_deltaUnits(deltaUnits)
    , m_source(source)
    , m_resendingPluginId(resendingPluginId)
{
}

DEFINE_TRACE(GestureEvent)
{
    MouseRelatedEvent::trace(visitor);
}

} // namespace blink
