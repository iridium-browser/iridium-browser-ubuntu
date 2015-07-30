// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class handling the in-session options menu (or menus in the case of apps v1).
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {Element} sendCtrlAltDel
 * @param {Element} sendPrtScrn
 * @param {Element} resizeToClient
 * @param {Element} shrinkToFit
 * @param {Element} newConnection
 * @param {Element?} fullscreen
 * @param {Element?} toggleStats
 * @param {Element?} startStopRecording
 * @constructor
 */
remoting.OptionsMenu = function(sendCtrlAltDel, sendPrtScrn,
                                resizeToClient, shrinkToFit,
                                newConnection, fullscreen, toggleStats,
                                startStopRecording) {
  this.sendCtrlAltDel_ = sendCtrlAltDel;
  this.sendPrtScrn_ = sendPrtScrn;
  this.resizeToClient_ = resizeToClient;
  this.shrinkToFit_ = shrinkToFit;
  this.newConnection_ = newConnection;
  this.fullscreen_ = fullscreen;
  this.toggleStats_ = toggleStats;
  this.startStopRecording_ = startStopRecording;

  /** @private {remoting.DesktopConnectedView} */
  this.desktopConnectedView_ = null;

  this.sendCtrlAltDel_.addEventListener(
      'click', this.onSendCtrlAltDel_.bind(this), false);
  this.sendPrtScrn_.addEventListener(
      'click', this.onSendPrtScrn_.bind(this), false);
  this.resizeToClient_.addEventListener(
      'click', this.onResizeToClient_.bind(this), false);
  this.shrinkToFit_.addEventListener(
      'click', this.onShrinkToFit_.bind(this), false);
  this.newConnection_.addEventListener(
      'click', this.onNewConnection_.bind(this), false);

  if (this.fullscreen_) {
    fullscreen.addEventListener('click', this.onFullscreen_.bind(this), false);
  }
  if (this.toggleStats_) {
    toggleStats.addEventListener(
        'click', this.onToggleStats_.bind(this), false);
  }
  if (this.startStopRecording_) {
    this.startStopRecording_.addEventListener(
        'click', this.onStartStopRecording_.bind(this), false);
  }
};

/**
 * @param {remoting.DesktopConnectedView} desktopConnectedView The view for the
 *     active session, or null if there is no connection.
 */
remoting.OptionsMenu.prototype.setDesktopConnectedView = function(
    desktopConnectedView) {
  this.desktopConnectedView_ = desktopConnectedView;
};

remoting.OptionsMenu.prototype.onShow = function() {
  if (this.desktopConnectedView_) {
    base.debug.assert(remoting.app instanceof remoting.DesktopRemoting);
    var drApp = /** @type {remoting.DesktopRemoting} */ (remoting.app);
    var mode = drApp.getConnectionMode();

    this.resizeToClient_.hidden = mode === remoting.DesktopRemoting.Mode.IT2ME;
    remoting.MenuButton.select(
        this.resizeToClient_, this.desktopConnectedView_.getResizeToClient());
    remoting.MenuButton.select(
        this.shrinkToFit_, this.desktopConnectedView_.getShrinkToFit());

    if (this.fullscreen_) {
      remoting.MenuButton.select(
          this.fullscreen_, remoting.fullscreen.isActive());
    }
    if (this.toggleStats_) {
      remoting.MenuButton.select(
          this.toggleStats_, this.desktopConnectedView_.isStatsVisible());
    }
    if (this.startStopRecording_) {
      this.startStopRecording_.hidden =
          !this.desktopConnectedView_.canRecordVideo();
      if (this.desktopConnectedView_.isRecordingVideo()) {
        l10n.localizeElementFromTag(this.startStopRecording_,
                                    /*i18n-content*/'STOP_RECORDING');
      } else {
        l10n.localizeElementFromTag(this.startStopRecording_,
                                    /*i18n-content*/'START_RECORDING');
      }
    }
  }
};

remoting.OptionsMenu.prototype.onSendCtrlAltDel_ = function() {
  if (this.desktopConnectedView_) {
    this.desktopConnectedView_.sendCtrlAltDel();
  }
};

remoting.OptionsMenu.prototype.onSendPrtScrn_ = function() {
  if (this.desktopConnectedView_) {
    this.desktopConnectedView_.sendPrintScreen();
  }
};

remoting.OptionsMenu.prototype.onResizeToClient_ = function() {
  if (this.desktopConnectedView_) {
    this.desktopConnectedView_.setScreenMode(
        this.desktopConnectedView_.getShrinkToFit(),
        !this.desktopConnectedView_.getResizeToClient());
  }
};

remoting.OptionsMenu.prototype.onShrinkToFit_ = function() {
  if (this.desktopConnectedView_) {
    this.desktopConnectedView_.setScreenMode(
        !this.desktopConnectedView_.getShrinkToFit(),
        this.desktopConnectedView_.getResizeToClient());
  }
};

remoting.OptionsMenu.prototype.onNewConnection_ = function() {
  chrome.app.window.create('main.html', {
    'width': 800,
    'height': 600,
    'frame': 'none'
  });
};

remoting.OptionsMenu.prototype.onFullscreen_ = function() {
  remoting.fullscreen.toggle();
};

remoting.OptionsMenu.prototype.onToggleStats_ = function() {
  if (this.desktopConnectedView_) {
    this.desktopConnectedView_.toggleStats();
  }
};

remoting.OptionsMenu.prototype.onStartStopRecording_ = function() {
  if (this.desktopConnectedView_) {
    this.desktopConnectedView_.startStopRecording();
  }
};

/**
 * @type {remoting.OptionsMenu}
 */
remoting.optionsMenu = null;
