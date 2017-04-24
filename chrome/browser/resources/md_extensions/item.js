// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /** @interface */
  var ItemDelegate = function() {};

  ItemDelegate.prototype = {
    /** @param {string} id */
    deleteItem: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} isEnabled
     */
    setItemEnabled: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} isAllowedIncognito
     */
    setItemAllowedIncognito: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} isAllowedOnFileUrls
     */
    setItemAllowedOnFileUrls: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} isAllowedOnAllSites
     */
    setItemAllowedOnAllSites: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} collectsErrors
     */
    setItemCollectsErrors: assertNotReached,

    /**
     * @param {string} id
     * @param {chrome.developerPrivate.ExtensionView} view
     */
    inspectItemView: assertNotReached,

    /** @param {string} id */
    reloadItem: assertNotReached,

    /** @param {string} id */
    repairItem: assertNotReached,

    /** @param {string} id */
    showItemOptionsPage: assertNotReached,
  };

  var Item = Polymer({
    is: 'extensions-item',

    behaviors: [I18nBehavior],

    properties: {
      // The item's delegate, or null.
      delegate: {
        type: Object,
      },

      // Whether or not dev mode is enabled.
      inDevMode: {
        type: Boolean,
        value: false,
      },

      // The underlying ExtensionInfo itself. Public for use in declarative
      // bindings.
      /** @type {chrome.developerPrivate.ExtensionInfo} */
      data: {
        type: Object,
      },

      // Whether or not the expanded view of the item is shown.
      /** @private */
      showingDetails_: {
        type: Boolean,
        value: false,
      },
    },

    observers: [
      'observeIdVisibility_(inDevMode, showingDetails_, data.id)',
    ],

    /** @private */
    observeIdVisibility_: function(inDevMode, showingDetails, id) {
      Polymer.dom.flush();
      var idElement = this.$$('#extension-id');
      if (idElement) {
        assert(this.data);
        idElement.innerHTML = this.i18n('itemId', this.data.id);
      }
      this.fire('extension-item-size-changed', {item: this.data});
    },

    /**
     * @return {boolean}
     * @private
     */
    computeErrorsHidden_: function() {
      return !this.data.manifestErrors.length &&
             !this.data.runtimeErrors.length;
    },

    /** @private */
    onRemoveTap_: function() {
      this.delegate.deleteItem(this.data.id);
    },

    /** @private */
    onEnableChange_: function() {
      this.delegate.setItemEnabled(this.data.id,
                                   this.$['enable-toggle'].checked);
    },

    /** @private */
    onErrorsTap_: function() {
      this.fire('extension-item-show-errors', {data: this.data});
    },

    /** @private */
    onDetailsTap_: function() {
      this.fire('extension-item-show-details', {data: this.data});
    },

    /**
     * @param {!{model: !{item: !chrome.developerPrivate.ExtensionView}}} e
     * @private
     */
    onInspectTap_: function(e) {
      this.delegate.inspectItemView(this.data.id, this.data.views[0]);
    },

    /** @private */
    onExtraInspectTap_: function() {
      this.fire('extension-item-show-details', {data: this.data});
    },

    /** @private */
    onReloadTap_: function() {
      this.delegate.reloadItem(this.data.id);
    },

    /** @private */
    onRepairTap_: function() {
      this.delegate.repairItem(this.data.id);
    },

    /**
     * Returns true if the extension is enabled, including terminated
     * extensions.
     * @return {boolean}
     * @private
     */
    isEnabled_: function() {
      switch (this.data.state) {
        case chrome.developerPrivate.ExtensionState.ENABLED:
        case chrome.developerPrivate.ExtensionState.TERMINATED:
          return true;
        case chrome.developerPrivate.ExtensionState.DISABLED:
          return false;
      }
      assertNotReached();  // FileNotFound.
    },

    /**
     * Returns true if the enable toggle should be shown.
     * @return {boolean}
     * @private
     */
    showEnableToggle_: function() {
      return !this.isTerminated_() && !this.data.disableReasons.corruptInstall;
    },

    /**
     * Returns true if the extension is in the terminated state.
     * @return {boolean}
     * @private
     */
    isTerminated_: function() {
      return this.data.state ==
          chrome.developerPrivate.ExtensionState.TERMINATED;
    },

    /**
     * return {string}
     * @private
     */
    computeClasses_: function() {
      var classes = this.isEnabled_() ? 'enabled' : 'disabled';
      if (this.inDevMode)
        classes += ' dev-mode';
      return classes;
    },

    /**
     * @return {string}
     * @private
     */
    computeSourceIndicatorIcon_: function() {
      switch (extensions.getItemSource(this.data)) {
        case SourceType.POLICY:
          return 'communication:business';
        case SourceType.SIDELOADED:
          return 'input';
        case SourceType.UNPACKED:
          return 'extensions-icons:unpacked';
        case SourceType.WEBSTORE:
          return '';
      }
      assertNotReached();
    },

    /**
     * @return {string}
     * @private
     */
    computeSourceIndicatorText_: function() {
      var sourceType = extensions.getItemSource(this.data);
      return sourceType == SourceType.WEBSTORE ? '' :
             extensions.getItemSourceString(sourceType);
    },

    /**
     * @return {boolean}
     * @private
     */
    computeInspectViewsHidden_: function() {
      return !this.data.views || this.data.views.length == 0;
    },

    /**
     * @return {string}
     * @private
     */
    computeFirstInspectLabel_: function() {
      var view = this.data.views[0];
      // Trim the "chrome-extension://<id>/".
      var url = new URL(view.url);
      var label = view.url;
      if (url.protocol == 'chrome-extension:')
        label = url.pathname.substring(1);
      if (label == '_generated_background_page.html')
        label = this.i18n('viewBackgroundPage');
      // Add any qualifiers.
      label += (view.incognito ? ' ' + this.i18n('viewIncognito') : '') +
               (view.renderProcessId == -1 ?
                    ' ' + this.i18n('viewInactive') : '') +
               (view.isIframe ? ' ' + this.i18n('viewIframe') : '');
      var index = this.data.views.indexOf(view);
      assert(index >= 0);
      if (index < this.data.views.length - 1)
        label += ',';
      return label;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeExtraViewsHidden_: function() {
      return this.data.views.length <= 1;
    },

    /**
     * @return {string}
     * @private
     */
    computeExtraInspectLabel_: function() {
      return loadTimeData.getStringF('itemInspectViewsExtra',
                                     this.data.views.length - 1);
    },

    /**
     * @return {boolean}
     * @private
     */
    hasWarnings_: function() {
      return this.data.disableReasons.corruptInstall ||
             this.data.disableReasons.suspiciousInstall ||
             !!this.data.blacklistText;
    },

    /**
     * @return {string}
     * @private
     */
    computeWarningsClasses_: function() {
      return this.data.blacklistText ? 'severe' : 'mild';
    },
  });

  return {
    Item: Item,
    ItemDelegate: ItemDelegate,
  };
});

