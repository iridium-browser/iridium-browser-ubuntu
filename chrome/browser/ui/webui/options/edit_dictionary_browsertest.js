// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['options_browsertest_base.js']);

/**
 * TestFixture for EditDictionaryOverlay WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function EditDictionaryWebUITest() {}

EditDictionaryWebUITest.prototype = {
  __proto__: OptionsBrowsertestBase.prototype,

  /**
   * Browse to the edit dictionary page & call our preLoad().
   */
  browsePreload: 'chrome://settings-frame/editDictionary',

  /**
   * Register a mock dictionary handler.
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(
        ['refreshDictionaryWords',
         'addDictionaryWord',
         'removeDictionaryWord',
        ]);
    this.mockHandler.stubs().refreshDictionaryWords().
        will(callFunction(function() {
          EditDictionaryOverlay.setWordList([]);
        }));
    this.mockHandler.stubs().addDictionaryWord(ANYTHING);
    this.mockHandler.stubs().removeDictionaryWord(ANYTHING);
  },

  /** @override */
  setUp: function() {
    OptionsBrowsertestBase.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_TEXT_01: http://crbug.com/570556
    this.accessibilityAuditConfig.ignoreSelectors(
        'controlsWithoutLabel',
        '#language-dictionary-overlay-word-list > .deletable-item > *');

    var unsupportedAriaAttributeSelectors = [
      '#language-dictionary-overlay-word-list',
      '#language-options-list',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_10: http://crbug.com/570559
    this.accessibilityAuditConfig.ignoreSelectors(
        'unsupportedAriaAttribute',
        unsupportedAriaAttributeSelectors);
  },
};

// Verify that users can add and remove words in the dictionary.
TEST_F('EditDictionaryWebUITest', 'testAddRemoveWords', function() {
  var testWord = 'foo';
  $('language-dictionary-overlay-word-list').querySelector('input').value =
      testWord;

  this.mockHandler.expects(once()).addDictionaryWord([testWord]).
      will(callFunction(function() {
          EditDictionaryOverlay.setWordList([testWord]);
      }));
  var addWordItem = EditDictionaryOverlay.getWordListForTesting().items[0];
  addWordItem.onEditCommitted_({currentTarget: addWordItem});

  this.mockHandler.expects(once()).removeDictionaryWord([testWord]).
      will(callFunction(function() {
          EditDictionaryOverlay.setWordList([]);
      }));
  EditDictionaryOverlay.getWordListForTesting().deleteItemAtIndex(0);
});

// Verify that users can search words in the dictionary.
TEST_F('EditDictionaryWebUITest', 'testSearch', function() {
  EditDictionaryOverlay.setWordList(['foo', 'bar']);
  expectEquals(3, EditDictionaryOverlay.getWordListForTesting().items.length);

  /**
   * @param {Element} el The element to dispatch an event on.
   * @param {string} value The text of the search event.
   */
  var fakeSearchEvent = function(el, value) {
    el.value = value;
    cr.dispatchSimpleEvent(el, 'search');
  };
  var searchField = $('language-dictionary-overlay-search-field');
  fakeSearchEvent(searchField, 'foo');
  expectEquals(2, EditDictionaryOverlay.getWordListForTesting().items.length);

  fakeSearchEvent(searchField, '');
  expectEquals(3, EditDictionaryOverlay.getWordListForTesting().items.length);
});

TEST_F('EditDictionaryWebUITest', 'testNoCloseOnSearchEnter', function() {
  var editDictionaryPage = EditDictionaryOverlay.getInstance();
  assertTrue(editDictionaryPage.visible);
  var searchField = $('language-dictionary-overlay-search-field');
  searchField.dispatchEvent(new KeyboardEvent('keydown', {
    'bubbles': true,
    'cancelable': true,
    'key': 'Enter'
  }));
  assertTrue(editDictionaryPage.visible);
});

// Verify that dictionary shows newly added words that arrived in a
// notification, but ignores duplicate add notifications.
TEST_F('EditDictionaryWebUITest', 'testAddNotification', function() {
  // Begin with an empty dictionary.
  EditDictionaryOverlay.setWordList([]);
  expectEquals(1, EditDictionaryOverlay.getWordListForTesting().items.length);

  // User adds word 'foo'.
  EditDictionaryOverlay.getWordListForTesting().addDictionaryWord_('foo');
  expectEquals(2, EditDictionaryOverlay.getWordListForTesting().items.length);

  // Backend notifies UI that the word 'foo' has been added. UI ignores this
  // notification, because the word is displayed immediately after user added
  // it.
  EditDictionaryOverlay.updateWords(['foo'], []);
  expectEquals(2, EditDictionaryOverlay.getWordListForTesting().items.length);

  // Backend notifies UI that the words 'bar' and 'baz' were added. UI shows
  // these new words.
  EditDictionaryOverlay.updateWords(['bar', 'baz'], []);
  expectEquals(4, EditDictionaryOverlay.getWordListForTesting().items.length);
});

// Verify that dictionary hides newly removed words that arrived in a
// notification, but ignores duplicate remove notifications.
// TODO(crbug.com/631940): Flaky on Win 7.
GEN('#if defined(OS_WIN)');
GEN('#define MAYBE_testRemoveNotification DISABLED_testRemoveNotification');
GEN('#else');
GEN('#define MAYBE_testRemoveNotification testRemoveNotification');
GEN('#endif  // defined(OS_WIN)');
TEST_F('EditDictionaryWebUITest', 'MAYBE_testRemoveNotification', function() {
  // Begin with a dictionary with words 'foo', 'bar', 'baz', and 'baz'. The
  // second instance of 'baz' appears because the user added the word twice.
  // The backend keeps only one copy of the word.
  EditDictionaryOverlay.setWordList(['foo', 'bar', 'baz', 'baz']);
  expectEquals(5, EditDictionaryOverlay.getWordListForTesting().items.length);

  // User deletes the second instance of 'baz'.
  EditDictionaryOverlay.getWordListForTesting().deleteItemAtIndex(3);
  expectEquals(4, EditDictionaryOverlay.getWordListForTesting().items.length);

  // Backend notifies UI that the word 'baz' has been removed. UI ignores this
  // notification.
  EditDictionaryOverlay.updateWords([], ['baz']);
  expectEquals(4, EditDictionaryOverlay.getWordListForTesting().items.length);

  // Backend notifies UI that words 'foo' and 'bar' have been removed. UI
  // removes these words.
  EditDictionaryOverlay.updateWords([], ['foo', 'bar']);
  expectEquals(2, EditDictionaryOverlay.getWordListForTesting().items.length);
});
