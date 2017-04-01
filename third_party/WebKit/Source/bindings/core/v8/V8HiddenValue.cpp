// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8HiddenValue.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8Binding.h"

namespace blink {

#define V8_DEFINE_METHOD(name)                                            \
  v8::Local<v8::String> V8HiddenValue::name(v8::Isolate* isolate) {       \
    V8HiddenValue* hiddenValue =                                          \
        V8PerIsolateData::from(isolate)->hiddenValue();                   \
    if (hiddenValue->m_##name.isEmpty()) {                                \
      hiddenValue->m_##name.set(isolate, v8AtomicString(isolate, #name)); \
    }                                                                     \
    return hiddenValue->m_##name.newLocal(isolate);                       \
  }

V8_HIDDEN_VALUES(V8_DEFINE_METHOD);

v8::Local<v8::Value> V8HiddenValue::getHiddenValue(ScriptState* scriptState,
                                                   v8::Local<v8::Object> object,
                                                   v8::Local<v8::String> key) {
  v8::Local<v8::Context> context = scriptState->context();
  v8::Local<v8::Private> privateKey =
      v8::Private::ForApi(scriptState->isolate(), key);
  v8::Local<v8::Value> value;
  // Callsites interpret an empty handle has absence of a result.
  if (!v8CallBoolean(object->HasPrivate(context, privateKey)))
    return v8::Local<v8::Value>();
  if (object->GetPrivate(context, privateKey).ToLocal(&value))
    return value;
  return v8::Local<v8::Value>();
}

bool V8HiddenValue::setHiddenValue(ScriptState* scriptState,
                                   v8::Local<v8::Object> object,
                                   v8::Local<v8::String> key,
                                   v8::Local<v8::Value> value) {
  if (UNLIKELY(value.IsEmpty()))
    return false;
  return v8CallBoolean(object->SetPrivate(
      scriptState->context(), v8::Private::ForApi(scriptState->isolate(), key),
      value));
}

bool V8HiddenValue::deleteHiddenValue(ScriptState* scriptState,
                                      v8::Local<v8::Object> object,
                                      v8::Local<v8::String> key) {
  // Actually deleting the value would make force the object into dictionary
  // mode which is unnecessarily slow. Instead, we replace the hidden value with
  // "undefined".
  return v8CallBoolean(object->SetPrivate(
      scriptState->context(), v8::Private::ForApi(scriptState->isolate(), key),
      v8::Undefined(scriptState->isolate())));
}

v8::Local<v8::Value> V8HiddenValue::getHiddenValueFromMainWorldWrapper(
    ScriptState* scriptState,
    ScriptWrappable* wrappable,
    v8::Local<v8::String> key) {
  v8::Local<v8::Object> wrapper =
      wrappable->mainWorldWrapper(scriptState->isolate());
  return wrapper.IsEmpty() ? v8::Local<v8::Value>()
                           : getHiddenValue(scriptState, wrapper, key);
}

}  // namespace blink
