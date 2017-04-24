/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/PendingScript.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/dom/Element.h"
#include "core/frame/SubresourceIntegrity.h"
#include "platform/SharedBuffer.h"
#include "wtf/CurrentTime.h"

namespace blink {

PendingScript* PendingScript::create(Element* element,
                                     ScriptResource* resource) {
  return new PendingScript(element, resource, TextPosition());
}

PendingScript* PendingScript::create(Element* element,
                                     const TextPosition& startingPosition) {
  return new PendingScript(element, nullptr, startingPosition);
}

PendingScript* PendingScript::createForTesting(ScriptResource* resource) {
  return new PendingScript(nullptr, resource, TextPosition(), true);
}

PendingScript::PendingScript(Element* element,
                             ScriptResource* resource,
                             const TextPosition& startingPosition,
                             bool isForTesting)
    : m_watchingForLoad(false),
      m_element(element),
      m_startingPosition(startingPosition),
      m_integrityFailure(false),
      m_parserBlockingLoadStartTime(0),
      m_client(nullptr),
      m_isForTesting(isForTesting) {
  checkState();
  setResource(resource);
  MemoryCoordinator::instance().registerClient(this);
}

PendingScript::~PendingScript() {}

NOINLINE void PendingScript::checkState() const {
  // TODO(hiroshige): Turn these CHECK()s into DCHECK() before going to beta.
  CHECK(m_isForTesting || m_element);
  CHECK(resource() || !m_streamer);
  CHECK(!m_streamer || m_streamer->resource() == resource());
}

void PendingScript::dispose() {
  stopWatchingForLoad();
  DCHECK(!m_client);
  DCHECK(!m_watchingForLoad);

  setResource(nullptr);
  m_startingPosition = TextPosition::belowRangePosition();
  m_integrityFailure = false;
  m_parserBlockingLoadStartTime = 0;
  if (m_streamer)
    m_streamer->cancel();
  m_streamer = nullptr;
  m_element = nullptr;
}

void PendingScript::watchForLoad(PendingScriptClient* client) {
  checkState();

  DCHECK(!m_watchingForLoad);
  // addClient() will call streamingFinished() if the load is complete. Callers
  // who do not expect to be re-entered from this call should not call
  // watchForLoad for a PendingScript which isReady. We also need to set
  // m_watchingForLoad early, since addClient() can result in calling
  // notifyFinished and further stopWatchingForLoad().
  m_watchingForLoad = true;
  m_client = client;
  if (isReady())
    m_client->pendingScriptFinished(this);
}

void PendingScript::stopWatchingForLoad() {
  if (!m_watchingForLoad)
    return;
  checkState();
  DCHECK(resource());
  m_client = nullptr;
  m_watchingForLoad = false;
}

Element* PendingScript::element() const {
  // As mentioned in the comment at |m_element| declaration, |m_element|
  // must points to the corresponding ScriptLoader's element.
  CHECK(m_element);
  return m_element.get();
}

void PendingScript::streamingFinished() {
  checkState();
  DCHECK(resource());
  if (m_client)
    m_client->pendingScriptFinished(this);
}

void PendingScript::markParserBlockingLoadStartTime() {
  DCHECK_EQ(m_parserBlockingLoadStartTime, 0.0);
  m_parserBlockingLoadStartTime = monotonicallyIncreasingTime();
}

// Returns true if SRI check passed.
static bool checkScriptResourceIntegrity(Resource* resource, Element* element) {
  DCHECK_EQ(resource->getType(), Resource::Script);
  ScriptResource* scriptResource = toScriptResource(resource);
  String integrityAttr = element->fastGetAttribute(HTMLNames::integrityAttr);

  // It is possible to get back a script resource with integrity metadata
  // for a request with an empty integrity attribute. In that case, the
  // integrity check should be skipped, so this check ensures that the
  // integrity attribute isn't empty in addition to checking if the
  // resource has empty integrity metadata.
  if (integrityAttr.isEmpty() || scriptResource->integrityMetadata().isEmpty())
    return true;

  switch (scriptResource->integrityDisposition()) {
    case ResourceIntegrityDisposition::Passed:
      return true;

    case ResourceIntegrityDisposition::Failed:
      // TODO(jww): This should probably also generate a console
      // message identical to the one produced by
      // CheckSubresourceIntegrity below. See https://crbug.com/585267.
      return false;

    case ResourceIntegrityDisposition::NotChecked: {
      if (!resource->resourceBuffer())
        return true;

      bool passed = SubresourceIntegrity::CheckSubresourceIntegrity(
          scriptResource->integrityMetadata(), *element,
          resource->resourceBuffer()->data(),
          resource->resourceBuffer()->size(), resource->url(), *resource);
      scriptResource->setIntegrityDisposition(
          passed ? ResourceIntegrityDisposition::Passed
                 : ResourceIntegrityDisposition::Failed);
      return passed;
    }
  }

  NOTREACHED();
  return true;
}

void PendingScript::notifyFinished(Resource* resource) {
  // The following SRI checks need to be here because, unfortunately, fetches
  // are not done purely according to the Fetch spec. In particular,
  // different requests for the same resource do not have different
  // responses; the memory cache can (and will) return the exact same
  // Resource object.
  //
  // For different requests, the same Resource object will be returned and
  // will not be associated with the particular request.  Therefore, when the
  // body of the response comes in, there's no way to validate the integrity
  // of the Resource object against a particular request (since there may be
  // several pending requests all tied to the identical object, and the
  // actual requests are not stored).
  //
  // In order to simulate the correct behavior, Blink explicitly does the SRI
  // checks here, when a PendingScript tied to a particular request is
  // finished (and in the case of a StyleSheet, at the point of execution),
  // while having proper Fetch checks in the fetch module for use in the
  // fetch JavaScript API. In a future world where the ResourceFetcher uses
  // the Fetch algorithm, this should be fixed by having separate Response
  // objects (perhaps attached to identical Resource objects) per request.
  //
  // See https://crbug.com/500701 for more information.
  checkState();
  if (m_element)
    m_integrityFailure = !checkScriptResourceIntegrity(resource, m_element);

  // If script streaming is in use, the client will be notified in
  // streamingFinished.
  if (m_streamer)
    m_streamer->notifyFinished(resource);
  else if (m_client)
    m_client->pendingScriptFinished(this);
}

void PendingScript::notifyAppendData(ScriptResource* resource) {
  if (m_streamer)
    m_streamer->notifyAppendData(resource);
}

DEFINE_TRACE(PendingScript) {
  visitor->trace(m_element);
  visitor->trace(m_streamer);
  visitor->trace(m_client);
  ResourceOwner<ScriptResource>::trace(visitor);
  MemoryCoordinatorClient::trace(visitor);
}

ScriptSourceCode PendingScript::getSource(const KURL& documentURL,
                                          bool& errorOccurred) const {
  checkState();

  errorOccurred = this->errorOccurred();
  if (resource()) {
    DCHECK(resource()->isLoaded());
    if (m_streamer && !m_streamer->streamingSuppressed())
      return ScriptSourceCode(m_streamer, resource());
    return ScriptSourceCode(resource());
  }

  return ScriptSourceCode(m_element->textContent(), documentURL,
                          startingPosition());
}

void PendingScript::setStreamer(ScriptStreamer* streamer) {
  DCHECK(!m_streamer);
  DCHECK(!m_watchingForLoad);
  m_streamer = streamer;
  checkState();
}

bool PendingScript::isReady() const {
  checkState();
  if (resource()) {
    return resource()->isLoaded() && (!m_streamer || m_streamer->isFinished());
  }

  return true;
}

bool PendingScript::errorOccurred() const {
  checkState();
  if (resource())
    return resource()->errorOccurred() || m_integrityFailure;

  return false;
}

void PendingScript::onPurgeMemory() {
  checkState();
  if (!m_streamer)
    return;
  m_streamer->cancel();
  m_streamer = nullptr;
}

}  // namespace blink
