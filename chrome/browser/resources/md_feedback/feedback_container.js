// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element is used as a container for all the feedback
// elements. Based on a number of factors, it determines which elements
// to show and what will be submitted to the feedback servers.
Polymer({
  is: 'feedback-container',

  /**
   * Retrieves the feedback privacy note text, if it exists. On non-officially
   * branded builds, the string is not defined.
   *
   * @return {string} Privacy note text.
   */
  getPrivacyNote_: function() {
    return loadTimeData.valueExists('privacyNote') ?
        this.i18n('privacyNote') : '';
  },
});
