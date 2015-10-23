// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/gc_callback.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

GCCallback::GCCallback(ScriptContext* context,
                       const v8::Local<v8::Object>& object,
                       const v8::Local<v8::Function>& callback,
                       const base::Closure& fallback)
    : context_(context),
      object_(context->isolate(), object),
      callback_(context->isolate(), callback),
      fallback_(fallback),
      weak_ptr_factory_(this) {
  object_.SetWeak(this, OnObjectGC, v8::WeakCallbackType::kParameter);
  context->AddInvalidationObserver(base::Bind(&GCCallback::OnContextInvalidated,
                                              weak_ptr_factory_.GetWeakPtr()));
}

GCCallback::~GCCallback() {}

// static
void GCCallback::OnObjectGC(const v8::WeakCallbackInfo<GCCallback>& data) {
  // Usually FirstWeakCallback should do nothing other than reset |object_|
  // and then set a second weak callback to run later. We can sidestep that,
  // because posting a task to the current message loop is all but free - but
  // DO NOT add any more work to this method. The only acceptable place to add
  // code is RunCallback.
  GCCallback* self = data.GetParameter();
  self->object_.Reset();
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&GCCallback::RunCallback,
                            self->weak_ptr_factory_.GetWeakPtr()));
}

void GCCallback::RunCallback() {
  v8::Isolate* isolate = context_->isolate();
  v8::HandleScope handle_scope(isolate);
  context_->CallFunction(v8::Local<v8::Function>::New(isolate, callback_));
  delete this;
}

void GCCallback::OnContextInvalidated() {
  fallback_.Run();
  delete this;
}

}  // namespace extensions
