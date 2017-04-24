/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
    rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "platform/loader/fetch/ResourceFetcher.h"

#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/loader/fetch/FetchContext.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/loader/fetch/ResourceLoadingLog.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/mhtml/ArchiveResource.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/network/NetworkInstrumentation.h"
#include "platform/network/NetworkUtils.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"

using blink::WebURLRequest;

namespace blink {

namespace {

// Events for UMA. Do not reorder or delete. Add new events at the end, but
// before SriResourceIntegrityMismatchEventCount.
enum SriResourceIntegrityMismatchEvent {
  CheckingForIntegrityMismatch = 0,
  RefetchDueToIntegrityMismatch = 1,
  SriResourceIntegrityMismatchEventCount
};

#define DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, name)                        \
  case Resource::name: {                                                      \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                          \
        EnumerationHistogram, resourceHistogram,                              \
        new EnumerationHistogram(                                             \
            "Blink.MemoryCache.RevalidationPolicy." prefix #name, Load + 1)); \
    resourceHistogram.count(policy);                                          \
    break;                                                                    \
  }

#define DEFINE_RESOURCE_HISTOGRAM(prefix)                    \
  switch (factory.type()) {                                  \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, CSSStyleSheet)  \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Font)           \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Image)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, ImportResource) \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, LinkPrefetch)   \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, MainResource)   \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Manifest)       \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Media)          \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Mock)           \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Raw)            \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, Script)         \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, SVGDocument)    \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, TextTrack)      \
    DEFINE_SINGLE_RESOURCE_HISTOGRAM(prefix, XSLStyleSheet)  \
  }

void addRedirectsToTimingInfo(Resource* resource, ResourceTimingInfo* info) {
  // Store redirect responses that were packed inside the final response.
  const auto& responses = resource->response().redirectResponses();
  for (size_t i = 0; i < responses.size(); ++i) {
    const KURL& newURL = i + 1 < responses.size()
                             ? KURL(responses[i + 1].url())
                             : resource->resourceRequest().url();
    bool crossOrigin =
        !SecurityOrigin::areSameSchemeHostPort(responses[i].url(), newURL);
    info->addRedirect(responses[i], crossOrigin);
  }
}

void RecordSriResourceIntegrityMismatchEvent(
    SriResourceIntegrityMismatchEvent event) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, integrityHistogram,
      new EnumerationHistogram("sri.resource_integrity_mismatch_event",
                               SriResourceIntegrityMismatchEventCount));
  integrityHistogram.count(event);
}

ResourceLoadPriority typeToPriority(Resource::Type type) {
  switch (type) {
    case Resource::MainResource:
    case Resource::CSSStyleSheet:
    case Resource::Font:
      // Also parser-blocking scripts (set explicitly in loadPriority)
      return ResourceLoadPriorityVeryHigh;
    case Resource::XSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::Raw:
    case Resource::ImportResource:
    case Resource::Script:
      // Also visible resources/images (set explicitly in loadPriority)
      return ResourceLoadPriorityHigh;
    case Resource::Manifest:
    case Resource::Mock:
      // Also late-body scripts discovered by the preload scanner (set
      // explicitly in loadPriority)
      return ResourceLoadPriorityMedium;
    case Resource::Image:
    case Resource::TextTrack:
    case Resource::Media:
    case Resource::SVGDocument:
      // Also async scripts (set explicitly in loadPriority)
      return ResourceLoadPriorityLow;
    case Resource::LinkPrefetch:
      return ResourceLoadPriorityVeryLow;
  }

  NOTREACHED();
  return ResourceLoadPriorityUnresolved;
}

}  // namespace

ResourceLoadPriority ResourceFetcher::computeLoadPriority(
    Resource::Type type,
    const ResourceRequest& resourceRequest,
    ResourcePriority::VisibilityStatus visibility,
    FetchRequest::DeferOption deferOption,
    bool speculativePreload) {
  ResourceLoadPriority priority = typeToPriority(type);

  // Visible resources (images in practice) get a boost to High priority.
  if (visibility == ResourcePriority::Visible)
    priority = ResourceLoadPriorityHigh;

  // Resources before the first image are considered "early" in the document and
  // resources after the first image are "late" in the document.  Important to
  // note that this is based on when the preload scanner discovers a resource
  // for the most part so the main parser may not have reached the image element
  // yet.
  if (type == Resource::Image)
    m_imageFetched = true;

  if (FetchRequest::IdleLoad == deferOption) {
    priority = ResourceLoadPriorityVeryLow;
  } else if (type == Resource::Script) {
    // Special handling for scripts.
    // Default/Parser-Blocking/Preload early in document: High (set in
    // typeToPriority)
    // Async/Defer: Low Priority (applies to both preload and parser-inserted)
    // Preload late in document: Medium
    if (FetchRequest::LazyLoad == deferOption) {
      priority = ResourceLoadPriorityLow;
    } else if (speculativePreload && m_imageFetched) {
      // Speculative preload is used as a signal for scripts at the bottom of
      // the document.
      priority = ResourceLoadPriorityMedium;
    }
  } else if (FetchRequest::LazyLoad == deferOption) {
    priority = ResourceLoadPriorityVeryLow;
  }

  // A manually set priority acts as a floor. This is used to ensure that
  // synchronous requests are always given the highest possible priority, as
  // well as to ensure that there isn't priority churn if images move in and out
  // of the viewport, or is displayed more than once, both in and out of the
  // viewport.
  return std::max(context().modifyPriorityForExperiments(priority),
                  resourceRequest.priority());
}

static void populateTimingInfo(ResourceTimingInfo* info, Resource* resource) {
  KURL initialURL = resource->response().redirectResponses().isEmpty()
                        ? resource->resourceRequest().url()
                        : resource->response().redirectResponses()[0].url();
  info->setInitialURL(initialURL);
  info->setFinalResponse(resource->response());
}

static WebURLRequest::RequestContext requestContextFromType(
    bool isMainFrame,
    Resource::Type type) {
  switch (type) {
    case Resource::MainResource:
      if (!isMainFrame)
        return WebURLRequest::RequestContextIframe;
      // FIXME: Change this to a context frame type (once we introduce them):
      // http://fetch.spec.whatwg.org/#concept-request-context-frame-type
      return WebURLRequest::RequestContextHyperlink;
    case Resource::XSLStyleSheet:
      DCHECK(RuntimeEnabledFeatures::xsltEnabled());
    case Resource::CSSStyleSheet:
      return WebURLRequest::RequestContextStyle;
    case Resource::Script:
      return WebURLRequest::RequestContextScript;
    case Resource::Font:
      return WebURLRequest::RequestContextFont;
    case Resource::Image:
      return WebURLRequest::RequestContextImage;
    case Resource::Raw:
      return WebURLRequest::RequestContextSubresource;
    case Resource::ImportResource:
      return WebURLRequest::RequestContextImport;
    case Resource::LinkPrefetch:
      return WebURLRequest::RequestContextPrefetch;
    case Resource::TextTrack:
      return WebURLRequest::RequestContextTrack;
    case Resource::SVGDocument:
      return WebURLRequest::RequestContextImage;
    case Resource::Media:  // TODO: Split this.
      return WebURLRequest::RequestContextVideo;
    case Resource::Manifest:
      return WebURLRequest::RequestContextManifest;
    case Resource::Mock:
      return WebURLRequest::RequestContextSubresource;
  }
  NOTREACHED();
  return WebURLRequest::RequestContextSubresource;
}

