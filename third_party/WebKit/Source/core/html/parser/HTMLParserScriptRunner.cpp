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

#include "core/html/parser/HTMLParserScriptRunner.h"

#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/DocumentParserTiming.h"
#include "core/dom/Element.h"
#include "core/dom/IgnoreDestructiveWriteCountIncrementer.h"
#include "core/dom/ScriptLoader.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/fetch/MemoryCache.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/HTMLInputStream.h"
#include "core/html/parser/HTMLParserScriptRunnerHost.h"
#include "core/html/parser/NestingLevelIncrementer.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/Histogram.h"
#include "platform/WebFrameScheduler.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "public/platform/Platform.h"
#include <inttypes.h>
#include <memory>

namespace blink {

namespace {

// TODO(bmcquade): move this to a shared location if we find ourselves wanting
// to trace similar data elsewhere in the codebase.
std::unique_ptr<TracedValue> getTraceArgsForScriptElement(
    Element* element,
    const TextPosition& textPosition) {
  std::unique_ptr<TracedValue> value = TracedValue::create();
  ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element);
  if (scriptLoader && scriptLoader->resource())
    value->setString("url", scriptLoader->resource()->url().getString());
  if (element->ownerDocument() && element->ownerDocument()->frame()) {
    value->setString(
        "frame",
        String::format("0x%" PRIx64,
                       static_cast<uint64_t>(reinterpret_cast<intptr_t>(
                           element->ownerDocument()->frame()))));
  }
  if (textPosition.m_line.zeroBasedInt() > 0 ||
      textPosition.m_column.zeroBasedInt() > 0) {
    value->setInteger("lineNumber", textPosition.m_line.oneBasedInt());
    value->setInteger("columnNumber", textPosition.m_column.oneBasedInt());
  }
  return value;
}

bool doExecuteScript(Element* scriptElement,
                     const ScriptSourceCode& sourceCode,
                     const TextPosition& textPosition) {
  ScriptLoader* scriptLoader = toScriptLoaderIfPossible(scriptElement);
  DCHECK(scriptLoader);
  TRACE_EVENT_WITH_FLOW1(
      "blink", "HTMLParserScriptRunner ExecuteScript", scriptElement,
      TRACE_EVENT_FLAG_FLOW_IN, "data",
      getTraceArgsForScriptElement(scriptElement, textPosition));
  return scriptLoader->executeScript(sourceCode);
}

void traceParserBlockingScript(const PendingScript* pendingScript,
                               bool waitingForResources) {
  // The HTML parser must yield before executing script in the following
  // cases:
  // * the script's execution is blocked on the completed load of the script
  //   resource
  //   (https://html.spec.whatwg.org/multipage/scripting.html#pending-parsing-blocking-script)
  // * the script's execution is blocked on the load of a style sheet or other
  //   resources that are blocking scripts
  //   (https://html.spec.whatwg.org/multipage/semantics.html#a-style-sheet-that-is-blocking-scripts)
  //
  // Both of these cases can introduce significant latency when loading a
  // web page, especially for users on slow connections, since the HTML parser
  // must yield until the blocking resources finish loading.
  //
  // We trace these parser yields here using flow events, so we can track
  // both when these yields occur, as well as how long the parser had
  // to yield. The connecting flow events are traced once the parser becomes
  // unblocked when the script actually executes, in doExecuteScript.
  Element* element = pendingScript->element();
  if (!element)
    return;
  TextPosition scriptStartPosition = pendingScript->startingPosition();
  if (!pendingScript->isReady()) {
    if (waitingForResources) {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "YieldParserForScriptLoadAndBlockingResources", element,
          TRACE_EVENT_FLAG_FLOW_OUT, "data",
          getTraceArgsForScriptElement(element, scriptStartPosition));
    } else {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "YieldParserForScriptLoad", element,
          TRACE_EVENT_FLAG_FLOW_OUT, "data",
          getTraceArgsForScriptElement(element, scriptStartPosition));
    }
  } else if (waitingForResources) {
    TRACE_EVENT_WITH_FLOW1(
        "blink", "YieldParserForScriptBlockingResources", element,
        TRACE_EVENT_FLAG_FLOW_OUT, "data",
        getTraceArgsForScriptElement(element, scriptStartPosition));
  }
}

