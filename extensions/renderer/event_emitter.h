// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EVENT_EMITTER_H_
#define EXTENSIONS_RENDERER_EVENT_EMITTER_H_

#include <vector>

#include "extensions/renderer/api_binding_types.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {

// A gin::Wrappable Event object. One is expected to be created per event, per
// context. Note: this object *does not* clear any events, so it must be
// destroyed with the context to avoid leaking.
class EventEmitter final : public gin::Wrappable<EventEmitter> {
 public:
  using Listeners = std::vector<v8::Global<v8::Function>>;
  using ListenersChangedMethod =
      base::Callback<void(binding::EventListenersChanged,
                          v8::Local<v8::Context>)>;

  EventEmitter(const binding::RunJSFunction& run_js,
               const ListenersChangedMethod& listeners_changed);
  ~EventEmitter() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  void Fire(v8::Local<v8::Context> context,
            std::vector<v8::Local<v8::Value>>* args);

  Listeners* listeners() { return &listeners_; }

 private:
  // Bound methods for the Event JS object.
  void AddListener(gin::Arguments* arguments);
  void RemoveListener(gin::Arguments* arguments);
  bool HasListener(v8::Local<v8::Function> function);
  bool HasListeners();
  void Dispatch(gin::Arguments* arguments);

  Listeners listeners_;

  binding::RunJSFunction run_js_;

  ListenersChangedMethod listeners_changed_;

  DISALLOW_COPY_AND_ASSIGN(EventEmitter);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EVENT_EMITTER_H_
