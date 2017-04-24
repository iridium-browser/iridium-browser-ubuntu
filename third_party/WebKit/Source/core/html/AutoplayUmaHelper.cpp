// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/AutoplayUmaHelper.h"

#include "core/dom/Document.h"
#include "core/dom/ElementVisibilityObserver.h"
#include "core/events/Event.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "platform/Histogram.h"
#include "public/platform/Platform.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

const int32_t maxOffscreenDurationUmaMS = 60 * 60 * 1000;
const int32_t offscreenDurationUmaBucketCount = 50;

}  // namespace

AutoplayUmaHelper* AutoplayUmaHelper::create(HTMLMediaElement* element) {
  return new AutoplayUmaHelper(element);
}

AutoplayUmaHelper::AutoplayUmaHelper(HTMLMediaElement* element)
    : EventListener(CPPEventListenerType),
      ContextLifecycleObserver(nullptr),
      m_source(AutoplaySource::NumberOfSources),
      m_element(element),
      m_mutedVideoPlayMethodVisibilityObserver(nullptr),
      m_mutedVideoAutoplayOffscreenStartTimeMS(0),
      m_mutedVideoAutoplayOffscreenDurationMS(0),
      m_isVisible(false),
      m_mutedVideoOffscreenDurationVisibilityObserver(nullptr) {}

AutoplayUmaHelper::~AutoplayUmaHelper() = default;

bool AutoplayUmaHelper::operator==(const EventListener& other) const {
  return this == &other;
}

void AutoplayUmaHelper::onAutoplayInitiated(AutoplaySource source) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, videoHistogram,
                      ("Media.Video.Autoplay",
                       static_cast<int>(AutoplaySource::NumberOfSources)));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, mutedVideoHistogram,
                      ("Media.Video.Autoplay.Muted",
                       static_cast<int>(AutoplaySource::NumberOfSources)));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, audioHistogram,
                      ("Media.Audio.Autoplay",
                       static_cast<int>(AutoplaySource::NumberOfSources)));
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, blockedMutedVideoHistogram,
      ("Media.Video.Autoplay.Muted.Blocked", AutoplayBlockedReasonMax));

  // Autoplay already initiated
  // TODO(zqzhang): how about having autoplay attribute and calling `play()` in
  // the script?
  if (hasSource())
    return;

  m_source = source;

  // Record the source.
  if (m_element->isHTMLVideoElement()) {
    videoHistogram.count(static_cast<int>(m_source));
    if (m_element->muted())
      mutedVideoHistogram.count(static_cast<int>(m_source));
  } else {
    audioHistogram.count(static_cast<int>(m_source));
  }

  // Record the child frame and top-level frame URLs for autoplay muted videos
  // by attribute.
  if (m_element->isHTMLVideoElement() && m_element->muted()) {
    if (source == AutoplaySource::Attribute) {
      Platform::current()->recordRapporURL(
          "Media.Video.Autoplay.Muted.Attribute.Frame",
          m_element->document().url());
    } else {
      DCHECK(source == AutoplaySource::Method);
      Platform::current()->recordRapporURL(
          "Media.Video.Autoplay.Muted.PlayMethod.Frame",
          m_element->document().url());
    }
  }

  // Record if it will be blocked by Data Saver or Autoplay setting.
  if (m_element->isHTMLVideoElement() && m_element->muted() &&
      RuntimeEnabledFeatures::autoplayMutedVideosEnabled()) {
    bool dataSaverEnabled =
        m_element->document().settings() &&
        m_element->document().settings()->getDataSaverEnabled();
    bool blockedBySetting = !m_element->isAutoplayAllowedPerSettings();

    if (dataSaverEnabled && blockedBySetting) {
      blockedMutedVideoHistogram.count(
          AutoplayBlockedReasonDataSaverAndSetting);
    } else if (dataSaverEnabled) {
      blockedMutedVideoHistogram.count(AutoplayBlockedReasonDataSaver);
    } else if (blockedBySetting) {
      blockedMutedVideoHistogram.count(AutoplayBlockedReasonSetting);
    }
  }

  m_element->addEventListener(EventTypeNames::playing, this, false);
}