static KURL documentURLForScriptExecution(Document* document) {
  if (!document)
    return KURL();

  if (!document->frame()) {
    if (document->importsController())
      return document->url();
    return KURL();
  }

  // Use the URL of the currently active document for this frame.
  return document->frame()->document()->url();
}

}  // namespace

using namespace HTMLNames;

HTMLParserScriptRunner::HTMLParserScriptRunner(
    HTMLParserReentryPermit* reentryPermit,
    Document* document,
    HTMLParserScriptRunnerHost* host)
    : m_reentryPermit(reentryPermit),
      m_document(document),
      m_host(host),
      m_parserBlockingScript(PendingScript::create(nullptr, nullptr)) {
  DCHECK(m_host);
}

HTMLParserScriptRunner::~HTMLParserScriptRunner() {
  // Verify that detach() has been called.
  DCHECK(!m_document);
}

void HTMLParserScriptRunner::detach() {
  if (!m_document)
    return;

  m_parserBlockingScript->dispose();

  while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
    PendingScript* pendingScript = m_scriptsToExecuteAfterParsing.takeFirst();
    pendingScript->dispose();
  }
  m_document = nullptr;
  // m_reentryPermit is not cleared here, because the script runner
  // may continue to run pending scripts after the parser has
  // detached.
}

bool HTMLParserScriptRunner::isParserBlockingScriptReady() {
  if (!m_document->isScriptExecutionReady())
    return false;
  return m_parserBlockingScript->isReady();
}

void HTMLParserScriptRunner::executePendingScriptAndDispatchEvent(
    PendingScript* pendingScript,
    ScriptStreamer::Type pendingScriptType) {
  bool errorOccurred = false;
  ScriptSourceCode sourceCode = pendingScript->getSource(
      documentURLForScriptExecution(m_document), errorOccurred);

  // Stop watching loads before executeScript to prevent recursion if the script
  // reloads itself.
  // TODO(kouhei): Consider merging this w/ pendingScript->dispose() after the
  // if block.
  pendingScript->stopWatchingForLoad();

  if (!isExecutingScript()) {
    Microtask::performCheckpoint(V8PerIsolateData::mainThreadIsolate());
    if (pendingScriptType == ScriptStreamer::ParsingBlocking) {
      // The parser cannot be unblocked as a microtask requested another
      // resource
      if (!m_document->isScriptExecutionReady())
        return;
    }
  }

  TextPosition scriptStartPosition = pendingScript->startingPosition();
  double scriptParserBlockingTime =
      pendingScript->parserBlockingLoadStartTime();
  // Clear the pending script before possible re-entrancy from executeScript()
  Element* element = pendingScript->element();
  pendingScript->dispose();

  if (ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element)) {
    HTMLParserReentryPermit::ScriptNestingLevelIncrementer
        nestingLevelIncrementer =
            m_reentryPermit->incrementScriptNestingLevel();
    IgnoreDestructiveWriteCountIncrementer
        ignoreDestructiveWriteCountIncrementer(m_document);
    if (errorOccurred) {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "HTMLParserScriptRunner ExecuteScriptFailed", element,
          TRACE_EVENT_FLAG_FLOW_IN, "data",
          getTraceArgsForScriptElement(element, scriptStartPosition));
      scriptLoader->dispatchErrorEvent();
    } else {
      DCHECK(isExecutingScript());
      if (scriptParserBlockingTime > 0.0) {
        DocumentParserTiming::from(*m_document)
            .recordParserBlockedOnScriptLoadDuration(
                monotonicallyIncreasingTime() - scriptParserBlockingTime,
                scriptLoader->wasCreatedDuringDocumentWrite());
      }
      if (!doExecuteScript(element, sourceCode, scriptStartPosition)) {
        scriptLoader->dispatchErrorEvent();
      } else {
        element->dispatchEvent(Event::create(EventTypeNames::load));
      }
    }
  }

  DCHECK(!isExecutingScript());
}

