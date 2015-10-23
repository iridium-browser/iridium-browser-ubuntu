/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "core/CoreExport.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/HTMLElement.h"
#include "core/html/track/TextTrack.h"
#include "platform/Supplementable.h"
#include "public/platform/WebMediaPlayerClient.h"
#include "public/platform/WebMimeRegistry.h"

#if ENABLE(WEB_AUDIO)
#include "platform/audio/AudioSourceProvider.h"
#include "public/platform/WebAudioSourceProviderClient.h"
#endif

namespace blink {

#if ENABLE(WEB_AUDIO)
class AudioSourceProviderClient;
class WebAudioSourceProvider;
#endif
class AudioTrackList;
class ContentType;
class CueTimeline;
class Event;
class ExceptionState;
class HTMLSourceElement;
class HTMLTrackElement;
class KURL;
class MediaController;
class MediaControls;
class MediaError;
class HTMLMediaSource;
class TextTrackContainer;
class TextTrackList;
class TimeRanges;
class URLRegistry;
class VideoTrackList;
class WebInbandTextTrack;
class WebLayer;

class CORE_EXPORT HTMLMediaElement : public HTMLElement, public WillBeHeapSupplementable<HTMLMediaElement>, public ActiveDOMObject, private WebMediaPlayerClient {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElement);
    WILL_BE_USING_PRE_FINALIZER(HTMLMediaElement, dispose);
public:
    static WebMimeRegistry::SupportsType supportsType(const ContentType&, const String& keySystem = String());

    static void setMediaStreamRegistry(URLRegistry*);
    static bool isMediaStreamURL(const String& url);

    DECLARE_VIRTUAL_TRACE();
#if ENABLE(WEB_AUDIO)
    void clearWeakMembers(Visitor*);
#endif
    WebMediaPlayer* webMediaPlayer() const
    {
        return m_webMediaPlayer.get();
    }

    virtual bool hasVideo() const { return false; }
    bool hasAudio() const;

    bool supportsSave() const;

    WebLayer* platformLayer() const;

    enum DelayedActionType {
        LoadMediaResource = 1 << 0,
        LoadTextTrackResource = 1 << 1,
        TextTrackChangesNotification = 1 << 2
    };
    void scheduleDelayedAction(DelayedActionType);

    bool hasRemoteRoutes() const { return m_remoteRoutesAvailable; }
    bool isPlayingRemotely() const { return m_playingRemotely; }

    // error state
    MediaError* error() const;

    // network state
    void setSrc(const AtomicString&);
    const KURL& currentSrc() const { return m_currentSrc; }

    enum NetworkState { NETWORK_EMPTY, NETWORK_IDLE, NETWORK_LOADING, NETWORK_NO_SOURCE };
    NetworkState networkState() const;

    String preload() const;
    void setPreload(const AtomicString&);
    WebMediaPlayer::Preload preloadType() const;
    WebMediaPlayer::Preload effectivePreloadType() const;

    TimeRanges* buffered() const;
    void load();
    String canPlayType(const String& mimeType, const String& keySystem = String()) const;

    // ready state
    enum ReadyState { HAVE_NOTHING, HAVE_METADATA, HAVE_CURRENT_DATA, HAVE_FUTURE_DATA, HAVE_ENOUGH_DATA };
    ReadyState readyState() const;
    bool seeking() const;

    // playback state
    double currentTime() const;
    void setCurrentTime(double, ExceptionState&);
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
    bool loop() const;
    void setLoop(bool);
    void play();
    void pause();
    void requestRemotePlayback();
    void requestRemotePlaybackControl();

    // statistics
    unsigned webkitAudioDecodedByteCount() const;
    unsigned webkitVideoDecodedByteCount() const;

    // media source extensions
    void closeMediaSource();
    void durationChanged(double duration, bool requestSeek);

    // controls
    bool shouldShowControls() const;
    double volume() const;
    void setVolume(double, ExceptionState&);
    bool muted() const;
    void setMuted(bool);

