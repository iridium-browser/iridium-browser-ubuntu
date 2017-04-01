/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/inspector/InspectorDOMDebuggerAgent.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/V8EventTarget.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/V8InspectorString.h"

namespace {

enum DOMBreakpointType {
  SubtreeModified = 0,
  AttributeModified,
  NodeRemoved,
  DOMBreakpointTypesCount
};

static const char listenerEventCategoryType[] = "listener:";
static const char instrumentationEventCategoryType[] = "instrumentation:";

const uint32_t inheritableDOMBreakpointTypesMask = (1 << SubtreeModified);
const int domBreakpointDerivedTypeShift = 16;

}  // namespace

namespace blink {

static const char webglErrorFiredEventName[] = "webglErrorFired";
static const char webglWarningFiredEventName[] = "webglWarningFired";
static const char webglErrorNameProperty[] = "webglErrorName";
static const char scriptBlockedByCSPEventName[] = "scriptBlockedByCSP";

namespace DOMDebuggerAgentState {
static const char eventListenerBreakpoints[] = "eventListenerBreakpoints";
static const char eventTargetAny[] = "*";
static const char pauseOnAllXHRs[] = "pauseOnAllXHRs";
static const char xhrBreakpoints[] = "xhrBreakpoints";
static const char enabled[] = "enabled";
}

static void removeEventListenerCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> data = info.Data().As<v8::Object>();

  v8::Local<v8::Value> v8Target;
  if (!data->Get(context, v8String(isolate, "target")).ToLocal(&v8Target) ||
      !v8Target->IsObject())
    return;
  EventTarget* target = V8EventTarget::toImplWithTypeCheck(isolate, v8Target);
  // We need to handle LocalDOMWindow specially, because LocalDOMWindow wrapper
  // exists on prototype chain.
  if (!target)
    target = toDOMWindow(isolate, v8Target);
  if (!target || !target->getExecutionContext())
    return;

  v8::Local<v8::Value> v8Handler;
  if (!data->Get(context, v8String(isolate, "handler")).ToLocal(&v8Handler) ||
      !v8Handler->IsObject())
    return;
  v8::Local<v8::Value> v8Type;
  if (!data->Get(context, v8String(isolate, "type")).ToLocal(&v8Type) ||
      !v8Type->IsString())
    return;
  AtomicString type =
      AtomicString(toCoreString(v8::Local<v8::String>::Cast(v8Type)));
  v8::Local<v8::Value> v8UseCapture;
  if (!data->Get(context, v8String(isolate, "useCapture"))
           .ToLocal(&v8UseCapture) ||
      !v8UseCapture->IsBoolean())
    return;
  bool useCapture = v8::Local<v8::Boolean>::Cast(v8UseCapture)->Value();

  EventListener* eventListener = nullptr;
  EventListenerVector* listeners = target->getEventListeners(type);
  if (!listeners)
    return;
  for (size_t i = 0; i < listeners->size(); ++i) {
    if (listeners->at(i).capture() != useCapture)
      continue;
    V8AbstractEventListener* v8Listener =
        V8AbstractEventListener::cast(listeners->at(i).listener());
    if (!v8Listener)
      continue;
    if (!v8Listener->hasExistingListenerObject())
      continue;
    if (!v8Listener->getExistingListenerObject()
             ->Equals(context, v8Handler)
             .FromMaybe(false))
      continue;
    eventListener = v8Listener;
    break;
  }
  if (!eventListener)
    return;
  EventListenerOptions options;
  options.setCapture(useCapture);
  target->removeEventListener(type, eventListener, options);
}

static void returnDataCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(info.Data());
}

