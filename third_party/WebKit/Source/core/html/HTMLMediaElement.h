/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights
 * reserved.
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

#ifndef HTMLMediaElement_h
#define HTMLMediaElement_h

#include <memory>
#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/CoreExport.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/SuspendableObject.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLMediaElementControlsList.h"
#include "core/html/track/TextTrack.h"
#include "platform/Supplementable.h"
#include "platform/WebTaskRunner.h"
#include "platform/audio/AudioSourceProvider.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "public/platform/WebAudioSourceProviderClient.h"
#include "public/platform/WebMediaPlayerClient.h"

namespace blink {

class AudioSourceProviderClient;
class AudioTrack;
class AudioTrackList;
class AutoplayUmaHelper;
class ContentType;
class CueTimeline;
class ElementVisibilityObserver;
class EnumerationHistogram;
class Event;
class ExceptionState;
class HTMLSourceElement;
class HTMLTrackElement;
class KURL;
class MediaControls;
class MediaError;
class MediaStreamDescriptor;
class HTMLMediaSource;
class ScriptState;
class TextTrackContainer;
class TextTrackList;
class TimeRanges;
class URLRegistry;
class VideoTrack;
class VideoTrackList;
class WebAudioSourceProvider;
class WebInbandTextTrack;
class WebLayer;
class WebRemotePlaybackClient;

class CORE_EXPORT HTMLMediaElement
    : public HTMLElement,
      public Supplementable<HTMLMediaElement>,
      public ActiveScriptWrappable<HTMLMediaElement>,
      public SuspendableObject,
      private WebMediaPlayerClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElement);
  USING_PRE_FINALIZER(HTMLMediaElement, dispose);

 public:
  static MIMETypeRegistry::SupportsType supportsType(const ContentType&);

  enum class RecordMetricsBehavior { DoNotRecord, DoRecord };

  static void setMediaStreamRegistry(URLRegistry*);
  static bool isMediaStreamURL(const String& url);
  static bool isHLSURL(const KURL&);

  // If HTMLMediaElement is using MediaTracks (either placeholder or provided
  // by the page).
  static bool mediaTracksEnabledInternally();

  // Notify the HTMLMediaElement that the media controls settings have changed
  // for the given document.
  static void onMediaControlsEnabledChange(Document*);

  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  void clearWeakMembers(Visitor*);
  WebMediaPlayer* webMediaPlayer() const { return m_webMediaPlayer.get(); }

  // Returns true if the loaded media has a video track.
  // Note that even an audio element can have video track in cases such as
  // <audio src="video.webm">, in which case this function will return true.
  bool hasVideo() const;
  // Returns true if loaded media has an audio track.
  bool hasAudio() const;

  bool supportsSave() const;

  WebLayer* platformLayer() const;

  enum DelayedActionType {
    LoadMediaResource = 1 << 0,
    LoadTextTrackResource = 1 << 1
  };
  void scheduleTextTrackResourceLoad();

  bool hasRemoteRoutes() const;
  bool isPlayingRemotely() const { return m_playingRemotely; }

  // error state
  MediaError* error() const;

  // network state
  void setSrc(const AtomicString&);
  const KURL& currentSrc() const { return m_currentSrc; }
  void setSrcObject(MediaStreamDescriptor*);
  MediaStreamDescriptor* getSrcObject() const { return m_srcObject.get(); }

  enum NetworkState {
    kNetworkEmpty,
    kNetworkIdle,
    kNetworkLoading,
    kNetworkNoSource
  };
  NetworkState getNetworkState() const;

  String preload() const;
  void setPreload(const AtomicString&);
  WebMediaPlayer::Preload preloadType() const;
  String effectivePreload() const;
  WebMediaPlayer::Preload effectivePreloadType() const;

  TimeRanges* buffered() const;
  void load();
  String canPlayType(const String& mimeType) const;

  // ready state
  enum ReadyState {
    kHaveNothing,
    kHaveMetadata,
    kHaveCurrentData,
    kHaveFutureData,
    kHaveEnoughData
  };
  ReadyState getReadyState() const;
  bool seeking() const;

  // playback state
  double currentTime() const;
  void setCurrentTime(double);
  double duration() const;
  bool paused() const;
  double defaultPlaybackRate() const;
  void setDefaultPlaybackRate(double);
  double playbackRate() const;
  void setPlaybackRate(double);
  void updatePlaybackRate();
  TimeRanges* played();
  TimeRanges* seekable() const;
  bool ended() const;
  bool autoplay() const;
  bool shouldAutoplay();
  bool loop() const;
  void setLoop(bool);
  ScriptPromise playForBindings(ScriptState*);
  Nullable<ExceptionCode> play();
  void pause();
  void requestRemotePlayback();
  void requestRemotePlaybackControl();
  void requestRemotePlaybackStop();

  // statistics
  unsigned webkitAudioDecodedByteCount() const;
  unsigned webkitVideoDecodedByteCount() const;

  // media source extensions
  void closeMediaSource();
  void durationChanged(double duration, bool requestSeek);

  // controls
  bool shouldShowControls(
      const RecordMetricsBehavior = RecordMetricsBehavior::DoNotRecord) const;
  HTMLMediaElementControlsList* controlsList() const;
  void controlsListValueWasSet();
  double volume() const;
  void setVolume(double, ExceptionState& = ASSERT_NO_EXCEPTION);
  bool muted() const;
  void setMuted(bool);

  void togglePlayState();

  AudioTrackList& audioTracks();
  void audioTrackChanged(AudioTrack*);

  VideoTrackList& videoTracks();
  void selectedVideoTrackChanged(VideoTrack*);

  TextTrack* addTextTrack(const AtomicString& kind,
                          const AtomicString& label,
                          const AtomicString& language,
                          ExceptionState&);

  TextTrackList* textTracks();
  CueTimeline& cueTimeline();

  void addTextTrack(TextTrack*);
  void removeTextTrack(TextTrack*);
  void textTracksChanged();
  void notifyMediaPlayerOfTextTrackChanges();

  // Implements the "forget the media element's media-resource-specific tracks"
  // algorithm in the HTML5 spec.
  void forgetResourceSpecificTracks();

  void didAddTrackElement(HTMLTrackElement*);
  void didRemoveTrackElement(HTMLTrackElement*);

  void honorUserPreferencesForAutomaticTextTrackSelection();

  bool textTracksAreReady() const;
  void configureTextTrackDisplay();
  void updateTextTrackDisplay();
  double lastSeekTime() const { return m_lastSeekTime; }
  void textTrackReadyStateChanged(TextTrack*);

  void textTrackModeChanged(TextTrack*);
  void disableAutomaticTextTrackSelection();

  // EventTarget function.
  // Both Node (via HTMLElement) and SuspendableObject define this method, which
  // causes an ambiguity error at compile time. This class's constructor
  // ensures that both implementations return document, so return the result
  // of one of them here.
  using HTMLElement::getExecutionContext;

  bool hasSingleSecurityOrigin() const {
    return webMediaPlayer() && webMediaPlayer()->hasSingleSecurityOrigin();
  }

  bool isFullscreen() const;
  void didEnterFullscreen();
  void didExitFullscreen();
  virtual bool usesOverlayFullscreenVideo() const { return false; }

  bool hasClosedCaptions() const;
  bool textTracksVisible() const;

  static void setTextTrackKindUserPreferenceForAllMediaElements(Document*);
  void automaticTrackSelectionForUpdatedUserPreference();

  // Returns the MediaControls, or null if they have not been added yet.
  // Note that this can be non-null even if there is no controls attribute.
  MediaControls* mediaControls() const;

  // Notifies the media element that the media controls became visible, so
  // that text track layout may be updated to avoid overlapping them.
  void mediaControlsDidBecomeVisible();

  void sourceWasRemoved(HTMLSourceElement*);
  void sourceWasAdded(HTMLSourceElement*);

  // ScriptWrappable functions.
  bool hasPendingActivity() const final;

  AudioSourceProviderClient* audioSourceNode() { return m_audioSourceNode; }
  void setAudioSourceNode(AudioSourceProviderClient*);

  AudioSourceProvider& getAudioSourceProvider() {
    return m_audioSourceProvider;
  }

  enum InvalidURLAction { DoNothing, Complain };
  bool isSafeToLoadURL(const KURL&, InvalidURLAction);

  // Checks to see if current media data is CORS-same-origin as the
  // specified origin.
  bool isMediaDataCORSSameOrigin(SecurityOrigin*) const;

  // Returns this media element is in a cross-origin frame.
  bool isInCrossOriginFrame() const;

  void scheduleEvent(Event*);
  void scheduleTimeupdateEvent(bool periodicEvent);

  // Returns the "effective media volume" value as specified in the HTML5 spec.
  double effectiveMediaVolume() const;

  // Predicates also used when dispatching wrapper creation (cf.
  // [SpecialWrapFor] IDL attribute usage.)
  virtual bool isHTMLAudioElement() const { return false; }
  virtual bool isHTMLVideoElement() const { return false; }

  void videoWillBeDrawnToCanvas() const;

  WebRemotePlaybackClient* remotePlaybackClient() {
    return m_remotePlaybackClient;
  }
  const WebRemotePlaybackClient* remotePlaybackClient() const {
    return m_remotePlaybackClient;
  }

 protected:
  HTMLMediaElement(const QualifiedName&, Document&);
  ~HTMLMediaElement() override;
  void dispose();

  void parseAttribute(const AttributeModificationParams&) override;
  void finishParsingChildren() final;
  bool isURLAttribute(const Attribute&) const override;
  void attachLayoutTree(const AttachContext& = AttachContext()) override;

  InsertionNotificationRequest insertedInto(ContainerNode*) override;
  void removedFrom(ContainerNode*) override;

  void didMoveToNewDocument(Document& oldDocument) override;
  virtual KURL posterImageURL() const { return KURL(); }

  enum DisplayMode { Unknown, Poster, Video };
  DisplayMode getDisplayMode() const { return m_displayMode; }
  virtual void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }

 private:
  // Friend class for testing.
  friend class MediaElementFillingViewportTest;

  void resetMediaPlayerAndMediaSource();

  bool alwaysCreateUserAgentShadowRoot() const final { return true; }
  bool areAuthorShadowsAllowed() const final { return false; }

  bool supportsFocus() const final;
  bool isMouseFocusable() const final;
  bool layoutObjectIsNeeded(const ComputedStyle&) override;
  LayoutObject* createLayoutObject(const ComputedStyle&) override;
  void didNotifySubtreeInsertionsToDocument() override;
  void didRecalcStyle() final;

  bool canStartSelection() const override { return false; }

  bool isInteractiveContent() const final;

  // SuspendableObject functions.
  void contextDestroyed(ExecutionContext*) override;

  virtual void updateDisplayState() {}

  void setReadyState(ReadyState);
  void setNetworkState(WebMediaPlayer::NetworkState);

  // WebMediaPlayerClient implementation.
  void networkStateChanged() final;
  void readyStateChanged() final;
  void timeChanged() final;
  void repaint() final;
  void durationChanged() final;
  void sizeChanged() final;
  void playbackStateChanged() final;

  void setWebLayer(WebLayer*) final;
  WebMediaPlayer::TrackId addAudioTrack(const WebString&,
                                        WebMediaPlayerClient::AudioTrackKind,
                                        const WebString&,
                                        const WebString&,
                                        bool) final;
  void removeAudioTrack(WebMediaPlayer::TrackId) final;
  WebMediaPlayer::TrackId addVideoTrack(const WebString&,
                                        WebMediaPlayerClient::VideoTrackKind,
                                        const WebString&,
                                        const WebString&,
                                        bool) final;
  void removeVideoTrack(WebMediaPlayer::TrackId) final;
  void addTextTrack(WebInbandTextTrack*) final;
  void removeTextTrack(WebInbandTextTrack*) final;
  void mediaSourceOpened(WebMediaSource*) final;
  void requestSeek(double) final;
  void remoteRouteAvailabilityChanged(WebRemotePlaybackAvailability) final;
  void connectedToRemoteDevice() final;
  void disconnectedFromRemoteDevice() final;
  void cancelledRemotePlaybackRequest() final;
  void remotePlaybackStarted() final;
  void onBecamePersistentVideo(bool) override{};
  bool hasSelectedVideoTrack() final;
  WebMediaPlayer::TrackId getSelectedVideoTrackId() final;
  bool isAutoplayingMuted() final;
  void requestReload(const WebURL&) final;
  void activateViewportIntersectionMonitoring(bool) final;

  void loadTimerFired(TimerBase*);
  void progressEventTimerFired(TimerBase*);
  void playbackProgressTimerFired(TimerBase*);
  void checkViewportIntersectionTimerFired(TimerBase*);
  void startPlaybackProgressTimer();
  void startProgressEventTimer();
  void stopPeriodicTimers();

  void seek(double time);
  void finishSeek();
  void checkIfSeekNeeded();
  void addPlayedRange(double start, double end);

  // FIXME: Rename to scheduleNamedEvent for clarity.
  void scheduleEvent(const AtomicString& eventName);

  // loading
  void invokeLoadAlgorithm();
  void invokeResourceSelectionAlgorithm();
  void loadInternal();
  void selectMediaResource();
  void loadResource(const WebMediaPlayerSource&, const ContentType&);
  void startPlayerLoad(const KURL& playerProvidedUrl = KURL());
  void setPlayerPreload();
  WebMediaPlayer::LoadType loadType() const;
  void scheduleNextSourceChild();
  void loadSourceFromObject();
  void loadSourceFromAttribute();
  void loadNextSourceChild();
  void clearMediaPlayer();
  void clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
  bool havePotentialSourceChild();
  void noneSupported();
  void mediaEngineError(MediaError*);
  void cancelPendingEventsAndCallbacks();
  void waitForSourceChange();
  void setIgnorePreloadNone();

  KURL selectNextSourceChild(ContentType*, InvalidURLAction);

  void mediaLoadingFailed(WebMediaPlayer::NetworkState);

  // deferred loading (preload=none)
  bool loadIsDeferred() const;
  void deferLoad();
  void cancelDeferredLoad();
  void startDeferredLoad();
  void executeDeferredLoad();
  void deferredLoadTimerFired(TimerBase*);

  void markCaptionAndSubtitleTracksAsUnconfigured();

  // This does not check user gesture restrictions.
  void playInternal();

  // This does not change the buffering strategy.
  void pauseInternal();

  void allowVideoRendering();

  void updateVolume();
  void updatePlayState();
  bool potentiallyPlaying() const;
  bool stoppedDueToErrors() const;
  bool couldPlayIfEnoughData() const;

  // Generally the presence of the loop attribute should be considered to mean
  // playback has not "ended", as "ended" and "looping" are mutually exclusive.
  // See
  // https://html.spec.whatwg.org/multipage/embedded-content.html#ended-playback
  enum class LoopCondition { Included, Ignored };
  bool endedPlayback(LoopCondition = LoopCondition::Included) const;

  void setShouldDelayLoadEvent(bool);

  double earliestPossiblePosition() const;
  double currentPlaybackPosition() const;
  double officialPlaybackPosition() const;
  void setOfficialPlaybackPosition(double) const;
  void requireOfficialPlaybackPositionUpdate() const;

  void ensureMediaControls();
  void updateControlsVisibility();

  TextTrackContainer& ensureTextTrackContainer();

  void changeNetworkStateFromLoadingToIdle();

  WebMediaPlayer::CORSMode corsMode() const;

  // Returns the "direction of playback" value as specified in the HTML5 spec.
  enum DirectionOfPlayback { Backward, Forward };
  DirectionOfPlayback getDirectionOfPlayback() const;

  // Creates placeholder AudioTrack and/or VideoTrack objects when
  // WebMemediaPlayer objects advertise they have audio and/or video, but don't
  // explicitly signal them via addAudioTrack() and addVideoTrack().
  // FIXME: Remove this once all WebMediaPlayer implementations properly report
  // their track info.
  void createPlaceholderTracksIfNecessary();

  // Sets the selected/enabled tracks if they aren't set before we initially
  // transition to kHaveMetadata.
  void selectInitialTracksIfNecessary();

  // Return true if and only if a user gesture is required to unlock this
  // media element for unrestricted autoplay / script control.  Don't confuse
  // this with isGestureNeededForPlayback().  The latter is usually what one
  // should use, if checking to see if an action is allowed.
  bool isLockedPendingUserGesture() const;

  bool isLockedPendingUserGestureIfCrossOriginExperimentEnabled() const;

  // If the user gesture is required, then this will remove it.  Note that
  // one should not generally call this method directly; use the one on
  // m_helper and give it a reason.
  void unlockUserGesture();

  // Return true if and only if a user gesture is requried for playback.  Even
  // if isLockedPendingUserGesture() return true, this might return false if
  // the requirement is currently overridden.  This does not check if a user
  // gesture is currently being processed.
  bool isGestureNeededForPlayback() const;

  bool isGestureNeededForPlaybackIfCrossOriginExperimentEnabled() const;

  bool isGestureNeededForPlaybackIfPendingUserGestureIsLocked() const;

  // Return true if and only if the settings allow autoplay of media on this
  // frame.
  bool isAutoplayAllowedPerSettings() const;

  void setNetworkState(NetworkState);

  void audioTracksTimerFired(TimerBase*);

  void scheduleResolvePlayPromises();
  void scheduleRejectPlayPromises(ExceptionCode);
  void scheduleNotifyPlaying();
  void resolveScheduledPlayPromises();
  void rejectScheduledPlayPromises();
  void rejectPlayPromises(ExceptionCode, const String&);
  void rejectPlayPromisesInternal(ExceptionCode, const String&);

  EnumerationHistogram& showControlsHistogram() const;

  void onVisibilityChangedForAutoplay(bool isVisible);

  void viewportFillDebouncerTimerFired(TimerBase*);

  TaskRunnerTimer<HTMLMediaElement> m_loadTimer;
  TaskRunnerTimer<HTMLMediaElement> m_progressEventTimer;
  TaskRunnerTimer<HTMLMediaElement> m_playbackProgressTimer;
  TaskRunnerTimer<HTMLMediaElement> m_audioTracksTimer;
  TaskRunnerTimer<HTMLMediaElement> m_viewportFillDebouncerTimer;
  TaskRunnerTimer<HTMLMediaElement> m_checkViewportIntersectionTimer;

  Member<TimeRanges> m_playedTimeRanges;
  Member<GenericEventQueue> m_asyncEventQueue;

  double m_playbackRate;
  double m_defaultPlaybackRate;
  NetworkState m_networkState;
  ReadyState m_readyState;
  ReadyState m_readyStateMaximum;
  KURL m_currentSrc;
  Member<MediaStreamDescriptor> m_srcObject;

  Member<MediaError> m_error;

  double m_volume;
  double m_lastSeekTime;

  double m_previousProgressTime;

  // Cached duration to suppress duplicate events if duration unchanged.
  double m_duration;

  // The last time a timeupdate event was sent (wall clock).
  double m_lastTimeUpdateEventWallTime;

  // The last time a timeupdate event was sent in movie time.
  double m_lastTimeUpdateEventMediaTime;

  // The default playback start position.
  double m_defaultPlaybackStartPosition;

  // Loading state.
  enum LoadState {
    WaitingForSource,
    LoadingFromSrcObject,
    LoadingFromSrcAttr,
    LoadingFromSourceElement
  };
  LoadState m_loadState;
  Member<HTMLSourceElement> m_currentSourceNode;
  Member<Node> m_nextChildNodeToConsider;

  // "Deferred loading" state (for preload=none).
  enum DeferredLoadState {
    // The load is not deferred.
    NotDeferred,
    // The load is deferred, and waiting for the task to set the
    // delaying-the-load-event flag (to false).
    WaitingForStopDelayingLoadEventTask,
    // The load is the deferred, and waiting for a triggering event.
    WaitingForTrigger,
    // The load is deferred, and waiting for the task to set the
    // delaying-the-load-event flag, after which the load will be executed.
    ExecuteOnStopDelayingLoadEventTask
  };
  DeferredLoadState m_deferredLoadState;
  TaskRunnerTimer<HTMLMediaElement> m_deferredLoadTimer;

  std::unique_ptr<WebMediaPlayer> m_webMediaPlayer;
  WebLayer* m_webLayer;

  DisplayMode m_displayMode;

  Member<HTMLMediaSource> m_mediaSource;

  // Stores "official playback position", updated periodically from "current
  // playback position". Official playback position should not change while
  // scripts are running. See setOfficialPlaybackPosition().
  mutable double m_officialPlaybackPosition;
  mutable bool m_officialPlaybackPositionNeedsUpdate;

  double m_fragmentEndTime;

  typedef unsigned PendingActionFlags;
  PendingActionFlags m_pendingActionFlags;

  // FIXME: HTMLMediaElement has way too many state bits.
  bool m_lockedPendingUserGesture : 1;
  bool m_lockedPendingUserGestureIfCrossOriginExperimentEnabled : 1;
  bool m_playing : 1;
  bool m_shouldDelayLoadEvent : 1;
  bool m_haveFiredLoadedData : 1;
  bool m_canAutoplay : 1;
  bool m_muted : 1;
  bool m_paused : 1;
  bool m_seeking : 1;

  // data has not been loaded since sending a "stalled" event
  bool m_sentStalledEvent : 1;

  bool m_ignorePreloadNone : 1;

  bool m_textTracksVisible : 1;
  bool m_shouldPerformAutomaticTrackSelection : 1;

  bool m_tracksAreReady : 1;
  bool m_processingPreferenceChange : 1;
  bool m_playingRemotely : 1;
  // Whether this element is in overlay fullscreen mode.
  bool m_inOverlayFullscreenVideo : 1;

  bool m_mostlyFillingViewport : 1;

  TraceWrapperMember<AudioTrackList> m_audioTracks;
  TraceWrapperMember<VideoTrackList> m_videoTracks;
  TraceWrapperMember<TextTrackList> m_textTracks;
  HeapVector<Member<TextTrack>> m_textTracksWhenResourceSelectionBegan;

  Member<CueTimeline> m_cueTimeline;

  HeapVector<Member<ScriptPromiseResolver>> m_playPromiseResolvers;
  TaskHandle m_playPromiseResolveTaskHandle;
  TaskHandle m_playPromiseRejectTaskHandle;
  HeapVector<Member<ScriptPromiseResolver>> m_playPromiseResolveList;
  HeapVector<Member<ScriptPromiseResolver>> m_playPromiseRejectList;
  ExceptionCode m_playPromiseErrorCode;

  // This is a weak reference, since m_audioSourceNode holds a reference to us.
  // TODO(Oilpan): Consider making this a strongly traced pointer with oilpan
  // where strong cycles are not a problem.
  GC_PLUGIN_IGNORE("http://crbug.com/404577")
  WeakMember<AudioSourceProviderClient> m_audioSourceNode;

  // AudioClientImpl wraps an AudioSourceProviderClient.
  // When the audio format is known, Chromium calls setFormat().
  class AudioClientImpl final
      : public GarbageCollectedFinalized<AudioClientImpl>,
        public WebAudioSourceProviderClient {
   public:
    explicit AudioClientImpl(AudioSourceProviderClient* client)
        : m_client(client) {}

    ~AudioClientImpl() override {}

    // WebAudioSourceProviderClient
    void setFormat(size_t numberOfChannels, float sampleRate) override;

    DECLARE_TRACE();

   private:
    Member<AudioSourceProviderClient> m_client;
  };

  // AudioSourceProviderImpl wraps a WebAudioSourceProvider.
  // provideInput() calls into Chromium to get a rendered audio stream.
  class AudioSourceProviderImpl final : public AudioSourceProvider {
    DISALLOW_NEW();

   public:
    AudioSourceProviderImpl() : m_webAudioSourceProvider(nullptr) {}

    ~AudioSourceProviderImpl() override {}

    // Wraps the given WebAudioSourceProvider.
    void wrap(WebAudioSourceProvider*);

    // AudioSourceProvider
    void setClient(AudioSourceProviderClient*) override;
    void provideInput(AudioBus*, size_t framesToProcess) override;

    DECLARE_TRACE();

   private:
    WebAudioSourceProvider* m_webAudioSourceProvider;
    Member<AudioClientImpl> m_client;
    Mutex provideInputLock;
  };

  AudioSourceProviderImpl m_audioSourceProvider;

  friend class AutoplayUmaHelper;  // for isAutoplayAllowedPerSettings
  friend class AutoplayUmaHelperTest;
  friend class Internals;
  friend class TrackDisplayUpdateScope;
  friend class MediaControlsTest;
  friend class HTMLMediaElementTest;
  friend class HTMLMediaElementEventListenersTest;
  friend class HTMLVideoElement;
  friend class HTMLVideoElementTest;
  friend class MediaControlsOrientationLockDelegateTest;

  Member<AutoplayUmaHelper> m_autoplayUmaHelper;

  WebRemotePlaybackClient* m_remotePlaybackClient;

  // class AutoplayVisibilityObserver;
  Member<ElementVisibilityObserver> m_autoplayVisibilityObserver;

  IntRect m_currentIntersectRect;

  Member<MediaControls> m_mediaControls;
  Member<HTMLMediaElementControlsList> m_controlsList;

  static URLRegistry* s_mediaStreamRegistry;
};

inline bool isHTMLMediaElement(const HTMLElement& element) {
  return isHTMLAudioElement(element) || isHTMLVideoElement(element);
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLMediaElement);

}  // namespace blink

#endif  // HTMLMediaElement_h