void AutoplayUmaHelper::recordCrossOriginAutoplayResult(
    CrossOriginAutoplayResult result) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplayResultHistogram,
      ("Media.Autoplay.CrossOrigin.Result",
       static_cast<int>(CrossOriginAutoplayResult::NumberOfResults)));

  if (!m_element->isHTMLVideoElement())
    return;
  if (!m_element->isInCrossOriginFrame())
    return;

  // Record each metric only once per element, since the metric focuses on the
  // site distribution. If a page calls play() multiple times, it will be
  // recorded only once.
  if (m_recordedCrossOriginAutoplayResults.count(result))
    return;

  switch (result) {
    case CrossOriginAutoplayResult::AutoplayAllowed:
      // Record metric
      Platform::current()->recordRapporURL(
          "Media.Autoplay.CrossOrigin.Allowed.ChildFrame",
          m_element->document().url());
      Platform::current()->recordRapporURL(
          "Media.Autoplay.CrossOrigin.Allowed.TopLevelFrame",
          m_element->document().topDocument().url());
      autoplayResultHistogram.count(static_cast<int>(result));
      m_recordedCrossOriginAutoplayResults.insert(result);
      break;
    case CrossOriginAutoplayResult::AutoplayBlocked:
      Platform::current()->recordRapporURL(
          "Media.Autoplay.CrossOrigin.Blocked.ChildFrame",
          m_element->document().url());
      Platform::current()->recordRapporURL(
          "Media.Autoplay.CrossOrigin.Blocked.TopLevelFrame",
          m_element->document().topDocument().url());
      autoplayResultHistogram.count(static_cast<int>(result));
      m_recordedCrossOriginAutoplayResults.insert(result);
      break;
    case CrossOriginAutoplayResult::PlayedWithGesture:
      // Record this metric only when the video has been blocked from autoplay
      // previously. This is to record the sites having videos that are blocked
      // to autoplay but the user starts the playback by gesture.
      if (!m_recordedCrossOriginAutoplayResults.count(
              CrossOriginAutoplayResult::AutoplayBlocked)) {
        return;
      }
      Platform::current()->recordRapporURL(
          "Media.Autoplay.CrossOrigin.PlayedWithGestureAfterBlock.ChildFrame",
          m_element->document().url());
      Platform::current()->recordRapporURL(
          "Media.Autoplay.CrossOrigin.PlayedWithGestureAfterBlock."
          "TopLevelFrame",
          m_element->document().topDocument().url());
      autoplayResultHistogram.count(static_cast<int>(result));
      m_recordedCrossOriginAutoplayResults.insert(result);
      break;
    case CrossOriginAutoplayResult::UserPaused:
      if (!shouldRecordUserPausedAutoplayingCrossOriginVideo())
        return;
      if (m_element->ended() || m_element->seeking())
        return;
      Platform::current()->recordRapporURL(
          "Media.Autoplay.CrossOrigin.UserPausedAutoplayingVideo.ChildFrame",
          m_element->document().url());
      Platform::current()->recordRapporURL(
          "Media.Autoplay.CrossOrigin.UserPausedAutoplayingVideo."
          "TopLevelFrame",
          m_element->document().topDocument().url());
      autoplayResultHistogram.count(static_cast<int>(result));
      m_recordedCrossOriginAutoplayResults.insert(result);
      break;
    default:
      NOTREACHED();
  }
}

void AutoplayUmaHelper::recordAutoplayUnmuteStatus(
    AutoplayUnmuteActionStatus status) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplayUnmuteHistogram,
      ("Media.Video.Autoplay.Muted.UnmuteAction",
       static_cast<int>(AutoplayUnmuteActionStatus::NumberOfStatus)));

  autoplayUnmuteHistogram.count(static_cast<int>(status));
}

void AutoplayUmaHelper::didMoveToNewDocument(Document& oldDocument) {
  if (!shouldListenToContextDestroyed())
    return;

  setContext(&m_element->document());
}

void AutoplayUmaHelper::onVisibilityChangedForMutedVideoPlayMethodBecomeVisible(
    bool isVisible) {
  if (!isVisible || !m_mutedVideoPlayMethodVisibilityObserver)
    return;

  maybeStopRecordingMutedVideoPlayMethodBecomeVisible(true);
}

void AutoplayUmaHelper::onVisibilityChangedForMutedVideoOffscreenDuration(
    bool isVisible) {
  if (isVisible == m_isVisible)
    return;

  if (isVisible)
    m_mutedVideoAutoplayOffscreenDurationMS +=
        static_cast<int64_t>(monotonicallyIncreasingTimeMS()) -
        m_mutedVideoAutoplayOffscreenStartTimeMS;
  else
    m_mutedVideoAutoplayOffscreenStartTimeMS =
        static_cast<int64_t>(monotonicallyIncreasingTimeMS());

  m_isVisible = isVisible;
}

void AutoplayUmaHelper::handleEvent(ExecutionContext* executionContext,
                                    Event* event) {
  if (event->type() == EventTypeNames::playing)
    handlePlayingEvent();
  else if (event->type() == EventTypeNames::pause)
    handlePauseEvent();
  else
    NOTREACHED();
}

void AutoplayUmaHelper::handlePlayingEvent() {
  maybeStartRecordingMutedVideoPlayMethodBecomeVisible();
  maybeStartRecordingMutedVideoOffscreenDuration();

  m_element->removeEventListener(EventTypeNames::playing, this, false);
}

void AutoplayUmaHelper::handlePauseEvent() {
  maybeStopRecordingMutedVideoOffscreenDuration();
  maybeRecordUserPausedAutoplayingCrossOriginVideo();
}

void AutoplayUmaHelper::contextDestroyed(ExecutionContext*) {
  handleContextDestroyed();
}

void AutoplayUmaHelper::handleContextDestroyed() {
  maybeStopRecordingMutedVideoPlayMethodBecomeVisible(false);
  maybeStopRecordingMutedVideoOffscreenDuration();
}

