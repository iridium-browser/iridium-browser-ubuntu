/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef V8DebuggerImpl_h
#define V8DebuggerImpl_h

#include "core/CoreExport.h"
#include "core/InspectorTypeBuilder.h"
#include "core/inspector/v8/V8Debugger.h"
#include "core/inspector/v8/V8DebuggerListener.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"

#include <v8-debug.h>
#include <v8.h>

namespace blink {

class V8DebuggerListener;
class JavaScriptCallFrame;

class CORE_EXPORT V8DebuggerImpl : public V8Debugger {
    WTF_MAKE_NONCOPYABLE(V8DebuggerImpl);
public:
    V8DebuggerImpl(v8::Isolate*, V8DebuggerClient*);
    ~V8DebuggerImpl() override;

    bool enabled() const override;

    void addListener(int contextGroupId, V8DebuggerListener*) override;
    void removeListener(int contextGroupId) override;

    String setBreakpoint(const String& sourceID, const ScriptBreakpoint&, int* actualLineNumber, int* actualColumnNumber, bool interstatementLocation) override;
    void removeBreakpoint(const String& breakpointId) override;
    void setBreakpointsActivated(bool) override;

    PauseOnExceptionsState pauseOnExceptionsState() override;
    void setPauseOnExceptionsState(PauseOnExceptionsState) override;

    void setPauseOnNextStatement(bool) override;
    bool pausingOnNextStatement() override;
    bool canBreakProgram() override;
    void breakProgram() override;
    void continueProgram() override;
    void stepIntoStatement() override;
    void stepOverStatement() override;
    void stepOutOfFunction() override;
    void clearStepping() override;

    bool setScriptSource(const String& sourceID, const String& newContent, bool preview, String* error, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>&, v8::Global<v8::Object>* newCallFrames, TypeBuilder::OptOutput<bool>* stackChanged) override;
    v8::Local<v8::Object> currentCallFrames() override;
    v8::Local<v8::Object> currentCallFramesForAsyncStack() override;
    PassRefPtr<JavaScriptCallFrame> callFrameNoScopes(int index) override;
    int frameCount() override;

    bool isPaused() override;

    v8::Local<v8::Value> functionScopes(v8::Local<v8::Function>) override;
    v8::Local<v8::Value> generatorObjectDetails(v8::Local<v8::Object>&) override;
    v8::Local<v8::Value> collectionEntries(v8::Local<v8::Object>&) override;
    v8::MaybeLocal<v8::Value> setFunctionVariableValue(v8::Local<v8::Value> functionValue, int scopeNumber, const String& variableName, v8::Local<v8::Value> newValue) override;

    v8::Isolate* isolate() const override { return m_isolate; }

private:
    void enable();
    void disable();
    // Each script inherits debug data from v8::Context where it has been compiled.
    // Only scripts whose debug data matches |contextGroupId| will be reported.
    // Passing 0 will result in reporting all scripts.
    void getCompiledScripts(int contextGroupId, Vector<V8DebuggerListener::ParsedScript>&);
    V8DebuggerListener* getListenerForContext(v8::Local<v8::Context>);

    void compileDebuggerScript();
    v8::MaybeLocal<v8::Value> callDebuggerMethod(const char* functionName, int argc, v8::Local<v8::Value> argv[]);
    v8::Local<v8::Context> debuggerContext() const;
    void clearBreakpoints();

    V8DebuggerListener::ParsedScript createParsedScript(v8::Local<v8::Object> sourceObject, CompileResult);

    static void breakProgramCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    void handleProgramBreak(v8::Local<v8::Context> pausedContext, v8::Local<v8::Object> executionState, v8::Local<v8::Value> exception, v8::Local<v8::Array> hitBreakpoints, bool isPromiseRejection = false);
    static void v8DebugEventCallback(const v8::Debug::EventDetails&);
    v8::Local<v8::Value> callInternalGetterFunction(v8::Local<v8::Object>, const char* functionName);
    void handleV8DebugEvent(const v8::Debug::EventDetails&);

    v8::Local<v8::String> v8InternalizedString(const char*) const;

    enum ScopeInfoDetails {
        AllScopes,
        FastAsyncScopes,
        NoScopes // Should be the last option.
    };
    v8::Local<v8::Object> currentCallFramesInner(ScopeInfoDetails);
    PassRefPtr<JavaScriptCallFrame> wrapCallFrames(int maximumLimit, ScopeInfoDetails);
    void handleV8AsyncTaskEvent(V8DebuggerListener*, v8::Local<v8::Context>, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData);
    void handleV8PromiseEvent(V8DebuggerListener*, v8::Local<v8::Context>, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData);

    v8::Isolate* m_isolate;
    V8DebuggerClient* m_client;
    using ListenersMap = HashMap<int, V8DebuggerListener*>;
    ListenersMap m_listenersMap;
    bool m_breakpointsActivated;
    v8::UniquePersistent<v8::FunctionTemplate> m_breakProgramCallbackTemplate;
    v8::UniquePersistent<v8::Object> m_debuggerScript;
    v8::UniquePersistent<v8::Context> m_debuggerContext;
    v8::UniquePersistent<v8::FunctionTemplate> m_callFrameWrapperTemplate;
    v8::Local<v8::Object> m_executionState;
    v8::Local<v8::Context> m_pausedContext;
    bool m_runningNestedMessageLoop;
};

} // namespace blink


#endif // V8DebuggerImpl_h
