// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!HTMLElement} playerContainer Main container.
 * @param {!HTMLElement} videoContainer Container for the video element.
 * @param {!HTMLElement} controlsContainer Container for video controls.
 * @constructor
 * @struct
 * @extends {VideoControls}
 */
function FullWindowVideoControls(
    playerContainer, videoContainer, controlsContainer) {
  VideoControls.call(this,
      controlsContainer,
      this.onPlaybackError_.wrap(this),
      loadTimeData.getString.wrap(loadTimeData),
      this.toggleFullScreen_.wrap(this),
      videoContainer);

  this.playerContainer_ = playerContainer;
  this.decodeErrorOccured = false;

  this.casting = false;

  this.updateStyle();
  window.addEventListener('resize', this.updateStyle.wrap(this));
  document.addEventListener('keydown', function(e) {
    switch (util.getKeyModifiers(e) + e.keyIdentifier) {
      // Handle debug shortcut keys.
      case 'Ctrl-Shift-U+0049': // Ctrl+Shift+I
        chrome.fileManagerPrivate.openInspector('normal');
        break;
      case 'Ctrl-Shift-U+004A': // Ctrl+Shift+J
        chrome.fileManagerPrivate.openInspector('console');
        break;
      case 'Ctrl-Shift-U+0043': // Ctrl+Shift+C
        chrome.fileManagerPrivate.openInspector('element');
        break;
      case 'Ctrl-Shift-U+0042': // Ctrl+Shift+B
        chrome.fileManagerPrivate.openInspector('background');
        break;

      case 'U+0020': // Space
      case 'MediaPlayPause':
        this.togglePlayStateWithFeedback();
        break;
      case 'U+001B': // Escape
        util.toggleFullScreen(
            chrome.app.window.current(),
            false);  // Leave the full screen mode.
        break;
      case 'Right':
      case 'MediaNextTrack':
        player.advance_(1);
        break;
      case 'Left':
      case 'MediaPreviousTrack':
        player.advance_(0);
        break;
      case 'MediaStop':
        // TODO: Define "Stop" behavior.
        break;
    }
  }.wrap(this));

  // TODO(mtomasz): Simplify. crbug.com/254318.
  var clickInProgress = false;
  videoContainer.addEventListener('click', function(e) {
    if (clickInProgress)
      return;

    clickInProgress = true;
    var togglePlayState = function() {
      clickInProgress = false;

      if (e.ctrlKey) {
        this.toggleLoopedModeWithFeedback(true);
        if (!this.isPlaying())
          this.togglePlayStateWithFeedback();
      } else {
        this.togglePlayStateWithFeedback();
      }
    }.wrap(this);

    if (!this.media_)
      player.reloadCurrentVideo(togglePlayState);
    else
      setTimeout(togglePlayState, 0);
  }.wrap(this));

  /**
   * @type {MouseInactivityWatcher}
   * @private
   */
  this.inactivityWatcher_ = new MouseInactivityWatcher(playerContainer);
  this.inactivityWatcher_.check();
}

FullWindowVideoControls.prototype = { __proto__: VideoControls.prototype };

/**
 * Gets inactivity watcher.
 * @return {MouseInactivityWatcher} An inactivity watcher.
 */
FullWindowVideoControls.prototype.getInactivityWatcher = function() {
  return this.inactivityWatcher_;
};

/**
 * Displays error message.
 *
 * @param {string} message Message id.
 */
FullWindowVideoControls.prototype.showErrorMessage = function(message) {
  var errorBanner = queryRequiredElement(document, '#error');
  errorBanner.textContent = loadTimeData.getString(message);
  errorBanner.setAttribute('visible', 'true');

  // The window is hidden if the video has not loaded yet.
  chrome.app.window.current().show();
};

/**
 * Handles playback (decoder) errors.
 * @param {MediaError} error Error object.
 * @private
 */
FullWindowVideoControls.prototype.onPlaybackError_ = function(error) {
  if (error.target && error.target.error &&
      error.target.error.code === MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED) {
    if (this.casting)
      this.showErrorMessage('VIDEO_PLAYER_VIDEO_FILE_UNSUPPORTED_FOR_CAST');
    else
      this.showErrorMessage('VIDEO_PLAYER_VIDEO_FILE_UNSUPPORTED');
    this.decodeErrorOccured = false;
  } else {
    this.showErrorMessage('VIDEO_PLAYER_PLAYBACK_ERROR');
    this.decodeErrorOccured = true;
  }

  // Disable inactivity watcher, and disable the ui, by hiding tools manually.
  this.getInactivityWatcher().disabled = true;
  queryRequiredElement(document, '#video-player')
      .setAttribute('disabled', 'true');

  // Detach the video element, since it may be unreliable and reset stored
  // current playback time.
  this.cleanup();
  this.clearState();

  // Avoid reusing a video element.
  player.unloadVideo();
};

