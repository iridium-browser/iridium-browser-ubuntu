/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/parser/BackgroundHTMLParser.h"

#include "core/HTMLNames.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/html/parser/XSSAuditor.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/TextPosition.h"
#include <memory>

namespace blink {

// On a network with high latency and high bandwidth, using a device with a fast
// CPU, we could end up speculatively tokenizing the whole document, well ahead
// of when the main-thread actually needs it. This is a waste of memory (and
// potentially time if the speculation fails). So we limit our outstanding
// tokens arbitrarily to 10,000. Our maximal memory spent speculating will be
// approximately:
// (defaultOutstandingTokenLimit + defaultPendingTokenLimit) *
// sizeof(CompactToken)
//
// We use a separate low and high water mark to avoid
// constantly topping off the main thread's token buffer. At time of writing,
// this is (10000 + 1000) * 28 bytes = ~308kb of memory. These numbers have not
// been tuned.
static const size_t defaultOutstandingTokenLimit = 10000;

// We limit our chucks to 1000 tokens, to make sure the main thread is never
// waiting on the parser thread for tokens. This was tuned in
// https://bugs.webkit.org/show_bug.cgi?id=110408.
static const size_t defaultPendingTokenLimit = 1000;

using namespace HTMLNames;

#if DCHECK_IS_ON()

static void checkThatTokensAreSafeToSendToAnotherThread(
    const CompactHTMLTokenStream* tokens) {
  for (size_t i = 0; i < tokens->size(); ++i)
    ASSERT(tokens->at(i).isSafeToSendToAnotherThread());
}

static void checkThatPreloadsAreSafeToSendToAnotherThread(
    const PreloadRequestStream& preloads) {
  for (size_t i = 0; i < preloads.size(); ++i)
    ASSERT(preloads[i]->isSafeToSendToAnotherThread());
}

static void checkThatXSSInfosAreSafeToSendToAnotherThread(
    const XSSInfoStream& infos) {
  for (size_t i = 0; i < infos.size(); ++i)
    ASSERT(infos[i]->isSafeToSendToAnotherThread());
}

#endif

WeakPtr<BackgroundHTMLParser> BackgroundHTMLParser::create(
    std::unique_ptr<Configuration> config,
    RefPtr<WebTaskRunner> loadingTaskRunner) {
  auto* backgroundParser =
      new BackgroundHTMLParser(std::move(config), std::move(loadingTaskRunner));
  return backgroundParser->m_weakFactory.createWeakPtr();
}

void BackgroundHTMLParser::init(
    const KURL& documentURL,
    std::unique_ptr<CachedDocumentParameters> cachedDocumentParameters,
    const MediaValuesCached::MediaValuesCachedData& mediaValuesCachedData) {
  m_preloadScanner.reset(new TokenPreloadScanner(
      documentURL, std::move(cachedDocumentParameters), mediaValuesCachedData));
}

BackgroundHTMLParser::Configuration::Configuration()
    : outstandingTokenLimit(defaultOutstandingTokenLimit),
      pendingTokenLimit(defaultPendingTokenLimit),
      shouldCoalesceChunks(false) {}

BackgroundHTMLParser::BackgroundHTMLParser(
    std::unique_ptr<Configuration> config,
    RefPtr<WebTaskRunner> loadingTaskRunner)
    : m_weakFactory(this),
      m_token(WTF::wrapUnique(new HTMLToken)),
      m_tokenizer(HTMLTokenizer::create(config->options)),
      m_treeBuilderSimulator(config->options),
      m_options(config->options),
      m_outstandingTokenLimit(config->outstandingTokenLimit),
      m_parser(config->parser),
      m_pendingTokens(WTF::wrapUnique(new CompactHTMLTokenStream)),
      m_pendingTokenLimit(config->pendingTokenLimit),
      m_xssAuditor(std::move(config->xssAuditor)),
      m_decoder(std::move(config->decoder)),
      m_loadingTaskRunner(std::move(loadingTaskRunner)),
      m_tokenizedChunkQueue(std::move(config->tokenizedChunkQueue)),
      m_pendingCSPMetaTokenIndex(
          HTMLDocumentParser::TokenizedChunk::noPendingToken),
      m_startingScript(false),
      m_lastBytesReceivedTime(0.0),
      m_shouldCoalesceChunks(config->shouldCoalesceChunks) {
  ASSERT(m_outstandingTokenLimit > 0);
  ASSERT(m_pendingTokenLimit > 0);
  ASSERT(m_outstandingTokenLimit >= m_pendingTokenLimit);
}

BackgroundHTMLParser::~BackgroundHTMLParser() {}

void BackgroundHTMLParser::appendRawBytesFromMainThread(
    std::unique_ptr<Vector<char>> buffer,
    double bytesReceivedTime) {
  ASSERT(m_decoder);
  m_lastBytesReceivedTime = bytesReceivedTime;
  DEFINE_STATIC_LOCAL(CustomCountHistogram, queueDelay,
                      ("Parser.AppendBytesDelay", 1, 5000, 50));
  queueDelay.count(monotonicallyIncreasingTimeMS() - bytesReceivedTime);
  updateDocument(m_decoder->decode(buffer->data(), buffer->size()));
}

void BackgroundHTMLParser::appendDecodedBytes(const String& input) {
  ASSERT(!m_input.current().isClosed());
  m_input.append(input);
  pumpTokenizer();
}

void BackgroundHTMLParser::setDecoder(
    std::unique_ptr<TextResourceDecoder> decoder) {
  ASSERT(decoder);
  m_decoder = std::move(decoder);
}

void BackgroundHTMLParser::flush() {
  ASSERT(m_decoder);
  updateDocument(m_decoder->flush());
}

void BackgroundHTMLParser::updateDocument(const String& decodedData) {
  DocumentEncodingData encodingData(*m_decoder.get());

  if (encodingData != m_lastSeenEncodingData) {
    m_lastSeenEncodingData = encodingData;

    m_xssAuditor->setEncoding(encodingData.encoding());
    runOnMainThread(
        &HTMLDocumentParser::didReceiveEncodingDataFromBackgroundParser,
        m_parser, encodingData);
  }

  if (decodedData.isEmpty())
    return;

  appendDecodedBytes(decodedData);
}

void BackgroundHTMLParser::resumeFrom(std::unique_ptr<Checkpoint> checkpoint) {
  m_parser = checkpoint->parser;
  m_token = std::move(checkpoint->token);
  m_tokenizer = std::move(checkpoint->tokenizer);
  m_treeBuilderSimulator.setState(checkpoint->treeBuilderState);
  m_input.rewindTo(checkpoint->inputCheckpoint, checkpoint->unparsedInput);
  m_preloadScanner->rewindTo(checkpoint->preloadScannerCheckpoint);
  m_startingScript = false;
  m_tokenizedChunkQueue->clear();
  m_lastBytesReceivedTime = monotonicallyIncreasingTimeMS();
  pumpTokenizer();
}

void BackgroundHTMLParser::startedChunkWithCheckpoint(
    HTMLInputCheckpoint inputCheckpoint) {
  // Note, we should not have to worry about the index being invalid as messages
  // from the main thread will be processed in FIFO order.
  m_input.invalidateCheckpointsBefore(inputCheckpoint);
  pumpTokenizer();
}

void BackgroundHTMLParser::finish() {
  markEndOfFile();
  pumpTokenizer();
}

void BackgroundHTMLParser::stop() {
  delete this;
}

void BackgroundHTMLParser::forcePlaintextForTextDocument() {
  // This is only used by the TextDocumentParser (a subclass of
  // HTMLDocumentParser) to force us into the PLAINTEXT state w/o using a
  // <plaintext> tag. The TextDocumentParser uses a <pre> tag for historical /
  // compatibility reasons.
  m_tokenizer->setState(HTMLTokenizer::PLAINTEXTState);
}

void BackgroundHTMLParser::markEndOfFile() {
  ASSERT(!m_input.current().isClosed());
  m_input.append(String(&kEndOfFileMarker, 1));
  m_input.close();
}

void BackgroundHTMLParser::pumpTokenizer() {
  TRACE_EVENT0("loading", "BackgroundHTMLParser::pumpTokenizer");
  HTMLTreeBuilderSimulator::SimulatedToken simulatedToken =
      HTMLTreeBuilderSimulator::OtherToken;

  // No need to start speculating until the main thread has almost caught up.
  if (m_input.totalCheckpointTokenCount() > m_outstandingTokenLimit)
    return;

  bool shouldNotifyMainThread = false;
  while (true) {
    if (m_xssAuditor->isEnabled())
      m_sourceTracker.start(m_input.current(), m_tokenizer.get(), *m_token);

    if (!m_tokenizer->nextToken(m_input.current(), *m_token)) {
      // We've reached the end of our current input.
      shouldNotifyMainThread |= queueChunkForMainThread();
      break;
    }

    if (m_xssAuditor->isEnabled())
      m_sourceTracker.end(m_input.current(), m_tokenizer.get(), *m_token);

    {
      TextPosition position = TextPosition(m_input.current().currentLine(),
                                           m_input.current().currentColumn());

      if (std::unique_ptr<XSSInfo> xssInfo = m_xssAuditor->filterToken(
              FilterTokenRequest(*m_token, m_sourceTracker,
                                 m_tokenizer->shouldAllowCDATA()))) {
        xssInfo->m_textPosition = position;
        m_pendingXSSInfos.push_back(std::move(xssInfo));
      }

      CompactHTMLToken token(m_token.get(), position);

      bool shouldEvaluateForDocumentWrite = false;
      bool isCSPMetaTag = false;
      m_preloadScanner->scan(token, m_input.current(), m_pendingPreloads,
                             &m_viewportDescription, &isCSPMetaTag,
                             &shouldEvaluateForDocumentWrite);

      simulatedToken =
          m_treeBuilderSimulator.simulate(token, m_tokenizer.get());

      // Break chunks before a script tag is inserted and flag the chunk as
      // starting a script so the main parser can decide if it should yield
      // before processing the chunk.
      if (simulatedToken == HTMLTreeBuilderSimulator::ScriptStart) {
        shouldNotifyMainThread |= queueChunkForMainThread();
        m_startingScript = true;
      }

      m_pendingTokens->push_back(token);
      if (isCSPMetaTag) {
        m_pendingCSPMetaTokenIndex = m_pendingTokens->size() - 1;
      }
      if (shouldEvaluateForDocumentWrite) {
        m_likelyDocumentWriteScriptIndices.push_back(m_pendingTokens->size() -
                                                     1);
      }
    }

    m_token->clear();

    if (simulatedToken == HTMLTreeBuilderSimulator::ScriptEnd ||
        m_pendingTokens->size() >= m_pendingTokenLimit) {
      shouldNotifyMainThread |= queueChunkForMainThread();
      // If we're far ahead of the main thread, yield for a bit to avoid
      // consuming too much memory.
      if (m_input.totalCheckpointTokenCount() > m_outstandingTokenLimit)
        break;
    }

    if (!m_shouldCoalesceChunks && shouldNotifyMainThread) {
      runOnMainThread(&HTMLDocumentParser::notifyPendingTokenizedChunks,
                      m_parser);
      shouldNotifyMainThread = false;
    }
  }
  // Wait to notify the main thread about the chunks until we're at the limit.
  // This lets the background parser generate lots of valuable preloads before
  // anything expensive (extensions, scripts) take up time on the main thread. A
  // busy main thread can cause preload delays.
  if (shouldNotifyMainThread) {
    runOnMainThread(&HTMLDocumentParser::notifyPendingTokenizedChunks,
                    m_parser);
  }
}

bool BackgroundHTMLParser::queueChunkForMainThread() {
  if (m_pendingTokens->isEmpty())
    return false;

#if DCHECK_IS_ON()
  checkThatTokensAreSafeToSendToAnotherThread(m_pendingTokens.get());
  checkThatPreloadsAreSafeToSendToAnotherThread(m_pendingPreloads);
  checkThatXSSInfosAreSafeToSendToAnotherThread(m_pendingXSSInfos);
#endif

  double chunkStartTime = monotonicallyIncreasingTimeMS();
  std::unique_ptr<HTMLDocumentParser::TokenizedChunk> chunk =
      WTF::wrapUnique(new HTMLDocumentParser::TokenizedChunk);
  TRACE_EVENT_WITH_FLOW0("blink,loading",
                         "BackgroundHTMLParser::sendTokensToMainThread",
                         chunk.get(), TRACE_EVENT_FLAG_FLOW_OUT);

  if (!m_pendingPreloads.isEmpty()) {
    double delay = monotonicallyIncreasingTimeMS() - m_lastBytesReceivedTime;
    DEFINE_STATIC_LOCAL(CustomCountHistogram, preloadTokenizeDelay,
                        ("Parser.PreloadTokenizeDelay", 1, 10000, 50));
    preloadTokenizeDelay.count(delay);
  }

  chunk->preloads.swap(m_pendingPreloads);
  if (m_viewportDescription.set)
    chunk->viewport = m_viewportDescription;
  chunk->xssInfos.swap(m_pendingXSSInfos);
  chunk->tokenizerState = m_tokenizer->getState();
  chunk->treeBuilderState = m_treeBuilderSimulator.state();
  chunk->inputCheckpoint = m_input.createCheckpoint(m_pendingTokens->size());
  chunk->preloadScannerCheckpoint = m_preloadScanner->createCheckpoint();
  chunk->tokens = std::move(m_pendingTokens);
  chunk->startingScript = m_startingScript;
  chunk->likelyDocumentWriteScriptIndices.swap(
      m_likelyDocumentWriteScriptIndices);
  chunk->pendingCSPMetaTokenIndex = m_pendingCSPMetaTokenIndex;
  m_startingScript = false;
  m_pendingCSPMetaTokenIndex =
      HTMLDocumentParser::TokenizedChunk::noPendingToken;

  bool isEmpty = m_tokenizedChunkQueue->enqueue(std::move(chunk));

  DEFINE_STATIC_LOCAL(CustomCountHistogram, chunkEnqueueTime,
                      ("Parser.ChunkEnqueueTime", 1, 10000, 50));
  chunkEnqueueTime.count(monotonicallyIncreasingTimeMS() - chunkStartTime);

  m_pendingTokens = WTF::wrapUnique(new CompactHTMLTokenStream);
  return isEmpty;
}

// If the background parser is already running on the main thread, then it is
// not necessary to post a task to the main thread to run asynchronously. The
// main parser deals with chunking up its own work.
// TODO(csharrison): This is a pretty big hack because we don't actually need a
// CrossThreadClosure in these cases. This is just experimental.
template <typename FunctionType, typename... Ps>
void BackgroundHTMLParser::runOnMainThread(FunctionType function,
                                           Ps&&... parameters) {
  if (isMainThread()) {
    (*WTF::bind(function, std::forward<Ps>(parameters)...))();
  } else {
    m_loadingTaskRunner->postTask(
        BLINK_FROM_HERE,
        crossThreadBind(function, std::forward<Ps>(parameters)...));
  }
}

}  // namespace blink
