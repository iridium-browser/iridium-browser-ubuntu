/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "bindings/core/v8/V8JavaScriptCallFrame.h"

namespace blink {

void V8JavaScriptCallFrame::evaluateWithExceptionDetailsMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toImpl(info.Holder());
    v8SetReturnValue(info, impl->evaluateWithExceptionDetails(info[0], info[1]));
}

void V8JavaScriptCallFrame::restartMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toImpl(info.Holder());
    v8SetReturnValue(info, impl->restart());
}

void V8JavaScriptCallFrame::setVariableValueMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toImpl(info.Holder());
    v8::Maybe<int32_t> maybeScopeIndex = info[0]->Int32Value(info.GetIsolate()->GetCurrentContext());
    if (maybeScopeIndex.IsNothing())
        return;
    int scopeIndex = maybeScopeIndex.FromJust();
    v8SetReturnValue(info, impl->setVariableValue(scopeIndex, info[1], info[2]));
}

void V8JavaScriptCallFrame::scopeChainAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toImpl(info.Holder());
    v8SetReturnValue(info, impl->scopeChain());
}

void V8JavaScriptCallFrame::scopeTypeMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toImpl(info.Holder());
    v8::Maybe<int32_t> maybeScopeIndex = info[0]->Int32Value(info.GetIsolate()->GetCurrentContext());
    if (maybeScopeIndex.IsNothing())
        return;
    int scopeIndex = maybeScopeIndex.FromJust();
    v8SetReturnValue(info, impl->scopeType(scopeIndex));
}

void V8JavaScriptCallFrame::thisObjectAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toImpl(info.Holder());
    v8SetReturnValue(info, impl->thisObject());
}

void V8JavaScriptCallFrame::returnValueAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::toImpl(info.Holder());
    v8SetReturnValue(info, impl->returnValue());
}

} // namespace blink

