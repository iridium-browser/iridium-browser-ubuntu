/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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

#include "core/frame/csp/ContentSecurityPolicy.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/SandboxFlags.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/EventQueue.h"
#include "core/events/SecurityPolicyViolationEvent.h"
#include "core/fetch/IntegrityMetadata.h"
#include "core/frame/FrameClient.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/CSPDirectiveList.h"
#include "core/frame/csp/CSPSource.h"
#include "core/frame/csp/MediaListDirective.h"
#include "core/frame/csp/SourceListDirective.h"
#include "core/html/HTMLScriptElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/PingLoader.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/json/JSONValues.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebAddressSpace.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/NotFound.h"
#include "wtf/PtrUtil.h"
#include "wtf/StringHasher.h"
#include "wtf/text/ParsingUtilities.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include <memory>

namespace blink {

bool ContentSecurityPolicy::isNonceableElement(const Element* element) {
  if (RuntimeEnabledFeatures::hideNonceContentAttributeEnabled() &&
      isHTMLScriptElement(element)) {
    if (toHTMLScriptElement(element)->nonce().isNull())
      return false;
  } else if (!element->fastHasAttribute(HTMLNames::nonceAttr)) {
    return false;
  }

  bool nonceable = true;

  // To prevent an attacker from hijacking an existing nonce via a dangling
  // markup injection, we walk through the attributes of each nonced script
  // element: if their names or values contain "<script" or "<style", we won't
  // apply the nonce when loading script.
  //
  // See http://blog.innerht.ml/csp-2015/#danglingmarkupinjection for an example
  // of the kind of attack this is aimed at mitigating.
  static const char scriptString[] = "<script";
  static const char styleString[] = "<style";
  for (const Attribute& attr : element->attributes()) {
    AtomicString name = attr.localName().lowerASCII();
    AtomicString value = attr.value().lowerASCII();
    if (name.find(scriptString) != WTF::kNotFound ||
        name.find(styleString) != WTF::kNotFound ||
        value.find(scriptString) != WTF::kNotFound ||
        value.find(styleString) != WTF::kNotFound) {
      nonceable = false;
      break;
    }
  }

  UseCounter::count(
      element->document(),
      nonceable ? UseCounter::CleanScriptElementWithNonce
                : UseCounter::PotentiallyInjectedScriptElementWithNonce);

  // This behavior is locked behind the experimental flag for the moment; if we
  // decide to ship it, drop this check. https://crbug.com/639293
  return !RuntimeEnabledFeatures::
             experimentalContentSecurityPolicyFeaturesEnabled() ||
         nonceable;
}

static UseCounter::Feature getUseCounterType(
    ContentSecurityPolicyHeaderType type) {
  switch (type) {
    case ContentSecurityPolicyHeaderTypeEnforce:
      return UseCounter::ContentSecurityPolicy;
    case ContentSecurityPolicyHeaderTypeReport:
      return UseCounter::ContentSecurityPolicyReportOnly;
  }
  ASSERT_NOT_REACHED();
  return UseCounter::NumberOfFeatures;
}

ContentSecurityPolicy::ContentSecurityPolicy()
    : m_executionContext(nullptr),
      m_overrideInlineStyleAllowed(false),
      m_scriptHashAlgorithmsUsed(ContentSecurityPolicyHashAlgorithmNone),
      m_styleHashAlgorithmsUsed(ContentSecurityPolicyHashAlgorithmNone),
      m_sandboxMask(0),
      m_treatAsPublicAddress(false),
      m_insecureRequestPolicy(kLeaveInsecureRequestsAlone) {}

void ContentSecurityPolicy::bindToExecutionContext(
    ExecutionContext* executionContext) {
  m_executionContext = executionContext;
  applyPolicySideEffectsToExecutionContext();
}

void ContentSecurityPolicy::setupSelf(const SecurityOrigin& securityOrigin) {
  // Ensure that 'self' processes correctly.
  m_selfProtocol = securityOrigin.protocol();
  m_selfSource = new CSPSource(this, m_selfProtocol, securityOrigin.host(),
                               securityOrigin.port(), String(),
                               CSPSource::NoWildcard, CSPSource::NoWildcard);
}

void ContentSecurityPolicy::applyPolicySideEffectsToExecutionContext() {
  DCHECK(m_executionContext &&
         m_executionContext->securityContext().getSecurityOrigin());

  setupSelf(*m_executionContext->securityContext().getSecurityOrigin());

  // If we're in a Document, set mixed content checking and sandbox
  // flags, then dump all the parsing error messages, then poke at histograms.
  if (Document* document = this->document()) {
    if (m_sandboxMask != SandboxNone) {
      UseCounter::count(document, UseCounter::SandboxViaCSP);
      document->enforceSandboxFlags(m_sandboxMask);
    }
    if (m_treatAsPublicAddress)
      document->setAddressSpace(WebAddressSpacePublic);

    document->enforceInsecureRequestPolicy(m_insecureRequestPolicy);
    if (m_insecureRequestPolicy & kUpgradeInsecureRequests) {
      UseCounter::count(document, UseCounter::UpgradeInsecureRequestsEnabled);
      if (!document->url().host().isEmpty())
        document->addInsecureNavigationUpgrade(
            document->url().host().impl()->hash());
    }

    for (const auto& consoleMessage : m_consoleMessages)
      m_executionContext->addConsoleMessage(consoleMessage);
    m_consoleMessages.clear();

    for (const auto& policy : m_policies) {
      UseCounter::count(*document, getUseCounterType(policy->headerType()));
      if (policy->allowDynamic())
        UseCounter::count(*document, UseCounter::CSPWithStrictDynamic);
    }
  }

  // We disable 'eval()' even in the case of report-only policies, and rely on
  // the check in the V8Initializer::codeGenerationCheckCallbackInMainThread
  // callback to determine whether the call should execute or not.
  if (!m_disableEvalErrorMessage.isNull())
    m_executionContext->disableEval(m_disableEvalErrorMessage);
}

ContentSecurityPolicy::~ContentSecurityPolicy() {}

DEFINE_TRACE(ContentSecurityPolicy) {
  visitor->trace(m_executionContext);
  visitor->trace(m_policies);
  visitor->trace(m_consoleMessages);
  visitor->trace(m_selfSource);
}

Document* ContentSecurityPolicy::document() const {
  return (m_executionContext && m_executionContext->isDocument())
             ? toDocument(m_executionContext)
             : nullptr;
}

void ContentSecurityPolicy::copyStateFrom(const ContentSecurityPolicy* other) {
  ASSERT(m_policies.isEmpty());
  for (const auto& policy : other->m_policies)
    addAndReportPolicyFromHeaderValue(policy->header(), policy->headerType(),
                                      policy->headerSource());
}

void ContentSecurityPolicy::copyPluginTypesFrom(
    const ContentSecurityPolicy* other) {
  for (const auto& policy : other->m_policies) {
    if (policy->hasPluginTypes()) {
      addAndReportPolicyFromHeaderValue(policy->pluginTypesText(),
                                        policy->headerType(),
                                        policy->headerSource());
    }
  }
}

void ContentSecurityPolicy::didReceiveHeaders(
    const ContentSecurityPolicyResponseHeaders& headers) {
  if (!headers.contentSecurityPolicy().isEmpty())
    addAndReportPolicyFromHeaderValue(headers.contentSecurityPolicy(),
                                      ContentSecurityPolicyHeaderTypeEnforce,
                                      ContentSecurityPolicyHeaderSourceHTTP);
  if (!headers.contentSecurityPolicyReportOnly().isEmpty())
    addAndReportPolicyFromHeaderValue(headers.contentSecurityPolicyReportOnly(),
                                      ContentSecurityPolicyHeaderTypeReport,
                                      ContentSecurityPolicyHeaderSourceHTTP);
}

void ContentSecurityPolicy::didReceiveHeader(
    const String& header,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source) {
  addAndReportPolicyFromHeaderValue(header, type, source);

  // This might be called after we've been bound to an execution context. For
  // example, a <meta> element might be injected after page load.
  if (m_executionContext)
    applyPolicySideEffectsToExecutionContext();
}

bool ContentSecurityPolicy::shouldEnforceEmbeddersPolicy(
    const ResourceResponse& response,
    SecurityOrigin* parentOrigin) {
  if (response.url().isEmpty() || response.url().protocolIsAbout() ||
      response.url().protocolIsData() || response.url().protocolIs("blob") ||
      response.url().protocolIs("filesystem")) {
    return true;
  }

  if (parentOrigin->canAccess(SecurityOrigin::create(response.url()).get()))
    return true;

  String header = response.httpHeaderField(HTTPNames::Allow_CSP_From);
  header = header.stripWhiteSpace();
  if (header == "*")
    return true;
  if (RefPtr<SecurityOrigin> childOrigin =
          SecurityOrigin::createFromString(header)) {
    return parentOrigin->canAccess(childOrigin.get());
  }

  return false;
}

void ContentSecurityPolicy::addPolicyFromHeaderValue(
    const String& header,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source) {
  // If this is a report-only header inside a <meta> element, bail out.
  if (source == ContentSecurityPolicyHeaderSourceMeta &&
      type == ContentSecurityPolicyHeaderTypeReport) {
    reportReportOnlyInMeta(header);
    return;
  }

  Vector<UChar> characters;
  header.appendTo(characters);

  const UChar* begin = characters.data();
  const UChar* end = begin + characters.size();

  // RFC2616, section 4.2 specifies that headers appearing multiple times can
  // be combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  const UChar* position = begin;
  while (position < end) {
    skipUntil<UChar>(position, end, ',');

    // header1,header2 OR header1
    //        ^                  ^
    Member<CSPDirectiveList> policy =
        CSPDirectiveList::create(this, begin, position, type, source);

    if (!policy->allowEval(0, SuppressReport) &&
        m_disableEvalErrorMessage.isNull())
      m_disableEvalErrorMessage = policy->evalDisabledErrorMessage();

    m_policies.push_back(policy.release());

    // Skip the comma, and begin the next header from the current position.
    ASSERT(position == end || *position == ',');
    skipExactly<UChar>(position, end, ',');
    begin = position;
  }
}

void ContentSecurityPolicy::reportAccumulatedHeaders(
    FrameLoaderClient* client) const {
  // Notify the embedder about headers that have accumulated before the
  // navigation got committed.  See comments in
  // addAndReportPolicyFromHeaderValue for more details and context.
  DCHECK(client);
  for (const auto& policy : m_policies) {
    client->didAddContentSecurityPolicy(policy->header(), policy->headerType(),
                                        policy->headerSource());
  }
}

void ContentSecurityPolicy::addAndReportPolicyFromHeaderValue(
    const String& header,
    ContentSecurityPolicyHeaderType type,
    ContentSecurityPolicyHeaderSource source) {
  // Notify about the new header, so that it can be reported back to the
  // browser process.  This is needed in order to:
  // 1) replicate CSP directives (i.e. frame-src) to OOPIFs (only for now /
  // short-term).
  // 2) enforce CSP in the browser process (not yet / long-term - see
  // https://crbug.com/376522).
  if (document() && document()->frame())
    document()->frame()->client()->didAddContentSecurityPolicy(header, type,
                                                               source);

  addPolicyFromHeaderValue(header, type, source);
}

void ContentSecurityPolicy::setOverrideAllowInlineStyle(bool value) {
  m_overrideInlineStyleAllowed = value;
}

void ContentSecurityPolicy::setOverrideURLForSelf(const KURL& url) {
  // Create a temporary CSPSource so that 'self' expressions can be resolved
  // before we bind to an execution context (for 'frame-ancestor' resolution,
  // for example). This CSPSource will be overwritten when we bind this object
  // to an execution context.
  RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
  m_selfProtocol = origin->protocol();
  m_selfSource =
      new CSPSource(this, m_selfProtocol, origin->host(), origin->port(),
                    String(), CSPSource::NoWildcard, CSPSource::NoWildcard);
}

std::unique_ptr<Vector<CSPHeaderAndType>> ContentSecurityPolicy::headers()
    const {
  std::unique_ptr<Vector<CSPHeaderAndType>> headers =
      WTF::wrapUnique(new Vector<CSPHeaderAndType>);
  for (const auto& policy : m_policies) {
    CSPHeaderAndType headerAndType(policy->header(), policy->headerType());
    headers->push_back(headerAndType);
  }
  return headers;
}

template <bool (CSPDirectiveList::*allowed)(
    ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    ContentSecurityPolicy::ReportingStatus reportingStatus) {
  bool isAllowed = true;
  for (const auto& policy : policies)
    isAllowed &= (policy.get()->*allowed)(reportingStatus);
  return isAllowed;
}

template <bool (CSPDirectiveList::*allowed)(
    ScriptState* scriptState,
    ContentSecurityPolicy::ReportingStatus,
    ContentSecurityPolicy::ExceptionStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    ScriptState* scriptState,
                    ContentSecurityPolicy::ReportingStatus reportingStatus,
                    ContentSecurityPolicy::ExceptionStatus exceptionStatus) {
  bool isAllowed = true;
  for (const auto& policy : policies)
    isAllowed &=
        (policy.get()->*allowed)(scriptState, reportingStatus, exceptionStatus);
  return isAllowed;
}

template <bool (CSPDirectiveList::*allowed)(
    Element*,
    const String&,
    const WTF::OrdinalNumber&,
    ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    Element* element,
                    const String& contextURL,
                    const WTF::OrdinalNumber& contextLine,
                    ContentSecurityPolicy::ReportingStatus reportingStatus) {
  bool isAllowed = true;
  for (const auto& policy : policies) {
    isAllowed &= (policy.get()->*allowed)(element, contextURL, contextLine,
                                          reportingStatus);
  }
  return isAllowed;
}

template <
    bool (CSPDirectiveList::*allowed)(Element*,
                                      const String&,
                                      const String&,
                                      const WTF::OrdinalNumber&,
                                      ContentSecurityPolicy::ReportingStatus,
                                      const String& content) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    Element* element,
                    const String& contextURL,
                    const String& nonce,
                    const WTF::OrdinalNumber& contextLine,
                    ContentSecurityPolicy::ReportingStatus reportingStatus,
                    const String& content) {
  bool isAllowed = true;
  for (const auto& policy : policies) {
    isAllowed &= (policy.get()->*allowed)(
        element, contextURL, nonce, contextLine, reportingStatus, content);
  }
  return isAllowed;
}

