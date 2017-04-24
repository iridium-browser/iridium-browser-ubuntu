// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/RespondWithObserver.h"

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/modules/v8/V8Response.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/streams/Stream.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/BytesConsumer.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "v8/include/v8.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"

namespace blink {
namespace {

// Returns the error message to let the developer know about the reason of the
// unusual failures.
const String getMessageForResponseError(WebServiceWorkerResponseError error,
                                        const KURL& requestURL) {
  String errorMessage = "The FetchEvent for \"" + requestURL.getString() +
                        "\" resulted in a network error response: ";
  switch (error) {
    case WebServiceWorkerResponseErrorPromiseRejected:
      errorMessage = errorMessage + "the promise was rejected.";
      break;
    case WebServiceWorkerResponseErrorDefaultPrevented:
      errorMessage =
          errorMessage +
          "preventDefault() was called without calling respondWith().";
      break;
    case WebServiceWorkerResponseErrorNoV8Instance:
      errorMessage =
          errorMessage +
          "an object that was not a Response was passed to respondWith().";
      break;
    case WebServiceWorkerResponseErrorResponseTypeError:
      errorMessage = errorMessage +
                     "the promise was resolved with an error response object.";
      break;
    case WebServiceWorkerResponseErrorResponseTypeOpaque:
      errorMessage = errorMessage +
                     "an \"opaque\" response was used for a request whose type "
                     "is not no-cors";
      break;
    case WebServiceWorkerResponseErrorResponseTypeNotBasicOrDefault:
      ASSERT_NOT_REACHED();
      break;
    case WebServiceWorkerResponseErrorBodyUsed:
      errorMessage = errorMessage +
                     "a Response whose \"bodyUsed\" is \"true\" cannot be used "
                     "to respond to a request.";
      break;
    case WebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest:
      errorMessage = errorMessage +
                     "an \"opaque\" response was used for a client request.";
      break;
    case WebServiceWorkerResponseErrorResponseTypeOpaqueRedirect:
      errorMessage = errorMessage +
                     "an \"opaqueredirect\" type response was used for a "
                     "request whose redirect mode is not \"manual\".";
      break;
    case WebServiceWorkerResponseErrorBodyLocked:
      errorMessage = errorMessage +
                     "a Response whose \"body\" is locked cannot be used to "
                     "respond to a request.";
      break;
    case WebServiceWorkerResponseErrorNoForeignFetchResponse:
      errorMessage = errorMessage +
                     "an object that was not a ForeignFetchResponse was passed "
                     "to respondWith().";
      break;
    case WebServiceWorkerResponseErrorForeignFetchHeadersWithoutOrigin:
      errorMessage =
          errorMessage +
          "headers were specified for a response without an explicit origin.";
      break;
    case WebServiceWorkerResponseErrorForeignFetchMismatchedOrigin:
      errorMessage =
          errorMessage + "origin in response does not match origin of request.";
      break;
    case WebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest:
      errorMessage = errorMessage +
                     "a redirected response was used for a request whose "
                     "redirect mode is not \"follow\".";
      break;
    case WebServiceWorkerResponseErrorUnknown:
    default:
      errorMessage = errorMessage + "an unexpected error occurred.";
      break;
  }
  return errorMessage;
}

const String getErrorMessageForRedirectedResponseForNavigationRequest(
    const KURL& requestURL,
    const Vector<KURL>& responseURLList) {
  String errorMessage =
      "In Chrome 59, the navigation to \"" + requestURL.getString() + "\" " +
      "will result in a network error, because FetchEvent.respondWith() was " +
      "called with a redirected response. See https://crbug.com/658249. The " +
      "url list of the response was: [\"" + responseURLList[0].getString() +
      "\"";
  for (size_t i = 1; i < responseURLList.size(); ++i) {
    errorMessage =
        errorMessage + ", \"" + responseURLList[i].getString() + "\"";
  }
  return errorMessage + "]";
}

bool isNavigationRequest(WebURLRequest::FrameType frameType) {
  return frameType != WebURLRequest::FrameTypeNone;
}

bool isClientRequest(WebURLRequest::FrameType frameType,
                     WebURLRequest::RequestContext requestContext) {
  return isNavigationRequest(frameType) ||
         requestContext == WebURLRequest::RequestContextSharedWorker ||
         requestContext == WebURLRequest::RequestContextWorker;
}

class NoopLoaderClient final
    : public GarbageCollectedFinalized<NoopLoaderClient>,
      public FetchDataLoader::Client {
  WTF_MAKE_NONCOPYABLE(NoopLoaderClient);
  USING_GARBAGE_COLLECTED_MIXIN(NoopLoaderClient);

 public:
  NoopLoaderClient() = default;
  void didFetchDataLoadedStream() override {}
  void didFetchDataLoadFailed() override {}
  DEFINE_INLINE_TRACE() { FetchDataLoader::Client::trace(visitor); }
};

}  // namespace

class RespondWithObserver::ThenFunction final : public ScriptFunction {
 public:
  enum ResolveType {
    Fulfilled,
    Rejected,
  };