/**
 * Toggles the full screen mode.
 * @private
 */
FullWindowVideoControls.prototype.toggleFullScreen_ = function() {
  var appWindow = chrome.app.window.current();
  util.toggleFullScreen(appWindow, !util.isFullScreen(appWindow));
};

/**
 * Media completion handler.
 */
FullWindowVideoControls.prototype.onMediaComplete = function() {
  VideoControls.prototype.onMediaComplete.apply(this, arguments);
  if (!this.getMedia().loop)
    player.advance_(1);
};

/**
 * Video Player
 *
 * @constructor
 * @struct
 */
function VideoPlayer() {
  this.controls_ = null;
  this.videoElement_ = null;

  /**
   * @type {Array<!FileEntry>}
   * @private
   */
  this.videos_ = null;

  this.currentPos_ = 0;

  this.currentSession_ = null;
  this.currentCast_ = null;

  this.loadQueue_ = new AsyncUtil.Queue();

  this.onCastSessionUpdateBound_ = this.onCastSessionUpdate_.wrap(this);
}

VideoPlayer.prototype = /** @struct */ {
  /**
   * @return {FullWindowVideoControls}
   */
  get controls() {
    return this.controls_;
  }
};

/**
 * Initializes the video player window. This method must be called after DOM
 * initialization.
 * @param {!Array<!FileEntry>} videos List of videos.
 */
VideoPlayer.prototype.prepare = function(videos) {
  this.videos_ = videos;

  var preventDefault = function(event) { event.preventDefault(); }.wrap(null);

  document.ondragstart = preventDefault;

  var maximizeButton = queryRequiredElement(document, '.maximize-button');
  maximizeButton.addEventListener(
      'click',
      function(event) {
        var appWindow = chrome.app.window.current();
        if (appWindow.isMaximized())
          appWindow.restore();
        else
          appWindow.maximize();
        event.stopPropagation();
      }.wrap(null));
  maximizeButton.addEventListener('mousedown', preventDefault);

  var minimizeButton = queryRequiredElement(document, '.minimize-button');
  minimizeButton.addEventListener(
      'click',
      function(event) {
        chrome.app.window.current().minimize();
        event.stopPropagation();
      }.wrap(null));
  minimizeButton.addEventListener('mousedown', preventDefault);

  var closeButton = queryRequiredElement(document, '.close-button');
  closeButton.addEventListener(
      'click',
      function(event) {
        window.close();
        event.stopPropagation();
      }.wrap(null));
  closeButton.addEventListener('mousedown', preventDefault);

  var menu = queryRequiredElement(document, '#cast-menu');
  cr.ui.decorate(menu, cr.ui.Menu);

  this.controls_ = new FullWindowVideoControls(
      queryRequiredElement(document, '#video-player'),
      queryRequiredElement(document, '#video-container'),
      queryRequiredElement(document, '#controls'));

  var reloadVideo = function(e) {
    if (this.controls_.decodeErrorOccured &&
        // Ignore shortcut keys
        !e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey) {
      this.reloadCurrentVideo(function() {
        this.videoElement_.play();
      }.wrap(this));
      e.preventDefault();
    }
  }.wrap(this);

  var arrowRight = queryRequiredElement(document, '.arrow-box .arrow.right');
  arrowRight.addEventListener('click', this.advance_.wrap(this, 1));
  var arrowLeft = queryRequiredElement(document, '.arrow-box .arrow.left');
  arrowLeft.addEventListener('click', this.advance_.wrap(this, 0));

  var videoPlayerElement = queryRequiredElement(document, '#video-player');
  if (videos.length > 1)
    videoPlayerElement.setAttribute('multiple', true);
  else
    videoPlayerElement.removeAttribute('multiple');

  document.addEventListener('keydown', reloadVideo);
  document.addEventListener('click', reloadVideo);
};

/**
 * Unloads the player.
 */
