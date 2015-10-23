// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dimmable UI Controller.
 * @param {!HTMLElement} container Container.
 * @constructor
 * @struct
 */
function DimmableUIController(container) {
  /**
   * @private {!HTMLElement}
   * @const
   */
  this.container_ = container;

  /**
   * @private {NodeList}
   */
  this.tools_ = null;

  /**
   * @private {number}
   */
  this.timeoutId_ = 0;

  /**
   * @private {boolean}
   */
  this.isCursorInTools_ = false;

  /**
   * @private {boolean}
   */
  this.disabled_ = false;

  /**
   * @private {boolean}
   */
  this.maybeTap_ = false;

  /**
   * @private {number}
   */
  this.touchStartedAt_ = 0;

  /**
   * @private {HTMLElement}
   */
  this.touchTarget_ = null;

  this.container_.addEventListener('click', this.onClick_.bind(this));
  this.container_.addEventListener('mousemove', this.onMousemove_.bind(this));
  this.container_.addEventListener('touchstart', this.onTouchstart_.bind(this));
  this.container_.addEventListener('touchmove', this.onTouchmove_.bind(this));
  this.container_.addEventListener('touchend', this.onTouchend_.bind(this));
  this.container_.addEventListener('touchcancel',
      this.onTouchcancel_.bind(this));
}

/**
 * Default timeout.
 * @const {number}
 */
DimmableUIController.DEFAULT_TIMEOUT = 3000; // ms

/**
 * Duration to consider a touch operation as a tap.
 * @const {number}
 */
DimmableUIController.TAP_DURATION = 300; // ms

/**
 * Handles click event.
 * @param {!Event} event An event.
 * @private
 */
DimmableUIController.prototype.onClick_ = function(event) {
  if (this.disabled_ ||
      (event.target &&
       this.isPartOfTools_(/** @type {!HTMLElement} */ (event.target)))) {
    return;
  }

  this.toggle_();
};

/**
 * Handles mousemove event.
 * @private
 */
DimmableUIController.prototype.onMousemove_ = function() {
  if (this.disabled_)
    return;

  this.kick();
};

/**
 * Handles touchstart event.
 * @param {!Event} event An event.
 * @private
 */
DimmableUIController.prototype.onTouchstart_ = function(event) {
  if (this.disabled_)
    return;

  this.maybeTap_ = true;
  this.touchStartedAt_ = Date.now();
  this.touchTarget_ = /** @type {HTMLElement} */ (event.target);
};

/**
 * Handles touchmove event.
 * @private
 */
DimmableUIController.prototype.onTouchmove_ = function() {
  if (this.disabled_ || !this.maybeTap_ ||
      Date.now() - this.touchStartedAt_ < DimmableUIController.TAP_DURATION) {
    return;
  }

  this.maybeTap_ = false;
  this.show_(true);
  this.clearTimeout_();
};

/**
 * Handles touchend event.
 * @private
 */
DimmableUIController.prototype.onTouchend_ = function() {
  if (this.disabled_)
    return;

  if (this.maybeTap_ && this.touchTarget_ &&
      !this.isPartOfTools_(this.touchTarget_))
    this.toggle_();
  else
    this.kick();
};

/**
 * Handles touchcancel event.
 * @private
 */
DimmableUIController.prototype.onTouchcancel_ = function() {
  if (this.disabled_)
    return;

  // If touch operation has been canceled, we handle it mostly same with
  // touchend except that we never handle it as a tap.
  this.kick();
};

/**
 * Handles mouseover event.
 * @private
 */
DimmableUIController.prototype.onMouseover_ = function() {
  if (this.disabled_)
    return;

  this.isCursorInTools_ = true;
};

/**
 * Handles mouseout event.
 * @private
 */
DimmableUIController.prototype.onMouseout_ = function() {
  if (this.disabled_)
    return;

  this.isCursorInTools_ = false;
};

/**
 * Returns true if element is a part of tools.
 * @param {!HTMLElement} element A html element.
 * @return {boolean} True if element is a part of tools.
 */
DimmableUIController.prototype.isPartOfTools_ = function(element) {
  for (var i = 0; i < this.tools_.length; i++) {
    if (this.tools_[i].contains(element))
      return true;
  }
  return false;
};

/**
 * Toggles visibility of UI.
 * @private
 */
DimmableUIController.prototype.toggle_ = function() {
  if (this.isToolsVisible_()) {
    this.clearTimeout_();
    this.show_(false);
  } else {
    this.kick();
  }
};

/**
 * Returns true if UI is visible.
 * @return {boolean} True if UI is visible.
 */
DimmableUIController.prototype.isToolsVisible_ = function() {
  return this.container_.hasAttribute('tools');
};

/**
 * Shows UI.
 * @param {boolean} show True to show UI.
 */
DimmableUIController.prototype.show_ = function(show) {
  if (show)
    this.container_.setAttribute('tools', true);
  else
    this.container_.removeAttribute('tools');
};

/**
 * Clears current timeout.
 */
DimmableUIController.prototype.clearTimeout_ = function() {
  if (!this.timeoutId_)
    return;

  clearTimeout(this.timeoutId_);
  this.timeoutId_ = 0;
};

/**
 * Extends current timeout.
 * @param {number=} opt_timeout Timeout.
 */
DimmableUIController.prototype.extendTimeout_ = function(opt_timeout) {
  this.clearTimeout_();

  var timeout = opt_timeout || DimmableUIController.DEFAULT_TIMEOUT;
  this.timeoutId_ = setTimeout(this.onTimeout_.bind(this), timeout);
};

/**
 * Handles timeout.
 */
DimmableUIController.prototype.onTimeout_ = function() {
  // If mouse cursor is on tools, extend timeout.
  if (this.isCursorInTools_) {
    this.extendTimeout_();
    return;
  }

  this.show_(false /* hide */);
};

/**
 * Sets tools which are controlled by this controller.
 * This method must not be called more than once for an instance.
 * @param {!NodeList} tools Tools.
 */
DimmableUIController.prototype.setTools = function(tools) {
  assert(this.tools_ === null);

  this.tools_ = tools;

  for (var i = 0; i < this.tools_.length; i++) {
    this.tools_[i].addEventListener('mouseover', this.onMouseover_.bind(this));
    this.tools_[i].addEventListener('mouseout', this.onMouseout_.bind(this));
  }
};

/**
 * Shows UI and set timeout.
 * @param {number=} opt_timeout Timeout.
 */
DimmableUIController.prototype.kick = function(opt_timeout) {
  if (this.disabled_)
    return;

  this.show_(true);
  this.extendTimeout_(opt_timeout);
};

/**
 * Disables this controller.
 * When disabled, the UI is fixed to visible.
 * When enabled, the UI becomes visible with timeout.
 *
 * @param {boolean} disabled True to disable.
 */
DimmableUIController.prototype.setDisabled = function(disabled) {
  this.disabled_ = disabled;

  if (this.disabled_) {
    this.isCursorInTools_ = false;
    this.show_(true);
    this.clearTimeout_();
  } else {
    this.kick();
  }
};

/**
 * Sets cursor's state as out of tools. Mouseout event is not dispatched for
 * some cases even when mouse cursor goes out of elements. This method is used
 * to handle these cases manually.
 */
DimmableUIController.prototype.setCursorOutOfTools = function() {
  this.isCursorInTools_ = false;
};