  static v8::Local<v8::Function> createFunction(ScriptState* scriptState,
                                                RespondWithObserver* observer,
                                                ResolveType type) {
    ThenFunction* self = new ThenFunction(scriptState, observer, type);
    return self->bindToV8Function();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_observer);
    ScriptFunction::trace(visitor);
  }

 private:
  ThenFunction(ScriptState* scriptState,
               RespondWithObserver* observer,
               ResolveType type)
      : ScriptFunction(scriptState),
        m_observer(observer),
        m_resolveType(type) {}

  ScriptValue call(ScriptValue value) override {
    ASSERT(m_observer);
    ASSERT(m_resolveType == Fulfilled || m_resolveType == Rejected);
    if (m_resolveType == Rejected) {
      m_observer->responseWasRejected(
          WebServiceWorkerResponseErrorPromiseRejected);
      value =
          ScriptPromise::reject(value.getScriptState(), value).getScriptValue();
    } else {
      m_observer->responseWasFulfilled(value);
    }
    m_observer = nullptr;
    return value;
  }

  Member<RespondWithObserver> m_observer;
  ResolveType m_resolveType;
};

RespondWithObserver::~RespondWithObserver() {}

RespondWithObserver* RespondWithObserver::create(
    ExecutionContext* context,
    int fetchEventID,
    const KURL& requestURL,
    WebURLRequest::FetchRequestMode requestMode,
    WebURLRequest::FetchRedirectMode redirectMode,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext,
    WaitUntilObserver* observer) {
  return new RespondWithObserver(context, fetchEventID, requestURL, requestMode,
                                 redirectMode, frameType, requestContext,
                                 observer);
}

void RespondWithObserver::contextDestroyed(ExecutionContext*) {
  if (m_observer) {
    DCHECK_EQ(Pending, m_state);
    m_observer.clear();
  }
  m_state = Done;
}

void RespondWithObserver::willDispatchEvent() {
  m_eventDispatchTime = WTF::currentTime();
}

void RespondWithObserver::didDispatchEvent(DispatchEventResult dispatchResult) {
  ASSERT(getExecutionContext());
  if (m_state != Initial)
    return;

  if (dispatchResult != DispatchEventResult::NotCanceled) {
    m_observer->incrementPendingActivity();
    responseWasRejected(WebServiceWorkerResponseErrorDefaultPrevented);
    return;
  }

  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToFetchEvent(m_fetchEventID, m_eventDispatchTime);
  m_state = Done;
  m_observer.clear();
}

void RespondWithObserver::respondWith(ScriptState* scriptState,
                                      ScriptPromise scriptPromise,
                                      ExceptionState& exceptionState) {
  if (m_state != Initial) {
    exceptionState.throwDOMException(
        InvalidStateError, "The fetch event has already been responded to.");
    return;
  }

  m_state = Pending;
  m_observer->incrementPendingActivity();
  scriptPromise.then(
      ThenFunction::createFunction(scriptState, this, ThenFunction::Fulfilled),
      ThenFunction::createFunction(scriptState, this, ThenFunction::Rejected));
}

void RespondWithObserver::responseWasRejected(
    WebServiceWorkerResponseError error) {
  ASSERT(getExecutionContext());
  getExecutionContext()->addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
                             getMessageForResponseError(error, m_requestURL)));

  // The default value of WebServiceWorkerResponse's status is 0, which maps
  // to a network error.
  WebServiceWorkerResponse webResponse;
  webResponse.setError(error);
  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToFetchEvent(m_fetchEventID, webResponse, m_eventDispatchTime);
  m_state = Done;
  m_observer->decrementPendingActivity();
  m_observer.clear();
}

