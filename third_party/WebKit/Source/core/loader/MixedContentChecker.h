/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef MixedContentChecker_h
#define MixedContentChecker_h

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "public/platform/WebMixedContentContextType.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Frame;
class LocalFrame;
class KURL;
class ResourceResponse;
class SecurityOrigin;

// Checks resource loads for mixed content. If PlzNavigate is enabled then this
// class only checks for sub-resource loads while frame-level loads are
// delegated to the browser where they are checked by
// MixedContentNavigationThrottle. Changes to this class might need to be
// reflected on its browser counterpart.
//
// Current mixed content W3C draft that drives this implementation:
// https://w3c.github.io/webappsec-mixed-content/
class CORE_EXPORT MixedContentChecker final {
  WTF_MAKE_NONCOPYABLE(MixedContentChecker);
  DISALLOW_NEW();

 public:
  static bool shouldBlockFetch(LocalFrame*,
                               WebURLRequest::RequestContext,
                               WebURLRequest::FrameType,
                               ResourceRequest::RedirectStatus,
                               const KURL&,
                               SecurityViolationReportingPolicy =
                                   SecurityViolationReportingPolicy::Report);
  static bool shouldBlockFetch(LocalFrame* frame,
                               const ResourceRequest& request,
                               const KURL& url,
                               SecurityViolationReportingPolicy status =
                                   SecurityViolationReportingPolicy::Report) {
    return shouldBlockFetch(frame, request.requestContext(),
                            request.frameType(), request.redirectStatus(), url,
                            status);
  }

  static bool shouldBlockWebSocket(
      LocalFrame*,
      const KURL&,
      SecurityViolationReportingPolicy =
          SecurityViolationReportingPolicy::Report);

  static bool isMixedContent(SecurityOrigin*, const KURL&);
  static bool isMixedFormAction(LocalFrame*,
                                const KURL&,
                                SecurityViolationReportingPolicy =
                                    SecurityViolationReportingPolicy::Report);

  static void checkMixedPrivatePublic(LocalFrame*,
                                      const AtomicString& resourceIPAddress);

  static WebMixedContentContextType contextTypeForInspector(
      LocalFrame*,
      const ResourceRequest&);

  // Returns the frame that should be considered the effective frame
  // for a mixed content check for the given frame type.
  static Frame* effectiveFrameForFrameType(LocalFrame*,
                                           WebURLRequest::FrameType);

  static void handleCertificateError(LocalFrame*,
                                     const ResourceResponse&,
                                     WebURLRequest::FrameType,
                                     WebURLRequest::RequestContext);

  // Receive information about mixed content found externally.
  static void mixedContentFound(LocalFrame*,
                                const KURL& mainResourceUrl,
                                const KURL& mixedContentUrl,
                                WebURLRequest::RequestContext,
                                bool wasAllowed,
                                bool hadRedirect);

 private:
  FRIEND_TEST_ALL_PREFIXES(MixedContentCheckerTest, HandleCertificateError);

  static Frame* inWhichFrameIsContentMixed(Frame*,
                                           WebURLRequest::FrameType,
                                           const KURL&);

  static void logToConsoleAboutFetch(LocalFrame*,
                                     const KURL&,
                                     const KURL&,
                                     WebURLRequest::RequestContext,
                                     bool allowed);
  static void logToConsoleAboutWebSocket(LocalFrame*,
                                         const KURL&,
                                         const KURL&,
                                         bool allowed);
  static void count(Frame*, WebURLRequest::RequestContext);
};

}  // namespace blink

#endif  // MixedContentChecker_h
