// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * @param {remoting.SessionConnector} sessionConnector
 * @param {remoting.Host} host
 *
 * @constructor
 */
remoting.Me2MeConnectFlow = function(sessionConnector, host) {
  /** @private */
  this.host_ = host;
  /** @private */
  this.connector_ = sessionConnector;
};

remoting.Me2MeConnectFlow.prototype.start = function() {
  var webappVersion = chrome.runtime.getManifest().version;
  var needsUpdateDialog = new remoting.HostNeedsUpdateDialog(
      document.getElementById('host-needs-update-dialog'), this.host_);
  var that = this;

  needsUpdateDialog.showIfNecessary(webappVersion).then(function() {
    return that.host_.options.load();
  }).then(function() {
    that.connect_();
  }).catch(function(/** remoting.Error */ error) {
    if (error.hasTag(remoting.Error.Tag.CANCELLED)) {
      remoting.setMode(remoting.AppMode.HOME);
    }
  });
};

/** @private */
remoting.Me2MeConnectFlow.prototype.connect_ = function() {
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
  var host = this.host_;

  /**
   * @param {string} tokenUrl Token-issue URL received from the host.
   * @param {string} hostPublicKey Host public key (DER and Base64 encoded).
   * @param {string} scope OAuth scope to request the token for.
   * @param {function(string, string):void} onThirdPartyTokenFetched Callback.
   */
  var fetchThirdPartyToken = function(
      tokenUrl, hostPublicKey, scope, onThirdPartyTokenFetched) {
    var thirdPartyTokenFetcher = new remoting.ThirdPartyTokenFetcher(
        tokenUrl, hostPublicKey, scope, host.tokenUrlPatterns,
        onThirdPartyTokenFetched);
    thirdPartyTokenFetcher.fetchToken();
  };

  /**
   * @param {boolean} supportsPairing
   * @param {function(string):void} onPinFetched
   */
  var requestPin = function(supportsPairing, onPinFetched) {
    var pinDialog =
        new remoting.PinDialog(document.getElementById('pin-dialog'), host);
    pinDialog.show(supportsPairing).then(function(/** string */ pin) {
      remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
      onPinFetched(pin);
      /** @type {boolean} */
      remoting.pairingRequested = pinDialog.pairingRequested();
    }).catch(function(/** remoting.Error */ error) {
      base.debug.assert(error.hasTag(remoting.Error.Tag.CANCELLED));
      remoting.setMode(remoting.AppMode.HOME);
    });
  };

  var pairingInfo = host.options.pairingInfo;
  this.connector_.connectMe2Me(host, requestPin, fetchThirdPartyToken,
                               pairingInfo.clientId, pairingInfo.sharedSecret);
};


/**
 * @param {HTMLElement} rootElement
 * @param {remoting.Host} host
 * @constructor
 */
remoting.HostNeedsUpdateDialog = function(rootElement, host) {
  /** @private */
  this.host_ = host;
  /** @private */
  this.rootElement_ = rootElement;
  /** @private {base.Deferred} */
  this.deferred_ = null;
  /** @private {base.Disposables} */
  this.eventHooks_ = null;

  l10n.localizeElementFromTag(
      rootElement.querySelector('.host-needs-update-message'),
      /*i18n-content*/'HOST_NEEDS_UPDATE_TITLE', host.hostName);
};

/**
 * Shows the HostNeedsUpdateDialog if the user is trying to connect to a legacy
 * host.
 *
 * @param {string} webappVersion
 * @return {Promise}  Promise that resolves when no update is required or
 *    when the user ignores the update warning.  Rejects with
 *    |remoting.Error.CANCELLED| if the user cancels the connection.
 */
remoting.HostNeedsUpdateDialog.prototype.showIfNecessary =
    function(webappVersion) {
  if (!remoting.Host.needsUpdate(this.host_, webappVersion)) {
    return Promise.resolve();
  }

  this.eventHooks_ = new base.Disposables(
    new base.DomEventHook(this.rootElement_.querySelector('.connect-button'),
                         'click', this.onOK_.bind(this), false),
    new base.DomEventHook(this.rootElement_.querySelector('.cancel-button'),
                          'click', this.onCancel_.bind(this), false));

  base.debug.assert(this.deferred_ === null);
  this.deferred_ = new base.Deferred();

  remoting.setMode(remoting.AppMode.CLIENT_HOST_NEEDS_UPGRADE);

  return this.deferred_.promise();
};

/** @private */
remoting.HostNeedsUpdateDialog.prototype.cleanup_ = function() {
  base.dispose(this.eventHooks_);
  this.eventHooks_ = null;
  this.deferred_ = null;
};


/** @private */
remoting.HostNeedsUpdateDialog.prototype.onOK_ = function() {
  this.deferred_.resolve();
  this.cleanup_();
};

/** @private */
remoting.HostNeedsUpdateDialog.prototype.onCancel_ = function() {
  this.deferred_.reject(new remoting.Error(remoting.Error.Tag.CANCELLED));
  this.cleanup_();
};

/**
 * @param {HTMLElement} rootElement
 * @param {remoting.Host} host
 * @constructor
 */
remoting.PinDialog = function(rootElement, host) {
  /** @private */
  this.rootElement_ = rootElement;
  /** @private */
  this.pairingCheckbox_ = rootElement.querySelector('.pairing-checkbox');
  /** @private */
  this.pinInput_ = rootElement.querySelector('.pin-inputField');
  /** @private */
  this.host_ = host;
  /** @private */
  this.dialog_ = new remoting.InputDialog(
    remoting.AppMode.CLIENT_PIN_PROMPT,
    this.rootElement_.querySelector('form'),
    this.pinInput_,
    this.rootElement_.querySelector('.cancel-button'));
};


/**
 * @param {boolean} supportsPairing
 * @return {Promise<string>}  Promise that resolves with the PIN or rejects with
 *    |remoting.Error.CANCELLED| if the user cancels the connection.
 */
remoting.PinDialog.prototype.show = function(supportsPairing) {
  // Reset the UI.
  this.pairingCheckbox_.checked = false;
  this.rootElement_.querySelector('.pairing-section').hidden = !supportsPairing;
  var message = this.rootElement_.querySelector('.pin-message');
  l10n.localizeElement(message, this.host_.hostName);
  this.pinInput_.value = '';
  return this.dialog_.show();
};

/** @return {boolean} */
remoting.PinDialog.prototype.pairingRequested = function() {
  return this.pairingCheckbox_.checked;
};

})();
