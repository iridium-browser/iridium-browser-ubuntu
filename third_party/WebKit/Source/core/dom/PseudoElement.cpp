/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "core/dom/PseudoElement.h"

#include "core/dom/FirstLetterPseudoElement.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutQuote.h"
#include "core/style/ContentData.h"

namespace blink {

PseudoElement* PseudoElement::create(Element* parent, PseudoId pseudoId) {
  return new PseudoElement(parent, pseudoId);
}

const QualifiedName& pseudoElementTagName(PseudoId pseudoId) {
  switch (pseudoId) {
    case PseudoIdAfter: {
      DEFINE_STATIC_LOCAL(QualifiedName, after,
                          (nullAtom, "<pseudo:after>", nullAtom));
      return after;
    }
    case PseudoIdBefore: {
      DEFINE_STATIC_LOCAL(QualifiedName, before,
                          (nullAtom, "<pseudo:before>", nullAtom));
      return before;
    }
    case PseudoIdBackdrop: {
      DEFINE_STATIC_LOCAL(QualifiedName, backdrop,
                          (nullAtom, "<pseudo:backdrop>", nullAtom));
      return backdrop;
    }
    case PseudoIdFirstLetter: {
      DEFINE_STATIC_LOCAL(QualifiedName, firstLetter,
                          (nullAtom, "<pseudo:first-letter>", nullAtom));
      return firstLetter;
    }
    default:
      NOTREACHED();
  }
  DEFINE_STATIC_LOCAL(QualifiedName, name, (nullAtom, "<pseudo>", nullAtom));
  return name;
}

String PseudoElement::pseudoElementNameForEvents(PseudoId pseudoId) {
  DEFINE_STATIC_LOCAL(const String, after, ("::after"));
  DEFINE_STATIC_LOCAL(const String, before, ("::before"));
  switch (pseudoId) {
    case PseudoIdAfter:
      return after;
    case PseudoIdBefore:
      return before;
    default:
      return emptyString();
  }
}

PseudoElement::PseudoElement(Element* parent, PseudoId pseudoId)
    : Element(pseudoElementTagName(pseudoId),
              &parent->document(),
              CreateElement),
      m_pseudoId(pseudoId) {
  DCHECK_NE(pseudoId, PseudoIdNone);
  parent->treeScope().adoptIfNeeded(*this);
  setParentOrShadowHostNode(parent);
  setHasCustomStyleCallbacks();
  if ((pseudoId == PseudoIdBefore || pseudoId == PseudoIdAfter) &&
      parent->hasTagName(HTMLNames::inputTag))
    UseCounter::count(parent->document(),
                      UseCounter::PseudoBeforeAfterForInputElement);
}

PassRefPtr<ComputedStyle> PseudoElement::customStyleForLayoutObject() {
  return parentOrShadowHostElement()->layoutObject()->getCachedPseudoStyle(
      m_pseudoId);
}

void PseudoElement::dispose() {
  DCHECK(parentOrShadowHostElement());

  InspectorInstrumentation::pseudoElementDestroyed(this);

  DCHECK(!nextSibling());
  DCHECK(!previousSibling());

  detachLayoutTree();
  Element* parent = parentOrShadowHostElement();
  document().adoptIfNeeded(*this);
  setParentOrShadowHostNode(0);
  removedFrom(parent);
}

void PseudoElement::attachLayoutTree(const AttachContext& context) {
  DCHECK(!layoutObject());

  Element::attachLayoutTree(context);

  LayoutObject* layoutObject = this->layoutObject();
  if (!layoutObject)
    return;

  ComputedStyle& style = layoutObject->mutableStyleRef();
  if (style.styleType() != PseudoIdBefore && style.styleType() != PseudoIdAfter)
    return;
  DCHECK(style.contentData());

  for (const ContentData* content = style.contentData(); content;
       content = content->next()) {
    LayoutObject* child = content->createLayoutObject(document(), style);
    if (layoutObject->isChildAllowed(child, style)) {
      layoutObject->addChild(child);
      if (child->isQuote())
        toLayoutQuote(child)->attachQuote();
    } else {
      child->destroy();
    }
  }
}

bool PseudoElement::layoutObjectIsNeeded(const ComputedStyle& style) {
  return pseudoElementLayoutObjectIsNeeded(&style);
}

void PseudoElement::didRecalcStyle(StyleRecalcChange) {
  if (!layoutObject())
    return;

  // The layoutObjects inside pseudo elements are anonymous so they don't get
  // notified of recalcStyle and must have the style propagated downward
  // manually similar to LayoutObject::propagateStyleToAnonymousChildren.
  LayoutObject* layoutObject = this->layoutObject();
  for (LayoutObject* child = layoutObject->nextInPreOrder(layoutObject); child;
       child = child->nextInPreOrder(layoutObject)) {
    // We only manage the style for the generated content items.
    if (!child->isText() && !child->isQuote() && !child->isImage())
      continue;

    child->setPseudoStyle(layoutObject->mutableStyle());
  }
}

// With PseudoElements the DOM tree and Layout tree can differ. When you attach
// a, first-letter for example, into the DOM we walk down the Layout
// tree to find the correct insertion point for the LayoutObject. But, this
// means if we ask for the parentOrShadowHost Node from the first-letter
// pseudo element we will get some arbitrary ancestor of the LayoutObject.
//
// For hit testing, we need the parent Node of the LayoutObject for the
// first-letter pseudo element. So, by walking up the Layout tree we know
// we will get the parent and not some other ancestor.
Node* PseudoElement::findAssociatedNode() const {
  // The ::backdrop element is parented to the LayoutView, not to the node
  // that it's associated with. We need to make sure ::backdrop sends the
  // events to the parent node correctly.
  if (getPseudoId() == PseudoIdBackdrop)
    return parentOrShadowHostNode();

  DCHECK(layoutObject());
  DCHECK(layoutObject()->parent());

  // We can have any number of anonymous layout objects inserted between
  // us and our parent so make sure we skip over them.
  LayoutObject* ancestor = layoutObject()->parent();
  while (ancestor->isAnonymous() ||
         (ancestor->node() && ancestor->node()->isPseudoElement())) {
    DCHECK(ancestor->parent());
    ancestor = ancestor->parent();
  }
  return ancestor->node();
}

}  // namespace blink