void RespondWithObserver::responseWasFulfilled(const ScriptValue& value) {
  ASSERT(getExecutionContext());
  if (!V8Response::hasInstance(value.v8Value(),
                               toIsolate(getExecutionContext()))) {
    responseWasRejected(WebServiceWorkerResponseErrorNoV8Instance);
    return;
  }
  Response* response = V8Response::toImplWithTypeCheck(
      toIsolate(getExecutionContext()), value.v8Value());
  // "If one of the following conditions is true, return a network error:
  //   - |response|'s type is |error|.
  //   - |request|'s mode is not |no-cors| and response's type is |opaque|.
  //   - |request| is a client request and |response|'s type is neither
  //     |basic| nor |default|."
  const FetchResponseData::Type responseType = response->response()->getType();
  if (responseType == FetchResponseData::ErrorType) {
    responseWasRejected(WebServiceWorkerResponseErrorResponseTypeError);
    return;
  }
  if (responseType == FetchResponseData::OpaqueType) {
    if (m_requestMode != WebURLRequest::FetchRequestModeNoCORS) {
      responseWasRejected(WebServiceWorkerResponseErrorResponseTypeOpaque);
      return;
    }

    // The request mode of client requests should be "same-origin" but it is
    // not explicitly stated in the spec yet. So we need to check here.
    // FIXME: Set the request mode of client requests to "same-origin" and
    // remove this check when the spec will be updated.
    // Spec issue: https://github.com/whatwg/fetch/issues/101
    if (isClientRequest(m_frameType, m_requestContext)) {
      responseWasRejected(
          WebServiceWorkerResponseErrorResponseTypeOpaqueForClientRequest);
      return;
    }
  }
  if (m_redirectMode != WebURLRequest::FetchRedirectModeManual &&
      responseType == FetchResponseData::OpaqueRedirectType) {
    responseWasRejected(
        WebServiceWorkerResponseErrorResponseTypeOpaqueRedirect);
    return;
  }
  if (m_redirectMode != WebURLRequest::FetchRedirectModeFollow &&
      response->redirected()) {
    if (!isNavigationRequest(m_frameType)) {
      responseWasRejected(
          WebServiceWorkerResponseErrorRedirectedResponseForNotFollowRequest);
      return;
    }
    // TODO(horo): We should just reject even if the request was a navigation.
    // Currently we measure the impact of the restriction with the use counter
    // in DocumentLoader.
    getExecutionContext()->addConsoleMessage(ConsoleMessage::create(
        JSMessageSource, ErrorMessageLevel,
        getErrorMessageForRedirectedResponseForNavigationRequest(
            m_requestURL, response->internalURLList())));
  }
  if (response->isBodyLocked()) {
    responseWasRejected(WebServiceWorkerResponseErrorBodyLocked);
    return;
  }
  if (response->bodyUsed()) {
    responseWasRejected(WebServiceWorkerResponseErrorBodyUsed);
    return;
  }

  WebServiceWorkerResponse webResponse;
  response->populateWebServiceWorkerResponse(webResponse);
  BodyStreamBuffer* buffer = response->internalBodyBuffer();
  if (buffer) {
    RefPtr<BlobDataHandle> blobDataHandle = buffer->drainAsBlobDataHandle(
        BytesConsumer::BlobSizePolicy::AllowBlobWithInvalidSize);
    if (blobDataHandle) {
      webResponse.setBlobDataHandle(blobDataHandle);
    } else {
      Stream* outStream = Stream::create(getExecutionContext(), "");
      webResponse.setStreamURL(outStream->url());
      buffer->startLoading(FetchDataLoader::createLoaderAsStream(outStream),
                           new NoopLoaderClient);
    }
  }
  ServiceWorkerGlobalScopeClient::from(getExecutionContext())
      ->respondToFetchEvent(m_fetchEventID, webResponse, m_eventDispatchTime);
  m_state = Done;
  m_observer->decrementPendingActivity();
  m_observer.clear();
}

RespondWithObserver::RespondWithObserver(
    ExecutionContext* context,
    int fetchEventID,
    const KURL& requestURL,
    WebURLRequest::FetchRequestMode requestMode,
    WebURLRequest::FetchRedirectMode redirectMode,
    WebURLRequest::FrameType frameType,
    WebURLRequest::RequestContext requestContext,
    WaitUntilObserver* observer)
    : ContextLifecycleObserver(context),
      m_fetchEventID(fetchEventID),
      m_requestURL(requestURL),
      m_requestMode(requestMode),
      m_redirectMode(redirectMode),
      m_frameType(frameType),
      m_requestContext(requestContext),
      m_state(Initial),
      m_observer(observer) {}

DEFINE_TRACE(RespondWithObserver) {
  visitor->trace(m_observer);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