static v8::MaybeLocal<v8::Function> createRemoveFunction(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> object,
    v8::Local<v8::Object> handler,
    AtomicString type,
    bool useCapture) {
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Object> data = v8::Object::New(isolate);
  if (!data->Set(context, v8String(isolate, "target"), object).FromMaybe(false))
    return v8::MaybeLocal<v8::Function>();
  if (!data->Set(context, v8String(isolate, "handler"), handler)
           .FromMaybe(false))
    return v8::MaybeLocal<v8::Function>();
  if (!data->Set(context, v8String(isolate, "type"), v8String(isolate, type))
           .FromMaybe(false))
    return v8::MaybeLocal<v8::Function>();
  if (!data->Set(context, v8String(isolate, "useCapture"),
                 v8Boolean(useCapture, isolate))
           .FromMaybe(false))
    return v8::MaybeLocal<v8::Function>();
  v8::Local<v8::Function> removeFunction =
      v8::Function::New(context, removeEventListenerCallback, data, 0,
                        v8::ConstructorBehavior::kThrow)
          .ToLocalChecked();
  v8::Local<v8::Function> toStringFunction;
  if (v8::Function::New(
          context, returnDataCallback,
          v8String(isolate, "function remove() { [Command Line API] }"), 0,
          v8::ConstructorBehavior::kThrow)
          .ToLocal(&toStringFunction))
    removeFunction->Set(v8String(context->GetIsolate(), "toString"),
                        toStringFunction);
  return removeFunction;
}

void InspectorDOMDebuggerAgent::eventListenersInfoForTarget(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    V8EventListenerInfoList& eventInformation) {
  EventTarget* target = V8EventTarget::toImplWithTypeCheck(isolate, value);
  // We need to handle LocalDOMWindow specially, because LocalDOMWindow wrapper
  // exists on prototype chain.
  if (!target)
    target = toDOMWindow(isolate, value);
  if (!target || !target->getExecutionContext())
    return;

  ExecutionContext* executionContext = target->getExecutionContext();

  // Nodes and their Listeners for the concerned event types (order is top to
  // bottom).
  Vector<AtomicString> eventTypes = target->eventTypes();
  for (size_t j = 0; j < eventTypes.size(); ++j) {
    AtomicString& type = eventTypes[j];
    EventListenerVector* listeners = target->getEventListeners(type);
    if (!listeners)
      continue;
    for (size_t k = 0; k < listeners->size(); ++k) {
      EventListener* eventListener = listeners->at(k).listener();
      if (eventListener->type() != EventListener::JSEventListenerType)
        continue;
      V8AbstractEventListener* v8Listener =
          static_cast<V8AbstractEventListener*>(eventListener);
      v8::Local<v8::Context> context =
          toV8Context(executionContext, v8Listener->world());
      // Hide listeners from other contexts.
      if (context != isolate->GetCurrentContext())
        continue;
      // getListenerObject() may cause JS in the event attribute to get
      // compiled, potentially unsuccessfully.  In that case, the function
      // returns the empty handle without an exception.
      v8::Local<v8::Object> handler =
          v8Listener->getListenerObject(executionContext);
      if (handler.IsEmpty())
        continue;
      bool useCapture = listeners->at(k).capture();
      eventInformation.push_back(V8EventListenerInfo(
          type, useCapture, listeners->at(k).passive(), listeners->at(k).once(),
          handler,
          createRemoveFunction(context, value, handler, type, useCapture)));
    }
  }
}

InspectorDOMDebuggerAgent::InspectorDOMDebuggerAgent(
    v8::Isolate* isolate,
    InspectorDOMAgent* domAgent,
    v8_inspector::V8InspectorSession* v8Session)
    : m_isolate(isolate), m_domAgent(domAgent), m_v8Session(v8Session) {}

InspectorDOMDebuggerAgent::~InspectorDOMDebuggerAgent() {}

DEFINE_TRACE(InspectorDOMDebuggerAgent) {
  visitor->trace(m_domAgent);
  visitor->trace(m_domBreakpoints);
  InspectorBaseAgent::trace(visitor);
}

Response InspectorDOMDebuggerAgent::disable() {
  setEnabled(false);
  m_domBreakpoints.clear();
  m_state->remove(DOMDebuggerAgentState::eventListenerBreakpoints);
  m_state->remove(DOMDebuggerAgentState::xhrBreakpoints);
  m_state->remove(DOMDebuggerAgentState::pauseOnAllXHRs);
  return Response::OK();
}

void InspectorDOMDebuggerAgent::restore() {
  if (m_state->booleanProperty(DOMDebuggerAgentState::enabled, false))
    m_instrumentingAgents->addInspectorDOMDebuggerAgent(this);
}

Response InspectorDOMDebuggerAgent::setEventListenerBreakpoint(
    const String& eventName,
    Maybe<String> targetName) {
  return setBreakpoint(String(listenerEventCategoryType) + eventName,
                       targetName.fromMaybe(String()));
}

