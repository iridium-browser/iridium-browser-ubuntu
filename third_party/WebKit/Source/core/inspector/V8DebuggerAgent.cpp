// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/V8DebuggerAgent.h"

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "bindings/core/v8/ScriptRegexp.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8RecursionScope.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/Microtask.h"
#include "core/inspector/AsyncCallChain.h"
#include "core/inspector/ContentSearchUtils.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/JSONParser.h"
#include "core/inspector/RemoteObjectId.h"
#include "core/inspector/ScriptAsyncCallStack.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/V8AsyncCallTracker.h"
#include "core/inspector/v8/JavaScriptCallFrame.h"
#include "core/inspector/v8/V8Debugger.h"
#include "core/inspector/v8/V8JavaScriptCallFrame.h"
#include "platform/JSONValues.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

using blink::TypeBuilder::Array;
using blink::TypeBuilder::Console::AsyncStackTrace;
using blink::TypeBuilder::Debugger::AsyncOperation;
using blink::TypeBuilder::Debugger::BreakpointId;
using blink::TypeBuilder::Debugger::CallFrame;
using blink::TypeBuilder::Debugger::CollectionEntry;
using blink::TypeBuilder::Debugger::ExceptionDetails;
using blink::TypeBuilder::Debugger::FunctionDetails;
using blink::TypeBuilder::Debugger::GeneratorObjectDetails;
using blink::TypeBuilder::Debugger::PromiseDetails;
using blink::TypeBuilder::Debugger::ScriptId;
using blink::TypeBuilder::Debugger::StackTrace;
using blink::TypeBuilder::Runtime::RemoteObject;

namespace blink {

namespace DebuggerAgentState {
static const char debuggerEnabled[] = "debuggerEnabled";
static const char javaScriptBreakpoints[] = "javaScriptBreakopints";
static const char pauseOnExceptionsState[] = "pauseOnExceptionsState";
static const char asyncCallStackDepth[] = "asyncCallStackDepth";
static const char promiseTrackerEnabled[] = "promiseTrackerEnabled";
static const char promiseTrackerCaptureStacks[] = "promiseTrackerCaptureStacks";

// Breakpoint properties.
static const char url[] = "url";
static const char isRegex[] = "isRegex";
static const char lineNumber[] = "lineNumber";
static const char columnNumber[] = "columnNumber";
static const char condition[] = "condition";
static const char skipStackPattern[] = "skipStackPattern";
static const char skipContentScripts[] = "skipContentScripts";
static const char skipAllPauses[] = "skipAllPauses";

};

static const int maxSkipStepFrameCount = 128;

const char V8DebuggerAgent::backtraceObjectGroup[] = "backtrace";

const int V8DebuggerAgent::unknownAsyncOperationId = 0;

static String breakpointIdSuffix(V8DebuggerAgent::BreakpointSource source)
{
    switch (source) {
    case V8DebuggerAgent::UserBreakpointSource:
        break;
    case V8DebuggerAgent::DebugCommandBreakpointSource:
        return ":debug";
    case V8DebuggerAgent::MonitorCommandBreakpointSource:
        return ":monitor";
    }
    return String();
}

static String generateBreakpointId(const String& scriptId, int lineNumber, int columnNumber, V8DebuggerAgent::BreakpointSource source)
{
    return scriptId + ':' + String::number(lineNumber) + ':' + String::number(columnNumber) + breakpointIdSuffix(source);
}

static ScriptCallFrame toScriptCallFrame(JavaScriptCallFrame* callFrame)
{
    String scriptId = String::number(callFrame->sourceID());
    // FIXME(WK62725): Debugger line/column are 0-based, while console ones are 1-based.
    int line = callFrame->line() + 1;
    int column = callFrame->column() + 1;
    return ScriptCallFrame(callFrame->functionName(), scriptId, callFrame->scriptName(), line, column);
}

static PassRefPtrWillBeRawPtr<ScriptCallStack> toScriptCallStack(JavaScriptCallFrame* callFrame)
{
    Vector<ScriptCallFrame> frames;
    for (; callFrame; callFrame = callFrame->caller())
        frames.append(toScriptCallFrame(callFrame));
    return ScriptCallStack::create(frames);
}

static PassRefPtr<JavaScriptCallFrame> toJavaScriptCallFrame(v8::Local<v8::Object> value)
{
    if (value.IsEmpty())
        return nullptr;
    return V8JavaScriptCallFrame::unwrap(value);
}

static PassRefPtrWillBeRawPtr<ScriptCallStack> toScriptCallStack(v8::Local<v8::Object> callFrames)
{
    RefPtr<JavaScriptCallFrame> jsCallFrame = toJavaScriptCallFrame(callFrames);
    return jsCallFrame ? toScriptCallStack(jsCallFrame.get()) : nullptr;
}

V8DebuggerAgent::V8DebuggerAgent(InjectedScriptManager* injectedScriptManager, V8Debugger* debugger, V8DebuggerAgent::Client* client, int contextGroupId)
    : m_injectedScriptManager(injectedScriptManager)
    , m_debugger(debugger)
    , m_client(client)
    , m_contextGroupId(contextGroupId)
    , m_state(nullptr)
    , m_frontend(nullptr)
    , m_isolate(debugger->isolate())
    , m_pausedScriptState(nullptr)
    , m_breakReason(InspectorFrontend::Debugger::Reason::Other)
    , m_scheduledDebuggerStep(NoStep)
    , m_skipNextDebuggerStepOut(false)
    , m_javaScriptPauseScheduled(false)
    , m_steppingFromFramework(false)
    , m_pausingOnNativeEvent(false)
    , m_pausingOnAsyncOperation(false)
    , m_skippedStepFrameCount(0)
    , m_recursionLevelForStepOut(0)
    , m_recursionLevelForStepFrame(0)
    , m_skipAllPauses(false)
    , m_skipContentScripts(false)
    , m_cachedSkipStackGeneration(0)
    , m_lastAsyncOperationId(0)
    , m_maxAsyncCallStackDepth(0)
    , m_currentAsyncCallChain(nullptr)
    , m_nestedAsyncCallCount(0)
    , m_currentAsyncOperationId(unknownAsyncOperationId)
    , m_pendingTraceAsyncOperationCompleted(false)
    , m_startingStepIntoAsync(false)
    , m_compiledScripts(debugger->isolate())
{
    ASSERT(contextGroupId);
    m_v8AsyncCallTracker = V8AsyncCallTracker::create(this);
    m_promiseTracker = PromiseTracker::create(this, m_isolate);
    clearBreakDetails();
}

V8DebuggerAgent::~V8DebuggerAgent()
{
}

DEFINE_TRACE(V8DebuggerAgent)
{
#if ENABLE(OILPAN)
    visitor->trace(m_injectedScriptManager);
    visitor->trace(m_asyncCallTrackingListeners);
    visitor->trace(m_v8AsyncCallTracker);
    visitor->trace(m_promiseTracker);
    visitor->trace(m_asyncOperations);
    visitor->trace(m_currentAsyncCallChain);
#endif
}

bool V8DebuggerAgent::checkEnabled(ErrorString* errorString)
{
    if (enabled())
        return true;
    *errorString = "Debugger agent is not enabled";
    return false;
}

void V8DebuggerAgent::enable()
{
    // debugger().addListener may result in reporting all parsed scripts to
    // the agent so it should already be in enabled state by then.
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, true);
    debugger().addListener(m_contextGroupId, this);
    // FIXME(WK44513): breakpoints activated flag should be synchronized between all front-ends
    debugger().setBreakpointsActivated(true);
    m_client->debuggerAgentEnabled();
}

bool V8DebuggerAgent::enabled()
{
    return m_state->getBoolean(DebuggerAgentState::debuggerEnabled);
}

void V8DebuggerAgent::enable(ErrorString*)
{
    if (enabled())
        return;

    enable();

    ASSERT(m_frontend);
}

