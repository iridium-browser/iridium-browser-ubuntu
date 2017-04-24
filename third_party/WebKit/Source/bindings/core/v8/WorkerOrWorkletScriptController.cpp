/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"

#include <memory>

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/core/v8/V8ErrorHandler.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/events/ErrorEvent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/heap/ThreadState.h"
#include "public/platform/Platform.h"
#include "v8/include/v8.h"

namespace blink {

class WorkerOrWorkletScriptController::ExecutionState final {
  STACK_ALLOCATED();

 public:
  explicit ExecutionState(WorkerOrWorkletScriptController* controller)
      : hadException(false),
        m_controller(controller),
        m_outerState(controller->m_executionState) {
    m_controller->m_executionState = this;
  }

  ~ExecutionState() { m_controller->m_executionState = m_outerState; }

  bool hadException;
  String errorMessage;
  std::unique_ptr<SourceLocation> m_location;
  ScriptValue exception;
  Member<ErrorEvent> m_errorEventFromImportedScript;

  // A ExecutionState context is stack allocated by
  // WorkerOrWorkletScriptController::evaluate(), with the contoller using it
  // during script evaluation. To handle nested evaluate() uses,
  // ExecutionStates are chained together;
  // |m_outerState| keeps a pointer to the context object one level out
  // (or 0, if outermost.) Upon return from evaluate(), the
  // WorkerOrWorkletScriptController's ExecutionState is popped and the
  // previous one restored (see above dtor.)
  //
  // With Oilpan, |m_outerState| isn't traced. It'll be "up the stack"
  // and its fields will be traced when scanning the stack.
  Member<WorkerOrWorkletScriptController> m_controller;
  ExecutionState* m_outerState;
};

WorkerOrWorkletScriptController* WorkerOrWorkletScriptController::create(
    WorkerOrWorkletGlobalScope* globalScope,
    v8::Isolate* isolate) {
  return new WorkerOrWorkletScriptController(globalScope, isolate);
}

WorkerOrWorkletScriptController::WorkerOrWorkletScriptController(
    WorkerOrWorkletGlobalScope* globalScope,
    v8::Isolate* isolate)
    : m_globalScope(globalScope),
      m_isolate(isolate),
      m_executionForbidden(false),
      m_rejectedPromises(RejectedPromises::create()),
      m_executionState(0) {
  ASSERT(isolate);
  m_world = DOMWrapperWorld::create(isolate, WorkerWorldId);
}

WorkerOrWorkletScriptController::~WorkerOrWorkletScriptController() {
  ASSERT(!m_rejectedPromises);
}

void WorkerOrWorkletScriptController::dispose() {
  m_rejectedPromises->dispose();
  m_rejectedPromises.release();

  m_world->dispose();

  disposeContextIfNeeded();
}

void WorkerOrWorkletScriptController::disposeContextIfNeeded() {
  if (!isContextInitialized())
    return;

  if (m_globalScope->isWorkerGlobalScope() ||
      m_globalScope->isThreadedWorkletGlobalScope()) {
    ScriptState::Scope scope(m_scriptState.get());
    WorkerThreadDebugger* debugger = WorkerThreadDebugger::from(m_isolate);
    debugger->contextWillBeDestroyed(m_globalScope->thread(),
                                     m_scriptState->context());
  }
  m_scriptState->disposePerContextData();
}

bool WorkerOrWorkletScriptController::initializeContextIfNeeded() {
  v8::HandleScope handleScope(m_isolate);

  if (isContextInitialized())
    return true;

  // Create a new v8::Context with the worker/worklet as the global object
  // (aka the inner global).
  ScriptWrappable* scriptWrappable = m_globalScope->getScriptWrappable();
  const WrapperTypeInfo* wrapperTypeInfo = scriptWrappable->wrapperTypeInfo();
  v8::Local<v8::FunctionTemplate> globalInterfaceTemplate =
      wrapperTypeInfo->domTemplate(m_isolate, *m_world);
  if (globalInterfaceTemplate.IsEmpty())
    return false;
  v8::Local<v8::ObjectTemplate> globalTemplate =
      globalInterfaceTemplate->InstanceTemplate();
  v8::Local<v8::Context> context;
  {
    // Initialize V8 extensions before creating the context.
    Vector<const char*> extensionNames;
    if (m_globalScope->isServiceWorkerGlobalScope() &&
        Platform::current()->allowScriptExtensionForServiceWorker(
            toWorkerGlobalScope(m_globalScope.get())->url())) {
      const V8Extensions& extensions = ScriptController::registeredExtensions();
      extensionNames.reserveInitialCapacity(extensions.size());
      for (const auto* extension : extensions)
        extensionNames.push_back(extension->name());
    }
    v8::ExtensionConfiguration extensionConfiguration(extensionNames.size(),
                                                      extensionNames.data());

    V8PerIsolateData::UseCounterDisabledScope useCounterDisabled(
        V8PerIsolateData::from(m_isolate));
    context =
        v8::Context::New(m_isolate, &extensionConfiguration, globalTemplate);
  }
  if (context.IsEmpty())
    return false;

  m_scriptState = ScriptState::create(context, m_world);

  ScriptState::Scope scope(m_scriptState.get());

  // Associate the global proxy object, the global object and the worker
  // instance (C++ object) as follows.
  //
  //   global proxy object <====> worker or worklet instance
  //                               ^
  //                               |
  //   global object       --------+
  //
  // Per HTML spec, there is no corresponding object for workers to WindowProxy.
  // However, V8 always creates the global proxy object, we associate these
  // objects in the same manner as WindowProxy and Window.
  //
  // a) worker or worklet instance --> global proxy object
  // As we shouldn't expose the global object to author scripts, we map the
  // worker or worklet instance to the global proxy object.
  // b) global proxy object --> worker or worklet instance
  // Blink's callback functions are called by V8 with the global proxy object,
  // we need to map the global proxy object to the worker or worklet instance.
  // c) global object --> worker or worklet instance
  // The global proxy object is NOT considered as a wrapper object of the
  // worker or worklet instance because it's not an instance of
  // v8::FunctionTemplate of worker or worklet, especially note that
  // v8::Object::FindInstanceInPrototypeChain skips the global proxy object.
  // Thus we need to map the global object to the worker or worklet instance.

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> globalProxy = context->Global();
  v8::Local<v8::Object> associatedWrapper =
      V8DOMWrapper::associateObjectWithWrapper(
          m_isolate, scriptWrappable, wrapperTypeInfo, globalProxy);
  CHECK(globalProxy == associatedWrapper);

  // The global object, aka worker/worklet wrapper object.
  v8::Local<v8::Object> globalObject =
      globalProxy->GetPrototype().As<v8::Object>();
  V8DOMWrapper::setNativeInfo(m_isolate, globalObject, wrapperTypeInfo,
                              scriptWrappable);

  // All interfaces must be registered to V8PerContextData.
  // So we explicitly call constructorForType for the global object.
  V8PerContextData::from(context)->constructorForType(wrapperTypeInfo);

  // Name new context for debugging. For main thread worklet global scopes
  // this is done once the context is initialized.
  if (m_globalScope->isWorkerGlobalScope() ||
      m_globalScope->isThreadedWorkletGlobalScope()) {
    WorkerThreadDebugger* debugger = WorkerThreadDebugger::from(m_isolate);
    debugger->contextCreated(m_globalScope->thread(), context);
  }

  return true;
}

ScriptValue WorkerOrWorkletScriptController::evaluate(
    const String& script,
    const String& fileName,
    const TextPosition& scriptStartPosition,
    CachedMetadataHandler* cacheHandler,
    V8CacheOptions v8CacheOptions) {
  TRACE_EVENT1("devtools.timeline", "EvaluateScript", "data",
               InspectorEvaluateScriptEvent::data(nullptr, fileName,
                                                  scriptStartPosition));
  if (!initializeContextIfNeeded())
    return ScriptValue();

  ScriptState::Scope scope(m_scriptState.get());

  if (!m_disableEvalPending.isEmpty()) {
    m_scriptState->context()->AllowCodeGenerationFromStrings(false);
    m_scriptState->context()->SetErrorMessageForCodeGenerationFromStrings(
        v8String(m_isolate, m_disableEvalPending));
    m_disableEvalPending = String();
  }

  v8::TryCatch block(m_isolate);

  v8::Local<v8::Script> compiledScript;
  v8::MaybeLocal<v8::Value> maybeResult;
  if (v8Call(V8ScriptRunner::compileScript(
                 script, fileName, String(), scriptStartPosition, m_isolate,
                 cacheHandler, SharableCrossOrigin, v8CacheOptions),
             compiledScript, block))
    maybeResult = V8ScriptRunner::runCompiledScript(m_isolate, compiledScript,
                                                    m_globalScope);

  if (!block.CanContinue()) {
    forbidExecution();
    return ScriptValue();
  }

  if (block.HasCaught()) {
    v8::Local<v8::Message> message = block.Message();
    m_executionState->hadException = true;
    m_executionState->errorMessage = toCoreString(message->Get());
    m_executionState->m_location = SourceLocation::fromMessage(
        m_isolate, message, m_scriptState->getExecutionContext());
    m_executionState->exception =
        ScriptValue(m_scriptState.get(), block.Exception());
    block.Reset();
  } else {
    m_executionState->hadException = false;
  }

  v8::Local<v8::Value> result;
  if (!maybeResult.ToLocal(&result) || result->IsUndefined())
    return ScriptValue();

  return ScriptValue(m_scriptState.get(), result);
}

bool WorkerOrWorkletScriptController::evaluate(
    const ScriptSourceCode& sourceCode,
    ErrorEvent** errorEvent,
    CachedMetadataHandler* cacheHandler,
    V8CacheOptions v8CacheOptions) {
  if (isExecutionForbidden())
    return false;

  ExecutionState state(this);
  evaluate(sourceCode.source(), sourceCode.url().getString(),
           sourceCode.startPosition(), cacheHandler, v8CacheOptions);
  if (isExecutionForbidden())
    return false;
  if (state.hadException) {
    if (errorEvent) {
      if (state.m_errorEventFromImportedScript) {
        // Propagate inner error event outwards.
        *errorEvent = state.m_errorEventFromImportedScript.release();
        return false;
      }
      if (m_globalScope->shouldSanitizeScriptError(state.m_location->url(),
                                                   NotSharableCrossOrigin))
        *errorEvent = ErrorEvent::createSanitizedError(m_world.get());
      else
        *errorEvent = ErrorEvent::create(
            state.errorMessage, state.m_location->clone(), m_world.get());
      V8ErrorHandler::storeExceptionOnErrorEventWrapper(
          m_scriptState.get(), *errorEvent, state.exception.v8Value(),
          m_scriptState->context()->Global());
    } else {
      DCHECK(!m_globalScope->shouldSanitizeScriptError(state.m_location->url(),
                                                       NotSharableCrossOrigin));
      ErrorEvent* event = nullptr;
      if (state.m_errorEventFromImportedScript)
        event = state.m_errorEventFromImportedScript.release();
      else
        event = ErrorEvent::create(state.errorMessage,
                                   state.m_location->clone(), m_world.get());
      m_globalScope->dispatchErrorEvent(event, NotSharableCrossOrigin);
    }
    return false;
  }
  return true;
}

void WorkerOrWorkletScriptController::forbidExecution() {
  ASSERT(m_globalScope->isContextThread());
  m_executionForbidden = true;
}

bool WorkerOrWorkletScriptController::isExecutionForbidden() const {
  ASSERT(m_globalScope->isContextThread());
  return m_executionForbidden;
}

void WorkerOrWorkletScriptController::disableEval(const String& errorMessage) {
  m_disableEvalPending = errorMessage;
}

void WorkerOrWorkletScriptController::rethrowExceptionFromImportedScript(
    ErrorEvent* errorEvent,
    ExceptionState& exceptionState) {
  const String& errorMessage = errorEvent->message();
  if (m_executionState)
    m_executionState->m_errorEventFromImportedScript = errorEvent;
  exceptionState.rethrowV8Exception(
      V8ThrowException::createError(m_isolate, errorMessage));
}

DEFINE_TRACE(WorkerOrWorkletScriptController) {
  visitor->trace(m_globalScope);
}

}  // namespace blink
