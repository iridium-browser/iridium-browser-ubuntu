// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/input/TouchEventManager.h"

#include "core/dom/Document.h"
#include "core/events/TouchEvent.h"
#include "core/frame/Deprecation.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/input/EventHandlingUtil.h"
#include "core/input/TouchActionUtil.h"
#include "core/layout/HitTestCanvasResult.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/Histogram.h"
#include "public/platform/WebTouchEvent.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

bool hasTouchHandlers(const EventHandlerRegistry& registry) {
  return registry.hasEventHandlers(
             EventHandlerRegistry::TouchStartOrMoveEventBlocking) ||
         registry.hasEventHandlers(
             EventHandlerRegistry::TouchStartOrMoveEventPassive) ||
         registry.hasEventHandlers(
             EventHandlerRegistry::TouchEndOrCancelEventBlocking) ||
         registry.hasEventHandlers(
             EventHandlerRegistry::TouchEndOrCancelEventPassive);
}

const AtomicString& touchEventNameForTouchPointState(
    WebTouchPoint::State state) {
  switch (state) {
    case WebTouchPoint::StateReleased:
      return EventTypeNames::touchend;
    case WebTouchPoint::StateCancelled:
      return EventTypeNames::touchcancel;
    case WebTouchPoint::StatePressed:
      return EventTypeNames::touchstart;
    case WebTouchPoint::StateMoved:
      return EventTypeNames::touchmove;
    case WebTouchPoint::StateStationary:
    // Fall through to default
    default:
      ASSERT_NOT_REACHED();
      return emptyAtom;
  }
}

enum TouchEventDispatchResultType {
  UnhandledTouches,  // Unhandled touch events.
  HandledTouches,    // Handled touch events.
  TouchEventDispatchResultTypeMax,
};

bool IsTouchSequenceStart(const WebTouchEvent& event) {
  if (!event.touchesLength)
    return false;
  if (event.type() != WebInputEvent::TouchStart)
    return false;
  for (size_t i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].state != blink::WebTouchPoint::StatePressed)
      return false;
  }
  return true;
}

// Defining this class type local to dispatchTouchEvents() and annotating
// it with STACK_ALLOCATED(), runs into MSVC(VS 2013)'s C4822 warning
// that the local class doesn't provide a local definition for 'operator new'.
// Which it intentionally doesn't and shouldn't.
//
// Work around such toolchain bugginess by lifting out the type, thereby
// taking it out of C4822's reach.
class ChangedTouches final {
  STACK_ALLOCATED();

 public:
  // The touches corresponding to the particular change state this struct
  // instance represents.
  Member<TouchList> m_touches;

  using EventTargetSet = HeapHashSet<Member<EventTarget>>;
  // Set of targets involved in m_touches.
  EventTargetSet m_targets;

  WebPointerProperties::PointerType m_pointerType;
};

}  // namespace

TouchEventManager::TouchEventManager(LocalFrame& frame) : m_frame(frame) {
  clear();
}

void TouchEventManager::clear() {
  m_touchSequenceDocument.clear();
  m_targetForTouchID.clear();
  m_regionForTouchID.clear();
  m_touchPressed = false;
  m_suppressingTouchmovesWithinSlop = false;
  m_currentTouchAction = TouchActionAuto;
}

DEFINE_TRACE(TouchEventManager) {
  visitor->trace(m_frame);
  visitor->trace(m_touchSequenceDocument);
  visitor->trace(m_targetForTouchID);
}

