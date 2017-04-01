/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/inspector/InspectorPageAgent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptRegexp.h"
#include "core/HTMLNames.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/VoidCallback.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/DOMPatchSupport.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/inspector/V8InspectorString.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/PlatformResourceLoader.h"
#include "platform/UserGestureIndicator.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"
#include "wtf/ListHashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/Base64.h"
#include "wtf/text/TextEncoding.h"
#include <memory>

namespace blink {

namespace PageAgentState {
static const char pageAgentEnabled[] = "pageAgentEnabled";
static const char pageAgentScriptsToEvaluateOnLoad[] =
    "pageAgentScriptsToEvaluateOnLoad";
static const char screencastEnabled[] = "screencastEnabled";
static const char autoAttachToCreatedPages[] = "autoAttachToCreatedPages";
static const char overlaySuspended[] = "overlaySuspended";
static const char overlayMessage[] = "overlayMessage";
}

namespace {

KURL urlWithoutFragment(const KURL& url) {
  KURL result = url;
  result.removeFragmentIdentifier();
  return result;
}

String frameId(LocalFrame* frame) {
  return frame ? IdentifiersFactory::frameId(frame) : "";
}

String dialogTypeToProtocol(ChromeClient::DialogType dialogType) {
  switch (dialogType) {
    case ChromeClient::AlertDialog:
      return protocol::Page::DialogTypeEnum::Alert;
    case ChromeClient::ConfirmDialog:
      return protocol::Page::DialogTypeEnum::Confirm;
    case ChromeClient::PromptDialog:
      return protocol::Page::DialogTypeEnum::Prompt;
    case ChromeClient::HTMLDialog:
      return protocol::Page::DialogTypeEnum::Beforeunload;
  }
  return protocol::Page::DialogTypeEnum::Alert;
}

}  // namespace

static bool prepareResourceBuffer(Resource* cachedResource, bool* hasZeroSize) {
  if (!cachedResource)
    return false;

  if (cachedResource->getDataBufferingPolicy() == DoNotBufferData)
    return false;

  // Zero-sized resources don't have data at all -- so fake the empty buffer,
  // instead of indicating error by returning 0.
  if (!cachedResource->encodedSize()) {
    *hasZeroSize = true;
    return true;
  }

  *hasZeroSize = false;
  return true;
}

static bool hasTextContent(Resource* cachedResource) {
  Resource::Type type = cachedResource->getType();
  return type == Resource::CSSStyleSheet || type == Resource::XSLStyleSheet ||
         type == Resource::Script || type == Resource::Raw ||
         type == Resource::ImportResource || type == Resource::MainResource;
}

static std::unique_ptr<TextResourceDecoder> createResourceTextDecoder(
    const String& mimeType,
    const String& textEncodingName) {
  if (!textEncodingName.isEmpty())
    return TextResourceDecoder::create("text/plain", textEncodingName);
  if (DOMImplementation::isXMLMIMEType(mimeType)) {
    std::unique_ptr<TextResourceDecoder> decoder =
        TextResourceDecoder::create("application/xml");
    decoder->useLenientXMLDecoding();
    return decoder;
  }
  if (equalIgnoringCase(mimeType, "text/html"))
    return TextResourceDecoder::create("text/html", "UTF-8");
  if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType) ||
      DOMImplementation::isJSONMIMEType(mimeType))
    return TextResourceDecoder::create("text/plain", "UTF-8");
  if (DOMImplementation::isTextMIMEType(mimeType))
    return TextResourceDecoder::create("text/plain", "ISO-8859-1");
  return std::unique_ptr<TextResourceDecoder>();
}

static void maybeEncodeTextContent(const String& textContent,
                                   PassRefPtr<const SharedBuffer> buffer,
                                   String* result,
                                   bool* base64Encoded) {
  if (!textContent.isNull() &&
      !textContent.utf8(WTF::StrictUTF8Conversion).isNull()) {
    *result = textContent;
    *base64Encoded = false;
  } else if (buffer) {
    *result = base64Encode(buffer->data(), buffer->size());
    *base64Encoded = true;
  } else if (textContent.isNull()) {
    *result = "";
    *base64Encoded = false;
  } else {
    DCHECK(!textContent.is8Bit());
    *result = base64Encode(textContent.utf8(WTF::LenientUTF8Conversion));
    *base64Encoded = true;
  }
}

