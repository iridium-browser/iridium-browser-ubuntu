/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/dom/ProcessingInstruction.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaList.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/IncrementLoadEventDelayCount.h"
#include "core/dom/StyleEngine.h"
#include "core/loader/resource/CSSStyleSheetResource.h"
#include "core/loader/resource/XSLStyleSheetResource.h"
#include "core/xml/DocumentXSLT.h"
#include "core/xml/XSLStyleSheet.h"
#include "core/xml/parser/XMLDocumentParser.h"  // for parseAttributes()
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include <memory>

namespace blink {

inline ProcessingInstruction::ProcessingInstruction(Document& document,
                                                    const String& target,
                                                    const String& data)
    : CharacterData(document, data, CreateOther),
      m_target(target),
      m_loading(false),
      m_alternate(false),
      m_isCSS(false),
      m_isXSL(false),
      m_listenerForXSLT(nullptr) {}

ProcessingInstruction* ProcessingInstruction::create(Document& document,
                                                     const String& target,
                                                     const String& data) {
  return new ProcessingInstruction(document, target, data);
}

ProcessingInstruction::~ProcessingInstruction() {}

EventListener* ProcessingInstruction::eventListenerForXSLT() {
  if (!m_listenerForXSLT)
    return 0;

  return m_listenerForXSLT->toEventListener();
}

void ProcessingInstruction::clearEventListenerForXSLT() {
  if (m_listenerForXSLT) {
    m_listenerForXSLT->detach();
    m_listenerForXSLT.clear();
  }
}

String ProcessingInstruction::nodeName() const {
  return m_target;
}

Node::NodeType ProcessingInstruction::getNodeType() const {
  return kProcessingInstructionNode;
}

Node* ProcessingInstruction::cloneNode(bool /*deep*/, ExceptionState&) {
  // FIXME: Is it a problem that this does not copy m_localHref?
  // What about other data members?
  return create(document(), m_target, m_data);
}

void ProcessingInstruction::didAttributeChanged() {
  if (m_sheet)
    clearSheet();

  String href;
  String charset;
  if (!checkStyleSheet(href, charset))
    return;
  process(href, charset);
}

bool ProcessingInstruction::checkStyleSheet(String& href, String& charset) {
  if (m_target != "xml-stylesheet" || !document().frame() ||
      parentNode() != document())
    return false;

  // see http://www.w3.org/TR/xml-stylesheet/
  // ### support stylesheet included in a fragment of this (or another) document
  // ### make sure this gets called when adding from javascript
  bool attrsOk;
  const HashMap<String, String> attrs = parseAttributes(m_data, attrsOk);
  if (!attrsOk)
    return false;
  HashMap<String, String>::const_iterator i = attrs.find("type");
  String type;
  if (i != attrs.end())
    type = i->value;

  m_isCSS = type.isEmpty() || type == "text/css";
  m_isXSL = (type == "text/xml" || type == "text/xsl" ||
             type == "application/xml" || type == "application/xhtml+xml" ||
             type == "application/rss+xml" || type == "application/atom+xml");
  if (!m_isCSS && !m_isXSL)
    return false;

  href = attrs.at("href");
  charset = attrs.at("charset");
  String alternate = attrs.at("alternate");
  m_alternate = alternate == "yes";
  m_title = attrs.at("title");
  m_media = attrs.at("media");

  return !m_alternate || !m_title.isEmpty();
}

void ProcessingInstruction::process(const String& href, const String& charset) {
  if (href.length() > 1 && href[0] == '#') {
    m_localHref = href.substring(1);
    // We need to make a synthetic XSLStyleSheet that is embedded.
    // It needs to be able to kick off import/include loads that
    // can hang off some parent sheet.
    if (m_isXSL && RuntimeEnabledFeatures::xsltEnabled()) {
      KURL finalURL(ParsedURLString, m_localHref);
      m_sheet = XSLStyleSheet::createEmbedded(this, finalURL);
      m_loading = false;
    }
    return;
  }

  clearResource();

  String url = document().completeURL(href).getString();

  StyleSheetResource* resource = nullptr;
  FetchRequest request(ResourceRequest(document().completeURL(href)),
                       FetchInitiatorTypeNames::processinginstruction);
  if (m_isXSL) {
    if (RuntimeEnabledFeatures::xsltEnabled())
      resource = XSLStyleSheetResource::fetch(request, document().fetcher());
  } else {
    request.setCharset(charset.isEmpty() ? document().characterSet() : charset);
    resource = CSSStyleSheetResource::fetch(request, document().fetcher());
  }

  if (resource) {
    m_loading = true;
    if (!m_isXSL)
      document().styleEngine().addPendingSheet(m_styleEngineContext);
    setResource(resource);
  }
}

bool ProcessingInstruction::isLoading() const {
  if (m_loading)
    return true;
  if (!m_sheet)
    return false;
  return m_sheet->isLoading();
}

bool ProcessingInstruction::sheetLoaded() {
  if (!isLoading()) {
    if (!DocumentXSLT::sheetLoaded(document(), this))
      document().styleEngine().removePendingSheet(*this, m_styleEngineContext);
    return true;
  }
  return false;
}

void ProcessingInstruction::setCSSStyleSheet(
    const String& href,
    const KURL& baseURL,
    const String& charset,
    const CSSStyleSheetResource* sheet) {
  if (!isConnected()) {
    DCHECK(!m_sheet);
    return;
  }

  DCHECK(m_isCSS);
  CSSParserContext* parserContext =
      CSSParserContext::create(document(), baseURL, charset);

  StyleSheetContents* newSheet =
      StyleSheetContents::create(href, parserContext);

  CSSStyleSheet* cssSheet = CSSStyleSheet::create(newSheet, *this);
  cssSheet->setDisabled(m_alternate);
  cssSheet->setTitle(m_title);
  if (!m_alternate && !m_title.isEmpty())
    document().styleEngine().setPreferredStylesheetSetNameIfNotSet(m_title);
  cssSheet->setMediaQueries(MediaQuerySet::create(m_media));

  m_sheet = cssSheet;

  // We don't need the cross-origin security check here because we are
  // getting the sheet text in "strict" mode. This enforces a valid CSS MIME
  // type.
  parseStyleSheet(sheet->sheetText());
}

void ProcessingInstruction::setXSLStyleSheet(const String& href,
                                             const KURL& baseURL,
                                             const String& sheet) {
  if (!isConnected()) {
    DCHECK(!m_sheet);
    return;
  }

  DCHECK(m_isXSL);
  m_sheet = XSLStyleSheet::create(this, href, baseURL);
  std::unique_ptr<IncrementLoadEventDelayCount> delay =
      IncrementLoadEventDelayCount::create(document());
  parseStyleSheet(sheet);
}

void ProcessingInstruction::parseStyleSheet(const String& sheet) {
  if (m_isCSS)
    toCSSStyleSheet(m_sheet.get())->contents()->parseString(sheet);
  else if (m_isXSL)
    toXSLStyleSheet(m_sheet.get())->parseString(sheet);

  clearResource();
  m_loading = false;

  if (m_isCSS)
    toCSSStyleSheet(m_sheet.get())->contents()->checkLoaded();
  else if (m_isXSL)
    toXSLStyleSheet(m_sheet.get())->checkLoaded();
}

Node::InsertionNotificationRequest ProcessingInstruction::insertedInto(
    ContainerNode* insertionPoint) {
  CharacterData::insertedInto(insertionPoint);
  if (!insertionPoint->isConnected())
    return InsertionDone;

  String href;
  String charset;
  bool isValid = checkStyleSheet(href, charset);
  if (!DocumentXSLT::processingInstructionInsertedIntoDocument(document(),
                                                               this))
    document().styleEngine().addStyleSheetCandidateNode(*this);
  if (isValid)
    process(href, charset);
  return InsertionDone;
}

void ProcessingInstruction::removedFrom(ContainerNode* insertionPoint) {
  CharacterData::removedFrom(insertionPoint);
  if (!insertionPoint->isConnected())
    return;

  // No need to remove XSLStyleSheet from StyleEngine.
  if (!DocumentXSLT::processingInstructionRemovedFromDocument(document(),
                                                              this)) {
    document().styleEngine().removeStyleSheetCandidateNode(*this,
                                                           *insertionPoint);
  }

  if (m_sheet) {
    DCHECK_EQ(m_sheet->ownerNode(), this);
    clearSheet();
  }

  // No need to remove pending sheets.
  clearResource();
}

void ProcessingInstruction::clearSheet() {
  DCHECK(m_sheet);
  if (m_sheet->isLoading())
    document().styleEngine().removePendingSheet(*this, m_styleEngineContext);
  m_sheet.release()->clearOwnerNode();
}

DEFINE_TRACE(ProcessingInstruction) {
  visitor->trace(m_sheet);
  visitor->trace(m_listenerForXSLT);
  CharacterData::trace(visitor);
  ResourceOwner<StyleSheetResource>::trace(visitor);
}

}  // namespace blink