WebInputEventResult TouchEventManager::dispatchTouchEvents(
    const WebTouchEvent& event,
    const HeapVector<TouchInfo>& touchInfos,
    bool allTouchesReleased) {
  // Build up the lists to use for the |touches|, |targetTouches| and
  // |changedTouches| attributes in the JS event. See
  // http://www.w3.org/TR/touch-events/#touchevent-interface for how these
  // lists fit together.

  if (event.type() == WebInputEvent::TouchEnd ||
      event.type() == WebInputEvent::TouchCancel || event.touchesLength > 1) {
    m_suppressingTouchmovesWithinSlop = false;
  }

  if (m_suppressingTouchmovesWithinSlop &&
      event.type() == WebInputEvent::TouchMove) {
    if (!event.movedBeyondSlopRegion)
      return WebInputEventResult::HandledSuppressed;
    m_suppressingTouchmovesWithinSlop = false;
  }

  // Holds the complete set of touches on the screen.
  TouchList* touches = TouchList::create();

  // A different view on the 'touches' list above, filtered and grouped by
  // event target. Used for the |targetTouches| list in the JS event.
  using TargetTouchesHeapMap = HeapHashMap<EventTarget*, Member<TouchList>>;
  TargetTouchesHeapMap touchesByTarget;

  // Array of touches per state, used to assemble the |changedTouches| list.
  ChangedTouches changedTouches[WebTouchPoint::StateMax + 1];

  for (auto touchInfo : touchInfos) {
    const WebTouchPoint& point = touchInfo.point;
    WebTouchPoint::State pointState = point.state;

    Touch* touch = Touch::create(
        touchInfo.targetFrame.get(), touchInfo.touchNode.get(), point.id,
        point.screenPosition, touchInfo.contentPoint, touchInfo.adjustedRadius,
        point.rotationAngle, point.force, touchInfo.region);

    // Ensure this target's touch list exists, even if it ends up empty, so
    // it can always be passed to TouchEvent::Create below.
    TargetTouchesHeapMap::iterator targetTouchesIterator =
        touchesByTarget.find(touchInfo.touchNode.get());
    if (targetTouchesIterator == touchesByTarget.end()) {
      touchesByTarget.set(touchInfo.touchNode.get(), TouchList::create());
      targetTouchesIterator = touchesByTarget.find(touchInfo.touchNode.get());
    }

    // |touches| and |targetTouches| should only contain information about
    // touches still on the screen, so if this point is released or
    // cancelled it will only appear in the |changedTouches| list.
    if (pointState != WebTouchPoint::StateReleased &&
        pointState != WebTouchPoint::StateCancelled) {
      touches->append(touch);
      targetTouchesIterator->value->append(touch);
    }

    // Now build up the correct list for |changedTouches|.
    // Note that  any touches that are in the TouchStationary state (e.g. if
    // the user had several points touched but did not move them all) should
    // never be in the |changedTouches| list so we do not handle them
    // explicitly here. See https://bugs.webkit.org/show_bug.cgi?id=37609
    // for further discussion about the TouchStationary state.
    if (pointState != WebTouchPoint::StateStationary && touchInfo.knownTarget) {
      DCHECK_LE(pointState, WebTouchPoint::StateMax);
      if (!changedTouches[pointState].m_touches)
        changedTouches[pointState].m_touches = TouchList::create();
      changedTouches[pointState].m_touches->append(touch);
      changedTouches[pointState].m_targets.insert(touchInfo.touchNode);
      changedTouches[pointState].m_pointerType = point.pointerType;
    }
  }

  if (allTouchesReleased) {
    m_touchSequenceDocument.clear();
    m_currentTouchAction = TouchActionAuto;
  }

  WebInputEventResult eventResult = WebInputEventResult::NotHandled;

  // Now iterate through the |changedTouches| list and |m_targets| within it,
  // sending TouchEvents to the targets as required.
  for (unsigned state = 0; state <= WebTouchPoint::StateMax; ++state) {
    if (!changedTouches[state].m_touches)
      continue;

    const AtomicString& eventName(touchEventNameForTouchPointState(
        static_cast<WebTouchPoint::State>(state)));
    for (const auto& eventTarget : changedTouches[state].m_targets) {
      EventTarget* touchEventTarget = eventTarget;
      TouchEvent* touchEvent = TouchEvent::create(
          event, touches, touchesByTarget.at(touchEventTarget),
          changedTouches[state].m_touches.get(), eventName,
          touchEventTarget->toNode()->document().domWindow(),
          m_currentTouchAction);

      DispatchEventResult domDispatchResult =
          touchEventTarget->dispatchEvent(touchEvent);

      // Only report for top level documents with a single touch on
      // touch-start or the first touch-move.
      if (event.touchStartOrFirstTouchMove && touchInfos.size() == 1 &&
          m_frame->isMainFrame()) {
        // Record the disposition and latency of touch starts and first touch
        // moves before and after the page is fully loaded respectively.
        int64_t latencyInMicros =
            (TimeTicks::Now() -
             TimeTicks::FromSeconds(event.timeStampSeconds()))
                .InMicroseconds();
        if (event.isCancelable()) {
          if (m_frame->document()->isLoadCompleted()) {
            DEFINE_STATIC_LOCAL(EnumerationHistogram,
                                touchDispositionsAfterPageLoadHistogram,
                                ("Event.Touch.TouchDispositionsAfterPageLoad",
                                 TouchEventDispatchResultTypeMax));
            touchDispositionsAfterPageLoadHistogram.count(
                (domDispatchResult != DispatchEventResult::NotCanceled)
                    ? HandledTouches
                    : UnhandledTouches);

            DEFINE_STATIC_LOCAL(
                CustomCountHistogram, eventLatencyAfterPageLoadHistogram,
                ("Event.Touch.TouchLatencyAfterPageLoad", 1, 100000000, 50));
            eventLatencyAfterPageLoadHistogram.count(latencyInMicros);
          } else {
            DEFINE_STATIC_LOCAL(EnumerationHistogram,
                                touchDispositionsBeforePageLoadHistogram,
                                ("Event.Touch.TouchDispositionsBeforePageLoad",
                                 TouchEventDispatchResultTypeMax));
            touchDispositionsBeforePageLoadHistogram.count(
                (domDispatchResult != DispatchEventResult::NotCanceled)
                    ? HandledTouches
                    : UnhandledTouches);

            DEFINE_STATIC_LOCAL(
                CustomCountHistogram, eventLatencyBeforePageLoadHistogram,
                ("Event.Touch.TouchLatencyBeforePageLoad", 1, 100000000, 50));
            eventLatencyBeforePageLoadHistogram.count(latencyInMicros);
          }
          // Report the touch disposition there is no active fling animation.
          DEFINE_STATIC_LOCAL(EnumerationHistogram,
                              touchDispositionsOutsideFlingHistogram,
                              ("Event.Touch.TouchDispositionsOutsideFling2",
                               TouchEventDispatchResultTypeMax));
          touchDispositionsOutsideFlingHistogram.count(
              (domDispatchResult != DispatchEventResult::NotCanceled)
                  ? HandledTouches
                  : UnhandledTouches);
        }

        // Report the touch disposition when there is an active fling animation.
        if (event.dispatchType ==
            WebInputEvent::ListenersForcedNonBlockingDueToFling) {
          DEFINE_STATIC_LOCAL(EnumerationHistogram,
                              touchDispositionsDuringFlingHistogram,
                              ("Event.Touch.TouchDispositionsDuringFling2",
                               TouchEventDispatchResultTypeMax));
          touchDispositionsDuringFlingHistogram.count(
              touchEvent->preventDefaultCalledOnUncancelableEvent()
                  ? HandledTouches
                  : UnhandledTouches);
        }
      }
      eventResult = EventHandlingUtil::mergeEventResult(
          eventResult,
          EventHandlingUtil::toWebInputEventResult(domDispatchResult));
    }
  }

  // Do not suppress any touchmoves if the touchstart is consumed.
  if (IsTouchSequenceStart(event) &&
      eventResult == WebInputEventResult::NotHandled) {
    m_suppressingTouchmovesWithinSlop = true;
  }

  return eventResult;
}