Response InspectorDOMDebuggerAgent::setInstrumentationBreakpoint(
    const String& eventName) {
  return setBreakpoint(String(instrumentationEventCategoryType) + eventName,
                       String());
}

static protocol::DictionaryValue* ensurePropertyObject(
    protocol::DictionaryValue* object,
    const String& propertyName) {
  protocol::Value* value = object->get(propertyName);
  if (value)
    return protocol::DictionaryValue::cast(value);

  std::unique_ptr<protocol::DictionaryValue> newResult =
      protocol::DictionaryValue::create();
  protocol::DictionaryValue* result = newResult.get();
  object->setObject(propertyName, std::move(newResult));
  return result;
}

protocol::DictionaryValue*
InspectorDOMDebuggerAgent::eventListenerBreakpoints() {
  protocol::DictionaryValue* breakpoints =
      m_state->getObject(DOMDebuggerAgentState::eventListenerBreakpoints);
  if (!breakpoints) {
    std::unique_ptr<protocol::DictionaryValue> newBreakpoints =
        protocol::DictionaryValue::create();
    breakpoints = newBreakpoints.get();
    m_state->setObject(DOMDebuggerAgentState::eventListenerBreakpoints,
                       std::move(newBreakpoints));
  }
  return breakpoints;
}

protocol::DictionaryValue* InspectorDOMDebuggerAgent::xhrBreakpoints() {
  protocol::DictionaryValue* breakpoints =
      m_state->getObject(DOMDebuggerAgentState::xhrBreakpoints);
  if (!breakpoints) {
    std::unique_ptr<protocol::DictionaryValue> newBreakpoints =
        protocol::DictionaryValue::create();
    breakpoints = newBreakpoints.get();
    m_state->setObject(DOMDebuggerAgentState::xhrBreakpoints,
                       std::move(newBreakpoints));
  }
  return breakpoints;
}

Response InspectorDOMDebuggerAgent::setBreakpoint(const String& eventName,
                                                  const String& targetName) {
  if (eventName.isEmpty())
    return Response::Error("Event name is empty");
  protocol::DictionaryValue* breakpointsByTarget =
      ensurePropertyObject(eventListenerBreakpoints(), eventName);
  if (targetName.isEmpty())
    breakpointsByTarget->setBoolean(DOMDebuggerAgentState::eventTargetAny,
                                    true);
  else
    breakpointsByTarget->setBoolean(targetName.lower(), true);
  didAddBreakpoint();
  return Response::OK();
}

Response InspectorDOMDebuggerAgent::removeEventListenerBreakpoint(
    const String& eventName,
    Maybe<String> targetName) {
  return removeBreakpoint(String(listenerEventCategoryType) + eventName,
                          targetName.fromMaybe(String()));
}

Response InspectorDOMDebuggerAgent::removeInstrumentationBreakpoint(
    const String& eventName) {
  return removeBreakpoint(String(instrumentationEventCategoryType) + eventName,
                          String());
}

Response InspectorDOMDebuggerAgent::removeBreakpoint(const String& eventName,
                                                     const String& targetName) {
  if (eventName.isEmpty())
    return Response::Error("Event name is empty");
  protocol::DictionaryValue* breakpointsByTarget =
      ensurePropertyObject(eventListenerBreakpoints(), eventName);
  if (targetName.isEmpty())
    breakpointsByTarget->remove(DOMDebuggerAgentState::eventTargetAny);
  else
    breakpointsByTarget->remove(targetName.lower());
  didRemoveBreakpoint();
  return Response::OK();
}

void InspectorDOMDebuggerAgent::didInvalidateStyleAttr(Node* node) {
  if (hasBreakpoint(node, AttributeModified))
    breakProgramOnDOMEvent(node, AttributeModified, false);
}

void InspectorDOMDebuggerAgent::didInsertDOMNode(Node* node) {
  if (m_domBreakpoints.size()) {
    uint32_t mask =
        m_domBreakpoints.get(InspectorDOMAgent::innerParentNode(node));
    uint32_t inheritableTypesMask =
        (mask | (mask >> domBreakpointDerivedTypeShift)) &
        inheritableDOMBreakpointTypesMask;
    if (inheritableTypesMask)
      updateSubtreeBreakpoints(node, inheritableTypesMask, true);
  }
}