    // play/pause toggling that uses the media controller if present. togglePlayStateWillPlay() is
    // true if togglePlayState() will call play() or unpause() on the media element or controller.
    bool togglePlayStateWillPlay() const;
    void togglePlayState();

    AudioTrackList& audioTracks();
    void audioTrackChanged();

    VideoTrackList& videoTracks();
    void selectedVideoTrackChanged(WebMediaPlayer::TrackId*);

    TextTrack* addTextTrack(const AtomicString& kind, const AtomicString& label, const AtomicString& language, ExceptionState&);

    TextTrackList* textTracks();
    CueTimeline& cueTimeline();

    void addTextTrack(TextTrack*);
    void removeTextTrack(TextTrack*);
    void textTracksChanged();
    void notifyMediaPlayerOfTextTrackChanges();

    // Implements the "forget the media element's media-resource-specific tracks" algorithm in the HTML5 spec.
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

    // EventTarget function.
    // Both Node (via HTMLElement) and ActiveDOMObject define this method, which
    // causes an ambiguity error at compile time. This class's constructor
    // ensures that both implementations return document, so return the result
    // of one of them here.
    using HTMLElement::executionContext;

    bool hasSingleSecurityOrigin() const { return webMediaPlayer() && webMediaPlayer()->hasSingleSecurityOrigin(); }

    bool isFullscreen() const;
    void enterFullscreen();
    void exitFullscreen();
    virtual bool usesOverlayFullscreenVideo() const { return false; }

    bool hasClosedCaptions() const;
    bool closedCaptionsVisible() const;
    void setClosedCaptionsVisible(bool);

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

    // ActiveDOMObject functions.
    bool hasPendingActivity() const final;
    void contextDestroyed() final;

#if ENABLE(WEB_AUDIO)
    AudioSourceProviderClient* audioSourceNode() { return m_audioSourceNode; }
    void setAudioSourceNode(AudioSourceProviderClient*);

    AudioSourceProvider& audioSourceProvider() { return m_audioSourceProvider; }
#endif

    enum InvalidURLAction { DoNothing, Complain };
    bool isSafeToLoadURL(const KURL&, InvalidURLAction);

    // Checks to see if current media data is CORS-same-origin as the
    // specified origin.
    bool isMediaDataCORSSameOrigin(SecurityOrigin*) const;

    MediaController* controller() const;
    void setController(MediaController*); // Resets the MediaGroup and sets the MediaController.

    void scheduleEvent(PassRefPtrWillBeRawPtr<Event>);
    void scheduleTimeupdateEvent(bool periodicEvent);

    // Returns the "effective media volume" value as specified in the HTML5 spec.
    double effectiveMediaVolume() const;

    // Predicates also used when dispatching wrapper creation (cf. [SpecialWrapFor] IDL attribute usage.)
    virtual bool isHTMLAudioElement() const { return false; }
    virtual bool isHTMLVideoElement() const { return false; }

protected:
    HTMLMediaElement(const QualifiedName&, Document&);
    ~HTMLMediaElement() override;
#if ENABLE(OILPAN)
    void dispose();
#endif

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void finishParsingChildren() final;
    bool isURLAttribute(const Attribute&) const override;
    void attach(const AttachContext& = AttachContext()) override;

    void didMoveToNewDocument(Document& oldDocument) override;
    virtual KURL posterImageURL() const { return KURL(); }

    enum DisplayMode { Unknown, Poster, Video };
    DisplayMode displayMode() const { return m_displayMode; }
    virtual void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }

    void setControllerInternal(MediaController*);

