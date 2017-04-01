// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * TestFixture for content options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function ContentOptionsWebUITest() {}

ContentOptionsWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to content options.
   * @override
   */
  browsePreload: 'chrome://settings-frame/content',
};

// Test opening the content options has correct location.
TEST_F('ContentOptionsWebUITest', 'testOpenContentOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});
