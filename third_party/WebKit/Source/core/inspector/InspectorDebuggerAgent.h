/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorDebuggerAgent_h
#define InspectorDebuggerAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/V8DebuggerAgent.h"

namespace blink {

class CORE_EXPORT InspectorDebuggerAgent
    : public InspectorBaseAgent<InspectorDebuggerAgent, InspectorFrontend::Debugger>
    , public InspectorBackendDispatcher::DebuggerCommandHandler
    , public V8DebuggerAgent::Client {
public:
    ~InspectorDebuggerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    // InspectorBackendDispatcher::DebuggerCommandHandler implementation.
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setBreakpointsActive(ErrorString*, bool inActive) override;
    void setSkipAllPauses(ErrorString*, bool inSkipped) override;
    void setBreakpointByUrl(ErrorString*, int inLineNumber, const String* inUrl, const String* inUrlRegex, const int* inColumnNumber, const String* inCondition, TypeBuilder::Debugger::BreakpointId* outBreakpointId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location>>& outLocations) override;
    void setBreakpoint(ErrorString*, const RefPtr<JSONObject>& inLocation, const String* inCondition, TypeBuilder::Debugger::BreakpointId* outBreakpointId, RefPtr<TypeBuilder::Debugger::Location>& outActualLocation) override;
    void removeBreakpoint(ErrorString*, const String& inBreakpointId) override;
    void continueToLocation(ErrorString*, const RefPtr<JSONObject>& inLocation, const bool* inInterstatementLocation) override;
    void stepOver(ErrorString*) override;
    void stepInto(ErrorString*) override;
    void stepOut(ErrorString*) override;
    void pause(ErrorString*) override;
    void resume(ErrorString*) override;
    void stepIntoAsync(ErrorString*) override;
    void searchInContent(ErrorString*, const String& inScriptId, const String& inQuery, const bool* inCaseSensitive, const bool* inIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::SearchMatch>>& outResult) override;
    void canSetScriptSource(ErrorString*, bool* outResult) override;
    void setScriptSource(ErrorString*, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>& errorData, const String& inScriptId, const String& inScriptSource, const bool* inPreview, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& optOutCallFrames, TypeBuilder::OptOutput<bool>* optOutStackChanged, RefPtr<TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace) override;
    void restartFrame(ErrorString*, const String& inCallFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& outCallFrames, RefPtr<TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace) override;
    void getScriptSource(ErrorString*, const String& inScriptId, String* outScriptSource) override;
    void getFunctionDetails(ErrorString*, const String& inFunctionId, RefPtr<TypeBuilder::Debugger::FunctionDetails>& outDetails) override;
    void getGeneratorObjectDetails(ErrorString*, const String& inObjectId, RefPtr<TypeBuilder::Debugger::GeneratorObjectDetails>& outDetails) override;
    void getCollectionEntries(ErrorString*, const String& inObjectId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CollectionEntry>>& outEntries) override;
    void setPauseOnExceptions(ErrorString*, const String& inState) override;
    void evaluateOnCallFrame(ErrorString*, const String& inCallFrameId, const String& inExpression, const String* inObjectGroup, const bool* inIncludeCommandLineAPI, const bool* inDoNotPauseOnExceptionsAndMuteConsole, const bool* inReturnByValue, const bool* inGeneratePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>& outResult, TypeBuilder::OptOutput<bool>* optOutWasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& optOutExceptionDetails) override;
    void compileScript(ErrorString*, const String& inExpression, const String& inSourceURL, bool inPersistScript, const int* inExecutionContextId, TypeBuilder::OptOutput<TypeBuilder::Debugger::ScriptId>* optOutScriptId, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& optOutExceptionDetails) override;
    void runScript(ErrorString*, const String& inScriptId, const int* inExecutionContextId, const String* inObjectGroup, const bool* inDoNotPauseOnExceptionsAndMuteConsole, RefPtr<TypeBuilder::Runtime::RemoteObject>& outResult, RefPtr<TypeBuilder::Debugger::ExceptionDetails>& optOutExceptionDetails) override;
    void setVariableValue(ErrorString*, int inScopeNumber, const String& inVariableName, const RefPtr<JSONObject>& inNewValue, const String* inCallFrameId, const String* inFunctionObjectId) override;
    void getStepInPositions(ErrorString*, const String& inCallFrameId, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::Location>>& optOutStepInPositions) override;
    void getBacktrace(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Debugger::CallFrame>>& outCallFrames, RefPtr<TypeBuilder::Debugger::StackTrace>& optOutAsyncStackTrace) override;
    void skipStackFrames(ErrorString*, const String* inScript, const bool* inSkipContentScripts) override;
    void setAsyncCallStackDepth(ErrorString*, int inMaxDepth) override;
    void enablePromiseTracker(ErrorString*, const bool* inCaptureStacks) override;
    void disablePromiseTracker(ErrorString*) override;
    void getPromiseById(ErrorString*, int inPromiseId, const String* inObjectGroup, RefPtr<TypeBuilder::Runtime::RemoteObject>& outPromise) override;
    void flushAsyncOperationEvents(ErrorString*) override;
    void setAsyncOperationBreakpoint(ErrorString*, int inOperationId) override;
    void removeAsyncOperationBreakpoint(ErrorString*, int inOperationId) override;

    // V8DebuggerAgent::Client implementation.
    void debuggerAgentEnabled() override;
    void debuggerAgentDisabled() override;

    // Called by InspectorInstrumentation.
    bool isPaused();
    PassRefPtrWillBeRawPtr<ScriptAsyncCallStack> currentAsyncStackTraceForConsole();
    void didFireTimer();
    void didHandleEvent();
    void scriptExecutionBlockedByCSP(const String& directiveText);
    void willCallFunction(const DevToolsFunctionInfo&);
    void didCallFunction();
    void willEvaluateScript();
    void didEvaluateScript();

    // InspectorBaseAgent overrides.
    void init() override;
    void setFrontend(InspectorFrontend*) override;
    void clearFrontend() override;
    void restore() override;

    V8DebuggerAgent* v8DebuggerAgent() const { return m_v8DebuggerAgent.get(); }

protected:
    InspectorDebuggerAgent(InjectedScriptManager*, V8Debugger*, int contextGroupId);

    OwnPtrWillBeMember<V8DebuggerAgent> m_v8DebuggerAgent;
};

} // namespace blink


#endif // !defined(InspectorDebuggerAgent_h)