ResourceFetcher::ResourceFetcher(FetchContext* newContext)
    : m_context(newContext),
      m_archive(context().isMainFrame() ? nullptr : context().archive()),
      // loadingTaskRunner() is null in tests that use the null fetch context.
      m_resourceTimingReportTimer(
          context().loadingTaskRunner()
              ? context().loadingTaskRunner()
              : Platform::current()->currentThread()->getWebTaskRunner(),
          this,
          &ResourceFetcher::resourceTimingReportTimerFired),
      m_autoLoadImages(true),
      m_imagesEnabled(true),
      m_allowStaleResources(false),
      m_imageFetched(false) {}

ResourceFetcher::~ResourceFetcher() {}

Resource* ResourceFetcher::cachedResource(const KURL& resourceURL) const {
  KURL url = MemoryCache::removeFragmentIdentifierIfNeeded(resourceURL);
  const WeakMember<Resource>& resource = m_documentResources.at(url);
  return resource.get();
}

bool ResourceFetcher::isControlledByServiceWorker() const {
  return context().isControlledByServiceWorker();
}

bool ResourceFetcher::resourceNeedsLoad(Resource* resource,
                                        const FetchRequest& request,
                                        RevalidationPolicy policy) {
  // Defer a font load until it is actually needed unless this is a link
  // preload.
  if (resource->getType() == Resource::Font && !request.isLinkPreload())
    return false;
  if (resource->isImage() && shouldDeferImageLoad(resource->url()))
    return false;
  return policy != Use || resource->stillNeedsLoad();
}

// Limit the number of URLs in m_validatedURLs to avoid memory bloat.
// http://crbug.com/52411
static const int kMaxValidatedURLsSize = 10000;

void ResourceFetcher::requestLoadStarted(unsigned long identifier,
                                         Resource* resource,
                                         const FetchRequest& request,
                                         ResourceLoadStartType type,
                                         bool isStaticData) {
  if (type == ResourceLoadingFromCache &&
      resource->getStatus() == ResourceStatus::Cached &&
      !m_validatedURLs.contains(resource->url())) {
    context().dispatchDidLoadResourceFromMemoryCache(
        identifier, resource, request.resourceRequest().frameType(),
        request.resourceRequest().requestContext());
  }

  if (isStaticData)
    return;

  if (type == ResourceLoadingFromCache && !resource->stillNeedsLoad() &&
      !m_validatedURLs.contains(request.resourceRequest().url())) {
    // Resources loaded from memory cache should be reported the first time
    // they're used.
    std::unique_ptr<ResourceTimingInfo> info = ResourceTimingInfo::create(
        request.options().initiatorInfo.name, monotonicallyIncreasingTime(),
        resource->getType() == Resource::MainResource);
    populateTimingInfo(info.get(), resource);
    info->clearLoadTimings();
    info->setLoadFinishTime(info->initialTime());
    m_scheduledResourceTimingReports.push_back(std::move(info));
    if (!m_resourceTimingReportTimer.isActive())
      m_resourceTimingReportTimer.startOneShot(0, BLINK_FROM_HERE);
  }

  if (m_validatedURLs.size() >= kMaxValidatedURLsSize) {
    m_validatedURLs.clear();
  }
  m_validatedURLs.insert(request.resourceRequest().url());
}

static std::unique_ptr<TracedValue> urlForTraceEvent(const KURL& url) {
  std::unique_ptr<TracedValue> value = TracedValue::create();
  value->setString("url", url.getString());
  return value;
}

Resource* ResourceFetcher::resourceForStaticData(
    const FetchRequest& request,
    const ResourceFactory& factory,
    const SubstituteData& substituteData) {
  const KURL& url = request.resourceRequest().url();
  DCHECK(url.protocolIsData() || substituteData.isValid() || m_archive);

  // TODO(japhet): We only send main resource data: urls through WebURLLoader
  // for the benefit of a service worker test
  // (RenderViewImplTest.ServiceWorkerNetworkProviderSetup), which is at a layer
  // where it isn't easy to mock out a network load. It uses data: urls to
  // emulate the behavior it wants to test, which would otherwise be reserved
  // for network loads.
  if (!m_archive && !substituteData.isValid() &&
      (factory.type() == Resource::MainResource ||
       factory.type() == Resource::Raw))
    return nullptr;

  const String cacheIdentifier = getCacheIdentifier();
  if (Resource* oldResource =
          memoryCache()->resourceForURL(url, cacheIdentifier)) {
    // There's no reason to re-parse if we saved the data from the previous
    // parse.
    if (request.options().dataBufferingPolicy != DoNotBufferData)
      return oldResource;
    memoryCache()->remove(oldResource);
  }

  AtomicString mimetype;
  AtomicString charset;
  RefPtr<SharedBuffer> data;
  if (substituteData.isValid()) {
    mimetype = substituteData.mimeType();
    charset = substituteData.textEncoding();
    data = substituteData.content();
  } else if (url.protocolIsData()) {
    data = PassRefPtr<SharedBuffer>(
        NetworkUtils::parseDataURL(url, mimetype, charset));
    if (!data)
      return nullptr;
  } else {
    ArchiveResource* archiveResource =
        m_archive->subresourceForURL(request.url());
    // Fall back to the network if the archive doesn't contain the resource.
    if (!archiveResource)
      return nullptr;
    mimetype = archiveResource->mimeType();
    charset = archiveResource->textEncoding();
    data = archiveResource->data();
  }

  ResourceResponse response(url, mimetype, data->size(), charset);
  if (!substituteData.isValid() && url.protocolIsData()) {
    response.setHTTPStatusCode(200);
    response.setHTTPStatusText("OK");
  }

  Resource* resource = factory.create(request.resourceRequest(),
                                      request.options(), request.charset());
  resource->setNeedsSynchronousCacheHit(substituteData.forceSynchronousLoad());
  // FIXME: We should provide a body stream here.
  resource->responseReceived(response, nullptr);
  resource->setDataBufferingPolicy(BufferData);
  if (data->size())
    resource->setResourceBuffer(data);
  resource->setIdentifier(createUniqueIdentifier());
  resource->setCacheIdentifier(cacheIdentifier);
  resource->finish();

  if (!substituteData.isValid())
    memoryCache()->add(resource);

  return resource;
}

Resource* ResourceFetcher::resourceForBlockedRequest(
    const FetchRequest& request,
    const ResourceFactory& factory,
    ResourceRequestBlockedReason blockedReason) {
  Resource* resource = factory.create(request.resourceRequest(),
                                      request.options(), request.charset());
  resource->error(ResourceError::cancelledDueToAccessCheckError(request.url(),
                                                                blockedReason));
  return resource;
}