function unload() {
  // Releases keep awake just in case (should be released on unloading video).
  chrome.power.releaseKeepAwake();

  if (!player.controls || !player.controls.getMedia())
    return;

  player.controls.savePosition(true /* exiting */);
  player.controls.cleanup();
}

/**
 * Loads the video file.
 * @param {!FileEntry} video Entry of the video to be played.
 * @param {function()=} opt_callback Completion callback.
 * @private
 */
VideoPlayer.prototype.loadVideo_ = function(video, opt_callback) {
  this.unloadVideo(true);

  this.loadQueue_.run(function(callback) {
    document.title = video.name;

    queryRequiredElement(document, '#title').innerText = video.name;

    var videoPlayerElement = queryRequiredElement(document, '#video-player');
    if (this.currentPos_ === (this.videos_.length - 1))
      videoPlayerElement.setAttribute('last-video', true);
    else
      videoPlayerElement.removeAttribute('last-video');

    if (this.currentPos_ === 0)
      videoPlayerElement.setAttribute('first-video', true);
    else
      videoPlayerElement.removeAttribute('first-video');

    // Re-enables ui and hides error message if already displayed.
    queryRequiredElement(document, '#video-player').removeAttribute('disabled');
    queryRequiredElement(document, '#error').removeAttribute('visible');
    this.controls.detachMedia();
    this.controls.getInactivityWatcher().disabled = true;
    this.controls.decodeErrorOccured = false;
    this.controls.casting = !!this.currentCast_;

    videoPlayerElement.setAttribute('loading', true);

    var media = new MediaManager(video);

    Promise.all([media.getThumbnail(), media.getToken(false)])
        .then(function(results) {
          var url = results[0];
          var token = results[1];
          if (url && token) {
            queryRequiredElement(document, '#thumbnail').style.backgroundImage =
                'url(' + url + '&access_token=' + token + ')';
          } else {
            queryRequiredElement(document, '#thumbnail').style.backgroundImage =
                '';
          }
        })
        .catch(function() {
          // Shows no image on error.
          queryRequiredElement(document, '#thumbnail').style.backgroundImage =
              '';
        });

    var videoElementInitializePromise;
    if (this.currentCast_) {
      metrics.recordPlayType(metrics.PLAY_TYPE.CAST);

      videoPlayerElement.setAttribute('casting', true);

      queryRequiredElement(document, '#cast-name').textContent =
          this.currentCast_.friendlyName;

      videoPlayerElement.setAttribute('castable', true);

      videoElementInitializePromise = media.isAvailableForCast()
          .then(function(result) {
            if (!result)
              return Promise.reject('No casts are available.');

            return new Promise(function(fulfill, reject) {
              chrome.cast.requestSession(
                  fulfill, reject, undefined, this.currentCast_.label);
            }.bind(this)).then(function(session) {
              session.addUpdateListener(this.onCastSessionUpdateBound_);

              this.currentSession_ = session;
              this.videoElement_ = new CastVideoElement(media, session);
              this.controls.attachMedia(this.videoElement_);
            }.bind(this));
          }.bind(this));
    } else {
      metrics.recordPlayType(metrics.PLAY_TYPE.LOCAL);
      videoPlayerElement.removeAttribute('casting');

      this.videoElement_ = document.createElement('video');
      queryRequiredElement(document, '#video-container').appendChild(
          this.videoElement_);

      this.controls.attachMedia(this.videoElement_);
      this.videoElement_.src = video.toURL();

      media.isAvailableForCast().then(function(result) {
        if (result)
          videoPlayerElement.setAttribute('castable', true);
        else
          videoPlayerElement.removeAttribute('castable');
      }).catch(function() {
        videoPlayerElement.setAttribute('castable', true);
      });

      videoElementInitializePromise = Promise.resolve();
    }

    videoElementInitializePromise
        .then(function() {
          var handler = function(currentPos) {
            if (currentPos === this.currentPos_) {
              if (opt_callback)
                opt_callback();
              videoPlayerElement.removeAttribute('loading');
              this.controls.getInactivityWatcher().disabled = false;
            }

            this.videoElement_.removeEventListener('loadedmetadata', handler);
          }.wrap(this, this.currentPos_);

          this.videoElement_.addEventListener('loadedmetadata', handler);

          this.videoElement_.addEventListener('play', function() {
            chrome.power.requestKeepAwake('display');
          }.wrap());
          this.videoElement_.addEventListener('pause', function() {
            chrome.power.releaseKeepAwake();
          }.wrap());

          this.videoElement_.load();
          callback();
        }.bind(this))
        // In case of error.
        .catch(function(error) {
          if (this.currentCast_)
            metrics.recordCastVideoErrorAction();

          videoPlayerElement.removeAttribute('loading');
          console.error('Failed to initialize the video element.',
                        error.stack || error);
          this.controls_.showErrorMessage(
              'VIDEO_PLAYER_VIDEO_FILE_UNSUPPORTED');
          callback();
        }.bind(this));
  }.wrap(this));
};