void V8DebuggerAgent::disable(ErrorString*)
{
    if (!enabled())
        return;

    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, JSONObject::create());
    m_state->setLong(DebuggerAgentState::pauseOnExceptionsState, V8Debugger::DontPauseOnExceptions);
    m_state->setString(DebuggerAgentState::skipStackPattern, "");
    m_state->setBoolean(DebuggerAgentState::skipContentScripts, false);
    m_state->setLong(DebuggerAgentState::asyncCallStackDepth, 0);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, false);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerCaptureStacks, false);

    debugger().removeListener(m_contextGroupId);
    m_client->debuggerAgentDisabled();
    m_pausedScriptState = nullptr;
    m_currentCallStack.Reset();
    m_scripts.clear();
    m_breakpointIdToDebuggerBreakpointIds.clear();
    internalSetAsyncCallStackDepth(0);
    m_promiseTracker->setEnabled(false, false);
    m_continueToLocationBreakpointId = String();
    clearBreakDetails();
    m_scheduledDebuggerStep = NoStep;
    m_skipNextDebuggerStepOut = false;
    m_javaScriptPauseScheduled = false;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    m_asyncOperationNotifications.clear();
    m_compiledScripts.Clear();
    clearStepIntoAsync();
    m_skipAllPauses = false;
    m_state->setBoolean(DebuggerAgentState::debuggerEnabled, false);
}

static PassOwnPtr<ScriptRegexp> compileSkipCallFramePattern(String patternText)
{
    if (patternText.isEmpty())
        return nullptr;
    OwnPtr<ScriptRegexp> result = adoptPtr(new ScriptRegexp(patternText, TextCaseSensitive));
    if (!result->isValid())
        result.clear();
    return result.release();
}

void V8DebuggerAgent::increaseCachedSkipStackGeneration()
{
    ++m_cachedSkipStackGeneration;
    if (!m_cachedSkipStackGeneration)
        m_cachedSkipStackGeneration = 1;
}

void V8DebuggerAgent::internalSetAsyncCallStackDepth(int depth)
{
    if (depth <= 0) {
        m_maxAsyncCallStackDepth = 0;
        resetAsyncCallTracker();
    } else {
        m_maxAsyncCallStackDepth = depth;
    }
    for (auto& listener: m_asyncCallTrackingListeners)
        listener->asyncCallTrackingStateChanged(m_maxAsyncCallStackDepth);
}

void V8DebuggerAgent::clearFrontend()
{
    ErrorString error;
    disable(&error);
    ASSERT(m_frontend);
    m_frontend = nullptr;
}

void V8DebuggerAgent::restore()
{
    if (enabled()) {
        m_frontend->globalObjectCleared();
        enable();
        long pauseState = m_state->getLong(DebuggerAgentState::pauseOnExceptionsState, V8Debugger::DontPauseOnExceptions);
        String error;
        setPauseOnExceptionsImpl(&error, pauseState);
        m_cachedSkipStackRegExp = compileSkipCallFramePattern(m_state->getString(DebuggerAgentState::skipStackPattern));
        increaseCachedSkipStackGeneration();
        m_skipContentScripts = m_state->getBoolean(DebuggerAgentState::skipContentScripts);
        m_skipAllPauses = m_state->getBoolean(DebuggerAgentState::skipAllPauses);
        internalSetAsyncCallStackDepth(m_state->getLong(DebuggerAgentState::asyncCallStackDepth));
        m_promiseTracker->setEnabled(m_state->getBoolean(DebuggerAgentState::promiseTrackerEnabled), m_state->getBoolean(DebuggerAgentState::promiseTrackerCaptureStacks));
    }
}

void V8DebuggerAgent::setBreakpointsActive(ErrorString* errorString, bool active)
{
    if (!checkEnabled(errorString))
        return;
    debugger().setBreakpointsActivated(active);
}

void V8DebuggerAgent::setSkipAllPauses(ErrorString*, bool skipped)
{
    m_skipAllPauses = skipped;
    m_state->setBoolean(DebuggerAgentState::skipAllPauses, m_skipAllPauses);
}

bool V8DebuggerAgent::isPaused()
{
    return debugger().isPaused();
}

static PassRefPtr<JSONObject> buildObjectForBreakpointCookie(const String& url, int lineNumber, int columnNumber, const String& condition, bool isRegex)
{
    RefPtr<JSONObject> breakpointObject = JSONObject::create();
    breakpointObject->setString(DebuggerAgentState::url, url);
    breakpointObject->setNumber(DebuggerAgentState::lineNumber, lineNumber);
    breakpointObject->setNumber(DebuggerAgentState::columnNumber, columnNumber);
    breakpointObject->setString(DebuggerAgentState::condition, condition);
    breakpointObject->setBoolean(DebuggerAgentState::isRegex, isRegex);
    return breakpointObject.release();
}

static bool matches(const String& url, const String& pattern, bool isRegex)
{
    if (isRegex) {
        ScriptRegexp regex(pattern, TextCaseSensitive);
        return regex.match(url) != -1;
    }
    return url == pattern;
}

void V8DebuggerAgent::setBreakpointByUrl(ErrorString* errorString, int lineNumber, const String* const optionalURL, const String* const optionalURLRegex, const int* const optionalColumnNumber, const String* const optionalCondition, BreakpointId* outBreakpointId, RefPtr<Array<TypeBuilder::Debugger::Location>>& locations)
{
    locations = Array<TypeBuilder::Debugger::Location>::create();
    if (!optionalURL == !optionalURLRegex) {
        *errorString = "Either url or urlRegex must be specified.";
        return;
    }

    String url = optionalURL ? *optionalURL : *optionalURLRegex;
    int columnNumber = 0;
    if (optionalColumnNumber) {
        columnNumber = *optionalColumnNumber;
        if (columnNumber < 0) {
            *errorString = "Incorrect column number";
            return;
        }
    }
    String condition = optionalCondition ? *optionalCondition : "";
    bool isRegex = optionalURLRegex;

    String breakpointId = (isRegex ? "/" + url + "/" : url) + ':' + String::number(lineNumber) + ':' + String::number(columnNumber);
    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    if (breakpointsCookie->find(breakpointId) != breakpointsCookie->end()) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }

    breakpointsCookie->setObject(breakpointId, buildObjectForBreakpointCookie(url, lineNumber, columnNumber, condition, isRegex));
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    for (auto& script : m_scripts) {
        if (!matches(script.value.sourceURL(), url, isRegex))
            continue;
        RefPtr<TypeBuilder::Debugger::Location> location = resolveBreakpoint(breakpointId, script.key, breakpoint, UserBreakpointSource);
        if (location)
            locations->addItem(location);
    }

    *outBreakpointId = breakpointId;
}

static bool parseLocation(ErrorString* errorString, PassRefPtr<JSONObject> location, String* scriptId, int* lineNumber, int* columnNumber)
{
    if (!location->getString("scriptId", scriptId) || !location->getNumber("lineNumber", lineNumber)) {
        // FIXME: replace with input validation.
        *errorString = "scriptId and lineNumber are required.";
        return false;
    }
    *columnNumber = 0;
    location->getNumber("columnNumber", columnNumber);
    return true;
}

void V8DebuggerAgent::setBreakpoint(ErrorString* errorString, const RefPtr<JSONObject>& location, const String* const optionalCondition, BreakpointId* outBreakpointId, RefPtr<TypeBuilder::Debugger::Location>& actualLocation)
{
    String scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    String condition = optionalCondition ? *optionalCondition : emptyString();

    String breakpointId = generateBreakpointId(scriptId, lineNumber, columnNumber, UserBreakpointSource);
    if (m_breakpointIdToDebuggerBreakpointIds.find(breakpointId) != m_breakpointIdToDebuggerBreakpointIds.end()) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    actualLocation = resolveBreakpoint(breakpointId, scriptId, breakpoint, UserBreakpointSource);
    if (actualLocation)
        *outBreakpointId = breakpointId;
    else
        *errorString = "Could not resolve breakpoint";
}

void V8DebuggerAgent::removeBreakpoint(ErrorString* errorString, const String& breakpointId)
{
    if (!checkEnabled(errorString))
        return;
    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    breakpointsCookie->remove(breakpointId);
    m_state->setObject(DebuggerAgentState::javaScriptBreakpoints, breakpointsCookie);
    removeBreakpoint(breakpointId);
}

void V8DebuggerAgent::removeBreakpoint(const String& breakpointId)
{
    ASSERT(enabled());
    BreakpointIdToDebuggerBreakpointIdsMap::iterator debuggerBreakpointIdsIterator = m_breakpointIdToDebuggerBreakpointIds.find(breakpointId);
    if (debuggerBreakpointIdsIterator == m_breakpointIdToDebuggerBreakpointIds.end())
        return;
    for (size_t i = 0; i < debuggerBreakpointIdsIterator->value.size(); ++i) {
        const String& debuggerBreakpointId = debuggerBreakpointIdsIterator->value[i];
        debugger().removeBreakpoint(debuggerBreakpointId);
        m_serverBreakpoints.remove(debuggerBreakpointId);
    }
    m_breakpointIdToDebuggerBreakpointIds.remove(debuggerBreakpointIdsIterator);
}

