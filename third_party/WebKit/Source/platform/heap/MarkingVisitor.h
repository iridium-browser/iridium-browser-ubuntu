// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MarkingVisitor_h
#define MarkingVisitor_h

#include "platform/heap/MarkingVisitorImpl.h"

namespace blink {

class MarkingVisitor final : public Visitor,
                             public MarkingVisitorImpl<MarkingVisitor> {
 public:
  using Impl = MarkingVisitorImpl<MarkingVisitor>;

  MarkingVisitor(ThreadState* state, VisitorMarkingMode mode)
      : Visitor(state, mode) {}

  void markHeader(HeapObjectHeader* header, TraceCallback callback) override {
    Impl::markHeader(header, header->payload(), callback);
  }

  void mark(const void* objectPointer, TraceCallback callback) override {
    Impl::mark(objectPointer, callback);
  }

  void registerDelayedMarkNoTracing(const void* object) override {
    Impl::registerDelayedMarkNoTracing(object);
  }

  void registerWeakMembers(const void* closure,
                           const void* objectPointer,
                           WeakCallback callback) override {
    Impl::registerWeakMembers(closure, objectPointer, callback);
  }

  virtual void registerWeakTable(const void* closure,
                                 EphemeronCallback iterationCallback,
                                 EphemeronCallback iterationDoneCallback) {
    Impl::registerWeakTable(closure, iterationCallback, iterationDoneCallback);
  }

#if DCHECK_IS_ON()
  virtual bool weakTableRegistered(const void* closure) {
    return Impl::weakTableRegistered(closure);
  }
#endif

  void registerMovingObjectReference(MovableReference* slot) override {
    Impl::registerMovingObjectReference(slot);
  }

  void registerMovingObjectCallback(MovableReference backingStore,
                                    MovingObjectCallback callback,
                                    void* callbackData) override {
    Impl::registerMovingObjectCallback(backingStore, callback, callbackData);
  }

  bool ensureMarked(const void* objectPointer) override {
    return Impl::ensureMarked(objectPointer);
  }

  void registerWeakCellWithCallback(void** cell,
                                    WeakCallback callback) override {
    Impl::registerWeakCellWithCallback(cell, callback);
  }
};

}  // namespace blink

#endif
