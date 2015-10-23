// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/object_backed_native_handler.h"

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {
// Key for the base::Bound routed function.
const char* kHandlerFunction = "handler_function";
}  // namespace

ObjectBackedNativeHandler::ObjectBackedNativeHandler(ScriptContext* context)
    : router_data_(context->isolate()),
      context_(context),
      object_template_(context->isolate(),
                       v8::ObjectTemplate::New(context->isolate())) {
}

ObjectBackedNativeHandler::~ObjectBackedNativeHandler() {
}

v8::Local<v8::Object> ObjectBackedNativeHandler::NewInstance() {
  return v8::Local<v8::ObjectTemplate>::New(GetIsolate(), object_template_)
      ->NewInstance();
}

// static
void ObjectBackedNativeHandler::Router(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope handle_scope(args.GetIsolate());
  v8::Local<v8::Object> data = args.Data().As<v8::Object>();

  v8::Local<v8::Value> handler_function_value =
      data->Get(v8::String::NewFromUtf8(args.GetIsolate(), kHandlerFunction));
  // See comment in header file for why we do this.
  if (handler_function_value.IsEmpty() ||
      handler_function_value->IsUndefined()) {
    ScriptContext* script_context = ScriptContextSet::GetContextByV8Context(
        args.GetIsolate()->GetCallingContext());
    console::Error(script_context ? script_context->GetRenderFrame() : nullptr,
                   "Extension view no longer exists");
    return;
  }
  DCHECK(handler_function_value->IsExternal());
  static_cast<HandlerFunction*>(
      handler_function_value.As<v8::External>()->Value())->Run(args);
}

void ObjectBackedNativeHandler::RouteFunction(
    const std::string& name,
    const HandlerFunction& handler_function) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context_->v8_context());

  v8::Local<v8::Object> data = v8::Object::New(isolate);
  data->Set(
      v8::String::NewFromUtf8(isolate, kHandlerFunction),
      v8::External::New(isolate, new HandlerFunction(handler_function)));
  v8::Local<v8::FunctionTemplate> function_template =
      v8::FunctionTemplate::New(isolate, Router, data);
  v8::Local<v8::ObjectTemplate>::New(isolate, object_template_)
      ->Set(isolate, name.c_str(), function_template);
  router_data_.Append(data);
}

v8::Isolate* ObjectBackedNativeHandler::GetIsolate() const {
  return context_->isolate();
}

void ObjectBackedNativeHandler::Invalidate() {
  v8::Isolate* isolate = GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context_->v8_context());

  for (size_t i = 0; i < router_data_.Size(); i++) {
    v8::Local<v8::Object> data = router_data_.Get(i);
    v8::Local<v8::Value> handler_function_value =
        data->Get(v8::String::NewFromUtf8(isolate, kHandlerFunction));
    CHECK(!handler_function_value.IsEmpty());
    delete static_cast<HandlerFunction*>(
        handler_function_value.As<v8::External>()->Value());
    data->Delete(v8::String::NewFromUtf8(isolate, kHandlerFunction));
  }

  router_data_.Clear();
  object_template_.Reset();

  NativeHandler::Invalidate();
}

}  // namespace extensions
