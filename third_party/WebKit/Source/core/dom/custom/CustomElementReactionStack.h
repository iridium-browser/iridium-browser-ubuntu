// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementReactionStack_h
#define CustomElementReactionStack_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CustomElementReaction;
class CustomElementReactionQueue;
class Element;

// https://html.spec.whatwg.org/multipage/scripting.html#custom-element-reactions
class CORE_EXPORT CustomElementReactionStack final
    : public GarbageCollected<CustomElementReactionStack>,
      public TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(CustomElementReactionStack);

 public:
  CustomElementReactionStack();

  DECLARE_TRACE();
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  void push();
  void popInvokingReactions();
  void enqueueToCurrentQueue(Element*, CustomElementReaction*);
  void enqueueToBackupQueue(Element*, CustomElementReaction*);
  void clearQueue(Element*);

  static CustomElementReactionStack& current();

 private:
  friend class CustomElementReactionStackTestSupport;

  using ElementReactionQueueMap =
      HeapHashMap<TraceWrapperMember<Element>,
                  Member<CustomElementReactionQueue>>;
  ElementReactionQueueMap m_map;

  using ElementQueue = HeapVector<Member<Element>, 1>;
  HeapVector<Member<ElementQueue>> m_stack;
  Member<ElementQueue> m_backupQueue;

  void invokeBackupQueue();
  void invokeReactions(ElementQueue&);
  void enqueue(Member<ElementQueue>&, Element*, CustomElementReaction*);
};

class CORE_EXPORT CustomElementReactionStackTestSupport final {
 private:
  friend class ResetCustomElementReactionStackForTest;

  CustomElementReactionStackTestSupport() = delete;
  static CustomElementReactionStack* setCurrentForTest(
      CustomElementReactionStack*);
};

}  // namespace blink

#endif  // CustomElementReactionStack_h