/**
 * Plays the first video.
 */
VideoPlayer.prototype.playFirstVideo = function() {
  this.currentPos_ = 0;
  this.reloadCurrentVideo(this.onFirstVideoReady_.wrap(this));
};

/**
 * Unloads the current video.
 * @param {boolean=} opt_keepSession If true, keep using the current session.
 *     Otherwise, discards the session.
 */
VideoPlayer.prototype.unloadVideo = function(opt_keepSession) {
  this.loadQueue_.run(function(callback) {
    chrome.power.releaseKeepAwake();

    // Detaches the media from the control.
    this.controls.detachMedia();

    if (this.videoElement_) {
      // If the element has dispose method, call it (CastVideoElement has it).
      if (this.videoElement_.dispose)
        this.videoElement_.dispose();
      // Detach the previous video element, if exists.
      if (this.videoElement_.parentNode)
        this.videoElement_.parentNode.removeChild(this.videoElement_);
    }
    this.videoElement_ = null;

    if (!opt_keepSession && this.currentSession_) {
      this.currentSession_.stop(callback, callback);
      this.currentSession_.removeUpdateListener(this.onCastSessionUpdateBound_);
      this.currentSession_ = null;
    } else {
      callback();
    }
  }.wrap(this));
};

/**
 * Called when the first video is ready after starting to load.
 * @private
 */
VideoPlayer.prototype.onFirstVideoReady_ = function() {
  var videoWidth = this.videoElement_.videoWidth;
  var videoHeight = this.videoElement_.videoHeight;

  var aspect = videoWidth / videoHeight;
  var newWidth = videoWidth;
  var newHeight = videoHeight;

  var shrinkX = newWidth / window.screen.availWidth;
  var shrinkY = newHeight / window.screen.availHeight;
  if (shrinkX > 1 || shrinkY > 1) {
    if (shrinkY > shrinkX) {
      newHeight = newHeight / shrinkY;
      newWidth = newHeight * aspect;
    } else {
      newWidth = newWidth / shrinkX;
      newHeight = newWidth / aspect;
    }
  }

  var oldLeft = window.screenX;
  var oldTop = window.screenY;
  var oldWidth = window.outerWidth;
  var oldHeight = window.outerHeight;

  if (!oldWidth && !oldHeight) {
    oldLeft = window.screen.availWidth / 2;
    oldTop = window.screen.availHeight / 2;
  }

  var appWindow = chrome.app.window.current();
  appWindow.resizeTo(newWidth, newHeight);
  appWindow.moveTo(oldLeft - (newWidth - oldWidth) / 2,
                   oldTop - (newHeight - oldHeight) / 2);
  appWindow.show();

  this.videoElement_.play();
};

/**
 * Advances to the next (or previous) track.
 *
 * @param {boolean} direction True to the next, false to the previous.
 * @private
 */
VideoPlayer.prototype.advance_ = function(direction) {
  var newPos = this.currentPos_ + (direction ? 1 : -1);
  if (0 <= newPos && newPos < this.videos_.length) {
    this.currentPos_ = newPos;
    this.reloadCurrentVideo(function() {
      this.videoElement_.play();
    }.wrap(this));
  }
};

/**
 * Reloads the current video.
 *
 * @param {function()=} opt_callback Completion callback.
 */
VideoPlayer.prototype.reloadCurrentVideo = function(opt_callback) {
  var currentVideo = this.videos_[this.currentPos_];
  this.loadVideo_(currentVideo, opt_callback);
};

/**
 * Invokes when a menuitem in the cast menu is selected.
 * @param {Object} cast Selected element in the list of casts.
 * @private
 */
