/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/FetchRequest.h"

#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/ResourceFetcher.h"

namespace blink {

FetchRequest::FetchRequest(const ResourceRequest& resourceRequest, const AtomicString& initiator, const String& charset)
    : m_resourceRequest(resourceRequest)
    , m_charset(charset)
    , m_options(ResourceFetcher::defaultResourceOptions())
    , m_forPreload(false)
    , m_linkPreload(false)
    , m_preloadDiscoveryTime(0.0)
    , m_defer(NoDefer)
    , m_originRestriction(UseDefaultOriginRestrictionForType)
{
    m_options.initiatorInfo.name = initiator;
}

FetchRequest::FetchRequest(const ResourceRequest& resourceRequest, const AtomicString& initiator, const ResourceLoaderOptions& options)
    : m_resourceRequest(resourceRequest)
    , m_options(options)
    , m_forPreload(false)
    , m_linkPreload(false)
    , m_preloadDiscoveryTime(0.0)
    , m_defer(NoDefer)
    , m_originRestriction(UseDefaultOriginRestrictionForType)
{
    m_options.initiatorInfo.name = initiator;
}

FetchRequest::FetchRequest(const ResourceRequest& resourceRequest, const FetchInitiatorInfo& initiator)
    : m_resourceRequest(resourceRequest)
    , m_options(ResourceFetcher::defaultResourceOptions())
    , m_forPreload(false)
    , m_linkPreload(false)
    , m_preloadDiscoveryTime(0.0)
    , m_defer(NoDefer)
    , m_originRestriction(UseDefaultOriginRestrictionForType)
{
    m_options.initiatorInfo = initiator;
}

FetchRequest::~FetchRequest()
{
}

void FetchRequest::setCrossOriginAccessControl(SecurityOrigin* origin, CrossOriginAttributeValue crossOrigin)
{
    ASSERT(crossOrigin != CrossOriginAttributeNotSet);
    const bool useCredentials = crossOrigin == CrossOriginAttributeUseCredentials;
    const bool isSameOriginRequest = origin && origin->canRequestNoSuborigin(m_resourceRequest.url());

    // Currently FetchRequestMode and FetchCredentialsMode are only used when the request goes to Service Worker.
    m_resourceRequest.setFetchRequestMode(WebURLRequest::FetchRequestModeCORS);
    m_resourceRequest.setFetchCredentialsMode(useCredentials ? WebURLRequest::FetchCredentialsModeInclude : WebURLRequest::FetchCredentialsModeSameOrigin);

    m_options.allowCredentials = (isSameOriginRequest || useCredentials) ? AllowStoredCredentials : DoNotAllowStoredCredentials;
    m_options.corsEnabled = IsCORSEnabled;
    m_options.securityOrigin = origin;
    m_options.credentialsRequested = useCredentials ? ClientRequestedCredentials : ClientDidNotRequestCredentials;

    updateRequestForAccessControl(m_resourceRequest, origin, m_options.allowCredentials);
}


void FetchRequest::setResourceWidth(ResourceWidth resourceWidth)
{
    if (resourceWidth.isSet) {
        m_resourceWidth.width = resourceWidth.width;
        m_resourceWidth.isSet = true;
    }
}

void FetchRequest::setForPreload(bool forPreload, double discoveryTime)
{
    m_forPreload = forPreload;
    m_preloadDiscoveryTime = discoveryTime;
}

void FetchRequest::makeSynchronous()
{
    // Synchronous requests should always be max priority, lest they hang the renderer.
    m_resourceRequest.setPriority(ResourceLoadPriorityHighest);
    m_resourceRequest.setTimeoutInterval(10);
    m_options.synchronousPolicy = RequestSynchronously;
}

} // namespace blink
