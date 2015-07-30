// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   guid: string,
 *   label: string,
 *   sublabel: string,
 *   isLocal: boolean,
 *   isCached: boolean
 * }}
 * @see chrome/browser/ui/webui/options/autofill_options_handler.cc
 */
var AutofillEntityMetadata;

cr.define('options.autofillOptions', function() {
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;

  /**
   * @return {!HTMLButtonElement}
   */
  function AutofillEditProfileButton(edit) {
    var editButtonEl = /** @type {HTMLButtonElement} */(
        document.createElement('button'));
    editButtonEl.className =
        'list-inline-button hide-until-hover custom-appearance';
    editButtonEl.textContent =
        loadTimeData.getString('autofillEditProfileButton');
    editButtonEl.onclick = edit;

    editButtonEl.onmousedown = function(e) {
      // Don't select the row when clicking the button.
      e.stopPropagation();
      // Don't focus on the button when clicking it.
      e.preventDefault();
    };

    return editButtonEl;
  }

  /** @return {!Element} */
  function CreateGoogleAccountLabel() {
    var label = document.createElement('div');
    label.className = 'deemphasized hides-on-hover';
    label.textContent = loadTimeData.getString('autofillFromGoogleAccount');
    return label;
  }

  /**
   * Creates a new address list item.
   * @constructor
   * @param {AutofillEntityMetadata} metadata Details about an address profile.
   * @extends {options.DeletableItem}
   * @see chrome/browser/ui/webui/options/autofill_options_handler.cc
   */
  function AddressListItem(metadata) {
    var el = cr.doc.createElement('div');
    el.__proto__ = AddressListItem.prototype;
    /** @private */
    el.metadata_ = metadata;
    el.decorate();

    return el;
  }

  AddressListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var label = this.ownerDocument.createElement('div');
      label.className = 'autofill-list-item';
      label.textContent = this.metadata_.label;
      this.contentElement.appendChild(label);

      var sublabel = this.ownerDocument.createElement('div');
      sublabel.className = 'deemphasized';
      sublabel.textContent = this.metadata_.sublabel;
      this.contentElement.appendChild(sublabel);

      if (!this.metadata_.isLocal) {
        this.deletable = false;
        this.contentElement.appendChild(CreateGoogleAccountLabel());
      }

      // The 'Edit' button.
      var metadata = this.metadata_;
      var editButtonEl = AutofillEditProfileButton(
          AddressListItem.prototype.loadAddressEditor.bind(this));
      this.contentElement.appendChild(editButtonEl);
    },

    /**
     * For local Autofill data, this function causes the AutofillOptionsHandler
     * to call showEditAddressOverlay(). For Wallet data, the user is
     * redirected to the Wallet web interface.
     */
    loadAddressEditor: function() {
      if (this.metadata_.isLocal)
        chrome.send('loadAddressEditor', [this.metadata_.guid]);
      else
        window.open(loadTimeData.getString('manageWalletAddressesUrl'));
    },
  };

  /**
   * Creates a new credit card list item.
   * @param {AutofillEntityMetadata} metadata Details about a credit card.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function CreditCardListItem(metadata) {
    var el = cr.doc.createElement('div');
    el.__proto__ = CreditCardListItem.prototype;
    /** @private */
    el.metadata_ = metadata;
    el.decorate();

    return el;
  }

  CreditCardListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var label = this.ownerDocument.createElement('div');
      label.className = 'autofill-list-item';
      label.textContent = this.metadata_.label;
      this.contentElement.appendChild(label);

      var sublabel = this.ownerDocument.createElement('div');
      sublabel.className = 'deemphasized';
      sublabel.textContent = this.metadata_.sublabel;
      this.contentElement.appendChild(sublabel);

      if (!this.metadata_.isLocal) {
        this.deletable = false;
        this.contentElement.appendChild(CreateGoogleAccountLabel());
      }

      var guid = this.metadata_.guid;
      if (this.metadata_.isCached) {
        var localCopyText = this.ownerDocument.createElement('span');
        localCopyText.className = 'hide-until-hover deemphasized';
        localCopyText.textContent =
            loadTimeData.getString('autofillDescribeLocalCopy');
        this.contentElement.appendChild(localCopyText);

        var clearLocalCopyButton = AutofillEditProfileButton(
            function() { chrome.send('clearLocalCardCopy', [guid]); });
        clearLocalCopyButton.textContent =
            loadTimeData.getString('autofillClearLocalCopyButton');
        this.contentElement.appendChild(clearLocalCopyButton);
      }

      // The 'Edit' button.
      var metadata = this.metadata_;
      var editButtonEl = AutofillEditProfileButton(
          CreditCardListItem.prototype.loadCreditCardEditor.bind(this));
      this.contentElement.appendChild(editButtonEl);
    },

    /**
     * For local Autofill data, this function causes the AutofillOptionsHandler
     * to call showEditCreditCardOverlay(). For Wallet data, the user is
     * redirected to the Wallet web interface.
     */
    loadCreditCardEditor: function() {
      if (this.metadata_.isLocal)
        chrome.send('loadCreditCardEditor', [this.metadata_.guid]);
      else
        window.open(loadTimeData.getString('manageWalletPaymentMethodsUrl'));
    },
  };

  /**
   * Creates a new value list item.
   * @param {options.autofillOptions.AutofillValuesList} list The parent list of
   *     this item.
   * @param {string} entry A string value.
   * @constructor
   * @extends {options.InlineEditableItem}
   */
  function ValuesListItem(list, entry) {
    var el = cr.doc.createElement('div');
    el.list = list;
    el.value = entry ? entry : '';
    el.__proto__ = ValuesListItem.prototype;
    el.decorate();

    return el;
  }

  ValuesListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /** @override */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      // Note: This must be set prior to calling |createEditableTextCell|.
      this.isPlaceholder = !this.value;

      // The stored value.
      var cell = this.createEditableTextCell(String(this.value));
      this.contentElement.appendChild(cell);
      this.input = cell.querySelector('input');

      if (this.isPlaceholder) {
        this.input.placeholder = this.list.getAttribute('placeholder');
        this.deletable = false;
      }

      this.addEventListener('commitedit', this.onEditCommitted_);
      this.closeButtonFocusAllowed = true;
      this.setFocusableColumnIndex(this.input, 0);
      this.setFocusableColumnIndex(this.closeButtonElement, 1);
    },

    /**
     * @return {Array} This item's value.
     * @protected
     */
    value_: function() {
      return this.input.value;
    },

    /**
     * @param {*} value The value to test.
     * @return {boolean} True if the given value is non-empty.
     * @protected
     */
    valueIsNonEmpty_: function(value) {
      return !!value;
    },

    /**
     * @return {boolean} True if value1 is logically equal to value2.
     */
    valuesAreEqual_: function(value1, value2) {
      return value1 === value2;
    },

    /**
     * Clears the item's value.
     * @protected
     */
    clearValue_: function() {
      this.input.value = '';
    },

    /**
     * Called when committing an edit.
     * If this is an "Add ..." item, committing a non-empty value adds that
     * value to the end of the values list, but also leaves this "Add ..." item
     * in place.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      var value = this.value_();
      var i = this.list.items.indexOf(this);
      if (i < this.list.dataModel.length &&
          this.valuesAreEqual_(value, this.list.dataModel.item(i))) {
        return;
      }

      var entries = this.list.dataModel.slice();
      if (this.valueIsNonEmpty_(value) &&
          !entries.some(this.valuesAreEqual_.bind(this, value))) {
        // Update with new value.
        if (this.isPlaceholder) {
          // It is important that updateIndex is done before validateAndSave.
          // Otherwise we can not be sure about AddRow index.
          this.list.ignoreChangeEvents(function() {
            this.list.dataModel.updateIndex(i);
          }.bind(this));
          this.list.validateAndSave(i, 0, value);
        } else {
          this.list.validateAndSave(i, 1, value);
        }
      } else {
        // Reject empty values and duplicates.
        if (!this.isPlaceholder) {
          this.list.ignoreChangeEvents(function() {
            this.list.dataModel.splice(i, 1);
          }.bind(this));
          this.list.selectIndexWithoutFocusing(i);
        } else {
          this.clearValue_();
        }
      }
    },
  };

  /**
   * Creates a new name value list item.
   * @param {options.autofillOptions.AutofillNameValuesList} list The parent
   *     list of this item.
   * @param {Array<string>} entry An array of [first, middle, last] names.
   * @constructor
   * @extends {options.autofillOptions.ValuesListItem}
   */
  function NameListItem(list, entry) {
    var el = cr.doc.createElement('div');
    el.list = list;
    el.first = entry ? entry[0] : '';
    el.middle = entry ? entry[1] : '';
    el.last = entry ? entry[2] : '';
    el.__proto__ = NameListItem.prototype;
    el.decorate();

    return el;
  }

  NameListItem.prototype = {
    __proto__: ValuesListItem.prototype,

    /** @override */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      // Note: This must be set prior to calling |createEditableTextCell|.
      this.isPlaceholder = !this.first && !this.middle && !this.last;

      // The stored value.
      // For the simulated static "input element" to display correctly, the
      // value must not be empty.  We use a space to force the UI to render
      // correctly when the value is logically empty.
      var cell = this.createEditableTextCell(this.first);
      this.contentElement.appendChild(cell);
      this.firstNameInput = cell.querySelector('input');

      cell = this.createEditableTextCell(this.middle);
      this.contentElement.appendChild(cell);
      this.middleNameInput = cell.querySelector('input');

      cell = this.createEditableTextCell(this.last);
      this.contentElement.appendChild(cell);
      this.lastNameInput = cell.querySelector('input');

      if (this.isPlaceholder) {
        this.firstNameInput.placeholder =
            loadTimeData.getString('autofillAddFirstNamePlaceholder');
        this.middleNameInput.placeholder =
            loadTimeData.getString('autofillAddMiddleNamePlaceholder');
        this.lastNameInput.placeholder =
            loadTimeData.getString('autofillAddLastNamePlaceholder');
        this.deletable = false;
      }

      this.addEventListener('commitedit', this.onEditCommitted_);
    },

    /** @override */
    value_: function() {
      return [this.firstNameInput.value,
              this.middleNameInput.value,
              this.lastNameInput.value];
    },

    /** @override */
    valueIsNonEmpty_: function(value) {
      return value[0] || value[1] || value[2];
    },

    /** @override */
    valuesAreEqual_: function(value1, value2) {
      // First, check for null values.
      if (!value1 || !value2)
        return value1 == value2;

      return value1[0] === value2[0] &&
             value1[1] === value2[1] &&
             value1[2] === value2[2];
    },

    /** @override */
    clearValue_: function() {
      this.firstNameInput.value = '';
      this.middleNameInput.value = '';
      this.lastNameInput.value = '';
    },
  };

  /**
   * Base class for shared implementation between address and credit card lists.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutofillProfileList = cr.ui.define('list');

  AutofillProfileList.prototype = {
    __proto__: DeletableItemList.prototype,

    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);

      this.addEventListener('blur', this.onBlur_);
    },

    /**
     * When the list loses focus, unselect all items in the list.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
    },
  };

  /**
   * Create a new address list.
   * @constructor
   * @extends {options.autofillOptions.AutofillProfileList}
   */
  var AutofillAddressList = cr.ui.define('list');

  AutofillAddressList.prototype = {
    __proto__: AutofillProfileList.prototype,

    decorate: function() {
      AutofillProfileList.prototype.decorate.call(this);
    },

    /** @override */
    activateItemAtIndex: function(index) {
      this.getListItemByIndex(index).loadAddressEditor();
    },

    /**
     * @override
     * @param {AutofillEntityMetadata} metadata
     */
    createItem: function(metadata) {
      return new AddressListItem(metadata);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      AutofillOptions.removeData(this.dataModel.item(index).guid,
                                 'Options_AutofillAddressDeleted');
    },
  };

  /**
   * Create a new credit card list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var AutofillCreditCardList = cr.ui.define('list');

  AutofillCreditCardList.prototype = {
    __proto__: AutofillProfileList.prototype,

    decorate: function() {
      AutofillProfileList.prototype.decorate.call(this);
    },

    /** @override */
    activateItemAtIndex: function(index) {
      this.getListItemByIndex(index).loadCreditCardEditor();
    },

    /**
     * @override
     * @param {AutofillEntityMetadata} metadata
     */
    createItem: function(metadata) {
      return new CreditCardListItem(metadata);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      AutofillOptions.removeData(this.dataModel.item(index).guid,
                                 'Options_AutofillCreditCardDeleted');
    },
  };

  /**
   * Create a new value list.
   * @constructor
   * @extends {options.InlineEditableItemList}
   */
  var AutofillValuesList = cr.ui.define('list');

  AutofillValuesList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /** @override */
    createItem: function(entry) {
      assert(entry === null || typeof entry == 'string');
      return new ValuesListItem(this, entry);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      this.dataModel.splice(index, 1);
    },

    /** @override */
    shouldFocusPlaceholderOnEditCommit: function() {
      return false;
    },

    /**
     * Called when a new list item should be validated; subclasses are
     * responsible for implementing if validation is required.
     * @param {number} index The index of the item that was inserted or changed.
     * @param {number} remove The number items to remove.
     * @param {string} value The value of the item to insert.
     */
    validateAndSave: function(index, remove, value) {
      this.ignoreChangeEvents(function() {
        this.dataModel.splice(index, remove, value);
      }.bind(this));
      this.selectIndexWithoutFocusing(index);
    },
  };

  /**
   * Create a new value list for phone number validation.
   * @constructor
   * @extends {options.autofillOptions.AutofillValuesList}
   */
  var AutofillNameValuesList = cr.ui.define('list');

  AutofillNameValuesList.prototype = {
    __proto__: AutofillValuesList.prototype,

    /**
     * @override
     * @param {?string|Array<string>} entry
     */
    createItem: function(entry) {
      var arrayOrNull = entry ? assertInstanceof(entry, Array) : null;
      return new NameListItem(this, arrayOrNull);
    },
  };

  /**
   * Create a new value list for phone number validation.
   * @constructor
   * @extends {options.autofillOptions.AutofillValuesList}
   */
  var AutofillPhoneValuesList = cr.ui.define('list');

  AutofillPhoneValuesList.prototype = {
    __proto__: AutofillValuesList.prototype,

    /** @override */
    validateAndSave: function(index, remove, value) {
      var numbers = this.dataModel.slice(0, this.dataModel.length - 1);
      numbers.splice(index, remove, value);
      var info = new Array();
      info[0] = index;
      info[1] = numbers;
      info[2] = document.querySelector(
          '#autofill-edit-address-overlay [field=country]').value;
      this.validationRequests_++;
      chrome.send('validatePhoneNumbers', info);
    },

    /**
     * The number of ongoing validation requests.
     * @type {number}
     * @private
     */
    validationRequests_: 0,

    /**
     * Pending Promise resolver functions.
     * @type {Array<!Function>}
     * @private
     */
    validationPromiseResolvers_: [],

    /**
     * This should be called when a reply of chrome.send('validatePhoneNumbers')
     * is received.
     */
    didReceiveValidationResult: function() {
      this.validationRequests_--;
      assert(this.validationRequests_ >= 0);
      if (this.validationRequests_ <= 0) {
        while (this.validationPromiseResolvers_.length) {
          this.validationPromiseResolvers_.pop()();
        }
      }
    },

    /**
     * Returns a Promise which is fulfilled when all of validation requests are
     * completed.
     * @return {!Promise} A promise.
     */
    doneValidating: function() {
      if (this.validationRequests_ <= 0)
        return Promise.resolve();
      return new Promise(function(resolve) {
        this.validationPromiseResolvers_.push(resolve);
      }.bind(this));
    }
  };

  return {
    AutofillProfileList: AutofillProfileList,
    AddressListItem: AddressListItem,
    CreditCardListItem: CreditCardListItem,
    ValuesListItem: ValuesListItem,
    NameListItem: NameListItem,
    AutofillAddressList: AutofillAddressList,
    AutofillCreditCardList: AutofillCreditCardList,
    AutofillValuesList: AutofillValuesList,
    AutofillNameValuesList: AutofillNameValuesList,
    AutofillPhoneValuesList: AutofillPhoneValuesList,
  };
});