void ResourceFetcher::makePreloadedResourceBlockOnloadIfNeeded(
    Resource* resource,
    const FetchRequest& request) {
  // TODO(yoav): Test that non-blocking resources (video/audio/track) continue
  // to not-block even after being preloaded and discovered.
  if (resource && resource->loader() &&
      resource->isLoadEventBlockingResourceType() &&
      resource->isLinkPreload() && !request.isLinkPreload() &&
      m_nonBlockingLoaders.contains(resource->loader())) {
    m_nonBlockingLoaders.erase(resource->loader());
    m_loaders.insert(resource->loader());
  }
}

void ResourceFetcher::updateMemoryCacheStats(Resource* resource,
                                             RevalidationPolicy policy,
                                             const FetchRequest& request,
                                             const ResourceFactory& factory,
                                             bool isStaticData) const {
  if (isStaticData)
    return;

  if (request.isSpeculativePreload() || request.isLinkPreload()) {
    DEFINE_RESOURCE_HISTOGRAM("Preload.");
  } else {
    DEFINE_RESOURCE_HISTOGRAM("");
  }

  // Aims to count Resource only referenced from MemoryCache (i.e. what would be
  // dead if MemoryCache holds weak references to Resource). Currently we check
  // references to Resource from ResourceClient and |m_preloads| only, because
  // they are major sources of references.
  if (resource && !resource->isAlive() &&
      (!m_preloads || !m_preloads->contains(resource))) {
    DEFINE_RESOURCE_HISTOGRAM("Dead.");
  }
}

ResourceFetcher::PrepareRequestResult ResourceFetcher::prepareRequest(
    FetchRequest& request,
    const ResourceFactory& factory,
    const SubstituteData& substituteData,
    unsigned long identifier,
    ResourceRequestBlockedReason& blockedReason) {
  ResourceRequest& resourceRequest = request.mutableResourceRequest();

  DCHECK(request.options().synchronousPolicy == RequestAsynchronously ||
         factory.type() == Resource::Raw ||
         factory.type() == Resource::XSLStyleSheet);

  context().populateResourceRequest(
      factory.type(), request.clientHintsPreferences(),
      request.getResourceWidth(), resourceRequest);

  if (!request.url().isValid())
    return Abort;

  resourceRequest.setPriority(computeLoadPriority(
      factory.type(), request.resourceRequest(), ResourcePriority::NotVisible,
      request.defer(), request.isSpeculativePreload()));
  initializeResourceRequest(resourceRequest, factory.type(), request.defer());
  network_instrumentation::resourcePrioritySet(identifier,
                                               resourceRequest.priority());

  blockedReason = context().canRequest(
      factory.type(), resourceRequest,
      MemoryCache::removeFragmentIdentifierIfNeeded(request.url()),
      request.options(),
      /* Don't send security violation reports for speculative preloads */
      request.isSpeculativePreload()
          ? SecurityViolationReportingPolicy::SuppressReporting
          : SecurityViolationReportingPolicy::Report,
      request.getOriginRestriction());
  if (blockedReason != ResourceRequestBlockedReason::None) {
    DCHECK(!substituteData.forceSynchronousLoad());
    return Block;
  }

  context().willStartLoadingResource(
      identifier, resourceRequest, factory.type(),
      request.options().initiatorInfo.name,
      (request.isSpeculativePreload()
           ? FetchContext::V8ActivityLoggingPolicy::SuppressLogging
           : FetchContext::V8ActivityLoggingPolicy::Log));
  if (!request.url().isValid())
    return Abort;

  resourceRequest.setAllowStoredCredentials(
      request.options().allowCredentials == AllowStoredCredentials);
  return Continue;
}

Resource* ResourceFetcher::requestResource(
    FetchRequest& request,
    const ResourceFactory& factory,
    const SubstituteData& substituteData) {
  unsigned long identifier = createUniqueIdentifier();
  ResourceRequest& resourceRequest = request.mutableResourceRequest();
  network_instrumentation::ScopedResourceLoadTracker scopedResourceLoadTracker(
      identifier, resourceRequest);
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.Fetch.RequestResourceTime");
  // TODO(dproy): Remove this. http://crbug.com/659666
  TRACE_EVENT1("blink", "ResourceFetcher::requestResource", "url",
               urlForTraceEvent(request.url()));

  Resource* resource = nullptr;
  ResourceRequestBlockedReason blockedReason =
      ResourceRequestBlockedReason::None;

  PrepareRequestResult result = prepareRequest(request, factory, substituteData,
                                               identifier, blockedReason);
  if (result == Abort)
    return nullptr;
  if (result == Block)
    return resourceForBlockedRequest(request, factory, blockedReason);

  bool isDataUrl = resourceRequest.url().protocolIsData();
  bool isStaticData = isDataUrl || substituteData.isValid() || m_archive;
  if (isStaticData) {
    resource = resourceForStaticData(request, factory, substituteData);
    // Abort the request if the archive doesn't contain the resource, except in
    // the case of data URLs which might have resources such as fonts that need
    // to be decoded only on demand.  These data URLs are allowed to be
    // processed using the normal ResourceFetcher machinery.
    if (!resource && !isDataUrl && m_archive)
      return nullptr;
  }
  if (!resource) {
    resource =
        memoryCache()->resourceForURL(request.url(), getCacheIdentifier());
  }

  // If we got a preloaded resource from the cache for a non-preload request,
  // we may need to make it block the onload event.
  makePreloadedResourceBlockOnloadIfNeeded(resource, request);

  const RevalidationPolicy policy = determineRevalidationPolicy(
      factory.type(), request, resource, isStaticData);
  TRACE_EVENT_INSTANT1("blink", "ResourceFetcher::determineRevalidationPolicy",
                       TRACE_EVENT_SCOPE_THREAD, "revalidationPolicy", policy);

  updateMemoryCacheStats(resource, policy, request, factory, isStaticData);

  switch (policy) {
    case Reload:
      memoryCache()->remove(resource);
    // Fall through
    case Load:
      resource = createResourceForLoading(request, request.charset(), factory);
      break;
    case Revalidate:
      initializeRevalidation(resourceRequest, resource);
      break;
    case Use:
      if (resource->isLinkPreload() && !request.isLinkPreload())
        resource->setLinkPreload(false);
      break;
  }
  if (!resource)
    return nullptr;

  // TODO(yoav): turn to a DCHECK. See https://crbug.com/690632
  CHECK_EQ(resource->getType(), factory.type());

  if (!resource->isAlive())
    m_deadStatsRecorder.update(policy);

  if (policy != Use)
    resource->setIdentifier(identifier);

  // TODO(yoav): It is not clear why preloads are exempt from this check. Can we
  // remove the exemption?
  if (!request.isSpeculativePreload() || policy != Use) {
    // When issuing another request for a resource that is already in-flight
    // make sure to not demote the priority of the in-flight request. If the new
    // request isn't at the same priority as the in-flight request, only allow
    // promotions. This can happen when a visible image's priority is increased
    // and then another reference to the image is parsed (which would be at a
    // lower priority).
    if (resourceRequest.priority() > resource->resourceRequest().priority())
      resource->didChangePriority(resourceRequest.priority(), 0);
    // TODO(yoav): I'd expect the stated scenario to not go here, as its policy
    // would be Use.
  }

  // If only the fragment identifiers differ, it is the same resource.
  DCHECK(equalIgnoringFragmentIdentifier(resource->url(), request.url()));
  requestLoadStarted(
      identifier, resource, request,
      policy == Use ? ResourceLoadingFromCache : ResourceLoadingFromNetwork,
      isStaticData);
  m_documentResources.set(
      MemoryCache::removeFragmentIdentifierIfNeeded(request.url()), resource);

  // Returns with an existing resource if the resource does not need to start
  // loading immediately. If revalidation policy was determined as |Revalidate|,
  // the resource was already initialized for the revalidation here, but won't
  // start loading.
  if (!resourceNeedsLoad(resource, request, policy))
    return resource;

  if (!startLoad(resource))
    return nullptr;
  scopedResourceLoadTracker.resourceLoadContinuesBeyondScope();

  DCHECK(!resource->errorOccurred() ||
         request.options().synchronousPolicy == RequestSynchronously);
  return resource;
}

