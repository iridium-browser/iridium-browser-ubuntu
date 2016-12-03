// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for search engine manager WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function SearchEngineManagerWebUITest() {}

SearchEngineManagerWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the search engine manager.
   */
  browsePreload: 'chrome://settings-frame/searchEngines',
};

// See crosbug.com/22673 for OS_CHROMEOS
// See crbug.com/579666 for OS_LINUX
// See crbug.com/638884 for OS_WIN
GEN('#if defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_WIN)');
GEN('#define MAYBE_testOpenSearchEngineManager ' +
        'DISABLED_testOpenSearchEngineManager');
GEN('#else');
GEN('#define MAYBE_testOpenSearchEngineManager testOpenSearchEngineManager');
GEN('#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_WIN)');

// Test opening the search engine manager has correct location.
TEST_F('SearchEngineManagerWebUITest', 'MAYBE_testOpenSearchEngineManager',
       function() {
         assertEquals(this.browsePreload, document.location.href);
       });
