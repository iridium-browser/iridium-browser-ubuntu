/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXMediaControls.h"

#include "core/layout/LayoutObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using blink::WebLocalizedString;
using namespace HTMLNames;

static inline String queryString(WebLocalizedString::Name name) {
  return Locale::defaultLocale().queryString(name);
}

AccessibilityMediaControl::AccessibilityMediaControl(
    LayoutObject* layoutObject,
    AXObjectCacheImpl& axObjectCache)
    : AXLayoutObject(layoutObject, axObjectCache) {}

AXObject* AccessibilityMediaControl::create(LayoutObject* layoutObject,
                                            AXObjectCacheImpl& axObjectCache) {
  ASSERT(layoutObject->node());

  switch (mediaControlElementType(layoutObject->node())) {
    case MediaSlider:
      return AccessibilityMediaTimeline::create(layoutObject, axObjectCache);

    case MediaCurrentTimeDisplay:
    case MediaTimeRemainingDisplay:
      return AccessibilityMediaTimeDisplay::create(layoutObject, axObjectCache);

    case MediaControlsPanel:
      return AXMediaControlsContainer::create(layoutObject, axObjectCache);

    case MediaEnterFullscreenButton:
    case MediaMuteButton:
    case MediaPlayButton:
    case MediaSliderThumb:
    case MediaShowClosedCaptionsButton:
    case MediaHideClosedCaptionsButton:
    case MediaTextTrackList:
    case MediaUnMuteButton:
    case MediaPauseButton:
    case MediaTimelineContainer:
    case MediaTrackSelectionCheckmark:
    case MediaVolumeSliderContainer:
    case MediaVolumeSlider:
    case MediaVolumeSliderThumb:
    case MediaFullscreenVolumeSlider:
    case MediaFullscreenVolumeSliderThumb:
    case MediaExitFullscreenButton:
    case MediaOverlayPlayButton:
    case MediaCastOffButton:
    case MediaCastOnButton:
    case MediaOverlayCastOffButton:
    case MediaOverlayCastOnButton:
    case MediaOverflowButton:
    case MediaOverflowList:
    case MediaDownloadButton:
      return new AccessibilityMediaControl(layoutObject, axObjectCache);
  }

  NOTREACHED();
  return new AccessibilityMediaControl(layoutObject, axObjectCache);
}

MediaControlElementType AccessibilityMediaControl::controlType() const {
  if (!getLayoutObject() || !getLayoutObject()->node())
    return MediaTimelineContainer;  // Timeline container is not accessible.

  return mediaControlElementType(getLayoutObject()->node());
}

String AccessibilityMediaControl::textAlternative(
    bool recursive,
    bool inAriaLabelledByTraversal,
    AXObjectSet& visited,
    AXNameFrom& nameFrom,
    AXRelatedObjectVector* relatedObjects,
    NameSources* nameSources) const {
  switch (controlType()) {
    case MediaEnterFullscreenButton:
      return queryString(WebLocalizedString::AXMediaEnterFullscreenButton);
    case MediaExitFullscreenButton:
      return queryString(WebLocalizedString::AXMediaExitFullscreenButton);
    case MediaMuteButton:
      return queryString(WebLocalizedString::AXMediaMuteButton);
    case MediaPlayButton:
    case MediaOverlayPlayButton:
      return queryString(WebLocalizedString::AXMediaPlayButton);
    case MediaUnMuteButton:
      return queryString(WebLocalizedString::AXMediaUnMuteButton);
    case MediaPauseButton:
      return queryString(WebLocalizedString::AXMediaPauseButton);
    case MediaCurrentTimeDisplay:
      return queryString(WebLocalizedString::AXMediaCurrentTimeDisplay);
    case MediaTimeRemainingDisplay:
      return queryString(WebLocalizedString::AXMediaTimeRemainingDisplay);
    case MediaShowClosedCaptionsButton:
      return queryString(WebLocalizedString::AXMediaShowClosedCaptionsButton);
    case MediaHideClosedCaptionsButton:
      return queryString(WebLocalizedString::AXMediaHideClosedCaptionsButton);
    case MediaCastOffButton:
    case MediaOverlayCastOffButton:
      return queryString(WebLocalizedString::AXMediaCastOffButton);
    case MediaCastOnButton:
    case MediaOverlayCastOnButton:
      return queryString(WebLocalizedString::AXMediaCastOnButton);
    case MediaDownloadButton:
      return queryString(WebLocalizedString::AXMediaDownloadButton);
    case MediaOverflowButton:
      return queryString(WebLocalizedString::AXMediaOverflowButton);
    case MediaSliderThumb:
    case MediaTextTrackList:
    case MediaTimelineContainer:
    case MediaTrackSelectionCheckmark:
    case MediaControlsPanel:
    case MediaVolumeSliderContainer:
    case MediaVolumeSlider:
    case MediaVolumeSliderThumb:
    case MediaFullscreenVolumeSlider:
    case MediaFullscreenVolumeSliderThumb:
    case MediaOverflowList:
      return queryString(WebLocalizedString::AXMediaDefault);
    case MediaSlider:
      NOTREACHED();
      return queryString(WebLocalizedString::AXMediaDefault);
  }

  NOTREACHED();
  return queryString(WebLocalizedString::AXMediaDefault);
}

