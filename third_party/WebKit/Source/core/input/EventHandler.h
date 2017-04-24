/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#ifndef EventHandler_h
#define EventHandler_h

#include "core/CoreExport.h"
#include "core/events/TextEventInputType.h"
#include "core/input/GestureManager.h"
#include "core/input/KeyboardEventManager.h"
#include "core/input/MouseEventManager.h"
#include "core/input/PointerEventManager.h"
#include "core/input/ScrollManager.h"
#include "core/layout/HitTestRequest.h"
#include "core/page/DragActions.h"
#include "core/page/EventWithHitTestResults.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/Cursor.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/platform/WebInputEvent.h"
#include "public/platform/WebInputEventResult.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashTraits.h"
#include "wtf/RefPtr.h"

namespace blink {

class DataTransfer;
class PaintLayer;
class Element;
class Event;
class EventTarget;
template <typename EventType>
class EventWithHitTestResults;
class FloatQuad;
class FrameHost;
class HTMLFrameSetElement;
class HitTestRequest;
class HitTestResult;
class LayoutObject;
class LocalFrame;
class Node;
class OptionalCursor;
class ScrollableArea;
class Scrollbar;
class SelectionController;
class TextEvent;
class WebGestureEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebTouchEvent;

class CORE_EXPORT EventHandler final
    : public GarbageCollectedFinalized<EventHandler> {
  WTF_MAKE_NONCOPYABLE(EventHandler);

 public:
  explicit EventHandler(LocalFrame&);
  DECLARE_TRACE();

  void clear();

  void updateSelectionForMouseDrag();
  void startMiddleClickAutoscroll(LayoutObject*);

  // TODO(nzolghadr): Some of the APIs in this class only forward the action
  // to the corresponding Manager class. We need to investigate whether it is
  // better to expose the manager instance itself later or can the access to
  // those APIs be more limited or removed.

  void stopAutoscroll();

  void dispatchFakeMouseMoveEventSoon();
  void dispatchFakeMouseMoveEventSoonInQuad(const FloatQuad&);

  HitTestResult hitTestResultAtPoint(
      const LayoutPoint&,
      HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly |
                                                   HitTestRequest::Active,
      const LayoutSize& padding = LayoutSize());

  bool mousePressed() const { return m_mouseEventManager->mousePressed(); }
  bool isMousePositionUnknown() const {
    return m_mouseEventManager->isMousePositionUnknown();
  }
  void clearMouseEventManager() const { m_mouseEventManager->clear(); }
  void setCapturingMouseEventsNode(
      Node*);  // A caller is responsible for resetting capturing node to 0.

  WebInputEventResult updateDragAndDrop(const WebMouseEvent&, DataTransfer*);
  void cancelDragAndDrop(const WebMouseEvent&, DataTransfer*);
  WebInputEventResult performDragAndDrop(const WebMouseEvent&, DataTransfer*);
  void updateDragStateAfterEditDragIfNeeded(Element* rootEditableElement);

  void scheduleHoverStateUpdate();
  void scheduleCursorUpdate();

  // Return whether a mouse cursor update is currently pending.  Used for
  // testing.
  bool cursorUpdatePending();

  void setResizingFrameSet(HTMLFrameSetElement*);

  void resizeScrollableAreaDestroyed();

  IntPoint lastKnownMousePosition() const;

  IntPoint dragDataTransferLocationForTesting();

  // Performs a logical scroll that chains, crossing frames, starting from
  // the given node or a reasonable default (focus/last clicked).
  bool bubblingScroll(ScrollDirection,
                      ScrollGranularity,
                      Node* startingNode = nullptr);

  WebInputEventResult handleMouseMoveEvent(
      const WebMouseEvent&,
      const Vector<WebMouseEvent>& coalescedEvents);
  void handleMouseLeaveEvent(const WebMouseEvent&);

  WebInputEventResult handleMousePressEvent(const WebMouseEvent&);
  WebInputEventResult handleMouseReleaseEvent(const WebMouseEvent&);
  WebInputEventResult handleWheelEvent(const WebMouseWheelEvent&);

  // Called on the local root frame exactly once per gesture event.
  WebInputEventResult handleGestureEvent(const WebGestureEvent&);
  WebInputEventResult handleGestureEvent(const GestureEventWithHitTestResults&);

  // Clear the old hover/active state within frames before moving the hover
  // state to the another frame
  void updateGestureHoverActiveState(const HitTestRequest&, Element*);

  // Hit-test the provided (non-scroll) gesture event, applying touch-adjustment
  // and updating hover/active state across all frames if necessary. This should
  // be called at most once per gesture event, and called on the local root
  // frame.
  // Note: This is similar to (the less clearly named) prepareMouseEvent.
  // FIXME: Remove readOnly param when there is only ever a single call to this.
  GestureEventWithHitTestResults targetGestureEvent(const WebGestureEvent&,
                                                    bool readOnly = false);
  GestureEventWithHitTestResults hitTestResultForGestureEvent(
      const WebGestureEvent&,
      HitTestRequest::HitTestRequestType);
  // Handle the provided non-scroll gesture event. Should be called only on the
  // inner frame.
  WebInputEventResult handleGestureEventInFrame(
      const GestureEventWithHitTestResults&);

  // Handle the provided scroll gesture event, propagating down to child frames
  // as necessary.
  WebInputEventResult handleGestureScrollEvent(const WebGestureEvent&);
  WebInputEventResult handleGestureScrollEnd(const WebGestureEvent&);
  bool isScrollbarHandlingGestures() const;

  bool bestClickableNodeForHitTestResult(const HitTestResult&,
                                         IntPoint& targetPoint,
                                         Node*& targetNode);
  bool bestContextMenuNodeForHitTestResult(const HitTestResult&,
                                           IntPoint& targetPoint,
                                           Node*& targetNode);
  // FIXME: This doesn't appear to be used outside tests anymore, what path are
  // we using now and is it tested?
  bool bestZoomableAreaForTouchPoint(const IntPoint& touchCenter,
                                     const IntSize& touchRadius,
                                     IntRect& targetArea,
                                     Node*& targetNode);

  WebInputEventResult sendContextMenuEvent(const WebMouseEvent&,
                                           Node* overrideTargetNode = nullptr);
  WebInputEventResult sendContextMenuEventForKey(
      Element* overrideTargetElement = nullptr);

  // Returns whether pointerId is active or not
  bool isPointerEventActive(int);

  void setPointerCapture(int, EventTarget*);
  void releasePointerCapture(int, EventTarget*);
  bool hasPointerCapture(int, const EventTarget*) const;
  bool hasProcessedPointerCapture(int, const EventTarget*) const;

  void elementRemoved(EventTarget*);

  void setMouseDownMayStartAutoscroll();

  bool handleAccessKey(const WebKeyboardEvent&);
  WebInputEventResult keyEvent(const WebKeyboardEvent&);
  void defaultKeyboardEventHandler(KeyboardEvent*);

  bool handleTextInputEvent(const String& text,
                            Event* underlyingEvent = nullptr,
                            TextEventInputType = TextEventInputKeyboard);
  void defaultTextInputEventHandler(TextEvent*);

  void dragSourceEndedAt(const WebMouseEvent&, DragOperation);

  void capsLockStateMayHaveChanged();  // Only called by FrameSelection

  WebInputEventResult handleTouchEvent(
      const WebTouchEvent&,
      const Vector<WebTouchEvent>& coalescedEvents);

  bool useHandCursor(Node*, bool isOverLink);

  void notifyElementActivated();

  PassRefPtr<UserGestureToken> takeLastMouseDownGestureToken() {
    return std::move(m_lastMouseDownUserGestureToken);
  }

  SelectionController& selectionController() const {
    return *m_selectionController;
  }

  // FIXME(nzolghadr): This function is technically a private function of
  // EventHandler class. Making it public temporary to make it possible to
  // move some code around in the refactoring process.
  // Performs a chaining logical scroll, within a *single* frame, starting
  // from either a provided starting node or a default based on the focused or
  // most recently clicked node, falling back to the frame.
  // Returns true if the scroll was consumed.
  // direction - The logical direction to scroll in. This will be converted to
  //             a physical direction for each LayoutBox we try to scroll
  //             based on that box's writing mode.
  // granularity - The units that the  scroll delta parameter is in.
  // startNode - Optional. If provided, start chaining from the given node.
  //             If not, use the current focus or last clicked node.
  bool logicalScroll(ScrollDirection,
                     ScrollGranularity,
                     Node* startNode = nullptr);

  bool isTouchPointerIdActiveOnFrame(int, LocalFrame*) const;

 private:
  WebInputEventResult handleMouseMoveOrLeaveEvent(
      const WebMouseEvent&,
      const Vector<WebMouseEvent>&,
      HitTestResult* hoveredNode = nullptr,
      bool onlyUpdateScrollbars = false,
      bool forceLeave = false);

  void applyTouchAdjustment(WebGestureEvent*, HitTestResult*);
  WebInputEventResult handleGestureTapDown(
      const GestureEventWithHitTestResults&);
  WebInputEventResult handleGestureTap(const GestureEventWithHitTestResults&);
  WebInputEventResult handleGestureLongPress(
      const GestureEventWithHitTestResults&);
  WebInputEventResult handleGestureLongTap(
      const GestureEventWithHitTestResults&);

  void updateGestureTargetNodeForMouseEvent(
      const GestureEventWithHitTestResults&);

  bool shouldApplyTouchAdjustment(const WebGestureEvent&) const;

  OptionalCursor selectCursor(const HitTestResult&);
  OptionalCursor selectAutoCursor(const HitTestResult&,
                                  Node*,
                                  const Cursor& iBeam);

  void hoverTimerFired(TimerBase*);
  void cursorUpdateTimerFired(TimerBase*);
  void activeIntervalTimerFired(TimerBase*);

  void updateCursor();

  ScrollableArea* associatedScrollableArea(const PaintLayer*) const;

  Node* updateMouseEventTargetNode(Node*);

  // Dispatches ME after corresponding PE provided the PE has not been canceled.
  // The eventType arg must be a mouse event that can be gated though a
  // preventDefaulted pointerdown (i.e., one of
  // {mousedown, mousemove, mouseup}).
  // TODO(mustaq): Can we avoid the clickCount param, instead use
  // WebmMouseEvent's count?
  //     Same applied to dispatchMouseEvent() above.
  WebInputEventResult updatePointerTargetAndDispatchEvents(
      const AtomicString& mouseEventType,
      Node* target,
      const String& canvasRegionId,
      const WebMouseEvent&,
      const Vector<WebMouseEvent>& coalescedEvents);

  // Clears drag target and related states. It is called when drag is done or
  // canceled.
  void clearDragState();

  WebInputEventResult passMousePressEventToSubframe(
      MouseEventWithHitTestResults&,
      LocalFrame* subframe);
  WebInputEventResult passMouseMoveEventToSubframe(
      MouseEventWithHitTestResults&,
      const Vector<WebMouseEvent>&,
      LocalFrame* subframe,
      HitTestResult* hoveredNode = nullptr);
  WebInputEventResult passMouseReleaseEventToSubframe(
      MouseEventWithHitTestResults&,
      LocalFrame* subframe);

  bool passMousePressEventToScrollbar(MouseEventWithHitTestResults&);

  void defaultSpaceEventHandler(KeyboardEvent*);
  void defaultBackspaceEventHandler(KeyboardEvent*);
  void defaultTabEventHandler(KeyboardEvent*);
  void defaultEscapeEventHandler(KeyboardEvent*);
  void defaultArrowEventHandler(WebFocusType, KeyboardEvent*);

  void updateLastScrollbarUnderMouse(Scrollbar*, bool);

  WebInputEventResult handleGestureShowPress();

  bool shouldBrowserControlsConsumeScroll(FloatSize) const;

  FrameHost* frameHost() const;

  bool rootFrameTouchPointerActiveInCurrentFrame(int pointerId) const;

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |EventHandler::clear()|.

  const Member<LocalFrame> m_frame;

  const Member<SelectionController> m_selectionController;

  TaskRunnerTimer<EventHandler> m_hoverTimer;

  // TODO(rbyers): Mouse cursor update is page-wide, not per-frame.  Page-wide
  // state should move out of EventHandler to a new PageEventHandler class.
  // crbug.com/449649
  TaskRunnerTimer<EventHandler> m_cursorUpdateTimer;

  Member<Node> m_capturingMouseEventsNode;
  bool m_eventHandlerWillResetCapturingMouseEventsNode;

  Member<LocalFrame> m_lastMouseMoveEventSubframe;
  Member<Scrollbar> m_lastScrollbarUnderMouse;

  Member<Node> m_dragTarget;
  bool m_shouldOnlyFireDragOverEvent;

  Member<HTMLFrameSetElement> m_frameSetBeingResized;

  RefPtr<UserGestureToken> m_lastMouseDownUserGestureToken;

  Member<ScrollManager> m_scrollManager;
  Member<MouseEventManager> m_mouseEventManager;
  Member<KeyboardEventManager> m_keyboardEventManager;
  Member<PointerEventManager> m_pointerEventManager;
  Member<GestureManager> m_gestureManager;

  double m_maxMouseMovedDuration;

  bool m_longTapShouldInvokeContextMenu;

  TaskRunnerTimer<EventHandler> m_activeIntervalTimer;
  double m_lastShowPressTimestamp;
  Member<Element> m_lastDeferredTapElement;

  // Set on GestureTapDown if the |pointerdown| event corresponding to the
  // triggering |touchstart| event was canceled. This suppresses mouse event
  // firing for the current gesture sequence (i.e. until next GestureTapDown).
  bool m_suppressMouseEventsFromGestures;
};

}  // namespace blink

#endif  // EventHandler_h
