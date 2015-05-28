// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8BindingForModules_h
#define V8BindingForModules_h

#include "bindings/core/v8/V8Binding.h"
#include "modules/webdatabase/sqlite/SQLValue.h"

namespace blink {

class IDBAny;
class IDBKey;
class IDBKeyPath;
class IDBKeyRange;
class SerializedScriptValue;
class SharedBuffer;
class WebBlobInfo;

// Exposed for unit testing:
bool injectV8KeyIntoV8Value(v8::Isolate*, v8::Local<v8::Value> key, v8::Local<v8::Value>, const IDBKeyPath&);

// For use by Source/modules/indexeddb:
bool canInjectIDBKeyIntoScriptValue(v8::Isolate*, const ScriptValue&, const IDBKeyPath&);
ScriptValue deserializeScriptValue(ScriptState*, SerializedScriptValue*, const Vector<blink::WebBlobInfo>*);

#if ENABLE(ASSERT)
void assertPrimaryKeyValidOrInjectable(ScriptState*, PassRefPtr<SharedBuffer>, const Vector<blink::WebBlobInfo>*, IDBKey*, const IDBKeyPath&);
#endif

template <>
struct NativeValueTraits<SQLValue> {
    static SQLValue nativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

template <>
struct NativeValueTraits<IDBKey*> {
    static IDBKey* nativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
    static IDBKey* nativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&, const IDBKeyPath&);
};

template <>
struct NativeValueTraits<IDBKeyRange*> {
    static IDBKeyRange* nativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
};

} // namespace blink

#endif // V8BindingForModules_h
