/*
 * Copyright (C) 2009, 2012 Ericsson AB. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "modules/eventsource/EventSource.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/ThreadableLoader.h"
#include "modules/eventsource/EventSourceInit.h"
#include "platform/HTTPNames.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

const unsigned long long EventSource::defaultReconnectDelay = 3000;

inline EventSource::EventSource(ExecutionContext* context,
                                const KURL& url,
                                const EventSourceInit& eventSourceInit)
    : ContextLifecycleObserver(context),
      m_url(url),
      m_currentURL(url),
      m_withCredentials(eventSourceInit.withCredentials()),
      m_state(kConnecting),
      m_connectTimer(TaskRunnerHelper::get(TaskType::RemoteEvent, context),
                     this,
                     &EventSource::connectTimerFired),
      m_reconnectDelay(defaultReconnectDelay) {}

EventSource* EventSource::create(ExecutionContext* context,
                                 const String& url,
                                 const EventSourceInit& eventSourceInit,
                                 ExceptionState& exceptionState) {
  if (context->isDocument())
    UseCounter::count(toDocument(context), UseCounter::EventSourceDocument);
  else
    UseCounter::count(context, UseCounter::EventSourceWorker);

  if (url.isEmpty()) {
    exceptionState.throwDOMException(
        SyntaxError, "Cannot open an EventSource to an empty URL.");
    return nullptr;
  }

  KURL fullURL = context->completeURL(url);
  if (!fullURL.isValid()) {
    exceptionState.throwDOMException(
        SyntaxError,
        "Cannot open an EventSource to '" + url + "'. The URL is invalid.");
    return nullptr;
  }

  // FIXME: Convert this to check the isolated world's Content Security Policy
  // once webkit.org/b/104520 is solved.
  if (!ContentSecurityPolicy::shouldBypassMainWorld(context) &&
      !context->contentSecurityPolicy()->allowConnectToSource(fullURL)) {
    // We can safely expose the URL to JavaScript, as this exception is generate
    // synchronously before any redirects take place.
    exceptionState.throwSecurityError(
        "Refused to connect to '" + fullURL.elidedString() +
        "' because it violates the document's Content Security Policy.");
    return nullptr;
  }

  EventSource* source = new EventSource(context, fullURL, eventSourceInit);

  source->scheduleInitialConnect();
  return source;
}

EventSource::~EventSource() {
  DCHECK_EQ(kClosed, m_state);
  DCHECK(!m_loader);
}

void EventSource::dispose() {
  probe::detachClientRequest(getExecutionContext(), this);
}

void EventSource::scheduleInitialConnect() {
  DCHECK_EQ(kConnecting, m_state);
  DCHECK(!m_loader);

  m_connectTimer.startOneShot(0, BLINK_FROM_HERE);
}

void EventSource::connect() {
  DCHECK_EQ(kConnecting, m_state);
  DCHECK(!m_loader);
  DCHECK(getExecutionContext());

  ExecutionContext& executionContext = *this->getExecutionContext();
  ResourceRequest request(m_currentURL);
  request.setHTTPMethod(HTTPNames::GET);
  request.setHTTPHeaderField(HTTPNames::Accept, "text/event-stream");
  request.setHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
  request.setRequestContext(WebURLRequest::RequestContextEventSource);
  request.setExternalRequestStateFromRequestorAddressSpace(
      executionContext.securityContext().addressSpace());
  if (m_parser && !m_parser->lastEventId().isEmpty()) {
    // HTTP headers are Latin-1 byte strings, but the Last-Event-ID header is
    // encoded as UTF-8.
    // TODO(davidben): This should be captured in the type of
    // setHTTPHeaderField's arguments.
    CString lastEventIdUtf8 = m_parser->lastEventId().utf8();
    request.setHTTPHeaderField(
        HTTPNames::Last_Event_ID,
        AtomicString(reinterpret_cast<const LChar*>(lastEventIdUtf8.data()),
                     lastEventIdUtf8.length()));
  }

  SecurityOrigin* origin = executionContext.getSecurityOrigin();

  ThreadableLoaderOptions options;
  options.preflightPolicy = PreventPreflight;
  options.crossOriginRequestPolicy = UseAccessControl;
  options.contentSecurityPolicyEnforcement =
      ContentSecurityPolicy::shouldBypassMainWorld(&executionContext)
          ? DoNotEnforceContentSecurityPolicy
          : EnforceContentSecurityPolicy;

  ResourceLoaderOptions resourceLoaderOptions;
  resourceLoaderOptions.allowCredentials =
      (origin->canRequestNoSuborigin(m_currentURL) || m_withCredentials)
          ? AllowStoredCredentials
          : DoNotAllowStoredCredentials;
  resourceLoaderOptions.credentialsRequested =
      m_withCredentials ? ClientRequestedCredentials
                        : ClientDidNotRequestCredentials;
  resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
  resourceLoaderOptions.securityOrigin = origin;

  probe::willSendEventSourceRequest(&executionContext, this);
  // probe::documentThreadableLoaderStartedLoadingForClient
  // will be called synchronously.
  m_loader = ThreadableLoader::create(executionContext, this, options,
                                      resourceLoaderOptions);
  m_loader->start(request);
}

void EventSource::networkRequestEnded() {
  probe::didFinishEventSourceRequest(getExecutionContext(), this);

  m_loader = nullptr;

  if (m_state != kClosed)
    scheduleReconnect();
}

void EventSource::scheduleReconnect() {
  m_state = kConnecting;
  m_connectTimer.startOneShot(m_reconnectDelay / 1000.0, BLINK_FROM_HERE);
  dispatchEvent(Event::create(EventTypeNames::error));
}

void EventSource::connectTimerFired(TimerBase*) {
  connect();
}

String EventSource::url() const {
  return m_url.getString();
}

bool EventSource::withCredentials() const {
  return m_withCredentials;
}

EventSource::State EventSource::readyState() const {
  return m_state;
}

void EventSource::close() {
  if (m_state == kClosed) {
    DCHECK(!m_loader);
    return;
  }
  if (m_parser)
    m_parser->stop();

  // Stop trying to reconnect if EventSource was explicitly closed or if
  // contextDestroyed() was called.
  if (m_connectTimer.isActive()) {
    m_connectTimer.stop();
  }

  if (m_loader) {
    m_loader->cancel();
    m_loader = nullptr;
  }

  m_state = kClosed;
}

const AtomicString& EventSource::interfaceName() const {
  return EventTargetNames::EventSource;
}

ExecutionContext* EventSource::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

void EventSource::didReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  DCHECK_EQ(kConnecting, m_state);
  DCHECK(m_loader);

  m_currentURL = response.url();
  m_eventStreamOrigin = SecurityOrigin::create(response.url())->toString();
  int statusCode = response.httpStatusCode();
  bool mimeTypeIsValid = response.mimeType() == "text/event-stream";
  bool responseIsValid = statusCode == 200 && mimeTypeIsValid;
  if (responseIsValid) {
    const String& charset = response.textEncodingName();
    // If we have a charset, the only allowed value is UTF-8 (case-insensitive).
    responseIsValid = charset.isEmpty() || equalIgnoringCase(charset, "UTF-8");
    if (!responseIsValid) {
      StringBuilder message;
      message.append("EventSource's response has a charset (\"");
      message.append(charset);
      message.append("\") that is not UTF-8. Aborting the connection.");
      // FIXME: We are missing the source line.
      getExecutionContext()->addConsoleMessage(ConsoleMessage::create(
          JSMessageSource, ErrorMessageLevel, message.toString()));
    }
  } else {
    // To keep the signal-to-noise ratio low, we only log 200-response with an
    // invalid MIME type.
    if (statusCode == 200 && !mimeTypeIsValid) {
      StringBuilder message;
      message.append("EventSource's response has a MIME type (\"");
      message.append(response.mimeType());
      message.append(
          "\") that is not \"text/event-stream\". Aborting the connection.");
      // FIXME: We are missing the source line.
      getExecutionContext()->addConsoleMessage(ConsoleMessage::create(
          JSMessageSource, ErrorMessageLevel, message.toString()));
    }
  }

  if (responseIsValid) {
    m_state = kOpen;
    AtomicString lastEventId;
    if (m_parser) {
      // The new parser takes over the event ID.
      lastEventId = m_parser->lastEventId();
    }
    m_parser = new EventSourceParser(lastEventId, this);
    dispatchEvent(Event::create(EventTypeNames::open));
  } else {
    m_loader->cancel();
    dispatchEvent(Event::create(EventTypeNames::error));
  }
}

void EventSource::didReceiveData(const char* data, unsigned length) {
  DCHECK_EQ(kOpen, m_state);
  DCHECK(m_loader);
  DCHECK(m_parser);

  m_parser->addBytes(data, length);
}

void EventSource::didFinishLoading(unsigned long, double) {
  DCHECK_EQ(kOpen, m_state);
  DCHECK(m_loader);

  networkRequestEnded();
}

void EventSource::didFail(const ResourceError& error) {
  DCHECK_NE(kClosed, m_state);
  DCHECK(m_loader);

  if (error.isCancellation())
    m_state = kClosed;
  networkRequestEnded();
}

void EventSource::didFailAccessControlCheck(const ResourceError& error) {
  DCHECK(m_loader);

  String message = "EventSource cannot load " + error.failingURL() + ". " +
                   error.localizedDescription();
  getExecutionContext()->addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));

  abortConnectionAttempt();
}

void EventSource::didFailRedirectCheck() {
  DCHECK(m_loader);

  abortConnectionAttempt();
}

void EventSource::onMessageEvent(const AtomicString& eventType,
                                 const String& data,
                                 const AtomicString& lastEventId) {
  MessageEvent* e = MessageEvent::create();
  e->initMessageEvent(eventType, false, false, data, m_eventStreamOrigin,
                      lastEventId, 0, nullptr);

  probe::willDispatchEventSourceEvent(getExecutionContext(), this, eventType,
                                      lastEventId, data);
  dispatchEvent(e);
}

void EventSource::onReconnectionTimeSet(unsigned long long reconnectionTime) {
  m_reconnectDelay = reconnectionTime;
}

void EventSource::abortConnectionAttempt() {
  DCHECK_EQ(kConnecting, m_state);

  m_loader = nullptr;
  m_state = kClosed;
  networkRequestEnded();

  dispatchEvent(Event::create(EventTypeNames::error));
}

void EventSource::contextDestroyed(ExecutionContext*) {
  probe::detachClientRequest(getExecutionContext(), this);
  close();
}

bool EventSource::hasPendingActivity() const {
  return m_state != kClosed;
}

DEFINE_TRACE(EventSource) {
  visitor->trace(m_parser);
  visitor->trace(m_loader);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  EventSourceParser::Client::trace(visitor);
}

}  // namespace blink
