/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/shadow/MediaControls.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ClientRect.h"
#include "core/dom/Fullscreen.h"
#include "core/events/MouseEvent.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/MediaController.h"
#include "core/html/track/TextTrackContainer.h"
#include "core/layout/LayoutTheme.h"

namespace blink {

// If you change this value, then also update the corresponding value in
// LayoutTests/media/media-controls.js.
static const double timeWithoutMouseMovementBeforeHidingMediaControls = 3;

static bool shouldShowFullscreenButton(const HTMLMediaElement& mediaElement)
{
    // Unconditionally allow the user to exit fullscreen if we are in it
    // now.  Especially on android, when we might not yet know if
    // fullscreen is supported, we sometimes guess incorrectly and show
    // the button earlier, and we don't want to remove it here if the
    // user chose to enter fullscreen.  crbug.com/500732 .
    if (mediaElement.isFullscreen())
        return true;

    if (!mediaElement.hasVideo())
        return false;

    if (!Fullscreen::fullscreenEnabled(mediaElement.document()))
        return false;

    return true;
}

static bool preferHiddenVolumeControls(const Document& document)
{
    return !document.settings() || document.settings()->preferHiddenVolumeControls();
}

class MediaControls::BatchedControlUpdate {
    WTF_MAKE_NONCOPYABLE(BatchedControlUpdate);
    STACK_ALLOCATED();
public:
    explicit BatchedControlUpdate(MediaControls* controls)
        : m_controls(controls)
    {
        ASSERT(isMainThread());
        ASSERT(s_batchDepth >= 0);
        ++s_batchDepth;
    }
    ~BatchedControlUpdate()
    {
        ASSERT(isMainThread());
        ASSERT(s_batchDepth > 0);
        if (!(--s_batchDepth))
            m_controls->computeWhichControlsFit();
    }

private:
    RawPtrWillBeMember<MediaControls> m_controls;
    static int s_batchDepth;
};

// Count of number open batches for controls visibility.
int MediaControls::BatchedControlUpdate::s_batchDepth = 0;

MediaControls::MediaControls(HTMLMediaElement& mediaElement)
    : HTMLDivElement(mediaElement.document())
    , m_mediaElement(&mediaElement)
    , m_overlayEnclosure(nullptr)
    , m_overlayPlayButton(nullptr)
    , m_overlayCastButton(nullptr)
    , m_enclosure(nullptr)
    , m_panel(nullptr)
    , m_playButton(nullptr)
    , m_timeline(nullptr)
    , m_currentTimeDisplay(nullptr)
    , m_durationDisplay(nullptr)
    , m_muteButton(nullptr)
    , m_volumeSlider(nullptr)
    , m_toggleClosedCaptionsButton(nullptr)
    , m_castButton(nullptr)
    , m_fullScreenButton(nullptr)
    , m_hideMediaControlsTimer(this, &MediaControls::hideMediaControlsTimerFired)
    , m_hideTimerBehaviorFlags(IgnoreNone)
    , m_isMouseOverControls(false)
    , m_isPausedForScrubbing(false)
    , m_panelWidthChangedTimer(this, &MediaControls::panelWidthChangedTimerFired)
    , m_panelWidth(0)
    , m_allowHiddenVolumeControls(RuntimeEnabledFeatures::newMediaPlaybackUiEnabled())
{
}

PassRefPtrWillBeRawPtr<MediaControls> MediaControls::create(HTMLMediaElement& mediaElement)
{
    RefPtrWillBeRawPtr<MediaControls> controls = adoptRefWillBeNoop(new MediaControls(mediaElement));
    controls->setShadowPseudoId(AtomicString("-webkit-media-controls", AtomicString::ConstructFromLiteral));
    controls->initializeControls();
    return controls.release();
}

// The media controls DOM structure looks like:
//
// MediaControls                                       (-webkit-media-controls)
// +-MediaControlOverlayEnclosureElement               (-webkit-media-controls-overlay-enclosure)
// | +-MediaControlOverlayPlayButtonElement            (-webkit-media-controls-overlay-play-button)
// | | {if mediaControlsOverlayPlayButtonEnabled}
// | \-MediaControlCastButtonElement                   (-internal-media-controls-overlay-cast-button)
// \-MediaControlPanelEnclosureElement                 (-webkit-media-controls-enclosure)
//   \-MediaControlPanelElement                        (-webkit-media-controls-panel)
//     +-MediaControlPlayButtonElement                 (-webkit-media-controls-play-button)
//     | {if !RTE::newMediaPlaybackUi()}
//     +-MediaControlTimelineElement                   (-webkit-media-controls-timeline)
//     +-MediaControlCurrentTimeDisplayElement         (-webkit-media-controls-current-time-display)
//     +-MediaControlTimeRemainingDisplayElement       (-webkit-media-controls-time-remaining-display)
//     | {if RTE::newMediaPlaybackUi()}
//     +-MediaControlTimelineElement                   (-webkit-media-controls-timeline)
//     +-MediaControlMuteButtonElement                 (-webkit-media-controls-mute-button)
//     +-MediaControlVolumeSliderElement               (-webkit-media-controls-volume-slider)
//     +-MediaControlToggleClosedCaptionsButtonElement (-webkit-media-controls-toggle-closed-captions-button)
//     +-MediaControlCastButtonElement                 (-internal-media-controls-cast-button)
//     \-MediaControlFullscreenButtonElement           (-webkit-media-controls-fullscreen-button)
void MediaControls::initializeControls()
{
    const bool useNewUi = RuntimeEnabledFeatures::newMediaPlaybackUiEnabled();
    RefPtrWillBeRawPtr<MediaControlOverlayEnclosureElement> overlayEnclosure = MediaControlOverlayEnclosureElement::create(*this);

    if (document().settings() && document().settings()->mediaControlsOverlayPlayButtonEnabled()) {
        RefPtrWillBeRawPtr<MediaControlOverlayPlayButtonElement> overlayPlayButton = MediaControlOverlayPlayButtonElement::create(*this);
        m_overlayPlayButton = overlayPlayButton.get();
        overlayEnclosure->appendChild(overlayPlayButton.release());
    }

    RefPtrWillBeRawPtr<MediaControlCastButtonElement> overlayCastButton = MediaControlCastButtonElement::create(*this, true);
    m_overlayCastButton = overlayCastButton.get();
    overlayEnclosure->appendChild(overlayCastButton.release());

    m_overlayEnclosure = overlayEnclosure.get();
    appendChild(overlayEnclosure.release());

    // Create an enclosing element for the panel so we can visually offset the controls correctly.
    RefPtrWillBeRawPtr<MediaControlPanelEnclosureElement> enclosure = MediaControlPanelEnclosureElement::create(*this);

    RefPtrWillBeRawPtr<MediaControlPanelElement> panel = MediaControlPanelElement::create(*this);

    RefPtrWillBeRawPtr<MediaControlPlayButtonElement> playButton = MediaControlPlayButtonElement::create(*this);
    m_playButton = playButton.get();
    panel->appendChild(playButton.release());

    RefPtrWillBeRawPtr<MediaControlTimelineElement> timeline = MediaControlTimelineElement::create(*this);
    m_timeline = timeline.get();
    // In old UX, timeline is before the time / duration text.
    if (!useNewUi)
        panel->appendChild(timeline.release());
    // else we will attach it later.

    RefPtrWillBeRawPtr<MediaControlCurrentTimeDisplayElement> currentTimeDisplay = MediaControlCurrentTimeDisplayElement::create(*this);
    m_currentTimeDisplay = currentTimeDisplay.get();
    m_currentTimeDisplay->setIsWanted(useNewUi);
    panel->appendChild(currentTimeDisplay.release());

    RefPtrWillBeRawPtr<MediaControlTimeRemainingDisplayElement> durationDisplay = MediaControlTimeRemainingDisplayElement::create(*this);
    m_durationDisplay = durationDisplay.get();
    panel->appendChild(durationDisplay.release());

    // Timeline is after the time / duration text if newMediaPlaybackUiEnabled.
    if (useNewUi)
        panel->appendChild(timeline.release());

    RefPtrWillBeRawPtr<MediaControlMuteButtonElement> muteButton = MediaControlMuteButtonElement::create(*this);
    m_muteButton = muteButton.get();
    panel->appendChild(muteButton.release());

    RefPtrWillBeRawPtr<MediaControlVolumeSliderElement> slider = MediaControlVolumeSliderElement::create(*this);
    m_volumeSlider = slider.get();
    panel->appendChild(slider.release());
    if (m_allowHiddenVolumeControls && preferHiddenVolumeControls(document()))
        m_volumeSlider->setIsWanted(false);

    RefPtrWillBeRawPtr<MediaControlToggleClosedCaptionsButtonElement> toggleClosedCaptionsButton = MediaControlToggleClosedCaptionsButtonElement::create(*this);
    m_toggleClosedCaptionsButton = toggleClosedCaptionsButton.get();
    panel->appendChild(toggleClosedCaptionsButton.release());

    RefPtrWillBeRawPtr<MediaControlCastButtonElement> castButton = MediaControlCastButtonElement::create(*this, false);
    m_castButton = castButton.get();
    panel->appendChild(castButton.release());

    RefPtrWillBeRawPtr<MediaControlFullscreenButtonElement> fullscreenButton = MediaControlFullscreenButtonElement::create(*this);
    m_fullScreenButton = fullscreenButton.get();
    panel->appendChild(fullscreenButton.release());

    m_panel = panel.get();
    enclosure->appendChild(panel.release());

    m_enclosure = enclosure.get();
    appendChild(enclosure.release());
}

void MediaControls::reset()
{
    const bool useNewUi = RuntimeEnabledFeatures::newMediaPlaybackUiEnabled();
    BatchedControlUpdate batch(this);

    m_allowHiddenVolumeControls = useNewUi;

    const double duration = mediaElement().duration();
    m_durationDisplay->setInnerText(LayoutTheme::theme().formatMediaControlsTime(duration), ASSERT_NO_EXCEPTION);
    m_durationDisplay->setCurrentValue(duration);

    if (useNewUi) {
        // Show everything that we might hide.
        // If we don't have a duration, then mark it to be hidden.  For the
        // old UI case, want / don't want is the same as show / hide since
        // it is never marked as not fitting.
        m_durationDisplay->setIsWanted(std::isfinite(duration));
        m_currentTimeDisplay->setIsWanted(true);
        m_timeline->setIsWanted(true);
    }

    updatePlayState();

    updateCurrentTimeDisplay();

    m_timeline->setDuration(duration);
    m_timeline->setPosition(mediaElement().currentTime());

    updateVolume();

    refreshClosedCaptionsButtonVisibility();

    m_fullScreenButton->setIsWanted(shouldShowFullscreenButton(mediaElement()));

    refreshCastButtonVisibilityWithoutUpdate();
    makeOpaque();

    // Set the panel width here, and force a layout, before the controls update.
    // This would be harmless for the !useNewUi case too, but it causes
    // compositing/geometry/video-fixed-scrolling.html to fail with two extra
    // 0 height nodes in the render tree.
    if (useNewUi)
        m_panelWidth = m_panel->clientWidth();
}

LayoutObject* MediaControls::layoutObjectForTextTrackLayout()
{
    return m_panel->layoutObject();
}

void MediaControls::show()
{
    makeOpaque();
    m_panel->setIsWanted(true);
    m_panel->setIsDisplayed(true);
    if (m_overlayPlayButton)
        m_overlayPlayButton->updateDisplayType();
}

void MediaControls::mediaElementFocused()
{
    if (mediaElement().shouldShowControls()) {
        show();
        resetHideMediaControlsTimer();
    }
}

void MediaControls::hide()
{
    m_panel->setIsWanted(false);
    m_panel->setIsDisplayed(false);
    if (m_overlayPlayButton)
        m_overlayPlayButton->setIsWanted(false);
}

void MediaControls::makeOpaque()
{
    m_panel->makeOpaque();
}

void MediaControls::makeTransparent()
{
    m_panel->makeTransparent();
}

bool MediaControls::shouldHideMediaControls(unsigned behaviorFlags) const
{
    // Never hide for a media element without visual representation.
    if (!mediaElement().hasVideo() || mediaElement().isPlayingRemotely())
        return false;
    // Don't hide if the mouse is over the controls.
    const bool ignoreControlsHover = behaviorFlags & IgnoreControlsHover;
    if (!ignoreControlsHover && m_panel->hovered())
        return false;
    // Don't hide if the mouse is over the video area.
    const bool ignoreVideoHover = behaviorFlags & IgnoreVideoHover;
    if (!ignoreVideoHover && m_isMouseOverControls)
        return false;
    // Don't hide if focus is on the HTMLMediaElement or within the
    // controls/shadow tree. (Perform the checks separately to avoid going
    // through all the potential ancestor hosts for the focused element.)
    const bool ignoreFocus = behaviorFlags & IgnoreFocus;
    if (!ignoreFocus && (mediaElement().focused() || contains(document().focusedElement())))
        return false;
    return true;
}

void MediaControls::playbackStarted()
{
    BatchedControlUpdate batch(this);

    if (!RuntimeEnabledFeatures::newMediaPlaybackUiEnabled()) {
        m_currentTimeDisplay->setIsWanted(true);
        m_durationDisplay->setIsWanted(false);
    }

    updatePlayState();
    m_timeline->setPosition(mediaElement().currentTime());
    updateCurrentTimeDisplay();

    startHideMediaControlsTimer();
}

void MediaControls::playbackProgressed()
{
    m_timeline->setPosition(mediaElement().currentTime());
    updateCurrentTimeDisplay();

    if (shouldHideMediaControls())
        makeTransparent();
}

void MediaControls::playbackStopped()
{
    updatePlayState();
    m_timeline->setPosition(mediaElement().currentTime());
    updateCurrentTimeDisplay();
    makeOpaque();

    stopHideMediaControlsTimer();
}

void MediaControls::updatePlayState()
{
    if (m_isPausedForScrubbing)
        return;

    if (m_overlayPlayButton)
        m_overlayPlayButton->updateDisplayType();
    m_playButton->updateDisplayType();
}

void MediaControls::beginScrubbing()
{
    if (!mediaElement().togglePlayStateWillPlay()) {
        m_isPausedForScrubbing = true;
        mediaElement().togglePlayState();
    }
}

void MediaControls::endScrubbing()
{
    if (m_isPausedForScrubbing) {
        m_isPausedForScrubbing = false;
        if (mediaElement().togglePlayStateWillPlay())
            mediaElement().togglePlayState();
    }
}

void MediaControls::updateCurrentTimeDisplay()
{
    double now = mediaElement().currentTime();
    double duration = mediaElement().duration();

    // After seek, hide duration display and show current time.
    if (!RuntimeEnabledFeatures::newMediaPlaybackUiEnabled() && now > 0) {
        BatchedControlUpdate batch(this);
        m_currentTimeDisplay->setIsWanted(true);
        m_durationDisplay->setIsWanted(false);
    }

    // Allow the theme to format the time.
    m_currentTimeDisplay->setInnerText(LayoutTheme::theme().formatMediaControlsCurrentTime(now, duration), IGNORE_EXCEPTION);
    m_currentTimeDisplay->setCurrentValue(now);
}

void MediaControls::updateVolume()
{
    m_muteButton->updateDisplayType();
    // Invalidate the mute button because it paints differently according to volume.
    if (LayoutObject* layoutObject = m_muteButton->layoutObject())
        layoutObject->setShouldDoFullPaintInvalidation();

    if (mediaElement().muted())
        m_volumeSlider->setVolume(0);
    else
        m_volumeSlider->setVolume(mediaElement().volume());

    // Update the visibility of our audio elements.
    // We never want the volume slider if there's no audio.
    // If there is audio, then we want it unless hiding audio is enabled and
    // we prefer to hide it.
    BatchedControlUpdate batch(this);
    m_volumeSlider->setIsWanted(mediaElement().hasAudio()
        && !(m_allowHiddenVolumeControls && preferHiddenVolumeControls(document())));

    // The mute button is a little more complicated.  If enableNewMediaPlaybackUi
    // is true, then we choose to hide or show the mute button to save space.
    // If enableNew* is not set, then we never touch the mute button, and
    // instead leave it to the CSS.
    // Note that this is why m_allowHiddenVolumeControls isn't rolled into prefer...().
    if (m_allowHiddenVolumeControls) {
        // If there is no audio track, then hide the mute button.
        m_muteButton->setIsWanted(mediaElement().hasAudio());
    }

    // Invalidate the volume slider because it paints differently according to volume.
    if (LayoutObject* layoutObject = m_volumeSlider->layoutObject())
        layoutObject->setShouldDoFullPaintInvalidation();
}

void MediaControls::changedClosedCaptionsVisibility()
{
    m_toggleClosedCaptionsButton->updateDisplayType();
}

void MediaControls::refreshClosedCaptionsButtonVisibility()
{
    m_toggleClosedCaptionsButton->setIsWanted(mediaElement().hasClosedCaptions());
    BatchedControlUpdate batch(this);
}

static Element* elementFromCenter(Element& element)
{
    ClientRect* clientRect = element.getBoundingClientRect();
    int centerX = static_cast<int>((clientRect->left() + clientRect->right()) / 2);
    int centerY = static_cast<int>((clientRect->top() + clientRect->bottom()) / 2);

    return element.document().elementFromPoint(centerX , centerY);
}

void MediaControls::tryShowOverlayCastButton()
{
    // The element needs to be shown to have its dimensions and position.
    m_overlayCastButton->setIsWanted(true);
    if (elementFromCenter(*m_overlayCastButton) != &mediaElement())
        m_overlayCastButton->setIsWanted(false);
}

void MediaControls::refreshCastButtonVisibility()
{
    refreshCastButtonVisibilityWithoutUpdate();
    BatchedControlUpdate batch(this);
}

void MediaControls::refreshCastButtonVisibilityWithoutUpdate()
{
    if (mediaElement().hasRemoteRoutes()) {
        // The reason for the autoplay test is that some pages (e.g. vimeo.com) have an autoplay background video, which
        // doesn't autoplay on Chrome for Android (we prevent it) so starts paused. In such cases we don't want to automatically
        // show the cast button, since it looks strange and is unlikely to correspond with anything the user wants to do.
        // If a user does want to cast a paused autoplay video then they can still do so by touching or clicking on the
        // video, which will cause the cast button to appear.
        if (!mediaElement().shouldShowControls() && !mediaElement().autoplay() && mediaElement().paused()) {
            // Note that this is a case where we add the overlay cast button
            // without wanting the panel cast button.  We depend on the fact
            // that computeWhichControlsFit() won't change overlay cast button
            // visibility in the case where the cast button isn't wanted.
            // We don't call compute...() here, but it will be called as
            // non-cast changes (e.g., resize) occur.  If the panel button
            // is shown, however, compute...() will take control of the
            // overlay cast button if it needs to hide it from the panel.
            tryShowOverlayCastButton();
            m_castButton->setIsWanted(false);
        } else if (mediaElement().shouldShowControls()) {
            m_overlayCastButton->setIsWanted(false);
            m_castButton->setIsWanted(true);
            // Check that the cast button actually fits on the bar.  For the
            // newMediaPlaybackUiEnabled case, we let computeWhichControlsFit()
            // handle this.
            if ( !RuntimeEnabledFeatures::newMediaPlaybackUiEnabled()
                && m_fullScreenButton->getBoundingClientRect()->right() > m_panel->getBoundingClientRect()->right()) {
                m_castButton->setIsWanted(false);
                tryShowOverlayCastButton();
            }
        }
    } else {
        m_castButton->setIsWanted(false);
        m_overlayCastButton->setIsWanted(false);
    }
}

void MediaControls::showOverlayCastButton()
{
    tryShowOverlayCastButton();
    resetHideMediaControlsTimer();
}

void MediaControls::enteredFullscreen()
{
    m_fullScreenButton->setIsFullscreen(true);
    stopHideMediaControlsTimer();
    startHideMediaControlsTimer();
}

void MediaControls::exitedFullscreen()
{
    m_fullScreenButton->setIsFullscreen(false);
    stopHideMediaControlsTimer();
    startHideMediaControlsTimer();
}

void MediaControls::startedCasting()
{
    m_castButton->setIsPlayingRemotely(true);
    m_overlayCastButton->setIsPlayingRemotely(true);
}

void MediaControls::stoppedCasting()
{
    m_castButton->setIsPlayingRemotely(false);
    m_overlayCastButton->setIsPlayingRemotely(false);
}

void MediaControls::defaultEventHandler(Event* event)
{
    HTMLDivElement::defaultEventHandler(event);

    // Add IgnoreControlsHover to m_hideTimerBehaviorFlags when we see a touch event,
    // to allow the hide-timer to do the right thing when it fires.
    // FIXME: Preferably we would only do this when we're actually handling the event
    // here ourselves.
    bool wasLastEventTouch = event->isTouchEvent() || event->isGestureEvent()
        || (event->isMouseEvent() && toMouseEvent(event)->fromTouch());
    m_hideTimerBehaviorFlags |= wasLastEventTouch ? IgnoreControlsHover : IgnoreNone;

    if (event->type() == EventTypeNames::mouseover) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = true;
            if (!mediaElement().togglePlayStateWillPlay()) {
                makeOpaque();
                if (shouldHideMediaControls())
                    startHideMediaControlsTimer();
            }
        }
        return;
    }

    if (event->type() == EventTypeNames::mouseout) {
        if (!containsRelatedTarget(event)) {
            m_isMouseOverControls = false;
            stopHideMediaControlsTimer();
        }
        return;
    }

    if (event->type() == EventTypeNames::mousemove) {
        // When we get a mouse move, show the media controls, and start a timer
        // that will hide the media controls after a 3 seconds without a mouse move.
        makeOpaque();
        refreshCastButtonVisibility();
        if (shouldHideMediaControls(IgnoreVideoHover))
            startHideMediaControlsTimer();
        return;
    }
}

