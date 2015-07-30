/*
 * Copyright (c) 2010-2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/core/v8/ScriptDebugServer.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8JavaScriptCallFrame.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/inspector/JavaScriptCallFrame.h"
#include "core/inspector/ScriptDebugListener.h"
#include "platform/JSONValues.h"
#include "wtf/Deque.h"
#include "wtf/StdLibExtras.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/Vector.h"
#include "wtf/dtoa/utils.h"
#include "wtf/text/CString.h"

namespace blink {

namespace {
const char stepIntoV8MethodName[] = "stepIntoStatement";
const char stepOutV8MethodName[] = "stepOutOfFunction";
}

class ScriptDebugServer::ThreadSafeTaskQueue {
    WTF_MAKE_NONCOPYABLE(ThreadSafeTaskQueue);
public:
    ThreadSafeTaskQueue() { }
    PassOwnPtr<Task> tryTake()
    {
        MutexLocker lock(m_mutex);
        if (m_queue.isEmpty())
            return nullptr;
        return m_queue.takeFirst();
    }
    void append(PassOwnPtr<Task> task)
    {
        MutexLocker lock(m_mutex);
        m_queue.append(task);
    }
private:
    Mutex m_mutex;
    Deque<OwnPtr<Task>> m_queue;
};

v8::MaybeLocal<v8::Value> ScriptDebugServer::callDebuggerMethod(const char* functionName, int argc, v8::Local<v8::Value> argv[])
{
    v8::Local<v8::Object> debuggerScript = debuggerScriptLocal();
    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(debuggerScript->Get(v8InternalizedString(functionName)));
    ASSERT(m_isolate->InContext());
    return V8ScriptRunner::callInternalFunction(function, debuggerScript, argc, argv, m_isolate);
}

ScriptDebugServer::ScriptDebugServer(v8::Isolate* isolate, Client* client)
    : m_isolate(isolate)
    , m_client(client)
    , m_breakpointsActivated(true)
    , m_runningNestedMessageLoop(false)
    , m_taskQueue(adoptPtr(new ThreadSafeTaskQueue))
{
}

ScriptDebugServer::~ScriptDebugServer()
{
}

DEFINE_TRACE(ScriptDebugServer)
{
}

void ScriptDebugServer::enable()
{
    ASSERT(!enabled());
    v8::HandleScope scope(m_isolate);
    v8::Debug::SetDebugEventListener(&ScriptDebugServer::v8DebugEventCallback, v8::External::New(m_isolate, this));
    m_debuggerContext.Reset(m_isolate, v8::Debug::GetDebugContext());
    compileDebuggerScript();
}

void ScriptDebugServer::disable()
{
    ASSERT(enabled());
    clearBreakpoints();
    m_debuggerScript.Reset();
    m_debuggerContext.Reset();
    v8::Debug::SetDebugEventListener(nullptr);
}

bool ScriptDebugServer::enabled() const
{
    return !m_debuggerScript.IsEmpty();
}

void ScriptDebugServer::setContextDebugData(v8::Local<v8::Context> context, const String& contextDebugData)
{
    v8::HandleScope scope(context->GetIsolate());
    v8::Context::Scope contextScope(context);
    context->SetEmbedderData(static_cast<int>(gin::kDebugIdIndex), v8String(context->GetIsolate(), contextDebugData));
}

void ScriptDebugServer::reportCompiledScripts(const String& contextDebugDataSubstring, ScriptDebugListener* listener)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> debuggerScript = debuggerScriptLocal();
    ASSERT(!debuggerScript->IsUndefined());
    v8::Local<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(debuggerScript->Get(v8InternalizedString("getScripts")));
    v8::Local<v8::Value> argv[] = { v8String(m_isolate, contextDebugDataSubstring) };
    v8::Local<v8::Value> value;
    if (!V8ScriptRunner::callInternalFunction(getScriptsFunction, debuggerScript, WTF_ARRAY_LENGTH(argv), argv, m_isolate).ToLocal(&value))
        return;
    ASSERT(value->IsArray());
    v8::Local<v8::Array> scriptsArray = v8::Local<v8::Array>::Cast(value);
    for (unsigned i = 0; i < scriptsArray->Length(); ++i)
        dispatchDidParseSource(listener, v8::Local<v8::Object>::Cast(scriptsArray->Get(v8::Integer::New(m_isolate, i))), CompileSuccess);
}

String ScriptDebugServer::setBreakpoint(const String& sourceID, const ScriptBreakpoint& scriptBreakpoint, int* actualLineNumber, int* actualColumnNumber, bool interstatementLocation)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("sourceID"), v8String(m_isolate, sourceID));
    info->Set(v8InternalizedString("lineNumber"), v8::Integer::New(m_isolate, scriptBreakpoint.lineNumber));
    info->Set(v8InternalizedString("columnNumber"), v8::Integer::New(m_isolate, scriptBreakpoint.columnNumber));
    info->Set(v8InternalizedString("interstatementLocation"), v8Boolean(interstatementLocation, m_isolate));
    info->Set(v8InternalizedString("condition"), v8String(m_isolate, scriptBreakpoint.condition));

    v8::Local<v8::Function> setBreakpointFunction = v8::Local<v8::Function>::Cast(debuggerScriptLocal()->Get(v8InternalizedString("setBreakpoint")));
    v8::Local<v8::Value> breakpointId = v8::Debug::Call(setBreakpointFunction, info);
    if (breakpointId.IsEmpty() || !breakpointId->IsString())
        return "";
    *actualLineNumber = info->Get(v8InternalizedString("lineNumber"))->Int32Value();
    *actualColumnNumber = info->Get(v8InternalizedString("columnNumber"))->Int32Value();
    return toCoreString(breakpointId.As<v8::String>());
}

void ScriptDebugServer::removeBreakpoint(const String& breakpointId)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("breakpointId"), v8String(m_isolate, breakpointId));

    v8::Local<v8::Function> removeBreakpointFunction = v8::Local<v8::Function>::Cast(debuggerScriptLocal()->Get(v8InternalizedString("removeBreakpoint")));
    v8::Debug::Call(removeBreakpointFunction, info);
}

void ScriptDebugServer::clearBreakpoints()
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Function> clearBreakpoints = v8::Local<v8::Function>::Cast(debuggerScriptLocal()->Get(v8InternalizedString("clearBreakpoints")));
    v8::Debug::Call(clearBreakpoints);
}

void ScriptDebugServer::setBreakpointsActivated(bool activated)
{
    if (!enabled()) {
        ASSERT_NOT_REACHED();
        return;
    }
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("enabled"), v8::Boolean::New(m_isolate, activated));
    v8::Local<v8::Function> setBreakpointsActivated = v8::Local<v8::Function>::Cast(debuggerScriptLocal()->Get(v8InternalizedString("setBreakpointsActivated")));
    v8::Debug::Call(setBreakpointsActivated, info);

    m_breakpointsActivated = activated;
}

ScriptDebugServer::PauseOnExceptionsState ScriptDebugServer::pauseOnExceptionsState()
{
    ASSERT(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8Undefined() };
    v8::Local<v8::Value> result = callDebuggerMethod("pauseOnExceptionsState", 0, argv).ToLocalChecked();
    return static_cast<ScriptDebugServer::PauseOnExceptionsState>(result->Int32Value());
}

void ScriptDebugServer::setPauseOnExceptionsState(PauseOnExceptionsState pauseOnExceptionsState)
{
    ASSERT(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8::Int32::New(m_isolate, pauseOnExceptionsState) };
    callDebuggerMethod("setPauseOnExceptionsState", 1, argv);
}

void ScriptDebugServer::setPauseOnNextStatement(bool pause)
{
    ASSERT(!m_runningNestedMessageLoop);
    if (pause)
        v8::Debug::DebugBreak(m_isolate);
    else
        v8::Debug::CancelDebugBreak(m_isolate);
}

bool ScriptDebugServer::pausingOnNextStatement()
{
    return v8::Debug::CheckDebugBreak(m_isolate);
}

bool ScriptDebugServer::canBreakProgram()
{
    if (!m_breakpointsActivated)
        return false;
    return m_isolate->InContext();
}

void ScriptDebugServer::breakProgram()
{
    if (isPaused()) {
        ASSERT(!m_runningNestedMessageLoop);
        v8::Local<v8::Value> exception;
        v8::Local<v8::Array> hitBreakpoints;
        handleProgramBreak(m_pausedScriptState.get(), m_executionState, exception, hitBreakpoints);
        return;
    }

    if (!canBreakProgram())
        return;

    v8::HandleScope scope(m_isolate);
    if (m_breakProgramCallbackTemplate.IsEmpty()) {
        v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(m_isolate);
        templ->SetCallHandler(&ScriptDebugServer::breakProgramCallback, v8::External::New(m_isolate, this));
        m_breakProgramCallbackTemplate.Reset(m_isolate, templ);
    }

    v8::Local<v8::Function> breakProgramFunction = v8::Local<v8::FunctionTemplate>::New(m_isolate, m_breakProgramCallbackTemplate)->GetFunction();
    v8::Debug::Call(breakProgramFunction);
}

void ScriptDebugServer::continueProgram()
{
    if (isPaused())
        m_client->quitMessageLoopOnPause();
    m_pausedScriptState.clear();
    m_executionState.Clear();
}

void ScriptDebugServer::stepIntoStatement()
{
    ASSERT(isPaused());
    ASSERT(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod(stepIntoV8MethodName, 1, argv);
    continueProgram();
}

void ScriptDebugServer::stepOverStatement()
{
    ASSERT(isPaused());
    ASSERT(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod("stepOverStatement", 1, argv);
    continueProgram();
}

void ScriptDebugServer::stepOutOfFunction()
{
    ASSERT(isPaused());
    ASSERT(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod(stepOutV8MethodName, 1, argv);
    continueProgram();
}

void ScriptDebugServer::clearStepping()
{
    ASSERT(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8Undefined() };
    callDebuggerMethod("clearStepping", 0, argv);
}

bool ScriptDebugServer::setScriptSource(const String& sourceID, const String& newContent, bool preview, String* error, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>& errorData, ScriptValue* newCallFrames, RefPtr<JSONObject>* result)
{
    class EnableLiveEditScope {
    public:
        explicit EnableLiveEditScope(v8::Isolate* isolate) : m_isolate(isolate) { v8::Debug::SetLiveEditEnabled(m_isolate, true); }
        ~EnableLiveEditScope() { v8::Debug::SetLiveEditEnabled(m_isolate, false); }
    private:
        v8::Isolate* m_isolate;
    };

    ASSERT(enabled());
    v8::HandleScope scope(m_isolate);

    OwnPtr<v8::Context::Scope> contextScope;
    if (!isPaused())
        contextScope = adoptPtr(new v8::Context::Scope(debuggerContext()));

    v8::Local<v8::Value> argv[] = { v8String(m_isolate, sourceID), v8String(m_isolate, newContent), v8Boolean(preview, m_isolate) };

    v8::Local<v8::Value> v8result;
    {
        EnableLiveEditScope enableLiveEditScope(m_isolate);
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(false);
        v8::MaybeLocal<v8::Value> maybeResult = callDebuggerMethod("liveEditScriptSource", 3, argv);
        if (tryCatch.HasCaught()) {
            v8::Local<v8::Message> message = tryCatch.Message();
            if (!message.IsEmpty())
                *error = toCoreStringWithUndefinedOrNullCheck(message->Get());
            else
                *error = "Unknown error.";
            return false;
        }
        v8result = maybeResult.ToLocalChecked();
    }
    ASSERT(!v8result.IsEmpty());
    v8::Local<v8::Object> resultTuple = v8result->ToObject(m_isolate);
    int code = static_cast<int>(resultTuple->Get(0)->ToInteger(m_isolate)->Value());
    switch (code) {
    case 0:
        {
            v8::Local<v8::Value> normalResult = resultTuple->Get(1);
            NonThrowableExceptionState exceptionState;
            RefPtr<JSONValue> jsonResult = ScriptValue::to<JSONValuePtr>(m_isolate, normalResult, exceptionState);
            if (jsonResult)
                *result = jsonResult->asObject();
            // Call stack may have changed after if the edited function was on the stack.
            if (!preview && isPaused())
                *newCallFrames = currentCallFrames();
            return true;
        }
    // Compile error.
    case 1:
        {
            RefPtr<TypeBuilder::Debugger::SetScriptSourceError::CompileError> compileError =
                TypeBuilder::Debugger::SetScriptSourceError::CompileError::create()
                    .setMessage(toCoreStringWithUndefinedOrNullCheck(resultTuple->Get(2)))
                    .setLineNumber(resultTuple->Get(3)->ToInteger(m_isolate)->Value())
                    .setColumnNumber(resultTuple->Get(4)->ToInteger(m_isolate)->Value());

            *error = toCoreStringWithUndefinedOrNullCheck(resultTuple->Get(1));
            errorData = TypeBuilder::Debugger::SetScriptSourceError::create();
            errorData->setCompileError(compileError);
            return false;
        }
    }
    *error = "Unknown error.";
    return false;
}

int ScriptDebugServer::frameCount()
{
    ASSERT(isPaused());
    ASSERT(!m_executionState.IsEmpty());
    v8::Local<v8::Value> argv[] = { m_executionState };
    v8::Local<v8::Value> result = callDebuggerMethod("frameCount", WTF_ARRAY_LENGTH(argv), argv).ToLocalChecked();
    if (result->IsInt32())
        return result->Int32Value();
    return 0;
}

PassRefPtrWillBeRawPtr<JavaScriptCallFrame> ScriptDebugServer::toJavaScriptCallFrameUnsafe(const ScriptValue& value)
{
    if (value.isEmpty())
        return nullptr;
    ScriptState* scriptState = value.scriptState();
    if (!scriptState || !scriptState->contextIsValid())
        return nullptr;
    ScriptState::Scope scope(scriptState);
    ASSERT(value.isObject());
    return V8JavaScriptCallFrame::toImpl(v8::Local<v8::Object>::Cast(value.v8ValueUnsafe()));
}

PassRefPtrWillBeRawPtr<JavaScriptCallFrame> ScriptDebugServer::wrapCallFrames(int maximumLimit, ScopeInfoDetails scopeDetails)
{
    const int scopeBits = 2;
    static_assert(NoScopes < (1 << scopeBits), "there must be enough bits to encode ScopeInfoDetails");

    ASSERT(maximumLimit >= 0);
    int data = (maximumLimit << scopeBits) | scopeDetails;
    v8::Local<v8::Value> currentCallFrameV8;
    if (m_executionState.IsEmpty()) {
        v8::Local<v8::Function> currentCallFrameFunction = v8::Local<v8::Function>::Cast(debuggerScriptLocal()->Get(v8InternalizedString("currentCallFrame")));
        currentCallFrameV8 = v8::Debug::Call(currentCallFrameFunction, v8::Integer::New(m_isolate, data));
    } else {
        v8::Local<v8::Value> argv[] = { m_executionState, v8::Integer::New(m_isolate, data) };
        currentCallFrameV8 = callDebuggerMethod("currentCallFrame", WTF_ARRAY_LENGTH(argv), argv).ToLocalChecked();
    }
    ASSERT(!currentCallFrameV8.IsEmpty());
    if (!currentCallFrameV8->IsObject())
        return nullptr;
    return JavaScriptCallFrame::create(debuggerContext(), v8::Local<v8::Object>::Cast(currentCallFrameV8));
}

ScriptValue ScriptDebugServer::currentCallFramesInner(ScopeInfoDetails scopeDetails)
{
    if (!m_isolate->InContext())
        return ScriptValue();
    v8::HandleScope handleScope(m_isolate);

    // Filter out stack traces entirely consisting of V8's internal scripts.
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(m_isolate, 1);
    if (!stackTrace->GetFrameCount())
        return ScriptValue();

    RefPtrWillBeRawPtr<JavaScriptCallFrame> currentCallFrame = wrapCallFrames(0, scopeDetails);
    if (!currentCallFrame)
        return ScriptValue();

    ScriptState* scriptState = m_pausedScriptState ? m_pausedScriptState.get() : ScriptState::current(m_isolate);
    ScriptState::Scope scope(scriptState);
    return ScriptValue(scriptState, toV8(currentCallFrame.release(), scriptState->context()->Global(), m_isolate));
}

ScriptValue ScriptDebugServer::currentCallFrames()
{
    return currentCallFramesInner(AllScopes);
}

ScriptValue ScriptDebugServer::currentCallFramesForAsyncStack()
{
    return currentCallFramesInner(FastAsyncScopes);
}

PassRefPtrWillBeRawPtr<JavaScriptCallFrame> ScriptDebugServer::callFrameNoScopes(int index)
{
    if (!m_isolate->InContext())
        return nullptr;
    v8::HandleScope handleScope(m_isolate);

    v8::Local<v8::Value> currentCallFrameV8;
    if (m_executionState.IsEmpty()) {
        v8::Local<v8::Function> currentCallFrameFunction = v8::Local<v8::Function>::Cast(debuggerScriptLocal()->Get(v8InternalizedString("currentCallFrameByIndex")));
        currentCallFrameV8 = v8::Debug::Call(currentCallFrameFunction, v8::Integer::New(m_isolate, index));
    } else {
        v8::Local<v8::Value> argv[] = { m_executionState, v8::Integer::New(m_isolate, index) };
        currentCallFrameV8 = callDebuggerMethod("currentCallFrameByIndex", WTF_ARRAY_LENGTH(argv), argv).ToLocalChecked();
    }
    ASSERT(!currentCallFrameV8.IsEmpty());
    if (!currentCallFrameV8->IsObject())
        return nullptr;
    return JavaScriptCallFrame::create(debuggerContext(), v8::Local<v8::Object>::Cast(currentCallFrameV8));
}

void ScriptDebugServer::interruptAndRun(PassOwnPtr<Task> task)
{
    m_taskQueue->append(task);
    m_isolate->RequestInterrupt(&v8InterruptCallback, this);
}

void ScriptDebugServer::runPendingTasks()
{
    if (!enabled())
        return;
    while (true) {
        OwnPtr<Task> task = m_taskQueue->tryTake();
        if (!task)
            return;
        task->run();
    }
}

static ScriptDebugServer* toScriptDebugServer(v8::Local<v8::Value> data)
{
    void* p = v8::Local<v8::External>::Cast(data)->Value();
    return static_cast<ScriptDebugServer*>(p);
}

void ScriptDebugServer::breakProgramCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ASSERT(2 == info.Length());
    ScriptDebugServer* thisPtr = toScriptDebugServer(info.Data());
    ScriptState* pausedScriptState = ScriptState::current(thisPtr->m_isolate);
    v8::Local<v8::Value> exception;
    v8::Local<v8::Array> hitBreakpoints;
    thisPtr->handleProgramBreak(pausedScriptState, v8::Local<v8::Object>::Cast(info[0]), exception, hitBreakpoints);
}

void ScriptDebugServer::handleProgramBreak(ScriptState* pausedScriptState, v8::Local<v8::Object> executionState, v8::Local<v8::Value> exception, v8::Local<v8::Array> hitBreakpointNumbers, bool isPromiseRejection)
{
    // Don't allow nested breaks.
    if (m_runningNestedMessageLoop)
        return;

    ScriptDebugListener* listener = m_client->getDebugListenerForContext(pausedScriptState->context());
    if (!listener)
        return;

    Vector<String> breakpointIds;
    if (!hitBreakpointNumbers.IsEmpty()) {
        breakpointIds.resize(hitBreakpointNumbers->Length());
        for (size_t i = 0; i < hitBreakpointNumbers->Length(); i++) {
            v8::Local<v8::Value> hitBreakpointNumber = hitBreakpointNumbers->Get(i);
            ASSERT(!hitBreakpointNumber.IsEmpty() && hitBreakpointNumber->IsInt32());
            breakpointIds[i] = String::number(hitBreakpointNumber->Int32Value());
        }
    }

    m_pausedScriptState = pausedScriptState;
    m_executionState = executionState;
    ScriptDebugListener::SkipPauseRequest result = listener->didPause(pausedScriptState, currentCallFrames(), ScriptValue(pausedScriptState, exception), breakpointIds, isPromiseRejection);
    if (result == ScriptDebugListener::NoSkip) {
        m_runningNestedMessageLoop = true;
        m_client->runMessageLoopOnPause(pausedScriptState->context());
        m_runningNestedMessageLoop = false;
    }
    m_pausedScriptState.clear();
    m_executionState.Clear();

    if (result == ScriptDebugListener::StepFrame) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod("stepFrameStatement", 1, argv);
    } else if (result == ScriptDebugListener::StepInto) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod(stepIntoV8MethodName, 1, argv);
    } else if (result == ScriptDebugListener::StepOut) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod(stepOutV8MethodName, 1, argv);
    }
}

void ScriptDebugServer::v8InterruptCallback(v8::Isolate*, void* data)
{
    ScriptDebugServer* server = static_cast<ScriptDebugServer*>(data);
    server->runPendingTasks();
}

void ScriptDebugServer::v8DebugEventCallback(const v8::Debug::EventDetails& eventDetails)
{
    ScriptDebugServer* thisPtr = toScriptDebugServer(eventDetails.GetCallbackData());
    thisPtr->handleV8DebugEvent(eventDetails);
}

v8::Local<v8::Value> ScriptDebugServer::callInternalGetterFunction(v8::Local<v8::Object> object, const char* functionName)
{
    v8::Local<v8::Value> getterValue = object->Get(v8InternalizedString(functionName));
    ASSERT(!getterValue.IsEmpty() && getterValue->IsFunction());
    return V8ScriptRunner::callInternalFunction(v8::Local<v8::Function>::Cast(getterValue), object, 0, 0, m_isolate).ToLocalChecked();
}

void ScriptDebugServer::handleV8DebugEvent(const v8::Debug::EventDetails& eventDetails)
{
    if (!enabled())
        return;
    v8::DebugEvent event = eventDetails.GetEvent();
    if (event != v8::AsyncTaskEvent && event != v8::Break && event != v8::Exception && event != v8::AfterCompile && event != v8::BeforeCompile && event != v8::CompileError && event != v8::PromiseEvent)
        return;

    v8::Local<v8::Context> eventContext = eventDetails.GetEventContext();
    ASSERT(!eventContext.IsEmpty());

    ScriptDebugListener* listener = m_client->getDebugListenerForContext(eventContext);
    if (listener) {
        v8::HandleScope scope(m_isolate);
        if (event == v8::AfterCompile || event == v8::CompileError) {
            v8::Context::Scope contextScope(debuggerContext());
            v8::Local<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Local<v8::Value> value = callDebuggerMethod("getAfterCompileScript", 1, argv).ToLocalChecked();
            ASSERT(value->IsObject());
            v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
            dispatchDidParseSource(listener, object, event != v8::AfterCompile ? CompileError : CompileSuccess);
        } else if (event == v8::Exception) {
            v8::Local<v8::Object> eventData = eventDetails.GetEventData();
            v8::Local<v8::Value> exception = callInternalGetterFunction(eventData, "exception");
            v8::Local<v8::Value> promise = callInternalGetterFunction(eventData, "promise");
            bool isPromiseRejection = !promise.IsEmpty() && promise->IsObject();
            handleProgramBreak(ScriptState::from(eventContext), eventDetails.GetExecutionState(), exception, v8::Local<v8::Array>(), isPromiseRejection);
        } else if (event == v8::Break) {
            v8::Local<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Local<v8::Value> hitBreakpoints = callDebuggerMethod("getBreakpointNumbers", 1, argv).ToLocalChecked();
            ASSERT(hitBreakpoints->IsArray());
            handleProgramBreak(ScriptState::from(eventContext), eventDetails.GetExecutionState(), v8::Local<v8::Value>(), hitBreakpoints.As<v8::Array>());
        } else if (event == v8::AsyncTaskEvent) {
            if (listener->v8AsyncTaskEventsEnabled())
                handleV8AsyncTaskEvent(listener, ScriptState::from(eventContext), eventDetails.GetExecutionState(), eventDetails.GetEventData());
        } else if (event == v8::PromiseEvent) {
            if (listener->v8PromiseEventsEnabled())
                handleV8PromiseEvent(listener, ScriptState::from(eventContext), eventDetails.GetExecutionState(), eventDetails.GetEventData());
        }
    }
}

void ScriptDebugServer::handleV8AsyncTaskEvent(ScriptDebugListener* listener, ScriptState* pausedScriptState, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData)
{
    String type = toCoreStringWithUndefinedOrNullCheck(callInternalGetterFunction(eventData, "type"));
    String name = toCoreStringWithUndefinedOrNullCheck(callInternalGetterFunction(eventData, "name"));
    int id = callInternalGetterFunction(eventData, "id")->ToInteger(m_isolate)->Value();

    m_pausedScriptState = pausedScriptState;
    m_executionState = executionState;
    listener->didReceiveV8AsyncTaskEvent(pausedScriptState, type, name, id);
    m_pausedScriptState.clear();
    m_executionState.Clear();
}

void ScriptDebugServer::handleV8PromiseEvent(ScriptDebugListener* listener, ScriptState* pausedScriptState, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData)
{
    v8::Local<v8::Value> argv[] = { eventData };
    v8::Local<v8::Value> value = callDebuggerMethod("getPromiseDetails", 1, argv).ToLocalChecked();
    ASSERT(value->IsObject());
    v8::Local<v8::Object> promiseDetails = v8::Local<v8::Object>::Cast(value);
    v8::Local<v8::Object> promise = promiseDetails->Get(v8InternalizedString("promise"))->ToObject(m_isolate);
    int status = promiseDetails->Get(v8InternalizedString("status"))->ToInteger(m_isolate)->Value();
    v8::Local<v8::Value> parentPromise = promiseDetails->Get(v8InternalizedString("parentPromise"));

    m_pausedScriptState = pausedScriptState;
    m_executionState = executionState;
    listener->didReceiveV8PromiseEvent(pausedScriptState, promise, parentPromise, status);
    m_pausedScriptState.clear();
    m_executionState.Clear();
}

void ScriptDebugServer::dispatchDidParseSource(ScriptDebugListener* listener, v8::Local<v8::Object> object, CompileResult compileResult)
{
    v8::Local<v8::Value> id = object->Get(v8InternalizedString("id"));
    ASSERT(!id.IsEmpty() && id->IsInt32());
    String sourceID = String::number(id->Int32Value());

    ScriptDebugListener::Script script;
    script.setURL(toCoreStringWithUndefinedOrNullCheck(object->Get(v8InternalizedString("name"))))
        .setSourceURL(toCoreStringWithUndefinedOrNullCheck(object->Get(v8InternalizedString("sourceURL"))))
        .setSourceMappingURL(toCoreStringWithUndefinedOrNullCheck(object->Get(v8InternalizedString("sourceMappingURL"))))
        .setSource(toCoreStringWithUndefinedOrNullCheck(object->Get(v8InternalizedString("source"))))
        .setStartLine(object->Get(v8InternalizedString("startLine"))->ToInteger(m_isolate)->Value())
        .setStartColumn(object->Get(v8InternalizedString("startColumn"))->ToInteger(m_isolate)->Value())
        .setEndLine(object->Get(v8InternalizedString("endLine"))->ToInteger(m_isolate)->Value())
        .setEndColumn(object->Get(v8InternalizedString("endColumn"))->ToInteger(m_isolate)->Value())
        .setIsContentScript(object->Get(v8InternalizedString("isContentScript"))->ToBoolean(m_isolate)->Value())
        .setIsInternalScript(object->Get(v8InternalizedString("isInternalScript"))->ToBoolean(m_isolate)->Value());

    listener->didParseSource(sourceID, script, compileResult);
}

void ScriptDebugServer::compileDebuggerScript()
{
    if (!m_debuggerScript.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());
    v8::Local<v8::Object> value = m_client->compileDebuggerScript();
    if (value.IsEmpty())
        return;
    m_debuggerScript.Reset(m_isolate, value);
}

v8::Local<v8::Object> ScriptDebugServer::debuggerScriptLocal() const
{
    return v8::Local<v8::Object>::New(m_isolate, m_debuggerScript);
}

v8::Local<v8::Context> ScriptDebugServer::debuggerContext() const
{
    ASSERT(!m_debuggerContext.IsEmpty());
    return v8::Local<v8::Context>::New(m_isolate, m_debuggerContext);
}

v8::Local<v8::String> ScriptDebugServer::v8InternalizedString(const char* str) const
{
    return v8::String::NewFromUtf8(m_isolate, str, v8::NewStringType::kInternalized).ToLocalChecked();
}

v8::Local<v8::Value> ScriptDebugServer::functionScopes(v8::Local<v8::Function> function)
{
    if (!enabled()) {
        ASSERT_NOT_REACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { function };
    return callDebuggerMethod("getFunctionScopes", 1, argv).ToLocalChecked();
}

v8::Local<v8::Value> ScriptDebugServer::generatorObjectDetails(v8::Local<v8::Object>& object)
{
    if (!enabled()) {
        ASSERT_NOT_REACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { object };
    return callDebuggerMethod("getGeneratorObjectDetails", 1, argv).ToLocalChecked();
}

v8::Local<v8::Value> ScriptDebugServer::collectionEntries(v8::Local<v8::Object>& object)
{
    if (!enabled()) {
        ASSERT_NOT_REACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { object };
    return callDebuggerMethod("getCollectionEntries", 1, argv).ToLocalChecked();
}

v8::Local<v8::Value> ScriptDebugServer::getInternalProperties(v8::Local<v8::Object>& object)
{
    if (!enabled()) {
        // FIXME: provide a way to collect internal properties without enabling debugger. See crbug.com/485451
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { object };
    return callDebuggerMethod("getInternalProperties", 1, argv).ToLocalChecked();
}

v8::MaybeLocal<v8::Value> ScriptDebugServer::setFunctionVariableValue(v8::Local<v8::Value> functionValue, int scopeNumber, const String& variableName, v8::Local<v8::Value> newValue)
{
    if (m_debuggerScript.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return m_isolate->ThrowException(v8::String::NewFromUtf8(m_isolate, "Debugging is not enabled.", v8::NewStringType::kNormal).ToLocalChecked());
    }

    v8::Local<v8::Value> argv[] = {
        functionValue,
        v8::Local<v8::Value>(v8::Integer::New(m_isolate, scopeNumber)),
        v8String(m_isolate, variableName),
        newValue
    };
    return callDebuggerMethod("setFunctionVariableValue", 4, argv);
}


bool ScriptDebugServer::isPaused()
{
    return m_pausedScriptState;
}

} // namespace blink