void ResourceFetcher::resourceTimingReportTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &m_resourceTimingReportTimer);
  Vector<std::unique_ptr<ResourceTimingInfo>> timingReports;
  timingReports.swap(m_scheduledResourceTimingReports);
  for (const auto& timingInfo : timingReports)
    context().addResourceTiming(*timingInfo);
}

void ResourceFetcher::determineRequestContext(ResourceRequest& request,
                                              Resource::Type type,
                                              bool isMainFrame) {
  WebURLRequest::RequestContext requestContext =
      requestContextFromType(isMainFrame, type);
  request.setRequestContext(requestContext);
}

void ResourceFetcher::determineRequestContext(ResourceRequest& request,
                                              Resource::Type type) {
  determineRequestContext(request, type, context().isMainFrame());
}

void ResourceFetcher::initializeResourceRequest(
    ResourceRequest& request,
    Resource::Type type,
    FetchRequest::DeferOption defer) {
  if (request.getCachePolicy() == WebCachePolicy::UseProtocolCachePolicy) {
    request.setCachePolicy(
        context().resourceRequestCachePolicy(request, type, defer));
  }
  if (request.requestContext() == WebURLRequest::RequestContextUnspecified)
    determineRequestContext(request, type);
  if (type == Resource::LinkPrefetch)
    request.setHTTPHeaderField(HTTPNames::Purpose, "prefetch");

  context().addAdditionalRequestHeaders(
      request,
      (type == Resource::MainResource) ? FetchMainResource : FetchSubresource);
}

void ResourceFetcher::initializeRevalidation(
    ResourceRequest& revalidatingRequest,
    Resource* resource) {
  DCHECK(resource);
  DCHECK(memoryCache()->contains(resource));
  DCHECK(resource->isLoaded());
  DCHECK(resource->canUseCacheValidator());
  DCHECK(!resource->isCacheValidator());
  DCHECK(!context().isControlledByServiceWorker());

  const AtomicString& lastModified =
      resource->response().httpHeaderField(HTTPNames::Last_Modified);
  const AtomicString& eTag =
      resource->response().httpHeaderField(HTTPNames::ETag);
  if (!lastModified.isEmpty() || !eTag.isEmpty()) {
    DCHECK_NE(WebCachePolicy::BypassingCache,
              revalidatingRequest.getCachePolicy());
    if (revalidatingRequest.getCachePolicy() ==
        WebCachePolicy::ValidatingCacheData) {
      revalidatingRequest.setHTTPHeaderField(HTTPNames::Cache_Control,
                                             "max-age=0");
    }
  }
  if (!lastModified.isEmpty()) {
    revalidatingRequest.setHTTPHeaderField(HTTPNames::If_Modified_Since,
                                           lastModified);
  }
  if (!eTag.isEmpty())
    revalidatingRequest.setHTTPHeaderField(HTTPNames::If_None_Match, eTag);

  double stalenessLifetime = resource->stalenessLifetime();
  if (std::isfinite(stalenessLifetime) && stalenessLifetime > 0) {
    revalidatingRequest.setHTTPHeaderField(
        HTTPNames::Resource_Freshness,
        AtomicString(String::format(
            "max-age=%.0lf,stale-while-revalidate=%.0lf,age=%.0lf",
            resource->freshnessLifetime(), stalenessLifetime,
            resource->currentAge())));
  }

  resource->setRevalidatingRequest(revalidatingRequest);
}

Resource* ResourceFetcher::createResourceForLoading(
    FetchRequest& request,
    const String& charset,
    const ResourceFactory& factory) {
  const String cacheIdentifier = getCacheIdentifier();
  DCHECK(!memoryCache()->resourceForURL(request.resourceRequest().url(),
                                        cacheIdentifier));

  RESOURCE_LOADING_DVLOG(1) << "Loading Resource for "
                            << request.resourceRequest().url().elidedString();

  Resource* resource =
      factory.create(request.resourceRequest(), request.options(), charset);
  resource->setLinkPreload(request.isLinkPreload());
  if (request.isSpeculativePreload()) {
    resource->setPreloadDiscoveryTime(request.preloadDiscoveryTime());
  }
  resource->setCacheIdentifier(cacheIdentifier);

  // - Don't add main resource to cache to prevent reuse.
  // - Don't add the resource if its body will not be stored.
  if (factory.type() != Resource::MainResource &&
      request.options().dataBufferingPolicy != DoNotBufferData) {
    memoryCache()->add(resource);
  }
  return resource;
}

void ResourceFetcher::storePerformanceTimingInitiatorInformation(
    Resource* resource) {
  const AtomicString& fetchInitiator = resource->options().initiatorInfo.name;
  if (fetchInitiator == FetchInitiatorTypeNames::internal)
    return;

  bool isMainResource = resource->getType() == Resource::MainResource;

  // The request can already be fetched in a previous navigation. Thus
  // startTime must be set accordingly.
  double startTime = resource->resourceRequest().navigationStartTime()
                         ? resource->resourceRequest().navigationStartTime()
                         : monotonicallyIncreasingTime();

  // This buffer is created and populated for providing transferSize
  // and redirect timing opt-in information.
  if (isMainResource) {
    DCHECK(!m_navigationTimingInfo);
    m_navigationTimingInfo =
        ResourceTimingInfo::create(fetchInitiator, startTime, isMainResource);
  }

  std::unique_ptr<ResourceTimingInfo> info =
      ResourceTimingInfo::create(fetchInitiator, startTime, isMainResource);

  if (resource->isCacheValidator()) {
    const AtomicString& timingAllowOrigin =
        resource->response().httpHeaderField(HTTPNames::Timing_Allow_Origin);
    if (!timingAllowOrigin.isEmpty())
      info->setOriginalTimingAllowOrigin(timingAllowOrigin);
  }

  if (!isMainResource ||
      context().updateTimingInfoForIFrameNavigation(info.get())) {
    m_resourceTimingInfoMap.insert(resource, std::move(info));
  }
}

