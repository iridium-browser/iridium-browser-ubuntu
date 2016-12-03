// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * TestFixture for language options WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function LanguageOptionsWebUITest() {}

LanguageOptionsWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /** @override */
  browsePreload: 'chrome://settings-frame/languages',

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/559266
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        '#language-options-list');

    // Enable when failure is resolved.
    // AX_TEXT_04: http://crbug.com/559271
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose',
        '#languagePage > .content-area > .language-options-header > A');
  }
};

// Test opening language options has correct location.
TEST_F('LanguageOptionsWebUITest', 'testOpenLanguageOptions', function() {
  assertEquals(this.browsePreload, document.location.href);
});

GEN('#if defined(OS_WIN) || defined(OS_CHROMEOS)');
// Test reselecting the same language as the current UI locale. This should show
// a "Chrome is displayed in this language" message rather than a restart banner
// or a [ Display Chrome in this language ] button.
TEST_F('LanguageOptionsWebUITest', 'reselectUILocale', function() {
  var currentLang = loadTimeData.getString('currentUiLanguageCode');
  $('language-options-list').selectLanguageByCode(currentLang);
  LanguageOptions.uiLanguageSaved(currentLang);

  expectTrue($('language-options-ui-language-button').hidden);
  expectFalse($('language-options-ui-language-message').hidden);
  expectTrue($('language-options-ui-notification-bar').hidden);
});
GEN('#endif');  // defined(OS_WIN) || defined(OS_CHROMEOS)
