// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/events/DragEvent.h"

#include "core/clipboard/DataTransfer.h"
#include "core/dom/Element.h"
#include "core/events/EventDispatcher.h"

namespace blink {

PassRefPtrWillBeRawPtr<DragEvent> DragEvent::create(const AtomicString& type, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView> view,
    int detail, int screenX, int screenY, int windowX, int windowY,
    int movementX, int movementY,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
    short button, unsigned short buttons,
    PassRefPtrWillBeRawPtr<EventTarget> relatedTarget, DataTransfer* dataTransfer, bool isSimulated, PlatformMouseEvent::SyntheticEventType syntheticEventType,
    double uiCreateTime)
{
    return adoptRefWillBeNoop(new DragEvent(type, canBubble, cancelable, view,
        detail, screenX, screenY, windowX, windowY,
        movementX, movementY,
        ctrlKey, altKey, shiftKey, metaKey, button, buttons, relatedTarget, dataTransfer, isSimulated, syntheticEventType, uiCreateTime));
}


DragEvent::DragEvent()
    : m_dataTransfer(nullptr)
{
}

DragEvent::DragEvent(DataTransfer* dataTransfer)
    : m_dataTransfer(dataTransfer)
{
}

DragEvent::DragEvent(const AtomicString& eventType, bool canBubble, bool cancelable, PassRefPtrWillBeRawPtr<AbstractView> view,
    int detail, int screenX, int screenY, int windowX, int windowY,
    int movementX, int movementY,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey,
    short button, unsigned short buttons, PassRefPtrWillBeRawPtr<EventTarget> relatedTarget,
    DataTransfer* dataTransfer, bool isSimulated, PlatformMouseEvent::SyntheticEventType syntheticEventType,
    double uiCreateTime)
    : MouseEvent(eventType, canBubble, cancelable, view, detail, screenX, screenY,
        windowX, windowY, movementX, movementY, ctrlKey, altKey, shiftKey, metaKey, button, buttons, relatedTarget,
        isSimulated, syntheticEventType, uiCreateTime)
    , m_dataTransfer(dataTransfer)

{
}

DragEvent::DragEvent(const AtomicString& type, const DragEventInit& initializer)
    : MouseEvent(type, initializer)
    , m_dataTransfer(initializer.dataTransfer())
{
}

bool DragEvent::isDragEvent() const
{
    return true;
}

bool DragEvent::isMouseEvent() const
{
    return false;
}

PassRefPtrWillBeRawPtr<EventDispatchMediator> DragEvent::createMediator()
{
    return DragEventDispatchMediator::create(this);
}

DEFINE_TRACE(DragEvent)
{
    visitor->trace(m_dataTransfer);
    MouseEvent::trace(visitor);
}

PassRefPtrWillBeRawPtr<DragEventDispatchMediator> DragEventDispatchMediator::create(PassRefPtrWillBeRawPtr<DragEvent> dragEvent)
{
    return adoptRefWillBeNoop(new DragEventDispatchMediator(dragEvent));
}

DragEventDispatchMediator::DragEventDispatchMediator(PassRefPtrWillBeRawPtr<DragEvent> dragEvent)
    : EventDispatchMediator(dragEvent)
{
}

DragEvent& DragEventDispatchMediator::event() const
{
    return toDragEvent(EventDispatchMediator::event());
}

bool DragEventDispatchMediator::dispatchEvent(EventDispatcher& dispatcher) const
{
    event().eventPath().adjustForRelatedTarget(dispatcher.node(), event().relatedTarget());
    return EventDispatchMediator::dispatchEvent(dispatcher);
}

} // namespace blink
