// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8IntersectionObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8IntersectionObserverCallback.h"
#include "bindings/core/v8/V8IntersectionObserverInit.h"
#include "core/dom/Element.h"

namespace blink {

void V8IntersectionObserver::constructorCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(),
                                ExceptionState::ConstructionContext,
                                "IntersectionObserver");

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.throwTypeError(
        ExceptionMessages::notEnoughArguments(1, info.Length()));
    return;
  }

  v8::Local<v8::Object> wrapper = info.Holder();

  if (!info[0]->IsFunction()) {
    exceptionState.throwTypeError(
        "The callback provided as parameter 1 is not a function.");
    return;
  }

  if (info.Length() > 1 && !isUndefinedOrNull(info[1]) &&
      !info[1]->IsObject()) {
    exceptionState.throwTypeError("parameter 2 ('options') is not an object.");
    return;
  }

  IntersectionObserverInit intersectionObserverInit;
  V8IntersectionObserverInit::toImpl(info.GetIsolate(), info[1],
                                     intersectionObserverInit, exceptionState);
  if (exceptionState.hadException())
    return;

  IntersectionObserverCallback* callback = new V8IntersectionObserverCallback(
      v8::Local<v8::Function>::Cast(info[0]), wrapper,
      ScriptState::current(info.GetIsolate()));
  IntersectionObserver* observer = IntersectionObserver::create(
      intersectionObserverInit, *callback, exceptionState);
  if (exceptionState.hadException())
    return;
  ASSERT(observer);
  v8SetReturnValue(info,
                   V8DOMWrapper::associateObjectWithWrapper(
                       info.GetIsolate(), observer, &wrapperTypeInfo, wrapper));
}

void V8IntersectionObserver::visitDOMWrapperCustom(
    v8::Isolate* isolate,
    ScriptWrappable* scriptWrappable,
    const v8::Persistent<v8::Object>& wrapper) {
  IntersectionObserver* observer =
      scriptWrappable->toImpl<IntersectionObserver>();
  for (auto& observation : observer->observations()) {
    Element* target = observation->target();
    if (!target)
      continue;
    v8::UniqueId id(reinterpret_cast<intptr_t>(
        V8GCController::opaqueRootForGC(isolate, target)));
    isolate->SetReferenceFromGroup(id, wrapper);
  }
}

}  // namespace blink
