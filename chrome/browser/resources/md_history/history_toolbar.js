// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-toolbar',
  properties: {
    // Number of history items currently selected.
    // TODO(calamity): bind this to
    // listContainer.selectedItem.selectedPaths.length.
    count: {
      type: Number,
      value: 0,
      observer: 'changeToolbarView_',
    },

    // True if 1 or more history items are selected. When this value changes
    // the background colour changes.
    itemsSelected_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    // The most recent term entered in the search field. Updated incrementally
    // as the user types.
    searchTerm: {
      type: String,
      observer: 'searchTermChanged_',
      notify: true,
    },

    // True if the backend is processing and a spinner should be shown in the
    // toolbar.
    spinnerActive: {
      type: Boolean,
      value: false,
    },

    hasDrawer: {
      type: Boolean,
      reflectToAttribute: true,
    },

    // Whether domain-grouped history is enabled.
    isGroupedMode: {
      type: Boolean,
      reflectToAttribute: true,
    },

    // The period to search over. Matches BrowsingHistoryHandler::Range.
    groupedRange: {
      type: Number,
      reflectToAttribute: true,
      notify: true,
    },

    groupedOffset: {
      type: Number,
      notify: true,
    },

    querying: Boolean,

    hasMoreResults: Boolean,

    // The start time of the query range.
    queryStartTime: String,

    // The end time of the query range.
    queryEndTime: String,

    // Whether to show the menu promo (a tooltip that points at the menu button
    // in narrow mode).
    showMenuPromo: Boolean,

    showSyncNotice: Boolean,
  },

  /** @return {CrToolbarSearchFieldElement} */
  get searchField() {
    return /** @type {CrToolbarElement} */ (this.$['main-toolbar'])
        .getSearchField();
  },

  showSearchField: function() {
    this.searchField.showAndFocus();
  },

  /**
   * Changes the toolbar background color depending on whether any history items
   * are currently selected.
   * @private
   */
  changeToolbarView_: function() {
    this.itemsSelected_ = this.count > 0;
  },

  /**
   * When changing the search term externally, update the search field to
   * reflect the new search term.
   * @private
   */
  searchTermChanged_: function() {
    if (this.searchField.getValue() != this.searchTerm) {
      this.searchField.showAndFocus();
      this.searchField.setValue(this.searchTerm);
    }
  },

  /**
   * @param {!CustomEvent} event
   * @private
   */
  onSearchChanged_: function(event) {
    this.searchTerm = /** @type {string} */ (event.detail);
  },

  /** @private */
  onInfoButtonTap_: function() {
    var dropdown = this.$.syncNotice.get();
    dropdown.positionTarget = this.$$('#info-button-icon');
    // It is possible for this listener to trigger while the dialog is
    // closing. Ensure the dialog is fully closed before reopening it.
    if (dropdown.style.display == 'none')
      dropdown.open();
  },

  /** @private */
  onClearSelectionTap_: function() {
    this.fire('unselect-all');
  },

  /** @private */
  onDeleteTap_: function() {
    this.fire('delete-selected');
  },

  /**
   * If the user is a supervised user the delete button is not shown.
   * @private
   */
  deletingAllowed_: function() {
    return loadTimeData.getBoolean('allowDeletingHistory');
  },

  /** @private */
  numberOfItemsSelected_: function(count) {
    return count > 0 ? loadTimeData.getStringF('itemsSelected', count) : '';
  },

  /** @private */
  getHistoryInterval_: function(queryStartTime, queryEndTime) {
    // TODO(calamity): Fix the format of these dates.
    return loadTimeData.getStringF(
        'historyInterval', queryStartTime, queryEndTime);
  },

  /** @private */
  onTodayTap_: function() {
    if (!this.querying)
      this.groupedOffset = 0;
  },

  /** @private */
  onPrevTap_: function() {
    if (!this.querying)
      this.groupedOffset = this.groupedOffset + 1;
  },

  /** @private */
  onNextTap_: function() {
    if (!this.querying)
      this.groupedOffset = this.groupedOffset - 1;
  },

  /**
   * @private
   * @return {boolean}
   */
  isToday_: function() {
    return this.groupedOffset == 0;
  },
});