void ResourceFetcher::recordResourceTimingOnRedirect(
    Resource* resource,
    const ResourceResponse& redirectResponse,
    bool crossOrigin) {
  ResourceTimingInfoMap::iterator it = m_resourceTimingInfoMap.find(resource);
  if (it != m_resourceTimingInfoMap.end()) {
    it->value->addRedirect(redirectResponse, crossOrigin);
  }

  if (resource->getType() == Resource::MainResource) {
    DCHECK(m_navigationTimingInfo);
    m_navigationTimingInfo->addRedirect(redirectResponse, crossOrigin);
  }
}

ResourceFetcher::RevalidationPolicy
ResourceFetcher::determineRevalidationPolicy(Resource::Type type,
                                             const FetchRequest& fetchRequest,
                                             Resource* existingResource,
                                             bool isStaticData) const {
  const ResourceRequest& request = fetchRequest.resourceRequest();

  if (!existingResource)
    return Load;

  // If the existing resource is loading and the associated fetcher is not equal
  // to |this|, we must not use the resource. Otherwise, CSP violation may
  // happen in redirect handling.
  if (existingResource->loader() &&
      existingResource->loader()->fetcher() != this) {
    return Reload;
  }

  // Checks if the resource has an explicit policy about integrity metadata.
  //
  // This is necessary because ScriptResource and CSSStyleSheetResource objects
  // do not keep the raw data around after the source is accessed once, so if
  // the resource is accessed from the MemoryCache for a second time, there is
  // no way to redo an integrity check.
  //
  // Thus, Blink implements a scheme where it caches the integrity information
  // for those resources after the first time it is checked, and if there is
  // another request for that resource, with the same integrity metadata, Blink
  // skips the integrity calculation. However, if the integrity metadata is a
  // mismatch, the MemoryCache must be skipped here, and a new request for the
  // resource must be made to get the raw data. This is expected to be an
  // uncommon case, however, as it implies two same-origin requests to the same
  // resource, but with different integrity metadata.
  RecordSriResourceIntegrityMismatchEvent(CheckingForIntegrityMismatch);
  if (existingResource->mustRefetchDueToIntegrityMetadata(fetchRequest)) {
    RecordSriResourceIntegrityMismatchEvent(RefetchDueToIntegrityMismatch);
    return Reload;
  }

  // Service Worker's CORS fallback message must not be cached.
  if (existingResource->response().wasFallbackRequiredByServiceWorker())
    return Reload;

  // If the same URL has been loaded as a different type, we need to reload.
  if (existingResource->getType() != type) {
    // FIXME: If existingResource is a Preload and the new type is LinkPrefetch
    // We really should discard the new prefetch since the preload has more
    // specific type information! crbug.com/379893
    // fast/dom/HTMLLinkElement/link-and-subresource-test hits this case.
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to type mismatch.";
    return Reload;
  }

  // We already have a preload going for this URL.
  if (fetchRequest.isSpeculativePreload() && existingResource->isPreloaded())
    return Use;

  // Do not load from cache if images are not enabled. There are two general
  // cases:
  //
  // 1. Images are disabled. Don't ever load images, even if the image is cached
  // or it is a data: url. In this case, we "Reload" the image, then defer it
  // with resourceNeedsLoad() so that it never actually goes to the network.
  //
  // 2. Images are enabled, but not loaded automatically. In this case, we will
  // Use cached resources or data: urls, but will similarly fall back to a
  // deferred network load if we don't have the data available without a network
  // request. We check allowImage() here, which is affected by m_imagesEnabled
  // but not m_autoLoadImages, in order to allow for this differing behavior.
  //
  // TODO(japhet): Can we get rid of one of these settings?
  if (existingResource->isImage() &&
      !context().allowImage(m_imagesEnabled, existingResource->url())) {
    return Reload;
  }

  // Never use cache entries for downloadToFile / useStreamOnResponse requests.
  // The data will be delivered through other paths.
  if (request.downloadToFile() || request.useStreamOnResponse())
    return Reload;

  // Never reuse opaque responses from a service worker for requests that are
  // not no-cors. https://crbug.com/625575
  if (existingResource->response().wasFetchedViaServiceWorker() &&
      existingResource->response().serviceWorkerResponseType() ==
          WebServiceWorkerResponseTypeOpaque &&
      request.fetchRequestMode() != WebURLRequest::FetchRequestModeNoCORS) {
    return Reload;
  }

  // If resource was populated from a SubstituteData load or data: url, use it.
  if (isStaticData)
    return Use;

  if (!existingResource->canReuse(request))
    return Reload;

  // Certain requests (e.g., XHRs) might have manually set headers that require
  // revalidation. In theory, this should be a Revalidate case. In practice, the
  // MemoryCache revalidation path assumes a whole bunch of things about how
  // revalidation works that manual headers violate, so punt to Reload instead.
  //
  // Similarly, a request with manually added revalidation headers can lead to a
  // 304 response for a request that wasn't flagged as a revalidation attempt.
  // Normally, successful revalidation will maintain the original response's
  // status code, but for a manual revalidation the response code remains 304.
  // In this case, the Resource likely has insufficient context to provide a
  // useful cache hit or revalidation. See http://crbug.com/643659
  if (request.isConditional() ||
      existingResource->response().httpStatusCode() == 304) {
    return Reload;
  }

  // Don't reload resources while pasting.
  if (m_allowStaleResources)
    return Use;

  if (!fetchRequest.options().canReuseRequest(existingResource->options()))
    return Reload;

  // Always use preloads.
  if (existingResource->isPreloaded())
    return Use;

  // WebCachePolicy::ReturnCacheDataElseLoad uses the cache no matter what.
  if (request.getCachePolicy() == WebCachePolicy::ReturnCacheDataElseLoad)
    return Use;

  // Don't reuse resources with Cache-control: no-store.
  if (existingResource->hasCacheControlNoStoreHeader()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to Cache-control: no-store.";
    return Reload;
  }

  // If credentials were sent with the previous request and won't be with this
  // one, or vice versa, re-fetch the resource.
  //
  // This helps with the case where the server sends back
  // "Access-Control-Allow-Origin: *" all the time, but some of the client's
  // requests are made without CORS and some with.
  if (existingResource->resourceRequest().allowStoredCredentials() !=
      request.allowStoredCredentials()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to difference in credentials "
                                 "settings.";
    return Reload;
  }

  // During the initial load, avoid loading the same resource multiple times for
  // a single document, even if the cache policies would tell us to. We also
  // group loads of the same resource together. Raw resources are exempted, as
  // XHRs fall into this category and may have user-set Cache-Control: headers
  // or other factors that require separate requests.
  if (type != Resource::Raw) {
    if (!context().isLoadComplete() &&
        m_validatedURLs.contains(existingResource->url()))
      return Use;
    if (existingResource->isLoading())
      return Use;
  }

  // WebCachePolicy::BypassingCache always reloads
  if (request.getCachePolicy() == WebCachePolicy::BypassingCache) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to "
                                 "WebCachePolicy::BypassingCache.";
    return Reload;
  }

  // We'll try to reload the resource if it failed last time.
  if (existingResource->errorOccurred()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to resource being in the error "
                                 "state";
    return Reload;
  }

  // List of available images logic allows images to be re-used without cache
  // validation. We restrict this only to images from memory cache which are the
  // same as the version in the current document.
  if (type == Resource::Image &&
      existingResource == cachedResource(request.url())) {
    return Use;
  }

  if (existingResource->mustReloadDueToVaryHeader(request))
    return Reload;

  // If any of the redirects in the chain to loading the resource were not
  // cacheable, we cannot reuse our cached resource.
  if (!existingResource->canReuseRedirectChain()) {
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to an uncacheable redirect";
    return Reload;
  }

  // Check if the cache headers requires us to revalidate (cache expiration for
  // example).
  if (request.getCachePolicy() == WebCachePolicy::ValidatingCacheData ||
      existingResource->mustRevalidateDueToCacheHeaders() ||
      request.cacheControlContainsNoCache()) {
    // See if the resource has usable ETag or Last-modified headers. If the page
    // is controlled by the ServiceWorker, we choose the Reload policy because
    // the revalidation headers should not be exposed to the
    // ServiceWorker.(crbug.com/429570)
    if (existingResource->canUseCacheValidator() &&
        !context().isControlledByServiceWorker()) {
      // If the resource is already a cache validator but not started yet, the
      // |Use| policy should be applied to subsequent requests.
      if (existingResource->isCacheValidator()) {
        DCHECK(existingResource->stillNeedsLoad());
        return Use;
      }
      return Revalidate;
    }

    // No, must reload.
    RESOURCE_LOADING_DVLOG(1) << "ResourceFetcher::determineRevalidationPolicy "
                                 "reloading due to missing cache validators.";
    return Reload;
  }

  return Use;
}

