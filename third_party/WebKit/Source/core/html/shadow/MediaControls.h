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

#ifndef MediaControls_h
#define MediaControls_h

#include "core/html/HTMLDivElement.h"
#include "core/html/shadow/MediaControlElements.h"

namespace blink {

class Event;
class MediaControlsMediaEventListener;
class MediaControlsOrientationLockDelegate;
class MediaControlsWindowEventListener;
class ShadowRoot;

class CORE_EXPORT MediaControls final : public HTMLDivElement {
 public:
  static MediaControls* create(HTMLMediaElement&, ShadowRoot&);

  HTMLMediaElement& mediaElement() const { return *m_mediaElement; }

  void reset();
  void onControlsListUpdated();

  void show();
  void hide();
  bool isVisible() const;

  void beginScrubbing();
  void endScrubbing();

  void updateCurrentTimeDisplay();

  void toggleTextTrackList();
  void showTextTrackAtIndex(unsigned indexToEnable);
  void disableShowingTextTracks();

  // Called by the fullscreen buttons to toggle fulllscreen on/off.
  void enterFullscreen();
  void exitFullscreen();

  void showOverlayCastButtonIfNeeded();
  // Update cast button visibility, but don't try to update our panel
  // button visibility for space.
  void refreshCastButtonVisibilityWithoutUpdate();

  void setAllowHiddenVolumeControls(bool);

  // Returns the layout object for the part of the controls that should be
  // used for overlap checking during text track layout. May be null.
  LayoutObject* layoutObjectForTextTrackLayout();

  // Return the internal elements, which is used by registering clicking
  // EventHandlers from MediaControlsWindowEventListener.
  MediaControlPanelElement* panelElement() { return m_panel; }
  MediaControlTimelineElement* timelineElement() { return m_timeline; }
  MediaControlCastButtonElement* castButtonElement() { return m_castButton; }
  MediaControlVolumeSliderElement* volumeSliderElement() {
    return m_volumeSlider;
  }

  // Notify us that our controls enclosure has changed width.
  void notifyPanelWidthChanged(const LayoutUnit& newWidth);

  // Notify us that the media element's network state has changed.
  void networkStateChanged();

  void toggleOverflowMenu();

  bool overflowMenuVisible();

  // TODO(mlamouri): this is temporary to notify the controls that an
  // HTMLTrackElement failed to load because there is no web exposed way to
  // be notified on the TextTrack object. See https://crbug.com/669977
  void onTrackElementFailedToLoad() { onTextTracksAddedOrRemoved(); }

  // TODO(mlamouri): the following methods will be able to become private when
  // the controls have moved to modules/ and have access to RemotePlayback.
  void onRemotePlaybackAvailabilityChanged() { refreshCastButtonVisibility(); }
  void onRemotePlaybackConnecting() { startedCasting(); }
  void onRemotePlaybackDisconnected() { stoppedCasting(); }

  // TODO(mlamouri): this method is needed in order to notify the controls that
  // the attribute have changed.
  void onDisableRemotePlaybackAttributeChanged() {
    refreshCastButtonVisibility();
  }

  // TODO(mlamouri): this method is needed in order to notify the controls that
  // the `mediaControlsEnabled` setting has changed.
  void onMediaControlsEnabledChange() {
    // There is no update because only the overlay is expected to change.
    refreshCastButtonVisibilityWithoutUpdate();
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class MediaControlsMediaEventListener;
  friend class MediaControlsOrientationLockDelegateTest;
  friend class MediaControlsTest;

  void invalidate(Element*);

  class BatchedControlUpdate;

  explicit MediaControls(HTMLMediaElement&);

  void initializeControls();

  void makeOpaque();
  void makeTransparent();

  void updatePlayState();

  enum HideBehaviorFlags {
    IgnoreNone = 0,
    IgnoreVideoHover = 1 << 0,
    IgnoreFocus = 1 << 1,
    IgnoreControlsHover = 1 << 2,
    IgnoreWaitForTimer = 1 << 3,
  };

  bool shouldHideMediaControls(unsigned behaviorFlags = 0) const;
  void hideMediaControlsTimerFired(TimerBase*);
  void startHideMediaControlsTimer();
  void stopHideMediaControlsTimer();
  void resetHideMediaControlsTimer();

  void panelWidthChangedTimerFired(TimerBase*);

  void hideAllMenus();

  // Hide elements that don't fit, and show those things that we want which
  // do fit.  This requires that m_panelWidth is current.
  void computeWhichControlsFit();

  // Node
  bool isMediaControls() const override { return true; }
  bool willRespondToMouseMoveEvents() override { return true; }
  void defaultEventHandler(Event*) override;
  bool containsRelatedTarget(Event*);

  // Methods called by MediaControlsMediaEventListener.
  void onInsertedIntoDocument();
  void onRemovedFromDocument();
  void onVolumeChange();
  void onFocusIn();
  void onTimeUpdate();
  void onDurationChange();
  void onPlay();
  void onPause();
  void onTextTracksAddedOrRemoved();
  void onTextTracksChanged();
  void onError();
  void onLoadedMetadata();
  void onEnteredFullscreen();
  void onExitedFullscreen();

  // Internal cast related methods.
  void startedCasting();
  void stoppedCasting();
  void refreshCastButtonVisibility();

  Member<HTMLMediaElement> m_mediaElement;

  // Media control elements.
  Member<MediaControlOverlayEnclosureElement> m_overlayEnclosure;
  Member<MediaControlOverlayPlayButtonElement> m_overlayPlayButton;
  Member<MediaControlCastButtonElement> m_overlayCastButton;
  Member<MediaControlPanelEnclosureElement> m_enclosure;
  Member<MediaControlPanelElement> m_panel;
  Member<MediaControlPlayButtonElement> m_playButton;
  Member<MediaControlTimelineElement> m_timeline;
  Member<MediaControlCurrentTimeDisplayElement> m_currentTimeDisplay;
  Member<MediaControlTimeRemainingDisplayElement> m_durationDisplay;
  Member<MediaControlMuteButtonElement> m_muteButton;
  Member<MediaControlVolumeSliderElement> m_volumeSlider;
  Member<MediaControlToggleClosedCaptionsButtonElement>
      m_toggleClosedCaptionsButton;
  Member<MediaControlTextTrackListElement> m_textTrackList;
  Member<MediaControlOverflowMenuButtonElement> m_overflowMenu;
  Member<MediaControlOverflowMenuListElement> m_overflowList;

  Member<MediaControlCastButtonElement> m_castButton;
  Member<MediaControlFullscreenButtonElement> m_fullscreenButton;
  Member<MediaControlDownloadButtonElement> m_downloadButton;

  Member<MediaControlsMediaEventListener> m_mediaEventListener;
  Member<MediaControlsWindowEventListener> m_windowEventListener;
  Member<MediaControlsOrientationLockDelegate> m_orientationLockDelegate;

  TaskRunnerTimer<MediaControls> m_hideMediaControlsTimer;
  unsigned m_hideTimerBehaviorFlags;
  bool m_isMouseOverControls : 1;
  bool m_isPausedForScrubbing : 1;

  TaskRunnerTimer<MediaControls> m_panelWidthChangedTimer;
  int m_panelWidth;

  bool m_keepShowingUntilTimerFires : 1;
};

DEFINE_ELEMENT_TYPE_CASTS(MediaControls, isMediaControls());

}  // namespace blink

#endif
