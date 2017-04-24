/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#include "core/dom/DocumentFragment.h"

#include "core/dom/Document.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "core/xml/parser/XMLDocumentParser.h"

namespace blink {

DocumentFragment::DocumentFragment(Document* document,
                                   ConstructionType constructionType)
    : ContainerNode(document, constructionType) {}

DocumentFragment* DocumentFragment::create(Document& document) {
  return new DocumentFragment(&document, Node::CreateDocumentFragment);
}

String DocumentFragment::nodeName() const {
  return "#document-fragment";
}

Node::NodeType DocumentFragment::getNodeType() const {
  return kDocumentFragmentNode;
}

bool DocumentFragment::childTypeAllowed(NodeType type) const {
  switch (type) {
    case kElementNode:
    case kProcessingInstructionNode:
    case kCommentNode:
    case kTextNode:
    case kCdataSectionNode:
      return true;
    default:
      return false;
  }
}

Node* DocumentFragment::cloneNode(bool deep, ExceptionState&) {
  DocumentFragment* clone = create(document());
  if (deep)
    cloneChildNodes(clone);
  return clone;
}

void DocumentFragment::parseHTML(const String& source,
                                 Element* contextElement,
                                 ParserContentPolicy parserContentPolicy) {
  HTMLDocumentParser::parseDocumentFragment(source, this, contextElement,
                                            parserContentPolicy);
}

bool DocumentFragment::parseXML(const String& source,
                                Element* contextElement,
                                ParserContentPolicy parserContentPolicy) {
  return XMLDocumentParser::parseDocumentFragment(source, this, contextElement,
                                                  parserContentPolicy);
}

}  // namespace blink
