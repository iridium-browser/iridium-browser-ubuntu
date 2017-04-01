// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlinedGlobalMarkingVisitor_h
#define InlinedGlobalMarkingVisitor_h

#include "platform/heap/MarkingVisitorImpl.h"

namespace blink {

class InlinedGlobalMarkingVisitor final
    : public VisitorHelper<InlinedGlobalMarkingVisitor>,
      public MarkingVisitorImpl<InlinedGlobalMarkingVisitor> {
 public:
  friend class VisitorHelper<InlinedGlobalMarkingVisitor>;
  using Helper = VisitorHelper<InlinedGlobalMarkingVisitor>;
  friend class MarkingVisitorImpl<InlinedGlobalMarkingVisitor>;
  using Impl = MarkingVisitorImpl<InlinedGlobalMarkingVisitor>;

  InlinedGlobalMarkingVisitor(ThreadState* state,
                              VisitorMarkingMode markingMode)
      : VisitorHelper(state, markingMode) {}

  // Hack to unify interface to visitor->trace().
  // Without this hack, we need to use visitor.trace() for
  // trace(InlinedGlobalMarkingVisitor) and visitor->trace() for
  // trace(Visitor*).
  InlinedGlobalMarkingVisitor* operator->() { return this; }

  using Impl::mark;
  using Impl::ensureMarked;
  using Impl::registerDelayedMarkNoTracing;
  using Impl::registerWeakTable;
  using Impl::registerWeakMembers;
#if DCHECK_IS_ON()
  using Impl::weakTableRegistered;
#endif

  template <typename T>
  void mark(T* t) {
    Helper::mark(t);
  }

  template <typename T, void (T::*method)(Visitor*)>
  void registerWeakMembers(const T* obj) {
    Helper::template registerWeakMembers<T, method>(obj);
  }

 private:
  static InlinedGlobalMarkingVisitor fromHelper(Helper* helper) {
    return *static_cast<InlinedGlobalMarkingVisitor*>(helper);
  }
};

inline void GarbageCollectedMixin::trace(InlinedGlobalMarkingVisitor) {}

}  // namespace blink

#endif
