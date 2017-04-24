// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ToV8_h
#define ToV8_h

// ToV8() provides C++ -> V8 conversion. Note that ToV8() can return an empty
// handle. Call sites must check IsEmpty() before using return value.

#include <utility>

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/IDLDictionaryBase.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "v8/include/v8.h"
#include "wtf/Forward.h"

namespace blink {

class DOMWindow;
class Dictionary;
class EventTarget;

// ScriptWrappable

inline v8::Local<v8::Value> ToV8(ScriptWrappable* impl,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  if (UNLIKELY(!impl))
    return v8::Null(isolate);
  v8::Local<v8::Value> wrapper = DOMDataStore::getWrapper(impl, isolate);
  if (!wrapper.IsEmpty())
    return wrapper;

  wrapper = impl->wrap(isolate, creationContext);
  DCHECK(!wrapper.IsEmpty());
  return wrapper;
}

inline v8::Local<v8::Value> ToV8(Node* impl,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  if (UNLIKELY(!impl))
    return v8::Null(isolate);
  v8::Local<v8::Value> wrapper = DOMDataStore::getWrapper(impl, isolate);
  if (!wrapper.IsEmpty())
    return wrapper;

  wrapper = ScriptWrappable::fromNode(impl)->wrap(isolate, creationContext);
  DCHECK(!wrapper.IsEmpty());
  return wrapper;
}

// Special versions for DOMWindow and EventTarget

CORE_EXPORT v8::Local<v8::Value> ToV8(DOMWindow*,
                                      v8::Local<v8::Object> creationContext,
                                      v8::Isolate*);
CORE_EXPORT v8::Local<v8::Value> ToV8(EventTarget*,
                                      v8::Local<v8::Object> creationContext,
                                      v8::Isolate*);

// Primitives

inline v8::Local<v8::Value> ToV8(const String& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return v8String(isolate, value);
}

inline v8::Local<v8::Value> ToV8(const char* value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return v8String(isolate, value);
}

template <size_t sizeOfValue>
inline v8::Local<v8::Value> toV8SignedIntegerInternal(int64_t value,
                                                      v8::Isolate*);

template <>
inline v8::Local<v8::Value> toV8SignedIntegerInternal<4>(int64_t value,
                                                         v8::Isolate* isolate) {
  return v8::Integer::New(isolate, static_cast<int32_t>(value));
}

template <>
inline v8::Local<v8::Value> toV8SignedIntegerInternal<8>(int64_t value,
                                                         v8::Isolate* isolate) {
  int32_t valueIn32Bit = static_cast<int32_t>(value);
  if (valueIn32Bit == value)
    return v8::Integer::New(isolate, value);
  // V8 doesn't have a 64-bit integer implementation.
  return v8::Number::New(isolate, value);
}

template <size_t sizeOfValue>
inline v8::Local<v8::Value> toV8UnsignedIntegerInternal(uint64_t value,
                                                        v8::Isolate*);

template <>
inline v8::Local<v8::Value> toV8UnsignedIntegerInternal<4>(
    uint64_t value,
    v8::Isolate* isolate) {
  return v8::Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(value));
}

template <>
inline v8::Local<v8::Value> toV8UnsignedIntegerInternal<8>(
    uint64_t value,
    v8::Isolate* isolate) {
  uint32_t valueIn32Bit = static_cast<uint32_t>(value);
  if (valueIn32Bit == value)
    return v8::Integer::NewFromUnsigned(isolate, value);
  // V8 doesn't have a 64-bit integer implementation.
  return v8::Number::New(isolate, value);
}

