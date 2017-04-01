/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2008, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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
 *
 */

#include "core/html/HTMLElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/MathMLNames.h"
#include "core/XMLNames.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSMarkup.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/EventListener.h"
#include "core/events/KeyboardEvent.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLBRElement.h"
#include "core/html/HTMLDimension.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMenuElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutObject.h"
#include "core/page/SpatialNavigation.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/Language.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/BidiTextRun.h"
#include "platform/text/TextRunIterator.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"

namespace blink {

using namespace HTMLNames;
using namespace WTF;

using namespace std;

namespace {

// https://w3c.github.io/editing/execCommand.html#editing-host
bool isEditingHost(const Node& node) {
  if (!node.isHTMLElement())
    return false;
  String normalizedValue = toHTMLElement(node).contentEditable();
  if (normalizedValue == "true" || normalizedValue == "plaintext-only")
    return true;
  return node.document().inDesignMode() &&
         node.document().documentElement() == &node;
}

// https://w3c.github.io/editing/execCommand.html#editable
bool isEditable(const Node& node) {
  if (isEditingHost(node))
    return false;
  if (node.isHTMLElement() && toHTMLElement(node).contentEditable() == "false")
    return false;
  if (!node.parentNode())
    return false;
  if (!isEditingHost(*node.parentNode()) && !isEditable(*node.parentNode()))
    return false;
  if (node.isHTMLElement())
    return true;
  if (isSVGSVGElement(node))
    return true;
  if (node.isElementNode() && toElement(node).hasTagName(MathMLNames::mathTag))
    return true;
  return !node.isElementNode() && node.parentNode()->isHTMLElement();
}

}  // anonymous namespace

DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(HTMLElement);

String HTMLElement::debugNodeName() const {
  if (document().isHTMLDocument()) {
    return tagQName().hasPrefix() ? Element::nodeName().upper()
                                  : tagQName().localName().upper();
  }
  return Element::nodeName();
}

String HTMLElement::nodeName() const {
  // localNameUpper may intern and cache an AtomicString.
  DCHECK(isMainThread());

  // FIXME: Would be nice to have an atomicstring lookup based off uppercase
  // chars that does not have to copy the string on a hit in the hash.
  // FIXME: We should have a way to detect XHTML elements and replace the
  // hasPrefix() check with it.
  if (document().isHTMLDocument()) {
    if (!tagQName().hasPrefix())
      return tagQName().localNameUpper();
    return Element::nodeName().upper();
  }
  return Element::nodeName();
}

bool HTMLElement::ieForbidsInsertHTML() const {
  // FIXME: Supposedly IE disallows settting innerHTML, outerHTML
  // and createContextualFragment on these tags.  We have no tests to
  // verify this however, so this list could be totally wrong.
  // This list was moved from the previous endTagRequirement() implementation.
  // This is also called from editing and assumed to be the list of tags
  // for which no end tag should be serialized. It's unclear if the list for
  // IE compat and the list for serialization sanity are the same.
  if (hasTagName(areaTag) || hasTagName(baseTag) || hasTagName(basefontTag) ||
      hasTagName(brTag) || hasTagName(colTag) || hasTagName(embedTag) ||
      hasTagName(frameTag) || hasTagName(hrTag) || hasTagName(imageTag) ||
      hasTagName(imgTag) || hasTagName(inputTag) || hasTagName(keygenTag) ||
      hasTagName(linkTag) || (RuntimeEnabledFeatures::contextMenuEnabled() &&
                              hasTagName(menuitemTag)) ||
      hasTagName(metaTag) || hasTagName(paramTag) || hasTagName(sourceTag) ||
      hasTagName(trackTag) || hasTagName(wbrTag))
    return true;
  return false;
}

static inline CSSValueID unicodeBidiAttributeForDirAuto(HTMLElement* element) {
  if (element->hasTagName(preTag) || element->hasTagName(textareaTag))
    return CSSValueWebkitPlaintext;
  // FIXME: For bdo element, dir="auto" should result in "bidi-override isolate"
  // but we don't support having multiple values in unicode-bidi yet.
  // See https://bugs.webkit.org/show_bug.cgi?id=73164.
  return CSSValueWebkitIsolate;
}

unsigned HTMLElement::parseBorderWidthAttribute(
    const AtomicString& value) const {
  unsigned borderWidth = 0;
  if (value.isEmpty() || !parseHTMLNonNegativeInteger(value, borderWidth)) {
    if (hasTagName(tableTag) && !value.isNull())
      return 1;
  }
  return borderWidth;
}

void HTMLElement::applyBorderAttributeToStyle(const AtomicString& value,
                                              MutableStylePropertySet* style) {
  addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth,
                                          parseBorderWidthAttribute(value),
                                          CSSPrimitiveValue::UnitType::Pixels);
  addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderStyle,
                                          CSSValueSolid);
}

void HTMLElement::mapLanguageAttributeToLocale(const AtomicString& value,
                                               MutableStylePropertySet* style) {
  if (!value.isEmpty()) {
    // Have to quote so the locale id is treated as a string instead of as a CSS
    // keyword.
    addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLocale,
                                            serializeString(value));

    // FIXME: Remove the following UseCounter code when we collect enough
    // data.
    UseCounter::count(document(), UseCounter::LangAttribute);
    if (isHTMLHtmlElement(*this))
      UseCounter::count(document(), UseCounter::LangAttributeOnHTML);
    else if (isHTMLBodyElement(*this))
      UseCounter::count(document(), UseCounter::LangAttributeOnBody);
    String htmlLanguage = value.getString();
    size_t firstSeparator = htmlLanguage.find('-');
    if (firstSeparator != kNotFound)
      htmlLanguage = htmlLanguage.left(firstSeparator);
    String uiLanguage = defaultLanguage();
    firstSeparator = uiLanguage.find('-');
    if (firstSeparator != kNotFound)
      uiLanguage = uiLanguage.left(firstSeparator);
    firstSeparator = uiLanguage.find('_');
    if (firstSeparator != kNotFound)
      uiLanguage = uiLanguage.left(firstSeparator);
    if (!equalIgnoringCase(htmlLanguage, uiLanguage))
      UseCounter::count(document(),
                        UseCounter::LangAttributeDoesNotMatchToUILocale);
  } else {
    // The empty string means the language is explicitly unknown.
    addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLocale,
                                            CSSValueAuto);
  }
}