void MediaControls::hideMediaControlsTimerFired(Timer<MediaControls>*)
{
    unsigned behaviorFlags = m_hideTimerBehaviorFlags | IgnoreFocus | IgnoreVideoHover;
    m_hideTimerBehaviorFlags = IgnoreNone;

    if (mediaElement().togglePlayStateWillPlay())
        return;

    if (!shouldHideMediaControls(behaviorFlags))
        return;

    makeTransparent();
    m_overlayCastButton->setIsWanted(false);
}

void MediaControls::startHideMediaControlsTimer()
{
    m_hideMediaControlsTimer.startOneShot(timeWithoutMouseMovementBeforeHidingMediaControls, FROM_HERE);
}

void MediaControls::stopHideMediaControlsTimer()
{
    m_hideMediaControlsTimer.stop();
}

void MediaControls::resetHideMediaControlsTimer()
{
    stopHideMediaControlsTimer();
    if (!mediaElement().paused())
        startHideMediaControlsTimer();
}


bool MediaControls::containsRelatedTarget(Event* event)
{
    if (!event->isMouseEvent())
        return false;
    EventTarget* relatedTarget = toMouseEvent(event)->relatedTarget();
    if (!relatedTarget)
        return false;
    return contains(relatedTarget->toNode());
}

void MediaControls::notifyPanelWidthChanged(const LayoutUnit& newWidth)
{
    // Don't bother to do any work if this matches the most recent panel
    // width, since we're called after layout.
    // Note that this code permits a bad frame on resize, since it is
    // run after the relayout / paint happens.  It would be great to improve
    // this, but it would be even greater to move this code entirely to
    // JS and fix it there.
    const int panelWidth = newWidth.toInt();

    if (!RuntimeEnabledFeatures::newMediaPlaybackUiEnabled())
        return;

    m_panelWidth = panelWidth;

    // Adjust for effective zoom.
    if (!m_panel->layoutObject() || !m_panel->layoutObject()->style())
        return;
    m_panelWidth = ceil(m_panelWidth / m_panel->layoutObject()->style()->effectiveZoom());

    m_panelWidthChangedTimer.startOneShot(0, FROM_HERE);
}

