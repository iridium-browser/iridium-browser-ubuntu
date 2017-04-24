// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/imports/HTMLImportTreeRoot.h"

#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/html/imports/HTMLImportChild.h"

namespace blink {

HTMLImportTreeRoot* HTMLImportTreeRoot::create(Document* document) {
  return new HTMLImportTreeRoot(document);
}

HTMLImportTreeRoot::HTMLImportTreeRoot(Document* document)
    : HTMLImport(HTMLImport::Sync),
      m_document(document),
      m_recalcTimer(
          TaskRunnerHelper::get(TaskType::UnspecedTimer, document->frame()),
          this,
          &HTMLImportTreeRoot::recalcTimerFired) {
  scheduleRecalcState();  // This recomputes initial state.
}

HTMLImportTreeRoot::~HTMLImportTreeRoot() {}

void HTMLImportTreeRoot::dispose() {
  for (const auto& importChild : m_imports)
    importChild->dispose();
  m_imports.clear();
  m_document = nullptr;
  m_recalcTimer.stop();
}

Document* HTMLImportTreeRoot::document() const {
  return m_document;
}

bool HTMLImportTreeRoot::hasFinishedLoading() const {
  return !m_document->parsing() &&
         m_document->styleEngine().haveScriptBlockingStylesheetsLoaded();
}

void HTMLImportTreeRoot::stateWillChange() {
  scheduleRecalcState();
}

void HTMLImportTreeRoot::stateDidChange() {
  HTMLImport::stateDidChange();

  if (!state().isReady())
    return;
  if (LocalFrame* frame = m_document->frame())
    frame->loader().checkCompleted();
}

void HTMLImportTreeRoot::scheduleRecalcState() {
  DCHECK(m_document);
  if (m_recalcTimer.isActive() || !m_document->isActive())
    return;
  m_recalcTimer.startOneShot(0, BLINK_FROM_HERE);
}

HTMLImportChild* HTMLImportTreeRoot::add(HTMLImportChild* child) {
  m_imports.push_back(child);
  return m_imports.back().get();
}

HTMLImportChild* HTMLImportTreeRoot::find(const KURL& url) const {
  for (const auto& candidate : m_imports) {
    if (equalIgnoringFragmentIdentifier(candidate->url(), url))
      return candidate;
  }

  return nullptr;
}

void HTMLImportTreeRoot::recalcTimerFired(TimerBase*) {
  DCHECK(m_document);
  HTMLImport::recalcTreeState(this);
}

DEFINE_TRACE(HTMLImportTreeRoot) {
  visitor->trace(m_document);
  visitor->trace(m_imports);
  HTMLImport::trace(visitor);
}

}  // namespace blink