void ResourceFetcher::setAutoLoadImages(bool enable) {
  if (enable == m_autoLoadImages)
    return;

  m_autoLoadImages = enable;

  if (!m_autoLoadImages)
    return;

  reloadImagesIfNotDeferred();
}

void ResourceFetcher::setImagesEnabled(bool enable) {
  if (enable == m_imagesEnabled)
    return;

  m_imagesEnabled = enable;

  if (!m_imagesEnabled)
    return;

  reloadImagesIfNotDeferred();
}

bool ResourceFetcher::shouldDeferImageLoad(const KURL& url) const {
  return !context().allowImage(m_imagesEnabled, url) || !m_autoLoadImages;
}

void ResourceFetcher::reloadImagesIfNotDeferred() {
  for (Resource* resource : m_documentResources.values()) {
    if (resource->getType() == Resource::Image && resource->stillNeedsLoad() &&
        !shouldDeferImageLoad(resource->url()))
      startLoad(resource);
  }
}

void ResourceFetcher::clearContext() {
  clearPreloads(ResourceFetcher::ClearAllPreloads);
  m_context.clear();
}

int ResourceFetcher::requestCount() const {
  return m_loaders.size();
}

bool ResourceFetcher::hasPendingRequest() const {
  return m_loaders.size() > 0 || m_nonBlockingLoaders.size() > 0;
}

void ResourceFetcher::preloadStarted(Resource* resource) {
  if (m_preloads && m_preloads->contains(resource))
    return;
  resource->increasePreloadCount();

  if (!m_preloads)
    m_preloads = new HeapListHashSet<Member<Resource>>;
  m_preloads->insert(resource);

  if (m_preloadedURLsForTest)
    m_preloadedURLsForTest->insert(resource->url().getString());
}

void ResourceFetcher::enableIsPreloadedForTest() {
  if (m_preloadedURLsForTest)
    return;
  m_preloadedURLsForTest = WTF::wrapUnique(new HashSet<String>);

  if (m_preloads) {
    for (const auto& resource : *m_preloads)
      m_preloadedURLsForTest->insert(resource->url().getString());
  }
}

bool ResourceFetcher::isPreloadedForTest(const KURL& url) const {
  DCHECK(m_preloadedURLsForTest);
  return m_preloadedURLsForTest->contains(url.getString());
}

void ResourceFetcher::clearPreloads(ClearPreloadsPolicy policy) {
  if (!m_preloads)
    return;

  logPreloadStats(policy);

  for (const auto& resource : *m_preloads) {
    if (policy == ClearAllPreloads || !resource->isLinkPreload()) {
      resource->decreasePreloadCount();
      if (resource->getPreloadResult() == Resource::PreloadNotReferenced)
        memoryCache()->remove(resource.get());
      m_preloads->remove(resource);
    }
  }
  if (!m_preloads->size())
    m_preloads.clear();
}

void ResourceFetcher::warnUnusedPreloads() {
  if (!m_preloads)
    return;
  for (const auto& resource : *m_preloads) {
    if (resource && resource->isLinkPreload() &&
        resource->getPreloadResult() == Resource::PreloadNotReferenced) {
      context().addConsoleMessage(
          "The resource " + resource->url().getString() +
              " was preloaded using link preload but not used within a few "
              "seconds from the window's load event. Please make sure it "
              "wasn't preloaded for nothing.",
          FetchContext::LogWarningMessage);
    }
  }
}

ArchiveResource* ResourceFetcher::createArchive(Resource* resource) {
  // Only the top-frame can load MHTML.
  if (!context().isMainFrame())
    return nullptr;
  m_archive = MHTMLArchive::create(resource->url(), resource->resourceBuffer());
  return m_archive ? m_archive->mainResource() : nullptr;
}

ResourceTimingInfo* ResourceFetcher::getNavigationTimingInfo() {
  return m_navigationTimingInfo.get();
}

void ResourceFetcher::handleLoadCompletion(Resource* resource) {
  context().didLoadResource(resource);

  resource->reloadIfLoFiOrPlaceholderImage(this, Resource::kReloadIfNeeded);
}

