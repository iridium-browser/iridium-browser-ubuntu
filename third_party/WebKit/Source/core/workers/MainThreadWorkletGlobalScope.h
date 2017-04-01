// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MainThreadWorkletGlobalScope_h
#define MainThreadWorkletGlobalScope_h

#include "core/CoreExport.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkletGlobalScope.h"
#include "core/workers/WorkletGlobalScopeProxy.h"

namespace blink {

class ConsoleMessage;
class LocalFrame;
class ScriptSourceCode;

class CORE_EXPORT MainThreadWorkletGlobalScope : public WorkletGlobalScope,
                                                 public WorkletGlobalScopeProxy,
                                                 public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(MainThreadWorkletGlobalScope);

 public:
  MainThreadWorkletGlobalScope(LocalFrame*,
                               const KURL&,
                               const String& userAgent,
                               PassRefPtr<SecurityOrigin>,
                               v8::Isolate*);
  ~MainThreadWorkletGlobalScope() override;
  bool isMainThreadWorkletGlobalScope() const final { return true; }

  // WorkerOrWorkletGlobalScope
  void countFeature(UseCounter::Feature) final;
  void countDeprecation(UseCounter::Feature) final;
  WorkerThread* thread() const final;

  // WorkletGlobalScopeProxy
  void evaluateScript(const ScriptSourceCode&) final;
  void terminateWorkletGlobalScope() final;

  void addConsoleMessage(ConsoleMessage*) final;
  void exceptionThrown(ErrorEvent*) final;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    WorkletGlobalScope::trace(visitor);
    ContextClient::trace(visitor);
  }
};

DEFINE_TYPE_CASTS(MainThreadWorkletGlobalScope,
                  ExecutionContext,
                  context,
                  context->isMainThreadWorkletGlobalScope(),
                  context.isMainThreadWorkletGlobalScope());

}  // namespace blink

#endif  // MainThreadWorkletGlobalScope_h
