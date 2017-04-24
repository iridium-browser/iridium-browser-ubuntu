// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptFunction.h"

#include "bindings/core/v8/V8Binding.h"

namespace blink {

v8::Local<v8::Function> ScriptFunction::bindToV8Function() {
#if DCHECK_IS_ON()
  DCHECK(!m_bindToV8FunctionAlreadyCalled);
  m_bindToV8FunctionAlreadyCalled = true;
#endif

  v8::Isolate* isolate = m_scriptState->isolate();
  v8::Local<v8::External> wrapper = v8::External::New(isolate, this);
  m_scriptState->world().registerDOMObjectHolder(isolate, this, wrapper);
  return v8::Function::New(m_scriptState->context(), callCallback, wrapper, 0,
                           v8::ConstructorBehavior::kThrow)
      .ToLocalChecked();
}

void ScriptFunction::callCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ASSERT(args.Data()->IsExternal());
  ScriptFunction* scriptFunction = static_cast<ScriptFunction*>(
      v8::Local<v8::External>::Cast(args.Data())->Value());
  ScriptValue result = scriptFunction->call(
      ScriptValue(scriptFunction->getScriptState(), args[0]));
  v8SetReturnValue(args, result.v8Value());
}

}  // namespace blink
