// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TouchEventManager_h
#define TouchEventManager_h

#include "core/CoreExport.h"
#include "core/events/PointerEventFactory.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/WebTouchPoint.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"

namespace blink {

class LocalFrame;
class Document;
class WebTouchEvent;

// This class takes care of dispatching all touch events and
// maintaining related states.
class CORE_EXPORT TouchEventManager
    : public GarbageCollectedFinalized<TouchEventManager> {
  WTF_MAKE_NONCOPYABLE(TouchEventManager);

 public:
  class TouchInfo {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    DEFINE_INLINE_TRACE() {
      visitor->trace(touchNode);
      visitor->trace(targetFrame);
    }

    WebTouchPoint point;
    Member<Node> touchNode;
    Member<LocalFrame> targetFrame;
    FloatPoint contentPoint;
    FloatSize adjustedRadius;
    bool knownTarget;
    String region;
  };

  explicit TouchEventManager(LocalFrame&);
  DECLARE_TRACE();

  // Does the hit-testing again if the original hit test result was not inside
  // capturing frame for touch events. Returns true if touch events could be
  // dispatched and otherwise returns false.
  bool reHitTestTouchPointsIfNeeded(const WebTouchEvent&,
                                    HeapVector<TouchInfo>&);

  // The TouchInfo array is reference just to prevent the copy. However, it
  // cannot be const as this function might change some of the properties in
  // TouchInfo objects.
  WebInputEventResult handleTouchEvent(const WebTouchEvent&,
                                       HeapVector<TouchInfo>&);

  // Resets the internal state of this object.
  void clear();

  // Returns whether there is any touch on the screen.
  bool isAnyTouchActive() const;

 private:
  void updateTargetAndRegionMapsForTouchStarts(HeapVector<TouchInfo>&);
  void setAllPropertiesOfTouchInfos(HeapVector<TouchInfo>&);

  WebInputEventResult dispatchTouchEvents(const WebTouchEvent&,
                                          const HeapVector<TouchInfo>&,
                                          bool allTouchesReleased);

  // NOTE: If adding a new field to this class please ensure that it is
  // cleared in |TouchEventManager::clear()|.

  const Member<LocalFrame> m_frame;

  // The target of each active touch point indexed by the touch ID.
  using TouchTargetMap =
      HeapHashMap<unsigned,
                  Member<Node>,
                  DefaultHash<unsigned>::Hash,
                  WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;
  TouchTargetMap m_targetForTouchID;
  using TouchRegionMap = HashMap<unsigned,
                                 String,
                                 DefaultHash<unsigned>::Hash,
                                 WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;
  TouchRegionMap m_regionForTouchID;

  // If set, the document of the active touch sequence. Unset if no touch
  // sequence active.
  Member<Document> m_touchSequenceDocument;

  bool m_touchPressed;
  bool m_suppressingTouchmovesWithinSlop;

  // The current touch action, computed on each touch start and is
  // a union of all touches. Reset when all touches are released.
  TouchAction m_currentTouchAction;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::TouchEventManager::TouchInfo);

#endif  // TouchEventManager_h