private:
    void resetMediaPlayerAndMediaSource();

    bool alwaysCreateUserAgentShadowRoot() const final { return true; }
    bool areAuthorShadowsAllowed() const final { return false; }

    bool supportsFocus() const final;
    bool isMouseFocusable() const final;
    bool layoutObjectIsNeeded(const ComputedStyle&) override;
    LayoutObject* createLayoutObject(const ComputedStyle&) override;
    InsertionNotificationRequest insertedInto(ContainerNode*) final;
    void didNotifySubtreeInsertionsToDocument() override;
    void removedFrom(ContainerNode*) final;
    void didRecalcStyle(StyleRecalcChange) final;

    bool canStartSelection() const override { return false; }

    void didBecomeFullscreenElement() final;
    void willStopBeingFullscreenElement() final;
    bool isInteractiveContent() const final;
    void defaultEventHandler(Event*) final;

    // ActiveDOMObject functions.
    void stop() final;

    virtual void updateDisplayState() { }

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
    WebMediaPlayer::TrackId addAudioTrack(const WebString&, WebMediaPlayerClient::AudioTrackKind, const WebString&, const WebString&, bool) final;
    void removeAudioTrack(WebMediaPlayer::TrackId) final;
    WebMediaPlayer::TrackId addVideoTrack(const WebString&, WebMediaPlayerClient::VideoTrackKind, const WebString&, const WebString&, bool) final;
    void removeVideoTrack(WebMediaPlayer::TrackId) final;
    void addTextTrack(WebInbandTextTrack*) final;
    void removeTextTrack(WebInbandTextTrack*) final;
    void mediaSourceOpened(WebMediaSource*) final;
    void requestSeek(double) final;
    void remoteRouteAvailabilityChanged(bool) final;
    void connectedToRemoteDevice() final;
    void disconnectedFromRemoteDevice() final;

    void loadTimerFired(Timer<HTMLMediaElement>*);
    void progressEventTimerFired(Timer<HTMLMediaElement>*);
    void playbackProgressTimerFired(Timer<HTMLMediaElement>*);
    void startPlaybackProgressTimer();
    void startProgressEventTimer();
    void stopPeriodicTimers();

    void seek(double time);
    void finishSeek();
    void checkIfSeekNeeded();
    void addPlayedRange(double start, double end);

    void scheduleEvent(const AtomicString& eventName); // FIXME: Rename to scheduleNamedEvent for clarity.

    // loading
    void prepareForLoad();
    void loadInternal();
    void selectMediaResource();
    void loadResource(const KURL&, ContentType&, const String& keySystem);
    void startPlayerLoad();
    void setPlayerPreload();
    WebMediaPlayer::LoadType loadType() const;
    void scheduleNextSourceChild();
    void loadNextSourceChild();
    void userCancelledLoad();
    void clearMediaPlayer(int flags);
    void clearMediaPlayerAndAudioSourceProviderClientWithoutLocking();
    bool havePotentialSourceChild();
    void noneSupported();
    void mediaEngineError(MediaError*);
    void cancelPendingEventsAndCallbacks();
    void waitForSourceChange();
    void prepareToPlay();

    KURL selectNextSourceChild(ContentType*, String* keySystem, InvalidURLAction);

    void mediaLoadingFailed(WebMediaPlayer::NetworkState);

    // deferred loading (preload=none)
    bool loadIsDeferred() const;
    void deferLoad();
    void cancelDeferredLoad();
    void startDeferredLoad();
    void executeDeferredLoad();
    void deferredLoadTimerFired(Timer<HTMLMediaElement>*);

    void markCaptionAndSubtitleTracksAsUnconfigured();

    // This does not check user gesture restrictions.
    void playInternal();

    void gesturelessInitialPlayHalted();
    void autoplayMediaEncountered();
    void allowVideoRendering();

    void updateVolume();
    void updatePlayState();
    bool potentiallyPlaying() const;
    bool stoppedDueToErrors() const;
    bool couldPlayIfEnoughData() const;

    // Generally the presence of the loop attribute should be considered to mean playback
    // has not "ended", as "ended" and "looping" are mutually exclusive. See
    // https://html.spec.whatwg.org/multipage/embedded-content.html#ended-playback
    enum class LoopCondition { Included, Ignored };
    bool endedPlayback(LoopCondition = LoopCondition::Included) const;

    void setShouldDelayLoadEvent(bool);
    void invalidateCachedTime();
    void refreshCachedTime() const;

    void ensureMediaControls();
    void configureMediaControls();

    TextTrackContainer& ensureTextTrackContainer();

    void* preDispatchEventHandler(Event*) final;

    void changeNetworkStateFromLoadingToIdle();

    const AtomicString& mediaGroup() const;
    void setMediaGroup(const AtomicString&);
    void updateMediaController();
    bool isBlocked() const;
    bool isBlockedOnMediaController() const;
    bool isAutoplaying() const { return m_autoplaying; }

    void setAllowHiddenVolumeControls(bool);

    WebMediaPlayer::CORSMode corsMode() const;

    // Returns the "direction of playback" value as specified in the HTML5 spec.
    enum DirectionOfPlayback { Backward, Forward };
    DirectionOfPlayback directionOfPlayback() const;

    // Returns the "effective playback rate" value as specified in the HTML5 spec.
    double effectivePlaybackRate() const;

    // Creates placeholder AudioTrack and/or VideoTrack objects when WebMemediaPlayer objects
    // advertise they have audio and/or video, but don't explicitly signal them via
    // addAudioTrack() and addVideoTrack().
    // FIXME: Remove this once all WebMediaPlayer implementations properly report their track info.
    void createPlaceholderTracksIfNecessary();

    // Sets the selected/enabled tracks if they aren't set before we initially
    // transition to HAVE_METADATA.
    void selectInitialTracksIfNecessary();

    void audioTracksTimerFired(Timer<HTMLMediaElement>*);

    Timer<HTMLMediaElement> m_loadTimer;
    Timer<HTMLMediaElement> m_progressEventTimer;
    Timer<HTMLMediaElement> m_playbackProgressTimer;
    Timer<HTMLMediaElement> m_audioTracksTimer;
    PersistentWillBeMember<TimeRanges> m_playedTimeRanges;
    OwnPtrWillBeMember<GenericEventQueue> m_asyncEventQueue;

    double m_playbackRate;
    double m_defaultPlaybackRate;
    NetworkState m_networkState;
    ReadyState m_readyState;
    ReadyState m_readyStateMaximum;
    KURL m_currentSrc;

    PersistentWillBeMember<MediaError> m_error;

    double m_volume;
    double m_lastSeekTime;

    double m_previousProgressTime;

    // Cached duration to suppress duplicate events if duration unchanged.
    double m_duration;

    // The last time a timeupdate event was sent (wall clock).
    double m_lastTimeUpdateEventWallTime;

    // The last time a timeupdate event was sent in movie time.
    double m_lastTimeUpdateEventMovieTime;

    // The default playback start position.
    double m_defaultPlaybackStartPosition;

    // Loading state.
    enum LoadState { WaitingForSource, LoadingFromSrcAttr, LoadingFromSourceElement };
    LoadState m_loadState;
    RefPtrWillBeMember<HTMLSourceElement> m_currentSourceNode;
    RefPtrWillBeMember<Node> m_nextChildNodeToConsider;

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
    Timer<HTMLMediaElement> m_deferredLoadTimer;

    OwnPtr<WebMediaPlayer> m_webMediaPlayer;
    WebLayer* m_webLayer;

    DisplayMode m_displayMode;

    RefPtrWillBeMember<HTMLMediaSource> m_mediaSource;

    // Cached time value. Only valid when ready state is HAVE_METADATA or
    // higher, otherwise the current time is assumed to be zero.
    mutable double m_cachedTime;

    double m_fragmentEndTime;

    typedef unsigned PendingActionFlags;
    PendingActionFlags m_pendingActionFlags;

    // FIXME: MediaElement has way too many state bits.
    bool m_userGestureRequiredForPlay : 1;
    bool m_playing : 1;
    bool m_shouldDelayLoadEvent : 1;
    bool m_haveFiredLoadedData : 1;
    bool m_autoplaying : 1;
    bool m_muted : 1;
    bool m_paused : 1;
    bool m_seeking : 1;

    // data has not been loaded since sending a "stalled" event
    bool m_sentStalledEvent : 1;

    // time has not changed since sending an "ended" event
    bool m_sentEndEvent : 1;

    bool m_closedCaptionsVisible : 1;

    bool m_completelyLoaded : 1;
    bool m_havePreparedToPlay : 1;
    bool m_delayingLoadForPreloadNone : 1;

    bool m_tracksAreReady : 1;
    bool m_haveVisibleTextTrack : 1;
    bool m_processingPreferenceChange : 1;
    bool m_remoteRoutesAvailable : 1;
    bool m_playingRemotely : 1;
    bool m_isFinalizing : 1;
    bool m_initialPlayWithoutUserGestures : 1;
    bool m_autoplayMediaCounted : 1;
    // Whether this element is in overlay fullscreen mode.
    bool m_inOverlayFullscreenVideo : 1;

    PersistentWillBeMember<AudioTrackList> m_audioTracks;
    PersistentWillBeMember<VideoTrackList> m_videoTracks;
    PersistentWillBeMember<TextTrackList> m_textTracks;
    PersistentHeapVectorWillBeHeapVector<Member<TextTrack>> m_textTracksWhenResourceSelectionBegan;

    OwnPtrWillBeMember<CueTimeline> m_cueTimeline;

