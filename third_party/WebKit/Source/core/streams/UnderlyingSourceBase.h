// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnderlyingSourceBase_h
#define UnderlyingSourceBase_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"

namespace blink {

class ReadableStreamController;

class CORE_EXPORT UnderlyingSourceBase
    : public GarbageCollectedFinalized<UnderlyingSourceBase>,
      public ScriptWrappable,
      public ActiveScriptWrappable<UnderlyingSourceBase>,
      public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(UnderlyingSourceBase);

 public:
  DECLARE_VIRTUAL_TRACE();
  virtual ~UnderlyingSourceBase() {}

  ScriptPromise startWrapper(ScriptState*, ScriptValue stream);
  virtual ScriptPromise start(ScriptState*);

  virtual ScriptPromise pull(ScriptState*);

  ScriptPromise cancelWrapper(ScriptState*, ScriptValue reason);
  virtual ScriptPromise cancel(ScriptState*, ScriptValue reason);

  void notifyLockAcquired();
  void notifyLockReleased();

  // ScriptWrappable
  bool hasPendingActivity() const;

  // ContextLifecycleObserver
  void contextDestroyed(ExecutionContext*) override;

 protected:
  explicit UnderlyingSourceBase(ScriptState* scriptState)
      : ContextLifecycleObserver(scriptState->getExecutionContext()) {}

  ReadableStreamController* controller() const { return m_controller; }

 private:
  Member<ReadableStreamController> m_controller;
  bool m_isStreamLocked = false;
};

}  // namespace blink

#endif  // UnderlyingSourceBase_h
