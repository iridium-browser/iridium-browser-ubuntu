// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

// Length of the various components of the access code in number of digits.
var SUPPORT_ID_LENGTH = 7;
var HOST_SECRET_LENGTH = 5;
var ACCESS_CODE_LENGTH = SUPPORT_ID_LENGTH + HOST_SECRET_LENGTH;

/**
 * @constructor
 * @implements {remoting.Activity}
 */
remoting.It2MeActivity = function() {
  /** @private */
  this.hostId_ = '';
  /** @private */
  this.passCode_ = '';

  var form = document.getElementById('access-code-form');
  /** @private */
  this.accessCodeDialog_ = new remoting.InputDialog(
    remoting.AppMode.CLIENT_UNCONNECTED,
    form,
    form.querySelector('#access-code-entry'),
    form.querySelector('#cancel-access-code-button'));

  /** @private {remoting.DesktopRemotingActivity} */
  this.desktopActivity_ = null;
};

remoting.It2MeActivity.prototype.dispose = function() {
  base.dispose(this.desktopActivity_);
  this.desktopActivity_ = null;
};

remoting.It2MeActivity.prototype.start = function() {
  var that = this;

  this.desktopActivity_ = new remoting.DesktopRemotingActivity(this);

  this.accessCodeDialog_.show().then(function(/** string */ accessCode) {
    that.desktopActivity_.getConnectingDialog().show();
    return that.verifyAccessCode_(accessCode);
  }).then(function() {
    return remoting.HostListApi.getInstance().getSupportHost(that.hostId_);
  }).then(function(/** remoting.Host */ host) {
    that.connect_(host);
  }).catch(remoting.Error.handler(function(/** remoting.Error */ error) {
    if (error.hasTag(remoting.Error.Tag.CANCELLED)) {
      remoting.setMode(remoting.AppMode.HOME);
    } else {
      var errorDiv = document.getElementById('connect-error-message');
      l10n.localizeElementFromTag(errorDiv, error.getTag());
      remoting.setMode(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
    }
  }));
};

remoting.It2MeActivity.prototype.stop = function() {
  this.desktopActivity_.stop();
};

/**
 * @param {!remoting.Error} error
 */
remoting.It2MeActivity.prototype.onConnectionFailed = function(error) {
  this.showErrorMessage_(error);
  base.dispose(this.desktopActivity_);
  this.desktopActivity_ = null;
};

/**
 * @param {!remoting.ConnectionInfo} connectionInfo
 */
remoting.It2MeActivity.prototype.onConnected = function(connectionInfo) {
  this.accessCodeDialog_.inputField().value = '';
};

remoting.It2MeActivity.prototype.onDisconnected = function(error) {
  if (error.isNone()) {
    this.showFinishDialog_(remoting.AppMode.CLIENT_SESSION_FINISHED_IT2ME);
  } else {
    this.showErrorMessage_(error);
  }

  base.dispose(this.desktopActivity_);
  this.desktopActivity_ = null;
};

/**
 * @param {!remoting.Error} error
 * @private
 */
remoting.It2MeActivity.prototype.showErrorMessage_ = function(error) {
  var errorDiv = document.getElementById('connect-error-message');
  l10n.localizeElementFromTag(errorDiv, error.getTag());
  this.showFinishDialog_(remoting.AppMode.CLIENT_CONNECT_FAILED_IT2ME);
};

/** @return {remoting.DesktopRemotingActivity} */
remoting.It2MeActivity.prototype.getDesktopActivityForTesting = function() {
  return this.desktopActivity_;
};

/**
 * @param {remoting.AppMode} mode
 * @private
 */
remoting.It2MeActivity.prototype.showFinishDialog_ = function(mode) {
  var finishDialog = new remoting.MessageDialog(
      mode,
      document.getElementById('client-finished-it2me-button'));
  finishDialog.show().then(function() {
    remoting.setMode(remoting.AppMode.HOME);
  });
};

/**
 * @param {string} accessCode
 * @return {Promise}  Promise that resolves if the access code is valid.
 * @private
 */
remoting.It2MeActivity.prototype.verifyAccessCode_ = function(accessCode) {
  var normalizedAccessCode = accessCode.replace(/\s/g, '');
  if (normalizedAccessCode.length !== ACCESS_CODE_LENGTH) {
    return Promise.reject(new remoting.Error(
        remoting.Error.Tag.INVALID_ACCESS_CODE));
  }

  this.hostId_ = normalizedAccessCode.substring(0, SUPPORT_ID_LENGTH);
  this.passCode_ = normalizedAccessCode;

  return Promise.resolve();
};

/**
 * @param {remoting.Host} host
 * @private
 */
remoting.It2MeActivity.prototype.connect_ = function(host) {
  this.desktopActivity_.start(
      host, new remoting.CredentialsProvider({ accessCode: this.passCode_ }));
};

})();