void V8DebuggerAgent::continueToLocation(ErrorString* errorString, const RefPtr<JSONObject>& location, const bool* interstateLocationOpt)
{
    if (!checkEnabled(errorString))
        return;
    if (!m_continueToLocationBreakpointId.isEmpty()) {
        debugger().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }

    String scriptId;
    int lineNumber;
    int columnNumber;

    if (!parseLocation(errorString, location, &scriptId, &lineNumber, &columnNumber))
        return;

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, "");
    m_continueToLocationBreakpointId = debugger().setBreakpoint(scriptId, breakpoint, &lineNumber, &columnNumber, asBool(interstateLocationOpt));
    resume(errorString);
}

void V8DebuggerAgent::getStepInPositions(ErrorString* errorString, const String& callFrameId, RefPtr<Array<TypeBuilder::Debugger::Location>>& positions)
{
    if (!isPaused() || m_currentCallStack.IsEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(callFrameId);
    if (!remoteId) {
        *errorString = "Invalid call frame id";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> callStack = m_currentCallStack.Get(m_isolate);
    injectedScript.getStepInPositions(errorString, callStack, callFrameId, positions);
}

void V8DebuggerAgent::getBacktrace(ErrorString* errorString, RefPtr<Array<CallFrame>>& callFrames, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!assertPaused(errorString))
        return;
    m_currentCallStack.Reset(m_isolate, debugger().currentCallFrames());
    callFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();
}

bool V8DebuggerAgent::isCallStackEmptyOrBlackboxed()
{
    ASSERT(enabled());
    for (int index = 0; ; ++index) {
        RefPtr<JavaScriptCallFrame> frame = debugger().callFrameNoScopes(index);
        if (!frame)
            break;
        if (!isCallFrameWithUnknownScriptOrBlackboxed(frame.release()))
            return false;
    }
    return true;
}

bool V8DebuggerAgent::isTopCallFrameBlackboxed()
{
    ASSERT(enabled());
    return isCallFrameWithUnknownScriptOrBlackboxed(debugger().callFrameNoScopes(0));
}

bool V8DebuggerAgent::isCallFrameWithUnknownScriptOrBlackboxed(PassRefPtr<JavaScriptCallFrame> pFrame)
{
    RefPtr<JavaScriptCallFrame> frame = pFrame;
    if (!frame)
        return true;
    ScriptsMap::iterator it = m_scripts.find(String::number(frame->sourceID()));
    if (it == m_scripts.end()) {
        // Unknown scripts are blackboxed.
        return true;
    }
    if (m_skipContentScripts && it->value.isContentScript())
        return true;
    bool isBlackboxed = false;
    String scriptURL = it->value.sourceURL();
    if (m_cachedSkipStackRegExp && !scriptURL.isEmpty()) {
        if (!it->value.getBlackboxedState(m_cachedSkipStackGeneration, &isBlackboxed)) {
            isBlackboxed = m_cachedSkipStackRegExp->match(scriptURL) != -1;
            it->value.setBlackboxedState(m_cachedSkipStackGeneration, isBlackboxed);
        }
    }
    return isBlackboxed;
}

V8DebuggerListener::SkipPauseRequest V8DebuggerAgent::shouldSkipExceptionPause()
{
    if (m_steppingFromFramework)
        return V8DebuggerListener::NoSkip;
    if (isTopCallFrameBlackboxed())
        return V8DebuggerListener::Continue;
    return V8DebuggerListener::NoSkip;
}

V8DebuggerListener::SkipPauseRequest V8DebuggerAgent::shouldSkipStepPause()
{
    if (m_steppingFromFramework)
        return V8DebuggerListener::NoSkip;

    if (m_skipNextDebuggerStepOut) {
        m_skipNextDebuggerStepOut = false;
        if (m_scheduledDebuggerStep == StepOut)
            return V8DebuggerListener::StepOut;
    }

    if (!isTopCallFrameBlackboxed())
        return V8DebuggerListener::NoSkip;

    if (m_skippedStepFrameCount >= maxSkipStepFrameCount)
        return V8DebuggerListener::StepOut;

    if (!m_skippedStepFrameCount)
        m_recursionLevelForStepFrame = 1;

    ++m_skippedStepFrameCount;
    return V8DebuggerListener::StepFrame;
}

PassRefPtr<TypeBuilder::Debugger::Location> V8DebuggerAgent::resolveBreakpoint(const String& breakpointId, const String& scriptId, const ScriptBreakpoint& breakpoint, BreakpointSource source)
{
    ASSERT(enabled());
    // FIXME: remove these checks once crbug.com/520702 is resolved.
    RELEASE_ASSERT(!breakpointId.isEmpty());
    RELEASE_ASSERT(!scriptId.isEmpty());
    ScriptsMap::iterator scriptIterator = m_scripts.find(scriptId);
    if (scriptIterator == m_scripts.end())
        return nullptr;
    Script& script = scriptIterator->value;
    if (breakpoint.lineNumber < script.startLine() || script.endLine() < breakpoint.lineNumber)
        return nullptr;

    int actualLineNumber;
    int actualColumnNumber;
    String debuggerBreakpointId = debugger().setBreakpoint(scriptId, breakpoint, &actualLineNumber, &actualColumnNumber, false);
    if (debuggerBreakpointId.isEmpty())
        return nullptr;

    m_serverBreakpoints.set(debuggerBreakpointId, std::make_pair(breakpointId, source));

    RELEASE_ASSERT(!breakpointId.isEmpty());
    BreakpointIdToDebuggerBreakpointIdsMap::iterator debuggerBreakpointIdsIterator = m_breakpointIdToDebuggerBreakpointIds.find(breakpointId);
    if (debuggerBreakpointIdsIterator == m_breakpointIdToDebuggerBreakpointIds.end())
        m_breakpointIdToDebuggerBreakpointIds.set(breakpointId, Vector<String>()).storedValue->value.append(debuggerBreakpointId);
    else
        debuggerBreakpointIdsIterator->value.append(debuggerBreakpointId);

    RefPtr<TypeBuilder::Debugger::Location> location = TypeBuilder::Debugger::Location::create()
        .setScriptId(scriptId)
        .setLineNumber(actualLineNumber);
    location->setColumnNumber(actualColumnNumber);
    return location;
}

void V8DebuggerAgent::searchInContent(ErrorString* error, const String& scriptId, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<Array<TypeBuilder::Debugger::SearchMatch>>& results)
{
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it != m_scripts.end())
        results = ContentSearchUtils::searchInTextByLines(it->value.source(), query, asBool(optionalCaseSensitive), asBool(optionalIsRegex));
    else
        *error = "No script for id: " + scriptId;
}

void V8DebuggerAgent::setScriptSource(ErrorString* error, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>& errorData, const String& scriptId, const String& newContent, const bool* const preview, RefPtr<Array<CallFrame>>& newCallFrames, TypeBuilder::OptOutput<bool>* stackChanged, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!checkEnabled(error))
        return;
    if (!debugger().setScriptSource(scriptId, newContent, asBool(preview), error, errorData, &m_currentCallStack, stackChanged))
        return;

    newCallFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();

    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end())
        return;
    it->value.setSource(newContent);
}