void MediaControls::panelWidthChangedTimerFired(Timer<MediaControls>*)
{
    computeWhichControlsFit();
}

void MediaControls::computeWhichControlsFit()
{
    // Hide all controls that don't fit, and show the ones that do.
    // This might be better suited for a layout, but since JS media controls
    // won't benefit from that anwyay, we just do it here like JS will.

    if (!RuntimeEnabledFeatures::newMediaPlaybackUiEnabled())
        return;

    if (!m_panelWidth)
        return;

    // Controls that we'll hide / show, in order of decreasing priority.
    MediaControlElement* elements[] = {
        m_playButton.get(),
        m_toggleClosedCaptionsButton.get(),
        m_fullScreenButton.get(),
        m_timeline.get(),
        m_currentTimeDisplay.get(),
        m_volumeSlider.get(),
        m_castButton.get(),
        m_muteButton.get(),
        m_durationDisplay.get(),
    };

    int usedWidth = 0;
    bool droppedCastButton = false;
    // Assume that all controls require 48px.  Ideally, we could get this
    // the computed style, but that requires the controls to be shown.
    const int minimumWidth = 48;
    for (MediaControlElement* element : elements) {
        if (!element)
            continue;

        if (element->isWanted()) {
            if (usedWidth + minimumWidth <= m_panelWidth) {
                element->setDoesFit(true);
                usedWidth += minimumWidth;
            } else {
                element->setDoesFit(false);
                if (element == m_castButton.get())
                    droppedCastButton = true;
            }
        }
    }

    // Special case for cast: if we want a cast button but dropped it, then
    // show the overlay cast button instead.
    if (m_castButton->isWanted())
        m_overlayCastButton->setIsWanted(droppedCastButton);
}

void MediaControls::setAllowHiddenVolumeControls(bool allow)
{
    m_allowHiddenVolumeControls = allow;
    // Update the controls visibility.
    updateVolume();
}

DEFINE_TRACE(MediaControls)
{
    visitor->trace(m_mediaElement);
    visitor->trace(m_panel);
    visitor->trace(m_overlayPlayButton);
    visitor->trace(m_overlayEnclosure);
    visitor->trace(m_playButton);
    visitor->trace(m_currentTimeDisplay);
    visitor->trace(m_timeline);
    visitor->trace(m_muteButton);
    visitor->trace(m_volumeSlider);
    visitor->trace(m_toggleClosedCaptionsButton);
    visitor->trace(m_fullScreenButton);
    visitor->trace(m_durationDisplay);
    visitor->trace(m_enclosure);
    visitor->trace(m_castButton);
    visitor->trace(m_overlayCastButton);
    HTMLDivElement::trace(visitor);
}

}