void fetchBlockedDocWriteScript(Element* script,
                                bool isParserInserted,
                                const TextPosition& scriptStartPosition) {
  DCHECK(script);

  ScriptLoader* scriptLoader =
      ScriptLoader::create(script, isParserInserted, false, false);
  DCHECK(scriptLoader);
  scriptLoader->setFetchDocWrittenScriptDeferIdle();
  scriptLoader->prepareScript(scriptStartPosition);
}

void emitWarningForDocWriteScripts(const String& url, Document& document) {
  String message =
      "The Parser-blocking, cross site (i.e. different eTLD+1) "
      "script, " +
      url +
      ", invoked via document.write was NOT BLOCKED on this page load, but MAY "
      "be blocked by the browser in future page loads with poor network "
      "connectivity.";
  document.addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
  WTFLogAlways("%s", message.utf8().data());
}

void emitErrorForDocWriteScripts(const String& url, Document& document) {
  String message =
      "The Parser-blocking, cross site (i.e. different eTLD+1) "
      "script, " +
      url +
      ", invoked via document.write was BLOCKED by the browser due to poor "
      "network connectivity. ";
  document.addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
  WTFLogAlways("%s", message.utf8().data());
}

void HTMLParserScriptRunner::possiblyFetchBlockedDocWriteScript(
    PendingScript* pendingScript) {
  // If the script was blocked as part of document.write intervention,
  // then send an asynchronous GET request with an interventions header.
  TextPosition startingPosition;
  bool isParserInserted = false;

  if (m_parserBlockingScript != pendingScript)
    return;

  Element* element = m_parserBlockingScript->element();
  if (!element)
    return;

  ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element);
  if (!scriptLoader || !scriptLoader->disallowedFetchForDocWrittenScript())
    return;

  if (!pendingScript->errorOccurred()) {
    emitWarningForDocWriteScripts(pendingScript->resource()->url().getString(),
                                  *m_document);
    return;
  }

  // Due to dependency violation, not able to check the exact error to be
  // ERR_CACHE_MISS but other errors are rare with
  // WebCachePolicy::ReturnCacheDataDontLoad.

  emitErrorForDocWriteScripts(pendingScript->resource()->url().getString(),
                              *m_document);
  startingPosition = m_parserBlockingScript->startingPosition();
  isParserInserted = scriptLoader->isParserInserted();
  // Remove this resource entry from memory cache as the new request
  // should not join onto this existing entry.
  memoryCache()->remove(pendingScript->resource());
  fetchBlockedDocWriteScript(element, isParserInserted, startingPosition);
}

void HTMLParserScriptRunner::pendingScriptFinished(
    PendingScript* pendingScript) {
  // Handle cancellations of parser-blocking script loads without
  // notifying the host (i.e., parser) if these were initiated by nested
  // document.write()s. The cancellation may have been triggered by
  // script execution to signal an abrupt stop (e.g., window.close().)
  //
  // The parser is unprepared to be told, and doesn't need to be.
  if (isExecutingScript() && pendingScript->resource()->wasCanceled()) {
    pendingScript->dispose();
    return;
  }

  // If the script was blocked as part of document.write intervention,
  // then send an asynchronous GET request with an interventions header.
  possiblyFetchBlockedDocWriteScript(pendingScript);

  m_host->notifyScriptLoaded(pendingScript);
}