void V8DebuggerAgent::restartFrame(ErrorString* errorString, const String& callFrameId, RefPtr<Array<CallFrame>>& newCallFrames, RefPtr<StackTrace>& asyncStackTrace)
{
    if (!isPaused() || m_currentCallStack.IsEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(callFrameId);
    if (!remoteId) {
        *errorString = "Invalid call frame id";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> callStack = m_currentCallStack.Get(m_isolate);
    injectedScript.restartFrame(errorString, callStack, callFrameId);
    m_currentCallStack.Reset(m_isolate, debugger().currentCallFrames());
    newCallFrames = currentCallFrames();
    asyncStackTrace = currentAsyncStackTrace();
}

void V8DebuggerAgent::getScriptSource(ErrorString* error, const String& scriptId, String* scriptSource)
{
    if (!checkEnabled(error))
        return;
    ScriptsMap::iterator it = m_scripts.find(scriptId);
    if (it == m_scripts.end()) {
        *error = "No script for id: " + scriptId;
        return;
    }
    *scriptSource = it->value.source();
}

void V8DebuggerAgent::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<FunctionDetails>& details)
{
    if (!checkEnabled(errorString))
        return;
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(functionId);
    if (!remoteId) {
        *errorString = "Invalid object id";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (injectedScript.isEmpty()) {
        *errorString = "Function object id is obsolete";
        return;
    }
    injectedScript.getFunctionDetails(errorString, functionId, &details);
}

void V8DebuggerAgent::getGeneratorObjectDetails(ErrorString* errorString, const String& objectId, RefPtr<GeneratorObjectDetails>& details)
{
    if (!checkEnabled(errorString))
        return;
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(objectId);
    if (!remoteId) {
        *errorString = "Invalid object id";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    injectedScript.getGeneratorObjectDetails(errorString, objectId, &details);
}

void V8DebuggerAgent::getCollectionEntries(ErrorString* errorString, const String& objectId, RefPtr<TypeBuilder::Array<CollectionEntry>>& entries)
{
    if (!checkEnabled(errorString))
        return;
    OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(objectId);
    if (!remoteId) {
        *errorString = "Invalid object id";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    injectedScript.getCollectionEntries(errorString, objectId, &entries);
}

void V8DebuggerAgent::schedulePauseOnNextStatement(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data)
{
    ASSERT(enabled());
    if (m_scheduledDebuggerStep == StepInto || m_javaScriptPauseScheduled || isPaused())
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    m_pausingOnNativeEvent = true;
    m_skipNextDebuggerStepOut = false;
    debugger().setPauseOnNextStatement(true);
}

void V8DebuggerAgent::schedulePauseOnNextStatementIfSteppingInto()
{
    ASSERT(enabled());
    if (m_scheduledDebuggerStep != StepInto || m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    debugger().setPauseOnNextStatement(true);
}

void V8DebuggerAgent::cancelPauseOnNextStatement()
{
    if (m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    m_pausingOnNativeEvent = false;
    debugger().setPauseOnNextStatement(false);
}

bool V8DebuggerAgent::v8AsyncTaskEventsEnabled() const
{
    return trackingAsyncCalls();
}

void V8DebuggerAgent::didReceiveV8AsyncTaskEvent(v8::Local<v8::Context> context, const String& eventType, const String& eventName, int id)
{
    ASSERT(trackingAsyncCalls());
    ScriptState* state = ScriptState::from(context);
    m_v8AsyncCallTracker->didReceiveV8AsyncTaskEvent(state, eventType, eventName, id);
}

bool V8DebuggerAgent::v8PromiseEventsEnabled() const
{
    return m_promiseTracker->isEnabled();
}

void V8DebuggerAgent::didReceiveV8PromiseEvent(v8::Local<v8::Context> context, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status)
{
    ASSERT(m_promiseTracker->isEnabled());
    ScriptState* scriptState = ScriptState::from(context);
    m_promiseTracker->didReceiveV8PromiseEvent(scriptState, promise, parentPromise, status);
}

void V8DebuggerAgent::pause(ErrorString* errorString)
{
    if (!checkEnabled(errorString))
        return;
    if (m_javaScriptPauseScheduled || isPaused())
        return;
    clearBreakDetails();
    clearStepIntoAsync();
    m_javaScriptPauseScheduled = true;
    m_scheduledDebuggerStep = NoStep;
    m_skippedStepFrameCount = 0;
    m_steppingFromFramework = false;
    debugger().setPauseOnNextStatement(true);
}

void V8DebuggerAgent::resume(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = NoStep;
    m_steppingFromFramework = false;
    m_injectedScriptManager->releaseObjectGroup(V8DebuggerAgent::backtraceObjectGroup);
    debugger().continueProgram();
}

void V8DebuggerAgent::stepOver(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    // StepOver at function return point should fallback to StepInto.
    RefPtr<JavaScriptCallFrame> frame = debugger().callFrameNoScopes(0);
    if (frame && frame->isAtReturn()) {
        stepInto(errorString);
        return;
    }
    m_scheduledDebuggerStep = StepOver;
    m_steppingFromFramework = isTopCallFrameBlackboxed();
    m_injectedScriptManager->releaseObjectGroup(V8DebuggerAgent::backtraceObjectGroup);
    debugger().stepOverStatement();
}

void V8DebuggerAgent::stepInto(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = StepInto;
    m_steppingFromFramework = isTopCallFrameBlackboxed();
    m_injectedScriptManager->releaseObjectGroup(V8DebuggerAgent::backtraceObjectGroup);
    debugger().stepIntoStatement();
}

void V8DebuggerAgent::stepOut(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_scheduledDebuggerStep = StepOut;
    m_skipNextDebuggerStepOut = false;
    m_recursionLevelForStepOut = 1;
    m_steppingFromFramework = isTopCallFrameBlackboxed();
    m_injectedScriptManager->releaseObjectGroup(V8DebuggerAgent::backtraceObjectGroup);
    debugger().stepOutOfFunction();
}

void V8DebuggerAgent::stepIntoAsync(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    if (!trackingAsyncCalls()) {
        *errorString = "Can only perform operation if async call stacks are enabled.";
        return;
    }
    clearStepIntoAsync();
    m_startingStepIntoAsync = true;
    stepInto(errorString);
}

void V8DebuggerAgent::setPauseOnExceptions(ErrorString* errorString, const String& stringPauseState)
{
    if (!checkEnabled(errorString))
        return;
    V8Debugger::PauseOnExceptionsState pauseState;
    if (stringPauseState == "none") {
        pauseState = V8Debugger::DontPauseOnExceptions;
    } else if (stringPauseState == "all") {
        pauseState = V8Debugger::PauseOnAllExceptions;
    } else if (stringPauseState == "uncaught") {
        pauseState = V8Debugger::PauseOnUncaughtExceptions;
    } else {
        *errorString = "Unknown pause on exceptions mode: " + stringPauseState;
        return;
    }
    setPauseOnExceptionsImpl(errorString, pauseState);
}

void V8DebuggerAgent::setPauseOnExceptionsImpl(ErrorString* errorString, int pauseState)
{
    debugger().setPauseOnExceptionsState(static_cast<V8Debugger::PauseOnExceptionsState>(pauseState));
    if (debugger().pauseOnExceptionsState() != pauseState)
        *errorString = "Internal error. Could not change pause on exceptions state";
    else
        m_state->setLong(DebuggerAgentState::pauseOnExceptionsState, pauseState);
}

bool V8DebuggerAgent::callStackForId(ErrorString* errorString, const RemoteCallFrameId& callFrameId, v8::Local<v8::Object>* callStack, bool* isAsync)
{
    unsigned asyncOrdinal = callFrameId.asyncStackOrdinal(); // 0 is current call stack
    if (!asyncOrdinal) {
        *callStack = m_currentCallStack.Get(m_isolate);
        *isAsync = false;
        return true;
    }
    if (!m_currentAsyncCallChain || asyncOrdinal < 1 || asyncOrdinal >= m_currentAsyncCallChain->callStacks().size()) {
        *errorString = "Async call stack not found";
        return false;
    }
    RefPtrWillBeRawPtr<AsyncCallStack> asyncStack = m_currentAsyncCallChain->callStacks()[asyncOrdinal - 1];
    *callStack = asyncStack->callFrames(m_isolate);
    *isAsync = true;
    return true;
}

void V8DebuggerAgent::evaluateOnCallFrame(ErrorString* errorString, const String& callFrameId, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, const bool* generatePreview, RefPtr<RemoteObject>& result, TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& exceptionDetails)
{
    if (!isPaused() || m_currentCallStack.IsEmpty()) {
        *errorString = "Attempt to access callframe when debugger is not on pause";
        return;
    }
    OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(callFrameId);
    if (!remoteId) {
        *errorString = "Invalid call frame id";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    v8::HandleScope scope(m_isolate);
    bool isAsync = false;
    v8::Local<v8::Object> callStack;
    if (!callStackForId(errorString, *remoteId, &callStack, &isAsync))
        return;
    ASSERT(!callStack.IsEmpty());

    V8Debugger::PauseOnExceptionsState previousPauseOnExceptionsState = debugger().pauseOnExceptionsState();
    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        if (previousPauseOnExceptionsState != V8Debugger::DontPauseOnExceptions)
            debugger().setPauseOnExceptionsState(V8Debugger::DontPauseOnExceptions);
        m_client->muteConsole();
    }

    injectedScript.evaluateOnCallFrame(errorString, callStack, isAsync, callFrameId, expression, objectGroup ? *objectGroup : "", asBool(includeCommandLineAPI), asBool(returnByValue), asBool(generatePreview), &result, wasThrown, &exceptionDetails);
    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        m_client->unmuteConsole();
        if (debugger().pauseOnExceptionsState() != previousPauseOnExceptionsState)
            debugger().setPauseOnExceptionsState(previousPauseOnExceptionsState);
    }
}

InjectedScript V8DebuggerAgent::injectedScriptForEval(ErrorString* errorString, const int* executionContextId)
{
    InjectedScript injectedScript = executionContextId ? m_injectedScriptManager->injectedScriptForId(*executionContextId) : m_client->defaultInjectedScript();
    if (injectedScript.isEmpty())
        *errorString = "Execution context with given id not found.";
    return injectedScript;
}

void V8DebuggerAgent::compileScript(ErrorString* errorString, const String& expression, const String& sourceURL, bool persistScript, const int* executionContextId, TypeBuilder::OptOutput<ScriptId>* scriptId, RefPtr<ExceptionDetails>& exceptionDetails)
{
    if (!checkEnabled(errorString))
        return;
    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.isEmpty() || !injectedScript.scriptState()->contextIsValid()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    ScriptState::Scope scope(injectedScript.scriptState());
    v8::Local<v8::String> source = v8String(m_isolate, expression);
    v8::TryCatch tryCatch;
    v8::Local<v8::Script> script;
    if (!v8Call(V8ScriptRunner::compileScript(source, sourceURL, String(), TextPosition(), m_isolate), script, tryCatch)) {
        v8::Local<v8::Message> message = tryCatch.Message();
        if (!message.IsEmpty())
            exceptionDetails = createExceptionDetails(m_isolate, message);
        else
            *errorString = "Script compilation failed";
        return;
    }

    if (!persistScript)
        return;

    String scriptValueId = String::number(script->GetUnboundScript()->GetId());
    m_compiledScripts.Set(scriptValueId, script);
    *scriptId = scriptValueId;
}

void V8DebuggerAgent::runScript(ErrorString* errorString, const ScriptId& scriptId, const int* executionContextId, const String* const objectGroup, const bool* const doNotPauseOnExceptionsAndMuteConsole, RefPtr<RemoteObject>& result, RefPtr<ExceptionDetails>& exceptionDetails)
{
    if (!checkEnabled(errorString))
        return;
    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    V8Debugger::PauseOnExceptionsState previousPauseOnExceptionsState = debugger().pauseOnExceptionsState();
    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        if (previousPauseOnExceptionsState != V8Debugger::DontPauseOnExceptions)
            debugger().setPauseOnExceptionsState(V8Debugger::DontPauseOnExceptions);
        m_client->muteConsole();
    }

    if (!m_compiledScripts.Contains(scriptId)) {
        *errorString = "Script execution failed";
        return;
    }

    ScriptState* scriptState = injectedScript.scriptState();
    ScriptState::Scope scope(scriptState);
    v8::Local<v8::Script> script = v8::Local<v8::Script>::New(m_isolate, m_compiledScripts.Remove(scriptId));

    if (script.IsEmpty() || !scriptState->contextIsValid()) {
        *errorString = "Script execution failed";
        return;
    }
    v8::TryCatch tryCatch;
    v8::Local<v8::Value> value;
    ScriptValue scriptValue;
    if (v8Call(V8ScriptRunner::runCompiledScript(m_isolate, script, scriptState->executionContext()), value, tryCatch)) {
        scriptValue = ScriptValue(scriptState, value);
    } else {
        scriptValue = ScriptValue(scriptState, tryCatch.Exception());
        v8::Local<v8::Message> message = tryCatch.Message();
        if (!message.IsEmpty())
            exceptionDetails = createExceptionDetails(m_isolate, message);
    }

    if (scriptValue.isEmpty()) {
        *errorString = "Script execution failed";
        return;
    }

    result = injectedScript.wrapObject(scriptValue, objectGroup ? *objectGroup : "");

    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        m_client->unmuteConsole();
        if (debugger().pauseOnExceptionsState() != previousPauseOnExceptionsState)
            debugger().setPauseOnExceptionsState(previousPauseOnExceptionsState);
    }
}

void V8DebuggerAgent::setVariableValue(ErrorString* errorString, int scopeNumber, const String& variableName, const RefPtr<JSONObject>& newValue, const String* callFrameId, const String* functionObjectId)
{
    if (!checkEnabled(errorString))
        return;
    InjectedScript injectedScript;
    if (callFrameId) {
        if (!isPaused() || m_currentCallStack.IsEmpty()) {
            *errorString = "Attempt to access callframe when debugger is not on pause";
            return;
        }
        OwnPtr<RemoteCallFrameId> remoteId = RemoteCallFrameId::parse(*callFrameId);
        if (!remoteId) {
            *errorString = "Invalid call frame id";
            return;
        }
        injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
        if (injectedScript.isEmpty()) {
            *errorString = "Inspected frame has gone";
            return;
        }
    } else if (functionObjectId) {
        OwnPtr<RemoteObjectId> remoteId = RemoteObjectId::parse(*functionObjectId);
        if (!remoteId) {
            *errorString = "Invalid object id";
            return;
        }
        injectedScript = m_injectedScriptManager->findInjectedScript(remoteId.get());
        if (injectedScript.isEmpty()) {
            *errorString = "Function object id cannot be resolved";
            return;
        }
    } else {
        *errorString = "Either call frame or function object must be specified";
        return;
    }
    String newValueString = newValue->toJSONString();

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> currentCallStack = m_currentCallStack.Get(m_isolate);
    injectedScript.setVariableValue(errorString, currentCallStack, callFrameId, functionObjectId, scopeNumber, variableName, newValueString);
}

void V8DebuggerAgent::skipStackFrames(ErrorString* errorString, const String* pattern, const bool* skipContentScripts)
{
    if (!checkEnabled(errorString))
        return;
    OwnPtr<ScriptRegexp> compiled;
    String patternValue = pattern ? *pattern : "";
    if (!patternValue.isEmpty()) {
        compiled = compileSkipCallFramePattern(patternValue);
        if (!compiled) {
            *errorString = "Invalid regular expression";
            return;
        }
    }
    m_state->setString(DebuggerAgentState::skipStackPattern, patternValue);
    m_cachedSkipStackRegExp = compiled.release();
    increaseCachedSkipStackGeneration();
    m_skipContentScripts = asBool(skipContentScripts);
    m_state->setBoolean(DebuggerAgentState::skipContentScripts, m_skipContentScripts);
}

void V8DebuggerAgent::setAsyncCallStackDepth(ErrorString* errorString, int depth)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setLong(DebuggerAgentState::asyncCallStackDepth, depth);
    internalSetAsyncCallStackDepth(depth);
}

void V8DebuggerAgent::enablePromiseTracker(ErrorString* errorString, const bool* captureStacks)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, true);
    m_state->setBoolean(DebuggerAgentState::promiseTrackerCaptureStacks, asBool(captureStacks));
    m_promiseTracker->setEnabled(true, asBool(captureStacks));
}

