/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/core/v8/V8CustomEvent.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8Event.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "core/dom/ContextFeatures.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

static v8::Local<v8::Value> cacheState(v8::Isolate* isolate, v8::Local<v8::Object> customEvent, v8::Local<v8::Value> detail)
{
    V8HiddenValue::setHiddenValue(isolate, customEvent, V8HiddenValue::detail(isolate), detail);
    return detail;
}


void V8CustomEvent::detailAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    CustomEvent* event = V8CustomEvent::toImpl(info.Holder());

    v8::Local<v8::Value> result = V8HiddenValue::getHiddenValue(info.GetIsolate(), info.Holder(), V8HiddenValue::detail(info.GetIsolate()));

    if (!result.IsEmpty()) {
        v8SetReturnValue(info, result);
        return;
    }

    if (!event->serializedDetail()) {
        // If we're in an isolated world and the event was created in the main world,
        // we need to find the 'detail' property on the main world wrapper and clone it.
        v8::Local<v8::Value> mainWorldDetail = V8HiddenValue::getHiddenValueFromMainWorldWrapper(info.GetIsolate(), event, V8HiddenValue::detail(info.GetIsolate()));
        if (!mainWorldDetail.IsEmpty())
            event->setSerializedDetail(SerializedScriptValueFactory::instance().createAndSwallowExceptions(info.GetIsolate(), mainWorldDetail));
    }

    if (event->serializedDetail()) {
        result = event->serializedDetail()->deserialize();
        v8SetReturnValue(info, cacheState(info.GetIsolate(), info.Holder(), result));
        return;
    }

    v8SetReturnValue(info, cacheState(info.GetIsolate(), info.Holder(), v8::Null(info.GetIsolate())));
}

void V8CustomEvent::initCustomEventMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    CustomEvent* event = V8CustomEvent::toImpl(info.Holder());
    ASSERT(!event->serializedDetail());

    TOSTRING_VOID(V8StringResource<>, typeArg, info[0]);
    bool canBubbleArg;
    bool cancelableArg;
    if (!v8Call(info[1]->BooleanValue(info.GetIsolate()->GetCurrentContext()), canBubbleArg)
        || !v8Call(info[2]->BooleanValue(info.GetIsolate()->GetCurrentContext()), cancelableArg))
        return;
    v8::Local<v8::Value> detailsArg = info[3];

    event->initEvent(typeArg, canBubbleArg, cancelableArg);

    if (!detailsArg.IsEmpty()) {
        V8HiddenValue::setHiddenValue(info.GetIsolate(), info.Holder(), V8HiddenValue::detail(info.GetIsolate()), detailsArg);
        if (DOMWrapperWorld::current(info.GetIsolate()).isIsolatedWorld())
            event->setSerializedDetail(SerializedScriptValueFactory::instance().createAndSwallowExceptions(info.GetIsolate(), detailsArg));
    }
}

} // namespace blink
