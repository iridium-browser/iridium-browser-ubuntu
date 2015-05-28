// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class that routes gnubby-auth extension messages to and from the gnubbyd
 * extension.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.ProtocolExtension}
 */
remoting.GnubbyAuthHandler = function() {
  /** @private {?function(string,string)} */
  this.sendMessageToHostCallback_ = null;
};

/** @private {string} */
remoting.GnubbyAuthHandler.EXTENSION_TYPE = 'gnubby-auth';

/** @return {Array<string>} */
remoting.GnubbyAuthHandler.prototype.getExtensionTypes = function() {
  return [remoting.GnubbyAuthHandler.EXTENSION_TYPE];
};

/**
 * @param {function(string,string)} sendMessageToHost Callback to send a message
 *     to the host.
 */
remoting.GnubbyAuthHandler.prototype.startExtension =
    function(sendMessageToHost) {
  this.sendMessageToHostCallback_ = sendMessageToHost;

  this.sendMessageToHost_({
    'type': 'control',
    'option': 'auth-v1'
  });
};

/**
 * @param {Object} data The data to send.
 * @private
 */
remoting.GnubbyAuthHandler.prototype.sendMessageToHost_ = function(data) {
  this.sendMessageToHostCallback_(remoting.GnubbyAuthHandler.EXTENSION_TYPE,
                                  JSON.stringify(data));
}

/**
 * Processes gnubby-auth messages.
 *
 * @param {string} type The message type.
 * @param {Object} message The parsed extension message data.
 * @return {boolean} True if the extension message was handled.
 */
remoting.GnubbyAuthHandler.prototype.onExtensionMessage =
    function(type, message) {
  var messageType = base.getStringAttr(message, 'type');
  if (messageType == 'data') {
    this.sendMessageToGnubbyd_({
      'type': 'auth-agent@openssh.com',
      'data': base.getArrayAttr(message, 'data')
    }, this.callback_.bind(this, base.getNumberAttr(message, 'connectionId')));
  } else {
    console.error('Invalid gnubby-auth message: ' + messageType);
    return false;
  }
  return true;
};

/**
 * Callback invoked with data to be returned to the host.
 * @param {number} connectionId The connection id.
 * @param {Object} response The JSON response with the data to send to the host.
 * @private
 */
remoting.GnubbyAuthHandler.prototype.callback_ =
    function(connectionId, response) {
  try {
    this.sendMessageToHost_({
      'type': 'data',
      'connectionId': connectionId,
      'data': base.getArrayAttr(response, 'data')
    });
  } catch (/** @type {*} */ err) {
    console.error('gnubby callback failed: ', err);
    this.sendMessageToHost_({
      'type': 'error',
      'connectionId': connectionId
    });
    return;
  }
};

/**
 * Send data to the gnubbyd extension.
 * @param {Object} jsonObject The JSON object to send to the gnubbyd extension.
 * @param {function(Object)} callback The callback to invoke with reply data.
 * @private
 */
remoting.GnubbyAuthHandler.prototype.sendMessageToGnubbyd_ =
    function(jsonObject, callback) {
  var kGnubbydDevExtensionId = 'dlfcjilkjfhdnfiecknlnddkmmiofjbg';

  chrome.runtime.sendMessage(
      kGnubbydDevExtensionId,
      jsonObject,
      onGnubbydDevReply_.bind(this, jsonObject, callback));
};

/**
 * Callback invoked as a result of sending a message to the gnubbyd-dev
 * extension. If that extension is not installed, reply will be undefined;
 * otherwise it will be the JSON response object.
 * @param {Object} jsonObject The JSON object to send to the gnubbyd extension.
 * @param {function(Object)} callback The callback to invoke with reply data.
 * @param {Object} reply The reply from the extension (or Chrome, if the
 *    extension does not exist.
 * @private
 */
function onGnubbydDevReply_(jsonObject, callback, reply) {
  var kGnubbydStableExtensionId = 'beknehfpfkghjoafdifaflglpjkojoco';

  if (reply) {
    callback(reply);
  } else {
    chrome.runtime.sendMessage(kGnubbydStableExtensionId, jsonObject, callback);
  }
}
