// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_V8_CONTEXT_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_V8_CONTEXT_NATIVE_HANDLER_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {

class Dispatcher;

class V8ContextNativeHandler : public ObjectBackedNativeHandler {
 public:
  V8ContextNativeHandler(ScriptContext* context, Dispatcher* dispatcher);

 private:
  void GetAvailability(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetModuleSystem(const v8::FunctionCallbackInfo<v8::Value>& args);

  void RunWithNativesEnabled(const v8::FunctionCallbackInfo<v8::Value>& args);

  ScriptContext* context_;
  Dispatcher* dispatcher_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_V8_CONTEXT_NATIVE_HANDLER_H_
