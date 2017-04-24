/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#ifndef ServiceWorkerGlobalScope_h
#define ServiceWorkerGlobalScope_h

#include "bindings/modules/v8/RequestOrUSVString.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "wtf/Assertions.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class Dictionary;
class ScriptPromise;
class ScriptState;
class ServiceWorkerClients;
class ServiceWorkerRegistration;
class ServiceWorkerThread;
class WaitUntilObserver;
class WorkerThreadStartupData;

typedef RequestOrUSVString RequestInfo;

class MODULES_EXPORT ServiceWorkerGlobalScope final : public WorkerGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ServiceWorkerGlobalScope* create(
      ServiceWorkerThread*,
      std::unique_ptr<WorkerThreadStartupData>);

  ~ServiceWorkerGlobalScope() override;
  bool isServiceWorkerGlobalScope() const override { return true; }

  // Counts an evaluated script and its size. Called for each of the main
  // worker script and imported scripts.
  void countScript(size_t scriptSize, size_t cachedMetadataSize);

  // Called when the main worker script is evaluated.
  void didEvaluateWorkerScript();

  // ServiceWorkerGlobalScope.idl
  ServiceWorkerClients* clients();
  ServiceWorkerRegistration* registration();

  ScriptPromise fetch(ScriptState*,
                      const RequestInfo&,
                      const Dictionary&,
                      ExceptionState&);

  ScriptPromise skipWaiting(ScriptState*);

  void setRegistration(std::unique_ptr<WebServiceWorkerRegistration::Handle>);

  // EventTarget
  const AtomicString& interfaceName() const override;

  void dispatchExtendableEvent(Event*, WaitUntilObserver*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(install);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(activate);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(fetch);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(foreignfetch);

  DECLARE_VIRTUAL_TRACE();

 protected:
  // EventTarget
  DispatchEventResult dispatchEventInternal(Event*) override;
  bool addEventListenerInternal(
      const AtomicString& eventType,
      EventListener*,
      const AddEventListenerOptionsResolved&) override;

 private:
  ServiceWorkerGlobalScope(const KURL&,
                           const String& userAgent,
                           ServiceWorkerThread*,
                           double timeOrigin,
                           std::unique_ptr<SecurityOrigin::PrivilegeData>,
                           WorkerClients*);
  void importScripts(const Vector<String>& urls, ExceptionState&) override;
  CachedMetadataHandler* createWorkerScriptCachedMetadataHandler(
      const KURL& scriptURL,
      const Vector<char>* metaData) override;
  void exceptionThrown(ErrorEvent*) override;

  Member<ServiceWorkerClients> m_clients;
  Member<ServiceWorkerRegistration> m_registration;
  bool m_didEvaluateScript;
  bool m_hadErrorInTopLevelEventHandler;
  unsigned m_eventNestingLevel;
  size_t m_scriptCount;
  size_t m_scriptTotalSize;
  size_t m_scriptCachedMetadataTotalSize;
};

DEFINE_TYPE_CASTS(ServiceWorkerGlobalScope,
                  ExecutionContext,
                  context,
                  context->isServiceWorkerGlobalScope(),
                  context.isServiceWorkerGlobalScope());

}  // namespace blink

#endif  // ServiceWorkerGlobalScope_h