void InspectorDOMDebuggerAgent::didRemoveDOMNode(Node* node) {
  if (m_domBreakpoints.size()) {
    // Remove subtree breakpoints.
    m_domBreakpoints.remove(node);
    HeapVector<Member<Node>> stack(1, InspectorDOMAgent::innerFirstChild(node));
    do {
      Node* node = stack.back();
      stack.pop_back();
      if (!node)
        continue;
      m_domBreakpoints.remove(node);
      stack.push_back(InspectorDOMAgent::innerFirstChild(node));
      stack.push_back(InspectorDOMAgent::innerNextSibling(node));
    } while (!stack.isEmpty());
  }
}

static Response domTypeForName(const String& typeString, int& type) {
  if (typeString == "subtree-modified") {
    type = SubtreeModified;
    return Response::OK();
  }
  if (typeString == "attribute-modified") {
    type = AttributeModified;
    return Response::OK();
  }
  if (typeString == "node-removed") {
    type = NodeRemoved;
    return Response::OK();
  }
  return Response::Error(String("Unknown DOM breakpoint type: " + typeString));
}

static String domTypeName(int type) {
  switch (type) {
    case SubtreeModified:
      return "subtree-modified";
    case AttributeModified:
      return "attribute-modified";
    case NodeRemoved:
      return "node-removed";
    default:
      break;
  }
  return "";
}

Response InspectorDOMDebuggerAgent::setDOMBreakpoint(int nodeId,
                                                     const String& typeString) {
  Node* node = nullptr;
  Response response = m_domAgent->assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  int type = -1;
  response = domTypeForName(typeString, type);
  if (!response.isSuccess())
    return response;

  uint32_t rootBit = 1 << type;
  m_domBreakpoints.set(node, m_domBreakpoints.get(node) | rootBit);
  if (rootBit & inheritableDOMBreakpointTypesMask) {
    for (Node* child = InspectorDOMAgent::innerFirstChild(node); child;
         child = InspectorDOMAgent::innerNextSibling(child))
      updateSubtreeBreakpoints(child, rootBit, true);
  }
  didAddBreakpoint();
  return Response::OK();
}

Response InspectorDOMDebuggerAgent::removeDOMBreakpoint(
    int nodeId,
    const String& typeString) {
  Node* node = nullptr;
  Response response = m_domAgent->assertNode(nodeId, node);
  if (!response.isSuccess())
    return response;

  int type = -1;
  response = domTypeForName(typeString, type);
  if (!response.isSuccess())
    return response;

  uint32_t rootBit = 1 << type;
  uint32_t mask = m_domBreakpoints.get(node) & ~rootBit;
  if (mask)
    m_domBreakpoints.set(node, mask);
  else
    m_domBreakpoints.remove(node);

  if ((rootBit & inheritableDOMBreakpointTypesMask) &&
      !(mask & (rootBit << domBreakpointDerivedTypeShift))) {
    for (Node* child = InspectorDOMAgent::innerFirstChild(node); child;
         child = InspectorDOMAgent::innerNextSibling(child))
      updateSubtreeBreakpoints(child, rootBit, false);
  }
  didRemoveBreakpoint();
  return Response::OK();
}

Response InspectorDOMDebuggerAgent::getEventListeners(
    const String& objectId,
    std::unique_ptr<protocol::Array<protocol::DOMDebugger::EventListener>>*
        listenersArray) {
  v8::HandleScope handles(m_isolate);
  v8::Local<v8::Value> object;
  v8::Local<v8::Context> context;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  std::unique_ptr<v8_inspector::StringBuffer> objectGroup;
  if (!m_v8Session->unwrapObject(&error, toV8InspectorStringView(objectId),
                                 &object, &context, &objectGroup)) {
    return Response::Error(toCoreString(std::move(error)));
  }
  v8::Context::Scope scope(context);
  *listenersArray =
      protocol::Array<protocol::DOMDebugger::EventListener>::create();
  V8EventListenerInfoList eventInformation;
  InspectorDOMDebuggerAgent::eventListenersInfoForTarget(
      context->GetIsolate(), object, eventInformation);
  for (const auto& info : eventInformation) {
    if (!info.useCapture)
      continue;
    std::unique_ptr<protocol::DOMDebugger::EventListener> listenerObject =
        buildObjectForEventListener(context, info, objectGroup->string());
    if (listenerObject)
      (*listenersArray)->addItem(std::move(listenerObject));
  }
  for (const auto& info : eventInformation) {
    if (info.useCapture)
      continue;
    std::unique_ptr<protocol::DOMDebugger::EventListener> listenerObject =
        buildObjectForEventListener(context, info, objectGroup->string());
    if (listenerObject)
      (*listenersArray)->addItem(std::move(listenerObject));
  }
  return Response::OK();
}

