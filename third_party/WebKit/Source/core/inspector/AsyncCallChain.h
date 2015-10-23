// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AsyncCallChain_h
#define AsyncCallChain_h

#include "platform/heap/Handle.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include <v8.h>

namespace blink {

class AsyncCallStack final : public RefCountedWillBeGarbageCollectedFinalized<AsyncCallStack> {
public:
    AsyncCallStack(const String&, v8::Local<v8::Object>);
    ~AsyncCallStack();
    DEFINE_INLINE_TRACE() { }
    String description() const { return m_description; }
    v8::Local<v8::Object> callFrames(v8::Isolate* isolate) const { return v8::Local<v8::Object>::New(isolate, m_callFrames); }
private:
    String m_description;
    v8::Global<v8::Object> m_callFrames;
};

using AsyncCallStackVector = WillBeHeapDeque<RefPtrWillBeMember<AsyncCallStack>, 4>;

class AsyncCallChain final : public RefCountedWillBeGarbageCollectedFinalized<AsyncCallChain> {
public:
    static PassRefPtrWillBeRawPtr<AsyncCallChain> create(PassRefPtrWillBeRawPtr<AsyncCallStack>, AsyncCallChain* prevChain, unsigned asyncCallChainMaxLength);
    ~AsyncCallChain();
    const AsyncCallStackVector& callStacks() const { return m_callStacks; }
    DECLARE_TRACE();

private:
    AsyncCallChain(PassRefPtrWillBeRawPtr<AsyncCallStack>, AsyncCallChain* prevChain, unsigned asyncCallChainMaxLength);

    AsyncCallStackVector m_callStacks;
};

} // namespace blink


#endif // !defined(AsyncCallChain_h)