bool HTMLElement::isPresentationAttribute(const QualifiedName& name) const {
  if (name == alignAttr || name == contenteditableAttr || name == hiddenAttr ||
      name == langAttr || name.matches(XMLNames::langAttr) ||
      name == draggableAttr || name == dirAttr)
    return true;
  return Element::isPresentationAttribute(name);
}

static inline bool isValidDirAttribute(const AtomicString& value) {
  return equalIgnoringCase(value, "auto") || equalIgnoringCase(value, "ltr") ||
         equalIgnoringCase(value, "rtl");
}

void HTMLElement::collectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == alignAttr) {
    if (equalIgnoringCase(value, "middle"))
      addPropertyToPresentationAttributeStyle(style, CSSPropertyTextAlign,
                                              CSSValueCenter);
    else
      addPropertyToPresentationAttributeStyle(style, CSSPropertyTextAlign,
                                              value);
  } else if (name == contenteditableAttr) {
    if (value.isEmpty() || equalIgnoringCase(value, "true")) {
      addPropertyToPresentationAttributeStyle(
          style, CSSPropertyWebkitUserModify, CSSValueReadWrite);
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap,
                                              CSSValueBreakWord);
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLineBreak,
                                              CSSValueAfterWhiteSpace);
      UseCounter::count(document(), UseCounter::ContentEditableTrue);
      if (hasTagName(htmlTag))
        UseCounter::count(document(), UseCounter::ContentEditableTrueOnHTML);
    } else if (equalIgnoringCase(value, "plaintext-only")) {
      addPropertyToPresentationAttributeStyle(
          style, CSSPropertyWebkitUserModify, CSSValueReadWritePlaintextOnly);
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap,
                                              CSSValueBreakWord);
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLineBreak,
                                              CSSValueAfterWhiteSpace);
      UseCounter::count(document(), UseCounter::ContentEditablePlainTextOnly);
    } else if (equalIgnoringCase(value, "false")) {
      addPropertyToPresentationAttributeStyle(
          style, CSSPropertyWebkitUserModify, CSSValueReadOnly);
    }
  } else if (name == hiddenAttr) {
    addPropertyToPresentationAttributeStyle(style, CSSPropertyDisplay,
                                            CSSValueNone);
  } else if (name == draggableAttr) {
    UseCounter::count(document(), UseCounter::DraggableAttribute);
    if (equalIgnoringCase(value, "true")) {
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserDrag,
                                              CSSValueElement);
      addPropertyToPresentationAttributeStyle(style, CSSPropertyUserSelect,
                                              CSSValueNone);
    } else if (equalIgnoringCase(value, "false")) {
      addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserDrag,
                                              CSSValueNone);
    }
  } else if (name == dirAttr) {
    if (equalIgnoringCase(value, "auto")) {
      addPropertyToPresentationAttributeStyle(
          style, CSSPropertyUnicodeBidi, unicodeBidiAttributeForDirAuto(this));
    } else {
      if (isValidDirAttribute(value))
        addPropertyToPresentationAttributeStyle(style, CSSPropertyDirection,
                                                value);
      else if (isHTMLBodyElement(*this))
        addPropertyToPresentationAttributeStyle(style, CSSPropertyDirection,
                                                "ltr");
      if (!hasTagName(bdiTag) && !hasTagName(bdoTag) && !hasTagName(outputTag))
        addPropertyToPresentationAttributeStyle(style, CSSPropertyUnicodeBidi,
                                                CSSValueIsolate);
    }
  } else if (name.matches(XMLNames::langAttr)) {
    mapLanguageAttributeToLocale(value, style);
  } else if (name == langAttr) {
    // xml:lang has a higher priority than lang.
    if (!fastHasAttribute(XMLNames::langAttr))
      mapLanguageAttributeToLocale(value, style);
  } else {
    Element::collectStyleForPresentationAttribute(name, value, style);
  }
}