void ResourceFetcher::handleLoaderFinish(Resource* resource,
                                         double finishTime,
                                         LoaderFinishType type) {
  DCHECK(resource);

  ResourceLoader* loader = resource->loader();
  if (type == DidFinishFirstPartInMultipart) {
    // When loading a multipart resource, make the loader non-block when
    // finishing loading the first part.
    moveResourceLoaderToNonBlocking(loader);
  } else {
    removeResourceLoader(loader);
    DCHECK(!m_nonBlockingLoaders.contains(loader));
  }
  DCHECK(!m_loaders.contains(loader));

  const int64_t encodedDataLength = resource->response().encodedDataLength();

  if (resource->getType() == Resource::MainResource) {
    DCHECK(m_navigationTimingInfo);
    // Store redirect responses that were packed inside the final response.
    addRedirectsToTimingInfo(resource, m_navigationTimingInfo.get());
    if (resource->response().isHTTP()) {
      populateTimingInfo(m_navigationTimingInfo.get(), resource);
      m_navigationTimingInfo->addFinalTransferSize(
          encodedDataLength == -1 ? 0 : encodedDataLength);
    }
  }
  if (std::unique_ptr<ResourceTimingInfo> info =
          m_resourceTimingInfoMap.take(resource)) {
    // Store redirect responses that were packed inside the final response.
    addRedirectsToTimingInfo(resource, info.get());

    if (resource->response().isHTTP() &&
        resource->response().httpStatusCode() < 400) {
      populateTimingInfo(info.get(), resource);
      info->setLoadFinishTime(finishTime);
      // encodedDataLength == -1 means "not available".
      // TODO(ricea): Find cases where it is not available but the
      // PerformanceResourceTiming spec requires it to be available and fix
      // them.
      info->addFinalTransferSize(encodedDataLength == -1 ? 0
                                                         : encodedDataLength);

      if (resource->options().requestInitiatorContext == DocumentContext)
        context().addResourceTiming(*info);
      resource->reportResourceTimingToClients(*info);
    }
  }

  context().dispatchDidFinishLoading(resource->identifier(), finishTime,
                                     encodedDataLength,
                                     resource->response().decodedBodyLength());

  if (type == DidFinishLoading)
    resource->finish(finishTime);

  handleLoadCompletion(resource);
}

void ResourceFetcher::handleLoaderError(Resource* resource,
                                        const ResourceError& error) {
  DCHECK(resource);

  removeResourceLoader(resource->loader());

  m_resourceTimingInfoMap.take(resource);

  bool isInternalRequest = resource->options().initiatorInfo.name ==
                           FetchInitiatorTypeNames::internal;

  context().dispatchDidFail(resource->identifier(), error,
                            resource->response().encodedDataLength(),
                            isInternalRequest);

  resource->error(error);

  handleLoadCompletion(resource);
}

void ResourceFetcher::moveResourceLoaderToNonBlocking(ResourceLoader* loader) {
  DCHECK(loader);
  // TODO(yoav): Convert CHECK to DCHECK if no crash reports come in.
  CHECK(m_loaders.contains(loader));
  m_nonBlockingLoaders.insert(loader);
  m_loaders.erase(loader);
}

bool ResourceFetcher::startLoad(Resource* resource) {
  DCHECK(resource);
  DCHECK(resource->stillNeedsLoad());
  if (!context().shouldLoadNewResource(resource->getType())) {
    memoryCache()->remove(resource);
    return false;
  }

  ResourceRequest request(resource->resourceRequest());
  context().dispatchWillSendRequest(resource->identifier(), request,
                                    ResourceResponse(),
                                    resource->options().initiatorInfo);

  // TODO(shaochuan): Saving modified ResourceRequest back to |resource|, remove
  // once dispatchWillSendRequest() takes const ResourceRequest.
  // crbug.com/632580
  resource->setResourceRequest(request);

  // Resource requests from suborigins should not be intercepted by the service
  // worker of the physical origin. This has the effect that, for now,
  // suborigins do not work with service workers. See
  // https://w3c.github.io/webappsec-suborigins/.
  SecurityOrigin* sourceOrigin = context().getSecurityOrigin();
  if (sourceOrigin && sourceOrigin->hasSuborigin())
    request.setServiceWorkerMode(WebURLRequest::ServiceWorkerMode::None);

  ResourceLoader* loader = ResourceLoader::create(this, resource);
  if (resource->shouldBlockLoadEvent())
    m_loaders.insert(loader);
  else
    m_nonBlockingLoaders.insert(loader);

  storePerformanceTimingInitiatorInformation(resource);
  resource->setFetcherSecurityOrigin(sourceOrigin);

  loader->activateCacheAwareLoadingIfNeeded(request);
  loader->start(request);
  return true;
}

void ResourceFetcher::removeResourceLoader(ResourceLoader* loader) {
  DCHECK(loader);
  if (m_loaders.contains(loader))
    m_loaders.erase(loader);
  else if (m_nonBlockingLoaders.contains(loader))
    m_nonBlockingLoaders.erase(loader);
  else
    NOTREACHED();
}

void ResourceFetcher::stopFetching() {
  HeapVector<Member<ResourceLoader>> loadersToCancel;
  for (const auto& loader : m_nonBlockingLoaders)
    loadersToCancel.push_back(loader);
  for (const auto& loader : m_loaders)
    loadersToCancel.push_back(loader);

  for (const auto& loader : loadersToCancel) {
    if (m_loaders.contains(loader) || m_nonBlockingLoaders.contains(loader))
      loader->cancel();
  }
}

bool ResourceFetcher::isFetching() const {
  return !m_loaders.isEmpty();
}

void ResourceFetcher::setDefersLoading(bool defers) {
  for (const auto& loader : m_nonBlockingLoaders)
    loader->setDefersLoading(defers);
  for (const auto& loader : m_loaders)
    loader->setDefersLoading(defers);
}

void ResourceFetcher::updateAllImageResourcePriorities() {
  TRACE_EVENT0(
      "blink",
      "ResourceLoadPriorityOptimizer::updateAllImageResourcePriorities");
  for (const auto& documentResource : m_documentResources) {
    Resource* resource = documentResource.value.get();
    if (!resource || !resource->isImage() || !resource->isLoading())
      continue;

    ResourcePriority resourcePriority = resource->priorityFromObservers();
    ResourceLoadPriority resourceLoadPriority =
        computeLoadPriority(Resource::Image, resource->resourceRequest(),
                            resourcePriority.visibility);
    if (resourceLoadPriority == resource->resourceRequest().priority())
      continue;

    resource->didChangePriority(resourceLoadPriority,
                                resourcePriority.intraPriorityValue);
    network_instrumentation::resourcePrioritySet(resource->identifier(),
                                                 resourceLoadPriority);
    context().dispatchDidChangeResourcePriority(
        resource->identifier(), resourceLoadPriority,
        resourcePriority.intraPriorityValue);
  }
}

void ResourceFetcher::reloadLoFiImages() {
  for (const auto& documentResource : m_documentResources) {
    Resource* resource = documentResource.value.get();
    if (resource)
      resource->reloadIfLoFiOrPlaceholderImage(this, Resource::kReloadAlways);
  }
}

