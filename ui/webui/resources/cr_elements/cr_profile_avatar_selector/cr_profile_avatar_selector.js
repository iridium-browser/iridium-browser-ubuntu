// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-profile-avatar-selector' is an element that displays
 * profile avatar icons and allows an avatar to be selected.
 */

/** @typedef {{url: string, label: string}} */
var AvatarIcon;

Polymer({
  is: 'cr-profile-avatar-selector',

  properties: {
    /**
     * The list of profile avatar URLs and labels.
     * @type {!Array<!AvatarIcon>}
     */
    avatars: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * The currently selected profile avatar URL. May be a data URI.
     * @type {string}
     */
    selectedAvatarUrl: {type: String, notify: true},

    ignoreModifiedKeyEvents: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS imageset for multiple scale factors.
   * @private
   */
  getIconImageset_: function(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },
});