const AtomicString& HTMLElement::eventNameForAttributeName(
    const QualifiedName& attrName) {
  if (!attrName.namespaceURI().isNull())
    return nullAtom;

  if (!attrName.localName().startsWith("on", TextCaseASCIIInsensitive))
    return nullAtom;

  typedef HashMap<AtomicString, AtomicString> StringToStringMap;
  DEFINE_STATIC_LOCAL(StringToStringMap, attributeNameToEventNameMap, ());
  if (!attributeNameToEventNameMap.size()) {
    struct AttrToEventName {
      const QualifiedName& attr;
      const AtomicString& event;
    };
    AttrToEventName attrToEventNames[] = {
        {onabortAttr, EventTypeNames::abort},
        {onanimationendAttr, EventTypeNames::animationend},
        {onanimationiterationAttr, EventTypeNames::animationiteration},
        {onanimationstartAttr, EventTypeNames::animationstart},
        {onauxclickAttr, EventTypeNames::auxclick},
        {onbeforecopyAttr, EventTypeNames::beforecopy},
        {onbeforecutAttr, EventTypeNames::beforecut},
        {onbeforepasteAttr, EventTypeNames::beforepaste},
        {onblurAttr, EventTypeNames::blur},
        {oncancelAttr, EventTypeNames::cancel},
        {oncanplayAttr, EventTypeNames::canplay},
        {oncanplaythroughAttr, EventTypeNames::canplaythrough},
        {onchangeAttr, EventTypeNames::change},
        {onclickAttr, EventTypeNames::click},
        {oncloseAttr, EventTypeNames::close},
        {oncontextmenuAttr, EventTypeNames::contextmenu},
        {oncopyAttr, EventTypeNames::copy},
        {oncuechangeAttr, EventTypeNames::cuechange},
        {oncutAttr, EventTypeNames::cut},
        {ondblclickAttr, EventTypeNames::dblclick},
        {ondragAttr, EventTypeNames::drag},
        {ondragendAttr, EventTypeNames::dragend},
        {ondragenterAttr, EventTypeNames::dragenter},
        {ondragleaveAttr, EventTypeNames::dragleave},
        {ondragoverAttr, EventTypeNames::dragover},
        {ondragstartAttr, EventTypeNames::dragstart},
        {ondropAttr, EventTypeNames::drop},
        {ondurationchangeAttr, EventTypeNames::durationchange},
        {onemptiedAttr, EventTypeNames::emptied},
        {onendedAttr, EventTypeNames::ended},
        {onerrorAttr, EventTypeNames::error},
        {onfocusAttr, EventTypeNames::focus},
        {onfocusinAttr, EventTypeNames::focusin},
        {onfocusoutAttr, EventTypeNames::focusout},
        {ongotpointercaptureAttr, EventTypeNames::gotpointercapture},
        {oninputAttr, EventTypeNames::input},
        {oninvalidAttr, EventTypeNames::invalid},
        {onkeydownAttr, EventTypeNames::keydown},
        {onkeypressAttr, EventTypeNames::keypress},
        {onkeyupAttr, EventTypeNames::keyup},
        {onloadAttr, EventTypeNames::load},
        {onloadeddataAttr, EventTypeNames::loadeddata},
        {onloadedmetadataAttr, EventTypeNames::loadedmetadata},
        {onloadstartAttr, EventTypeNames::loadstart},
        {onlostpointercaptureAttr, EventTypeNames::lostpointercapture},
        {onmousedownAttr, EventTypeNames::mousedown},
        {onmouseenterAttr, EventTypeNames::mouseenter},
        {onmouseleaveAttr, EventTypeNames::mouseleave},
        {onmousemoveAttr, EventTypeNames::mousemove},
        {onmouseoutAttr, EventTypeNames::mouseout},
        {onmouseoverAttr, EventTypeNames::mouseover},
        {onmouseupAttr, EventTypeNames::mouseup},
        {onmousewheelAttr, EventTypeNames::mousewheel},
        {onpasteAttr, EventTypeNames::paste},
        {onpauseAttr, EventTypeNames::pause},
        {onplayAttr, EventTypeNames::play},
        {onplayingAttr, EventTypeNames::playing},
        {onpointercancelAttr, EventTypeNames::pointercancel},
        {onpointerdownAttr, EventTypeNames::pointerdown},
        {onpointerenterAttr, EventTypeNames::pointerenter},
        {onpointerleaveAttr, EventTypeNames::pointerleave},
        {onpointermoveAttr, EventTypeNames::pointermove},
        {onpointeroutAttr, EventTypeNames::pointerout},
        {onpointeroverAttr, EventTypeNames::pointerover},
        {onpointerupAttr, EventTypeNames::pointerup},
        {onprogressAttr, EventTypeNames::progress},
        {onratechangeAttr, EventTypeNames::ratechange},
        {onresetAttr, EventTypeNames::reset},
        {onresizeAttr, EventTypeNames::resize},
        {onscrollAttr, EventTypeNames::scroll},
        {onseekedAttr, EventTypeNames::seeked},
        {onseekingAttr, EventTypeNames::seeking},
        {onselectAttr, EventTypeNames::select},
        {onselectstartAttr, EventTypeNames::selectstart},
        {onshowAttr, EventTypeNames::show},
        {onstalledAttr, EventTypeNames::stalled},
        {onsubmitAttr, EventTypeNames::submit},
        {onsuspendAttr, EventTypeNames::suspend},
        {ontimeupdateAttr, EventTypeNames::timeupdate},
        {ontoggleAttr, EventTypeNames::toggle},
        {ontouchcancelAttr, EventTypeNames::touchcancel},
        {ontouchendAttr, EventTypeNames::touchend},
        {ontouchmoveAttr, EventTypeNames::touchmove},
        {ontouchstartAttr, EventTypeNames::touchstart},
        {ontransitionendAttr, EventTypeNames::webkitTransitionEnd},
        {onvolumechangeAttr, EventTypeNames::volumechange},
        {onwaitingAttr, EventTypeNames::waiting},
        {onwebkitanimationendAttr, EventTypeNames::webkitAnimationEnd},
        {onwebkitanimationiterationAttr,
         EventTypeNames::webkitAnimationIteration},
        {onwebkitanimationstartAttr, EventTypeNames::webkitAnimationStart},
        {onwebkitfullscreenchangeAttr, EventTypeNames::webkitfullscreenchange},
        {onwebkitfullscreenerrorAttr, EventTypeNames::webkitfullscreenerror},
        {onwebkittransitionendAttr, EventTypeNames::webkitTransitionEnd},
        {onwheelAttr, EventTypeNames::wheel},
    };

    for (const auto& name : attrToEventNames)
      attributeNameToEventNameMap.set(name.attr.localName(), name.event);
  }

  return attributeNameToEventNameMap.get(attrName.localName());
}