// static
bool InspectorPageAgent::sharedBufferContent(
    PassRefPtr<const SharedBuffer> buffer,
    const String& mimeType,
    const String& textEncodingName,
    String* result,
    bool* base64Encoded) {
  if (!buffer)
    return false;

  String textContent;
  std::unique_ptr<TextResourceDecoder> decoder =
      createResourceTextDecoder(mimeType, textEncodingName);
  WTF::TextEncoding encoding(textEncodingName);

  if (decoder) {
    textContent = decoder->decode(buffer->data(), buffer->size());
    textContent = textContent + decoder->flush();
  } else if (encoding.isValid()) {
    textContent = encoding.decode(buffer->data(), buffer->size());
  }

  maybeEncodeTextContent(textContent, std::move(buffer), result, base64Encoded);
  return true;
}

// static
bool InspectorPageAgent::cachedResourceContent(Resource* cachedResource,
                                               String* result,
                                               bool* base64Encoded) {
  bool hasZeroSize;
  if (!prepareResourceBuffer(cachedResource, &hasZeroSize))
    return false;

  if (!hasTextContent(cachedResource)) {
    RefPtr<const SharedBuffer> buffer =
        hasZeroSize ? SharedBuffer::create() : cachedResource->resourceBuffer();
    if (!buffer)
      return false;
    *result = base64Encode(buffer->data(), buffer->size());
    *base64Encoded = true;
    return true;
  }

  if (hasZeroSize) {
    *result = "";
    *base64Encoded = false;
    return true;
  }

  DCHECK(cachedResource);
  switch (cachedResource->getType()) {
    case Resource::CSSStyleSheet:
      maybeEncodeTextContent(
          toCSSStyleSheetResource(cachedResource)
              ->sheetText(CSSStyleSheetResource::MIMETypeCheck::Lax),
          cachedResource->resourceBuffer(), result, base64Encoded);
      return true;
    case Resource::Script:
      maybeEncodeTextContent(
          cachedResource->resourceBuffer()
              ? toScriptResource(cachedResource)->decodedText()
              : toScriptResource(cachedResource)->script(),
          cachedResource->resourceBuffer(), result, base64Encoded);
      return true;
    default:
      String textEncodingName = cachedResource->response().textEncodingName();
      if (textEncodingName.isEmpty() &&
          cachedResource->getType() != Resource::Raw)
        textEncodingName = "WinLatin1";
      return InspectorPageAgent::sharedBufferContent(
          cachedResource->resourceBuffer(),
          cachedResource->response().mimeType(), textEncodingName, result,
          base64Encoded);
  }
}

InspectorPageAgent* InspectorPageAgent::create(
    InspectedFrames* inspectedFrames,
    Client* client,
    InspectorResourceContentLoader* resourceContentLoader,
    v8_inspector::V8InspectorSession* v8Session) {
  return new InspectorPageAgent(inspectedFrames, client, resourceContentLoader,
                                v8Session);
}

Resource* InspectorPageAgent::cachedResource(LocalFrame* frame,
                                             const KURL& url) {
  Document* document = frame->document();
  if (!document)
    return nullptr;
  Resource* cachedResource = document->fetcher()->cachedResource(url);
  if (!cachedResource) {
    HeapVector<Member<Document>> allImports =
        InspectorPageAgent::importsForFrame(frame);
    for (Document* import : allImports) {
      cachedResource = import->fetcher()->cachedResource(url);
      if (cachedResource)
        break;
    }
  }
  if (!cachedResource)
    cachedResource = memoryCache()->resourceForURL(
        url, document->fetcher()->getCacheIdentifier());
  return cachedResource;
}

String InspectorPageAgent::resourceTypeJson(
    InspectorPageAgent::ResourceType resourceType) {
  switch (resourceType) {
    case DocumentResource:
      return protocol::Page::ResourceTypeEnum::Document;
    case FontResource:
      return protocol::Page::ResourceTypeEnum::Font;
    case ImageResource:
      return protocol::Page::ResourceTypeEnum::Image;
    case MediaResource:
      return protocol::Page::ResourceTypeEnum::Media;
    case ScriptResource:
      return protocol::Page::ResourceTypeEnum::Script;
    case StylesheetResource:
      return protocol::Page::ResourceTypeEnum::Stylesheet;
    case TextTrackResource:
      return protocol::Page::ResourceTypeEnum::TextTrack;
    case XHRResource:
      return protocol::Page::ResourceTypeEnum::XHR;
    case FetchResource:
      return protocol::Page::ResourceTypeEnum::Fetch;
    case EventSourceResource:
      return protocol::Page::ResourceTypeEnum::EventSource;
    case WebSocketResource:
      return protocol::Page::ResourceTypeEnum::WebSocket;
    case ManifestResource:
      return protocol::Page::ResourceTypeEnum::Manifest;
    case OtherResource:
      return protocol::Page::ResourceTypeEnum::Other;
  }
  return protocol::Page::ResourceTypeEnum::Other;
}

