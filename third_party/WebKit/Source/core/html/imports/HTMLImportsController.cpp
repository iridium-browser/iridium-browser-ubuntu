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

#include "core/html/imports/HTMLImportsController.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportChildClient.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportTreeRoot.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

HTMLImportsController::HTMLImportsController(Document& master)
    : m_root(HTMLImportTreeRoot::create(&master)) {
  UseCounter::count(master, UseCounter::HTMLImports);
}

void HTMLImportsController::dispose() {
  if (m_root) {
    m_root->dispose();
    m_root.clear();
  }

  for (const auto& loader : m_loaders)
    loader->dispose();
  m_loaders.clear();
}

static bool makesCycle(HTMLImport* parent, const KURL& url) {
  for (HTMLImport* ancestor = parent; ancestor; ancestor = ancestor->parent()) {
    if (!ancestor->isRoot() &&
        equalIgnoringFragmentIdentifier(toHTMLImportChild(parent)->url(), url))
      return true;
  }

  return false;
}

HTMLImportChild* HTMLImportsController::createChild(
    const KURL& url,
    HTMLImportLoader* loader,
    HTMLImport* parent,
    HTMLImportChildClient* client) {
  HTMLImport::SyncMode mode = client->isSync() && !makesCycle(parent, url)
                                  ? HTMLImport::Sync
                                  : HTMLImport::Async;
  if (mode == HTMLImport::Async)
    UseCounter::count(root()->document(),
                      UseCounter::HTMLImportsAsyncAttribute);

  HTMLImportChild* child = new HTMLImportChild(url, loader, mode);
  child->setClient(client);
  parent->appendImport(child);
  loader->addImport(child);
  return root()->add(child);
}

HTMLImportChild* HTMLImportsController::load(HTMLImport* parent,
                                             HTMLImportChildClient* client,
                                             FetchRequest request) {
  DCHECK(!request.url().isEmpty());
  DCHECK(request.url().isValid());
  DCHECK(parent == root() ||
         toHTMLImportChild(parent)->loader()->isFirstImport(
             toHTMLImportChild(parent)));

  if (HTMLImportChild* childToShareWith = root()->find(request.url())) {
    HTMLImportLoader* loader = childToShareWith->loader();
    DCHECK(loader);
    HTMLImportChild* child = createChild(request.url(), loader, parent, client);
    child->didShareLoader();
    return child;
  }

  request.setCrossOriginAccessControl(master()->getSecurityOrigin(),
                                      CrossOriginAttributeAnonymous);
  RawResource* resource =
      RawResource::fetchImport(request, parent->document()->fetcher());
  if (!resource)
    return nullptr;

  HTMLImportLoader* loader = createLoader();
  HTMLImportChild* child = createChild(request.url(), loader, parent, client);
  // We set resource after the import tree is built since
  // Resource::addClient() immediately calls back to feed the bytes when the
  // resource is cached.
  loader->startLoading(resource);
  child->didStartLoading();
  return child;
}

Document* HTMLImportsController::master() const {
  return root() ? root()->document() : nullptr;
}

bool HTMLImportsController::shouldBlockScriptExecution(
    const Document& document) const {
  DCHECK_EQ(document.importsController(), this);
  if (HTMLImportLoader* loader = loaderFor(document))
    return loader->shouldBlockScriptExecution();
  return root()->state().shouldBlockScriptExecution();
}

HTMLImportLoader* HTMLImportsController::createLoader() {
  m_loaders.push_back(HTMLImportLoader::create(this));
  return m_loaders.back().get();
}

HTMLImportLoader* HTMLImportsController::loaderFor(
    const Document& document) const {
  for (const auto& loader : m_loaders) {
    if (loader->document() == &document)
      return loader.get();
  }

  return nullptr;
}

DEFINE_TRACE(HTMLImportsController) {
  visitor->trace(m_root);
  visitor->trace(m_loaders);
}

DEFINE_TRACE_WRAPPERS(HTMLImportsController) {
  visitor->traceWrappers(master());
}

}  // namespace blink