std::unique_ptr<protocol::DOMDebugger::EventListener>
InspectorDOMDebuggerAgent::buildObjectForEventListener(
    v8::Local<v8::Context> context,
    const V8EventListenerInfo& info,
    const v8_inspector::StringView& objectGroupId) {
  if (info.handler.IsEmpty())
    return nullptr;

  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Function> function =
      eventListenerEffectiveFunction(isolate, info.handler);
  if (function.IsEmpty())
    return nullptr;

  String scriptId;
  int lineNumber;
  int columnNumber;
  getFunctionLocation(function, scriptId, lineNumber, columnNumber);

  std::unique_ptr<protocol::DOMDebugger::EventListener> value =
      protocol::DOMDebugger::EventListener::create()
          .setType(info.eventType)
          .setUseCapture(info.useCapture)
          .setPassive(info.passive)
          .setOnce(info.once)
          .setScriptId(scriptId)
          .setLineNumber(lineNumber)
          .setColumnNumber(columnNumber)
          .build();
  if (objectGroupId.length()) {
    value->setHandler(
        m_v8Session->wrapObject(context, function, objectGroupId));
    value->setOriginalHandler(
        m_v8Session->wrapObject(context, info.handler, objectGroupId));
    v8::Local<v8::Function> removeFunction;
    if (info.removeFunction.ToLocal(&removeFunction))
      value->setRemoveFunction(
          m_v8Session->wrapObject(context, removeFunction, objectGroupId));
  }
  return value;
}

void InspectorDOMDebuggerAgent::allowNativeBreakpoint(
    const String& breakpointName,
    const String* targetName,
    bool sync) {
  pauseOnNativeEventIfNeeded(
      preparePauseOnNativeEventData(breakpointName, targetName), sync);
}

void InspectorDOMDebuggerAgent::willInsertDOMNode(Node* parent) {
  if (hasBreakpoint(parent, SubtreeModified))
    breakProgramOnDOMEvent(parent, SubtreeModified, true);
}

void InspectorDOMDebuggerAgent::willRemoveDOMNode(Node* node) {
  Node* parentNode = InspectorDOMAgent::innerParentNode(node);
  if (hasBreakpoint(node, NodeRemoved))
    breakProgramOnDOMEvent(node, NodeRemoved, false);
  else if (parentNode && hasBreakpoint(parentNode, SubtreeModified))
    breakProgramOnDOMEvent(node, SubtreeModified, false);
  didRemoveDOMNode(node);
}

void InspectorDOMDebuggerAgent::willModifyDOMAttr(Element* element,
                                                  const AtomicString&,
                                                  const AtomicString&) {
  if (hasBreakpoint(element, AttributeModified))
    breakProgramOnDOMEvent(element, AttributeModified, false);
}

