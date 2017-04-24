/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/V8PerIsolateData.h"

#include <memory>

#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8PrivateProperty.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8ValueCache.h"
#include "platform/ScriptForbiddenScope.h"
#include "public/platform/Platform.h"
#include "v8/include/v8-debug.h"
#include "wtf/LeakAnnotations.h"
#include "wtf/PtrUtil.h"

namespace blink {

static V8PerIsolateData* mainThreadPerIsolateData = 0;

static void beforeCallEnteredCallback(v8::Isolate* isolate) {
  RELEASE_ASSERT(!ScriptForbiddenScope::isScriptForbidden());
}

static void microtasksCompletedCallback(v8::Isolate* isolate) {
  V8PerIsolateData::from(isolate)->runEndOfScopeTasks();
}

V8PerIsolateData::V8PerIsolateData(WebTaskRunner* taskRunner)
    : m_isolateHolder(WTF::makeUnique<gin::IsolateHolder>(
          taskRunner ? taskRunner->toSingleThreadTaskRunner() : nullptr,
          gin::IsolateHolder::kSingleThread,
          isMainThread() ? gin::IsolateHolder::kDisallowAtomicsWait
                         : gin::IsolateHolder::kAllowAtomicsWait)),
      m_stringCache(WTF::wrapUnique(new StringCache(isolate()))),
      m_hiddenValue(V8HiddenValue::create()),
      m_privateProperty(V8PrivateProperty::create()),
      m_constructorMode(ConstructorMode::CreateNewObject),
      m_useCounterDisabled(false),
      m_isHandlingRecursionLevelError(false),
      m_isReportingException(false) {
  // FIXME: Remove once all v8::Isolate::GetCurrent() calls are gone.
  isolate()->Enter();
  isolate()->AddBeforeCallEnteredCallback(&beforeCallEnteredCallback);
  isolate()->AddMicrotasksCompletedCallback(&microtasksCompletedCallback);
  if (isMainThread())
    mainThreadPerIsolateData = this;
}

V8PerIsolateData::~V8PerIsolateData() {}

v8::Isolate* V8PerIsolateData::mainThreadIsolate() {
  ASSERT(mainThreadPerIsolateData);
  return mainThreadPerIsolateData->isolate();
}

v8::Isolate* V8PerIsolateData::initialize(WebTaskRunner* taskRunner) {
  V8PerIsolateData* data = new V8PerIsolateData(taskRunner);
  v8::Isolate* isolate = data->isolate();
  isolate->SetData(gin::kEmbedderBlink, data);
  return isolate;
}

void V8PerIsolateData::enableIdleTasks(
    v8::Isolate* isolate,
    std::unique_ptr<gin::V8IdleTaskRunner> taskRunner) {
  from(isolate)->m_isolateHolder->EnableIdleTasks(
      std::unique_ptr<gin::V8IdleTaskRunner>(taskRunner.release()));
}

v8::Persistent<v8::Value>& V8PerIsolateData::ensureLiveRoot() {
  if (m_liveRoot.isEmpty())
    m_liveRoot.set(isolate(), v8::Null(isolate()));
  return m_liveRoot.get();
}

// willBeDestroyed() clear things that should be cleared before
// ThreadState::detach() gets called.
void V8PerIsolateData::willBeDestroyed(v8::Isolate* isolate) {
  V8PerIsolateData* data = from(isolate);

  data->m_threadDebugger.reset();
  // Clear any data that may have handles into the heap,
  // prior to calling ThreadState::detach().
  data->clearEndOfScopeTasks();

  data->m_activeScriptWrappables.clear();
}

// destroy() clear things that should be cleared after ThreadState::detach()
// gets called but before the Isolate exits.
void V8PerIsolateData::destroy(v8::Isolate* isolate) {
  isolate->RemoveBeforeCallEnteredCallback(&beforeCallEnteredCallback);
  isolate->RemoveMicrotasksCompletedCallback(&microtasksCompletedCallback);
  V8PerIsolateData* data = from(isolate);

  // Clear everything before exiting the Isolate.
  if (data->m_scriptRegexpScriptState)
    data->m_scriptRegexpScriptState->disposePerContextData();
  data->m_liveRoot.clear();
  data->m_hiddenValue.reset();
  data->m_privateProperty.reset();
  data->m_stringCache->dispose();
  data->m_stringCache.reset();
  data->m_interfaceTemplateMapForNonMainWorld.clear();
  data->m_interfaceTemplateMapForMainWorld.clear();
  data->m_operationTemplateMapForNonMainWorld.clear();
  data->m_operationTemplateMapForMainWorld.clear();
  if (isMainThread())
    mainThreadPerIsolateData = 0;

  // FIXME: Remove once all v8::Isolate::GetCurrent() calls are gone.
  isolate->Exit();
  delete data;
}

V8PerIsolateData::V8FunctionTemplateMap&
V8PerIsolateData::selectInterfaceTemplateMap(const DOMWrapperWorld& world) {
  return world.isMainWorld() ? m_interfaceTemplateMapForMainWorld
                             : m_interfaceTemplateMapForNonMainWorld;
}

V8PerIsolateData::V8FunctionTemplateMap&
V8PerIsolateData::selectOperationTemplateMap(const DOMWrapperWorld& world) {
  return world.isMainWorld() ? m_operationTemplateMapForMainWorld
                             : m_operationTemplateMapForNonMainWorld;
}

v8::Local<v8::FunctionTemplate> V8PerIsolateData::findOrCreateOperationTemplate(
    const DOMWrapperWorld& world,
    const void* key,
    v8::FunctionCallback callback,
    v8::Local<v8::Value> data,
    v8::Local<v8::Signature> signature,
    int length) {
  auto& map = selectOperationTemplateMap(world);
  auto result = map.find(key);
  if (result != map.end())
    return result->value.Get(isolate());

  v8::Local<v8::FunctionTemplate> templ =
      v8::FunctionTemplate::New(isolate(), callback, data, signature, length);
  templ->RemovePrototype();
  map.insert(key, v8::Eternal<v8::FunctionTemplate>(isolate(), templ));
  return templ;
}

v8::Local<v8::FunctionTemplate> V8PerIsolateData::findInterfaceTemplate(
    const DOMWrapperWorld& world,
    const void* key) {
  auto& map = selectInterfaceTemplateMap(world);
  auto result = map.find(key);
  if (result != map.end())
    return result->value.Get(isolate());
  return v8::Local<v8::FunctionTemplate>();
}

void V8PerIsolateData::setInterfaceTemplate(
    const DOMWrapperWorld& world,
    const void* key,
    v8::Local<v8::FunctionTemplate> value) {
  auto& map = selectInterfaceTemplateMap(world);
  map.insert(key, v8::Eternal<v8::FunctionTemplate>(isolate(), value));
}

v8::Local<v8::Context> V8PerIsolateData::ensureScriptRegexpContext() {
  if (!m_scriptRegexpScriptState) {
    LEAK_SANITIZER_DISABLED_SCOPE;
    v8::Local<v8::Context> context(v8::Context::New(isolate()));
    m_scriptRegexpScriptState =
        ScriptState::create(context, DOMWrapperWorld::create(isolate()));
  }
  return m_scriptRegexpScriptState->context();
}

void V8PerIsolateData::clearScriptRegexpContext() {
  if (m_scriptRegexpScriptState)
    m_scriptRegexpScriptState->disposePerContextData();
  m_scriptRegexpScriptState.clear();
}

bool V8PerIsolateData::hasInstance(
    const WrapperTypeInfo* untrustedWrapperTypeInfo,
    v8::Local<v8::Value> value) {
  return hasInstance(untrustedWrapperTypeInfo, value,
                     m_interfaceTemplateMapForMainWorld) ||
         hasInstance(untrustedWrapperTypeInfo, value,
                     m_interfaceTemplateMapForNonMainWorld);
}

bool V8PerIsolateData::hasInstance(
    const WrapperTypeInfo* untrustedWrapperTypeInfo,
    v8::Local<v8::Value> value,
    V8FunctionTemplateMap& map) {
  auto result = map.find(untrustedWrapperTypeInfo);
  if (result == map.end())
    return false;
  v8::Local<v8::FunctionTemplate> templ = result->value.Get(isolate());
  return templ->HasInstance(value);
}

v8::Local<v8::Object> V8PerIsolateData::findInstanceInPrototypeChain(
    const WrapperTypeInfo* info,
    v8::Local<v8::Value> value) {
  v8::Local<v8::Object> wrapper = findInstanceInPrototypeChain(
      info, value, m_interfaceTemplateMapForMainWorld);
  if (!wrapper.IsEmpty())
    return wrapper;
  return findInstanceInPrototypeChain(info, value,
                                      m_interfaceTemplateMapForNonMainWorld);
}

v8::Local<v8::Object> V8PerIsolateData::findInstanceInPrototypeChain(
    const WrapperTypeInfo* info,
    v8::Local<v8::Value> value,
    V8FunctionTemplateMap& map) {
  if (value.IsEmpty() || !value->IsObject())
    return v8::Local<v8::Object>();
  auto result = map.find(info);
  if (result == map.end())
    return v8::Local<v8::Object>();
  v8::Local<v8::FunctionTemplate> templ = result->value.Get(isolate());
  return v8::Local<v8::Object>::Cast(value)->FindInstanceInPrototypeChain(
      templ);
}

void V8PerIsolateData::addEndOfScopeTask(std::unique_ptr<EndOfScopeTask> task) {
  m_endOfScopeTasks.push_back(std::move(task));
}

void V8PerIsolateData::runEndOfScopeTasks() {
  Vector<std::unique_ptr<EndOfScopeTask>> tasks;
  tasks.swap(m_endOfScopeTasks);
  for (const auto& task : tasks)
    task->run();
  ASSERT(m_endOfScopeTasks.isEmpty());
}

void V8PerIsolateData::clearEndOfScopeTasks() {
  m_endOfScopeTasks.clear();
}

void V8PerIsolateData::setThreadDebugger(
    std::unique_ptr<V8PerIsolateData::Data> threadDebugger) {
  ASSERT(!m_threadDebugger);
  m_threadDebugger = std::move(threadDebugger);
}

V8PerIsolateData::Data* V8PerIsolateData::threadDebugger() {
  return m_threadDebugger.get();
}

void V8PerIsolateData::addActiveScriptWrappable(
    ActiveScriptWrappableBase* wrappable) {
  if (!m_activeScriptWrappables)
    m_activeScriptWrappables = new ActiveScriptWrappableSet();

  m_activeScriptWrappables->insert(wrappable);
}

void V8PerIsolateData::TemporaryScriptWrappableVisitorScope::
    swapWithV8PerIsolateDataVisitor(
        std::unique_ptr<ScriptWrappableVisitor>& visitor) {
  ScriptWrappableVisitor* current = currentVisitor();
  if (current)
    current->performCleanup();

  V8PerIsolateData::from(m_isolate)->m_scriptWrappableVisitor.swap(
      m_savedVisitor);
  m_isolate->SetEmbedderHeapTracer(currentVisitor());
}

}  // namespace blink
