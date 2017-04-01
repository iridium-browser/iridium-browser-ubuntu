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

#include "core/html/imports/HTMLImportLoader.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/custom/V0CustomElementSyncMicrotaskQueue.h"
#include "core/html/HTMLDocument.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/loader/DocumentWriter.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include <memory>

namespace blink {

HTMLImportLoader::HTMLImportLoader(HTMLImportsController* controller)
    : m_controller(controller),
      m_state(StateLoading),
      m_microtaskQueue(V0CustomElementSyncMicrotaskQueue::create()) {}

HTMLImportLoader::~HTMLImportLoader() {}

void HTMLImportLoader::dispose() {
  m_controller = nullptr;
  if (m_document) {
    if (m_document->parser())
      m_document->parser()->removeClient(this);
    m_document->clearImportsController();
    m_document.clear();
  }
  clearResource();
}

void HTMLImportLoader::startLoading(RawResource* resource) {
  setResource(resource);
}

void HTMLImportLoader::responseReceived(
    Resource* resource,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  // Resource may already have been loaded with the import loader
  // being added as a client later & now being notified. Fail early.
  if (resource->loadFailedOrCanceled() || response.httpStatusCode() >= 400 ||
      !response.httpHeaderField(HTTPNames::Content_Disposition).isNull()) {
    setState(StateError);
    return;
  }
  setState(startWritingAndParsing(response));
}

void HTMLImportLoader::dataReceived(Resource*,
                                    const char* data,
                                    size_t length) {
  m_writer->addData(data, length);
}

void HTMLImportLoader::notifyFinished(Resource* resource) {
  // The writer instance indicates that a part of the document can be already
  // loaded.  We don't take such a case as an error because the partially-loaded
  // document has been visible from script at this point.
  if (resource->loadFailedOrCanceled() && !m_writer) {
    setState(StateError);
    return;
  }

  setState(finishWriting());
}

HTMLImportLoader::State HTMLImportLoader::startWritingAndParsing(
    const ResourceResponse& response) {
  DCHECK(m_controller);
  DCHECK(!m_imports.isEmpty());
  DocumentInit init =
      DocumentInit(response.url(), 0, m_controller->master()->contextDocument(),
                   m_controller)
          .withRegistrationContext(
              m_controller->master()->registrationContext());
  m_document = HTMLDocument::create(init);
  m_writer = DocumentWriter::create(m_document.get(), AllowAsynchronousParsing,
                                    response.mimeType(), "UTF-8");

  DocumentParser* parser = m_document->parser();
  DCHECK(parser);
  parser->addClient(this);

  return StateLoading;
}

HTMLImportLoader::State HTMLImportLoader::finishWriting() {
  return StateWritten;
}

HTMLImportLoader::State HTMLImportLoader::finishParsing() {
  return StateParsed;
}

HTMLImportLoader::State HTMLImportLoader::finishLoading() {
  return StateLoaded;
}

void HTMLImportLoader::setState(State state) {
  if (m_state == state)
    return;

  m_state = state;

  if (m_state == StateParsed || m_state == StateError ||
      m_state == StateWritten) {
    if (DocumentWriter* writer = m_writer.release())
      writer->end();
  }

  // Since DocumentWriter::end() can let setState() reenter, we shouldn't refer
  // to m_state here.
  if (state == StateLoaded)
    m_document->setReadyState(Document::Complete);
  if (state == StateLoaded || state == StateError)
    didFinishLoading();
}

void HTMLImportLoader::notifyParserStopped() {
  setState(finishParsing());
  if (!hasPendingResources())
    setState(finishLoading());

  DocumentParser* parser = m_document->parser();
  DCHECK(parser);
  parser->removeClient(this);
}

void HTMLImportLoader::didRemoveAllPendingStylesheet() {
  if (m_state == StateParsed)
    setState(finishLoading());
}

bool HTMLImportLoader::hasPendingResources() const {
  return m_document &&
         m_document->styleEngine().hasPendingScriptBlockingSheets();
}

void HTMLImportLoader::didFinishLoading() {
  for (const auto& importChild : m_imports)
    importChild->didFinishLoading();

  clearResource();

  DCHECK(!m_document || !m_document->parsing());
}

void HTMLImportLoader::moveToFirst(HTMLImportChild* import) {
  size_t position = m_imports.find(import);
  DCHECK_NE(kNotFound, position);
  m_imports.remove(position);
  m_imports.insert(0, import);
}

void HTMLImportLoader::addImport(HTMLImportChild* import) {
  DCHECK_EQ(kNotFound, m_imports.find(import));

  m_imports.push_back(import);
  import->normalize();
  if (isDone())
    import->didFinishLoading();
}

void HTMLImportLoader::removeImport(HTMLImportChild* client) {
  DCHECK_NE(kNotFound, m_imports.find(client));
  m_imports.remove(m_imports.find(client));
}

bool HTMLImportLoader::shouldBlockScriptExecution() const {
  return firstImport()->state().shouldBlockScriptExecution();
}

V0CustomElementSyncMicrotaskQueue* HTMLImportLoader::microtaskQueue() const {
  return m_microtaskQueue;
}

DEFINE_TRACE(HTMLImportLoader) {
  visitor->trace(m_controller);
  visitor->trace(m_imports);
  visitor->trace(m_document);
  visitor->trace(m_writer);
  visitor->trace(m_microtaskQueue);
  DocumentParserClient::trace(visitor);
  ResourceOwner<RawResource>::trace(visitor);
}

}  // namespace blink
