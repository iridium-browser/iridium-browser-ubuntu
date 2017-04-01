/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "core/loader/appcache/ApplicationCacheHost.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/events/ApplicationCacheErrorEvent.h"
#include "core/events/ProgressEvent.h"
#include "core/frame/Deprecation.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/appcache/ApplicationCache.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebVector.h"

using namespace blink;

namespace blink {

// We provide a custom implementation of this class that calls out to the
// embedding application instead of using WebCore's built in appcache system.
// This file replaces webcore/appcache/ApplicationCacheHost.cpp in our build.

ApplicationCacheHost::ApplicationCacheHost(DocumentLoader* documentLoader)
    : m_domApplicationCache(nullptr),
      m_documentLoader(documentLoader),
      m_defersEvents(true) {
  DCHECK(m_documentLoader);
}

ApplicationCacheHost::~ApplicationCacheHost() {
  // Verify that detachFromDocumentLoader() has been performed already.
  DCHECK(!m_host);
}

void ApplicationCacheHost::willStartLoadingMainResource(
    ResourceRequest& request) {
  // We defer creating the outer host object to avoid spurious
  // creation/destruction around creating empty documents. At this point, we're
  // initiating a main resource load for the document, so its for real.

  if (!isApplicationCacheEnabled())
    return;

  DCHECK(m_documentLoader->frame());
  LocalFrame& frame = *m_documentLoader->frame();
  m_host = frame.loader().client()->createApplicationCacheHost(this);
  if (!m_host)
    return;

  WrappedResourceRequest wrapped(request);

  const WebApplicationCacheHost* spawningHost = nullptr;
  Frame* spawningFrame = frame.tree().parent();
  if (!spawningFrame || !spawningFrame->isLocalFrame())
    spawningFrame = frame.loader().opener();
  if (!spawningFrame || !spawningFrame->isLocalFrame())
    spawningFrame = &frame;
  if (DocumentLoader* spawningDocLoader =
          toLocalFrame(spawningFrame)->loader().documentLoader()) {
    spawningHost = spawningDocLoader->applicationCacheHost()
                       ? spawningDocLoader->applicationCacheHost()->m_host.get()
                       : nullptr;
  }

  m_host->willStartMainResourceRequest(wrapped, spawningHost);

  // NOTE: The semantics of this method, and others in this interface, are
  // subtly different than the method names would suggest. For example, in this
  // method never returns an appcached response in the SubstituteData out
  // argument, instead we return the appcached response thru the usual resource
  // loading pipeline.
}

void ApplicationCacheHost::selectCacheWithoutManifest() {
  if (m_host)
    m_host->selectCacheWithoutManifest();
}

void ApplicationCacheHost::selectCacheWithManifest(const KURL& manifestURL) {
  DCHECK(m_documentLoader);

  LocalFrame* frame = m_documentLoader->frame();
  Document* document = frame->document();
  if (document->isSecureContext()) {
    UseCounter::count(document,
                      UseCounter::ApplicationCacheManifestSelectSecureOrigin);
    UseCounter::countCrossOriginIframe(
        *document, UseCounter::ApplicationCacheManifestSelectSecureOrigin);
  } else {
    Deprecation::countDeprecation(
        document, UseCounter::ApplicationCacheManifestSelectInsecureOrigin);
    Deprecation::countDeprecationCrossOriginIframe(
        *document, UseCounter::ApplicationCacheManifestSelectInsecureOrigin);
    HostsUsingFeatures::countAnyWorld(
        *document, HostsUsingFeatures::Feature::
                       ApplicationCacheManifestSelectInsecureHost);
  }
  if (m_host && !m_host->selectCacheWithManifest(manifestURL)) {
    // It's a foreign entry, restart the current navigation from the top of the
    // navigation algorithm. The navigation will not result in the same resource
    // being loaded, because "foreign" entries are never picked during
    // navigation. see ApplicationCacheGroup::selectCache()
    frame->navigate(*document, document->url(), true, UserGestureStatus::None);
  }
}

void ApplicationCacheHost::didReceiveResponseForMainResource(
    const ResourceResponse& response) {
  if (m_host) {
    WrappedResourceResponse wrapped(response);
    m_host->didReceiveResponseForMainResource(wrapped);
  }
}

void ApplicationCacheHost::mainResourceDataReceived(const char* data,
                                                    size_t length) {
  if (m_host)
    m_host->didReceiveDataForMainResource(data, length);
}

void ApplicationCacheHost::failedLoadingMainResource() {
  if (m_host)
    m_host->didFinishLoadingMainResource(false);
}

void ApplicationCacheHost::finishedLoadingMainResource() {
  if (m_host)
    m_host->didFinishLoadingMainResource(true);
}

void ApplicationCacheHost::willStartLoadingResource(ResourceRequest& request) {
  if (m_host) {
    WrappedResourceRequest wrapped(request);
    m_host->willStartSubResourceRequest(wrapped);
  }
}

void ApplicationCacheHost::setApplicationCache(
    ApplicationCache* domApplicationCache) {
  DCHECK(!m_domApplicationCache || !domApplicationCache);
  m_domApplicationCache = domApplicationCache;
}

void ApplicationCacheHost::detachFromDocumentLoader() {
  // Detach from the owning DocumentLoader and let go of
  // WebApplicationCacheHost.
  setApplicationCache(nullptr);
  m_host.reset();
  m_documentLoader = nullptr;
}

void ApplicationCacheHost::notifyApplicationCache(
    EventID id,
    int progressTotal,
    int progressDone,
    WebApplicationCacheHost::ErrorReason errorReason,
    const String& errorURL,
    int errorStatus,
    const String& errorMessage) {
  if (id != kProgressEvent) {
    InspectorInstrumentation::updateApplicationCacheStatus(
        m_documentLoader->frame());
  }

  if (m_defersEvents) {
    // Event dispatching is deferred until document.onload has fired.
    m_deferredEvents.push_back(DeferredEvent(id, progressTotal, progressDone,
                                             errorReason, errorURL, errorStatus,
                                             errorMessage));
    return;
  }
  dispatchDOMEvent(id, progressTotal, progressDone, errorReason, errorURL,
                   errorStatus, errorMessage);
}

ApplicationCacheHost::CacheInfo ApplicationCacheHost::applicationCacheInfo() {
  if (!m_host)
    return CacheInfo(KURL(), 0, 0, 0);

  WebApplicationCacheHost::CacheInfo webInfo;
  m_host->getAssociatedCacheInfo(&webInfo);
  return CacheInfo(webInfo.manifestURL, webInfo.creationTime,
                   webInfo.updateTime, webInfo.totalSize);
}

void ApplicationCacheHost::fillResourceList(ResourceInfoList* resources) {
  if (!m_host)
    return;

  WebVector<WebApplicationCacheHost::ResourceInfo> webResources;
  m_host->getResourceList(&webResources);
  for (size_t i = 0; i < webResources.size(); ++i) {
    resources->push_back(
        ResourceInfo(webResources[i].url, webResources[i].isMaster,
                     webResources[i].isManifest, webResources[i].isFallback,
                     webResources[i].isForeign, webResources[i].isExplicit,
                     webResources[i].size));
  }
}

void ApplicationCacheHost::stopDeferringEvents() {
  for (unsigned i = 0; i < m_deferredEvents.size(); ++i) {
    const DeferredEvent& deferred = m_deferredEvents[i];
    dispatchDOMEvent(deferred.eventID, deferred.progressTotal,
                     deferred.progressDone, deferred.errorReason,
                     deferred.errorURL, deferred.errorStatus,
                     deferred.errorMessage);
  }
  m_deferredEvents.clear();
  m_defersEvents = false;
}

void ApplicationCacheHost::dispatchDOMEvent(
    EventID id,
    int progressTotal,
    int progressDone,
    WebApplicationCacheHost::ErrorReason errorReason,
    const String& errorURL,
    int errorStatus,
    const String& errorMessage) {
  // Don't dispatch an event if the window is detached.
  if (!m_domApplicationCache || !m_domApplicationCache->domWindow())
    return;

  const AtomicString& eventType = ApplicationCache::toEventType(id);
  if (eventType.isEmpty())
    return;
  Event* event = nullptr;
  if (id == kProgressEvent) {
    event = ProgressEvent::create(eventType, true, progressDone, progressTotal);
  } else if (id == kErrorEvent) {
    event = ApplicationCacheErrorEvent::create(errorReason, errorURL,
                                               errorStatus, errorMessage);
  } else {
    event = Event::create(eventType);
  }
  m_domApplicationCache->dispatchEvent(event);
}

ApplicationCacheHost::Status ApplicationCacheHost::getStatus() const {
  return m_host ? static_cast<Status>(m_host->getStatus()) : kUncached;
}

bool ApplicationCacheHost::update() {
  return m_host ? m_host->startUpdate() : false;
}

bool ApplicationCacheHost::swapCache() {
  bool success = m_host ? m_host->swapCache() : false;
  if (success) {
    InspectorInstrumentation::updateApplicationCacheStatus(
        m_documentLoader->frame());
  }
  return success;
}

void ApplicationCacheHost::abort() {
  if (m_host)
    m_host->abort();
}

bool ApplicationCacheHost::isApplicationCacheEnabled() {
  DCHECK(m_documentLoader->frame());
  return m_documentLoader->frame()->settings() &&
         m_documentLoader->frame()
             ->settings()
             ->getOfflineWebApplicationCacheEnabled();
}

void ApplicationCacheHost::didChangeCacheAssociation() {
  // FIXME: Prod the inspector to update its notion of what cache the page is
  // using.
}

void ApplicationCacheHost::notifyEventListener(
    WebApplicationCacheHost::EventID eventID) {
  notifyApplicationCache(static_cast<ApplicationCacheHost::EventID>(eventID), 0,
                         0, WebApplicationCacheHost::UnknownError, String(), 0,
                         String());
}

void ApplicationCacheHost::notifyProgressEventListener(const WebURL&,
                                                       int progressTotal,
                                                       int progressDone) {
  notifyApplicationCache(kProgressEvent, progressTotal, progressDone,
                         WebApplicationCacheHost::UnknownError, String(), 0,
                         String());
}

void ApplicationCacheHost::notifyErrorEventListener(
    WebApplicationCacheHost::ErrorReason reason,
    const WebURL& url,
    int status,
    const WebString& message) {
  notifyApplicationCache(kErrorEvent, 0, 0, reason, url.string(), status,
                         message);
}

DEFINE_TRACE(ApplicationCacheHost) {
  visitor->trace(m_domApplicationCache);
  visitor->trace(m_documentLoader);
}

}  // namespace blink