void InspectorDOMDebuggerAgent::breakProgramOnDOMEvent(Node* target,
                                                       int breakpointType,
                                                       bool insertion) {
  DCHECK(hasBreakpoint(target, breakpointType));
  std::unique_ptr<protocol::DictionaryValue> description =
      protocol::DictionaryValue::create();

  Node* breakpointOwner = target;
  if ((1 << breakpointType) & inheritableDOMBreakpointTypesMask) {
    // For inheritable breakpoint types, target node isn't always the same as
    // the node that owns a breakpoint.  Target node may be unknown to frontend,
    // so we need to push it first.
    description->setInteger("targetNodeId",
                            m_domAgent->pushNodePathToFrontend(target));

    // Find breakpoint owner node.
    if (!insertion)
      breakpointOwner = InspectorDOMAgent::innerParentNode(target);
    ASSERT(breakpointOwner);
    while (!(m_domBreakpoints.get(breakpointOwner) & (1 << breakpointType))) {
      Node* parentNode = InspectorDOMAgent::innerParentNode(breakpointOwner);
      if (!parentNode)
        break;
      breakpointOwner = parentNode;
    }

    if (breakpointType == SubtreeModified)
      description->setBoolean("insertion", insertion);
  }

  int breakpointOwnerNodeId = m_domAgent->boundNodeId(breakpointOwner);
  ASSERT(breakpointOwnerNodeId);
  description->setInteger("nodeId", breakpointOwnerNodeId);
  description->setString("type", domTypeName(breakpointType));
  String json = description->serialize();
  m_v8Session->breakProgram(
      toV8InspectorStringView(
          v8_inspector::protocol::Debugger::API::Paused::ReasonEnum::DOM),
      toV8InspectorStringView(json));
}

bool InspectorDOMDebuggerAgent::hasBreakpoint(Node* node, int type) {
  if (!m_domAgent->enabled())
    return false;
  uint32_t rootBit = 1 << type;
  uint32_t derivedBit = rootBit << domBreakpointDerivedTypeShift;
  return m_domBreakpoints.get(node) & (rootBit | derivedBit);
}

void InspectorDOMDebuggerAgent::updateSubtreeBreakpoints(Node* node,
                                                         uint32_t rootMask,
                                                         bool set) {
  uint32_t oldMask = m_domBreakpoints.get(node);
  uint32_t derivedMask = rootMask << domBreakpointDerivedTypeShift;
  uint32_t newMask = set ? oldMask | derivedMask : oldMask & ~derivedMask;
  if (newMask)
    m_domBreakpoints.set(node, newMask);
  else
    m_domBreakpoints.remove(node);

  uint32_t newRootMask = rootMask & ~newMask;
  if (!newRootMask)
    return;

  for (Node* child = InspectorDOMAgent::innerFirstChild(node); child;
       child = InspectorDOMAgent::innerNextSibling(child))
    updateSubtreeBreakpoints(child, newRootMask, set);
}

void InspectorDOMDebuggerAgent::pauseOnNativeEventIfNeeded(
    std::unique_ptr<protocol::DictionaryValue> eventData,
    bool synchronous) {
  if (!eventData)
    return;
  String json = eventData->serialize();
  if (synchronous)
    m_v8Session->breakProgram(
        toV8InspectorStringView(v8_inspector::protocol::Debugger::API::Paused::
                                    ReasonEnum::EventListener),
        toV8InspectorStringView(json));
  else
    m_v8Session->schedulePauseOnNextStatement(
        toV8InspectorStringView(v8_inspector::protocol::Debugger::API::Paused::
                                    ReasonEnum::EventListener),
        toV8InspectorStringView(json));
}

std::unique_ptr<protocol::DictionaryValue>
InspectorDOMDebuggerAgent::preparePauseOnNativeEventData(
    const String& eventName,
    const String* targetName) {
  String fullEventName = (targetName ? listenerEventCategoryType
                                     : instrumentationEventCategoryType) +
                         eventName;
  protocol::DictionaryValue* breakpoints = eventListenerBreakpoints();
  protocol::Value* value = breakpoints->get(fullEventName);
  if (!value)
    return nullptr;
  bool match = false;
  protocol::DictionaryValue* breakpointsByTarget =
      protocol::DictionaryValue::cast(value);
  breakpointsByTarget->getBoolean(DOMDebuggerAgentState::eventTargetAny,
                                  &match);
  if (!match && targetName)
    breakpointsByTarget->getBoolean(targetName->lower(), &match);
  if (!match)
    return nullptr;

  std::unique_ptr<protocol::DictionaryValue> eventData =
      protocol::DictionaryValue::create();
  eventData->setString("eventName", fullEventName);
  if (targetName)
    eventData->setString("targetName", *targetName);
  return eventData;
}

