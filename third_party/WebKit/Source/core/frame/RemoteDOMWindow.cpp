// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteDOMWindow.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/MediaQueryList.h"
#include "core/dom/Document.h"
#include "core/frame/RemoteFrameClient.h"

namespace blink {

ExecutionContext* RemoteDOMWindow::getExecutionContext() const {
  return nullptr;
}

DEFINE_TRACE(RemoteDOMWindow) {
  DOMWindow::trace(visitor);
}

Screen* RemoteDOMWindow::screen() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

History* RemoteDOMWindow::history() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

BarProp* RemoteDOMWindow::locationbar() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

BarProp* RemoteDOMWindow::menubar() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

BarProp* RemoteDOMWindow::personalbar() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

BarProp* RemoteDOMWindow::scrollbars() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

BarProp* RemoteDOMWindow::statusbar() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

BarProp* RemoteDOMWindow::toolbar() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

Navigator* RemoteDOMWindow::navigator() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

bool RemoteDOMWindow::offscreenBuffering() const {
  ASSERT_NOT_REACHED();
  return false;
}

int RemoteDOMWindow::outerHeight() const {
  ASSERT_NOT_REACHED();
  return 0;
}

int RemoteDOMWindow::outerWidth() const {
  ASSERT_NOT_REACHED();
  return 0;
}

int RemoteDOMWindow::innerHeight() const {
  ASSERT_NOT_REACHED();
  return 0;
}

int RemoteDOMWindow::innerWidth() const {
  ASSERT_NOT_REACHED();
  return 0;
}

int RemoteDOMWindow::screenX() const {
  ASSERT_NOT_REACHED();
  return 0;
}

int RemoteDOMWindow::screenY() const {
  ASSERT_NOT_REACHED();
  return 0;
}

double RemoteDOMWindow::scrollX() const {
  ASSERT_NOT_REACHED();
  return 0.0;
}

double RemoteDOMWindow::scrollY() const {
  ASSERT_NOT_REACHED();
  return 0.0;
}

const AtomicString& RemoteDOMWindow::name() const {
  // FIXME: Implement.
  return emptyAtom;
}

void RemoteDOMWindow::setName(const AtomicString&) {}

String RemoteDOMWindow::status() const {
  ASSERT_NOT_REACHED();
  return String();
}

void RemoteDOMWindow::setStatus(const String&) {
  ASSERT_NOT_REACHED();
}

String RemoteDOMWindow::defaultStatus() const {
  ASSERT_NOT_REACHED();
  return String();
}

void RemoteDOMWindow::setDefaultStatus(const String&) {
  ASSERT_NOT_REACHED();
}

Document* RemoteDOMWindow::document() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

StyleMedia* RemoteDOMWindow::styleMedia() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

double RemoteDOMWindow::devicePixelRatio() const {
  ASSERT_NOT_REACHED();
  return 0.0;
}

ApplicationCache* RemoteDOMWindow::applicationCache() const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

int RemoteDOMWindow::orientation() const {
  ASSERT_NOT_REACHED();
  return 0;
}

DOMSelection* RemoteDOMWindow::getSelection() {
  ASSERT_NOT_REACHED();
  return 0;
}

void RemoteDOMWindow::blur() {
  // FIXME: Implement.
}

void RemoteDOMWindow::print(ScriptState*) {
  ASSERT_NOT_REACHED();
}

void RemoteDOMWindow::stop() {
  // FIXME: Implement.
}

void RemoteDOMWindow::alert(ScriptState*, const String& message) {
  ASSERT_NOT_REACHED();
}

bool RemoteDOMWindow::confirm(ScriptState*, const String& message) {
  ASSERT_NOT_REACHED();
  return false;
}

String RemoteDOMWindow::prompt(ScriptState*,
                               const String& message,
                               const String& defaultValue) {
  ASSERT_NOT_REACHED();
  return String();
}

bool RemoteDOMWindow::find(const String&,
                           bool caseSensitive,
                           bool backwards,
                           bool wrap,
                           bool wholeWord,
                           bool searchInFrames,
                           bool showDialog) const {
  ASSERT_NOT_REACHED();
  return false;
}

void RemoteDOMWindow::scrollBy(double x, double y, ScrollBehavior) const {
  ASSERT_NOT_REACHED();
}

void RemoteDOMWindow::scrollBy(const ScrollToOptions&) const {
  ASSERT_NOT_REACHED();
}

void RemoteDOMWindow::scrollTo(double x, double y) const {
  ASSERT_NOT_REACHED();
}

void RemoteDOMWindow::scrollTo(const ScrollToOptions&) const {
  ASSERT_NOT_REACHED();
}

void RemoteDOMWindow::moveBy(int x, int y) const {
  ASSERT_NOT_REACHED();
}

void RemoteDOMWindow::moveTo(int x, int y) const {
  ASSERT_NOT_REACHED();
}

void RemoteDOMWindow::resizeBy(int x, int y) const {
  ASSERT_NOT_REACHED();
}

void RemoteDOMWindow::resizeTo(int width, int height) const {
  ASSERT_NOT_REACHED();
}

MediaQueryList* RemoteDOMWindow::matchMedia(const String&) {
  ASSERT_NOT_REACHED();
  return nullptr;
}

CSSStyleDeclaration* RemoteDOMWindow::getComputedStyle(
    Element*,
    const String& pseudoElt) const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

CSSRuleList* RemoteDOMWindow::getMatchedCSSRules(
    Element*,
    const String& pseudoElt) const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

int RemoteDOMWindow::requestAnimationFrame(FrameRequestCallback*) {
  ASSERT_NOT_REACHED();
  return 0;
}

int RemoteDOMWindow::webkitRequestAnimationFrame(FrameRequestCallback*) {
  ASSERT_NOT_REACHED();
  return 0;
}

void RemoteDOMWindow::cancelAnimationFrame(int id) {
  ASSERT_NOT_REACHED();
}

int RemoteDOMWindow::requestIdleCallback(IdleRequestCallback*,
                                         const IdleRequestOptions&) {
  ASSERT_NOT_REACHED();
  return 0;
}

void RemoteDOMWindow::cancelIdleCallback(int id) {
  ASSERT_NOT_REACHED();
}

CustomElementRegistry* RemoteDOMWindow::customElements(ScriptState*) const {
  ASSERT_NOT_REACHED();
  return nullptr;
}

RemoteDOMWindow::RemoteDOMWindow(RemoteFrame& frame) : DOMWindow(frame) {}

void RemoteDOMWindow::frameDetached() {
  disconnectFromFrame();
}

void RemoteDOMWindow::schedulePostMessage(MessageEvent* event,
                                          PassRefPtr<SecurityOrigin> target,
                                          Document* source) {
  frame()->client()->forwardPostMessage(event, std::move(target),
                                        source->frame());
}

}  // namespace blink
