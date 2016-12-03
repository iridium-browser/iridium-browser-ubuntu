// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{device: string,
 *           lastUpdateTime: string,
 *           opened: boolean,
 *           separatorIndexes: !Array<number>,
 *           timestamp: number,
 *           tabs: !Array<!ForeignSessionTab>,
 *           tag: string}}
 */
var ForeignDeviceInternal;

Polymer({
  is: 'history-synced-device-manager',

  properties: {
    /**
     * @type {?Array<!ForeignSession>}
     */
    sessionList: {
      type: Array,
      observer: 'updateSyncedDevices'
    },

    searchTerm: {
      type: String,
      observer: 'searchTermChanged'
    },

    /**
     * An array of synced devices with synced tab data.
     * @type {!Array<!ForeignDeviceInternal>}
     */
    syncedDevices_: {
      type: Array,
      value: function() { return []; }
    },

    /** @private */
    signInState: {
      type: Boolean,
      observer: 'signInStateChanged_',
    },

    /** @private */
    guestSession_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isGuestSession'),
    },

    /** @private */
    fetchingSyncedTabs_: {
      type: Boolean,
      value: false,
    },

    hasSeenForeignData_: Boolean,
  },

  listeners: {
    'toggle-menu': 'onToggleMenu_',
    'scroll': 'onListScroll_'
  },

  /** @override */
  attached: function() {
    // Update the sign in state.
    chrome.send('otherDevicesInitialized');
    md_history.BrowserService.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.INITIALIZED,
        SyncedTabsHistogram.LIMIT);
  },

  /** @return {HTMLElement} */
  getContentScrollTarget: function() { return this; },

  /**
   * @param {!ForeignSession} session
   * @return {!ForeignDeviceInternal}
   */
  createInternalDevice_: function(session) {
    var tabs = [];
    var separatorIndexes = [];
    for (var i = 0; i < session.windows.length; i++) {
      var windowId = session.windows[i].sessionId;
      var newTabs = session.windows[i].tabs;
      if (newTabs.length == 0)
        continue;

      newTabs.forEach(function(tab) {
        tab.windowId = windowId;
      });

      var windowAdded = false;
      if (!this.searchTerm) {
        // Add all the tabs if there is no search term.
        tabs = tabs.concat(newTabs);
        windowAdded = true;
      } else {
        var searchText = this.searchTerm.toLowerCase();
        for (var j = 0; j < newTabs.length; j++) {
          var tab = newTabs[j];
          if (tab.title.toLowerCase().indexOf(searchText) != -1) {
            tabs.push(tab);
            windowAdded = true;
          }
        }
      }
      if (windowAdded && i != session.windows.length - 1)
        separatorIndexes.push(tabs.length - 1);
    }
    return {
      device: session.name,
      lastUpdateTime: '– ' + session.modifiedTime,
      opened: true,
      separatorIndexes: separatorIndexes,
      timestamp: session.timestamp,
      tabs: tabs,
      tag: session.tag,
    };
  },

  onSignInTap_: function() {
    chrome.send('startSignInFlow');
  },

  onListScroll_: function() {
    var menu = this.$.menu.getIfExists();
    if (menu)
      menu.closeMenu();
  },

  onToggleMenu_: function(e) {
    this.$.menu.get().then(function(menu) {
      menu.toggleMenu(e.detail.target, e.detail.tag);
      if (menu.menuOpen) {
        md_history.BrowserService.getInstance().recordHistogram(
            SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.SHOW_SESSION_MENU,
            SyncedTabsHistogram.LIMIT);
      }
    });
  },

  onOpenAllTap_: function() {
    var menu = assert(this.$.menu.getIfExists());
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.OPEN_ALL,
        SyncedTabsHistogram.LIMIT);
    browserService.openForeignSessionAllTabs(
        menu.itemData);
    menu.closeMenu();
  },

  onDeleteSessionTap_: function() {
    var menu = assert(this.$.menu.getIfExists());
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HIDE_FOR_NOW,
        SyncedTabsHistogram.LIMIT);
    browserService.deleteForeignSession(menu.itemData);
    menu.closeMenu();
  },

  /** @private */
  clearDisplayedSyncedDevices_: function() {
    this.syncedDevices_ = [];
  },

  /**
   * Decide whether or not should display no synced tabs message.
   * @param {boolean} signInState
   * @param {number} syncedDevicesLength
   * @param {boolean} guestSession
   * @return {boolean}
   */
  showNoSyncedMessage: function(
      signInState, syncedDevicesLength, guestSession) {
    if (guestSession)
      return true;

    return signInState && syncedDevicesLength == 0;
  },

  /**
   * Shows the signin guide when the user is not signed in and not in a guest
   * session.
   * @param {boolean} signInState
   * @param {boolean} guestSession
   * @return {boolean}
   */
  showSignInGuide: function(signInState, guestSession) {
    var show = !signInState && !guestSession;
    if (show) {
      md_history.BrowserService.getInstance().recordAction(
          'Signin_Impression_FromRecentTabs');
    }

    return show;
  },

  /**
   * Decide what message should be displayed when user is logged in and there
   * are no synced tabs.
   * @param {boolean} fetchingSyncedTabs
   * @return {string}
   */
  noSyncedTabsMessage: function(fetchingSyncedTabs) {
    return loadTimeData.getString(
        fetchingSyncedTabs ? 'loading' : 'noSyncedResults');
  },

  /**
   * Replaces the currently displayed synced tabs with |sessionList|. It is
   * common for only a single session within the list to have changed, We try to
   * avoid doing extra work in this case. The logic could be more intelligent
   * about updating individual tabs rather than replacing whole sessions, but
   * this approach seems to have acceptable performance.
   * @param {?Array<!ForeignSession>} sessionList
   */
  updateSyncedDevices: function(sessionList) {
    this.fetchingSyncedTabs_ = false;

    if (!sessionList)
      return;

    if (sessionList.length > 0 && !this.hasSeenForeignData_) {
      this.hasSeenForeignData_ = true;
      md_history.BrowserService.getInstance().recordHistogram(
        SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HAS_FOREIGN_DATA,
        SyncedTabsHistogram.LIMIT);
    }

    // First, update any existing devices that have changed.
    var updateCount = Math.min(sessionList.length, this.syncedDevices_.length);
    for (var i = 0; i < updateCount; i++) {
      var oldDevice = this.syncedDevices_[i];
      if (oldDevice.tag != sessionList[i].tag ||
          oldDevice.timestamp != sessionList[i].timestamp) {
        this.splice(
            'syncedDevices_', i, 1, this.createInternalDevice_(sessionList[i]));
      }
    }

    if (sessionList.length >= this.syncedDevices_.length) {
      // The list grew; append new items.
      for (var i = updateCount; i < sessionList.length; i++) {
        this.push('syncedDevices_', this.createInternalDevice_(sessionList[i]));
      }
    } else {
      // The list shrank; remove deleted items.
      this.splice(
          'syncedDevices_', updateCount,
          this.syncedDevices_.length - updateCount);
    }
  },

  /**
   * End fetching synced tabs when sync is disabled.
   */
  tabSyncDisabled: function() {
    this.fetchingSyncedTabs_ = false;
    this.clearDisplayedSyncedDevices_();
  },

  /**
   * Get called when user's sign in state changes, this will affect UI of synced
   * tabs page. Sign in promo gets displayed when user is signed out, and
   * different messages are shown when there are no synced tabs.
   */
  signInStateChanged_: function() {
    this.fire('history-view-changed');

    // User signed out, clear synced device list and show the sign in promo.
    if (!this.signInState) {
      this.clearDisplayedSyncedDevices_();
      return;
    }
    // User signed in, show the loading message when querying for synced
    // devices.
    this.fetchingSyncedTabs_ = true;
  },

  searchTermChanged: function(searchTerm) {
    this.clearDisplayedSyncedDevices_();
    this.updateSyncedDevices(this.sessionList);
  }
});
