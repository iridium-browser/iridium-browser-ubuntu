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

#ifndef FetchRequest_h
#define FetchRequest_h

#include "platform/CrossOriginAttributeValue.h"
#include "platform/PlatformExport.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/network/ResourceRequest.h"
#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"

namespace blink {
class SecurityOrigin;

// A FetchRequest is a "parameter object" for ResourceFetcher::requestResource
// to avoid the method having too many arguments.
class PLATFORM_EXPORT FetchRequest {
  STACK_ALLOCATED();

 public:
  enum DeferOption { NoDefer, LazyLoad, IdleLoad };
  enum OriginRestriction {
    UseDefaultOriginRestrictionForType,
    RestrictToSameOrigin,
    NoOriginRestriction
  };
  enum PlaceholderImageRequestType {
    DisallowPlaceholder = 0,  // The requested image must not be a placeholder.
    AllowPlaceholder,         // The image is allowed to be a placeholder.
  };
  // TODO(toyoshim): Consider to define an enum for preload options, and use it
  // instead of bool in this class, FrameFetchContext, and so on. If it is
  // reasonable, we try merging m_speculativePreload and m_linkPreload into one
  // enum type. See https://crbug.com/675883.

  struct ResourceWidth {
    DISALLOW_NEW();
    float width;
    bool isSet;

    ResourceWidth() : width(0), isSet(false) {}
  };

  FetchRequest(const ResourceRequest&,
               const AtomicString& initiator,
               const String& charset = String());
  FetchRequest(const ResourceRequest&,
               const AtomicString& initiator,
               const ResourceLoaderOptions&);
  FetchRequest(const ResourceRequest&, const FetchInitiatorInfo&);
  ~FetchRequest();

  ResourceRequest& mutableResourceRequest() { return m_resourceRequest; }
  const ResourceRequest& resourceRequest() const { return m_resourceRequest; }
  const KURL& url() const { return m_resourceRequest.url(); }

  const String& charset() const { return m_charset; }
  void setCharset(const String& charset) { m_charset = charset; }

  const ResourceLoaderOptions& options() const { return m_options; }

  DeferOption defer() const { return m_defer; }
  void setDefer(DeferOption defer) { m_defer = defer; }

  ResourceWidth getResourceWidth() const { return m_resourceWidth; }
  void setResourceWidth(ResourceWidth);

  ClientHintsPreferences& clientHintsPreferences() {
    return m_clientHintPreferences;
  }

  bool isSpeculativePreload() const { return m_speculativePreload; }
  void setSpeculativePreload(bool speculativePreload, double discoveryTime = 0);

  double preloadDiscoveryTime() { return m_preloadDiscoveryTime; }

  bool isLinkPreload() const { return m_linkPreload; }
  void setLinkPreload(bool isLinkPreload) { m_linkPreload = isLinkPreload; }

  void setContentSecurityCheck(
      ContentSecurityPolicyDisposition contentSecurityPolicyOption) {
    m_options.contentSecurityPolicyOption = contentSecurityPolicyOption;
  }
  void setCrossOriginAccessControl(SecurityOrigin*, CrossOriginAttributeValue);
  OriginRestriction getOriginRestriction() const { return m_originRestriction; }
  void setOriginRestriction(OriginRestriction restriction) {
    m_originRestriction = restriction;
  }
  const IntegrityMetadataSet integrityMetadata() const {
    return m_options.integrityMetadata;
  }
  void setIntegrityMetadata(const IntegrityMetadataSet& metadata) {
    m_options.integrityMetadata = metadata;
  }

  String contentSecurityPolicyNonce() const {
    return m_options.contentSecurityPolicyNonce;
  }
  void setContentSecurityPolicyNonce(const String& nonce) {
    m_options.contentSecurityPolicyNonce = nonce;
  }

  void setParserDisposition(ParserDisposition parserDisposition) {
    m_options.parserDisposition = parserDisposition;
  }

  void setCacheAwareLoadingEnabled(
      CacheAwareLoadingEnabled cacheAwareLoadingEnabled) {
    m_options.cacheAwareLoadingEnabled = cacheAwareLoadingEnabled;
  }

  void makeSynchronous();

  PlaceholderImageRequestType placeholderImageRequestType() const {
    return m_placeholderImageRequestType;
  }

  // Configures the request to load an image placeholder if the request is
  // eligible (e.g. the url's protocol is HTTP, etc.). If this request is
  // non-eligible, this method doesn't modify the ResourceRequest. Calling this
  // method sets m_placeholderImageRequestType to the appropriate value.
  void setAllowImagePlaceholder();

 private:
  ResourceRequest m_resourceRequest;
  String m_charset;
  ResourceLoaderOptions m_options;
  bool m_speculativePreload;
  bool m_linkPreload;
  double m_preloadDiscoveryTime;
  DeferOption m_defer;
  OriginRestriction m_originRestriction;
  ResourceWidth m_resourceWidth;
  ClientHintsPreferences m_clientHintPreferences;
  PlaceholderImageRequestType m_placeholderImageRequestType;
};

}  // namespace blink

#endif
