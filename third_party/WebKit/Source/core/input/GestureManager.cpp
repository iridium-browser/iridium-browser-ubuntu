// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/GestureManager.h"

#include "core/dom/Document.h"
#include "core/editing/SelectionController.h"
#include "core/events/GestureEvent.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

namespace blink {

GestureManager::GestureManager(LocalFrame* frame, ScrollManager* scrollManager,
    PointerEventManager* pointerEventManager,
    SelectionController* selectionController)
    : m_frame(frame)
    , m_scrollManager(scrollManager)
    , m_pointerEventManager(pointerEventManager)
    , m_selectionController(selectionController)
{
    clear();
}

GestureManager::~GestureManager()
{
}

void GestureManager::clear()
{
    m_suppressMouseEventsFromGestures = false;
    m_longTapShouldInvokeContextMenu = false;
    m_lastShowPressTimestamp = 0;
}

DEFINE_TRACE(GestureManager)
{
    visitor->trace(m_frame);
    visitor->trace(m_selectionController);
}

HitTestRequest::HitTestRequestType GestureManager::getHitTypeForGestureType(PlatformEvent::EventType type)
{
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::TouchEvent;
    switch (type) {
    case PlatformEvent::GestureShowPress:
    case PlatformEvent::GestureTapUnconfirmed:
        return hitType | HitTestRequest::Active;
    case PlatformEvent::GestureTapDownCancel:
        // A TapDownCancel received when no element is active shouldn't really be changing hover state.
        if (!m_frame->document()->activeHoverElement())
            hitType |= HitTestRequest::ReadOnly;
        return hitType | HitTestRequest::Release;
    case PlatformEvent::GestureTap:
        return hitType | HitTestRequest::Release;
    case PlatformEvent::GestureTapDown:
    case PlatformEvent::GestureLongPress:
    case PlatformEvent::GestureLongTap:
    case PlatformEvent::GestureTwoFingerTap:
        // FIXME: Shouldn't LongTap and TwoFingerTap clear the Active state?
        return hitType | HitTestRequest::Active | HitTestRequest::ReadOnly;
    default:
        NOTREACHED();
        return hitType | HitTestRequest::Active | HitTestRequest::ReadOnly;
    }
}

WebInputEventResult GestureManager::handleGestureEventInFrame(const GestureEventWithHitTestResults& targetedEvent)
{
    DCHECK(!targetedEvent.event().isScrollEvent());

    Node* eventTarget = targetedEvent.hitTestResult().innerNode();
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();

    if (m_scrollManager->canHandleGestureEvent(targetedEvent))
        return WebInputEventResult::HandledSuppressed;

    if (eventTarget) {
        GestureEvent* gestureDomEvent = GestureEvent::create(eventTarget->document().domWindow(), gestureEvent);
        if (gestureDomEvent) {
            DispatchEventResult gestureDomEventResult = eventTarget->dispatchEvent(gestureDomEvent);
            if (gestureDomEventResult != DispatchEventResult::NotCanceled) {
                DCHECK(gestureDomEventResult != DispatchEventResult::CanceledByEventHandler);
                return EventHandler::toWebInputEventResult(gestureDomEventResult);
            }
        }
    }

    switch (gestureEvent.type()) {
    case PlatformEvent::GestureTapDown:
        return handleGestureTapDown(targetedEvent);
    case PlatformEvent::GestureTap:
        return handleGestureTap(targetedEvent);
    case PlatformEvent::GestureShowPress:
        return handleGestureShowPress();
    case PlatformEvent::GestureLongPress:
        return handleGestureLongPress(targetedEvent);
    case PlatformEvent::GestureLongTap:
        return handleGestureLongTap(targetedEvent);
    case PlatformEvent::GestureTwoFingerTap:
        return handleGestureTwoFingerTap(targetedEvent);
    case PlatformEvent::GesturePinchBegin:
    case PlatformEvent::GesturePinchEnd:
    case PlatformEvent::GesturePinchUpdate:
    case PlatformEvent::GestureTapDownCancel:
    case PlatformEvent::GestureTapUnconfirmed:
        break;
    default:
        NOTREACHED();
    }

    return WebInputEventResult::NotHandled;
}

WebInputEventResult GestureManager::handleGestureTapDown(const GestureEventWithHitTestResults& targetedEvent)
{
    m_suppressMouseEventsFromGestures = m_pointerEventManager->primaryPointerdownCanceled(targetedEvent.event().uniqueTouchEventId());
    return WebInputEventResult::NotHandled;
}

WebInputEventResult GestureManager::handleGestureTap(const GestureEventWithHitTestResults& targetedEvent)
{
    FrameView* frameView(m_frame->view());
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();
    HitTestRequest::HitTestRequestType hitType = getHitTypeForGestureType(gestureEvent.type());
    uint64_t preDispatchDomTreeVersion = m_frame->document()->domTreeVersion();
    uint64_t preDispatchStyleVersion = m_frame->document()->styleVersion();

    UserGestureIndicator gestureIndicator(DefinitelyProcessingUserGesture);

    HitTestResult currentHitTest = targetedEvent.hitTestResult();

    // We use the adjusted position so the application isn't surprised to see a event with
    // co-ordinates outside the target's bounds.
    IntPoint adjustedPoint = frameView->rootFrameToContents(gestureEvent.position());

    const unsigned modifiers = gestureEvent.getModifiers();

    if (!m_suppressMouseEventsFromGestures) {
        PlatformMouseEvent fakeMouseMove(gestureEvent.position(), gestureEvent.globalPosition(),
            WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved, /* clickCount */ 0,
            static_cast<PlatformEvent::Modifiers>(modifiers),
            PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);
        m_frame->eventHandler().dispatchMouseEvent(EventTypeNames::mousemove, currentHitTest.innerNode(), 0, fakeMouseMove);
    }

    // Do a new hit-test in case the mousemove event changed the DOM.
    // Note that if the original hit test wasn't over an element (eg. was over a scrollbar) we
    // don't want to re-hit-test because it may be in the wrong frame (and there's no way the page
    // could have seen the event anyway).
    // Also note that the position of the frame may have changed, so we need to recompute the content
    // co-ordinates (updating layout/style as hitTestResultAtPoint normally would).
    // FIXME: Use a hit-test cache to avoid unnecessary hit tests. http://crbug.com/398920
    if (currentHitTest.innerNode()) {
        LocalFrame* mainFrame = m_frame->localFrameRoot();
        if (mainFrame && mainFrame->view())
            mainFrame->view()->updateLifecycleToCompositingCleanPlusScrolling();
        adjustedPoint = frameView->rootFrameToContents(gestureEvent.position());
        currentHitTest = EventHandler::hitTestResultInFrame(m_frame, adjustedPoint, hitType);
    }

    // Capture data for showUnhandledTapUIIfNeeded.
    Node* tappedNode = currentHitTest.innerNode();
    IntPoint tappedPosition = gestureEvent.position();
    Node* tappedNonTextNode = tappedNode;

    if (tappedNonTextNode && tappedNonTextNode->isTextNode())
        tappedNonTextNode = FlatTreeTraversal::parent(*tappedNonTextNode);

    m_frame->eventHandler().setClickNode(tappedNonTextNode);

    PlatformMouseEvent fakeMouseDown(gestureEvent.position(), gestureEvent.globalPosition(),
        WebPointerProperties::Button::Left, PlatformEvent::MousePressed, gestureEvent.tapCount(),
        static_cast<PlatformEvent::Modifiers>(modifiers | PlatformEvent::LeftButtonDown),
        PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);

    // TODO(mustaq): We suppress MEs plus all it's side effects. What would that
    // mean for for TEs?  What's the right balance here? crbug.com/617255
    WebInputEventResult mouseDownEventResult = WebInputEventResult::HandledSuppressed;
    if (!m_suppressMouseEventsFromGestures) {
        mouseDownEventResult = m_frame->eventHandler().dispatchMouseEvent(EventTypeNames::mousedown, currentHitTest.innerNode(), gestureEvent.tapCount(), fakeMouseDown);
        m_selectionController->initializeSelectionState();
        if (mouseDownEventResult == WebInputEventResult::NotHandled)
            mouseDownEventResult = m_frame->eventHandler().handleMouseFocus(MouseEventWithHitTestResults(fakeMouseDown, currentHitTest), InputDeviceCapabilities::firesTouchEventsSourceCapabilities());
        if (mouseDownEventResult == WebInputEventResult::NotHandled)
            mouseDownEventResult = m_frame->eventHandler().handleMousePressEvent(MouseEventWithHitTestResults(fakeMouseDown, currentHitTest));
    }

    if (currentHitTest.innerNode()) {
        DCHECK(gestureEvent.type() == PlatformEvent::GestureTap);
        HitTestResult result = currentHitTest;
        result.setToShadowHostIfInUserAgentShadowRoot();
        m_frame->chromeClient().onMouseDown(result.innerNode());
    }

    // FIXME: Use a hit-test cache to avoid unnecessary hit tests. http://crbug.com/398920
    if (currentHitTest.innerNode()) {
        LocalFrame* mainFrame = m_frame->localFrameRoot();
        if (mainFrame && mainFrame->view())
            mainFrame->view()->updateAllLifecyclePhases();
        adjustedPoint = frameView->rootFrameToContents(gestureEvent.position());
        currentHitTest = EventHandler::hitTestResultInFrame(m_frame, adjustedPoint, hitType);
    }

    PlatformMouseEvent fakeMouseUp(gestureEvent.position(), gestureEvent.globalPosition(),
        WebPointerProperties::Button::Left, PlatformEvent::MouseReleased, gestureEvent.tapCount(),
        static_cast<PlatformEvent::Modifiers>(modifiers),
        PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);
    WebInputEventResult mouseUpEventResult = m_suppressMouseEventsFromGestures
        ? WebInputEventResult::HandledSuppressed
        : m_frame->eventHandler().dispatchMouseEvent(EventTypeNames::mouseup, currentHitTest.innerNode(), gestureEvent.tapCount(), fakeMouseUp);

    WebInputEventResult clickEventResult = WebInputEventResult::NotHandled;
    if (tappedNonTextNode) {
        if (currentHitTest.innerNode()) {
            // Updates distribution because a mouseup (or mousedown) event listener can make the
            // tree dirty at dispatchMouseEvent() invocation above.
            // Unless distribution is updated, commonAncestor would hit DCHECK.
            // Both tappedNonTextNode and currentHitTest.innerNode()) don't need to be updated
            // because commonAncestor() will exit early if their documents are different.
            tappedNonTextNode->updateDistribution();
            Node* clickTargetNode = currentHitTest.innerNode()->commonAncestor(*tappedNonTextNode, EventHandler::parentForClickEvent);
            clickEventResult = m_frame->eventHandler().dispatchMouseEvent(EventTypeNames::click, clickTargetNode, gestureEvent.tapCount(), fakeMouseUp);
        }
        m_frame->eventHandler().setClickNode(nullptr);
    }

    if (mouseUpEventResult == WebInputEventResult::NotHandled)
        mouseUpEventResult = m_frame->eventHandler().handleMouseReleaseEvent(MouseEventWithHitTestResults(fakeMouseUp, currentHitTest));
    m_frame->eventHandler().clearDragHeuristicState();

    WebInputEventResult eventResult = EventHandler::mergeEventResult(EventHandler::mergeEventResult(mouseDownEventResult, mouseUpEventResult), clickEventResult);
    if (eventResult == WebInputEventResult::NotHandled && tappedNode && m_frame->page()) {
        bool domTreeChanged = preDispatchDomTreeVersion != m_frame->document()->domTreeVersion();
        bool styleChanged = preDispatchStyleVersion != m_frame->document()->styleVersion();

        IntPoint tappedPositionInViewport = frameHost()->visualViewport().rootFrameToViewport(tappedPosition);
        m_frame->chromeClient().showUnhandledTapUIIfNeeded(tappedPositionInViewport, tappedNode, domTreeChanged || styleChanged);
    }
    return eventResult;
}

WebInputEventResult GestureManager::handleGestureLongPress(const GestureEventWithHitTestResults& targetedEvent)
{
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();

    // FIXME: Ideally we should try to remove the extra mouse-specific hit-tests here (re-using the
    // supplied HitTestResult), but that will require some overhaul of the touch drag-and-drop code
    // and LongPress is such a special scenario that it's unlikely to matter much in practice.

    IntPoint hitTestPoint = m_frame->view()->rootFrameToContents(gestureEvent.position());
    HitTestResult hitTestResult = m_frame->eventHandler().hitTestResultAtPoint(hitTestPoint);

    m_longTapShouldInvokeContextMenu = false;
    bool hitTestContainsLinks = hitTestResult.URLElement() || !hitTestResult.absoluteImageURL().isNull() || !hitTestResult.absoluteMediaURL().isNull();

    if (!hitTestContainsLinks && m_frame->eventHandler().handleDragDropIfPossible(targetedEvent)) {
        m_longTapShouldInvokeContextMenu = true;
        return WebInputEventResult::HandledSystem;
    }

    if (m_selectionController->handleGestureLongPress(gestureEvent, hitTestResult)) {
        m_frame->eventHandler().focusDocumentView();
        return WebInputEventResult::HandledSystem;
    }

    return sendContextMenuEventForGesture(targetedEvent);
}

WebInputEventResult GestureManager::handleGestureLongTap(const GestureEventWithHitTestResults& targetedEvent)
{
#if !OS(ANDROID)
    if (m_longTapShouldInvokeContextMenu) {
        m_longTapShouldInvokeContextMenu = false;
        return sendContextMenuEventForGesture(targetedEvent);
    }
#endif
    return WebInputEventResult::NotHandled;
}

WebInputEventResult GestureManager::handleGestureTwoFingerTap(const GestureEventWithHitTestResults& targetedEvent)
{
    return sendContextMenuEventForGesture(targetedEvent);
}

WebInputEventResult GestureManager::sendContextMenuEventForGesture(const GestureEventWithHitTestResults& targetedEvent)
{
    const PlatformGestureEvent& gestureEvent = targetedEvent.event();
    unsigned modifiers = gestureEvent.getModifiers();

    if (!m_suppressMouseEventsFromGestures) {
        // Send MouseMoved event prior to handling (https://crbug.com/485290).
        PlatformMouseEvent fakeMouseMove(gestureEvent.position(), gestureEvent.globalPosition(),
            WebPointerProperties::Button::NoButton, PlatformEvent::MouseMoved, /* clickCount */ 0,
            static_cast<PlatformEvent::Modifiers>(modifiers),
            PlatformMouseEvent::FromTouch, gestureEvent.timestamp(), WebPointerProperties::PointerType::Mouse);
        m_frame->eventHandler().dispatchMouseEvent(
            EventTypeNames::mousemove, targetedEvent.hitTestResult().innerNode(), 0, fakeMouseMove);
    }

    PlatformEvent::EventType eventType = PlatformEvent::MousePressed;
    if (m_frame->settings() && m_frame->settings()->showContextMenuOnMouseUp())
        eventType = PlatformEvent::MouseReleased;

    // To simulate right-click behavior, we send a right mouse down and then context menu event.
    // TODO(crbug.com/579564): Maybe we should not send mouse down at all
    PlatformMouseEvent mouseEvent(targetedEvent.event().position(), targetedEvent.event().globalPosition(), WebPointerProperties::Button::Right, eventType, 1,
        static_cast<PlatformEvent::Modifiers>(modifiers | PlatformEvent::RightButtonDown),
        PlatformMouseEvent::FromTouch, WTF::monotonicallyIncreasingTime(), WebPointerProperties::PointerType::Mouse);
    if (!m_suppressMouseEventsFromGestures) {
        // FIXME: Send HitTestResults to avoid redundant hit tests.
        m_frame->eventHandler().handleMousePressEvent(mouseEvent);
    }

    return m_frame->eventHandler().sendContextMenuEvent(mouseEvent);
    // We do not need to send a corresponding mouse release because in case of
    // right-click, the context menu takes capture and consumes all events.
}

WebInputEventResult GestureManager::handleGestureShowPress()
{
    m_lastShowPressTimestamp = WTF::monotonicallyIncreasingTime();

    FrameView* view = m_frame->view();
    if (!view)
        return WebInputEventResult::NotHandled;
    if (ScrollAnimatorBase* scrollAnimator = view->existingScrollAnimator())
        scrollAnimator->cancelAnimation();
    const FrameView::ScrollableAreaSet* areas = view->scrollableAreas();
    if (!areas)
        return WebInputEventResult::NotHandled;
    for (const ScrollableArea* scrollableArea : *areas) {
        ScrollAnimatorBase* animator = scrollableArea->existingScrollAnimator();
        if (animator)
            animator->cancelAnimation();
    }
    return WebInputEventResult::NotHandled;
}

FrameHost* GestureManager::frameHost() const
{
    if (!m_frame->page())
        return nullptr;

    return &m_frame->page()->frameHost();
}

double GestureManager::getLastShowPressTimestamp() const
{
    return m_lastShowPressTimestamp;
}

} // namespace blink
