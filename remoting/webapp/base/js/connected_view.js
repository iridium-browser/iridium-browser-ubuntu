// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Implements a basic UX control for a connected remoting session.
 */

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * @param {remoting.ClientPlugin} plugin
 * @param {HTMLElement} viewportElement
 * @param {HTMLElement} cursorElement
 *
 * @constructor
 * @implements {base.Disposable}
 */
remoting.ConnectedView = function(plugin, viewportElement, cursorElement) {
  /** @private */
  this.viewportElement_ = viewportElement;

  /** @private */
  this.plugin_ = plugin;

  /** private */
  this.cursor_ = new remoting.ConnectedView.Cursor(
      plugin, viewportElement, cursorElement);

  /** @private {Element} */
  this.debugRegionContainer_ =
      viewportElement.querySelector('.debug-region-container');

  var pluginElement = plugin.element();

  /** private */
  this.disposables_ = new base.Disposables(
    this.cursor_,
    new base.DomEventHook(pluginElement, 'blur',
                          this.onPluginLostFocus_.bind(this), false),
    new base.DomEventHook(document, 'visibilitychange',
                          this.onVisibilityChanged_.bind(this), false),
    new remoting.Clipboard(plugin)
  );

  // TODO(wez): Only allow mouse lock if the app has the pointerLock permission.
  // Enable automatic mouse-lock.
  if (this.plugin_.hasFeature(remoting.ClientPlugin.Feature.ALLOW_MOUSE_LOCK)) {
    this.plugin_.allowMouseLock();
  }

  pluginElement.focus();
};

/**
 * @return {void} Nothing.
 */
remoting.ConnectedView.prototype.dispose = function() {
  base.dispose(this.disposables_);
  this.disposables_ = null;
  this.cursorRender_ = null;
  this.plugin_ = null;
};

/**
 * Called when the app window is hidden.
 * @return {void} Nothing.
 * @private
 */
remoting.ConnectedView.prototype.onVisibilityChanged_ = function() {
  var pause = document.hidden;
  this.plugin_.pauseVideo(pause);
  this.plugin_.pauseAudio(pause);
};

/**
 * Callback that the plugin invokes to indicate when the connection is
 * ready.
 *
 * @param {boolean} ready True if the connection is ready.
 */
remoting.ConnectedView.prototype.onConnectionReady = function(ready) {
  this.viewportElement_.classList.toggle('session-client-inactive', !ready);
};

/**
 * Callback function called when the plugin element loses focus.
 * @private
 */
remoting.ConnectedView.prototype.onPluginLostFocus_ = function() {
  // Release all keys to prevent them becoming 'stuck down' on the host.
  this.plugin_.releaseAllKeys();

  // Focus should stay on the element, not (for example) the toolbar.
  // Due to crbug.com/246335, we can't restore the focus immediately,
  // otherwise the plugin gets confused about whether or not it has focus.
  window.setTimeout(
      this.plugin_.element().focus.bind(this.plugin_.element()), 0);
};

/**
 * Handles dirty region debug messages.
 *
 * @param {{rects:Array<Array<number>>}} region Dirty region of the latest
 *     frame.
 */
remoting.ConnectedView.prototype.handleDebugRegion = function(region) {
  while (this.debugRegionContainer_.firstChild) {
    this.debugRegionContainer_.removeChild(
        this.debugRegionContainer_.firstChild);
  }
  if (region.rects) {
    var rects = region.rects;
    for (var i = 0; i < rects.length; ++i) {
      var rect = document.createElement('div');
      rect.classList.add('debug-region-rect');
      rect.style.left = rects[i][0] + 'px';
      rect.style.top = rects[i][1] +'px';
      rect.style.width = rects[i][2] +'px';
      rect.style.height = rects[i][3] + 'px';
      this.debugRegionContainer_.appendChild(rect);
    }
  }
};

/**
 * Enables or disables rendering of dirty regions for debugging.
 * @param {boolean} enable True to enable rendering.
 */
remoting.ConnectedView.prototype.enableDebugRegion = function(enable) {
  if (enable) {
    this.plugin_.setDebugDirtyRegionHandler(this.handleDebugRegion.bind(this));
  } else {
    this.plugin_.setDebugDirtyRegionHandler(null);
  }
};

/**
 * @param {remoting.ClientPlugin} plugin
 * @param {HTMLElement} viewportElement
 * @param {HTMLElement} cursorElement
 *
 * @constructor
 * @implements {base.Disposable}
 */
remoting.ConnectedView.Cursor = function(
    plugin, viewportElement, cursorElement) {
  /** @private */
  this.plugin_ = plugin;
  /** @private */
  this.cursorElement_ = cursorElement;
  /** @private */
  this.eventHook_ = new base.DomEventHook(
      viewportElement, 'mousemove', this.onMouseMoved_.bind(this), true);

  this.plugin_.setMouseCursorHandler(this.onCursorChanged_.bind(this));
};

remoting.ConnectedView.Cursor.prototype.dispose = function() {
  base.dispose(this.eventHook_);
  this.eventHook_ = null;
  this.plugin_.setMouseCursorHandler(
      /** function(string, string, number) */ base.doNothing);
  this.plugin_ = null;
};

/**
 * @param {string} url
 * @param {number} hotspotX
 * @param {number} hotspotY
 * @private
 */
remoting.ConnectedView.Cursor.prototype.onCursorChanged_ = function(
    url, hotspotX, hotspotY) {
  this.cursorElement_.hidden = !url;
  if (url) {
    this.cursorElement_.style.marginLeft = '-' + hotspotX + 'px';
    this.cursorElement_.style.marginTop = '-' + hotspotY + 'px';
    this.cursorElement_.src = url;
  }
};

/**
 * @param {Event} event
 * @private
 */
remoting.ConnectedView.Cursor.prototype.onMouseMoved_ = function(event) {
  this.cursorElement_.style.top = event.offsetY + 'px';
  this.cursorElement_.style.left = event.offsetX + 'px';
};

})();