InspectorPageAgent::ResourceType InspectorPageAgent::cachedResourceType(
    const Resource& cachedResource) {
  switch (cachedResource.getType()) {
    case Resource::Image:
      return InspectorPageAgent::ImageResource;
    case Resource::Font:
      return InspectorPageAgent::FontResource;
    case Resource::Media:
      return InspectorPageAgent::MediaResource;
    case Resource::Manifest:
      return InspectorPageAgent::ManifestResource;
    case Resource::TextTrack:
      return InspectorPageAgent::TextTrackResource;
    case Resource::CSSStyleSheet:
    // Fall through.
    case Resource::XSLStyleSheet:
      return InspectorPageAgent::StylesheetResource;
    case Resource::Script:
      return InspectorPageAgent::ScriptResource;
    case Resource::ImportResource:
    // Fall through.
    case Resource::MainResource:
      return InspectorPageAgent::DocumentResource;
    default:
      break;
  }
  return InspectorPageAgent::OtherResource;
}

String InspectorPageAgent::cachedResourceTypeJson(
    const Resource& cachedResource) {
  return resourceTypeJson(cachedResourceType(cachedResource));
}

InspectorPageAgent::InspectorPageAgent(
    InspectedFrames* inspectedFrames,
    Client* client,
    InspectorResourceContentLoader* resourceContentLoader,
    v8_inspector::V8InspectorSession* v8Session)
    : m_inspectedFrames(inspectedFrames),
      m_v8Session(v8Session),
      m_client(client),
      m_lastScriptIdentifier(0),
      m_enabled(false),
      m_reloading(false),
      m_inspectorResourceContentLoader(resourceContentLoader),
      m_resourceContentLoaderClientId(resourceContentLoader->createClientId()) {
}

void InspectorPageAgent::restore() {
  if (m_state->booleanProperty(PageAgentState::pageAgentEnabled, false))
    enable();
  if (m_client) {
    String overlayMessage;
    m_state->getString(PageAgentState::overlayMessage, &overlayMessage);
    m_client->configureOverlay(
        m_state->booleanProperty(PageAgentState::overlaySuspended, false),
        overlayMessage);
  }
}

Response InspectorPageAgent::enable() {
  m_enabled = true;
  m_state->setBoolean(PageAgentState::pageAgentEnabled, true);
  m_instrumentingAgents->addInspectorPageAgent(this);
  return Response::OK();
}

Response InspectorPageAgent::disable() {
  m_enabled = false;
  m_state->setBoolean(PageAgentState::pageAgentEnabled, false);
  m_state->remove(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
  m_scriptToEvaluateOnLoadOnce = String();
  m_pendingScriptToEvaluateOnLoadOnce = String();
  m_instrumentingAgents->removeInspectorPageAgent(this);
  m_inspectorResourceContentLoader->cancel(m_resourceContentLoaderClientId);

  stopScreencast();
  configureOverlay(false, String());

  finishReload();
  return Response::OK();
}

Response InspectorPageAgent::addScriptToEvaluateOnLoad(const String& source,
                                                       String* identifier) {
  protocol::DictionaryValue* scripts =
      m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
  if (!scripts) {
    std::unique_ptr<protocol::DictionaryValue> newScripts =
        protocol::DictionaryValue::create();
    scripts = newScripts.get();
    m_state->setObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad,
                       std::move(newScripts));
  }
  // Assure we don't override existing ids -- m_lastScriptIdentifier could get
  // out of sync WRT actual scripts once we restored the scripts from the cookie
  // during navigation.
  do {
    *identifier = String::number(++m_lastScriptIdentifier);
  } while (scripts->get(*identifier));
  scripts->setString(*identifier, source);
  return Response::OK();
}

Response InspectorPageAgent::removeScriptToEvaluateOnLoad(
    const String& identifier) {
  protocol::DictionaryValue* scripts =
      m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
  if (!scripts || !scripts->get(identifier))
    return Response::Error("Script not found");
  scripts->remove(identifier);
  return Response::OK();
}

