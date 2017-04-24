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

#include "web/WebDataSourceImpl.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/loader/SubresourceFilter.h"
#include "public/platform/WebDocumentSubresourceFilter.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "wtf/PtrUtil.h"

namespace blink {

WebDataSourceImpl* WebDataSourceImpl::create(
    LocalFrame* frame,
    const ResourceRequest& request,
    const SubstituteData& data,
    ClientRedirectPolicy clientRedirectPolicy) {
  DCHECK(frame);

  return new WebDataSourceImpl(frame, request, data, clientRedirectPolicy);
}

const WebURLRequest& WebDataSourceImpl::originalRequest() const {
  return m_originalRequestWrapper;
}

const WebURLRequest& WebDataSourceImpl::getRequest() const {
  return m_requestWrapper;
}

const WebURLResponse& WebDataSourceImpl::response() const {
  return m_responseWrapper;
}

bool WebDataSourceImpl::hasUnreachableURL() const {
  return !DocumentLoader::unreachableURL().isEmpty();
}

WebURL WebDataSourceImpl::unreachableURL() const {
  return DocumentLoader::unreachableURL();
}

void WebDataSourceImpl::appendRedirect(const WebURL& url) {
  DocumentLoader::appendRedirect(url);
}

void WebDataSourceImpl::updateNavigation(double redirectStartTime,
                                         double redirectEndTime,
                                         double fetchStartTime,
                                         bool hasRedirect) {
  // Updates the redirection timing if there is at least one redirection
  // (between two URLs).
  if (hasRedirect) {
    timing().setRedirectStart(redirectStartTime);
    timing().setRedirectEnd(redirectEndTime);
  }
  timing().setFetchStart(fetchStartTime);
}

void WebDataSourceImpl::redirectChain(WebVector<WebURL>& result) const {
  result.assign(m_redirectChain);
}

bool WebDataSourceImpl::isClientRedirect() const {
  return DocumentLoader::isClientRedirect();
}

bool WebDataSourceImpl::replacesCurrentHistoryItem() const {
  return DocumentLoader::replacesCurrentHistoryItem();
}

WebNavigationType WebDataSourceImpl::navigationType() const {
  return toWebNavigationType(DocumentLoader::getNavigationType());
}

WebDataSource::ExtraData* WebDataSourceImpl::getExtraData() const {
  return m_extraData.get();
}

void WebDataSourceImpl::setExtraData(ExtraData* extraData) {
  // extraData can't be a std::unique_ptr because setExtraData is a WebKit API
  // function.
  m_extraData = WTF::wrapUnique(extraData);
}

void WebDataSourceImpl::setNavigationStartTime(double navigationStart) {
  timing().setNavigationStart(navigationStart);
}

WebNavigationType WebDataSourceImpl::toWebNavigationType(NavigationType type) {
  switch (type) {
    case NavigationTypeLinkClicked:
      return WebNavigationTypeLinkClicked;
    case NavigationTypeFormSubmitted:
      return WebNavigationTypeFormSubmitted;
    case NavigationTypeBackForward:
      return WebNavigationTypeBackForward;
    case NavigationTypeReload:
      return WebNavigationTypeReload;
    case NavigationTypeFormResubmitted:
      return WebNavigationTypeFormResubmitted;
    case NavigationTypeOther:
    default:
      return WebNavigationTypeOther;
  }
}

WebDataSourceImpl::WebDataSourceImpl(LocalFrame* frame,
                                     const ResourceRequest& request,
                                     const SubstituteData& data,
                                     ClientRedirectPolicy clientRedirectPolicy)
    : DocumentLoader(frame, request, data, clientRedirectPolicy),
      m_originalRequestWrapper(DocumentLoader::originalRequest()),
      m_requestWrapper(DocumentLoader::getRequest()),
      m_responseWrapper(DocumentLoader::response()) {}

WebDataSourceImpl::~WebDataSourceImpl() {
  // Verify that detachFromFrame() has been called.
  DCHECK(!m_extraData);
}

void WebDataSourceImpl::detachFromFrame() {
  DocumentLoader::detachFromFrame();
  m_extraData.reset();
}

void WebDataSourceImpl::setSubresourceFilter(
    WebDocumentSubresourceFilter* subresourceFilter) {
  DocumentLoader::setSubresourceFilter(
      SubresourceFilter::create(this, WTF::wrapUnique(subresourceFilter)));
}

DEFINE_TRACE(WebDataSourceImpl) {
  DocumentLoader::trace(visitor);
}

}  // namespace blink