void TouchEventManager::updateTargetAndRegionMapsForTouchStarts(
    HeapVector<TouchInfo>& touchInfos) {
  for (auto& touchInfo : touchInfos) {
    // Touch events implicitly capture to the touched node, and don't change
    // active/hover states themselves (Gesture events do). So we only need
    // to hit-test on touchstart and when the target could be different than
    // the corresponding pointer event target.
    if (touchInfo.point.state == WebTouchPoint::StatePressed) {
      HitTestRequest::HitTestRequestType hitType = HitTestRequest::TouchEvent |
                                                   HitTestRequest::ReadOnly |
                                                   HitTestRequest::Active;
      HitTestResult result;
      // For the touchPressed points hit-testing is done in
      // PointerEventManager. If it was the second touch there is a
      // capturing documents for the touch and |m_touchSequenceDocument|
      // is not null. So if PointerEventManager should hit-test again
      // against |m_touchSequenceDocument| if the target set by
      // PointerEventManager was either null or not in
      // |m_touchSequenceDocument|.
      if (m_touchSequenceDocument &&
          (!touchInfo.touchNode ||
           &touchInfo.touchNode->document() != m_touchSequenceDocument)) {
        if (m_touchSequenceDocument->frame()) {
          LayoutPoint framePoint = LayoutPoint(
              m_touchSequenceDocument->frame()->view()->rootFrameToContents(
                  touchInfo.point.position));
          result = EventHandlingUtil::hitTestResultInFrame(
              m_touchSequenceDocument->frame(), framePoint, hitType);
          Node* node = result.innerNode();
          if (!node)
            continue;
          if (isHTMLCanvasElement(node)) {
            HitTestCanvasResult* hitTestCanvasResult =
                toHTMLCanvasElement(node)->getControlAndIdIfHitRegionExists(
                    result.pointInInnerNodeFrame());
            if (hitTestCanvasResult->getControl())
              node = hitTestCanvasResult->getControl();
            touchInfo.region = hitTestCanvasResult->getId();
          }
          // Touch events should not go to text nodes.
          if (node->isTextNode())
            node = FlatTreeTraversal::parent(*node);
          touchInfo.touchNode = node;
        } else {
          continue;
        }
      }
      if (!touchInfo.touchNode)
        continue;
      if (!m_touchSequenceDocument) {
        // Keep track of which document should receive all touch events
        // in the active sequence. This must be a single document to
        // ensure we don't leak Nodes between documents.
        m_touchSequenceDocument = &(touchInfo.touchNode->document());
        ASSERT(m_touchSequenceDocument->frame()->view());
      }

      // Ideally we'd ASSERT(!m_targetForTouchID.contains(point.id())
      // since we shouldn't get a touchstart for a touch that's already
      // down. However EventSender allows this to be violated and there's
      // some tests that take advantage of it. There may also be edge
      // cases in the browser where this happens.
      // See http://crbug.com/345372.
      m_targetForTouchID.set(touchInfo.point.id, touchInfo.touchNode);

      m_regionForTouchID.set(touchInfo.point.id, touchInfo.region);

      TouchAction effectiveTouchAction =
          TouchActionUtil::computeEffectiveTouchAction(*touchInfo.touchNode);
      if (effectiveTouchAction != TouchActionAuto) {
        m_frame->page()->chromeClient().setTouchAction(m_frame,
                                                       effectiveTouchAction);

        // Combine the current touch action sequence with the touch action
        // for the current finger press.
        m_currentTouchAction &= effectiveTouchAction;
      }
    }
  }
}