inline v8::Local<v8::Value> ToV8(int value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return toV8SignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(long value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return toV8SignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(long long value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return toV8SignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(unsigned value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return toV8UnsignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(unsigned long value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return toV8UnsignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(unsigned long long value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return toV8UnsignedIntegerInternal<sizeof value>(value, isolate);
}

inline v8::Local<v8::Value> ToV8(double value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return v8::Number::New(isolate, value);
}

inline v8::Local<v8::Value> ToV8(bool value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return v8::Boolean::New(isolate, value);
}

// Identity operator

inline v8::Local<v8::Value> ToV8(v8::Local<v8::Value> value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate*) {
  return value;
}

// Undefined

struct ToV8UndefinedGenerator {
  DISALLOW_NEW();
};  // Used only for having toV8 return v8::Undefined.

inline v8::Local<v8::Value> ToV8(const ToV8UndefinedGenerator& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return v8::Undefined(isolate);
}

// ScriptValue

inline v8::Local<v8::Value> ToV8(const ScriptValue& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  if (value.isEmpty())
    return v8::Undefined(isolate);
  return value.v8Value();
}

// Dictionary

inline v8::Local<v8::Value> ToV8(const Dictionary& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  NOTREACHED();
  return v8::Undefined(isolate);
}

inline v8::Local<v8::Value> ToV8(const IDLDictionaryBase& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return value.toV8Impl(creationContext, isolate);
}

// Array

// Declare the function here but define it later so it can call the ToV8()
// overloads below.
template <typename Sequence>
inline v8::Local<v8::Value> toV8SequenceInternal(
    const Sequence& sequence,
    v8::Local<v8::Object> creationContext,
    v8::Isolate*);

template <typename T, size_t inlineCapacity>
inline v8::Local<v8::Value> ToV8(const Vector<T, inlineCapacity>& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return toV8SequenceInternal(value, creationContext, isolate);
}

template <typename T, size_t inlineCapacity>
inline v8::Local<v8::Value> ToV8(const HeapVector<T, inlineCapacity>& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  return toV8SequenceInternal(value, creationContext, isolate);
}

template <typename T>
inline v8::Local<v8::Value> ToV8(const Vector<std::pair<String, T>>& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  v8::Local<v8::Object> object;
  {
    v8::Context::Scope contextScope(creationContext->CreationContext());
    object = v8::Object::New(isolate);
  }
  for (unsigned i = 0; i < value.size(); ++i) {
    v8::Local<v8::Value> v8Value = ToV8(value[i].second, object, isolate);
    if (v8Value.IsEmpty())
      v8Value = v8::Undefined(isolate);
    if (!v8CallBoolean(object->CreateDataProperty(
            isolate->GetCurrentContext(), v8String(isolate, value[i].first),
            v8Value)))
      return v8::Local<v8::Value>();
  }
  return object;
}

template <typename T>
inline v8::Local<v8::Value> ToV8(const HeapVector<std::pair<String, T>>& value,
                                 v8::Local<v8::Object> creationContext,
                                 v8::Isolate* isolate) {
  v8::Local<v8::Object> object;
  {
    v8::Context::Scope contextScope(creationContext->CreationContext());
    object = v8::Object::New(isolate);
  }
  for (unsigned i = 0; i < value.size(); ++i) {
    v8::Local<v8::Value> v8Value = ToV8(value[i].second, object, isolate);
    if (v8Value.IsEmpty())
      v8Value = v8::Undefined(isolate);
    if (!v8CallBoolean(object->CreateDataProperty(
            isolate->GetCurrentContext(), v8String(isolate, value[i].first),
            v8Value)))
      return v8::Local<v8::Value>();
  }
  return object;
}

template <typename Sequence>
inline v8::Local<v8::Value> toV8SequenceInternal(
    const Sequence& sequence,
    v8::Local<v8::Object> creationContext,
    v8::Isolate* isolate) {
  v8::Local<v8::Array> array;
  {
    v8::Context::Scope contextScope(creationContext->CreationContext());
    array = v8::Array::New(isolate, sequence.size());
  }
  uint32_t index = 0;
  typename Sequence::const_iterator end = sequence.end();
  for (typename Sequence::const_iterator iter = sequence.begin(); iter != end;
       ++iter) {
    v8::Local<v8::Value> value = ToV8(*iter, array, isolate);
    if (value.IsEmpty())
      value = v8::Undefined(isolate);
    if (!v8CallBoolean(array->CreateDataProperty(isolate->GetCurrentContext(),
                                                 index++, value)))
      return v8::Local<v8::Value>();
  }
  return array;
}

// In all cases allow script state instead of creation context + isolate.
// Use this function only if the call site does not otherwise need the global,
// since v8::Context::Global is heavy.
template <typename T>
inline v8::Local<v8::Value> ToV8(T&& value, ScriptState* scriptState) {
  return ToV8(std::forward<T>(value), scriptState->context()->Global(),
              scriptState->isolate());
}

// Only declare ToV8(void*,...) for checking function overload mismatch.
// This ToV8(void*,...) should be never used. So we will find mismatch
// because of "unresolved external symbol".
// Without ToV8(void*, ...), call to toV8 with T* will match with
// ToV8(bool, ...) if T is not a subclass of ScriptWrappable or if T is
// declared but not defined (so it's not clear that T is a subclass of
// ScriptWrappable).
// This hack helps detect such unwanted implicit conversions from T* to bool.
v8::Local<v8::Value> ToV8(void* value,
                          v8::Local<v8::Object> creationContext,
                          v8::Isolate*) = delete;

// Cannot define in ScriptValue because of the circular dependency between toV8
// and ScriptValue
template <typename T>
inline ScriptValue ScriptValue::from(ScriptState* scriptState, T&& value) {
  return ScriptValue(scriptState, ToV8(std::forward<T>(value), scriptState));
}

}  // namespace blink

#endif  // ToV8_h
