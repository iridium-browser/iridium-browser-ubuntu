// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RespondWithObserver_h
#define RespondWithObserver_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/WaitUntilObserver.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseError.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptState;
class ScriptValue;

// This class observes the service worker's handling of a FetchEvent and
// notifies the client.
class MODULES_EXPORT RespondWithObserver
    : public GarbageCollectedFinalized<RespondWithObserver>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(RespondWithObserver);

 public:
  virtual ~RespondWithObserver();

  static RespondWithObserver* create(ExecutionContext*,
                                     int fetchEventID,
                                     const KURL& requestURL,
                                     WebURLRequest::FetchRequestMode,
                                     WebURLRequest::FetchRedirectMode,
                                     WebURLRequest::FrameType,
                                     WebURLRequest::RequestContext,
                                     WaitUntilObserver*);

  void contextDestroyed(ExecutionContext*) override;

  void willDispatchEvent();
  void didDispatchEvent(DispatchEventResult dispatchResult);

  // Observes the promise and delays calling didHandleFetchEvent() until the
  // given promise is resolved or rejected.
  void respondWith(ScriptState*, ScriptPromise, ExceptionState&);

  void responseWasRejected(WebServiceWorkerResponseError);
  virtual void responseWasFulfilled(const ScriptValue&);

  DECLARE_VIRTUAL_TRACE();

 protected:
  RespondWithObserver(ExecutionContext*,
                      int fetchEventID,
                      const KURL& requestURL,
                      WebURLRequest::FetchRequestMode,
                      WebURLRequest::FetchRedirectMode,
                      WebURLRequest::FrameType,
                      WebURLRequest::RequestContext,
                      WaitUntilObserver*);

 private:
  class ThenFunction;

  const int m_fetchEventID;
  const KURL m_requestURL;
  const WebURLRequest::FetchRequestMode m_requestMode;
  const WebURLRequest::FetchRedirectMode m_redirectMode;
  const WebURLRequest::FrameType m_frameType;
  const WebURLRequest::RequestContext m_requestContext;

  double m_eventDispatchTime = 0;

  enum State { Initial, Pending, Done };
  State m_state;

  // RespondWith should ensure the ExtendableEvent is alive until the promise
  // passed to RespondWith is resolved. The lifecycle of the ExtendableEvent
  // is controlled by WaitUntilObserver, so not only
  // WaitUntilObserver::ThenFunction but RespondWith needs to have a strong
  // reference to the WaitUntilObserver.
  Member<WaitUntilObserver> m_observer;
};

}  // namespace blink

#endif  // RespondWithObserver_h
