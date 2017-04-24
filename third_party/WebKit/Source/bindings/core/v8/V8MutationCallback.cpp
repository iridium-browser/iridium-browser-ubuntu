/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8MutationCallback.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8MutationObserver.h"
#include "bindings/core/v8/V8MutationRecord.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/Assertions.h"

namespace blink {

V8MutationCallback::V8MutationCallback(v8::Local<v8::Function> callback,
                                       v8::Local<v8::Object> owner,
                                       ScriptState* scriptState)
    : m_callback(scriptState->isolate(), callback), m_scriptState(scriptState) {
  V8PrivateProperty::getMutationObserverCallback(scriptState->isolate())
      .set(scriptState->context(), owner, callback);
  m_callback.setPhantom();
}

V8MutationCallback::~V8MutationCallback() {}

void V8MutationCallback::call(
    const HeapVector<Member<MutationRecord>>& mutations,
    MutationObserver* observer) {
  v8::Isolate* isolate = m_scriptState->isolate();
  ExecutionContext* executionContext = m_scriptState->getExecutionContext();
  if (!executionContext || executionContext->isContextSuspended() ||
      executionContext->isContextDestroyed())
    return;
  if (!m_scriptState->contextIsValid())
    return;
  ScriptState::Scope scope(m_scriptState.get());

  if (m_callback.isEmpty())
    return;
  v8::Local<v8::Value> observerHandle =
      ToV8(observer, m_scriptState->context()->Global(), isolate);
  if (!observerHandle->IsObject())
    return;

  v8::Local<v8::Object> thisObject =
      v8::Local<v8::Object>::Cast(observerHandle);
  v8::Local<v8::Value> v8Mutations =
      ToV8(mutations, m_scriptState->context()->Global(), isolate);
  if (v8Mutations.IsEmpty())
    return;
  v8::Local<v8::Value> argv[] = {v8Mutations, observerHandle};

  v8::TryCatch exceptionCatcher(isolate);
  exceptionCatcher.SetVerbose(true);
  V8ScriptRunner::callFunction(m_callback.newLocal(isolate),
                               getExecutionContext(), thisObject,
                               WTF_ARRAY_LENGTH(argv), argv, isolate);
}

DEFINE_TRACE(V8MutationCallback) {
  MutationCallback::trace(visitor);
}

}  // namespace blink
