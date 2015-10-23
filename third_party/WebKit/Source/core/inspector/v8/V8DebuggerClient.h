// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8DebuggerClient_h
#define V8DebuggerClient_h

#include "core/CoreExport.h"

#include <v8.h>

namespace blink {

class V8DebuggerListener;

class CORE_EXPORT V8DebuggerClient {
public:
    virtual ~V8DebuggerClient() { }
    virtual v8::Local<v8::Object> compileDebuggerScript() = 0;
    virtual void runMessageLoopOnPause(v8::Local<v8::Context>) = 0;
    virtual void quitMessageLoopOnPause() = 0;
};

} // namespace blink


#endif // V8DebuggerClient_h
