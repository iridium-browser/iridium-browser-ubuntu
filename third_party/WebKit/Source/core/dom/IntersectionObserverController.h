// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionObserverController_h
#define IntersectionObserverController_h

#include "core/dom/IntersectionObserver.h"
#include "core/dom/SuspendableObject.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/WeakPtr.h"

// Design doc for IntersectionObserver implementation:
//   https://docs.google.com/a/google.com/document/d/1hLK0eyT5_BzyNS4OkjsnoqqFQDYCbKfyBinj94OnLiQ

namespace blink {

class Document;

class IntersectionObserverController
    : public GarbageCollectedFinalized<IntersectionObserverController>,
      public SuspendableObject {
  USING_GARBAGE_COLLECTED_MIXIN(IntersectionObserverController);

 public:
  static IntersectionObserverController* create(Document*);
  ~IntersectionObserverController();

  void resume() override;

  void scheduleIntersectionObserverForDelivery(IntersectionObserver&);
  void deliverIntersectionObservations();
  void computeTrackedIntersectionObservations();
  void addTrackedObserver(IntersectionObserver&);
  void removeTrackedObserversForRoot(const Node&);

  DECLARE_TRACE();

 private:
  explicit IntersectionObserverController(Document*);
  void postTaskToDeliverObservations();

 private:
  // IntersectionObservers for which this is the tracking document.
  HeapHashSet<WeakMember<IntersectionObserver>> m_trackedIntersectionObservers;
  // IntersectionObservers for which this is the execution context of the
  // callback.
  HeapHashSet<Member<IntersectionObserver>> m_pendingIntersectionObservers;
  WTF::WeakPtrFactory<IntersectionObserverController> m_weakPtrFactory;

  bool m_callbackFiredWhileSuspended;
};

}  // namespace blink

#endif  // IntersectionObserverController_h
