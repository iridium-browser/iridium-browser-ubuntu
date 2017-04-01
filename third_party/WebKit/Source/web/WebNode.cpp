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

#include "public/web/WebNode.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeList.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/TagCollection.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/Event.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutPart.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/Widget.h"
#include "public/platform/WebString.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebDOMEvent.h"
#include "public/web/WebDocument.h"
#include "public/web/WebElement.h"
#include "public/web/WebElementCollection.h"
#include "public/web/WebPluginContainer.h"
#include "web/FrameLoaderClientImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "wtf/PtrUtil.h"

namespace blink {

void WebNode::reset() {
  m_private.reset();
}

void WebNode::assign(const WebNode& other) {
  m_private = other.m_private;
}

bool WebNode::equals(const WebNode& n) const {
  return m_private.get() == n.m_private.get();
}

bool WebNode::lessThan(const WebNode& n) const {
  return m_private.get() < n.m_private.get();
}

WebNode WebNode::parentNode() const {
  return WebNode(const_cast<ContainerNode*>(m_private->parentNode()));
}

WebString WebNode::nodeValue() const {
  return m_private->nodeValue();
}

WebDocument WebNode::document() const {
  return WebDocument(&m_private->document());
}

WebNode WebNode::firstChild() const {
  return WebNode(m_private->firstChild());
}

WebNode WebNode::lastChild() const {
  return WebNode(m_private->lastChild());
}

WebNode WebNode::previousSibling() const {
  return WebNode(m_private->previousSibling());
}

WebNode WebNode::nextSibling() const {
  return WebNode(m_private->nextSibling());
}

bool WebNode::isLink() const {
  return m_private->isLink();
}

bool WebNode::isTextNode() const {
  return m_private->isTextNode();
}

bool WebNode::isCommentNode() const {
  return m_private->getNodeType() == Node::kCommentNode;
}

bool WebNode::isFocusable() const {
  if (!m_private->isElementNode())
    return false;
  m_private->document().updateStyleAndLayoutIgnorePendingStylesheets();
  return toElement(m_private.get())->isFocusable();
}

bool WebNode::isContentEditable() const {
  m_private->document().updateStyleAndLayoutTree();
  return hasEditableStyle(*m_private);
}

bool WebNode::isInsideFocusableElementOrARIAWidget() const {
  return AXObject::isInsideFocusableElementOrARIAWidget(
      *this->constUnwrap<Node>());
}

bool WebNode::isElementNode() const {
  return m_private->isElementNode();
}

bool WebNode::isDocumentNode() const {
  return m_private->isDocumentNode();
}

bool WebNode::isDocumentTypeNode() const {
  return m_private->getNodeType() == Node::kDocumentTypeNode;
}

void WebNode::simulateClick() {
  TaskRunnerHelper::get(TaskType::UserInteraction,
                        m_private->getExecutionContext())
      ->postTask(
          FROM_HERE,
          WTF::bind(&Node::dispatchSimulatedClick,
                    wrapWeakPersistent(m_private.get()), nullptr, SendNoEvents,
                    SimulatedClickCreationScope::FromUserAgent));
}

WebElementCollection WebNode::getElementsByHTMLTagName(
    const WebString& tag) const {
  if (m_private->isContainerNode())
    return WebElementCollection(
        toContainerNode(m_private.get())
            ->getElementsByTagNameNS(HTMLNames::xhtmlNamespaceURI, tag));
  return WebElementCollection();
}

WebElement WebNode::querySelector(const WebString& selector) const {
  if (!m_private->isContainerNode())
    return WebElement();
  return toContainerNode(m_private.get())
      ->querySelector(selector, IGNORE_EXCEPTION_FOR_TESTING);
}

bool WebNode::focused() const {
  return m_private->isFocused();
}

WebPluginContainer* WebNode::pluginContainerFromNode(const Node* node) {
  if (!node)
    return nullptr;

  if (!isHTMLObjectElement(node) && !isHTMLEmbedElement(node))
    return nullptr;

  LayoutObject* object = node->layoutObject();
  if (object && object->isLayoutPart()) {
    Widget* widget = toLayoutPart(object)->widget();
    if (widget && widget->isPluginContainer())
      return toWebPluginContainerImpl(widget);
  }

  return nullptr;
}

WebPluginContainer* WebNode::pluginContainer() const {
  return pluginContainerFromNode(constUnwrap<Node>());
}

WebAXObject WebNode::accessibilityObject() {
  WebDocument webDocument = document();
  const Document* doc = document().constUnwrap<Document>();
  AXObjectCacheImpl* cache = toAXObjectCacheImpl(doc->existingAXObjectCache());
  Node* node = unwrap<Node>();
  return cache ? WebAXObject(cache->get(node)) : WebAXObject();
}

WebNode::WebNode(Node* node) : m_private(node) {}

WebNode& WebNode::operator=(Node* node) {
  m_private = node;
  return *this;
}

WebNode::operator Node*() const {
  return m_private.get();
}

}  // namespace blink
