// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/FetchEvent.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"

namespace blink {

FetchEvent* FetchEvent::create(ScriptState* scriptState,
                               const AtomicString& type,
                               const FetchEventInit& initializer) {
  return new FetchEvent(scriptState, type, initializer, nullptr, nullptr,
                        false);
}

FetchEvent* FetchEvent::create(ScriptState* scriptState,
                               const AtomicString& type,
                               const FetchEventInit& initializer,
                               RespondWithObserver* respondWithObserver,
                               WaitUntilObserver* waitUntilObserver,
                               bool navigationPreloadSent) {
  return new FetchEvent(scriptState, type, initializer, respondWithObserver,
                        waitUntilObserver, navigationPreloadSent);
}

Request* FetchEvent::request() const {
  return m_request;
}

String FetchEvent::clientId() const {
  return m_clientId;
}

bool FetchEvent::isReload() const {
  return m_isReload;
}

void FetchEvent::respondWith(ScriptState* scriptState,
                             ScriptPromise scriptPromise,
                             ExceptionState& exceptionState) {
  stopImmediatePropagation();
  if (m_observer)
    m_observer->respondWith(scriptState, scriptPromise, exceptionState);
}

ScriptPromise FetchEvent::preloadResponse(ScriptState* scriptState) {
  return m_preloadResponseProperty->promise(scriptState->world());
}

const AtomicString& FetchEvent::interfaceName() const {
  return EventNames::FetchEvent;
}

FetchEvent::FetchEvent(ScriptState* scriptState,
                       const AtomicString& type,
                       const FetchEventInit& initializer,
                       RespondWithObserver* respondWithObserver,
                       WaitUntilObserver* waitUntilObserver,
                       bool navigationPreloadSent)
    : ExtendableEvent(type, initializer, waitUntilObserver),
      m_observer(respondWithObserver),
      m_preloadResponseProperty(new PreloadResponseProperty(
          scriptState->getExecutionContext(),
          this,
          PreloadResponseProperty::PreloadResponse)) {
  if (!navigationPreloadSent)
    m_preloadResponseProperty->resolveWithUndefined();

  m_clientId = initializer.clientId();
  m_isReload = initializer.isReload();
  if (initializer.hasRequest()) {
    ScriptState::Scope scope(scriptState);
    m_request = initializer.request();
    v8::Local<v8::Value> request = ToV8(m_request, scriptState);
    v8::Local<v8::Value> event = ToV8(this, scriptState);
    if (event.IsEmpty()) {
      // |toV8| can return an empty handle when the worker is terminating.
      // We don't want the renderer to crash in such cases.
      // TODO(yhirano): Replace this branch with an assertion when the
      // graceful shutdown mechanism is introduced.
      return;
    }
    DCHECK(event->IsObject());
    // Sets a hidden value in order to teach V8 the dependency from
    // the event to the request.
    V8HiddenValue::setHiddenValue(
        scriptState, event.As<v8::Object>(),
        V8HiddenValue::requestInFetchEvent(scriptState->isolate()), request);
    // From the same reason as above, setHiddenValue can return false.
    // TODO(yhirano): Add an assertion that it returns true once the
    // graceful shutdown mechanism is introduced.
  }
}

void FetchEvent::onNavigationPreloadResponse(
    ScriptState* scriptState,
    std::unique_ptr<WebURLResponse> response,
    std::unique_ptr<WebDataConsumerHandle> dataConsumeHandle) {
  if (!scriptState->contextIsValid())
    return;
  DCHECK(m_preloadResponseProperty);
  ScriptState::Scope scope(scriptState);
  FetchResponseData* responseData = FetchResponseData::createWithBuffer(
      new BodyStreamBuffer(scriptState, new BytesConsumerForDataConsumerHandle(
                                            scriptState->getExecutionContext(),
                                            std::move(dataConsumeHandle))));
  Vector<KURL> urlList(1);
  urlList[0] = response->url();
  responseData->setURLList(urlList);
  responseData->setStatus(response->httpStatusCode());
  responseData->setStatusMessage(response->httpStatusText());
  responseData->setResponseTime(response->toResourceResponse().responseTime());
  const HTTPHeaderMap& headers(
      response->toResourceResponse().httpHeaderFields());
  for (const auto& header : headers) {
    responseData->headerList()->append(header.key, header.value);
  }
  FetchResponseData* taintedResponse =
      responseData->createBasicFilteredResponse();
  m_preloadResponseProperty->resolve(
      Response::create(scriptState->getExecutionContext(), taintedResponse));
}

void FetchEvent::onNavigationPreloadError(
    ScriptState* scriptState,
    std::unique_ptr<WebServiceWorkerError> error) {
  if (!scriptState->contextIsValid())
    return;
  DCHECK(m_preloadResponseProperty);
  m_preloadResponseProperty->reject(
      ServiceWorkerError::take(nullptr, *error.get()));
}

DEFINE_TRACE(FetchEvent) {
  visitor->trace(m_observer);
  visitor->trace(m_request);
  visitor->trace(m_preloadResponseProperty);
  ExtendableEvent::trace(visitor);
}

}  // namespace blink