void HTMLElement::attributeChanged(const AttributeModificationParams& params) {
  Element::attributeChanged(params);
  if (params.reason != AttributeModificationReason::kDirectly)
    return;
  // adjustedFocusedElementInTreeScope() is not trivial. We should check
  // attribute names, then call adjustedFocusedElementInTreeScope().
  if (params.name == hiddenAttr && !params.newValue.isNull()) {
    if (adjustedFocusedElementInTreeScope() == this)
      blur();
  } else if (params.name == contenteditableAttr) {
    if (adjustedFocusedElementInTreeScope() != this)
      return;
    // The attribute change may cause supportsFocus() to return false
    // for the element which had focus.
    //
    // TODO(tkent): We should avoid updating style.  We'd like to check only
    // DOM-level focusability here.
    document().updateStyleAndLayoutTreeForNode(this);
    if (!supportsFocus())
      blur();
  }
}

void HTMLElement::parseAttribute(const AttributeModificationParams& params) {
  if (params.name == tabindexAttr || params.name == XMLNames::langAttr)
    return Element::parseAttribute(params);

  if (params.name == dirAttr) {
    dirAttributeChanged(params.newValue);
  } else if (params.name == langAttr) {
    pseudoStateChanged(CSSSelector::PseudoLang);
  } else {
    const AtomicString& eventName = eventNameForAttributeName(params.name);
    if (!eventName.isNull()) {
      setAttributeEventListener(
          eventName,
          createAttributeEventListener(this, params.name, params.newValue,
                                       eventParameterName()));
    }
  }
}

DocumentFragment* HTMLElement::textToFragment(const String& text,
                                              ExceptionState& exceptionState) {
  DocumentFragment* fragment = DocumentFragment::create(document());
  unsigned i, length = text.length();
  UChar c = 0;
  for (unsigned start = 0; start < length;) {
    // Find next line break.
    for (i = start; i < length; i++) {
      c = text[i];
      if (c == '\r' || c == '\n')
        break;
    }

    fragment->appendChild(
        Text::create(document(), text.substring(start, i - start)),
        exceptionState);
    if (exceptionState.hadException())
      return nullptr;

    if (c == '\r' || c == '\n') {
      fragment->appendChild(HTMLBRElement::create(document()), exceptionState);
      if (exceptionState.hadException())
        return nullptr;
      // Make sure \r\n doesn't result in two line breaks.
      if (c == '\r' && i + 1 < length && text[i + 1] == '\n')
        i++;
    }

    start = i + 1;  // Character after line break.
  }

  return fragment;
}

static inline bool shouldProhibitSetInnerOuterText(const HTMLElement& element) {
  return element.hasTagName(colTag) || element.hasTagName(colgroupTag) ||
         element.hasTagName(framesetTag) || element.hasTagName(headTag) ||
         element.hasTagName(htmlTag) || element.hasTagName(tableTag) ||
         element.hasTagName(tbodyTag) || element.hasTagName(tfootTag) ||
         element.hasTagName(theadTag) || element.hasTagName(trTag);
}

void HTMLElement::setInnerText(const String& text,
                               ExceptionState& exceptionState) {
  if (ieForbidsInsertHTML()) {
    exceptionState.throwDOMException(
        NoModificationAllowedError,
        "The '" + localName() + "' element does not support text insertion.");
    return;
  }
  if (shouldProhibitSetInnerOuterText(*this)) {
    exceptionState.throwDOMException(
        NoModificationAllowedError,
        "The '" + localName() + "' element does not support text insertion.");
    return;
  }

  // FIXME: This doesn't take whitespace collapsing into account at all.

  if (!text.contains('\n') && !text.contains('\r')) {
    if (text.isEmpty()) {
      removeChildren();
      return;
    }
    replaceChildrenWithText(this, text, exceptionState);
    return;
  }

  // FIXME: Do we need to be able to detect preserveNewline style even when
  // there's no layoutObject?
  // FIXME: Can the layoutObject be out of date here? Do we need to call
  // updateStyleIfNeeded?  For example, for the contents of textarea elements
  // that are display:none?
  LayoutObject* r = layoutObject();
  if (r && r->style()->preserveNewline()) {
    if (!text.contains('\r')) {
      replaceChildrenWithText(this, text, exceptionState);
      return;
    }
    String textWithConsistentLineBreaks = text;
    textWithConsistentLineBreaks.replace("\r\n", "\n");
    textWithConsistentLineBreaks.replace('\r', '\n');
    replaceChildrenWithText(this, textWithConsistentLineBreaks, exceptionState);
    return;
  }

  // Add text nodes and <br> elements.
  DocumentFragment* fragment = textToFragment(text, exceptionState);
  if (!exceptionState.hadException())
    replaceChildrenWithFragment(this, fragment, exceptionState);
}

void HTMLElement::setOuterText(const String& text,
                               ExceptionState& exceptionState) {
  if (ieForbidsInsertHTML()) {
    exceptionState.throwDOMException(
        NoModificationAllowedError,
        "The '" + localName() + "' element does not support text insertion.");
    return;
  }
  if (shouldProhibitSetInnerOuterText(*this)) {
    exceptionState.throwDOMException(
        NoModificationAllowedError,
        "The '" + localName() + "' element does not support text insertion.");
    return;
  }

  ContainerNode* parent = parentNode();
  if (!parent) {
    exceptionState.throwDOMException(NoModificationAllowedError,
                                     "The element has no parent.");
    return;
  }

  Node* prev = previousSibling();
  Node* next = nextSibling();
  Node* newChild = nullptr;

  // Convert text to fragment with <br> tags instead of linebreaks if needed.
  if (text.contains('\r') || text.contains('\n'))
    newChild = textToFragment(text, exceptionState);
  else
    newChild = Text::create(document(), text);

  // textToFragment might cause mutation events.
  if (!parentNode())
    exceptionState.throwDOMException(HierarchyRequestError,
                                     "The element has no parent.");

  if (exceptionState.hadException())
    return;

  parent->replaceChild(newChild, this, exceptionState);

  Node* node = next ? next->previousSibling() : nullptr;
  if (!exceptionState.hadException() && node && node->isTextNode())
    mergeWithNextTextNode(toText(node), exceptionState);

  if (!exceptionState.hadException() && prev && prev->isTextNode())
    mergeWithNextTextNode(toText(prev), exceptionState);
}