void TouchEventManager::setAllPropertiesOfTouchInfos(
    HeapVector<TouchInfo>& touchInfos) {
  for (auto& touchInfo : touchInfos) {
    WebTouchPoint::State pointState = touchInfo.point.state;
    Node* touchNode = nullptr;
    String regionID;

    if (pointState == WebTouchPoint::StateReleased ||
        pointState == WebTouchPoint::StateCancelled) {
      // The target should be the original target for this touch, so get
      // it from the hashmap. As it's a release or cancel we also remove
      // it from the map.
      touchNode = m_targetForTouchID.take(touchInfo.point.id);
      regionID = m_regionForTouchID.take(touchInfo.point.id);
    } else {
      // No hittest is performed on move or stationary, since the target
      // is not allowed to change anyway.
      touchNode = m_targetForTouchID.at(touchInfo.point.id);
      regionID = m_regionForTouchID.at(touchInfo.point.id);
    }

    LocalFrame* targetFrame = nullptr;
    bool knownTarget = false;
    if (touchNode) {
      Document& doc = touchNode->document();
      // If the target node has moved to a new document while it was being
      // touched, we can't send events to the new document because that could
      // leak nodes from one document to another. See http://crbug.com/394339.
      if (&doc == m_touchSequenceDocument.get()) {
        targetFrame = doc.frame();
        knownTarget = true;
      }
    }
    if (!knownTarget) {
      // If we don't have a target registered for the point it means we've
      // missed our opportunity to do a hit test for it (due to some
      // optimization that prevented blink from ever seeing the
      // touchstart), or that the touch started outside the active touch
      // sequence document. We should still include the touch in the
      // Touches list reported to the application (eg. so it can
      // differentiate between a one and two finger gesture), but we won't
      // actually dispatch any events for it. Set the target to the
      // Document so that there's some valid node here. Perhaps this
      // should really be LocalDOMWindow, but in all other cases the target of
      // a Touch is a Node so using the window could be a breaking change.
      // Since we know there was no handler invoked, the specific target
      // should be completely irrelevant to the application.
      touchNode = m_touchSequenceDocument;
      targetFrame = m_touchSequenceDocument->frame();
    }
    ASSERT(targetFrame);

    // pagePoint should always be in the target element's document coordinates.
    FloatPoint pagePoint =
        targetFrame->view()->rootFrameToContents(touchInfo.point.position);
    float scaleFactor = 1.0f / targetFrame->pageZoomFactor();

    touchInfo.touchNode = touchNode;
    touchInfo.targetFrame = targetFrame;
    touchInfo.contentPoint = pagePoint.scaledBy(scaleFactor);
    touchInfo.adjustedRadius =
        FloatSize(touchInfo.point.radiusX, touchInfo.point.radiusY)
            .scaledBy(scaleFactor);
    touchInfo.knownTarget = knownTarget;
    touchInfo.region = regionID;
  }
}

