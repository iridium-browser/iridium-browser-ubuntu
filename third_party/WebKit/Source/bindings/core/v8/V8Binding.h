/*
* Copyright (C) 2009 Google Inc. All rights reserved.
* Copyright (C) 2012 Ericsson AB. All rights reserved.
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

#ifndef V8Binding_h
#define V8Binding_h

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/NativeValueTraits.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8StringResource.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "bindings/core/v8/V8ValueCache.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringView.h"
#include <v8.h>

namespace blink {

class DOMWindow;
class EventListener;
class EventTarget;
class ExceptionState;
class ExecutionContext;
class FlexibleArrayBufferView;
class Frame;
class LocalDOMWindow;
class LocalFrame;
class NodeFilter;
class XPathNSResolver;

template <typename T>
struct V8TypeOf {
  STATIC_ONLY(V8TypeOf);
  // |Type| provides C++ -> V8 type conversion for DOM wrappers.
  // The Blink binding code generator will generate specialized version of
  // V8TypeOf for each wrapper class.
  typedef void Type;
};

template <typename CallbackInfo, typename S>
inline void v8SetReturnValue(const CallbackInfo& info,
                             const v8::Persistent<S>& handle) {
  info.GetReturnValue().Set(handle);
}

template <typename CallbackInfo, typename S>
inline void v8SetReturnValue(const CallbackInfo& info,
                             const v8::Local<S> handle) {
  info.GetReturnValue().Set(handle);
}

template <typename CallbackInfo, typename S>
inline void v8SetReturnValue(const CallbackInfo& info,
                             v8::MaybeLocal<S> maybe) {
  if (LIKELY(!maybe.IsEmpty()))
    info.GetReturnValue().Set(maybe.ToLocalChecked());
}

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, bool value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, double value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, int32_t value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, uint32_t value) {
  info.GetReturnValue().Set(value);
}

template <typename CallbackInfo>
inline void v8SetReturnValueBool(const CallbackInfo& info, bool v) {
  info.GetReturnValue().Set(v);
}

template <typename CallbackInfo>
inline void v8SetReturnValueInt(const CallbackInfo& info, int v) {
  info.GetReturnValue().Set(v);
}

template <typename CallbackInfo>
inline void v8SetReturnValueUnsigned(const CallbackInfo& info, unsigned v) {
  info.GetReturnValue().Set(v);
}

template <typename CallbackInfo>
inline void v8SetReturnValueNull(const CallbackInfo& info) {
  info.GetReturnValue().SetNull();
}

template <typename CallbackInfo>
inline void v8SetReturnValueUndefined(const CallbackInfo& info) {
  info.GetReturnValue().SetUndefined();
}

template <typename CallbackInfo>
inline void v8SetReturnValueEmptyString(const CallbackInfo& info) {
  info.GetReturnValue().SetEmptyString();
}

template <typename CallbackInfo>
inline void v8SetReturnValueString(const CallbackInfo& info,
                                   const String& string,
                                   v8::Isolate* isolate) {
  if (string.isNull()) {
    v8SetReturnValueEmptyString(info);
    return;
  }
  V8PerIsolateData::from(isolate)->getStringCache()->setReturnValueFromString(
      info.GetReturnValue(), string.impl());
}

template <typename CallbackInfo>
inline void v8SetReturnValueStringOrNull(const CallbackInfo& info,
                                         const String& string,
                                         v8::Isolate* isolate) {
  if (string.isNull()) {
    v8SetReturnValueNull(info);
    return;
  }
  V8PerIsolateData::from(isolate)->getStringCache()->setReturnValueFromString(
      info.GetReturnValue(), string.impl());
}

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo,
                             ScriptWrappable* impl,
                             v8::Local<v8::Object> creationContext) {
  if (UNLIKELY(!impl)) {
    v8SetReturnValueNull(callbackInfo);
    return;
  }
  if (DOMDataStore::setReturnValue(callbackInfo.GetReturnValue(), impl))
    return;
  v8::Local<v8::Object> wrapper =
      impl->wrap(callbackInfo.GetIsolate(), creationContext);
  v8SetReturnValue(callbackInfo, wrapper);
}

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo,
                             ScriptWrappable* impl) {
  v8SetReturnValue(callbackInfo, impl, callbackInfo.Holder());
}

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo, Node* impl) {
  if (UNLIKELY(!impl)) {
    v8SetReturnValueNull(callbackInfo);
    return;
  }
  if (DOMDataStore::setReturnValue(callbackInfo.GetReturnValue(), impl))
    return;
  v8::Local<v8::Object> wrapper = ScriptWrappable::fromNode(impl)->wrap(
      callbackInfo.GetIsolate(), callbackInfo.Holder());
  v8SetReturnValue(callbackInfo, wrapper);
}

// Special versions for DOMWindow and EventTarget

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo,
                             DOMWindow* impl) {
  v8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(),
                                      callbackInfo.GetIsolate()));
}

template <typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo,
                             EventTarget* impl) {
  v8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(),
                                      callbackInfo.GetIsolate()));
}

template <typename CallbackInfo, typename T>
inline void v8SetReturnValue(const CallbackInfo& callbackInfo,
                             PassRefPtr<T> impl) {
  v8SetReturnValue(callbackInfo, impl.get());
}

template <typename CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo,
                                         ScriptWrappable* impl) {
  ASSERT(DOMWrapperWorld::current(callbackInfo.GetIsolate()).isMainWorld());
  if (UNLIKELY(!impl)) {
    v8SetReturnValueNull(callbackInfo);
    return;
  }
  if (DOMDataStore::setReturnValueForMainWorld(callbackInfo.GetReturnValue(),
                                               impl))
    return;
  v8::Local<v8::Object> wrapper =
      impl->wrap(callbackInfo.GetIsolate(), callbackInfo.Holder());
  v8SetReturnValue(callbackInfo, wrapper);
}

template <typename CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo,
                                         Node* impl) {
  // Since EventTarget has a special version of ToV8 and V8EventTarget.h
  // defines its own v8SetReturnValue family, which are slow, we need to
  // override them with optimized versions for Node and its subclasses.
  // Without this overload, v8SetReturnValueForMainWorld for Node would be
  // very slow.
  //
  // class hierarchy:
  //     ScriptWrappable <-- EventTarget <--+-- Node <-- ...
  //                                        +-- Window
  // overloads:
  //     v8SetReturnValueForMainWorld(ScriptWrappable*)
  //         Optimized and very fast.
  //     v8SetReturnValueForMainWorld(EventTarget*)
  //         Uses custom toV8 function and slow.
  //     v8SetReturnValueForMainWorld(Node*)
  //         Optimized and very fast.
  //     v8SetReturnValueForMainWorld(Window*)
  //         Uses custom toV8 function and slow.
  v8SetReturnValueForMainWorld(callbackInfo, ScriptWrappable::fromNode(impl));
}

// Special versions for DOMWindow and EventTarget

template <typename CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo,
                                         DOMWindow* impl) {
  v8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(),
                                      callbackInfo.GetIsolate()));
}

template <typename CallbackInfo>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo,
                                         EventTarget* impl) {
  v8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(),
                                      callbackInfo.GetIsolate()));
}

template <typename CallbackInfo, typename T>
inline void v8SetReturnValueForMainWorld(const CallbackInfo& callbackInfo,
                                         PassRefPtr<T> impl) {
  v8SetReturnValueForMainWorld(callbackInfo, impl.get());
}

template <typename CallbackInfo>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo,
                                 ScriptWrappable* impl,
                                 const ScriptWrappable* wrappable) {
  if (UNLIKELY(!impl)) {
    v8SetReturnValueNull(callbackInfo);
    return;
  }
  if (DOMDataStore::setReturnValueFast(callbackInfo.GetReturnValue(), impl,
                                       callbackInfo.Holder(), wrappable))
    return;
  v8::Local<v8::Object> wrapper =
      impl->wrap(callbackInfo.GetIsolate(), callbackInfo.Holder());
  v8SetReturnValue(callbackInfo, wrapper);
}

template <typename CallbackInfo>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo,
                                 Node* impl,
                                 const ScriptWrappable* wrappable) {
  if (UNLIKELY(!impl)) {
    v8SetReturnValueNull(callbackInfo);
    return;
  }
  if (DOMDataStore::setReturnValueFast(callbackInfo.GetReturnValue(), impl,
                                       callbackInfo.Holder(), wrappable))
    return;
  v8::Local<v8::Object> wrapper = ScriptWrappable::fromNode(impl)->wrap(
      callbackInfo.GetIsolate(), callbackInfo.Holder());
  v8SetReturnValue(callbackInfo, wrapper);
}

// Special versions for DOMWindow and EventTarget

template <typename CallbackInfo>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo,
                                 DOMWindow* impl,
                                 const ScriptWrappable*) {
  v8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(),
                                      callbackInfo.GetIsolate()));
}

template <typename CallbackInfo>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo,
                                 EventTarget* impl,
                                 const ScriptWrappable*) {
  v8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(),
                                      callbackInfo.GetIsolate()));
}

template <typename CallbackInfo, typename T, typename Wrappable>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo,
                                 PassRefPtr<T> impl,
                                 const Wrappable* wrappable) {
  v8SetReturnValueFast(callbackInfo, impl.get(), wrappable);
}

template <typename CallbackInfo, typename T>
inline void v8SetReturnValueFast(const CallbackInfo& callbackInfo,
                                 const v8::Local<T> handle,
                                 const ScriptWrappable*) {
  v8SetReturnValue(callbackInfo, handle);
}

// Convert v8::String to a WTF::String. If the V8 string is not already
// an external string then it is transformed into an external string at this
// point to avoid repeated conversions.
inline String toCoreString(v8::Local<v8::String> value) {
  return v8StringToWebCoreString<String>(value, Externalize);
}

inline String toCoreStringWithNullCheck(v8::Local<v8::String> value) {
  if (value.IsEmpty() || value->IsNull())
    return String();
  return toCoreString(value);
}

inline String toCoreStringWithUndefinedOrNullCheck(
    v8::Local<v8::String> value) {
  if (value.IsEmpty() || value->IsNull() || value->IsUndefined())
    return String();
  return toCoreString(value);
}

inline AtomicString toCoreAtomicString(v8::Local<v8::String> value) {
  return v8StringToWebCoreString<AtomicString>(value, Externalize);
}

// This method will return a null String if the v8::Value does not contain a
// v8::String.  It will not call ToString() on the v8::Value. If you want
// ToString() to be called, please use the TONATIVE_FOR_V8STRINGRESOURCE_*()
// macros instead.
inline String toCoreStringWithUndefinedOrNullCheck(v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsString())
    return String();
  return toCoreString(value.As<v8::String>());
}

// Convert a string to a V8 string.

inline v8::Local<v8::String> v8String(v8::Isolate* isolate,
                                      const StringView& string) {
  DCHECK(isolate);
  if (string.isNull())
    return v8::String::Empty(isolate);
  if (StringImpl* impl = string.sharedImpl())
    return V8PerIsolateData::from(isolate)->getStringCache()->v8ExternalString(
        isolate, impl);
  if (string.is8Bit())
    return v8::String::NewFromOneByte(
               isolate, reinterpret_cast<const uint8_t*>(string.characters8()),
               v8::NewStringType::kNormal, static_cast<int>(string.length()))
        .ToLocalChecked();
  return v8::String::NewFromTwoByte(
             isolate, reinterpret_cast<const uint16_t*>(string.characters16()),
             v8::NewStringType::kNormal, static_cast<int>(string.length()))
      .ToLocalChecked();
}

inline v8::Local<v8::Value> v8StringOrNull(v8::Isolate* isolate,
                                           const AtomicString& string) {
  if (string.isNull())
    return v8::Null(isolate);
  return V8PerIsolateData::from(isolate)->getStringCache()->v8ExternalString(
      isolate, string.impl());
}

inline v8::Local<v8::String> v8AtomicString(v8::Isolate* isolate,
                                            const StringView& string) {
  DCHECK(isolate);
  if (string.is8Bit())
    return v8::String::NewFromOneByte(
               isolate, reinterpret_cast<const uint8_t*>(string.characters8()),
               v8::NewStringType::kInternalized,
               static_cast<int>(string.length()))
        .ToLocalChecked();
  return v8::String::NewFromTwoByte(
             isolate, reinterpret_cast<const uint16_t*>(string.characters16()),
             v8::NewStringType::kInternalized,
             static_cast<int>(string.length()))
      .ToLocalChecked();
}

inline v8::Local<v8::String> v8StringFromUtf8(v8::Isolate* isolate,
                                              const char* bytes,
                                              int length) {
  DCHECK(isolate);
  return v8::String::NewFromUtf8(isolate, bytes, v8::NewStringType::kNormal,
                                 length)
      .ToLocalChecked();
}

inline v8::Local<v8::Value> v8Undefined() {
  return v8::Local<v8::Value>();
}

// Conversion flags, used in toIntXX/toUIntXX.
enum IntegerConversionConfiguration { NormalConversion, EnforceRange, Clamp };

// Convert a value to a boolean.
CORE_EXPORT bool toBooleanSlow(v8::Isolate*,
                               v8::Local<v8::Value>,
                               ExceptionState&);
inline bool toBoolean(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      ExceptionState& exceptionState) {
  if (LIKELY(value->IsBoolean()))
    return value.As<v8::Boolean>()->Value();
  return toBooleanSlow(isolate, value, exceptionState);
}

// Convert a value to a 8-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-byte
CORE_EXPORT int8_t toInt8(v8::Isolate*,
                          v8::Local<v8::Value>,
                          IntegerConversionConfiguration,
                          ExceptionState&);

// Convert a value to a 8-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-octet
CORE_EXPORT uint8_t toUInt8(v8::Isolate*,
                            v8::Local<v8::Value>,
                            IntegerConversionConfiguration,
                            ExceptionState&);

// Convert a value to a 16-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-short
CORE_EXPORT int16_t toInt16(v8::Isolate*,
                            v8::Local<v8::Value>,
                            IntegerConversionConfiguration,
                            ExceptionState&);

// Convert a value to a 16-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-short
CORE_EXPORT uint16_t toUInt16(v8::Isolate*,
                              v8::Local<v8::Value>,
                              IntegerConversionConfiguration,
                              ExceptionState&);

// Convert a value to a 32-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-long
CORE_EXPORT int32_t toInt32Slow(v8::Isolate*,
                                v8::Local<v8::Value>,
                                IntegerConversionConfiguration,
                                ExceptionState&);
inline int32_t toInt32(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       IntegerConversionConfiguration configuration,
                       ExceptionState& exceptionState) {
  // Fast case. The value is already a 32-bit integer.
  if (LIKELY(value->IsInt32()))
    return value.As<v8::Int32>()->Value();
  return toInt32Slow(isolate, value, configuration, exceptionState);
}

// Convert a value to a 32-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-long
CORE_EXPORT uint32_t toUInt32Slow(v8::Isolate*,
                                  v8::Local<v8::Value>,
                                  IntegerConversionConfiguration,
                                  ExceptionState&);
inline uint32_t toUInt32(v8::Isolate* isolate,
                         v8::Local<v8::Value> value,
                         IntegerConversionConfiguration configuration,
                         ExceptionState& exceptionState) {
  // Fast case. The value is already a 32-bit unsigned integer.
  if (LIKELY(value->IsUint32()))
    return value.As<v8::Uint32>()->Value();

  // Fast case. The value is a 32-bit signed integer with NormalConversion
  // configuration.
  if (LIKELY(value->IsInt32() && configuration == NormalConversion))
    return value.As<v8::Int32>()->Value();

  return toUInt32Slow(isolate, value, configuration, exceptionState);
}

// Convert a value to a 64-bit signed integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-long-long
CORE_EXPORT int64_t toInt64Slow(v8::Isolate*,
                                v8::Local<v8::Value>,
                                IntegerConversionConfiguration,
                                ExceptionState&);
inline int64_t toInt64(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       IntegerConversionConfiguration configuration,
                       ExceptionState& exceptionState) {
  // Clamping not supported for int64_t/long long int. See
  // Source/wtf/MathExtras.h.
  ASSERT(configuration != Clamp);

  // Fast case. The value is a 32-bit integer.
  if (LIKELY(value->IsInt32()))
    return value.As<v8::Int32>()->Value();

  return toInt64Slow(isolate, value, configuration, exceptionState);
}

// Convert a value to a 64-bit unsigned integer. The conversion fails if the
// value cannot be converted to a number or the range violated per WebIDL:
// http://www.w3.org/TR/WebIDL/#es-unsigned-long-long
CORE_EXPORT uint64_t toUInt64Slow(v8::Isolate*,
                                  v8::Local<v8::Value>,
                                  IntegerConversionConfiguration,
                                  ExceptionState&);
inline uint64_t toUInt64(v8::Isolate* isolate,
                         v8::Local<v8::Value> value,
                         IntegerConversionConfiguration configuration,
                         ExceptionState& exceptionState) {
  // Fast case. The value is a 32-bit unsigned integer.
  if (LIKELY(value->IsUint32()))
    return value.As<v8::Uint32>()->Value();

  if (LIKELY(value->IsInt32() && configuration == NormalConversion))
    return value.As<v8::Int32>()->Value();

  return toUInt64Slow(isolate, value, configuration, exceptionState);
}

// Convert a value to a double precision float, which might fail.
CORE_EXPORT double toDoubleSlow(v8::Isolate*,
                                v8::Local<v8::Value>,
                                ExceptionState&);
inline double toDouble(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exceptionState) {
  if (LIKELY(value->IsNumber()))
    return value.As<v8::Number>()->Value();
  return toDoubleSlow(isolate, value, exceptionState);
}

// Convert a value to a double precision float, throwing on non-finite values.
CORE_EXPORT double toRestrictedDouble(v8::Isolate*,
                                      v8::Local<v8::Value>,
                                      ExceptionState&);

// Convert a value to a single precision float, which might fail.
inline float toFloat(v8::Isolate* isolate,
                     v8::Local<v8::Value> value,
                     ExceptionState& exceptionState) {
  return static_cast<float>(toDouble(isolate, value, exceptionState));
}

// Convert a value to a single precision float, throwing on non-finite values.
CORE_EXPORT float toRestrictedFloat(v8::Isolate*,
                                    v8::Local<v8::Value>,
                                    ExceptionState&);

// Converts a value to a String, throwing if any code unit is outside 0-255.
CORE_EXPORT String toByteString(v8::Isolate*,
                                v8::Local<v8::Value>,
                                ExceptionState&);

// Converts a value to a String, replacing unmatched UTF-16 surrogates with
// replacement characters.
CORE_EXPORT String toUSVString(v8::Isolate*,
                               v8::Local<v8::Value>,
                               ExceptionState&);

inline v8::Local<v8::Boolean> v8Boolean(bool value, v8::Isolate* isolate) {
  return value ? v8::True(isolate) : v8::False(isolate);
}

inline double toCoreDate(v8::Isolate* isolate,
                         v8::Local<v8::Value> object,
                         ExceptionState& exceptionState) {
  if (object->IsNull())
    return std::numeric_limits<double>::quiet_NaN();
  if (!object->IsDate()) {
    exceptionState.throwTypeError("The provided value is not a Date.");
    return 0;
  }
  return object.As<v8::Date>()->ValueOf();
}

inline v8::MaybeLocal<v8::Value> v8DateOrNaN(v8::Isolate* isolate,
                                             double value) {
  ASSERT(isolate);
  return v8::Date::New(isolate->GetCurrentContext(), value);
}

// FIXME: Remove the special casing for NodeFilter and XPathNSResolver.
NodeFilter* toNodeFilter(v8::Local<v8::Value>,
                         v8::Local<v8::Object>,
                         ScriptState*);
XPathNSResolver* toXPathNSResolver(ScriptState*, v8::Local<v8::Value>);

bool toV8Sequence(v8::Local<v8::Value>,
                  uint32_t& length,
                  v8::Isolate*,
                  ExceptionState&);

template <typename T>
HeapVector<Member<T>> toMemberNativeArray(v8::Local<v8::Value> value,
                                          int argumentIndex,
                                          v8::Isolate* isolate,
                                          ExceptionState& exceptionState) {
  v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
  uint32_t length = 0;
  if (value->IsArray()) {
    length = v8::Local<v8::Array>::Cast(v8Value)->Length();
  } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
    if (!exceptionState.hadException())
      exceptionState.throwTypeError(
          ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex));
    return HeapVector<Member<T>>();
  }

  HeapVector<Member<T>> result;
  result.reserveInitialCapacity(length);
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
  v8::TryCatch block(isolate);
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> element;
    if (!v8Call(object->Get(isolate->GetCurrentContext(), i), element, block)) {
      exceptionState.rethrowV8Exception(block.Exception());
      return HeapVector<Member<T>>();
    }
    if (V8TypeOf<T>::Type::hasInstance(element, isolate)) {
      v8::Local<v8::Object> elementObject =
          v8::Local<v8::Object>::Cast(element);
      result.uncheckedAppend(V8TypeOf<T>::Type::toImpl(elementObject));
    } else {
      exceptionState.throwTypeError("Invalid Array element type");
      return HeapVector<Member<T>>();
    }
  }
  return result;
}

template <typename T>
HeapVector<Member<T>> toMemberNativeArray(v8::Local<v8::Value> value,
                                          const String& propertyName,
                                          v8::Isolate* isolate,
                                          ExceptionState& exceptionState) {
  v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
  uint32_t length = 0;
  if (value->IsArray()) {
    length = v8::Local<v8::Array>::Cast(v8Value)->Length();
  } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
    if (!exceptionState.hadException())
      exceptionState.throwTypeError(
          ExceptionMessages::notASequenceTypeProperty(propertyName));
    return HeapVector<Member<T>>();
  }

  HeapVector<Member<T>> result;
  result.reserveInitialCapacity(length);
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
  v8::TryCatch block(isolate);
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> element;
    if (!v8Call(object->Get(isolate->GetCurrentContext(), i), element, block)) {
      exceptionState.rethrowV8Exception(block.Exception());
      return HeapVector<Member<T>>();
    }
    if (V8TypeOf<T>::Type::hasInstance(element, isolate)) {
      v8::Local<v8::Object> elementObject =
          v8::Local<v8::Object>::Cast(element);
      result.uncheckedAppend(V8TypeOf<T>::Type::toImpl(elementObject));
    } else {
      exceptionState.throwTypeError("Invalid Array element type");
      return HeapVector<Member<T>>();
    }
  }
  return result;
}

// Converts a JavaScript value to an array as per the Web IDL specification:
// http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-array
template <typename VectorType>
VectorType toImplArray(v8::Local<v8::Value> value,
                       int argumentIndex,
                       v8::Isolate* isolate,
                       ExceptionState& exceptionState) {
  typedef typename VectorType::ValueType ValueType;
  typedef NativeValueTraits<ValueType> TraitsType;

  uint32_t length = 0;
  if (value->IsArray()) {
    length = v8::Local<v8::Array>::Cast(value)->Length();
  } else if (!toV8Sequence(value, length, isolate, exceptionState)) {
    if (!exceptionState.hadException())
      exceptionState.throwTypeError(
          ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex));
    return VectorType();
  }

  if (length > WTF::kGenericMaxDirectMapped / sizeof(ValueType)) {
    exceptionState.throwTypeError("Array length exceeds supported limit.");
    return VectorType();
  }

  VectorType result;
  result.reserveInitialCapacity(length);
  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  v8::TryCatch block(isolate);
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> element;
    if (!v8Call(object->Get(isolate->GetCurrentContext(), i), element, block)) {
      exceptionState.rethrowV8Exception(block.Exception());
      return VectorType();
    }
    result.uncheckedAppend(
        TraitsType::nativeValue(isolate, element, exceptionState));
    if (exceptionState.hadException())
      return VectorType();
  }
  return result;
}

template <typename VectorType>
VectorType toImplArray(const Vector<ScriptValue>& value,
                       v8::Isolate* isolate,
                       ExceptionState& exceptionState) {
  VectorType result;
  typedef typename VectorType::ValueType ValueType;
  typedef NativeValueTraits<ValueType> TraitsType;
  result.reserveInitialCapacity(value.size());
  for (unsigned i = 0; i < value.size(); ++i) {
    result.uncheckedAppend(
        TraitsType::nativeValue(isolate, value[i].v8Value(), exceptionState));
    if (exceptionState.hadException())
      return VectorType();
  }
  return result;
}

template <typename VectorType>
VectorType toImplArguments(const v8::FunctionCallbackInfo<v8::Value>& info,
                           int startIndex,
                           ExceptionState& exceptionState) {
  VectorType result;
  typedef typename VectorType::ValueType ValueType;
  typedef NativeValueTraits<ValueType> TraitsType;
  int length = info.Length();
  if (startIndex < length) {
    result.reserveInitialCapacity(length - startIndex);
    for (int i = startIndex; i < length; ++i) {
      result.uncheckedAppend(
          TraitsType::nativeValue(info.GetIsolate(), info[i], exceptionState));
      if (exceptionState.hadException())
        return VectorType();
    }
  }
  return result;
}

// Gets an iterator from an Object.
CORE_EXPORT v8::Local<v8::Object> getEsIterator(v8::Isolate*,
                                                v8::Local<v8::Object>,
                                                ExceptionState&);

// Validates that the passed object is a sequence type per WebIDL spec
// http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-sequence
inline bool toV8Sequence(v8::Local<v8::Value> value,
                         uint32_t& length,
                         v8::Isolate* isolate,
                         ExceptionState& exceptionState) {
  // Attempt converting to a sequence if the value is not already an array but
  // is any kind of object except for a native Date object or a native RegExp
  // object.
  ASSERT(!value->IsArray());
  // FIXME: Do we really need to special case Date and RegExp object?
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=22806
  if (!value->IsObject() || value->IsDate() || value->IsRegExp()) {
    // The caller is responsible for reporting a TypeError.
    return false;
  }

  v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
  v8::Local<v8::String> lengthSymbol = v8AtomicString(isolate, "length");

  // FIXME: The specification states that the length property should be used as
  // fallback, if value is not a platform object that supports indexed
  // properties. If it supports indexed properties, length should actually be
  // one greater than value's maximum indexed property index.
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> lengthValue;
  if (!v8Call(object->Get(isolate->GetCurrentContext(), lengthSymbol),
              lengthValue, block)) {
    exceptionState.rethrowV8Exception(block.Exception());
    return false;
  }

  if (lengthValue->IsUndefined() || lengthValue->IsNull()) {
    // The caller is responsible for reporting a TypeError.
    return false;
  }

  uint32_t sequenceLength;
  if (!v8Call(lengthValue->Uint32Value(isolate->GetCurrentContext()),
              sequenceLength, block)) {
    exceptionState.rethrowV8Exception(block.Exception());
    return false;
  }

  length = sequenceLength;
  return true;
}

template <>
struct NativeValueTraits<String> {
  static inline String nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    V8StringResource<> stringValue(value);
    if (!stringValue.prepare(exceptionState))
      return String();
    return stringValue;
  }
};

template <>
struct NativeValueTraits<AtomicString> {
  static inline AtomicString nativeValue(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value,
                                         ExceptionState& exceptionState) {
    V8StringResource<> stringValue(value);
    if (!stringValue.prepare(exceptionState))
      return AtomicString();
    return stringValue;
  }
};

template <>
struct NativeValueTraits<int> {
  static inline int nativeValue(v8::Isolate* isolate,
                                v8::Local<v8::Value> value,
                                ExceptionState& exceptionState) {
    return toInt32(isolate, value, NormalConversion, exceptionState);
  }
};

template <>
struct NativeValueTraits<unsigned> {
  static inline unsigned nativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exceptionState) {
    return toUInt32(isolate, value, NormalConversion, exceptionState);
  }
};

template <>
struct NativeValueTraits<float> {
  static inline float nativeValue(v8::Isolate* isolate,
                                  v8::Local<v8::Value> value,
                                  ExceptionState& exceptionState) {
    return toFloat(isolate, value, exceptionState);
  }
};

template <>
struct NativeValueTraits<double> {
  static inline double nativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exceptionState) {
    return toDouble(isolate, value, exceptionState);
  }
};

template <>
struct NativeValueTraits<v8::Local<v8::Value>> {
  static inline v8::Local<v8::Value> nativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exceptionState) {
    return value;
  }
};

template <>
struct NativeValueTraits<ScriptValue> {
  static inline ScriptValue nativeValue(v8::Isolate* isolate,
                                        v8::Local<v8::Value> value,
                                        ExceptionState& exceptionState) {
    return ScriptValue(ScriptState::current(isolate), value);
  }
};

template <typename T>
struct NativeValueTraits<Vector<T>> {
  static inline Vector<T> nativeValue(v8::Isolate* isolate,
                                      v8::Local<v8::Value> value,
                                      ExceptionState& exceptionState) {
    return toImplArray<Vector<T>>(value, 0, isolate, exceptionState);
  }
};

CORE_EXPORT v8::Isolate* toIsolate(ExecutionContext*);
CORE_EXPORT v8::Isolate* toIsolate(LocalFrame*);

CORE_EXPORT DOMWindow* toDOMWindow(v8::Isolate*, v8::Local<v8::Value>);
DOMWindow* toDOMWindow(v8::Local<v8::Context>);
LocalDOMWindow* enteredDOMWindow(v8::Isolate*);
CORE_EXPORT LocalDOMWindow* currentDOMWindow(v8::Isolate*);
CORE_EXPORT ExecutionContext* toExecutionContext(v8::Local<v8::Context>);
CORE_EXPORT void registerToExecutionContextForModules(
    ExecutionContext* (*toExecutionContextForModules)(v8::Local<v8::Context>));
CORE_EXPORT ExecutionContext* currentExecutionContext(v8::Isolate*);
CORE_EXPORT ExecutionContext* enteredExecutionContext(v8::Isolate*);

// Returns a V8 context associated with a ExecutionContext and a
// DOMWrapperWorld.  This method returns an empty context if there is no frame
// or the frame is already detached.
CORE_EXPORT v8::Local<v8::Context> toV8Context(ExecutionContext*,
                                               DOMWrapperWorld&);
// Returns a V8 context associated with a Frame and a DOMWrapperWorld.
// This method returns an empty context if the frame is already detached.
CORE_EXPORT v8::Local<v8::Context> toV8Context(Frame*, DOMWrapperWorld&);
// Like toV8Context but also returns the context if the frame is already
// detached.
CORE_EXPORT v8::Local<v8::Context> toV8ContextEvenIfDetached(Frame*,
                                                             DOMWrapperWorld&);

// Returns the frame object of the window object associated with
// a context, if the window is currently being displayed in a Frame.
CORE_EXPORT Frame* toFrameIfNotDetached(v8::Local<v8::Context>);

CORE_EXPORT EventTarget* toEventTarget(v8::Isolate*, v8::Local<v8::Value>);

// If 'storage' is non-null, it must be large enough to copy all bytes in the
// array buffer view into it.  Use allocateFlexibleArrayBufferStorage(v8Value)
// to allocate it using alloca() in the callers stack frame.
CORE_EXPORT void toFlexibleArrayBufferView(v8::Isolate*,
                                           v8::Local<v8::Value>,
                                           FlexibleArrayBufferView&,
                                           void* storage = nullptr);

// Converts a V8 value to an array (an IDL sequence) as per the WebIDL
// specification: http://heycam.github.io/webidl/#es-sequence
template <typename VectorType>
VectorType toImplSequence(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exceptionState) {
  using ValueType = typename VectorType::ValueType;

  if (!value->IsObject() || value->IsRegExp()) {
    exceptionState.throwTypeError(
        "The provided value cannot be converted to a sequence.");
    return VectorType();
  }

  v8::TryCatch block(isolate);
  v8::Local<v8::Object> iterator =
      getEsIterator(isolate, value.As<v8::Object>(), exceptionState);
  if (exceptionState.hadException())
    return VectorType();

  v8::Local<v8::String> nextKey = v8String(isolate, "next");
  v8::Local<v8::String> valueKey = v8String(isolate, "value");
  v8::Local<v8::String> doneKey = v8String(isolate, "done");
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  VectorType result;
  while (true) {
    v8::Local<v8::Value> next;
    if (!iterator->Get(context, nextKey).ToLocal(&next)) {
      exceptionState.rethrowV8Exception(block.Exception());
      return VectorType();
    }
    // TODO(bashi): Support callable objects.
    if (!next->IsObject() || !next.As<v8::Object>()->IsFunction()) {
      exceptionState.throwTypeError("Iterator.next should be callable.");
      return VectorType();
    }
    v8::Local<v8::Value> nextResult;
    if (!V8ScriptRunner::callFunction(next.As<v8::Function>(),
                                      toExecutionContext(context), iterator, 0,
                                      nullptr, isolate)
             .ToLocal(&nextResult)) {
      exceptionState.rethrowV8Exception(block.Exception());
      return VectorType();
    }
    if (!nextResult->IsObject()) {
      exceptionState.throwTypeError(
          "Iterator.next() did not return an object.");
      return VectorType();
    }
    v8::Local<v8::Object> resultObject = nextResult.As<v8::Object>();
    v8::Local<v8::Value> element;
    v8::Local<v8::Value> done;
    if (!resultObject->Get(context, valueKey).ToLocal(&element) ||
        !resultObject->Get(context, doneKey).ToLocal(&done)) {
      exceptionState.rethrowV8Exception(block.Exception());
      return VectorType();
    }
    v8::Local<v8::Boolean> doneBoolean;
    if (!done->ToBoolean(context).ToLocal(&doneBoolean)) {
      exceptionState.rethrowV8Exception(block.Exception());
      return VectorType();
    }
    if (doneBoolean->Value())
      break;
    result.push_back(NativeValueTraits<ValueType>::nativeValue(isolate, element,
                                                               exceptionState));
  }
  return result;
}

// If the current context causes out of memory, JavaScript setting
// is disabled and it returns true.
bool handleOutOfMemory();
void crashIfIsolateIsDead(v8::Isolate*);

inline bool isUndefinedOrNull(v8::Local<v8::Value> value) {
  return value.IsEmpty() || value->IsNull() || value->IsUndefined();
}
v8::Local<v8::Function> getBoundFunction(v8::Local<v8::Function>);

// FIXME: This will be soon embedded in the generated code.
template <typename Collection>
static void indexedPropertyEnumerator(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  Collection* collection =
      toScriptWrappable(info.Holder())->toImpl<Collection>();
  int length = collection->length();
  v8::Local<v8::Array> properties = v8::Array::New(info.GetIsolate(), length);
  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  for (int i = 0; i < length; ++i) {
    v8::Local<v8::Integer> integer = v8::Integer::New(info.GetIsolate(), i);
    if (!v8CallBoolean(properties->CreateDataProperty(context, i, integer)))
      return;
  }
  v8SetReturnValue(info, properties);
}

CORE_EXPORT bool isValidEnum(const String& value,
                             const char** validValues,
                             size_t length,
                             const String& enumName,
                             ExceptionState&);
CORE_EXPORT bool isValidEnum(const Vector<String>& values,
                             const char** validValues,
                             size_t length,
                             const String& enumName,
                             ExceptionState&);

// These methods store hidden values into an array that is stored in the
// internal field of a DOM wrapper.
bool addHiddenValueToArray(v8::Isolate*,
                           v8::Local<v8::Object>,
                           v8::Local<v8::Value>,
                           int cacheIndex);
void removeHiddenValueFromArray(v8::Isolate*,
                                v8::Local<v8::Object>,
                                v8::Local<v8::Value>,
                                int cacheIndex);
CORE_EXPORT void moveEventListenerToNewWrapper(v8::Isolate*,
                                               v8::Local<v8::Object>,
                                               EventListener* oldValue,
                                               v8::Local<v8::Value> newValue,
                                               int cacheIndex);

// Result values for platform object 'deleter' methods,
// http://www.w3.org/TR/WebIDL/#delete
enum DeleteResult { DeleteSuccess, DeleteReject, DeleteUnknownProperty };

class V8IsolateInterruptor final : public BlinkGCInterruptor {
 public:
  explicit V8IsolateInterruptor(v8::Isolate* isolate) : m_isolate(isolate) {}

  static void onInterruptCallback(v8::Isolate* isolate, void* data) {
    V8IsolateInterruptor* interruptor =
        reinterpret_cast<V8IsolateInterruptor*>(data);
    interruptor->onInterrupted();
  }

  void requestInterrupt() override {
    m_isolate->RequestInterrupt(&onInterruptCallback, this);
  }

 private:
  v8::Isolate* m_isolate;
};

typedef void (*InstallTemplateFunction)(
    v8::Isolate*,
    const DOMWrapperWorld&,
    v8::Local<v8::FunctionTemplate> interfaceTemplate);

// Freeze a V8 object. The type of the first parameter and the return value is
// intentionally v8::Value so that this function can wrap ToV8().
// If the argument isn't an object, this will crash.
CORE_EXPORT v8::Local<v8::Value> freezeV8Object(v8::Local<v8::Value>,
                                                v8::Isolate*);

CORE_EXPORT v8::Local<v8::Value> fromJSONString(v8::Isolate*,
                                                const String& stringifiedJSON,
                                                ExceptionState&);
}  // namespace blink

#endif  // V8Binding_h