Response InspectorPageAgent::setAutoAttachToCreatedPages(bool autoAttach) {
  m_state->setBoolean(PageAgentState::autoAttachToCreatedPages, autoAttach);
  return Response::OK();
}

Response InspectorPageAgent::reload(
    Maybe<bool> optionalBypassCache,
    Maybe<String> optionalScriptToEvaluateOnLoad) {
  m_pendingScriptToEvaluateOnLoadOnce =
      optionalScriptToEvaluateOnLoad.fromMaybe("");
  m_v8Session->setSkipAllPauses(true);
  m_reloading = true;
  m_inspectedFrames->root()->reload(optionalBypassCache.fromMaybe(false)
                                        ? FrameLoadTypeReloadBypassingCache
                                        : FrameLoadTypeReloadMainResource,
                                    ClientRedirectPolicy::NotClientRedirect);
  return Response::OK();
}

Response InspectorPageAgent::navigate(const String& url, String* outFrameId) {
  *outFrameId = frameId(m_inspectedFrames->root());
  return Response::OK();
}

Response InspectorPageAgent::stopLoading() {
  return Response::OK();
}

static void cachedResourcesForDocument(Document* document,
                                       HeapVector<Member<Resource>>& result,
                                       bool skipXHRs) {
  const ResourceFetcher::DocumentResourceMap& allResources =
      document->fetcher()->allResources();
  for (const auto& resource : allResources) {
    Resource* cachedResource = resource.value.get();
    if (!cachedResource)
      continue;

    // Skip images that were not auto loaded (images disabled in the user
    // agent), fonts that were referenced in CSS but never used/downloaded, etc.
    if (cachedResource->stillNeedsLoad())
      continue;
    if (cachedResource->getType() == Resource::Raw && skipXHRs)
      continue;
    result.push_back(cachedResource);
  }
}

// static
HeapVector<Member<Document>> InspectorPageAgent::importsForFrame(
    LocalFrame* frame) {
  HeapVector<Member<Document>> result;
  Document* rootDocument = frame->document();

  if (HTMLImportsController* controller = rootDocument->importsController()) {
    for (size_t i = 0; i < controller->loaderCount(); ++i) {
      if (Document* document = controller->loaderAt(i)->document())
        result.push_back(document);
    }
  }

  return result;
}

static HeapVector<Member<Resource>> cachedResourcesForFrame(LocalFrame* frame,
                                                            bool skipXHRs) {
  HeapVector<Member<Resource>> result;
  Document* rootDocument = frame->document();
  HeapVector<Member<Document>> loaders =
      InspectorPageAgent::importsForFrame(frame);

  cachedResourcesForDocument(rootDocument, result, skipXHRs);
  for (size_t i = 0; i < loaders.size(); ++i)
    cachedResourcesForDocument(loaders[i], result, skipXHRs);

  return result;
}

Response InspectorPageAgent::getResourceTree(
    std::unique_ptr<protocol::Page::FrameResourceTree>* object) {
  *object = buildObjectForFrameTree(m_inspectedFrames->root());
  return Response::OK();
}

void InspectorPageAgent::finishReload() {
  if (!m_reloading)
    return;
  m_reloading = false;
  m_v8Session->setSkipAllPauses(false);
}

void InspectorPageAgent::getResourceContentAfterResourcesContentLoaded(
    const String& frameId,
    const String& url,
    std::unique_ptr<GetResourceContentCallback> callback) {
  LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
  if (!frame) {
    callback->sendFailure(Response::Error("No frame for given id found"));
    return;
  }
  String content;
  bool base64Encoded;
  if (InspectorPageAgent::cachedResourceContent(
          InspectorPageAgent::cachedResource(frame, KURL(ParsedURLString, url)),
          &content, &base64Encoded))
    callback->sendSuccess(content, base64Encoded);
  else
    callback->sendFailure(Response::Error("No resource with given URL found"));
}

void InspectorPageAgent::getResourceContent(
    const String& frameId,
    const String& url,
    std::unique_ptr<GetResourceContentCallback> callback) {
  if (!m_enabled) {
    callback->sendFailure(Response::Error("Agent is not enabled."));
    return;
  }
  m_inspectorResourceContentLoader->ensureResourcesContentLoaded(
      m_resourceContentLoaderClientId,
      WTF::bind(
          &InspectorPageAgent::getResourceContentAfterResourcesContentLoaded,
          wrapPersistent(this), frameId, url,
          WTF::passed(std::move(callback))));
}