template <
    bool (CSPDirectiveList::*allowed)(const String&,
                                      const String&,
                                      ParserDisposition,
                                      const WTF::OrdinalNumber&,
                                      ContentSecurityPolicy::ReportingStatus,
                                      const String& content) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    const String& contextURL,
                    const String& nonce,
                    ParserDisposition parserDisposition,
                    const WTF::OrdinalNumber& contextLine,
                    ContentSecurityPolicy::ReportingStatus reportingStatus,
                    const String& content) {
  bool isAllowed = true;
  for (const auto& policy : policies) {
    isAllowed &=
        (policy.get()->*allowed)(contextURL, nonce, parserDisposition,
                                 contextLine, reportingStatus, content);
  }
  return isAllowed;
}

template <bool (CSPDirectiveList::*allowed)(const CSPHashValue&,
                                            ContentSecurityPolicy::InlineType)
              const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    const CSPHashValue& hashValue,
                    ContentSecurityPolicy::InlineType type) {
  bool isAllowed = true;
  for (const auto& policy : policies)
    isAllowed &= (policy.get()->*allowed)(hashValue, type);
  return isAllowed;
}

template <bool (CSPDirectiveList::*allowFromURL)(
    const KURL&,
    RedirectStatus,
    ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    const KURL& url,
                    RedirectStatus redirectStatus,
                    ContentSecurityPolicy::ReportingStatus reportingStatus) {
  if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
    return true;

  bool isAllowed = true;
  for (const auto& policy : policies)
    isAllowed &=
        (policy.get()->*allowFromURL)(url, redirectStatus, reportingStatus);
  return isAllowed;
}

