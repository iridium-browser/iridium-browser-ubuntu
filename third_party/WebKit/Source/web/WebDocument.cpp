/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/web/WebDocument.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ElementRegistrationOptions.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStatisticsCollector.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebDistillability.h"
#include "public/platform/WebURL.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebDOMEvent.h"
#include "public/web/WebElement.h"
#include "public/web/WebElementCollection.h"
#include "public/web/WebFormElement.h"
#include "v8/include/v8.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/PassRefPtr.h"

namespace blink {

WebURL WebDocument::url() const {
  return constUnwrap<Document>()->url();
}

WebSecurityOrigin WebDocument::getSecurityOrigin() const {
  if (!constUnwrap<Document>())
    return WebSecurityOrigin();
  return WebSecurityOrigin(constUnwrap<Document>()->getSecurityOrigin());
}

bool WebDocument::isSecureContext() const {
  const Document* document = constUnwrap<Document>();
  return document && document->isSecureContext();
}

WebString WebDocument::encoding() const {
  return constUnwrap<Document>()->encodingName();
}

WebString WebDocument::contentLanguage() const {
  return constUnwrap<Document>()->contentLanguage();
}

WebString WebDocument::referrer() const {
  return constUnwrap<Document>()->referrer();
}

WebColor WebDocument::themeColor() const {
  return constUnwrap<Document>()->themeColor().rgb();
}

WebURL WebDocument::openSearchDescriptionURL() const {
  return const_cast<Document*>(constUnwrap<Document>())
      ->openSearchDescriptionURL();
}

WebLocalFrame* WebDocument::frame() const {
  return WebLocalFrameImpl::fromFrame(constUnwrap<Document>()->frame());
}

bool WebDocument::isHTMLDocument() const {
  return constUnwrap<Document>()->isHTMLDocument();
}

bool WebDocument::isXHTMLDocument() const {
  return constUnwrap<Document>()->isXHTMLDocument();
}

bool WebDocument::isPluginDocument() const {
  return constUnwrap<Document>()->isPluginDocument();
}

WebURL WebDocument::baseURL() const {
  return constUnwrap<Document>()->baseURL();
}

WebURL WebDocument::firstPartyForCookies() const {
  return constUnwrap<Document>()->firstPartyForCookies();
}

WebElement WebDocument::documentElement() const {
  return WebElement(constUnwrap<Document>()->documentElement());
}

WebElement WebDocument::body() const {
  return WebElement(constUnwrap<Document>()->body());
}

WebElement WebDocument::head() {
  return WebElement(unwrap<Document>()->head());
}

WebString WebDocument::title() const {
  return WebString(constUnwrap<Document>()->title());
}

WebString WebDocument::contentAsTextForTesting() const {
  if (Element* documentElement = constUnwrap<Document>()->documentElement())
    return WebString(documentElement->innerText());
  return WebString();
}

WebElementCollection WebDocument::all() {
  return WebElementCollection(unwrap<Document>()->all());
}

void WebDocument::forms(WebVector<WebFormElement>& results) const {
  HTMLCollection* forms =
      const_cast<Document*>(constUnwrap<Document>())->forms();
  size_t sourceLength = forms->length();
  Vector<WebFormElement> temp;
  temp.reserveCapacity(sourceLength);
  for (size_t i = 0; i < sourceLength; ++i) {
    Element* element = forms->item(i);
    // Strange but true, sometimes node can be 0.
    if (element && element->isHTMLElement())
      temp.push_back(WebFormElement(toHTMLFormElement(element)));
  }
  results.assign(temp);
}

WebURL WebDocument::completeURL(const WebString& partialURL) const {
  return constUnwrap<Document>()->completeURL(partialURL);
}

WebElement WebDocument::getElementById(const WebString& id) const {
  return WebElement(constUnwrap<Document>()->getElementById(id));
}

WebElement WebDocument::focusedElement() const {
  return WebElement(constUnwrap<Document>()->focusedElement());
}

void WebDocument::insertStyleSheet(const WebString& sourceCode) {
  Document* document = unwrap<Document>();
  DCHECK(document);
  StyleSheetContents* parsedSheet =
      StyleSheetContents::create(CSSParserContext::create(*document));
  parsedSheet->parseString(sourceCode);
  document->styleEngine().injectAuthorSheet(parsedSheet);
}

void WebDocument::watchCSSSelectors(const WebVector<WebString>& webSelectors) {
  Document* document = unwrap<Document>();
  CSSSelectorWatch* watch = CSSSelectorWatch::fromIfExists(*document);
  if (!watch && webSelectors.isEmpty())
    return;
  Vector<String> selectors;
  selectors.append(webSelectors.data(), webSelectors.size());
  CSSSelectorWatch::from(*document).watchCSSSelectors(selectors);
}

WebReferrerPolicy WebDocument::getReferrerPolicy() const {
  return static_cast<WebReferrerPolicy>(
      constUnwrap<Document>()->getReferrerPolicy());
}

WebString WebDocument::outgoingReferrer() {
  return WebString(unwrap<Document>()->outgoingReferrer());
}

WebAXObject WebDocument::accessibilityObject() const {
  const Document* document = constUnwrap<Document>();
  AXObjectCacheImpl* cache = toAXObjectCacheImpl(document->axObjectCache());
  return cache ? WebAXObject(cache->getOrCreate(
                     toLayoutView(LayoutAPIShim::layoutObjectFrom(
                         document->layoutViewItem()))))
               : WebAXObject();
}

WebAXObject WebDocument::accessibilityObjectFromID(int axID) const {
  const Document* document = constUnwrap<Document>();
  AXObjectCacheImpl* cache = toAXObjectCacheImpl(document->axObjectCache());
  return cache ? WebAXObject(cache->objectFromAXID(axID)) : WebAXObject();
}

WebAXObject WebDocument::focusedAccessibilityObject() const {
  const Document* document = constUnwrap<Document>();
  AXObjectCacheImpl* cache = toAXObjectCacheImpl(document->axObjectCache());
  return cache ? WebAXObject(cache->focusedObject()) : WebAXObject();
}

WebVector<WebDraggableRegion> WebDocument::draggableRegions() const {
  WebVector<WebDraggableRegion> draggableRegions;
  const Document* document = constUnwrap<Document>();
  if (document->hasAnnotatedRegions()) {
    const Vector<AnnotatedRegionValue>& regions = document->annotatedRegions();
    draggableRegions = WebVector<WebDraggableRegion>(regions.size());
    for (size_t i = 0; i < regions.size(); i++) {
      const AnnotatedRegionValue& value = regions[i];
      draggableRegions[i].draggable = value.draggable;
      draggableRegions[i].bounds = IntRect(value.bounds);
    }
  }
  return draggableRegions;
}

v8::Local<v8::Value> WebDocument::registerEmbedderCustomElement(
    const WebString& name,
    v8::Local<v8::Value> options,
    WebExceptionCode& ec) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  Document* document = unwrap<Document>();
  DummyExceptionStateForTesting exceptionState;
  ElementRegistrationOptions registrationOptions;
  V8ElementRegistrationOptions::toImpl(isolate, options, registrationOptions,
                                       exceptionState);
  if (exceptionState.hadException())
    return v8::Local<v8::Value>();
  ScriptValue constructor = document->registerElement(
      ScriptState::current(isolate), name, registrationOptions, exceptionState,
      V0CustomElement::EmbedderNames);
  ec = exceptionState.code();
  if (exceptionState.hadException())
    return v8::Local<v8::Value>();
  return constructor.v8Value();
}

WebURL WebDocument::manifestURL() const {
  const Document* document = constUnwrap<Document>();
  HTMLLinkElement* linkElement = document->linkManifest();
  if (!linkElement)
    return WebURL();
  return linkElement->href();
}

bool WebDocument::manifestUseCredentials() const {
  const Document* document = constUnwrap<Document>();
  HTMLLinkElement* linkElement = document->linkManifest();
  if (!linkElement)
    return false;
  return equalIgnoringASCIICase(
      linkElement->fastGetAttribute(HTMLNames::crossoriginAttr),
      "use-credentials");
}

WebDistillabilityFeatures WebDocument::distillabilityFeatures() {
  return DocumentStatisticsCollector::collectStatistics(*unwrap<Document>());
}

WebDocument::WebDocument(Document* elem) : WebNode(elem) {}

DEFINE_WEB_NODE_TYPE_CASTS(WebDocument, constUnwrap<Node>()->isDocumentNode());

WebDocument& WebDocument::operator=(Document* elem) {
  m_private = elem;
  return *this;
}

WebDocument::operator Document*() const {
  return toDocument(m_private.get());
}

}  // namespace blink