void ResourceFetcher::logPreloadStats(ClearPreloadsPolicy policy) {
  if (!m_preloads)
    return;
  unsigned scripts = 0;
  unsigned scriptMisses = 0;
  unsigned stylesheets = 0;
  unsigned stylesheetMisses = 0;
  unsigned images = 0;
  unsigned imageMisses = 0;
  unsigned fonts = 0;
  unsigned fontMisses = 0;
  unsigned medias = 0;
  unsigned mediaMisses = 0;
  unsigned textTracks = 0;
  unsigned textTrackMisses = 0;
  unsigned imports = 0;
  unsigned importMisses = 0;
  unsigned raws = 0;
  unsigned rawMisses = 0;
  for (const auto& resource : *m_preloads) {
    // Do not double count link rel preloads. These do not get cleared if the
    // ClearPreloadsPolicy is only clearing speculative markup preloads.
    if (resource->isLinkPreload() && policy == ClearSpeculativeMarkupPreloads) {
      continue;
    }
    int missCount =
        resource->getPreloadResult() == Resource::PreloadNotReferenced ? 1 : 0;
    switch (resource->getType()) {
      case Resource::Image:
        images++;
        imageMisses += missCount;
        break;
      case Resource::Script:
        scripts++;
        scriptMisses += missCount;
        break;
      case Resource::CSSStyleSheet:
        stylesheets++;
        stylesheetMisses += missCount;
        break;
      case Resource::Font:
        fonts++;
        fontMisses += missCount;
        break;
      case Resource::Media:
        medias++;
        mediaMisses += missCount;
        break;
      case Resource::TextTrack:
        textTracks++;
        textTrackMisses += missCount;
        break;
      case Resource::ImportResource:
        imports++;
        importMisses += missCount;
        break;
      case Resource::Raw:
        raws++;
        rawMisses += missCount;
        break;
      case Resource::Mock:
        // Do not count Resource::Mock because this type is only for testing.
        break;
      default:
        NOTREACHED();
    }
  }
  DEFINE_STATIC_LOCAL(CustomCountHistogram, imagePreloads,
                      ("PreloadScanner.Counts2.Image", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, imagePreloadMisses,
                      ("PreloadScanner.Counts2.Miss.Image", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, scriptPreloads,
                      ("PreloadScanner.Counts2.Script", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, scriptPreloadMisses,
                      ("PreloadScanner.Counts2.Miss.Script", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, stylesheetPreloads,
                      ("PreloadScanner.Counts2.CSSStyleSheet", 0, 100, 25));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, stylesheetPreloadMisses,
      ("PreloadScanner.Counts2.Miss.CSSStyleSheet", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, fontPreloads,
                      ("PreloadScanner.Counts2.Font", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, fontPreloadMisses,
                      ("PreloadScanner.Counts2.Miss.Font", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, mediaPreloads,
                      ("PreloadScanner.Counts2.Media", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, mediaPreloadMisses,
                      ("PreloadScanner.Counts2.Miss.Media", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, textTrackPreloads,
                      ("PreloadScanner.Counts2.TextTrack", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, textTrackPreloadMisses,
                      ("PreloadScanner.Counts2.Miss.TextTrack", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, importPreloads,
                      ("PreloadScanner.Counts2.Import", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, importPreloadMisses,
                      ("PreloadScanner.Counts2.Miss.Import", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, rawPreloads,
                      ("PreloadScanner.Counts2.Raw", 0, 100, 25));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, rawPreloadMisses,
                      ("PreloadScanner.Counts2.Miss.Raw", 0, 100, 25));
  if (images)
    imagePreloads.count(images);
  if (imageMisses)
    imagePreloadMisses.count(imageMisses);
  if (scripts)
    scriptPreloads.count(scripts);
  if (scriptMisses)
    scriptPreloadMisses.count(scriptMisses);
  if (stylesheets)
    stylesheetPreloads.count(stylesheets);
  if (stylesheetMisses)
    stylesheetPreloadMisses.count(stylesheetMisses);
  if (fonts)
    fontPreloads.count(fonts);
  if (fontMisses)
    fontPreloadMisses.count(fontMisses);
  if (medias)
    mediaPreloads.count(medias);
  if (mediaMisses)
    mediaPreloadMisses.count(mediaMisses);
  if (textTracks)
    textTrackPreloads.count(textTracks);
  if (textTrackMisses)
    textTrackPreloadMisses.count(textTrackMisses);
  if (imports)
    importPreloads.count(imports);
  if (importMisses)
    importPreloadMisses.count(importMisses);
  if (raws)
    rawPreloads.count(raws);
  if (rawMisses)
    rawPreloadMisses.count(rawMisses);
}

const ResourceLoaderOptions& ResourceFetcher::defaultResourceOptions() {
  DEFINE_STATIC_LOCAL(
      ResourceLoaderOptions, options,
      (BufferData, AllowStoredCredentials, ClientRequestedCredentials,
       CheckContentSecurityPolicy, DocumentContext));
  return options;
}

String ResourceFetcher::getCacheIdentifier() const {
  if (context().isControlledByServiceWorker())
    return String::number(context().serviceWorkerID());
  return MemoryCache::defaultCacheIdentifier();
}

void ResourceFetcher::emulateLoadStartedForInspector(
    Resource* resource,
    const KURL& url,
    WebURLRequest::RequestContext requestContext,
    const AtomicString& initiatorName) {
  if (cachedResource(url))
    return;
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(requestContext);
  FetchRequest request(resourceRequest, initiatorName, resource->options());
  context().canRequest(resource->getType(), resource->lastResourceRequest(),
                       resource->lastResourceRequest().url(), request.options(),
                       SecurityViolationReportingPolicy::Report,
                       request.getOriginRestriction());
  requestLoadStarted(resource->identifier(), resource, request,
                     ResourceLoadingFromCache);
}

ResourceFetcher::DeadResourceStatsRecorder::DeadResourceStatsRecorder()
    : m_useCount(0), m_revalidateCount(0), m_loadCount(0) {}

ResourceFetcher::DeadResourceStatsRecorder::~DeadResourceStatsRecorder() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, hitCountHistogram,
      new CustomCountHistogram("WebCore.ResourceFetcher.HitCount", 0, 1000,
                               50));
  hitCountHistogram.count(m_useCount);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, revalidateCountHistogram,
      new CustomCountHistogram("WebCore.ResourceFetcher.RevalidateCount", 0,
                               1000, 50));
  revalidateCountHistogram.count(m_revalidateCount);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, loadCountHistogram,
      new CustomCountHistogram("WebCore.ResourceFetcher.LoadCount", 0, 1000,
                               50));
  loadCountHistogram.count(m_loadCount);
}

void ResourceFetcher::DeadResourceStatsRecorder::update(
    RevalidationPolicy policy) {
  switch (policy) {
    case Reload:
    case Load:
      ++m_loadCount;
      return;
    case Revalidate:
      ++m_revalidateCount;
      return;
    case Use:
      ++m_useCount;
      return;
  }
}

DEFINE_TRACE(ResourceFetcher) {
  visitor->trace(m_context);
  visitor->trace(m_archive);
  visitor->trace(m_loaders);
  visitor->trace(m_nonBlockingLoaders);
  visitor->trace(m_documentResources);
  visitor->trace(m_preloads);
  visitor->trace(m_resourceTimingInfoMap);
}

}  // namespace blink
