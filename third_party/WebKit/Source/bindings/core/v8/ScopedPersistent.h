/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef ScopedPersistent_h
#define ScopedPersistent_h

#include <memory>

#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "v8/include/v8.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

template <typename T>
class ScopedPersistent {
  USING_FAST_MALLOC(ScopedPersistent);
  WTF_MAKE_NONCOPYABLE(ScopedPersistent);

 public:
  ScopedPersistent() {}

  ScopedPersistent(v8::Isolate* isolate, v8::Local<T> handle)
      : m_handle(isolate, handle) {}

  ScopedPersistent(v8::Isolate* isolate, v8::MaybeLocal<T> maybe) {
    v8::Local<T> local;
    if (maybe.ToLocal(&local))
      m_handle.Reset(isolate, local);
  }

  virtual ~ScopedPersistent() { clear(); }

  ALWAYS_INLINE v8::Local<T> newLocal(v8::Isolate* isolate) const {
    return v8::Local<T>::New(isolate, m_handle);
  }

  // If you don't need to get weak callback, use setPhantom instead.
  // setPhantom is faster than setWeak.
  template <typename P>
  void setWeak(P* parameters,
               void (*callback)(const v8::WeakCallbackInfo<P>&),
               v8::WeakCallbackType type = v8::WeakCallbackType::kParameter) {
    m_handle.SetWeak(parameters, callback, type);
  }

  // Turns this handle into a weak phantom handle without
  // finalization callback.
  void setPhantom() { m_handle.SetWeak(); }

  void clearWeak() { m_handle.template ClearWeak<void>(); }

  bool isEmpty() const { return m_handle.IsEmpty(); }
  bool isWeak() const { return m_handle.IsWeak(); }

  virtual void set(v8::Isolate* isolate, v8::Local<T> handle) {
    m_handle.Reset(isolate, handle);
  }

  // Note: This is clear in the std::unique_ptr sense, not the v8::Handle sense.
  void clear() { m_handle.Reset(); }

  bool operator==(const ScopedPersistent<T>& other) {
    return m_handle == other.m_handle;
  }

  template <class S>
  bool operator==(const v8::Local<S> other) const {
    return m_handle == other;
  }

  ALWAYS_INLINE v8::Persistent<T>& get() { return m_handle; }

 private:
  v8::Persistent<T> m_handle;
};

}  // namespace blink

#endif  // ScopedPersistent_h