VideoPlayer.prototype.onCastSelected_ = function(cast) {
  // If the selected item is same as the current item, do nothing.
  if ((this.currentCast_ && this.currentCast_.label) === (cast && cast.label))
    return;

  this.unloadVideo(false);

  // Waits for unloading video.
  this.loadQueue_.run(function(callback) {
    this.currentCast_ = cast || null;
    this.updateCheckOnCastMenu_();
    this.reloadCurrentVideo();
    callback();
  }.wrap(this));
};

/**
 * Set the list of casts.
 * @param {Array<Object>} casts List of casts.
 */
VideoPlayer.prototype.setCastList = function(casts) {
  var videoPlayerElement = queryRequiredElement(document, '#video-player');
  var menu = queryRequiredElement(document, '#cast-menu');
  menu.innerHTML = '';

  // TODO(yoshiki): Handle the case that the current cast disappears.

  if (casts.length === 0) {
    videoPlayerElement.removeAttribute('cast-available');
    if (this.currentCast_)
      this.onCurrentCastDisappear_();
    return;
  }

  if (this.currentCast_) {
    var currentCastAvailable = casts.some(function(cast) {
      return this.currentCast_.label === cast.label;
    }.wrap(this));

    if (!currentCastAvailable)
      this.onCurrentCastDisappear_();
  }

  var item = new cr.ui.MenuItem();
  item.label = loadTimeData.getString('VIDEO_PLAYER_PLAY_THIS_COMPUTER');
  item.setAttribute('aria-label', item.label);
  item.castLabel = '';
  item.addEventListener('activate', this.onCastSelected_.wrap(this, null));
  menu.appendChild(item);

  for (var i = 0; i < casts.length; i++) {
    var item = new cr.ui.MenuItem();
    item.label = casts[i].friendlyName;
    item.setAttribute('aria-label', item.label);
    item.castLabel = casts[i].label;
    item.addEventListener('activate',
                          this.onCastSelected_.wrap(this, casts[i]));
    menu.appendChild(item);
  }
  this.updateCheckOnCastMenu_();
  videoPlayerElement.setAttribute('cast-available', true);
};

/**
 * Updates the check status of the cast menu items.
 * @private
 */
VideoPlayer.prototype.updateCheckOnCastMenu_ = function() {
  var menu = queryRequiredElement(document, '#cast-menu');
  var menuItems = menu.menuItems;
  for (var i = 0; i < menuItems.length; i++) {
    var item = menuItems[i];
    if (this.currentCast_ === null) {
      // Playing on this computer.
      if (item.castLabel === '')
        item.checked = true;
      else
        item.checked = false;
    } else {
      // Playing on cast device.
      if (item.castLabel === this.currentCast_.label)
        item.checked = true;
      else
        item.checked = false;
    }
  }
};

/**
 * Called when the current cast is disappear from the cast list.
 * @private
 */
VideoPlayer.prototype.onCurrentCastDisappear_ = function() {
  this.currentCast_ = null;
  if (this.currentSession_) {
    this.currentSession_.removeUpdateListener(this.onCastSessionUpdateBound_);
    this.currentSession_ = null;
  }
  this.controls.showErrorMessage('VIDEO_PLAYER_PLAYBACK_ERROR');
  this.unloadVideo();
};

/**
 * This method should be called when the session is updated.
 * @param {boolean} alive Whether the session is alive or not.
 * @private
 */
VideoPlayer.prototype.onCastSessionUpdate_ = function(alive) {
  if (!alive)
    this.unloadVideo();
};

var player = new VideoPlayer();

/**
 * Initializes the strings.
 * @param {function()} callback Called when the sting data is ready.
 */
function initStrings(callback) {
  chrome.fileManagerPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;
    i18nTemplate.process(document, loadTimeData);
    callback();
  }.wrap(null));
}

function initVolumeManager(callback) {
  var volumeManager = new VolumeManagerWrapper(
      VolumeManagerWrapper.NonNativeVolumeStatus.ENABLED);
  volumeManager.ensureInitialized(callback);
}

var initPromise = Promise.all(
    [new Promise(initStrings.wrap(null)),
     new Promise(initVolumeManager.wrap(null)),
     new Promise(util.addPageLoadHandler.wrap(null))]);

initPromise.then(function(unused) {
  return new Promise(function(fulfill, reject) {
    util.URLsToEntries(window.appState.items, function(entries) {
      metrics.recordOpenVideoPlayerAction();
      metrics.recordNumberOfOpenedFiles(entries.length);

      player.prepare(entries);
      player.playFirstVideo(player, fulfill);
    }.wrap());
  }.wrap());
}.wrap());
