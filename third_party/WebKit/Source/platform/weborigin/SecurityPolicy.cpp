/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/weborigin/SecurityPolicy.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/OriginAccessEntry.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PtrUtil.h"
#include "wtf/Threading.h"
#include "wtf/text/StringHash.h"
#include <memory>

namespace blink {

using OriginAccessWhiteList = Vector<OriginAccessEntry>;
using OriginAccessMap = HashMap<String, std::unique_ptr<OriginAccessWhiteList>>;
using OriginSet = HashSet<String>;

static OriginAccessMap& originAccessMap() {
  DEFINE_STATIC_LOCAL(OriginAccessMap, originAccessMap, ());
  return originAccessMap;
}

static OriginSet& trustworthyOriginSet() {
  DEFINE_STATIC_LOCAL(OriginSet, trustworthyOriginSet, ());
  return trustworthyOriginSet;
}

void SecurityPolicy::init() {
  originAccessMap();
  trustworthyOriginSet();
}

bool SecurityPolicy::shouldHideReferrer(const KURL& url, const KURL& referrer) {
  bool referrerIsSecureURL = referrer.protocolIs("https");
  bool schemeIsAllowed =
      SchemeRegistry::shouldTreatURLSchemeAsAllowedForReferrer(
          referrer.protocol());

  if (!schemeIsAllowed)
    return true;

  if (!referrerIsSecureURL)
    return false;

  bool URLIsSecureURL = url.protocolIs("https");

  return !URLIsSecureURL;
}

Referrer SecurityPolicy::generateReferrer(ReferrerPolicy referrerPolicy,
                                          const KURL& url,
                                          const String& referrer) {
  ReferrerPolicy referrerPolicyNoDefault = referrerPolicy;
  if (referrerPolicyNoDefault == ReferrerPolicyDefault) {
    if (RuntimeEnabledFeatures::reducedReferrerGranularityEnabled()) {
      referrerPolicyNoDefault =
          ReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    } else {
      referrerPolicyNoDefault = ReferrerPolicyNoReferrerWhenDowngrade;
    }
  }
  if (referrer == Referrer::noReferrer())
    return Referrer(Referrer::noReferrer(), referrerPolicyNoDefault);
  ASSERT(!referrer.isEmpty());

  KURL referrerUrl = KURL(KURL(), referrer);
  String scheme = referrerUrl.protocol();
  if (!SchemeRegistry::shouldTreatURLSchemeAsAllowedForReferrer(scheme))
    return Referrer(Referrer::noReferrer(), referrerPolicyNoDefault);

  if (SecurityOrigin::shouldUseInnerURL(url))
    return Referrer(Referrer::noReferrer(), referrerPolicyNoDefault);

  switch (referrerPolicyNoDefault) {
    case ReferrerPolicyNever:
      return Referrer(Referrer::noReferrer(), referrerPolicyNoDefault);
    case ReferrerPolicyAlways:
      return Referrer(referrer, referrerPolicyNoDefault);
    case ReferrerPolicyOrigin: {
      String origin = SecurityOrigin::create(referrerUrl)->toString();
      // A security origin is not a canonical URL as it lacks a path. Add /
      // to turn it into a canonical URL we can use as referrer.
      return Referrer(origin + "/", referrerPolicyNoDefault);
    }
    case ReferrerPolicyOriginWhenCrossOrigin: {
      RefPtr<SecurityOrigin> referrerOrigin =
          SecurityOrigin::create(referrerUrl);
      RefPtr<SecurityOrigin> urlOrigin = SecurityOrigin::create(url);
      if (!urlOrigin->isSameSchemeHostPort(referrerOrigin.get())) {
        String origin = referrerOrigin->toString();
        return Referrer(origin + "/", referrerPolicyNoDefault);
      }
      break;
    }
    case ReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin: {
      // If the flag is enabled, and we're dealing with a cross-origin request,
      // strip it.  Otherwise fall through to NoReferrerWhenDowngrade behavior.
      RefPtr<SecurityOrigin> referrerOrigin =
          SecurityOrigin::create(referrerUrl);
      RefPtr<SecurityOrigin> urlOrigin = SecurityOrigin::create(url);
      if (!urlOrigin->isSameSchemeHostPort(referrerOrigin.get())) {
        String origin = referrerOrigin->toString();
        return Referrer(shouldHideReferrer(url, referrerUrl)
                            ? Referrer::noReferrer()
                            : origin + "/",
                        referrerPolicyNoDefault);
      }
      break;
    }
    case ReferrerPolicyNoReferrerWhenDowngrade:
      break;
    case ReferrerPolicyDefault:
      ASSERT_NOT_REACHED();
      break;
  }

  return Referrer(
      shouldHideReferrer(url, referrerUrl) ? Referrer::noReferrer() : referrer,
      referrerPolicyNoDefault);
}

void SecurityPolicy::addOriginTrustworthyWhiteList(
    PassRefPtr<SecurityOrigin> origin) {
#if DCHECK_IS_ON()
  // Must be called before we start other threads.
  DCHECK(WTF::isBeforeThreadCreated());
#endif
  if (origin->isUnique())
    return;
  trustworthyOriginSet().insert(origin->toRawString());
}

bool SecurityPolicy::isOriginWhiteListedTrustworthy(
    const SecurityOrigin& origin) {
  // Early return if there are no whitelisted origins to avoid unnecessary
  // allocations, copies, and frees.
  if (origin.isUnique() || trustworthyOriginSet().isEmpty())
    return false;
  return trustworthyOriginSet().contains(origin.toRawString());
}

bool SecurityPolicy::isUrlWhiteListedTrustworthy(const KURL& url) {
  // Early return to avoid initializing the SecurityOrigin.
  if (trustworthyOriginSet().isEmpty())
    return false;
  return isOriginWhiteListedTrustworthy(*SecurityOrigin::create(url).get());
}

bool SecurityPolicy::isAccessWhiteListed(const SecurityOrigin* activeOrigin,
                                         const SecurityOrigin* targetOrigin) {
  if (OriginAccessWhiteList* list =
          originAccessMap().at(activeOrigin->toString())) {
    for (size_t i = 0; i < list->size(); ++i) {
      if (list->at(i).matchesOrigin(*targetOrigin) !=
          OriginAccessEntry::DoesNotMatchOrigin)
        return true;
    }
  }
  return false;
}

bool SecurityPolicy::isAccessToURLWhiteListed(
    const SecurityOrigin* activeOrigin,
    const KURL& url) {
  RefPtr<SecurityOrigin> targetOrigin = SecurityOrigin::create(url);
  return isAccessWhiteListed(activeOrigin, targetOrigin.get());
}

void SecurityPolicy::addOriginAccessWhitelistEntry(
    const SecurityOrigin& sourceOrigin,
    const String& destinationProtocol,
    const String& destinationDomain,
    bool allowDestinationSubdomains) {
  ASSERT(isMainThread());
  ASSERT(!sourceOrigin.isUnique());
  if (sourceOrigin.isUnique())
    return;

  String sourceString = sourceOrigin.toString();
  OriginAccessMap::AddResult result =
      originAccessMap().insert(sourceString, nullptr);
  if (result.isNewEntry)
    result.storedValue->value = WTF::wrapUnique(new OriginAccessWhiteList);

  OriginAccessWhiteList* list = result.storedValue->value.get();
  list->push_back(OriginAccessEntry(
      destinationProtocol, destinationDomain,
      allowDestinationSubdomains ? OriginAccessEntry::AllowSubdomains
                                 : OriginAccessEntry::DisallowSubdomains));
}

void SecurityPolicy::removeOriginAccessWhitelistEntry(
    const SecurityOrigin& sourceOrigin,
    const String& destinationProtocol,
    const String& destinationDomain,
    bool allowDestinationSubdomains) {
  ASSERT(isMainThread());
  ASSERT(!sourceOrigin.isUnique());
  if (sourceOrigin.isUnique())
    return;

  String sourceString = sourceOrigin.toString();
  OriginAccessMap& map = originAccessMap();
  OriginAccessMap::iterator it = map.find(sourceString);
  if (it == map.end())
    return;

  OriginAccessWhiteList* list = it->value.get();
  size_t index = list->find(OriginAccessEntry(
      destinationProtocol, destinationDomain,
      allowDestinationSubdomains ? OriginAccessEntry::AllowSubdomains
                                 : OriginAccessEntry::DisallowSubdomains));

  if (index == kNotFound)
    return;

  list->remove(index);

  if (list->isEmpty())
    map.remove(it);
}

void SecurityPolicy::resetOriginAccessWhitelists() {
  ASSERT(isMainThread());
  originAccessMap().clear();
}

bool SecurityPolicy::referrerPolicyFromString(
    const String& policy,
    ReferrerPolicyLegacyKeywordsSupport legacyKeywordsSupport,
    ReferrerPolicy* result) {
  DCHECK(!policy.isNull());
  bool supportLegacyKeywords =
      (legacyKeywordsSupport == SupportReferrerPolicyLegacyKeywords);

  if (equalIgnoringASCIICase(policy, "no-referrer") ||
      (supportLegacyKeywords && equalIgnoringASCIICase(policy, "never"))) {
    *result = ReferrerPolicyNever;
    return true;
  }
  if (equalIgnoringASCIICase(policy, "unsafe-url") ||
      (supportLegacyKeywords && equalIgnoringASCIICase(policy, "always"))) {
    *result = ReferrerPolicyAlways;
    return true;
  }
  if (equalIgnoringASCIICase(policy, "origin")) {
    *result = ReferrerPolicyOrigin;
    return true;
  }
  if (equalIgnoringASCIICase(policy, "origin-when-cross-origin") ||
      (supportLegacyKeywords &&
       equalIgnoringASCIICase(policy, "origin-when-crossorigin"))) {
    *result = ReferrerPolicyOriginWhenCrossOrigin;
    return true;
  }
  if (equalIgnoringASCIICase(policy, "no-referrer-when-downgrade") ||
      (supportLegacyKeywords && equalIgnoringASCIICase(policy, "default"))) {
    *result = ReferrerPolicyNoReferrerWhenDowngrade;
    return true;
  }
  return false;
}

bool SecurityPolicy::referrerPolicyFromHeaderValue(
    const String& headerValue,
    ReferrerPolicyLegacyKeywordsSupport legacyKeywordsSupport,
    ReferrerPolicy* result) {
  ReferrerPolicy referrerPolicy = ReferrerPolicyDefault;

  Vector<String> tokens;
  headerValue.split(',', true, tokens);
  for (const auto& token : tokens) {
    ReferrerPolicy currentResult;
    if (SecurityPolicy::referrerPolicyFromString(token, legacyKeywordsSupport,
                                                 &currentResult)) {
      referrerPolicy = currentResult;
    }
  }

  if (referrerPolicy == ReferrerPolicyDefault)
    return false;

  *result = referrerPolicy;
  return true;
}

}  // namespace blink