String AccessibilityMediaControl::description(
    AXNameFrom nameFrom,
    AXDescriptionFrom& descriptionFrom,
    AXObjectVector* descriptionObjects) const {
  switch (controlType()) {
    case MediaEnterFullscreenButton:
      return queryString(WebLocalizedString::AXMediaEnterFullscreenButtonHelp);
    case MediaExitFullscreenButton:
      return queryString(WebLocalizedString::AXMediaExitFullscreenButtonHelp);
    case MediaMuteButton:
      return queryString(WebLocalizedString::AXMediaMuteButtonHelp);
    case MediaPlayButton:
    case MediaOverlayPlayButton:
      return queryString(WebLocalizedString::AXMediaPlayButtonHelp);
    case MediaUnMuteButton:
      return queryString(WebLocalizedString::AXMediaUnMuteButtonHelp);
    case MediaPauseButton:
      return queryString(WebLocalizedString::AXMediaPauseButtonHelp);
    case MediaCurrentTimeDisplay:
      return queryString(WebLocalizedString::AXMediaCurrentTimeDisplayHelp);
    case MediaTimeRemainingDisplay:
      return queryString(WebLocalizedString::AXMediaTimeRemainingDisplayHelp);
    case MediaShowClosedCaptionsButton:
      return queryString(
          WebLocalizedString::AXMediaShowClosedCaptionsButtonHelp);
    case MediaHideClosedCaptionsButton:
      return queryString(
          WebLocalizedString::AXMediaHideClosedCaptionsButtonHelp);
    case MediaCastOffButton:
    case MediaOverlayCastOffButton:
      return queryString(WebLocalizedString::AXMediaCastOffButtonHelp);
    case MediaCastOnButton:
    case MediaOverlayCastOnButton:
      return queryString(WebLocalizedString::AXMediaCastOnButtonHelp);
    case MediaOverflowButton:
      return queryString(WebLocalizedString::AXMediaOverflowButtonHelp);
    case MediaSliderThumb:
    case MediaTextTrackList:
    case MediaTimelineContainer:
    case MediaTrackSelectionCheckmark:
    case MediaControlsPanel:
    case MediaVolumeSliderContainer:
    case MediaVolumeSlider:
    case MediaVolumeSliderThumb:
    case MediaFullscreenVolumeSlider:
    case MediaFullscreenVolumeSliderThumb:
    case MediaOverflowList:
    case MediaDownloadButton:
      return queryString(WebLocalizedString::AXMediaDefault);
    case MediaSlider:
      NOTREACHED();
      return queryString(WebLocalizedString::AXMediaDefault);
  }

  NOTREACHED();
  return queryString(WebLocalizedString::AXMediaDefault);
}

bool AccessibilityMediaControl::computeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
  if (!m_layoutObject || !m_layoutObject->style() ||
      m_layoutObject->style()->visibility() != EVisibility::kVisible ||
      controlType() == MediaTimelineContainer)
    return true;

  return accessibilityIsIgnoredByDefault(ignoredReasons);
}

AccessibilityRole AccessibilityMediaControl::roleValue() const {
  switch (controlType()) {
    case MediaEnterFullscreenButton:
    case MediaExitFullscreenButton:
    case MediaMuteButton:
    case MediaPlayButton:
    case MediaUnMuteButton:
    case MediaPauseButton:
    case MediaShowClosedCaptionsButton:
    case MediaHideClosedCaptionsButton:
    case MediaOverlayPlayButton:
    case MediaOverlayCastOffButton:
    case MediaOverlayCastOnButton:
    case MediaOverflowButton:
    case MediaDownloadButton:
    case MediaCastOnButton:
    case MediaCastOffButton:
      return ButtonRole;

    case MediaTimelineContainer:
    case MediaVolumeSliderContainer:
    case MediaTextTrackList:
    case MediaOverflowList:
      return GroupRole;

    case MediaControlsPanel:
    case MediaCurrentTimeDisplay:
    case MediaTimeRemainingDisplay:
    case MediaSliderThumb:
    case MediaTrackSelectionCheckmark:
    case MediaVolumeSlider:
    case MediaVolumeSliderThumb:
    case MediaFullscreenVolumeSlider:
    case MediaFullscreenVolumeSliderThumb:
      return UnknownRole;

    case MediaSlider:
      // Not using AccessibilityMediaControl.
      NOTREACHED();
      return UnknownRole;
  }

  NOTREACHED();
  return UnknownRole;
}