template <bool (CSPDirectiveList::*allowFromURLWithNonce)(
    const KURL&,
    const String& nonce,
    RedirectStatus,
    ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    const KURL& url,
                    const String& nonce,
                    RedirectStatus redirectStatus,
                    ContentSecurityPolicy::ReportingStatus reportingStatus) {
  if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol()))
    return true;

  bool isAllowed = true;
  for (const auto& policy : policies)
    isAllowed &= (policy.get()->*allowFromURLWithNonce)(
        url, nonce, redirectStatus, reportingStatus);
  return isAllowed;
}

template <bool (CSPDirectiveList::*allowFromURLWithNonceAndParser)(
    const KURL&,
    const String& nonce,
    ParserDisposition parserDisposition,
    RedirectStatus,
    ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    const KURL& url,
                    const String& nonce,
                    ParserDisposition parserDisposition,
                    RedirectStatus redirectStatus,
                    ContentSecurityPolicy::ReportingStatus reportingStatus) {
  if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol())) {
    // If we're running experimental features, bypass CSP only for
    // non-parser-inserted resources whose scheme otherwise bypasses CSP. If
    // we're not running experimental features, bypass CSP for all resources
    // regardless of parser state. Once we have more data via the
    // 'ScriptWithCSPBypassingScheme*' metrics, make a decision about what
    // behavior to ship. https://crbug.com/653521
    if (parserDisposition == NotParserInserted ||
        !RuntimeEnabledFeatures::
            experimentalContentSecurityPolicyFeaturesEnabled()) {
      return true;
    }
  }

  bool isAllowed = true;
  for (const auto& policy : policies) {
    isAllowed &= (policy.get()->*allowFromURLWithNonceAndParser)(
        url, nonce, parserDisposition, redirectStatus, reportingStatus);
  }
  return isAllowed;
}

template <bool (CSPDirectiveList::*allowed)(
    LocalFrame*,
    const KURL&,
    ContentSecurityPolicy::ReportingStatus) const>
bool isAllowedByAll(const CSPDirectiveListVector& policies,
                    LocalFrame* frame,
                    const KURL& url,
                    ContentSecurityPolicy::ReportingStatus reportingStatus) {
  bool isAllowed = true;
  for (const auto& policy : policies)
    isAllowed &= (policy.get()->*allowed)(frame, url, reportingStatus);
  return isAllowed;
}

template <bool (CSPDirectiveList::*allowed)(const CSPHashValue&,
                                            ContentSecurityPolicy::InlineType)
              const>
bool checkDigest(const String& source,
                 ContentSecurityPolicy::InlineType type,
                 uint8_t hashAlgorithmsUsed,
                 const CSPDirectiveListVector& policies) {
  // Any additions or subtractions from this struct should also modify the
  // respective entries in the kSupportedPrefixes array in
  // SourceListDirective::parseHash().
  static const struct {
    ContentSecurityPolicyHashAlgorithm cspHashAlgorithm;
    HashAlgorithm algorithm;
  } kAlgorithmMap[] = {
      {ContentSecurityPolicyHashAlgorithmSha1, HashAlgorithmSha1},
      {ContentSecurityPolicyHashAlgorithmSha256, HashAlgorithmSha256},
      {ContentSecurityPolicyHashAlgorithmSha384, HashAlgorithmSha384},
      {ContentSecurityPolicyHashAlgorithmSha512, HashAlgorithmSha512}};

  // Only bother normalizing the source/computing digests if there are any
  // checks to be done.
  if (hashAlgorithmsUsed == ContentSecurityPolicyHashAlgorithmNone)
    return false;

  StringUTF8Adaptor utf8Source(source);

  for (const auto& algorithmMap : kAlgorithmMap) {
    DigestValue digest;
    if (algorithmMap.cspHashAlgorithm & hashAlgorithmsUsed) {
      bool digestSuccess =
          computeDigest(algorithmMap.algorithm, utf8Source.data(),
                        utf8Source.length(), digest);
      if (digestSuccess &&
          isAllowedByAll<allowed>(
              policies, CSPHashValue(algorithmMap.cspHashAlgorithm, digest),
              type))
        return true;
    }
  }

  return false;
}