void InspectorPageAgent::searchContentAfterResourcesContentLoaded(
    const String& frameId,
    const String& url,
    const String& query,
    bool caseSensitive,
    bool isRegex,
    std::unique_ptr<SearchInResourceCallback> callback) {
  LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
  if (!frame) {
    callback->sendFailure(Response::Error("No frame for given id found"));
    return;
  }
  String content;
  bool base64Encoded;
  if (!InspectorPageAgent::cachedResourceContent(
          InspectorPageAgent::cachedResource(frame, KURL(ParsedURLString, url)),
          &content, &base64Encoded)) {
    callback->sendFailure(Response::Error("No resource with given URL found"));
    return;
  }

  auto matches = m_v8Session->searchInTextByLines(
      toV8InspectorStringView(content), toV8InspectorStringView(query),
      caseSensitive, isRegex);
  auto results = protocol::Array<
      v8_inspector::protocol::Debugger::API::SearchMatch>::create();
  for (size_t i = 0; i < matches.size(); ++i)
    results->addItem(std::move(matches[i]));
  callback->sendSuccess(std::move(results));
}

void InspectorPageAgent::searchInResource(
    const String& frameId,
    const String& url,
    const String& query,
    Maybe<bool> optionalCaseSensitive,
    Maybe<bool> optionalIsRegex,
    std::unique_ptr<SearchInResourceCallback> callback) {
  if (!m_enabled) {
    callback->sendFailure(Response::Error("Agent is not enabled."));
    return;
  }
  m_inspectorResourceContentLoader->ensureResourcesContentLoaded(
      m_resourceContentLoaderClientId,
      WTF::bind(&InspectorPageAgent::searchContentAfterResourcesContentLoaded,
                wrapPersistent(this), frameId, url, query,
                optionalCaseSensitive.fromMaybe(false),
                optionalIsRegex.fromMaybe(false),
                WTF::passed(std::move(callback))));
}

Response InspectorPageAgent::setDocumentContent(const String& frameId,
                                                const String& html) {
  LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
  if (!frame)
    return Response::Error("No frame for given id found");

  Document* document = frame->document();
  if (!document)
    return Response::Error("No Document instance to set HTML for");
  DOMPatchSupport::patchDocument(*document, html);
  return Response::OK();
}

void InspectorPageAgent::didClearDocumentOfWindowObject(LocalFrame* frame) {
  if (!frontend())
    return;

  protocol::DictionaryValue* scripts =
      m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
  if (scripts) {
    for (size_t i = 0; i < scripts->size(); ++i) {
      auto script = scripts->at(i);
      String scriptText;
      if (script.second->asString(&scriptText))
        frame->script().executeScriptInMainWorld(scriptText);
    }
  }
  if (!m_scriptToEvaluateOnLoadOnce.isEmpty())
    frame->script().executeScriptInMainWorld(m_scriptToEvaluateOnLoadOnce);
}

void InspectorPageAgent::domContentLoadedEventFired(LocalFrame* frame) {
  if (frame != m_inspectedFrames->root())
    return;
  frontend()->domContentEventFired(monotonicallyIncreasingTime());
}

void InspectorPageAgent::loadEventFired(LocalFrame* frame) {
  if (frame != m_inspectedFrames->root())
    return;
  frontend()->loadEventFired(monotonicallyIncreasingTime());
}

void InspectorPageAgent::didCommitLoad(LocalFrame*, DocumentLoader* loader) {
  if (loader->frame() == m_inspectedFrames->root()) {
    finishReload();
    m_scriptToEvaluateOnLoadOnce = m_pendingScriptToEvaluateOnLoadOnce;
    m_pendingScriptToEvaluateOnLoadOnce = String();
  }
  frontend()->frameNavigated(buildObjectForFrame(loader->frame()));
}

void InspectorPageAgent::frameAttachedToParent(LocalFrame* frame) {
  Frame* parentFrame = frame->tree().parent();
  if (!parentFrame->isLocalFrame())
    parentFrame = 0;
  frontend()->frameAttached(frameId(frame), frameId(toLocalFrame(parentFrame)));
}

void InspectorPageAgent::frameDetachedFromParent(LocalFrame* frame) {
  frontend()->frameDetached(frameId(frame));
}