// Implements the steps for 'An end tag whose tag name is "script"'
// http://whatwg.org/html#scriptEndTag
// Script handling lives outside the tree builder to keep each class simple.
void HTMLParserScriptRunner::processScriptElement(
    Element* scriptElement,
    const TextPosition& scriptStartPosition) {
  DCHECK(scriptElement);
  TRACE_EVENT1(
      "blink", "HTMLParserScriptRunner::execute", "data",
      getTraceArgsForScriptElement(scriptElement, scriptStartPosition));
  // FIXME: If scripting is disabled, always just return.

  bool hadPreloadScanner = m_host->hasPreloadScanner();

  // Try to execute the script given to us.
  processScriptElementInternal(scriptElement, scriptStartPosition);

  if (hasParserBlockingScript()) {
    if (isExecutingScript()) {
      // Unwind to the outermost HTMLParserScriptRunner::processScriptElement
      // before continuing parsing.
      return;
    }

    traceParserBlockingScript(m_parserBlockingScript.get(),
                              !m_document->isScriptExecutionReady());
    m_parserBlockingScript->markParserBlockingLoadStartTime();

    // If preload scanner got created, it is missing the source after the
    // current insertion point. Append it and scan.
    if (!hadPreloadScanner && m_host->hasPreloadScanner())
      m_host->appendCurrentInputStreamToPreloadScannerAndScan();
    executeParsingBlockingScripts();
  }
}

bool HTMLParserScriptRunner::hasParserBlockingScript() const {
  return !!m_parserBlockingScript->element();
}

void HTMLParserScriptRunner::executeParsingBlockingScripts() {
  while (hasParserBlockingScript() && isParserBlockingScriptReady()) {
    DCHECK(m_document);
    DCHECK(!isExecutingScript());
    DCHECK(m_document->isScriptExecutionReady());

    InsertionPointRecord insertionPointRecord(m_host->inputStream());
    executePendingScriptAndDispatchEvent(m_parserBlockingScript.get(),
                                         ScriptStreamer::ParsingBlocking);
  }
}

void HTMLParserScriptRunner::executeScriptsWaitingForLoad(
    PendingScript* pendingScript) {
  TRACE_EVENT0("blink", "HTMLParserScriptRunner::executeScriptsWaitingForLoad");
  DCHECK(!isExecutingScript());
  DCHECK(hasParserBlockingScript());
  DCHECK_EQ(pendingScript, m_parserBlockingScript);
  DCHECK(m_parserBlockingScript->isReady());
  executeParsingBlockingScripts();
}

void HTMLParserScriptRunner::executeScriptsWaitingForResources() {
  TRACE_EVENT0("blink",
               "HTMLParserScriptRunner::executeScriptsWaitingForResources");
  DCHECK(m_document);
  DCHECK(!isExecutingScript());
  DCHECK(m_document->isScriptExecutionReady());
  executeParsingBlockingScripts();
}

bool HTMLParserScriptRunner::executeScriptsWaitingForParsing() {
  TRACE_EVENT0("blink",
               "HTMLParserScriptRunner::executeScriptsWaitingForParsing");
  while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
    DCHECK(!isExecutingScript());
    DCHECK(!hasParserBlockingScript());
    DCHECK(m_scriptsToExecuteAfterParsing.first()->resource());
    if (!m_scriptsToExecuteAfterParsing.first()->isReady()) {
      m_scriptsToExecuteAfterParsing.first()->watchForLoad(this);
      traceParserBlockingScript(m_scriptsToExecuteAfterParsing.first().get(),
                                !m_document->isScriptExecutionReady());
      m_scriptsToExecuteAfterParsing.first()->markParserBlockingLoadStartTime();
      return false;
    }
    PendingScript* first = m_scriptsToExecuteAfterParsing.takeFirst();
    executePendingScriptAndDispatchEvent(first, ScriptStreamer::Deferred);
    // FIXME: What is this m_document check for?
    if (!m_document)
      return false;
  }
  return true;
}

void HTMLParserScriptRunner::requestParsingBlockingScript(Element* element) {
  if (!requestPendingScript(m_parserBlockingScript.get(), element))
    return;

  DCHECK(m_parserBlockingScript->resource());

  // We only care about a load callback if resource is not already in the cache.
  // Callers will attempt to run the m_parserBlockingScript if possible before
  // returning control to the parser.
  if (!m_parserBlockingScript->isReady()) {
    if (m_document->frame()) {
      ScriptState* scriptState = ScriptState::forMainWorld(m_document->frame());
      if (scriptState) {
        ScriptStreamer::startStreaming(
            m_parserBlockingScript.get(), ScriptStreamer::ParsingBlocking,
            m_document->frame()->settings(), scriptState,
            TaskRunnerHelper::get(TaskType::Networking, m_document));
      }
    }

    m_parserBlockingScript->watchForLoad(this);
  }
}

