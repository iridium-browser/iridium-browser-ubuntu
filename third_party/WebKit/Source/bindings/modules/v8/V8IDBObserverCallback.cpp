// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8IDBObserverCallback.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "bindings/modules/v8/V8IDBObserver.h"
#include "bindings/modules/v8/V8IDBObserverChanges.h"
#include "wtf/Assertions.h"

namespace blink {

V8IDBObserverCallback::V8IDBObserverCallback(v8::Local<v8::Function> callback, v8::Local<v8::Object> owner, ScriptState* scriptState)
    : ActiveDOMCallback(scriptState->getExecutionContext())
    , m_callback(scriptState->isolate(), callback)
    , m_scriptState(scriptState)
{
    V8PrivateProperty::getIDBObserverCallback(scriptState->isolate()).set(scriptState->context(), owner, callback);
    m_callback.setPhantom();
}

V8IDBObserverCallback::~V8IDBObserverCallback()
{
}

void V8IDBObserverCallback::handleChanges(IDBObserverChanges& changes, IDBObserver& observer)
{
    if (!canInvokeCallback())
        return;

    if (!m_scriptState->contextIsValid())
        return;
    ScriptState::Scope scope(m_scriptState.get());

    if (m_callback.isEmpty())
        return;
    v8::Local<v8::Value> observerHandle = toV8(&observer, m_scriptState->context()->Global(), m_scriptState->isolate());
    if (!observerHandle->IsObject())
        return;

    v8::Local<v8::Object> thisObject = v8::Local<v8::Object>::Cast(observerHandle);
    v8::Local<v8::Value> changesHandle = toV8(&changes, m_scriptState->context()->Global(), m_scriptState->isolate());
    if (changesHandle.IsEmpty())
        return;

    v8::Local<v8::Value> argv[] = { changesHandle };

    V8ScriptRunner::callFunction(m_callback.newLocal(m_scriptState->isolate()), m_scriptState->getExecutionContext(), thisObject, WTF_ARRAY_LENGTH(argv), argv, m_scriptState->isolate());
}

DEFINE_TRACE(V8IDBObserverCallback)
{
    IDBObserverCallback::trace(visitor);
    ActiveDOMCallback::trace(visitor);
}

} // namespace blink
