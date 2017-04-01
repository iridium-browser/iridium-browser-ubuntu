/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "core/html/HTMLProgressElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/UseCounter.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ProgressShadowElement.h"
#include "core/layout/LayoutProgress.h"
#include "core/layout/api/LayoutProgressItem.h"

namespace blink {

using namespace HTMLNames;

const double HTMLProgressElement::IndeterminatePosition = -1;
const double HTMLProgressElement::InvalidPosition = -2;

HTMLProgressElement::HTMLProgressElement(Document& document)
    : LabelableElement(progressTag, document), m_value(nullptr) {
  UseCounter::count(document, UseCounter::ProgressElement);
}

HTMLProgressElement::~HTMLProgressElement() {}

HTMLProgressElement* HTMLProgressElement::create(Document& document) {
  HTMLProgressElement* progress = new HTMLProgressElement(document);
  progress->ensureUserAgentShadowRoot();
  return progress;
}

LayoutObject* HTMLProgressElement::createLayoutObject(
    const ComputedStyle& style) {
  if (!style.hasAppearance()) {
    UseCounter::count(document(),
                      UseCounter::ProgressElementWithNoneAppearance);
    return LayoutObject::createObject(this, style);
  }
  UseCounter::count(document(),
                    UseCounter::ProgressElementWithProgressBarAppearance);
  return new LayoutProgress(this);
}

LayoutProgress* HTMLProgressElement::layoutProgress() const {
  if (layoutObject() && layoutObject()->isProgress())
    return toLayoutProgress(layoutObject());
  return nullptr;
}

void HTMLProgressElement::parseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == valueAttr) {
    if (params.oldValue.isNull() != params.newValue.isNull())
      pseudoStateChanged(CSSSelector::PseudoIndeterminate);
    didElementStateChange();
  } else if (params.name == maxAttr) {
    didElementStateChange();
  } else {
    LabelableElement::parseAttribute(params);
  }
}

void HTMLProgressElement::attachLayoutTree(const AttachContext& context) {
  LabelableElement::attachLayoutTree(context);
  if (LayoutProgressItem layoutItem = LayoutProgressItem(layoutProgress()))
    layoutItem.updateFromElement();
}

double HTMLProgressElement::value() const {
  double value = getFloatingPointAttribute(valueAttr);
  // Otherwise, if the parsed value was greater than or equal to the maximum
  // value, then the current value of the progress bar is the maximum value
  // of the progress bar. Otherwise, if parsing the value attribute's value
  // resulted in an error, or a number less than or equal to zero, then the
  // current value of the progress bar is zero.
  return !std::isfinite(value) || value < 0 ? 0 : std::min(value, max());
}

void HTMLProgressElement::setValue(double value) {
  setFloatingPointAttribute(valueAttr, std::max(value, 0.));
}

double HTMLProgressElement::max() const {
  double max = getFloatingPointAttribute(maxAttr);
  // Otherwise, if the element has no max attribute, or if it has one but
  // parsing it resulted in an error, or if the parsed value was less than or
  // equal to zero, then the maximum value of the progress bar is 1.0.
  return !std::isfinite(max) || max <= 0 ? 1 : max;
}

void HTMLProgressElement::setMax(double max) {
  // FIXME: The specification says we should ignore the input value if it is
  // inferior or equal to 0.
  setFloatingPointAttribute(maxAttr, max > 0 ? max : 1);
}

double HTMLProgressElement::position() const {
  if (!isDeterminate())
    return HTMLProgressElement::IndeterminatePosition;
  return value() / max();
}

bool HTMLProgressElement::isDeterminate() const {
  return fastHasAttribute(valueAttr);
}

void HTMLProgressElement::didElementStateChange() {
  setValueWidthPercentage(position() * 100);
  if (LayoutProgressItem layoutItem = LayoutProgressItem(layoutProgress()))
    layoutItem.updateFromElement();
}

void HTMLProgressElement::didAddUserAgentShadowRoot(ShadowRoot& root) {
  DCHECK(!m_value);

  ProgressShadowElement* inner = ProgressShadowElement::create(document());
  inner->setShadowPseudoId(AtomicString("-webkit-progress-inner-element"));
  root.appendChild(inner);

  ProgressShadowElement* bar = ProgressShadowElement::create(document());
  bar->setShadowPseudoId(AtomicString("-webkit-progress-bar"));
  m_value = ProgressShadowElement::create(document());
  m_value->setShadowPseudoId(AtomicString("-webkit-progress-value"));
  setValueWidthPercentage(HTMLProgressElement::IndeterminatePosition * 100);
  bar->appendChild(m_value);

  inner->appendChild(bar);
}

bool HTMLProgressElement::shouldAppearIndeterminate() const {
  return !isDeterminate();
}

DEFINE_TRACE(HTMLProgressElement) {
  visitor->trace(m_value);
  LabelableElement::trace(visitor);
}

void HTMLProgressElement::setValueWidthPercentage(double width) const {
  m_value->setInlineStyleProperty(CSSPropertyWidth, width,
                                  CSSPrimitiveValue::UnitType::Percentage);
}

}  // namespace blink