bool InspectorPageAgent::screencastEnabled() {
  return m_enabled &&
         m_state->booleanProperty(PageAgentState::screencastEnabled, false);
}

void InspectorPageAgent::frameStartedLoading(LocalFrame* frame) {
  frontend()->frameStartedLoading(frameId(frame));
}

void InspectorPageAgent::frameStoppedLoading(LocalFrame* frame) {
  frontend()->frameStoppedLoading(frameId(frame));
}

void InspectorPageAgent::frameScheduledNavigation(LocalFrame* frame,
                                                  double delay) {
  frontend()->frameScheduledNavigation(frameId(frame), delay);
}

void InspectorPageAgent::frameClearedScheduledNavigation(LocalFrame* frame) {
  frontend()->frameClearedScheduledNavigation(frameId(frame));
}

void InspectorPageAgent::willRunJavaScriptDialog(
    const String& message,
    ChromeClient::DialogType dialogType) {
  frontend()->javascriptDialogOpening(message,
                                      dialogTypeToProtocol(dialogType));
  frontend()->flush();
}

void InspectorPageAgent::didRunJavaScriptDialog(bool result) {
  frontend()->javascriptDialogClosed(result);
  frontend()->flush();
}

void InspectorPageAgent::didUpdateLayout() {
  if (m_enabled && m_client)
    m_client->pageLayoutInvalidated(false);
}

void InspectorPageAgent::didResizeMainFrame() {
  if (!m_inspectedFrames->root()->isMainFrame())
    return;
#if !OS(ANDROID)
  if (m_enabled && m_client)
    m_client->pageLayoutInvalidated(true);
#endif
  frontend()->frameResized();
}

void InspectorPageAgent::didRecalculateStyle() {
  if (m_enabled && m_client)
    m_client->pageLayoutInvalidated(false);
}

void InspectorPageAgent::windowCreated(LocalFrame* created) {
  if (m_enabled &&
      m_state->booleanProperty(PageAgentState::autoAttachToCreatedPages, false))
    m_client->waitForCreateWindow(created);
}

std::unique_ptr<protocol::Page::Frame> InspectorPageAgent::buildObjectForFrame(
    LocalFrame* frame) {
  std::unique_ptr<protocol::Page::Frame> frameObject =
      protocol::Page::Frame::create()
          .setId(frameId(frame))
          .setLoaderId(
              IdentifiersFactory::loaderId(frame->loader().documentLoader()))
          .setUrl(urlWithoutFragment(frame->document()->url()).getString())
          .setMimeType(frame->loader().documentLoader()->responseMIMEType())
          .setSecurityOrigin(
              frame->document()->getSecurityOrigin()->toRawString())
          .build();
  // FIXME: This doesn't work for OOPI.
  Frame* parentFrame = frame->tree().parent();
  if (parentFrame && parentFrame->isLocalFrame())
    frameObject->setParentId(frameId(toLocalFrame(parentFrame)));
  if (frame->deprecatedLocalOwner()) {
    AtomicString name = frame->deprecatedLocalOwner()->getNameAttribute();
    if (name.isEmpty())
      name = frame->deprecatedLocalOwner()->getAttribute(HTMLNames::idAttr);
    frameObject->setName(name);
  }

  return frameObject;
}

