// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history', function() {
  var HistoryItem = Polymer({
    is: 'history-item',

    properties: {
      // Underlying HistoryEntry data for this item. Contains read-only fields
      // from the history backend, as well as fields computed by history-list.
      item: {type: Object, observer: 'showIcon_'},

      // Search term used to obtain this history-item.
      searchTerm: {type: String},

      selected: {type: Boolean, notify: true},

      isFirstItem: {type: Boolean, reflectToAttribute: true},

      isCardStart: {type: Boolean, reflectToAttribute: true},

      isCardEnd: {type: Boolean, reflectToAttribute: true},

      // True if the item is being displayed embedded in another element and
      // should not manage its own borders or size.
      embedded: {type: Boolean, reflectToAttribute: true},

      hasTimeGap: {type: Boolean},

      numberOfItems: {type: Number},

      // The path of this history item inside its parent.
      path: String,

      index: Number,
    },

    /**
     * When a history-item is selected the toolbar is notified and increases
     * or decreases its count of selected items accordingly.
     * @param {MouseEvent} e
     * @private
     */
    onCheckboxSelected_: function(e) {
      // TODO(calamity): Fire this event whenever |selected| changes.
      this.fire('history-checkbox-select', {
        element: this,
        shiftKey: e.shiftKey,
      });
      e.preventDefault();
    },

    /**
     * @param {MouseEvent} e
     * @private
     */
    onCheckboxMousedown_: function(e) {
      // Prevent shift clicking a checkbox from selecting text.
      if (e.shiftKey)
        e.preventDefault();
    },

    /**
     * Remove bookmark of current item when bookmark-star is clicked.
     * @private
     */
    onRemoveBookmarkTap_: function() {
      if (!this.item.starred)
        return;

      if (this.$$('#bookmark-star') == this.root.activeElement)
        this.$['menu-button'].focus();

      var browserService = md_history.BrowserService.getInstance();
      browserService.removeBookmark(this.item.url);
      browserService.recordAction('BookmarkStarClicked');

      this.fire('remove-bookmark-stars', this.item.url);
    },

    /**
     * Fires a custom event when the menu button is clicked. Sends the details
     * of the history item and where the menu should appear.
     */
    onMenuButtonTap_: function(e) {
      this.fire('toggle-menu', {
        target: Polymer.dom(e).localTarget,
        index: this.index,
        item: this.item,
        path: this.path,
      });

      // Stops the 'tap' event from closing the menu when it opens.
      e.stopPropagation();
    },

    /**
     * Record metrics when a result is clicked. This is deliberately tied to
   * on-click rather than on-tap, as on-click triggers from middle clicks.
     */
    onLinkClick_: function() {
      var browserService = md_history.BrowserService.getInstance();
      browserService.recordAction('EntryLinkClick');

      if (this.searchTerm)
        browserService.recordAction('SearchResultClick');

      if (this.index == undefined)
        return;

      browserService.recordHistogram(
          'HistoryPage.ClickPosition', this.index, UMA_MAX_BUCKET_VALUE);

      if (this.index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
        browserService.recordHistogram(
            'HistoryPage.ClickPositionSubset', this.index,
            UMA_MAX_SUBSET_BUCKET_VALUE);
      }
    },

    onLinkRightClick_: function() {
      md_history.BrowserService.getInstance().recordAction(
          'EntryLinkRightClick');
    },

    /**
     * Set the favicon image, based on the URL of the history item.
     * @private
     */
    showIcon_: function() {
      this.$.icon.style.backgroundImage =
          cr.icon.getFaviconImageSet(this.item.url);
    },

    selectionNotAllowed_: function() {
      return !loadTimeData.getBoolean('allowDeletingHistory');
    },

    /**
     * Generates the title for this history card.
     * @param {number} numberOfItems The number of items in the card.
     * @param {string} search The search term associated with these results.
     * @private
     */
    cardTitle_: function(numberOfItems, historyDate, search) {
      if (!search)
        return this.item.dateRelativeDay;

      var resultId = numberOfItems == 1 ? 'searchResult' : 'searchResults';
      return loadTimeData.getStringF('foundSearchResults', numberOfItems,
          loadTimeData.getString(resultId), search);
    },

    /**
     * Crop long item titles to reduce their effect on layout performance. See
     * crbug.com/621347.
     * @param {string} title
     * @return {string}
     */
    cropItemTitle_: function(title) {
      return (title.length > TITLE_MAX_LENGTH) ?
          title.substr(0, TITLE_MAX_LENGTH) :
          title;
    }
  });

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   * @param {Array<HistoryEntry>} visits
   * @param {number} currentIndex
   * @param {string} searchedTerm
   * @return {boolean} Whether or not time gap separator is required.
   * @private
   */
  HistoryItem.needsTimeGap = function(visits, currentIndex, searchedTerm) {
    if (currentIndex >= visits.length - 1 || visits.length == 0)
      return false;

    var currentItem = visits[currentIndex];
    var nextItem = visits[currentIndex + 1];

    if (searchedTerm)
      return currentItem.dateShort != nextItem.dateShort;

    return currentItem.time - nextItem.time > BROWSING_GAP_TIME &&
        currentItem.dateRelativeDay == nextItem.dateRelativeDay;
  };

  return { HistoryItem: HistoryItem };
});
