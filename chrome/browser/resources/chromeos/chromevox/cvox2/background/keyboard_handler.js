// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox keyboard handler.
 */

goog.provide('BackgroundKeyboardHandler');

goog.require('cvox.ChromeVoxKbHandler');

/** @constructor */
BackgroundKeyboardHandler = function() {
  // Classic keymap.
  cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromDefaults();

  /** @type {number} @private */
  this.passThroughKeyUpCount_ = 0;

  document.addEventListener('keydown', this.onKeyDown.bind(this), false);
  document.addEventListener('keyup', this.onKeyUp.bind(this), false);
};

BackgroundKeyboardHandler.prototype = {
  /**
   * Handles key down events.
   * @param {Event} evt The key down event to process.
   * @return {boolean} True if the default action should be performed.
   */
  onKeyDown: function(evt) {
    evt.stickyMode = cvox.ChromeVox.isStickyModeOn() && cvox.ChromeVox.isActive;
    if (cvox.ChromeVox.passThroughMode)
      return false;

    if (ChromeVoxState.instance.mode != ChromeVoxMode.CLASSIC &&
        !cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt)) {
      evt.preventDefault();
      evt.stopPropagation();
    }
    Output.flushNextSpeechUtterance();
    return false;
  },

  /**
   * Handles key up events.
   * @param {Event} evt The key down event to process.
   * @return {boolean} True if the default action should be performed.
   */
  onKeyUp: function(evt) {
    // Reset pass through mode once a keyup (not involving the pass through key)
    // is seen. The pass through command involves three keys.
    if (cvox.ChromeVox.passThroughMode) {
      if (this.passThroughKeyUpCount_ >= 3) {
        cvox.ChromeVox.passThroughMode = false;
        this.passThroughKeyUpCount_ = 0;
      } else {
        this.passThroughKeyUpCount_++;
      }
    }
    return false;
  },

  /**
   * React to mode changes.
   * @param {ChromeVoxMode} newMode
   * @param {ChromeVoxMode?} oldMode
   */
  onModeChanged: function(newMode, oldMode) {
    if (newMode == ChromeVoxMode.CLASSIC) {
      chrome.accessibilityPrivate.setKeyboardListener(false, false);
    } else {
      chrome.accessibilityPrivate.setKeyboardListener(
          true, cvox.ChromeVox.isStickyPrefOn);
    }

    // Switching out of next, force next, or uninitialized (on startup).
    if (newMode === ChromeVoxMode.NEXT ||
        newMode === ChromeVoxMode.FORCE_NEXT) {
      window['prefs'].switchToKeyMap('keymap_next');
    } else {
      // Moving from next to classic/compat should be the only case where
      // keymaps get reset. Note the classic <-> compat switches should preserve
      // keymaps especially if a user selected a different one.
      if (oldMode &&
          oldMode != ChromeVoxMode.CLASSIC &&
          oldMode != ChromeVoxMode.COMPAT) {
        // The user's configured key map gets wiped here; this is consistent
        // with previous behavior when switching keymaps.
        window['prefs'].switchToKeyMap('keymap_next');
      }
    }
  }
};