void V8DebuggerAgent::disablePromiseTracker(ErrorString* errorString)
{
    if (!checkEnabled(errorString))
        return;
    m_state->setBoolean(DebuggerAgentState::promiseTrackerEnabled, false);
    m_promiseTracker->setEnabled(false, false);
}

void V8DebuggerAgent::getPromiseById(ErrorString* errorString, int promiseId, const String* objectGroup, RefPtr<RemoteObject>& promise)
{
    if (!checkEnabled(errorString))
        return;
    if (!m_promiseTracker->isEnabled()) {
        *errorString = "Promise tracking is disabled";
        return;
    }
    ScriptValue value = m_promiseTracker->promiseById(promiseId);
    if (value.isEmpty()) {
        *errorString = "Promise with specified ID not found.";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(value.scriptState());
    promise = injectedScript.wrapObject(value, objectGroup ? *objectGroup : "");
}

void V8DebuggerAgent::didUpdatePromise(InspectorFrontend::Debugger::EventType::Enum eventType, PassRefPtr<TypeBuilder::Debugger::PromiseDetails> promise)
{
    if (m_frontend)
        m_frontend->promiseUpdated(eventType, promise);
}

int V8DebuggerAgent::traceAsyncOperationStarting(const String& description)
{
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> callFrames = debugger().currentCallFramesForAsyncStack();
    RefPtrWillBeRawPtr<AsyncCallChain> chain = nullptr;
    if (callFrames.IsEmpty()) {
        if (m_currentAsyncCallChain)
            chain = AsyncCallChain::create(nullptr, m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
    } else {
        chain = AsyncCallChain::create(adoptRefWillBeNoop(new AsyncCallStack(description, callFrames)), m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
    }
    do {
        ++m_lastAsyncOperationId;
        if (m_lastAsyncOperationId <= 0)
            m_lastAsyncOperationId = 1;
    } while (m_asyncOperations.contains(m_lastAsyncOperationId));
    m_asyncOperations.set(m_lastAsyncOperationId, chain);
    if (chain)
        m_asyncOperationNotifications.add(m_lastAsyncOperationId);

    if (m_startingStepIntoAsync) {
        // We have successfully started a StepIntoAsync, so revoke the debugger's StepInto
        // and wait for the corresponding async operation breakpoint.
        ASSERT(m_pausingAsyncOperations.isEmpty());
        m_pausingAsyncOperations.add(m_lastAsyncOperationId);
        m_startingStepIntoAsync = false;
        m_scheduledDebuggerStep = NoStep;
        debugger().clearStepping();
    } else if (m_pausingOnAsyncOperation) {
        m_pausingAsyncOperations.add(m_lastAsyncOperationId);
    }

    if (m_pausedScriptState)
        flushAsyncOperationEvents(nullptr);
    return m_lastAsyncOperationId;
}

void V8DebuggerAgent::traceAsyncCallbackStarting(int operationId)
{
    ASSERT(operationId > 0 || operationId == unknownAsyncOperationId);
    AsyncCallChain* chain = operationId > 0 ? m_asyncOperations.get(operationId) : nullptr;
    // FIXME: extract recursion check into a delegate.
    int recursionLevel = V8RecursionScope::recursionLevel(m_isolate);
    if (chain && (!recursionLevel || (recursionLevel == 1 && Microtask::performingCheckpoint(m_isolate)))) {
        // There can be still an old m_currentAsyncCallChain set if we start running Microtasks
        // right after executing a JS callback but before the corresponding traceAsyncCallbackCompleted().
        // In this case just call traceAsyncCallbackCompleted() now, and the subsequent ones will be ignored.
        //
        // The nested levels count may be greater than 1, for example, when events are guarded via custom
        // traceAsync* calls, like in window.postMessage(). In this case there will be a willHandleEvent
        // instrumentation with unknownAsyncOperationId bumping up the nested levels count.
        if (m_currentAsyncCallChain) {
            ASSERT(m_nestedAsyncCallCount >= 1);
            ASSERT(recursionLevel == 1 && Microtask::performingCheckpoint(m_isolate));
            m_nestedAsyncCallCount = 1;
            traceAsyncCallbackCompleted();
        }

        // Current AsyncCallChain corresponds to the bottommost JS call frame.
        ASSERT(!m_currentAsyncCallChain);
        m_currentAsyncCallChain = chain;
        m_currentAsyncOperationId = operationId;
        m_pendingTraceAsyncOperationCompleted = false;
        m_nestedAsyncCallCount = 1;

        if (m_pausingAsyncOperations.contains(operationId) || m_asyncOperationBreakpoints.contains(operationId)) {
            m_pausingOnAsyncOperation = true;
            m_scheduledDebuggerStep = StepInto;
            m_skippedStepFrameCount = 0;
            m_recursionLevelForStepFrame = 0;
            debugger().setPauseOnNextStatement(true);
        }
    } else {
        if (m_currentAsyncCallChain)
            ++m_nestedAsyncCallCount;
    }
}

void V8DebuggerAgent::traceAsyncCallbackCompleted()
{
    if (!m_nestedAsyncCallCount)
        return;
    ASSERT(m_currentAsyncCallChain);
    --m_nestedAsyncCallCount;
    if (!m_nestedAsyncCallCount) {
        clearCurrentAsyncOperation();
        if (!m_pausingOnAsyncOperation)
            return;
        m_pausingOnAsyncOperation = false;
        m_scheduledDebuggerStep = NoStep;
        debugger().setPauseOnNextStatement(false);
        if (m_startingStepIntoAsync && m_pausingAsyncOperations.isEmpty())
            clearStepIntoAsync();
    }
}

void V8DebuggerAgent::traceAsyncOperationCompleted(int operationId)
{
    ASSERT(operationId > 0 || operationId == unknownAsyncOperationId);
    bool shouldNotify = false;
    if (operationId > 0) {
        if (m_currentAsyncOperationId == operationId) {
            if (m_pendingTraceAsyncOperationCompleted) {
                m_pendingTraceAsyncOperationCompleted = false;
            } else {
                // Delay traceAsyncOperationCompleted() until the last async callback (being currently executed) is done.
                m_pendingTraceAsyncOperationCompleted = true;
                return;
            }
        }
        m_asyncOperations.remove(operationId);
        m_asyncOperationBreakpoints.remove(operationId);
        m_pausingAsyncOperations.remove(operationId);
        shouldNotify = !m_asyncOperationNotifications.take(operationId);
    }
    if (m_startingStepIntoAsync) {
        if (!m_pausingOnAsyncOperation && m_pausingAsyncOperations.isEmpty())
            clearStepIntoAsync();
    }
    if (m_frontend && shouldNotify)
        m_frontend->asyncOperationCompleted(operationId);
}

void V8DebuggerAgent::flushAsyncOperationEvents(ErrorString*)
{
    if (!m_frontend)
        return;

    for (int operationId : m_asyncOperationNotifications) {
        RefPtrWillBeRawPtr<AsyncCallChain> chain = m_asyncOperations.get(operationId);
        ASSERT(chain);
        const AsyncCallStackVector& callStacks = chain->callStacks();
        ASSERT(!callStacks.isEmpty());

        RefPtr<AsyncOperation> operation;
        RefPtr<AsyncStackTrace> lastAsyncStackTrace;
        for (const auto& callStack : callStacks) {
            v8::HandleScope scope(m_isolate);
            RefPtrWillBeRawPtr<ScriptCallStack> scriptCallStack = toScriptCallStack(callStack->callFrames(m_isolate));
            if (!scriptCallStack)
                break;
            if (!operation) {
                operation = AsyncOperation::create()
                    .setId(operationId)
                    .setDescription(callStack->description())
                    .release();
                operation->setStackTrace(scriptCallStack->buildInspectorArray());
                continue;
            }
            RefPtr<AsyncStackTrace> asyncStackTrace = AsyncStackTrace::create()
                .setCallFrames(scriptCallStack->buildInspectorArray());
            asyncStackTrace->setDescription(callStack->description());
            if (lastAsyncStackTrace)
                lastAsyncStackTrace->setAsyncStackTrace(asyncStackTrace);
            else
                operation->setAsyncStackTrace(asyncStackTrace);
            lastAsyncStackTrace = asyncStackTrace.release();
        }

        if (operation)
            m_frontend->asyncOperationStarted(operation.release());
    }

    m_asyncOperationNotifications.clear();
}

void V8DebuggerAgent::clearCurrentAsyncOperation()
{
    if (m_pendingTraceAsyncOperationCompleted && m_currentAsyncOperationId != unknownAsyncOperationId)
        traceAsyncOperationCompleted(m_currentAsyncOperationId);

    m_currentAsyncOperationId = unknownAsyncOperationId;
    m_pendingTraceAsyncOperationCompleted = false;
    m_nestedAsyncCallCount = 0;
    m_currentAsyncCallChain.clear();
}

void V8DebuggerAgent::resetAsyncCallTracker()
{
    clearCurrentAsyncOperation();
    clearStepIntoAsync();
    for (auto& listener: m_asyncCallTrackingListeners)
        listener->resetAsyncOperations();
    m_asyncOperations.clear();
    m_asyncOperationNotifications.clear();
    m_asyncOperationBreakpoints.clear();
}

void V8DebuggerAgent::setAsyncOperationBreakpoint(ErrorString* errorString, int operationId)
{
    if (!trackingAsyncCalls()) {
        *errorString = "Can only perform operation while tracking async call stacks.";
        return;
    }
    if (operationId <= 0) {
        *errorString = "Wrong async operation id.";
        return;
    }
    if (!m_asyncOperations.contains(operationId)) {
        *errorString = "Unknown async operation id.";
        return;
    }
    m_asyncOperationBreakpoints.add(operationId);
}

void V8DebuggerAgent::removeAsyncOperationBreakpoint(ErrorString* errorString, int operationId)
{
    if (!trackingAsyncCalls()) {
        *errorString = "Can only perform operation while tracking async call stacks.";
        return;
    }
    if (operationId <= 0) {
        *errorString = "Wrong async operation id.";
        return;
    }
    m_asyncOperationBreakpoints.remove(operationId);
}

void V8DebuggerAgent::addAsyncCallTrackingListener(AsyncCallTrackingListener* listener)
{
    m_asyncCallTrackingListeners.add(listener);
}

void V8DebuggerAgent::removeAsyncCallTrackingListener(AsyncCallTrackingListener* listener)
{
    ASSERT(m_asyncCallTrackingListeners.contains(listener));
    m_asyncCallTrackingListeners.remove(listener);
}

void V8DebuggerAgent::willCallFunction(int scriptId)
{
    changeJavaScriptRecursionLevel(+1);
    // Fast return.
    if (m_scheduledDebuggerStep != StepInto)
        return;
    // Skip unknown scripts (e.g. InjectedScript).
    if (!m_scripts.contains(String::number(scriptId)))
        return;
    schedulePauseOnNextStatementIfSteppingInto();
}

void V8DebuggerAgent::didCallFunction()
{
    changeJavaScriptRecursionLevel(-1);
}

void V8DebuggerAgent::willEvaluateScript()
{
    changeJavaScriptRecursionLevel(+1);
    schedulePauseOnNextStatementIfSteppingInto();
}

void V8DebuggerAgent::didEvaluateScript()
{
    changeJavaScriptRecursionLevel(-1);
}

void V8DebuggerAgent::changeJavaScriptRecursionLevel(int step)
{
    if (m_javaScriptPauseScheduled && !m_skipAllPauses && !isPaused()) {
        // Do not ever loose user's pause request until we have actually paused.
        debugger().setPauseOnNextStatement(true);
    }
    if (m_scheduledDebuggerStep == StepOut) {
        m_recursionLevelForStepOut += step;
        if (!m_recursionLevelForStepOut) {
            // When StepOut crosses a task boundary (i.e. js -> blink_c++) from where it was requested,
            // switch stepping to step into a next JS task, as if we exited to a blackboxed framework.
            m_scheduledDebuggerStep = StepInto;
            m_skipNextDebuggerStepOut = false;
        }
    }
    if (m_recursionLevelForStepFrame) {
        m_recursionLevelForStepFrame += step;
        if (!m_recursionLevelForStepFrame) {
            // We have walked through a blackboxed framework and got back to where we started.
            // If there was no stepping scheduled, we should cancel the stepping explicitly,
            // since there may be a scheduled StepFrame left.
            // Otherwise, if we were stepping in/over, the StepFrame will stop at the right location,
            // whereas if we were stepping out, we should continue doing so after debugger pauses
            // from the old StepFrame.
            m_skippedStepFrameCount = 0;
            if (m_scheduledDebuggerStep == NoStep)
                debugger().clearStepping();
            else if (m_scheduledDebuggerStep == StepOut)
                m_skipNextDebuggerStepOut = true;
        }
    }
}

PassRefPtr<Array<CallFrame>> V8DebuggerAgent::currentCallFrames()
{
    if (!m_pausedScriptState || m_currentCallStack.IsEmpty())
        return Array<CallFrame>::create();
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(m_pausedScriptState.get());
    if (injectedScript.isEmpty()) {
        ASSERT_NOT_REACHED();
        return Array<CallFrame>::create();
    }

    v8::HandleScope scope(m_isolate);
    v8::Local<v8::Object> currentCallStack = m_currentCallStack.Get(m_isolate);
    return injectedScript.wrapCallFrames(currentCallStack, 0);
}

PassRefPtr<StackTrace> V8DebuggerAgent::currentAsyncStackTrace()
{
    if (!m_pausedScriptState || !trackingAsyncCalls())
        return nullptr;
    const AsyncCallChain* chain = m_currentAsyncCallChain.get();
    if (!chain)
        return nullptr;
    const AsyncCallStackVector& callStacks = chain->callStacks();
    if (callStacks.isEmpty())
        return nullptr;
    RefPtr<StackTrace> result;
    int asyncOrdinal = callStacks.size();
    for (AsyncCallStackVector::const_reverse_iterator it = callStacks.rbegin(); it != callStacks.rend(); ++it, --asyncOrdinal) {
        v8::HandleScope scope(m_isolate);
        v8::Local<v8::Object> callFrames = (*it)->callFrames(m_isolate);
        ScriptState* scriptState =  ScriptState::from(callFrames->CreationContext());
        InjectedScript injectedScript = scriptState ? m_injectedScriptManager->injectedScriptFor(scriptState) : InjectedScript();
        if (injectedScript.isEmpty()) {
            result.clear();
            continue;
        }
        RefPtr<StackTrace> next = StackTrace::create()
            .setCallFrames(injectedScript.wrapCallFrames(callFrames, asyncOrdinal))
            .release();
        next->setDescription((*it)->description());
        if (result)
            next->setAsyncStackTrace(result.release());
        result.swap(next);
    }
    return result.release();
}

PassRefPtrWillBeRawPtr<ScriptAsyncCallStack> V8DebuggerAgent::currentAsyncStackTraceForConsole()
{
    if (!trackingAsyncCalls())
        return nullptr;
    const AsyncCallChain* chain = m_currentAsyncCallChain.get();
    if (!chain)
        return nullptr;
    const AsyncCallStackVector& callStacks = chain->callStacks();
    if (callStacks.isEmpty())
        return nullptr;
    RefPtrWillBeRawPtr<ScriptAsyncCallStack> result = nullptr;
    for (AsyncCallStackVector::const_reverse_iterator it = callStacks.rbegin(); it != callStacks.rend(); ++it) {
        v8::HandleScope scope(m_isolate);
        RefPtr<JavaScriptCallFrame> callFrame = toJavaScriptCallFrame((*it)->callFrames(m_isolate));
        if (!callFrame)
            break;
        result = ScriptAsyncCallStack::create((*it)->description(), toScriptCallStack(callFrame.get()), result.release());
    }
    return result.release();
}

String V8DebuggerAgent::sourceMapURLForScript(const Script& script, CompileResult compileResult)
{
    bool hasSyntaxError = compileResult != CompileSuccess;
    if (!hasSyntaxError)
        return script.sourceMappingURL();
    return ContentSearchUtils::findSourceMapURL(script.source(), ContentSearchUtils::JavaScriptMagicComment);
}

// V8DebuggerListener functions

void V8DebuggerAgent::didParseSource(const ParsedScript& parsedScript)
{
    Script script = parsedScript.script;

    bool hasSyntaxError = parsedScript.compileResult != CompileSuccess;
    if (hasSyntaxError)
        script.setSourceURL(ContentSearchUtils::findSourceURL(script.source(), ContentSearchUtils::JavaScriptMagicComment));

    bool isContentScript = script.isContentScript();
    bool isInternalScript = script.isInternalScript();
    bool hasSourceURL = script.hasSourceURL();
    String scriptURL = script.sourceURL();
    String sourceMapURL = sourceMapURLForScript(script, parsedScript.compileResult);

    const String* sourceMapURLParam = sourceMapURL.isNull() ? nullptr : &sourceMapURL;
    const bool* isContentScriptParam = isContentScript ? &isContentScript : nullptr;
    const bool* isInternalScriptParam = isInternalScript ? &isInternalScript : nullptr;
    const bool* hasSourceURLParam = hasSourceURL ? &hasSourceURL : nullptr;
    if (!hasSyntaxError)
        m_frontend->scriptParsed(parsedScript.scriptId, scriptURL, script.startLine(), script.startColumn(), script.endLine(), script.endColumn(), isContentScriptParam, isInternalScriptParam, sourceMapURLParam, hasSourceURLParam);
    else
        m_frontend->scriptFailedToParse(parsedScript.scriptId, scriptURL, script.startLine(), script.startColumn(), script.endLine(), script.endColumn(), isContentScriptParam, isInternalScriptParam, sourceMapURLParam, hasSourceURLParam);

    m_scripts.set(parsedScript.scriptId, script);

    if (scriptURL.isEmpty() || hasSyntaxError)
        return;

    RefPtr<JSONObject> breakpointsCookie = m_state->getObject(DebuggerAgentState::javaScriptBreakpoints);
    for (auto& cookie : *breakpointsCookie) {
        RefPtr<JSONObject> breakpointObject = cookie.value->asObject();
        bool isRegex;
        breakpointObject->getBoolean(DebuggerAgentState::isRegex, &isRegex);
        String url;
        breakpointObject->getString(DebuggerAgentState::url, &url);
        if (!matches(scriptURL, url, isRegex))
            continue;
        ScriptBreakpoint breakpoint;
        breakpointObject->getNumber(DebuggerAgentState::lineNumber, &breakpoint.lineNumber);
        breakpointObject->getNumber(DebuggerAgentState::columnNumber, &breakpoint.columnNumber);
        breakpointObject->getString(DebuggerAgentState::condition, &breakpoint.condition);
        RefPtr<TypeBuilder::Debugger::Location> location = resolveBreakpoint(cookie.key, parsedScript.scriptId, breakpoint, UserBreakpointSource);
        if (location)
            m_frontend->breakpointResolved(cookie.key, location);
    }
}

V8DebuggerListener::SkipPauseRequest V8DebuggerAgent::didPause(v8::Local<v8::Context> context, v8::Local<v8::Object> callFrames, v8::Local<v8::Value> v8exception, const Vector<String>& hitBreakpoints, bool isPromiseRejection)
{
    ScriptState* scriptState = ScriptState::from(context);
    ScriptValue exception(scriptState, v8exception);

    V8DebuggerListener::SkipPauseRequest result;
    if (m_skipAllPauses)
        result = V8DebuggerListener::Continue;
    else if (!hitBreakpoints.isEmpty())
        result = V8DebuggerListener::NoSkip; // Don't skip explicit breakpoints even if set in frameworks.
    else if (!exception.isEmpty())
        result = shouldSkipExceptionPause();
    else if (m_scheduledDebuggerStep != NoStep || m_javaScriptPauseScheduled || m_pausingOnNativeEvent)
        result = shouldSkipStepPause();
    else
        result = V8DebuggerListener::NoSkip;

    m_skipNextDebuggerStepOut = false;
    if (result != V8DebuggerListener::NoSkip)
        return result;

    // Skip pauses inside V8 internal scripts and on syntax errors.
    if (callFrames.IsEmpty())
        return V8DebuggerListener::Continue;

    ASSERT(scriptState);
    ASSERT(!m_pausedScriptState);
    m_pausedScriptState = scriptState;
    m_currentCallStack.Reset(m_isolate, callFrames);

    if (!exception.isEmpty()) {
        InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(scriptState);
        if (!injectedScript.isEmpty()) {
            m_breakReason = isPromiseRejection ? InspectorFrontend::Debugger::Reason::PromiseRejection : InspectorFrontend::Debugger::Reason::Exception;
            m_breakAuxData = injectedScript.wrapObject(exception, V8DebuggerAgent::backtraceObjectGroup)->openAccessors();
            // m_breakAuxData might be null after this.
        }
    } else if (m_pausingOnAsyncOperation) {
        m_breakReason = InspectorFrontend::Debugger::Reason::AsyncOperation;
        m_breakAuxData = JSONObject::create();
        m_breakAuxData->setNumber("operationId", m_currentAsyncOperationId);
    }

    RefPtr<Array<String>> hitBreakpointIds = Array<String>::create();

    for (const auto& point : hitBreakpoints) {
        DebugServerBreakpointToBreakpointIdAndSourceMap::iterator breakpointIterator = m_serverBreakpoints.find(point);
        if (breakpointIterator != m_serverBreakpoints.end()) {
            const String& localId = breakpointIterator->value.first;
            hitBreakpointIds->addItem(localId);

            BreakpointSource source = breakpointIterator->value.second;
            if (m_breakReason == InspectorFrontend::Debugger::Reason::Other && source == DebugCommandBreakpointSource)
                m_breakReason = InspectorFrontend::Debugger::Reason::DebugCommand;
        }
    }

    if (!m_asyncOperationNotifications.isEmpty())
        flushAsyncOperationEvents(nullptr);

    m_frontend->paused(currentCallFrames(), m_breakReason, m_breakAuxData, hitBreakpointIds, currentAsyncStackTrace());
    m_scheduledDebuggerStep = NoStep;
    m_javaScriptPauseScheduled = false;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    m_skippedStepFrameCount = 0;
    m_recursionLevelForStepFrame = 0;
    clearStepIntoAsync();

    if (!m_continueToLocationBreakpointId.isEmpty()) {
        debugger().removeBreakpoint(m_continueToLocationBreakpointId);
        m_continueToLocationBreakpointId = "";
    }
    return result;
}

void V8DebuggerAgent::didContinue()
{
    m_pausedScriptState = nullptr;
    m_currentCallStack.Reset();
    clearBreakDetails();
    m_frontend->resumed();
}

bool V8DebuggerAgent::canBreakProgram()
{
    return debugger().canBreakProgram();
}

void V8DebuggerAgent::breakProgram(InspectorFrontend::Debugger::Reason::Enum breakReason, PassRefPtr<JSONObject> data)
{
    ASSERT(enabled());
    if (m_skipAllPauses || m_pausedScriptState || isCallStackEmptyOrBlackboxed())
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    m_scheduledDebuggerStep = NoStep;
    m_steppingFromFramework = false;
    m_pausingOnNativeEvent = false;
    clearStepIntoAsync();
    debugger().breakProgram();
}

void V8DebuggerAgent::clearStepIntoAsync()
{
    m_startingStepIntoAsync = false;
    m_pausingOnAsyncOperation = false;
    m_pausingAsyncOperations.clear();
}

bool V8DebuggerAgent::assertPaused(ErrorString* errorString)
{
    if (!m_pausedScriptState) {
        *errorString = "Can only perform operation while paused.";
        return false;
    }
    return true;
}

void V8DebuggerAgent::clearBreakDetails()
{
    m_breakReason = InspectorFrontend::Debugger::Reason::Other;
    m_breakAuxData = nullptr;
}

void V8DebuggerAgent::setBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource source, const String& condition)
{
    String breakpointId = generateBreakpointId(scriptId, lineNumber, columnNumber, source);
    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition);
    resolveBreakpoint(breakpointId, scriptId, breakpoint, source);
}

void V8DebuggerAgent::removeBreakpoint(const String& scriptId, int lineNumber, int columnNumber, BreakpointSource source)
{
    removeBreakpoint(generateBreakpointId(scriptId, lineNumber, columnNumber, source));
}

void V8DebuggerAgent::reset()
{
    m_scheduledDebuggerStep = NoStep;
    m_scripts.clear();
    m_breakpointIdToDebuggerBreakpointIds.clear();
    resetAsyncCallTracker();
    m_promiseTracker->clear();
    if (m_frontend)
        m_frontend->globalObjectCleared();
}

PassRefPtr<TypeBuilder::Debugger::ExceptionDetails> V8DebuggerAgent::createExceptionDetails(v8::Isolate* isolate, v8::Local<v8::Message> message)
{
    RefPtr<ExceptionDetails> exceptionDetails = ExceptionDetails::create().setText(toCoreStringWithUndefinedOrNullCheck(message->Get()));
    exceptionDetails->setLine(message->GetLineNumber());
    exceptionDetails->setColumn(message->GetStartColumn());
    v8::Local<v8::StackTrace> messageStackTrace = message->GetStackTrace();
    if (!messageStackTrace.IsEmpty() && messageStackTrace->GetFrameCount() > 0)
        exceptionDetails->setStackTrace(createScriptCallStack(isolate, messageStackTrace, messageStackTrace->GetFrameCount())->buildInspectorArray());
    return exceptionDetails.release();
}

} // namespace blink
