/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "modules/vibration/NavigatorVibration.h"

#include "bindings/core/v8/ConditionalFeatures.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "modules/vibration/VibrationController.h"
#include "platform/Histogram.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/site_engagement.mojom-blink.h"

namespace blink {

NavigatorVibration::NavigatorVibration(Navigator& navigator)
    : ContextLifecycleObserver(navigator.frame()->document()) {}

NavigatorVibration::~NavigatorVibration() {}

// static
NavigatorVibration& NavigatorVibration::from(Navigator& navigator) {
  NavigatorVibration* navigatorVibration = static_cast<NavigatorVibration*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!navigatorVibration) {
    navigatorVibration = new NavigatorVibration(navigator);
    Supplement<Navigator>::provideTo(navigator, supplementName(),
                                     navigatorVibration);
  }
  return *navigatorVibration;
}

// static
const char* NavigatorVibration::supplementName() {
  return "NavigatorVibration";
}

// static
bool NavigatorVibration::vibrate(Navigator& navigator, unsigned time) {
  VibrationPattern pattern;
  pattern.push_back(time);
  return NavigatorVibration::vibrate(navigator, pattern);
}

// static
bool NavigatorVibration::vibrate(Navigator& navigator,
                                 const VibrationPattern& pattern) {
  LocalFrame* frame = navigator.frame();

  // There will be no frame if the window has been closed, but a JavaScript
  // reference to |window| or |navigator| was retained in another window.
  if (!frame)
    return false;
  collectHistogramMetrics(*frame);

  DCHECK(frame->document());
  DCHECK(frame->page());

  if (!frame->page()->isPageVisible())
    return false;

  // TODO(lunalu): When FeaturePolicy is ready, take out the check for the
  // runtime flag. Please pay attention to the user gesture code below.
  if (RuntimeEnabledFeatures::featurePolicyEnabled() &&
      !isFeatureEnabledInFrame(blink::kVibrateFeature, frame)) {
    frame->domWindow()->printErrorMessage(
        "Navigator.vibrate() is not enabled in feature policy for this "
        "frame.");
    return false;
  }

  if (!RuntimeEnabledFeatures::featurePolicyEnabled() &&
      frame->isCrossOriginSubframe() && !frame->hasReceivedUserGesture()) {
    frame->domWindow()->printErrorMessage(
        "Blocked call to navigator.vibrate inside a cross-origin iframe "
        "because the frame has never been activated by the user: "
        "https://www.chromestatus.com/feature/5682658461876224.");
    return false;
  }

  return NavigatorVibration::from(navigator).controller(*frame)->vibrate(
      pattern);
}

// static
void NavigatorVibration::collectHistogramMetrics(const LocalFrame& frame) {
  NavigatorVibrationType type;
  bool userGesture = UserGestureIndicator::processingUserGesture();
  UseCounter::count(&frame, UseCounter::NavigatorVibrate);
  if (!frame.isMainFrame()) {
    UseCounter::count(&frame, UseCounter::NavigatorVibrateSubFrame);
    if (frame.isCrossOriginSubframe()) {
      if (userGesture)
        type = NavigatorVibrationType::CrossOriginSubFrameWithUserGesture;
      else
        type = NavigatorVibrationType::CrossOriginSubFrameNoUserGesture;
    } else {
      if (userGesture)
        type = NavigatorVibrationType::SameOriginSubFrameWithUserGesture;
      else
        type = NavigatorVibrationType::SameOriginSubFrameNoUserGesture;
    }
  } else {
    if (userGesture)
      type = NavigatorVibrationType::MainFrameWithUserGesture;
    else
      type = NavigatorVibrationType::MainFrameNoUserGesture;
  }
  DEFINE_STATIC_LOCAL(EnumerationHistogram, NavigatorVibrateHistogram,
                      ("Vibration.Context", NavigatorVibrationType::EnumMax));
  NavigatorVibrateHistogram.count(type);

  switch (frame.document()->getEngagementLevel()) {
    case mojom::blink::EngagementLevel::NONE:
      UseCounter::count(&frame, UseCounter::NavigatorVibrateEngagementNone);
      break;
    case mojom::blink::EngagementLevel::MINIMAL:
      UseCounter::count(&frame, UseCounter::NavigatorVibrateEngagementMinimal);
      break;
    case mojom::blink::EngagementLevel::LOW:
      UseCounter::count(&frame, UseCounter::NavigatorVibrateEngagementLow);
      break;
    case mojom::blink::EngagementLevel::MEDIUM:
      UseCounter::count(&frame, UseCounter::NavigatorVibrateEngagementMedium);
      break;
    case mojom::blink::EngagementLevel::HIGH:
      UseCounter::count(&frame, UseCounter::NavigatorVibrateEngagementHigh);
      break;
    case mojom::blink::EngagementLevel::MAX:
      UseCounter::count(&frame, UseCounter::NavigatorVibrateEngagementMax);
      break;
  }
}

VibrationController* NavigatorVibration::controller(const LocalFrame& frame) {
  if (!m_controller)
    m_controller = new VibrationController(*frame.document());

  return m_controller.get();
}

void NavigatorVibration::contextDestroyed(ExecutionContext*) {
  if (m_controller) {
    m_controller->cancel();
    m_controller = nullptr;
  }
}

DEFINE_TRACE(NavigatorVibration) {
  visitor->trace(m_controller);
  Supplement<Navigator>::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