bool ContentSecurityPolicy::allowJavaScriptURLs(
    Element* element,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowJavaScriptURLs>(
      m_policies, element, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowInlineEventHandler(
    Element* element,
    const String& source,
    const String& contextURL,
    const WTF::OrdinalNumber& contextLine,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  // Inline event handlers may be whitelisted by hash, if
  // 'unsafe-hash-attributes' is present in a policy. Check against the digest
  // of the |source| first before proceeding on to checking whether inline
  // script is allowed.
  if (checkDigest<&CSPDirectiveList::allowScriptHash>(
          source, InlineType::Attribute, m_scriptHashAlgorithmsUsed,
          m_policies))
    return true;
  return isAllowedByAll<&CSPDirectiveList::allowInlineEventHandlers>(
      m_policies, element, contextURL, contextLine, reportingStatus);
}

bool ContentSecurityPolicy::allowInlineScript(
    Element* element,
    const String& contextURL,
    const String& nonce,
    const WTF::OrdinalNumber& contextLine,
    const String& scriptContent,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  DCHECK(element);
  return isAllowedByAll<&CSPDirectiveList::allowInlineScript>(
      m_policies, element, contextURL, nonce, contextLine, reportingStatus,
      scriptContent);
}

bool ContentSecurityPolicy::allowInlineStyle(
    Element* element,
    const String& contextURL,
    const String& nonce,
    const WTF::OrdinalNumber& contextLine,
    const String& styleContent,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  DCHECK(element);
  if (m_overrideInlineStyleAllowed)
    return true;
  return isAllowedByAll<&CSPDirectiveList::allowInlineStyle>(
      m_policies, element, contextURL, nonce, contextLine, reportingStatus,
      styleContent);
}

bool ContentSecurityPolicy::allowEval(
    ScriptState* scriptState,
    ContentSecurityPolicy::ReportingStatus reportingStatus,
    ContentSecurityPolicy::ExceptionStatus exceptionStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowEval>(
      m_policies, scriptState, reportingStatus, exceptionStatus);
}

String ContentSecurityPolicy::evalDisabledErrorMessage() const {
  for (const auto& policy : m_policies) {
    if (!policy->allowEval(0, SuppressReport))
      return policy->evalDisabledErrorMessage();
  }
  return String();
}

bool ContentSecurityPolicy::allowPluginType(
    const String& type,
    const String& typeAttribute,
    const KURL& url,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  for (const auto& policy : m_policies) {
    if (!policy->allowPluginType(type, typeAttribute, url, reportingStatus))
      return false;
  }
  return true;
}

bool ContentSecurityPolicy::allowPluginTypeForDocument(
    const Document& document,
    const String& type,
    const String& typeAttribute,
    const KURL& url,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (document.contentSecurityPolicy() &&
      !document.contentSecurityPolicy()->allowPluginType(type, typeAttribute,
                                                         url))
    return false;

  // CSP says that a plugin document in a nested browsing context should
  // inherit the plugin-types of its parent.
  //
  // FIXME: The plugin-types directive should be pushed down into the
  // current document instead of reaching up to the parent for it here.
  LocalFrame* frame = document.frame();
  if (frame && frame->tree().parent() && document.isPluginDocument()) {
    ContentSecurityPolicy* parentCSP =
        frame->tree().parent()->securityContext()->contentSecurityPolicy();
    if (parentCSP && !parentCSP->allowPluginType(type, typeAttribute, url))
      return false;
  }

  return true;
}

bool ContentSecurityPolicy::allowScriptFromSource(
    const KURL& url,
    const String& nonce,
    ParserDisposition parserDisposition,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(url.protocol())) {
    UseCounter::count(
        document(),
        parserDisposition == ParserInserted
            ? UseCounter::ScriptWithCSPBypassingSchemeParserInserted
            : UseCounter::ScriptWithCSPBypassingSchemeNotParserInserted);
  }
  return isAllowedByAll<&CSPDirectiveList::allowScriptFromSource>(
      m_policies, url, nonce, parserDisposition, redirectStatus,
      reportingStatus);
}

bool ContentSecurityPolicy::allowScriptWithHash(const String& source,
                                                InlineType type) const {
  return checkDigest<&CSPDirectiveList::allowScriptHash>(
      source, type, m_scriptHashAlgorithmsUsed, m_policies);
}

bool ContentSecurityPolicy::allowStyleWithHash(const String& source,
                                               InlineType type) const {
  return checkDigest<&CSPDirectiveList::allowStyleHash>(
      source, type, m_styleHashAlgorithmsUsed, m_policies);
}

bool ContentSecurityPolicy::allowRequestWithoutIntegrity(
    WebURLRequest::RequestContext context,
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  for (const auto& policy : m_policies) {
    if (!policy->allowRequestWithoutIntegrity(context, url, redirectStatus,
                                              reportingStatus))
      return false;
  }
  return true;
}

bool ContentSecurityPolicy::allowRequest(
    WebURLRequest::RequestContext context,
    const KURL& url,
    const String& nonce,
    const IntegrityMetadataSet& integrityMetadata,
    ParserDisposition parserDisposition,
    RedirectStatus redirectStatus,
    ReportingStatus reportingStatus) const {
  if (integrityMetadata.isEmpty() &&
      !allowRequestWithoutIntegrity(context, url, redirectStatus,
                                    reportingStatus))
    return false;

  switch (context) {
    case WebURLRequest::RequestContextAudio:
    case WebURLRequest::RequestContextTrack:
    case WebURLRequest::RequestContextVideo:
      return allowMediaFromSource(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextBeacon:
    case WebURLRequest::RequestContextEventSource:
    case WebURLRequest::RequestContextFetch:
    case WebURLRequest::RequestContextXMLHttpRequest:
    case WebURLRequest::RequestContextSubresource:
      return allowConnectToSource(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextEmbed:
    case WebURLRequest::RequestContextObject:
      return allowObjectFromSource(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextFavicon:
    case WebURLRequest::RequestContextImage:
    case WebURLRequest::RequestContextImageSet:
      return allowImageFromSource(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextFont:
      return allowFontFromSource(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextForm:
      return allowFormAction(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextFrame:
    case WebURLRequest::RequestContextIframe:
      return allowFrameFromSource(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextImport:
    case WebURLRequest::RequestContextScript:
      return allowScriptFromSource(url, nonce, parserDisposition,
                                   redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextXSLT:
      return allowScriptFromSource(url, nonce, parserDisposition,
                                   redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextManifest:
      return allowManifestFromSource(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextServiceWorker:
    case WebURLRequest::RequestContextSharedWorker:
    case WebURLRequest::RequestContextWorker:
      return allowWorkerContextFromSource(url, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextStyle:
      return allowStyleFromSource(url, nonce, redirectStatus, reportingStatus);
    case WebURLRequest::RequestContextCSPReport:
    case WebURLRequest::RequestContextDownload:
    case WebURLRequest::RequestContextHyperlink:
    case WebURLRequest::RequestContextInternal:
    case WebURLRequest::RequestContextLocation:
    case WebURLRequest::RequestContextPing:
    case WebURLRequest::RequestContextPlugin:
    case WebURLRequest::RequestContextPrefetch:
    case WebURLRequest::RequestContextUnspecified:
      return true;
  }
  ASSERT_NOT_REACHED();
  return true;
}

void ContentSecurityPolicy::usesScriptHashAlgorithms(uint8_t algorithms) {
  m_scriptHashAlgorithmsUsed |= algorithms;
}

void ContentSecurityPolicy::usesStyleHashAlgorithms(uint8_t algorithms) {
  m_styleHashAlgorithmsUsed |= algorithms;
}

bool ContentSecurityPolicy::allowObjectFromSource(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowObjectFromSource>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowFrameFromSource(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowFrameFromSource>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowImageFromSource(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(
          url.protocol(), SchemeRegistry::PolicyAreaImage))
    return true;
  return isAllowedByAll<&CSPDirectiveList::allowImageFromSource>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowStyleFromSource(
    const KURL& url,
    const String& nonce,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  if (SchemeRegistry::schemeShouldBypassContentSecurityPolicy(
          url.protocol(), SchemeRegistry::PolicyAreaStyle))
    return true;
  return isAllowedByAll<&CSPDirectiveList::allowStyleFromSource>(
      m_policies, url, nonce, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowFontFromSource(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowFontFromSource>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowMediaFromSource(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowMediaFromSource>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowConnectToSource(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowConnectToSource>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowFormAction(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowFormAction>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowBaseURI(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowBaseURI>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowWorkerContextFromSource(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  // CSP 1.1 moves workers from 'script-src' to the new 'child-src'. Measure the
  // impact of this backwards-incompatible change.
  if (Document* document = this->document()) {
    UseCounter::count(*document, UseCounter::WorkerSubjectToCSP);
    if (isAllowedByAll<&CSPDirectiveList::allowWorkerFromSource>(
            m_policies, url, redirectStatus, SuppressReport) &&
        !isAllowedByAll<&CSPDirectiveList::allowScriptFromSource>(
            m_policies, url, AtomicString(), NotParserInserted, redirectStatus,
            SuppressReport)) {
      UseCounter::count(*document,
                        UseCounter::WorkerAllowedByChildBlockedByScript);
    }
  }

  return isAllowedByAll<&CSPDirectiveList::allowWorkerFromSource>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowManifestFromSource(
    const KURL& url,
    RedirectStatus redirectStatus,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowManifestFromSource>(
      m_policies, url, redirectStatus, reportingStatus);
}

bool ContentSecurityPolicy::allowAncestors(
    LocalFrame* frame,
    const KURL& url,
    ContentSecurityPolicy::ReportingStatus reportingStatus) const {
  return isAllowedByAll<&CSPDirectiveList::allowAncestors>(
      m_policies, frame, url, reportingStatus);
}

bool ContentSecurityPolicy::isFrameAncestorsEnforced() const {
  for (const auto& policy : m_policies) {
    if (policy->isFrameAncestorsEnforced())
      return true;
  }
  return false;
}

bool ContentSecurityPolicy::isActive() const {
  return !m_policies.isEmpty();
}

const KURL ContentSecurityPolicy::url() const {
  return m_executionContext->contextURL();
}

KURL ContentSecurityPolicy::completeURL(const String& url) const {
  return m_executionContext->contextCompleteURL(url);
}

void ContentSecurityPolicy::enforceSandboxFlags(SandboxFlags mask) {
  m_sandboxMask |= mask;
}

void ContentSecurityPolicy::treatAsPublicAddress() {
  if (!RuntimeEnabledFeatures::corsRFC1918Enabled())
    return;
  m_treatAsPublicAddress = true;
}

void ContentSecurityPolicy::enforceStrictMixedContentChecking() {
  m_insecureRequestPolicy |= kBlockAllMixedContent;
}

void ContentSecurityPolicy::upgradeInsecureRequests() {
  m_insecureRequestPolicy |= kUpgradeInsecureRequests;
}

static String stripURLForUseInReport(
    ExecutionContext* context,
    const KURL& url,
    RedirectStatus redirectStatus,
    const ContentSecurityPolicy::DirectiveType& effectiveType) {
  if (!url.isValid())
    return String();
  if (!url.isHierarchical() || url.protocolIs("file"))
    return url.protocol();

  // Until we're more careful about the way we deal with navigations in frames
  // (and, by extension, in plugin documents), strip cross-origin 'frame-src'
  // and 'object-src' violations down to an origin. https://crbug.com/633306
  bool canSafelyExposeURL =
      context->getSecurityOrigin()->canRequest(url) ||
      (redirectStatus == RedirectStatus::NoRedirect &&
       effectiveType != ContentSecurityPolicy::DirectiveType::FrameSrc &&
       effectiveType != ContentSecurityPolicy::DirectiveType::ObjectSrc);

  if (canSafelyExposeURL) {
    // 'KURL::strippedForUseAsReferrer()' dumps 'String()' for non-webby URLs.
    // It's better for developers if we return the origin of those URLs rather
    // than nothing.
    if (url.protocolIsInHTTPFamily())
      return url.strippedForUseAsReferrer();
  }
  return SecurityOrigin::create(url)->toString();
}

static void gatherSecurityPolicyViolationEventData(
    SecurityPolicyViolationEventInit& init,
    ExecutionContext* context,
    const String& directiveText,
    const ContentSecurityPolicy::DirectiveType& effectiveType,
    const KURL& blockedURL,
    const String& header,
    RedirectStatus redirectStatus,
    ContentSecurityPolicyHeaderType headerType,
    ContentSecurityPolicy::ViolationType violationType,
    int contextLine) {
  if (effectiveType == ContentSecurityPolicy::DirectiveType::FrameAncestors) {
    // If this load was blocked via 'frame-ancestors', then the URL of
    // |document| has not yet been initialized. In this case, we'll set both
    // 'documentURI' and 'blockedURI' to the blocked document's URL.
    init.setDocumentURI(blockedURL.getString());
    init.setBlockedURI(blockedURL.getString());
  } else {
    init.setDocumentURI(context->url().getString());
    switch (violationType) {
      case ContentSecurityPolicy::InlineViolation:
        init.setBlockedURI("inline");
        break;
      case ContentSecurityPolicy::EvalViolation:
        init.setBlockedURI("eval");
        break;
      case ContentSecurityPolicy::URLViolation:
        init.setBlockedURI(stripURLForUseInReport(
            context, blockedURL, redirectStatus, effectiveType));
        break;
    }
  }

  String effectiveDirective =
      ContentSecurityPolicy::getDirectiveName(effectiveType);
  init.setViolatedDirective(effectiveDirective);
  init.setEffectiveDirective(effectiveDirective);
  init.setOriginalPolicy(header);
  init.setDisposition(headerType == ContentSecurityPolicyHeaderTypeEnforce
                          ? "enforce"
                          : "report");
  init.setSourceFile(String());
  init.setLineNumber(contextLine);
  init.setColumnNumber(0);
  init.setStatusCode(0);

  // TODO(mkwst): We only have referrer and status code information for
  // Documents. It would be nice to get them for Workers as well.
  if (context->isDocument()) {
    Document* document = toDocument(context);
    DCHECK(document);
    init.setReferrer(document->referrer());
    if (!SecurityOrigin::isSecure(context->url()) && document->loader())
      init.setStatusCode(document->loader()->response().httpStatusCode());
  }

  std::unique_ptr<SourceLocation> location = SourceLocation::capture(context);
  if (location->lineNumber()) {
    KURL source = KURL(ParsedURLString, location->url());
    init.setSourceFile(
        stripURLForUseInReport(context, source, redirectStatus, effectiveType));
    init.setLineNumber(location->lineNumber());
    init.setColumnNumber(location->columnNumber());
  }
}

void ContentSecurityPolicy::reportViolation(
    const String& directiveText,
    const DirectiveType& effectiveType,
    const String& consoleMessage,
    const KURL& blockedURL,
    const Vector<String>& reportEndpoints,
    const String& header,
    ContentSecurityPolicyHeaderType headerType,
    ViolationType violationType,
    LocalFrame* contextFrame,
    RedirectStatus redirectStatus,
    int contextLine,
    Element* element) {
  ASSERT(violationType == URLViolation || blockedURL.isEmpty());

  // TODO(lukasza): Support sending reports from OOPIFs -
  // https://crbug.com/611232 (or move CSP child-src and frame-src checks to the
  // browser process - see https://crbug.com/376522).
  if (!m_executionContext && !contextFrame) {
    DCHECK(effectiveType == DirectiveType::ChildSrc ||
           effectiveType == DirectiveType::FrameSrc ||
           effectiveType == DirectiveType::PluginTypes);
    return;
  }

  DCHECK((m_executionContext && !contextFrame) ||
         ((effectiveType == DirectiveType::FrameAncestors) && contextFrame));

  SecurityPolicyViolationEventInit violationData;

  // If we're processing 'frame-ancestors', use |contextFrame|'s execution
  // context to gather data. Otherwise, use the policy's execution context.
  ExecutionContext* relevantContext =
      contextFrame ? contextFrame->document() : m_executionContext;
  DCHECK(relevantContext);
  gatherSecurityPolicyViolationEventData(
      violationData, relevantContext, directiveText, effectiveType, blockedURL,
      header, redirectStatus, headerType, violationType, contextLine);

  // TODO(mkwst): Obviously, we shouldn't hit this check, as extension-loaded
  // resources should be allowed regardless. We apparently do, however, so
  // we should at least stop spamming reporting endpoints. See
  // https://crbug.com/524356 for detail.
  if (!violationData.sourceFile().isEmpty() &&
      SchemeRegistry::schemeShouldBypassContentSecurityPolicy(
          KURL(ParsedURLString, violationData.sourceFile()).protocol())) {
    return;
  }

  postViolationReport(violationData, contextFrame, reportEndpoints);

  // Fire a violation event if we're working within an execution context (e.g.
  // we're not processing 'frame-ancestors').
  if (m_executionContext) {
    m_executionContext->postTask(
        TaskType::Networking, BLINK_FROM_HERE,
        createSameThreadTask(&ContentSecurityPolicy::dispatchViolationEvents,
                             wrapPersistent(this), violationData,
                             wrapPersistent(element)));
  }
}

void ContentSecurityPolicy::postViolationReport(
    const SecurityPolicyViolationEventInit& violationData,
    LocalFrame* contextFrame,
    const Vector<String>& reportEndpoints) {
  // TODO(mkwst): Support POSTing violation reports from a Worker.
  Document* document =
      contextFrame ? contextFrame->document() : this->document();
  if (!document)
    return;

  // We need to be careful here when deciding what information to send to the
  // report-uri. Currently, we send only the current document's URL and the
  // directive that was violated. The document's URL is safe to send because
  // it's the document itself that's requesting that it be sent. You could
  // make an argument that we shouldn't send HTTPS document URLs to HTTP
  // report-uris (for the same reasons that we supress the Referer in that
  // case), but the Referer is sent implicitly whereas this request is only
  // sent explicitly. As for which directive was violated, that's pretty
  // harmless information.

  std::unique_ptr<JSONObject> cspReport = JSONObject::create();
  cspReport->setString("document-uri", violationData.documentURI());
  cspReport->setString("referrer", violationData.referrer());
  cspReport->setString("violated-directive", violationData.violatedDirective());
  cspReport->setString("effective-directive",
                       violationData.effectiveDirective());
  cspReport->setString("original-policy", violationData.originalPolicy());
  cspReport->setString("disposition", violationData.disposition());
  cspReport->setString("blocked-uri", violationData.blockedURI());
  if (violationData.lineNumber())
    cspReport->setInteger("line-number", violationData.lineNumber());
  if (violationData.columnNumber())
    cspReport->setInteger("column-number", violationData.columnNumber());
  if (!violationData.sourceFile().isEmpty())
    cspReport->setString("source-file", violationData.sourceFile());
  cspReport->setInteger("status-code", violationData.statusCode());

  std::unique_ptr<JSONObject> reportObject = JSONObject::create();
  reportObject->setObject("csp-report", std::move(cspReport));
  String stringifiedReport = reportObject->toJSONString();

  // Only POST unique reports to the external endpoint; repeated reports add no
  // value on the server side, as they're indistinguishable. Note that we'll
  // fire the DOM event for every violation, as the page has enough context to
  // react in some reasonable way to each violation as it occurs.
  if (shouldSendViolationReport(stringifiedReport)) {
    didSendViolationReport(stringifiedReport);

    RefPtr<EncodedFormData> report =
        EncodedFormData::create(stringifiedReport.utf8());

    LocalFrame* frame = document->frame();
    if (!frame)
      return;

    for (const String& endpoint : reportEndpoints) {
      // If we have a context frame we're dealing with 'frame-ancestors' and we
      // don't have our own execution context. Use the frame's document to
      // complete the endpoint URL, overriding its URL with the blocked
      // document's URL.
      DCHECK(!contextFrame || !m_executionContext);
      DCHECK(!contextFrame ||
             getDirectiveType(violationData.effectiveDirective()) ==
                 DirectiveType::FrameAncestors);
      KURL url =
          contextFrame
              ? frame->document()->completeURLWithOverride(
                    endpoint, KURL(ParsedURLString, violationData.blockedURI()))
              : completeURL(endpoint);
      PingLoader::sendViolationReport(
          frame, url, report, PingLoader::ContentSecurityPolicyViolationReport);
    }
  }
}

void ContentSecurityPolicy::dispatchViolationEvents(
    const SecurityPolicyViolationEventInit& violationData,
    Element* element) {
  // If the context is detached or closed (thus clearing its event queue)
  // between the violation occuring and this event dispatch, exit early.
  EventQueue* queue = m_executionContext->getEventQueue();
  if (!queue)
    return;

  SecurityPolicyViolationEvent* event = SecurityPolicyViolationEvent::create(
      EventTypeNames::securitypolicyviolation, violationData);
  DCHECK(event->bubbles());

  if (m_executionContext->isDocument()) {
    Document* document = toDocument(m_executionContext);
    if (element && element->isConnected() && element->document() == document)
      event->setTarget(element);
    else
      event->setTarget(document);
  } else if (m_executionContext->isWorkerGlobalScope()) {
    event->setTarget(toWorkerGlobalScope(m_executionContext));
  }
  queue->enqueueEvent(event);
}

void ContentSecurityPolicy::reportMixedContent(const KURL& mixedURL,
                                               RedirectStatus redirectStatus) {
  for (const auto& policy : m_policies)
    policy->reportMixedContent(mixedURL, redirectStatus);
}

void ContentSecurityPolicy::reportReportOnlyInMeta(const String& header) {
  logToConsole("The report-only Content Security Policy '" + header +
               "' was delivered via a <meta> element, which is disallowed. The "
               "policy has been ignored.");
}

void ContentSecurityPolicy::reportMetaOutsideHead(const String& header) {
  logToConsole("The Content Security Policy '" + header +
               "' was delivered via a <meta> element outside the document's "
               "<head>, which is disallowed. The policy has been ignored.");
}

void ContentSecurityPolicy::reportValueForEmptyDirective(const String& name,
                                                         const String& value) {
  logToConsole("The Content Security Policy directive '" + name +
               "' should be empty, but was delivered with a value of '" +
               value +
               "'. The directive has been applied, and the value ignored.");
}

void ContentSecurityPolicy::reportInvalidInReportOnly(const String& name) {
  logToConsole("The Content Security Policy directive '" + name +
               "' is ignored when delivered in a report-only policy.");
}

void ContentSecurityPolicy::reportInvalidDirectiveInMeta(
    const String& directive) {
  logToConsole(
      "Content Security Policies delivered via a <meta> element may not "
      "contain the " +
      directive + " directive.");
}

void ContentSecurityPolicy::reportUnsupportedDirective(const String& name) {
  static const char allow[] = "allow";
  static const char options[] = "options";
  static const char policyURI[] = "policy-uri";
  static const char allowMessage[] =
      "The 'allow' directive has been replaced with 'default-src'. Please use "
      "that directive instead, as 'allow' has no effect.";
  static const char optionsMessage[] =
      "The 'options' directive has been replaced with 'unsafe-inline' and "
      "'unsafe-eval' source expressions for the 'script-src' and 'style-src' "
      "directives. Please use those directives instead, as 'options' has no "
      "effect.";
  static const char policyURIMessage[] =
      "The 'policy-uri' directive has been removed from the "
      "specification. Please specify a complete policy via "
      "the Content-Security-Policy header.";

  String message =
      "Unrecognized Content-Security-Policy directive '" + name + "'.\n";
  MessageLevel level = ErrorMessageLevel;
  if (equalIgnoringCase(name, allow)) {
    message = allowMessage;
  } else if (equalIgnoringCase(name, options)) {
    message = optionsMessage;
  } else if (equalIgnoringCase(name, policyURI)) {
    message = policyURIMessage;
  } else if (getDirectiveType(name) != DirectiveType::Undefined) {
    message = "The Content-Security-Policy directive '" + name +
              "' is implemented behind a flag which is currently disabled.\n";
    level = InfoMessageLevel;
  }

  logToConsole(message, level);
}

void ContentSecurityPolicy::reportDirectiveAsSourceExpression(
    const String& directiveName,
    const String& sourceExpression) {
  String message = "The Content Security Policy directive '" + directiveName +
                   "' contains '" + sourceExpression +
                   "' as a source expression. Did you mean '" + directiveName +
                   " ...; " + sourceExpression + "...' (note the semicolon)?";
  logToConsole(message);
}

void ContentSecurityPolicy::reportDuplicateDirective(const String& name) {
  String message =
      "Ignoring duplicate Content-Security-Policy directive '" + name + "'.\n";
  logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidPluginTypes(const String& pluginType) {
  String message;
  if (pluginType.isNull())
    message =
        "'plugin-types' Content Security Policy directive is empty; all "
        "plugins will be blocked.\n";
  else if (pluginType == "'none'")
    message =
        "Invalid plugin type in 'plugin-types' Content Security Policy "
        "directive: '" +
        pluginType +
        "'. Did you mean to set the object-src directive to 'none'?\n";
  else
    message =
        "Invalid plugin type in 'plugin-types' Content Security Policy "
        "directive: '" +
        pluginType + "'.\n";
  logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSandboxFlags(
    const String& invalidFlags) {
  logToConsole(
      "Error while parsing the 'sandbox' Content Security Policy directive: " +
      invalidFlags);
}

void ContentSecurityPolicy::reportInvalidRequireSRIForTokens(
    const String& invalidTokens) {
  logToConsole(
      "Error while parsing the 'require-sri-for' Content Security Policy "
      "directive: " +
      invalidTokens);
}

void ContentSecurityPolicy::reportInvalidDirectiveValueCharacter(
    const String& directiveName,
    const String& value) {
  String message = "The value for Content Security Policy directive '" +
                   directiveName + "' contains an invalid character: '" +
                   value +
                   "'. Non-whitespace characters outside ASCII 0x21-0x7E must "
                   "be percent-encoded, as described in RFC 3986, section 2.1: "
                   "http://tools.ietf.org/html/rfc3986#section-2.1.";
  logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidPathCharacter(
    const String& directiveName,
    const String& value,
    const char invalidChar) {
  ASSERT(invalidChar == '#' || invalidChar == '?');

  String ignoring =
      "The fragment identifier, including the '#', will be ignored.";
  if (invalidChar == '?')
    ignoring = "The query component, including the '?', will be ignored.";
  String message = "The source list for Content Security Policy directive '" +
                   directiveName +
                   "' contains a source with an invalid path: '" + value +
                   "'. " + ignoring;
  logToConsole(message);
}

void ContentSecurityPolicy::reportInvalidSourceExpression(
    const String& directiveName,
    const String& source) {
  String message = "The source list for Content Security Policy directive '" +
                   directiveName + "' contains an invalid source: '" + source +
                   "'. It will be ignored.";
  if (equalIgnoringCase(source, "'none'"))
    message = message +
              " Note that 'none' has no effect unless it is the only "
              "expression in the source list.";
  logToConsole(message);
}

void ContentSecurityPolicy::reportMissingReportURI(const String& policy) {
  logToConsole("The Content Security Policy '" + policy +
               "' was delivered in report-only mode, but does not specify a "
               "'report-uri'; the policy will have no effect. Please either "
               "add a 'report-uri' directive, or deliver the policy via the "
               "'Content-Security-Policy' header.");
}

void ContentSecurityPolicy::logToConsole(const String& message,
                                         MessageLevel level) {
  logToConsole(ConsoleMessage::create(SecurityMessageSource, level, message));
}

void ContentSecurityPolicy::logToConsole(ConsoleMessage* consoleMessage,
                                         LocalFrame* frame) {
  if (frame)
    frame->document()->addConsoleMessage(consoleMessage);
  else if (m_executionContext)
    m_executionContext->addConsoleMessage(consoleMessage);
  else
    m_consoleMessages.push_back(consoleMessage);
}

void ContentSecurityPolicy::reportBlockedScriptExecutionToInspector(
    const String& directiveText) const {
  InspectorInstrumentation::scriptExecutionBlockedByCSP(m_executionContext,
                                                        directiveText);
}

bool ContentSecurityPolicy::experimentalFeaturesEnabled() const {
  return RuntimeEnabledFeatures::
      experimentalContentSecurityPolicyFeaturesEnabled();
}

bool ContentSecurityPolicy::shouldSendCSPHeader(Resource::Type type) const {
  for (const auto& policy : m_policies) {
    if (policy->shouldSendCSPHeader(type))
      return true;
  }
  return false;
}

bool ContentSecurityPolicy::urlMatchesSelf(const KURL& url) const {
  return m_selfSource->matches(url, RedirectStatus::NoRedirect);
}

bool ContentSecurityPolicy::protocolMatchesSelf(const KURL& url) const {
  if (equalIgnoringCase("http", m_selfProtocol))
    return url.protocolIsInHTTPFamily();
  return equalIgnoringCase(url.protocol(), m_selfProtocol);
}

bool ContentSecurityPolicy::selfMatchesInnerURL() const {
  // Due to backwards-compatibility concerns, we allow 'self' to match blob and
  // filesystem URLs if we're in a context that bypasses Content Security Policy
  // in the main world.
  //
  // TODO(mkwst): Revisit this once embedders have an opportunity to update
  // their extension models.
  return m_executionContext &&
         SchemeRegistry::schemeShouldBypassContentSecurityPolicy(
             m_executionContext->getSecurityOrigin()->protocol());
}

bool ContentSecurityPolicy::shouldBypassMainWorld(
    const ExecutionContext* context) {
  if (context && context->isDocument()) {
    const Document* document = toDocument(context);
    if (document->frame())
      return document->frame()->script().shouldBypassMainWorldCSP();
  }
  return false;
}

bool ContentSecurityPolicy::shouldSendViolationReport(
    const String& report) const {
  // Collisions have no security impact, so we can save space by storing only
  // the string's hash rather than the whole report.
  return !m_violationReportsSent.contains(report.impl()->hash());
}

void ContentSecurityPolicy::didSendViolationReport(const String& report) {
  m_violationReportsSent.add(report.impl()->hash());
}

const char* ContentSecurityPolicy::getDirectiveName(const DirectiveType& type) {
  switch (type) {
    case DirectiveType::BaseURI:
      return "base-uri";
    case DirectiveType::BlockAllMixedContent:
      return "block-all-mixed-content";
    case DirectiveType::ChildSrc:
      return "child-src";
    case DirectiveType::ConnectSrc:
      return "connect-src";
    case DirectiveType::DefaultSrc:
      return "default-src";
    case DirectiveType::FrameAncestors:
      return "frame-ancestors";
    case DirectiveType::FrameSrc:
      return "frame-src";
    case DirectiveType::FontSrc:
      return "font-src";
    case DirectiveType::FormAction:
      return "form-action";
    case DirectiveType::ImgSrc:
      return "img-src";
    case DirectiveType::ManifestSrc:
      return "manifest-src";
    case DirectiveType::MediaSrc:
      return "media-src";
    case DirectiveType::ObjectSrc:
      return "object-src";
    case DirectiveType::PluginTypes:
      return "plugin-types";
    case DirectiveType::ReportURI:
      return "report-uri";
    case DirectiveType::RequireSRIFor:
      return "require-sri-for";
    case DirectiveType::Sandbox:
      return "sandbox";
    case DirectiveType::ScriptSrc:
      return "script-src";
    case DirectiveType::StyleSrc:
      return "style-src";
    case DirectiveType::TreatAsPublicAddress:
      return "treat-as-public-address";
    case DirectiveType::UpgradeInsecureRequests:
      return "upgrade-insecure-requests";
    case DirectiveType::WorkerSrc:
      return "worker-src";
    case DirectiveType::Undefined:
      NOTREACHED();
      return "";
  }

  NOTREACHED();
  return "";
}

ContentSecurityPolicy::DirectiveType ContentSecurityPolicy::getDirectiveType(
    const String& name) {
  if (name == "base-uri")
    return DirectiveType::BaseURI;
  if (name == "block-all-mixed-content")
    return DirectiveType::BlockAllMixedContent;
  if (name == "child-src")
    return DirectiveType::ChildSrc;
  if (name == "connect-src")
    return DirectiveType::ConnectSrc;
  if (name == "default-src")
    return DirectiveType::DefaultSrc;
  if (name == "frame-ancestors")
    return DirectiveType::FrameAncestors;
  if (name == "frame-src")
    return DirectiveType::FrameSrc;
  if (name == "font-src")
    return DirectiveType::FontSrc;
  if (name == "form-action")
    return DirectiveType::FormAction;
  if (name == "img-src")
    return DirectiveType::ImgSrc;
  if (name == "manifest-src")
    return DirectiveType::ManifestSrc;
  if (name == "media-src")
    return DirectiveType::MediaSrc;
  if (name == "object-src")
    return DirectiveType::ObjectSrc;
  if (name == "plugin-types")
    return DirectiveType::PluginTypes;
  if (name == "report-uri")
    return DirectiveType::ReportURI;
  if (name == "require-sri-for")
    return DirectiveType::RequireSRIFor;
  if (name == "sandbox")
    return DirectiveType::Sandbox;
  if (name == "script-src")
    return DirectiveType::ScriptSrc;
  if (name == "style-src")
    return DirectiveType::StyleSrc;
  if (name == "treat-as-public-address")
    return DirectiveType::TreatAsPublicAddress;
  if (name == "upgrade-insecure-requests")
    return DirectiveType::UpgradeInsecureRequests;
  if (name == "worker-src")
    return DirectiveType::WorkerSrc;

  return DirectiveType::Undefined;
}

bool ContentSecurityPolicy::subsumes(const ContentSecurityPolicy& other) const {
  if (!m_policies.size() || !other.m_policies.size())
    return !m_policies.size();
  // Embedding-CSP must specify only one policy.
  if (m_policies.size() != 1)
    return false;

  CSPDirectiveListVector otherVector;
  for (const auto& policy : other.m_policies) {
    if (!policy->isReportOnly())
      otherVector.push_back(policy);
  }

  return m_policies[0]->subsumes(otherVector);
}

}  // namespace blink