void HTMLElement::applyAlignmentAttributeToStyle(
    const AtomicString& alignment,
    MutableStylePropertySet* style) {
  // Vertical alignment with respect to the current baseline of the text
  // right or left means floating images.
  CSSValueID floatValue = CSSValueInvalid;
  CSSValueID verticalAlignValue = CSSValueInvalid;

  if (equalIgnoringCase(alignment, "absmiddle")) {
    verticalAlignValue = CSSValueMiddle;
  } else if (equalIgnoringCase(alignment, "absbottom")) {
    verticalAlignValue = CSSValueBottom;
  } else if (equalIgnoringCase(alignment, "left")) {
    floatValue = CSSValueLeft;
    verticalAlignValue = CSSValueTop;
  } else if (equalIgnoringCase(alignment, "right")) {
    floatValue = CSSValueRight;
    verticalAlignValue = CSSValueTop;
  } else if (equalIgnoringCase(alignment, "top")) {
    verticalAlignValue = CSSValueTop;
  } else if (equalIgnoringCase(alignment, "middle")) {
    verticalAlignValue = CSSValueWebkitBaselineMiddle;
  } else if (equalIgnoringCase(alignment, "center")) {
    verticalAlignValue = CSSValueMiddle;
  } else if (equalIgnoringCase(alignment, "bottom")) {
    verticalAlignValue = CSSValueBaseline;
  } else if (equalIgnoringCase(alignment, "texttop")) {
    verticalAlignValue = CSSValueTextTop;
  }

  if (floatValue != CSSValueInvalid)
    addPropertyToPresentationAttributeStyle(style, CSSPropertyFloat,
                                            floatValue);

  if (verticalAlignValue != CSSValueInvalid)
    addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign,
                                            verticalAlignValue);
}

bool HTMLElement::hasCustomFocusLogic() const {
  return false;
}

String HTMLElement::contentEditable() const {
  const AtomicString& value = fastGetAttribute(contenteditableAttr);

  if (value.isNull())
    return "inherit";
  if (value.isEmpty() || equalIgnoringCase(value, "true"))
    return "true";
  if (equalIgnoringCase(value, "false"))
    return "false";
  if (equalIgnoringCase(value, "plaintext-only"))
    return "plaintext-only";

  return "inherit";
}

void HTMLElement::setContentEditable(const String& enabled,
                                     ExceptionState& exceptionState) {
  if (equalIgnoringCase(enabled, "true"))
    setAttribute(contenteditableAttr, "true");
  else if (equalIgnoringCase(enabled, "false"))
    setAttribute(contenteditableAttr, "false");
  else if (equalIgnoringCase(enabled, "plaintext-only"))
    setAttribute(contenteditableAttr, "plaintext-only");
  else if (equalIgnoringCase(enabled, "inherit"))
    removeAttribute(contenteditableAttr);
  else
    exceptionState.throwDOMException(SyntaxError,
                                     "The value provided ('" + enabled +
                                         "') is not one of 'true', 'false', "
                                         "'plaintext-only', or 'inherit'.");
}

bool HTMLElement::isContentEditableForBinding() const {
  return isEditingHost(*this) || isEditable(*this);
}

bool HTMLElement::draggable() const {
  return equalIgnoringCase(getAttribute(draggableAttr), "true");
}

void HTMLElement::setDraggable(bool value) {
  setAttribute(draggableAttr, value ? "true" : "false");
}

bool HTMLElement::spellcheck() const {
  return isSpellCheckingEnabled();
}

void HTMLElement::setSpellcheck(bool enable) {
  setAttribute(spellcheckAttr, enable ? "true" : "false");
}

void HTMLElement::click() {
  dispatchSimulatedClick(0, SendNoEvents,
                         SimulatedClickCreationScope::FromScript);
}