void InspectorDOMDebuggerAgent::didFireWebGLError(const String& errorName) {
  std::unique_ptr<protocol::DictionaryValue> eventData =
      preparePauseOnNativeEventData(webglErrorFiredEventName, 0);
  if (!eventData)
    return;
  if (!errorName.isEmpty())
    eventData->setString(webglErrorNameProperty, errorName);
  pauseOnNativeEventIfNeeded(std::move(eventData), false);
}

void InspectorDOMDebuggerAgent::didFireWebGLWarning() {
  pauseOnNativeEventIfNeeded(
      preparePauseOnNativeEventData(webglWarningFiredEventName, 0), false);
}

void InspectorDOMDebuggerAgent::didFireWebGLErrorOrWarning(
    const String& message) {
  if (message.findIgnoringCase("error") != WTF::kNotFound)
    didFireWebGLError(String());
  else
    didFireWebGLWarning();
}

void InspectorDOMDebuggerAgent::cancelNativeBreakpoint() {
  m_v8Session->cancelPauseOnNextStatement();
}

void InspectorDOMDebuggerAgent::scriptExecutionBlockedByCSP(
    const String& directiveText) {
  std::unique_ptr<protocol::DictionaryValue> eventData =
      preparePauseOnNativeEventData(scriptBlockedByCSPEventName, 0);
  if (!eventData)
    return;
  eventData->setString("directiveText", directiveText);
  pauseOnNativeEventIfNeeded(std::move(eventData), true);
}

Response InspectorDOMDebuggerAgent::setXHRBreakpoint(const String& url) {
  if (url.isEmpty())
    m_state->setBoolean(DOMDebuggerAgentState::pauseOnAllXHRs, true);
  else
    xhrBreakpoints()->setBoolean(url, true);
  didAddBreakpoint();
  return Response::OK();
}

Response InspectorDOMDebuggerAgent::removeXHRBreakpoint(const String& url) {
  if (url.isEmpty())
    m_state->setBoolean(DOMDebuggerAgentState::pauseOnAllXHRs, false);
  else
    xhrBreakpoints()->remove(url);
  didRemoveBreakpoint();
  return Response::OK();
}

void InspectorDOMDebuggerAgent::willSendXMLHttpOrFetchNetworkRequest(
    const String& url) {
  String breakpointURL;
  if (m_state->booleanProperty(DOMDebuggerAgentState::pauseOnAllXHRs, false))
    breakpointURL = "";
  else {
    protocol::DictionaryValue* breakpoints = xhrBreakpoints();
    for (size_t i = 0; i < breakpoints->size(); ++i) {
      auto breakpoint = breakpoints->at(i);
      if (url.contains(breakpoint.first)) {
        breakpointURL = breakpoint.first;
        break;
      }
    }
  }

  if (breakpointURL.isNull())
    return;

  std::unique_ptr<protocol::DictionaryValue> eventData =
      protocol::DictionaryValue::create();
  eventData->setString("breakpointURL", breakpointURL);
  eventData->setString("url", url);
  String json = eventData->serialize();
  m_v8Session->breakProgram(
      toV8InspectorStringView(
          v8_inspector::protocol::Debugger::API::Paused::ReasonEnum::XHR),
      toV8InspectorStringView(json));
}

void InspectorDOMDebuggerAgent::didAddBreakpoint() {
  if (m_state->booleanProperty(DOMDebuggerAgentState::enabled, false))
    return;
  setEnabled(true);
}

void InspectorDOMDebuggerAgent::didRemoveBreakpoint() {
  if (!m_domBreakpoints.isEmpty())
    return;
  if (eventListenerBreakpoints()->size())
    return;
  if (xhrBreakpoints()->size())
    return;
  if (m_state->booleanProperty(DOMDebuggerAgentState::pauseOnAllXHRs, false))
    return;
  setEnabled(false);
}

void InspectorDOMDebuggerAgent::setEnabled(bool enabled) {
  if (enabled) {
    m_instrumentingAgents->addInspectorDOMDebuggerAgent(this);
    m_state->setBoolean(DOMDebuggerAgentState::enabled, true);
  } else {
    m_state->remove(DOMDebuggerAgentState::enabled);
    m_instrumentingAgents->removeInspectorDOMDebuggerAgent(this);
  }
}

void InspectorDOMDebuggerAgent::didCommitLoadForLocalFrame(LocalFrame*) {
  m_domBreakpoints.clear();
}

}  // namespace blink