//
// AXMediaControlsContainer

AXMediaControlsContainer::AXMediaControlsContainer(
    LayoutObject* layoutObject,
    AXObjectCacheImpl& axObjectCache)
    : AccessibilityMediaControl(layoutObject, axObjectCache) {}

AXObject* AXMediaControlsContainer::create(LayoutObject* layoutObject,
                                           AXObjectCacheImpl& axObjectCache) {
  return new AXMediaControlsContainer(layoutObject, axObjectCache);
}

String AXMediaControlsContainer::textAlternative(
    bool recursive,
    bool inAriaLabelledByTraversal,
    AXObjectSet& visited,
    AXNameFrom& nameFrom,
    AXRelatedObjectVector* relatedObjects,
    NameSources* nameSources) const {
  return queryString(isControllingVideoElement()
                         ? WebLocalizedString::AXMediaVideoElement
                         : WebLocalizedString::AXMediaAudioElement);
}

String AXMediaControlsContainer::description(
    AXNameFrom nameFrom,
    AXDescriptionFrom& descriptionFrom,
    AXObjectVector* descriptionObjects) const {
  return queryString(isControllingVideoElement()
                         ? WebLocalizedString::AXMediaVideoElementHelp
                         : WebLocalizedString::AXMediaAudioElementHelp);
}

bool AXMediaControlsContainer::computeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
  return accessibilityIsIgnoredByDefault(ignoredReasons);
}

//
// AccessibilityMediaTimeline

static String localizedMediaTimeDescription(float /*time*/) {
  // FIXME: To be fixed. See
  // http://trac.webkit.org/browser/trunk/Source/WebCore/platform/LocalizedStrings.cpp#L928
  return String();
}

AccessibilityMediaTimeline::AccessibilityMediaTimeline(
    LayoutObject* layoutObject,
    AXObjectCacheImpl& axObjectCache)
    : AXSlider(layoutObject, axObjectCache) {}

AXObject* AccessibilityMediaTimeline::create(LayoutObject* layoutObject,
                                             AXObjectCacheImpl& axObjectCache) {
  return new AccessibilityMediaTimeline(layoutObject, axObjectCache);
}

String AccessibilityMediaTimeline::valueDescription() const {
  Node* node = m_layoutObject->node();
  if (!isHTMLInputElement(node))
    return String();

  return localizedMediaTimeDescription(
      toHTMLInputElement(node)->value().toFloat());
}

String AccessibilityMediaTimeline::description(
    AXNameFrom nameFrom,
    AXDescriptionFrom& descriptionFrom,
    AXObjectVector* descriptionObjects) const {
  return queryString(isControllingVideoElement()
                         ? WebLocalizedString::AXMediaVideoSliderHelp
                         : WebLocalizedString::AXMediaAudioSliderHelp);
}

//
// AccessibilityMediaTimeDisplay

AccessibilityMediaTimeDisplay::AccessibilityMediaTimeDisplay(
    LayoutObject* layoutObject,
    AXObjectCacheImpl& axObjectCache)
    : AccessibilityMediaControl(layoutObject, axObjectCache) {}

AXObject* AccessibilityMediaTimeDisplay::create(
    LayoutObject* layoutObject,
    AXObjectCacheImpl& axObjectCache) {
  return new AccessibilityMediaTimeDisplay(layoutObject, axObjectCache);
}

bool AccessibilityMediaTimeDisplay::computeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
  if (!m_layoutObject || !m_layoutObject->style() ||
      m_layoutObject->style()->visibility() != EVisibility::kVisible)
    return true;

  if (!m_layoutObject->style()->width().value())
    return true;

  return accessibilityIsIgnoredByDefault(ignoredReasons);
}

String AccessibilityMediaTimeDisplay::textAlternative(
    bool recursive,
    bool inAriaLabelledByTraversal,
    AXObjectSet& visited,
    AXNameFrom& nameFrom,
    AXRelatedObjectVector* relatedObjects,
    NameSources* nameSources) const {
  if (controlType() == MediaCurrentTimeDisplay)
    return queryString(WebLocalizedString::AXMediaCurrentTimeDisplay);
  return queryString(WebLocalizedString::AXMediaTimeRemainingDisplay);
}

String AccessibilityMediaTimeDisplay::stringValue() const {
  if (!m_layoutObject || !m_layoutObject->node())
    return String();

  MediaControlTimeDisplayElement* element =
      static_cast<MediaControlTimeDisplayElement*>(m_layoutObject->node());
  float time = element->currentValue();
  return localizedMediaTimeDescription(fabsf(time));
}

}  // namespace blink