void HTMLElement::accessKeyAction(bool sendMouseEvents) {
  dispatchSimulatedClick(
      0, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

String HTMLElement::title() const {
  return fastGetAttribute(titleAttr);
}

int HTMLElement::tabIndex() const {
  if (supportsFocus())
    return Element::tabIndex();
  return -1;
}

TranslateAttributeMode HTMLElement::translateAttributeMode() const {
  const AtomicString& value = getAttribute(translateAttr);

  if (value == nullAtom)
    return TranslateAttributeInherit;
  if (equalIgnoringCase(value, "yes") || equalIgnoringCase(value, ""))
    return TranslateAttributeYes;
  if (equalIgnoringCase(value, "no"))
    return TranslateAttributeNo;

  return TranslateAttributeInherit;
}

bool HTMLElement::translate() const {
  for (const HTMLElement* element = this; element;
       element = Traversal<HTMLElement>::firstAncestor(*element)) {
    TranslateAttributeMode mode = element->translateAttributeMode();
    if (mode != TranslateAttributeInherit) {
      DCHECK(mode == TranslateAttributeYes || mode == TranslateAttributeNo);
      return mode == TranslateAttributeYes;
    }
  }

  // Default on the root element is translate=yes.
  return true;
}

void HTMLElement::setTranslate(bool enable) {
  setAttribute(translateAttr, enable ? "yes" : "no");
}

// Returns the conforming 'dir' value associated with the state the attribute is
// in (in its canonical case), if any, or the empty string if the attribute is
// in a state that has no associated keyword value or if the attribute is not in
// a defined state (e.g. the attribute is missing and there is no missing value
// default).
// http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#limited-to-only-known-values
static inline const AtomicString& toValidDirValue(const AtomicString& value) {
  DEFINE_STATIC_LOCAL(const AtomicString, ltrValue, ("ltr"));
  DEFINE_STATIC_LOCAL(const AtomicString, rtlValue, ("rtl"));
  DEFINE_STATIC_LOCAL(const AtomicString, autoValue, ("auto"));

  if (equalIgnoringCase(value, ltrValue))
    return ltrValue;
  if (equalIgnoringCase(value, rtlValue))
    return rtlValue;
  if (equalIgnoringCase(value, autoValue))
    return autoValue;
  return nullAtom;
}

const AtomicString& HTMLElement::dir() {
  return toValidDirValue(fastGetAttribute(dirAttr));
}

void HTMLElement::setDir(const AtomicString& value) {
  setAttribute(dirAttr, value);
}

HTMLFormElement* HTMLElement::findFormAncestor() const {
  return Traversal<HTMLFormElement>::firstAncestor(*this);
}

static inline bool elementAffectsDirectionality(const Node* node) {
  return node->isHTMLElement() && (isHTMLBDIElement(toHTMLElement(*node)) ||
                                   toHTMLElement(*node).hasAttribute(dirAttr));
}

void HTMLElement::childrenChanged(const ChildrenChange& change) {
  Element::childrenChanged(change);
  adjustDirectionalityIfNeededAfterChildrenChanged(change);
}

bool HTMLElement::hasDirectionAuto() const {
  // <bdi> defaults to dir="auto"
  // https://html.spec.whatwg.org/multipage/semantics.html#the-bdi-element
  const AtomicString& direction = fastGetAttribute(dirAttr);
  return (isHTMLBDIElement(*this) && direction == nullAtom) ||
         equalIgnoringCase(direction, "auto");
}

TextDirection HTMLElement::directionalityIfhasDirAutoAttribute(
    bool& isAuto) const {
  isAuto = hasDirectionAuto();
  if (!isAuto)
    return TextDirection::kLtr;
  return directionality();
}

TextDirection HTMLElement::directionality(
    Node** strongDirectionalityTextNode) const {
  if (isHTMLInputElement(*this)) {
    HTMLInputElement* inputElement =
        toHTMLInputElement(const_cast<HTMLElement*>(this));
    bool hasStrongDirectionality;
    TextDirection textDirection = determineDirectionality(
        inputElement->value(), &hasStrongDirectionality);
    if (strongDirectionalityTextNode)
      *strongDirectionalityTextNode =
          hasStrongDirectionality ? inputElement : 0;
    return textDirection;
  }

  Node* node = FlatTreeTraversal::firstChild(*this);
  while (node) {
    // Skip bdi, script, style and text form controls.
    if (equalIgnoringCase(node->nodeName(), "bdi") ||
        isHTMLScriptElement(*node) || isHTMLStyleElement(*node) ||
        (node->isElementNode() && toElement(node)->isTextControl()) ||
        (node->isElementNode() &&
         toElement(node)->shadowPseudoId() == "-webkit-input-placeholder")) {
      node = FlatTreeTraversal::nextSkippingChildren(*node, this);
      continue;
    }

    // Skip elements with valid dir attribute
    if (node->isElementNode()) {
      AtomicString dirAttributeValue =
          toElement(node)->fastGetAttribute(dirAttr);
      if (isValidDirAttribute(dirAttributeValue)) {
        node = FlatTreeTraversal::nextSkippingChildren(*node, this);
        continue;
      }
    }

    if (node->isTextNode()) {
      bool hasStrongDirectionality;
      TextDirection textDirection = determineDirectionality(
          node->textContent(true), &hasStrongDirectionality);
      if (hasStrongDirectionality) {
        if (strongDirectionalityTextNode)
          *strongDirectionalityTextNode = node;
        return textDirection;
      }
    }
    node = FlatTreeTraversal::next(*node, this);
  }
  if (strongDirectionalityTextNode)
    *strongDirectionalityTextNode = 0;
  return TextDirection::kLtr;
}

bool HTMLElement::selfOrAncestorHasDirAutoAttribute() const {
  return layoutObject() && layoutObject()->style() &&
         layoutObject()->style()->selfOrAncestorHasDirAutoAttribute();
}

void HTMLElement::dirAttributeChanged(const AtomicString& value) {
  // If an ancestor has dir=auto, and this node has the first character,
  // changes to dir attribute may affect the ancestor.
  if (!canParticipateInFlatTree())
    return;
  updateDistribution();
  Element* parent = FlatTreeTraversal::parentElement(*this);
  if (parent && parent->isHTMLElement() &&
      toHTMLElement(parent)->selfOrAncestorHasDirAutoAttribute())
    toHTMLElement(parent)
        ->adjustDirectionalityIfNeededAfterChildAttributeChanged(this);

  if (equalIgnoringCase(value, "auto"))
    calculateAndAdjustDirectionality();
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildAttributeChanged(
    Element* child) {
  DCHECK(selfOrAncestorHasDirAutoAttribute());
  TextDirection textDirection = directionality();
  if (layoutObject() && layoutObject()->style() &&
      layoutObject()->style()->direction() != textDirection) {
    Element* elementToAdjust = this;
    for (; elementToAdjust;
         elementToAdjust = FlatTreeTraversal::parentElement(*elementToAdjust)) {
      if (elementAffectsDirectionality(elementToAdjust)) {
        elementToAdjust->setNeedsStyleRecalc(
            LocalStyleChange, StyleChangeReasonForTracing::create(
                                  StyleChangeReason::WritingModeChange));
        return;
      }
    }
  }
}

void HTMLElement::calculateAndAdjustDirectionality() {
  TextDirection textDirection = directionality();
  if (layoutObject() && layoutObject()->style() &&
      layoutObject()->style()->direction() != textDirection)
    setNeedsStyleRecalc(LocalStyleChange,
                        StyleChangeReasonForTracing::create(
                            StyleChangeReason::WritingModeChange));
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildrenChanged(
    const ChildrenChange& change) {
  if (!selfOrAncestorHasDirAutoAttribute())
    return;

  updateDistribution();

  for (Element* elementToAdjust = this; elementToAdjust;
       elementToAdjust = FlatTreeTraversal::parentElement(*elementToAdjust)) {
    if (elementAffectsDirectionality(elementToAdjust)) {
      toHTMLElement(elementToAdjust)->calculateAndAdjustDirectionality();
      return;
    }
  }
}

void HTMLElement::addHTMLLengthToStyle(MutableStylePropertySet* style,
                                       CSSPropertyID propertyID,
                                       const String& value,
                                       AllowPercentage allowPercentage) {
  HTMLDimension dimension;
  if (!parseDimensionValue(value, dimension))
    return;
  if (propertyID == CSSPropertyWidth &&
      (dimension.isPercentage() || dimension.isRelative())) {
    UseCounter::count(document(), UseCounter::HTMLElementDeprecatedWidth);
  }
  if (dimension.isRelative())
    return;
  if (dimension.isPercentage() && allowPercentage != AllowPercentageValues)
    return;
  CSSPrimitiveValue::UnitType unit =
      dimension.isPercentage() ? CSSPrimitiveValue::UnitType::Percentage
                               : CSSPrimitiveValue::UnitType::Pixels;
  addPropertyToPresentationAttributeStyle(style, propertyID, dimension.value(),
                                          unit);
}

static RGBA32 parseColorStringWithCrazyLegacyRules(const String& colorString) {
  // Per spec, only look at the first 128 digits of the string.
  const size_t maxColorLength = 128;
  // We'll pad the buffer with two extra 0s later, so reserve two more than the
  // max.
  Vector<char, maxColorLength + 2> digitBuffer;

  size_t i = 0;
  // Skip a leading #.
  if (colorString[0] == '#')
    i = 1;

  // Grab the first 128 characters, replacing non-hex characters with 0.
  // Non-BMP characters are replaced with "00" due to them appearing as two
  // "characters" in the String.
  for (; i < colorString.length() && digitBuffer.size() < maxColorLength; i++) {
    if (!isASCIIHexDigit(colorString[i]))
      digitBuffer.push_back('0');
    else
      digitBuffer.push_back(colorString[i]);
  }

  if (!digitBuffer.size())
    return Color::black;

  // Pad the buffer out to at least the next multiple of three in size.
  digitBuffer.push_back('0');
  digitBuffer.push_back('0');

  if (digitBuffer.size() < 6)
    return makeRGB(toASCIIHexValue(digitBuffer[0]),
                   toASCIIHexValue(digitBuffer[1]),
                   toASCIIHexValue(digitBuffer[2]));

  // Split the digits into three components, then search the last 8 digits of
  // each component.
  DCHECK_GE(digitBuffer.size(), 6u);
  size_t componentLength = digitBuffer.size() / 3;
  size_t componentSearchWindowLength = min<size_t>(componentLength, 8);
  size_t redIndex = componentLength - componentSearchWindowLength;
  size_t greenIndex = componentLength * 2 - componentSearchWindowLength;
  size_t blueIndex = componentLength * 3 - componentSearchWindowLength;
  // Skip digits until one of them is non-zero, or we've only got two digits
  // left in the component.
  while (digitBuffer[redIndex] == '0' && digitBuffer[greenIndex] == '0' &&
         digitBuffer[blueIndex] == '0' && (componentLength - redIndex) > 2) {
    redIndex++;
    greenIndex++;
    blueIndex++;
  }
  DCHECK_LT(redIndex + 1, componentLength);
  DCHECK_GE(greenIndex, componentLength);
  DCHECK_LT(greenIndex + 1, componentLength * 2);
  DCHECK_GE(blueIndex, componentLength * 2);
  SECURITY_DCHECK(blueIndex + 1 < digitBuffer.size());

  int redValue =
      toASCIIHexValue(digitBuffer[redIndex], digitBuffer[redIndex + 1]);
  int greenValue =
      toASCIIHexValue(digitBuffer[greenIndex], digitBuffer[greenIndex + 1]);
  int blueValue =
      toASCIIHexValue(digitBuffer[blueIndex], digitBuffer[blueIndex + 1]);
  return makeRGB(redValue, greenValue, blueValue);
}

// Color parsing that matches HTML's "rules for parsing a legacy color value"
bool HTMLElement::parseColorWithLegacyRules(const String& attributeValue,
                                            Color& parsedColor) {
  // An empty string doesn't apply a color. (One containing only whitespace
  // does, which is why this check occurs before stripping.)
  if (attributeValue.isEmpty())
    return false;

  String colorString = attributeValue.stripWhiteSpace();

  // "transparent" doesn't apply a color either.
  if (equalIgnoringCase(colorString, "transparent"))
    return false;

  // If the string is a 3/6-digit hex color or a named CSS color, use that.
  // Apply legacy rules otherwise. Note color.setFromString() accepts 4/8-digit
  // hex color, so restrict its use with length checks here to support legacy
  // HTML attributes.

  bool success = false;
  if ((colorString.length() == 4 || colorString.length() == 7) &&
      colorString[0] == '#')
    success = parsedColor.setFromString(colorString);
  if (!success)
    success = parsedColor.setNamedColor(colorString);
  if (!success) {
    parsedColor.setRGB(parseColorStringWithCrazyLegacyRules(colorString));
    success = true;
  }

  return success;
}

void HTMLElement::addHTMLColorToStyle(MutableStylePropertySet* style,
                                      CSSPropertyID propertyID,
                                      const String& attributeValue) {
  Color parsedColor;
  if (!parseColorWithLegacyRules(attributeValue, parsedColor))
    return;

  style->setProperty(propertyID, *CSSColorValue::create(parsedColor.rgb()));
}

bool HTMLElement::isInteractiveContent() const {
  return false;
}

HTMLMenuElement* HTMLElement::assignedContextMenu() const {
  if (HTMLMenuElement* menu = contextMenu())
    return menu;

  return parentElement() && parentElement()->isHTMLElement()
             ? toHTMLElement(parentElement())->assignedContextMenu()
             : nullptr;
}

HTMLMenuElement* HTMLElement::contextMenu() const {
  const AtomicString& contextMenuId(fastGetAttribute(contextmenuAttr));
  if (contextMenuId.isNull())
    return nullptr;

  Element* element = treeScope().getElementById(contextMenuId);
  // Not checking if the menu element is of type "popup".
  // Ignoring menu element type attribute is intentional according to the
  // standard.
  return isHTMLMenuElement(element) ? toHTMLMenuElement(element) : nullptr;
}

void HTMLElement::setContextMenu(HTMLMenuElement* contextMenu) {
  if (!contextMenu) {
    setAttribute(contextmenuAttr, "");
    return;
  }

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/infrastructure.html#reflecting-content-attributes-in-idl-attributes
  // On setting, if the given element has an id attribute, and has the same home
  // subtree as the element of the attribute being set, and the given element is
  // the first element in that home subtree whose ID is the value of that id
  // attribute, then the content attribute must be set to the value of that id
  // attribute.  Otherwise, the content attribute must be set to the empty
  // string.
  const AtomicString& contextMenuId(contextMenu->fastGetAttribute(idAttr));

  if (!contextMenuId.isNull() &&
      contextMenu == treeScope().getElementById(contextMenuId))
    setAttribute(contextmenuAttr, contextMenuId);
  else
    setAttribute(contextmenuAttr, "");
}

void HTMLElement::defaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::keypress && event->isKeyboardEvent()) {
    handleKeypressEvent(toKeyboardEvent(event));
    if (event->defaultHandled())
      return;
  }

  Element::defaultEventHandler(event);
}

bool HTMLElement::matchesReadOnlyPseudoClass() const {
  return !matchesReadWritePseudoClass();
}

bool HTMLElement::matchesReadWritePseudoClass() const {
  if (fastHasAttribute(contenteditableAttr)) {
    const AtomicString& value = fastGetAttribute(contenteditableAttr);

    if (value.isEmpty() || equalIgnoringCase(value, "true") ||
        equalIgnoringCase(value, "plaintext-only"))
      return true;
    if (equalIgnoringCase(value, "false"))
      return false;
    // All other values should be treated as "inherit".
  }

  return parentElement() && hasEditableStyle(*parentElement());
}

void HTMLElement::handleKeypressEvent(KeyboardEvent* event) {
  if (!isSpatialNavigationEnabled(document().frame()) || !supportsFocus())
    return;
  document().updateStyleAndLayoutTree();
  // if the element is a text form control (like <input type=text> or
  // <textarea>) or has contentEditable attribute on, we should enter a space or
  // newline even in spatial navigation mode instead of handling it as a "click"
  // action.
  if (isTextControl() || hasEditableStyle(*this))
    return;
  int charCode = event->charCode();
  if (charCode == '\r' || charCode == ' ') {
    dispatchSimulatedClick(event);
    event->setDefaultHandled();
  }
}

const AtomicString& HTMLElement::eventParameterName() {
  DEFINE_STATIC_LOCAL(const AtomicString, eventString, ("event"));
  return eventString;
}

int HTMLElement::offsetLeftForBinding() {
  Element* offsetParent = unclosedOffsetParent();
  if (LayoutBoxModelObject* layoutObject = layoutBoxModelObject())
    return adjustLayoutUnitForAbsoluteZoom(
               LayoutUnit(layoutObject->pixelSnappedOffsetLeft(offsetParent)),
               layoutObject->styleRef())
        .round();
  return 0;
}

int HTMLElement::offsetTopForBinding() {
  Element* offsetParent = unclosedOffsetParent();
  if (LayoutBoxModelObject* layoutObject = layoutBoxModelObject())
    return adjustLayoutUnitForAbsoluteZoom(
               LayoutUnit(layoutObject->pixelSnappedOffsetTop(offsetParent)),
               layoutObject->styleRef())
        .round();
  return 0;
}

int HTMLElement::offsetWidthForBinding() {
  Element* offsetParent = unclosedOffsetParent();
  if (LayoutBoxModelObject* layoutObject = layoutBoxModelObject())
    return adjustLayoutUnitForAbsoluteZoom(
               LayoutUnit(layoutObject->pixelSnappedOffsetWidth(offsetParent)),
               layoutObject->styleRef())
        .round();
  return 0;
}

DISABLE_CFI_PERF
int HTMLElement::offsetHeightForBinding() {
  Element* offsetParent = unclosedOffsetParent();
  if (LayoutBoxModelObject* layoutObject = layoutBoxModelObject())
    return adjustLayoutUnitForAbsoluteZoom(
               LayoutUnit(layoutObject->pixelSnappedOffsetHeight(offsetParent)),
               layoutObject->styleRef())
        .round();
  return 0;
}

Element* HTMLElement::unclosedOffsetParent() {
  document().updateStyleAndLayoutIgnorePendingStylesheetsForNode(this);

  LayoutObject* layoutObject = this->layoutObject();
  if (!layoutObject)
    return nullptr;

  return layoutObject->offsetParent(this);
}

}  // namespace blink

#ifndef NDEBUG

// For use in the debugger
void dumpInnerHTML(blink::HTMLElement*);

void dumpInnerHTML(blink::HTMLElement* element) {
  printf("%s\n", element->innerHTML().ascii().data());
}
#endif