#if ENABLE(WEB_AUDIO)
    // This is a weak reference, since m_audioSourceNode holds a reference to us.
    // FIXME: Oilpan: Consider making this a strongly traced pointer with oilpan where strong cycles are not a problem.
    GC_PLUGIN_IGNORE("http://crbug.com/404577")
    RawPtrWillBeWeakMember<AudioSourceProviderClient> m_audioSourceNode;

    // AudioClientImpl wraps an AudioSourceProviderClient.
    // When the audio format is known, Chromium calls setFormat().
    class AudioClientImpl final : public GarbageCollectedFinalized<AudioClientImpl>, public WebAudioSourceProviderClient {
    public:
        explicit AudioClientImpl(AudioSourceProviderClient* client)
            : m_client(client)
        {
        }

        ~AudioClientImpl() override { }

        // WebAudioSourceProviderClient
        void setFormat(size_t numberOfChannels, float sampleRate) override;

        DECLARE_TRACE();

    private:
        Member<AudioSourceProviderClient> m_client;
    };

    // AudioSourceProviderImpl wraps a WebAudioSourceProvider.
    // provideInput() calls into Chromium to get a rendered audio stream.
    class AudioSourceProviderImpl final : public AudioSourceProvider {
        DISALLOW_ALLOCATION();
    public:
        AudioSourceProviderImpl()
            : m_webAudioSourceProvider(nullptr)
        {
        }

        ~AudioSourceProviderImpl() override { }

        // Wraps the given WebAudioSourceProvider.
        void wrap(WebAudioSourceProvider*);

        // AudioSourceProvider
        void setClient(AudioSourceProviderClient*) override;
        void provideInput(AudioBus*, size_t framesToProcess) override;

        DECLARE_TRACE();

    private:
        WebAudioSourceProvider* m_webAudioSourceProvider;
        PersistentWillBeMember<AudioClientImpl> m_client;
        Mutex provideInputLock;
    };

    AudioSourceProviderImpl m_audioSourceProvider;
#endif

    friend class MediaController;
    PersistentWillBeMember<MediaController> m_mediaController;

    friend class Internals;
    friend class TrackDisplayUpdateScope;

    static URLRegistry* s_mediaStreamRegistry;
};

inline bool isHTMLMediaElement(const HTMLElement& element)
{
    return isHTMLAudioElement(element) || isHTMLVideoElement(element);
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLMediaElement);

} // namespace blink

#endif // HTMLMediaElement_h
