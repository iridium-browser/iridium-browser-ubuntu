// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/ScreenOrientationControllerImpl.h"

#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "modules/screen_orientation/ScreenOrientation.h"
#include "modules/screen_orientation/ScreenOrientationDispatcher.h"
#include "platform/LayoutTestSupport.h"
#include "platform/ScopedOrientationChangeIndicator.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationClient.h"
#include <memory>
#include <utility>

namespace blink {

ScreenOrientationControllerImpl::~ScreenOrientationControllerImpl() = default;

void ScreenOrientationControllerImpl::provideTo(
    LocalFrame& frame,
    WebScreenOrientationClient* client) {
  ScreenOrientationController::provideTo(
      frame, new ScreenOrientationControllerImpl(frame, client));
}

ScreenOrientationControllerImpl* ScreenOrientationControllerImpl::from(
    LocalFrame& frame) {
  return static_cast<ScreenOrientationControllerImpl*>(
      ScreenOrientationController::from(frame));
}

ScreenOrientationControllerImpl::ScreenOrientationControllerImpl(
    LocalFrame& frame,
    WebScreenOrientationClient* client)
    : ScreenOrientationController(frame),
      ContextLifecycleObserver(frame.document()),
      PlatformEventController(&frame),
      m_client(client),
      m_dispatchEventTimer(
          TaskRunnerHelper::get(TaskType::MiscPlatformAPI, &frame),
          this,
          &ScreenOrientationControllerImpl::dispatchEventTimerFired) {}

// Compute the screen orientation using the orientation angle and the screen
// width / height.
WebScreenOrientationType ScreenOrientationControllerImpl::computeOrientation(
    const IntRect& rect,
    uint16_t rotation) {
  // Bypass orientation detection in layout tests to get consistent results.
  // FIXME: The screen dimension should be fixed when running the layout tests
  // to avoid such issues.
  if (LayoutTestSupport::isRunningLayoutTest())
    return WebScreenOrientationPortraitPrimary;

  bool isTallDisplay = rotation % 180 ? rect.height() < rect.width()
                                      : rect.height() > rect.width();
  switch (rotation) {
    case 0:
      return isTallDisplay ? WebScreenOrientationPortraitPrimary
                           : WebScreenOrientationLandscapePrimary;
    case 90:
      return isTallDisplay ? WebScreenOrientationLandscapePrimary
                           : WebScreenOrientationPortraitSecondary;
    case 180:
      return isTallDisplay ? WebScreenOrientationPortraitSecondary
                           : WebScreenOrientationLandscapeSecondary;
    case 270:
      return isTallDisplay ? WebScreenOrientationLandscapeSecondary
                           : WebScreenOrientationPortraitPrimary;
    default:
      NOTREACHED();
      return WebScreenOrientationPortraitPrimary;
  }
}

void ScreenOrientationControllerImpl::updateOrientation() {
  DCHECK(m_orientation);
  DCHECK(frame());
  DCHECK(frame()->page());
  ChromeClient& chromeClient = frame()->page()->chromeClient();
  WebScreenInfo screenInfo = chromeClient.screenInfo();
  WebScreenOrientationType orientationType = screenInfo.orientationType;
  if (orientationType == WebScreenOrientationUndefined) {
    // The embedder could not provide us with an orientation, deduce it
    // ourselves.
    orientationType = computeOrientation(chromeClient.screenInfo().rect,
                                         screenInfo.orientationAngle);
  }
  DCHECK(orientationType != WebScreenOrientationUndefined);

  m_orientation->setType(orientationType);
  m_orientation->setAngle(screenInfo.orientationAngle);
}

bool ScreenOrientationControllerImpl::isActive() const {
  return m_orientation && m_client;
}

bool ScreenOrientationControllerImpl::isVisible() const {
  return page() && page()->isPageVisible();
}

bool ScreenOrientationControllerImpl::isActiveAndVisible() const {
  return isActive() && isVisible();
}

void ScreenOrientationControllerImpl::pageVisibilityChanged() {
  notifyDispatcher();

  if (!isActiveAndVisible())
    return;

  DCHECK(frame());
  DCHECK(frame()->page());

  // The orientation type and angle are tied in a way that if the angle has
  // changed, the type must have changed.
  unsigned short currentAngle =
      frame()->page()->chromeClient().screenInfo().orientationAngle;

  // FIXME: sendOrientationChangeEvent() currently send an event all the
  // children of the frame, so it should only be called on the frame on
  // top of the tree. We would need the embedder to call
  // sendOrientationChangeEvent on every WebFrame part of a WebView to be
  // able to remove this.
  if (frame() == frame()->localFrameRoot() &&
      m_orientation->angle() != currentAngle)
    notifyOrientationChanged();
}

void ScreenOrientationControllerImpl::notifyOrientationChanged() {
  if (!isVisible() || !frame())
    return;

  if (isActive())
    updateOrientation();

  // Keep track of the frames that need to be notified before notifying the
  // current frame as it will prevent side effects from the change event
  // handlers.
  HeapVector<Member<LocalFrame>> childFrames;
  for (Frame* child = frame()->tree().firstChild(); child;
       child = child->tree().nextSibling()) {
    if (child->isLocalFrame())
      childFrames.push_back(toLocalFrame(child));
  }

  // Notify current orientation object.
  if (isActive() && !m_dispatchEventTimer.isActive())
    m_dispatchEventTimer.startOneShot(0, BLINK_FROM_HERE);

  // ... and child frames, if they have a ScreenOrientationControllerImpl.
  for (size_t i = 0; i < childFrames.size(); ++i) {
    if (ScreenOrientationControllerImpl* controller =
            ScreenOrientationControllerImpl::from(*childFrames[i])) {
      controller->notifyOrientationChanged();
    }
  }
}

void ScreenOrientationControllerImpl::setOrientation(
    ScreenOrientation* orientation) {
  m_orientation = orientation;
  if (m_orientation)
    updateOrientation();
  notifyDispatcher();
}

void ScreenOrientationControllerImpl::lock(
    WebScreenOrientationLockType orientation,
    std::unique_ptr<WebLockOrientationCallback> callback) {
  // When detached, the client is no longer valid.
  if (!m_client)
    return;
  m_client->lockOrientation(orientation, std::move(callback));
  m_activeLock = true;
}

void ScreenOrientationControllerImpl::unlock() {
  // When detached, the client is no longer valid.
  if (!m_client)
    return;
  m_client->unlockOrientation();
  m_activeLock = false;
}

bool ScreenOrientationControllerImpl::maybeHasActiveLock() const {
  return m_activeLock;
}

void ScreenOrientationControllerImpl::dispatchEventTimerFired(TimerBase*) {
  if (!m_orientation)
    return;

  ScopedOrientationChangeIndicator orientationChangeIndicator;
  m_orientation->dispatchEvent(Event::create(EventTypeNames::change));
}

void ScreenOrientationControllerImpl::didUpdateData() {
  // Do nothing.
}

void ScreenOrientationControllerImpl::registerWithDispatcher() {
  ScreenOrientationDispatcher::instance().addController(this);
}

void ScreenOrientationControllerImpl::unregisterWithDispatcher() {
  ScreenOrientationDispatcher::instance().removeController(this);
}

bool ScreenOrientationControllerImpl::hasLastData() {
  return true;
}

void ScreenOrientationControllerImpl::contextDestroyed(ExecutionContext*) {
  stopUpdating();
  m_client = nullptr;
  m_activeLock = false;
}

void ScreenOrientationControllerImpl::notifyDispatcher() {
  if (m_orientation && page()->isPageVisible())
    startUpdating();
  else
    stopUpdating();
}

DEFINE_TRACE(ScreenOrientationControllerImpl) {
  visitor->trace(m_orientation);
  ContextLifecycleObserver::trace(visitor);
  Supplement<LocalFrame>::trace(visitor);
  PlatformEventController::trace(visitor);
}

}  // namespace blink
