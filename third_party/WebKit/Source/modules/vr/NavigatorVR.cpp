// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/NavigatorVR.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "modules/vr/VRController.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRGetDevicesCallback.h"
#include "modules/vr/VRPose.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/Platform.h"
#include "wtf/PtrUtil.h"

namespace blink {

NavigatorVR* NavigatorVR::from(Document& document) {
  if (!document.frame() || !document.frame()->domWindow())
    return 0;
  Navigator& navigator = *document.frame()->domWindow()->navigator();
  return &from(navigator);
}

NavigatorVR& NavigatorVR::from(Navigator& navigator) {
  NavigatorVR* supplement = static_cast<NavigatorVR*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorVR(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* scriptState,
                                         Navigator& navigator) {
  return NavigatorVR::from(navigator).getVRDisplays(scriptState);
}

ScriptPromise NavigatorVR::getVRDisplays(ScriptState* scriptState) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  if (!document()) {
    DOMException* exception = DOMException::create(
        InvalidStateError, "The object is no longer associated to a document.");
    resolver->reject(exception);
    return promise;
  }

  UseCounter::count(*document(), UseCounter::VRGetDisplays);
  ExecutionContext* executionContext = scriptState->getExecutionContext();
  if (!executionContext->isSecureContext())
    UseCounter::count(*document(), UseCounter::VRGetDisplaysInsecureOrigin);

  Platform::current()->recordRapporURL("VR.WebVR.GetDisplays",
                                       document()->url());

  controller()->getDisplays(resolver);

  return promise;
}

VRController* NavigatorVR::controller() {
  if (!supplementable()->frame())
    return 0;

  if (!m_controller) {
    m_controller = new VRController(this);
  }

  return m_controller;
}

Document* NavigatorVR::document() {
  return supplementable()->frame() ? supplementable()->frame()->document()
                                   : nullptr;
}

DEFINE_TRACE(NavigatorVR) {
  visitor->trace(m_controller);
  Supplement<Navigator>::trace(visitor);
  PageVisibilityObserver::trace(visitor);
}

NavigatorVR::NavigatorVR(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      PageVisibilityObserver(navigator.frame()->page()) {
  navigator.frame()->domWindow()->registerEventListenerObserver(this);
}

NavigatorVR::~NavigatorVR() {}

const char* NavigatorVR::supplementName() {
  return "NavigatorVR";
}

void NavigatorVR::enqueueVREvent(VRDisplayEvent* event) {
  if (supplementable()->frame()) {
    supplementable()->frame()->domWindow()->enqueueWindowEvent(event);
  }
}

void NavigatorVR::dispatchVRGestureEvent(VRDisplayEvent* event) {
  if (!(supplementable()->frame()))
    return;
  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(document()));
  LocalDOMWindow* window = supplementable()->frame()->domWindow();
  DCHECK(window);
  event->setTarget(window);
  window->dispatchEvent(event);
}

void NavigatorVR::pageVisibilityChanged() {
  if (!page())
    return;
  if (m_controller) {
    m_controller->setListeningForActivate(page()->isPageVisible() &&
                                          m_listeningForActivate);
  }
}

void NavigatorVR::didAddEventListener(LocalDOMWindow* window,
                                      const AtomicString& eventType) {
  if (eventType == EventTypeNames::vrdisplayactivate) {
    controller()->setListeningForActivate(true);
    m_listeningForActivate = true;
  } else if (eventType == EventTypeNames::vrdisplayconnect) {
    // If the page is listening for connection events make sure we've created a
    // controller so that we'll be notified of new devices.
    controller();
  }
}

void NavigatorVR::didRemoveEventListener(LocalDOMWindow* window,
                                         const AtomicString& eventType) {
  if (eventType == EventTypeNames::vrdisplayactivate &&
      !window->hasEventListeners(EventTypeNames::vrdisplayactivate)) {
    controller()->setListeningForActivate(false);
    m_listeningForActivate = false;
  }
}

void NavigatorVR::didRemoveAllEventListeners(LocalDOMWindow* window) {
  if (m_controller) {
    m_controller->setListeningForActivate(false);
    m_listeningForActivate = false;
  }
}

}  // namespace blink