void HTMLParserScriptRunner::requestDeferredScript(Element* element) {
  PendingScript* pendingScript = PendingScript::create(nullptr, nullptr);
  if (!requestPendingScript(pendingScript, element))
    return;

  if (m_document->frame() && !pendingScript->isReady()) {
    ScriptState* scriptState = ScriptState::forMainWorld(m_document->frame());
    if (scriptState) {
      ScriptStreamer::startStreaming(
          pendingScript, ScriptStreamer::Deferred,
          m_document->frame()->settings(), scriptState,
          TaskRunnerHelper::get(TaskType::Networking, m_document));
    }
  }

  DCHECK(pendingScript->resource());
  m_scriptsToExecuteAfterParsing.append(pendingScript);
}

bool HTMLParserScriptRunner::requestPendingScript(PendingScript* pendingScript,
                                                  Element* script) const {
  DCHECK(!pendingScript->element());
  pendingScript->setElement(script);
  // This should correctly return 0 for empty or invalid srcValues.
  ScriptResource* resource = toScriptLoaderIfPossible(script)->resource();
  if (!resource) {
    DVLOG(1) << "Not implemented.";  // Dispatch error event.
    return false;
  }
  pendingScript->setScriptResource(resource);
  return true;
}

// Implements the initial steps for 'An end tag whose tag name is "script"'
// http://whatwg.org/html#scriptEndTag
void HTMLParserScriptRunner::processScriptElementInternal(
    Element* script,
    const TextPosition& scriptStartPosition) {
  DCHECK(m_document);
  DCHECK(!hasParserBlockingScript());
  {
    ScriptLoader* scriptLoader = toScriptLoaderIfPossible(script);

    // This contains both a DCHECK and a null check since we should not
    // be getting into the case of a null script element, but seem to be from
    // time to time. The assertion is left in to help find those cases and
    // is being tracked by <https://bugs.webkit.org/show_bug.cgi?id=60559>.
    DCHECK(scriptLoader);
    if (!scriptLoader)
      return;

    DCHECK(scriptLoader->isParserInserted());

    if (!isExecutingScript())
      Microtask::performCheckpoint(V8PerIsolateData::mainThreadIsolate());

    InsertionPointRecord insertionPointRecord(m_host->inputStream());
    HTMLParserReentryPermit::ScriptNestingLevelIncrementer
        nestingLevelIncrementer =
            m_reentryPermit->incrementScriptNestingLevel();

    scriptLoader->prepareScript(scriptStartPosition);

    if (!scriptLoader->willBeParserExecuted())
      return;

    if (scriptLoader->willExecuteWhenDocumentFinishedParsing()) {
      requestDeferredScript(script);
    } else if (scriptLoader->readyToBeParserExecuted()) {
      if (m_reentryPermit->scriptNestingLevel() == 1u) {
        m_parserBlockingScript->setElement(script);
        m_parserBlockingScript->setStartingPosition(scriptStartPosition);
      } else {
        DCHECK_GT(m_reentryPermit->scriptNestingLevel(), 1u);
        m_parserBlockingScript->dispose();
        ScriptSourceCode sourceCode(script->textContent(),
                                    documentURLForScriptExecution(m_document),
                                    scriptStartPosition);
        doExecuteScript(script, sourceCode, scriptStartPosition);
      }
    } else {
      requestParsingBlockingScript(script);
    }
  }
}

DEFINE_TRACE(HTMLParserScriptRunner) {
  visitor->trace(m_document);
  visitor->trace(m_host);
  visitor->trace(m_parserBlockingScript);
  visitor->trace(m_scriptsToExecuteAfterParsing);
  PendingScriptClient::trace(visitor);
}

}  // namespace blink