void AutoplayUmaHelper::maybeStartRecordingMutedVideoPlayMethodBecomeVisible() {
  if (m_source != AutoplaySource::Method || !m_element->isHTMLVideoElement() ||
      !m_element->muted())
    return;

  m_mutedVideoPlayMethodVisibilityObserver = new ElementVisibilityObserver(
      m_element,
      WTF::bind(&AutoplayUmaHelper::
                    onVisibilityChangedForMutedVideoPlayMethodBecomeVisible,
                wrapWeakPersistent(this)));
  m_mutedVideoPlayMethodVisibilityObserver->start();
  setContext(&m_element->document());
}

void AutoplayUmaHelper::maybeStopRecordingMutedVideoPlayMethodBecomeVisible(
    bool visible) {
  if (!m_mutedVideoPlayMethodVisibilityObserver)
    return;

  DEFINE_STATIC_LOCAL(BooleanHistogram, histogram,
                      ("Media.Video.Autoplay.Muted.PlayMethod.BecomesVisible"));

  histogram.count(visible);
  m_mutedVideoPlayMethodVisibilityObserver->stop();
  m_mutedVideoPlayMethodVisibilityObserver = nullptr;
  maybeUnregisterContextDestroyedObserver();
}

void AutoplayUmaHelper::maybeStartRecordingMutedVideoOffscreenDuration() {
  if (!m_element->isHTMLVideoElement() || !m_element->muted())
    return;

  // Start recording muted video playing offscreen duration.
  m_mutedVideoAutoplayOffscreenStartTimeMS =
      static_cast<int64_t>(monotonicallyIncreasingTimeMS());
  m_isVisible = false;
  m_mutedVideoOffscreenDurationVisibilityObserver =
      new ElementVisibilityObserver(
          m_element,
          WTF::bind(&AutoplayUmaHelper::
                        onVisibilityChangedForMutedVideoOffscreenDuration,
                    wrapWeakPersistent(this)));
  m_mutedVideoOffscreenDurationVisibilityObserver->start();
  m_element->addEventListener(EventTypeNames::pause, this, false);
  setContext(&m_element->document());
}

void AutoplayUmaHelper::maybeStopRecordingMutedVideoOffscreenDuration() {
  if (!m_mutedVideoOffscreenDurationVisibilityObserver)
    return;

  if (!m_isVisible)
    m_mutedVideoAutoplayOffscreenDurationMS +=
        static_cast<int64_t>(monotonicallyIncreasingTimeMS()) -
        m_mutedVideoAutoplayOffscreenStartTimeMS;

  // Since histograms uses int32_t, the duration needs to be limited to
  // std::numeric_limits<int32_t>::max().
  int32_t boundedTime = static_cast<int32_t>(
      std::min<int64_t>(m_mutedVideoAutoplayOffscreenDurationMS,
                        std::numeric_limits<int32_t>::max()));

  if (m_source == AutoplaySource::Method) {
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, durationHistogram,
        ("Media.Video.Autoplay.Muted.PlayMethod.OffscreenDuration", 1,
         maxOffscreenDurationUmaMS, offscreenDurationUmaBucketCount));
    durationHistogram.count(boundedTime);
  }
  m_mutedVideoOffscreenDurationVisibilityObserver->stop();
  m_mutedVideoOffscreenDurationVisibilityObserver = nullptr;
  m_mutedVideoAutoplayOffscreenDurationMS = 0;
  maybeUnregisterMediaElementPauseListener();
  maybeUnregisterContextDestroyedObserver();
}

void AutoplayUmaHelper::maybeRecordUserPausedAutoplayingCrossOriginVideo() {
  recordCrossOriginAutoplayResult(CrossOriginAutoplayResult::UserPaused);
  maybeUnregisterMediaElementPauseListener();
}

void AutoplayUmaHelper::maybeUnregisterContextDestroyedObserver() {
  if (!shouldListenToContextDestroyed()) {
    setContext(nullptr);
  }
}

void AutoplayUmaHelper::maybeUnregisterMediaElementPauseListener() {
  if (m_mutedVideoOffscreenDurationVisibilityObserver)
    return;
  if (shouldRecordUserPausedAutoplayingCrossOriginVideo())
    return;
  m_element->removeEventListener(EventTypeNames::pause, this, false);
}

bool AutoplayUmaHelper::shouldListenToContextDestroyed() const {
  return m_mutedVideoPlayMethodVisibilityObserver ||
         m_mutedVideoOffscreenDurationVisibilityObserver;
}

bool AutoplayUmaHelper::shouldRecordUserPausedAutoplayingCrossOriginVideo()
    const {
  return m_element->isInCrossOriginFrame() && m_element->isHTMLVideoElement() &&
         m_source != AutoplaySource::NumberOfSources &&
         !m_recordedCrossOriginAutoplayResults.count(
             CrossOriginAutoplayResult::UserPaused);
}

DEFINE_TRACE(AutoplayUmaHelper) {
  EventListener::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  visitor->trace(m_element);
  visitor->trace(m_mutedVideoPlayMethodVisibilityObserver);
  visitor->trace(m_mutedVideoOffscreenDurationVisibilityObserver);
}

}  // namespace blink