bool TouchEventManager::reHitTestTouchPointsIfNeeded(
    const WebTouchEvent& event,
    HeapVector<TouchInfo>& touchInfos) {
  bool newTouchSequence = true;
  bool allTouchesReleased = true;

  for (unsigned i = 0; i < event.touchesLength; ++i) {
    WebTouchPoint::State state = event.touches[i].state;
    if (state != WebTouchPoint::StatePressed)
      newTouchSequence = false;
    if (state != WebTouchPoint::StateReleased &&
        state != WebTouchPoint::StateCancelled)
      allTouchesReleased = false;
  }
  if (newTouchSequence) {
    // Ideally we'd ASSERT(!m_touchSequenceDocument) here since we should
    // have cleared the active document when we saw the last release. But we
    // have some tests that violate this, ClusterFuzz could trigger it, and
    // there may be cases where the browser doesn't reliably release all
    // touches. http://crbug.com/345372 tracks this.
    m_touchSequenceDocument.clear();
  }

  ASSERT(m_frame->view());
  if (m_touchSequenceDocument && (!m_touchSequenceDocument->frame() ||
                                  !m_touchSequenceDocument->frame()->view())) {
    // If the active touch document has no frame or view, it's probably being
    // destroyed so we can't dispatch events.
    return false;
  }

  updateTargetAndRegionMapsForTouchStarts(touchInfos);

  m_touchPressed = !allTouchesReleased;

  // If there's no document receiving touch events, or no handlers on the
  // document set to receive the events, then we can skip all the rest of
  // this work.
  if (!m_touchSequenceDocument || !m_touchSequenceDocument->frameHost() ||
      !hasTouchHandlers(
          m_touchSequenceDocument->frameHost()->eventHandlerRegistry()) ||
      !m_touchSequenceDocument->frame()) {
    if (allTouchesReleased) {
      m_touchSequenceDocument.clear();
    }
    return false;
  }

  setAllPropertiesOfTouchInfos(touchInfos);

  return true;
}

WebInputEventResult TouchEventManager::handleTouchEvent(
    const WebTouchEvent& event,
    HeapVector<TouchInfo>& touchInfos) {
  if (!reHitTestTouchPointsIfNeeded(event, touchInfos))
    return WebInputEventResult::NotHandled;

  bool allTouchesReleased = true;
  for (unsigned i = 0; i < event.touchesLength; ++i) {
    WebTouchPoint::State state = event.touches[i].state;
    if (state != WebTouchPoint::StateReleased &&
        state != WebTouchPoint::StateCancelled)
      allTouchesReleased = false;
  }

  return dispatchTouchEvents(event, touchInfos, allTouchesReleased);
}

bool TouchEventManager::isAnyTouchActive() const {
  return m_touchPressed;
}

}  // namespace blink