std::unique_ptr<protocol::Page::FrameResourceTree>
InspectorPageAgent::buildObjectForFrameTree(LocalFrame* frame) {
  std::unique_ptr<protocol::Page::Frame> frameObject =
      buildObjectForFrame(frame);
  std::unique_ptr<protocol::Array<protocol::Page::FrameResource>> subresources =
      protocol::Array<protocol::Page::FrameResource>::create();

  HeapVector<Member<Resource>> allResources =
      cachedResourcesForFrame(frame, true);
  for (Resource* cachedResource : allResources) {
    std::unique_ptr<protocol::Page::FrameResource> resourceObject =
        protocol::Page::FrameResource::create()
            .setUrl(urlWithoutFragment(cachedResource->url()).getString())
            .setType(cachedResourceTypeJson(*cachedResource))
            .setMimeType(cachedResource->response().mimeType())
            .setLastModified(cachedResource->response().lastModified())
            .setContentSize(cachedResource->response().decodedBodyLength())
            .build();
    if (cachedResource->wasCanceled())
      resourceObject->setCanceled(true);
    else if (cachedResource->getStatus() == Resource::LoadError)
      resourceObject->setFailed(true);
    subresources->addItem(std::move(resourceObject));
  }

  HeapVector<Member<Document>> allImports =
      InspectorPageAgent::importsForFrame(frame);
  for (Document* import : allImports) {
    std::unique_ptr<protocol::Page::FrameResource> resourceObject =
        protocol::Page::FrameResource::create()
            .setUrl(urlWithoutFragment(import->url()).getString())
            .setType(resourceTypeJson(InspectorPageAgent::DocumentResource))
            .setMimeType(import->suggestedMIMEType())
            .build();
    subresources->addItem(std::move(resourceObject));
  }

  std::unique_ptr<protocol::Page::FrameResourceTree> result =
      protocol::Page::FrameResourceTree::create()
          .setFrame(std::move(frameObject))
          .setResources(std::move(subresources))
          .build();

  std::unique_ptr<protocol::Array<protocol::Page::FrameResourceTree>>
      childrenArray;
  for (Frame* child = frame->tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (!child->isLocalFrame())
      continue;
    if (!childrenArray)
      childrenArray =
          protocol::Array<protocol::Page::FrameResourceTree>::create();
    childrenArray->addItem(buildObjectForFrameTree(toLocalFrame(child)));
  }
  result->setChildFrames(std::move(childrenArray));
  return result;
}

Response InspectorPageAgent::startScreencast(Maybe<String> format,
                                             Maybe<int> quality,
                                             Maybe<int> maxWidth,
                                             Maybe<int> maxHeight,
                                             Maybe<int> everyNthFrame) {
  m_state->setBoolean(PageAgentState::screencastEnabled, true);
  return Response::OK();
}

Response InspectorPageAgent::stopScreencast() {
  m_state->setBoolean(PageAgentState::screencastEnabled, false);
  return Response::OK();
}

Response InspectorPageAgent::configureOverlay(Maybe<bool> suspended,
                                              Maybe<String> message) {
  m_state->setBoolean(PageAgentState::overlaySuspended,
                      suspended.fromMaybe(false));
  m_state->setString(PageAgentState::overlaySuspended,
                     message.fromMaybe(String()));
  if (m_client)
    m_client->configureOverlay(suspended.fromMaybe(false),
                               message.fromMaybe(String()));
  return Response::OK();
}

Response InspectorPageAgent::getLayoutMetrics(
    std::unique_ptr<protocol::Page::LayoutViewport>* outLayoutViewport,
    std::unique_ptr<protocol::Page::VisualViewport>* outVisualViewport) {
  LocalFrame* mainFrame = m_inspectedFrames->root();
  VisualViewport& visualViewport = mainFrame->host()->visualViewport();

  mainFrame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  IntRect visibleContents = mainFrame->view()->visibleContentRect();
  *outLayoutViewport = protocol::Page::LayoutViewport::create()
                           .setPageX(visibleContents.x())
                           .setPageY(visibleContents.y())
                           .setClientWidth(visibleContents.width())
                           .setClientHeight(visibleContents.height())
                           .build();

  FrameView* frameView = mainFrame->view();
  ScrollOffset pageOffset = frameView->getScrollableArea()->getScrollOffset();
  float pageZoom = mainFrame->pageZoomFactor();
  FloatRect visibleRect = visualViewport.visibleRect();
  float scale = visualViewport.scale();
  float scrollbarWidth = frameView->verticalScrollbarWidth() / scale;
  float scrollbarHeight = frameView->horizontalScrollbarHeight() / scale;

  *outVisualViewport =
      protocol::Page::VisualViewport::create()
          .setOffsetX(adjustScrollForAbsoluteZoom(visibleRect.x(), pageZoom))
          .setOffsetY(adjustScrollForAbsoluteZoom(visibleRect.y(), pageZoom))
          .setPageX(adjustScrollForAbsoluteZoom(pageOffset.width(), pageZoom))
          .setPageY(adjustScrollForAbsoluteZoom(pageOffset.height(), pageZoom))
          .setClientWidth(visibleRect.width() - scrollbarWidth)
          .setClientHeight(visibleRect.height() - scrollbarHeight)
          .setScale(scale)
          .build();
  return Response::OK();
}

DEFINE_TRACE(InspectorPageAgent) {
  visitor->trace(m_inspectedFrames);
  visitor->trace(m_inspectorResourceContentLoader);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
