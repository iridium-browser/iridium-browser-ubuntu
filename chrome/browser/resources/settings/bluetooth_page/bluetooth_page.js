// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-bluetooth-page' is the settings page for managing bluetooth
 *  properties and devices.
 *
 * Example:
 *    <core-animated-pages>
 *      <settings-bluetooth-page>
 *      </settings-bluetooth-page>
 *      ... other pages ...
 *    </core-animated-pages>
 */

var bluetoothPage = bluetoothPage || {
  /**
   * Set this to provide a fake implementation for testing.
   * @type {Bluetooth}
   */
  bluetoothApiForTest: null,

  /**
   * Set this to provide a fake implementation for testing.
   * @type {BluetoothPrivate}
   */
  bluetoothPrivateApiForTest: null,
};

Polymer({
  is: 'settings-bluetooth-page',

  behaviors: [I18nBehavior, CrScrollableBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    bluetoothEnabled_: {
      type: Boolean,
      value: false,
      observer: 'bluetoothEnabledChanged_',
    },

    /** @private */
    deviceListExpanded_: {
      type: Boolean,
      value: false,
    },

    /**
     * The cached bluetooth adapter state.
     * @type {!chrome.bluetooth.AdapterState|undefined}
     * @private
     */
    adapterState_: Object,

    /**
     * Whether or not a bluetooth device is connected.
     * @private
     */
    deviceConnected_: Boolean,

    /**
     * The ordered list of bluetooth devices.
     * @type {!Array<!chrome.bluetooth.Device>}
     * @private
     */
    deviceList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Reflects the iron-list selecteditem property.
     * @type {!chrome.bluetooth.Device}
     * @private
     */
    selectedItem_: {
      type: Object,
      observer: 'selectedItemChanged_',
    },

    /**
     * Set to the name of the dialog to show. This page uses a single
     * paper-dialog to host one of three dialog elements, 'addDevice',
     * 'pairDevice', or 'connectError'. This allows a seamless transition
     * between dialogs. Note: This property should be set before opening the
     * dialog and setting the property will not itself cause the dialog to open.
     * @private
     */
    dialogId_: String,

    /**
     * Current Pairing device.
     * @type {?chrome.bluetooth.Device|undefined}
     * @private
     */
    pairingDevice_: Object,

    /**
     * Current Pairing event.
     * @type {?chrome.bluetoothPrivate.PairingEvent|undefined}
     * @private
     */
    pairingEvent_: Object,

    /**
     * The translated error message to show when a connect error occurs.
     * @private
     */
    errorMessage_: String,

    /**
     * Interface for bluetooth calls. May be overriden by tests.
     * @type {Bluetooth}
     * @private
     */
    bluetooth: {
      type: Object,
      value: chrome.bluetooth,
    },

    /**
     * Interface for bluetoothPrivate calls. May be overriden by tests.
     * @type {BluetoothPrivate}
     * @private
     */
    bluetoothPrivate: {
      type: Object,
      value: chrome.bluetoothPrivate,
    },
  },

  observers: ['deviceListChanged_(deviceList_.*)'],

  /**
   * Listener for chrome.bluetooth.onAdapterStateChanged events.
   * @type {function(!chrome.bluetooth.AdapterState)|undefined}
   * @private
   */
  bluetoothAdapterStateChangedListener_: undefined,

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceAdded/Changed events.
   * @type {function(!chrome.bluetooth.Device)|undefined}
   * @private
   */
  bluetoothDeviceUpdatedListener_: undefined,

  /**
   * Listener for chrome.bluetooth.onBluetoothDeviceRemoved events.
   * @type {function(!chrome.bluetooth.Device)|undefined}
   * @private
   */
  bluetoothDeviceRemovedListener_: undefined,

  /**
   * Listener for chrome.bluetoothPrivate.onPairing events.
   * @type {function(!chrome.bluetoothPrivate.PairingEvent)|undefined}
   * @private
   */
  bluetoothPrivateOnPairingListener_: undefined,

  /** @override */
  ready: function() {
    if (bluetoothPage.bluetoothApiForTest)
      this.bluetooth = bluetoothPage.bluetoothApiForTest;
    if (bluetoothPage.bluetoothPrivateApiForTest)
      this.bluetoothPrivate = bluetoothPage.bluetoothPrivateApiForTest;
  },

  /** @override */
  attached: function() {
    this.bluetoothAdapterStateChangedListener_ =
        this.onBluetoothAdapterStateChanged_.bind(this);
    this.bluetooth.onAdapterStateChanged.addListener(
        this.bluetoothAdapterStateChangedListener_);

    this.bluetoothDeviceUpdatedListener_ =
        this.onBluetoothDeviceUpdated_.bind(this);
    this.bluetooth.onDeviceAdded.addListener(
        this.bluetoothDeviceUpdatedListener_);
    this.bluetooth.onDeviceChanged.addListener(
        this.bluetoothDeviceUpdatedListener_);

    this.bluetoothDeviceRemovedListener_ =
        this.onBluetoothDeviceRemoved_.bind(this);
    this.bluetooth.onDeviceRemoved.addListener(
        this.bluetoothDeviceRemovedListener_);

    // Request the inital adapter state.
    this.bluetooth.getAdapterState(this.bluetoothAdapterStateChangedListener_);
  },

  /** @override */
  detached: function() {
    if (this.bluetoothAdapterStateChangedListener_) {
      this.bluetooth.onAdapterStateChanged.removeListener(
          this.bluetoothAdapterStateChangedListener_);
    }
    if (this.bluetoothDeviceUpdatedListener_) {
      this.bluetooth.onDeviceAdded.removeListener(
          this.bluetoothDeviceUpdatedListener_);
      this.bluetooth.onDeviceChanged.removeListener(
          this.bluetoothDeviceUpdatedListener_);
    }
    if (this.bluetoothDeviceRemovedListener_) {
      this.bluetooth.onDeviceRemoved.removeListener(
          this.bluetoothDeviceRemovedListener_);
    }
  },

  /** @private */
  bluetoothEnabledChanged_: function() {
    // When bluetooth is enabled, auto-expand the device list.
    if (this.bluetoothEnabled_)
      this.deviceListExpanded_ = true;
  },

  /** @private */
  deviceListChanged_: function() {
    for (let device of this.deviceList_) {
      if (device.connected) {
        this.deviceConnected_ = true;
        return;
      }
    }
    this.deviceConnected_ = false;
  },

  /** @private */
  selectedItemChanged_: function() {
    if (this.selectedItem_)
      this.connectDevice_(this.selectedItem_);
  },

  /**
   * @return {string}
   * @private
   */
  getIcon_: function() {
    if (!this.bluetoothEnabled_)
      return 'settings:bluetooth-disabled';
    if (this.deviceConnected_)
      return 'settings:bluetooth-connected';
    return 'settings:bluetooth';
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_: function() {
    return this.i18n(
        this.bluetoothEnabled_ ? 'bluetoothEnabled' : 'bluetoothDisabled');
  },

  /** @private */
  toggleDeviceListExpanded_: function() {
    this.deviceListExpanded_ = !this.deviceListExpanded_;
  },

  /**
   * @return {boolean} Whether the <iron-collapse> can be shown.
   * @private
   */
  canShowDeviceList_: function() {
    return this.bluetoothEnabled_ && this.deviceListExpanded_;
  },

  /**
   * If bluetooth is enabled, request the complete list of devices and update
   * this.deviceList_.
   * @private
   */
  updateDeviceList_: function() {
    if (!this.bluetoothEnabled_) {
      this.deviceList_ = [];
      return;
    }
    this.bluetooth.getDevices(function(devices) {
      this.deviceList_ = devices;
      this.updateScrollableContents();
    }.bind(this));
  },

  /**
   * Event called when a user action changes the bluetoothEnabled state.
   * @private
   */
  onBluetoothEnabledChange_: function() {
    this.bluetoothPrivate.setAdapterState(
        {powered: this.bluetoothEnabled_}, function() {
          if (chrome.runtime.lastError) {
            console.error(
                'Error enabling bluetooth: ' +
                chrome.runtime.lastError.message);
          }
        });
  },

  /**
   * Process bluetooth.onAdapterStateChanged events.
   * @param {!chrome.bluetooth.AdapterState} state
   * @private
   */
  onBluetoothAdapterStateChanged_: function(state) {
    this.adapterState_ = state;
    this.bluetoothEnabled_ = state.powered;
    this.updateDeviceList_();
  },

  /**
   * Process bluetooth.onDeviceAdded and onDeviceChanged events.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceUpdated_: function(device) {
    var address = device.address;
    if (this.dialogId_ && this.pairingDevice_ &&
        this.pairingDevice_.address == address) {
      this.pairingDevice_ = device;
    }
    var index = this.getDeviceIndex_(address);
    if (index >= 0) {
      this.set('deviceList_.' + index, device);
      return;
    }
    this.push('deviceList_', device);
  },

  /**
   * Process bluetooth.onDeviceRemoved events.
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  onBluetoothDeviceRemoved_: function(device) {
    var address = device.address;
    var index = this.getDeviceIndex_(address);
    if (index < 0)
      return;
    this.splice('deviceList_', index, 1);
  },

  /** @private */
  startDiscovery_: function() {
    if (!this.adapterState_ || this.adapterState_.discovering)
      return;

    if (!this.bluetoothPrivateOnPairingListener_) {
      this.bluetoothPrivateOnPairingListener_ =
          this.onBluetoothPrivateOnPairing_.bind(this);
      this.bluetoothPrivate.onPairing.addListener(
          this.bluetoothPrivateOnPairingListener_);
    }

    this.bluetooth.startDiscovery(function() {
      if (chrome.runtime.lastError) {
        if (chrome.runtime.lastError.message == 'Failed to stop discovery') {
          // May happen if also started elsewhere; ignore.
          return;
        }
        console.error(
            'startDiscovery Error: ' + chrome.runtime.lastError.message);
      }
    });
  },

  /** @private */
  stopDiscovery_: function() {
    if (!this.get('adapterState_.discovering'))
      return;

    if (this.bluetoothPrivateOnPairingListener_) {
      this.bluetoothPrivate.onPairing.removeListener(
          this.bluetoothPrivateOnPairingListener_);
      this.bluetoothPrivateOnPairingListener_ = undefined;
    }

    this.bluetooth.stopDiscovery(function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error stopping bluetooth discovery: ' +
            chrome.runtime.lastError.message);
      }
    });
  },

  /**
   * Process bluetoothPrivate.onPairing events.
   * @param {!chrome.bluetoothPrivate.PairingEvent} e
   * @private
   */
  onBluetoothPrivateOnPairing_: function(e) {
    if (!this.dialogId_ || !this.pairingDevice_ ||
        e.device.address != this.pairingDevice_.address) {
      return;
    }
    if (e.pairing == chrome.bluetoothPrivate.PairingEventType.KEYS_ENTERED &&
        e.passkey === undefined && this.pairingEvent_) {
      // 'keysEntered' event might not include the updated passkey so preserve
      // the current one.
      e.passkey = this.pairingEvent_.passkey;
    }
    this.pairingEvent_ = e;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAddDeviceTap_: function(e) {
    e.preventDefault();
    this.openDialog_('addDevice');
  },

  /**
   * @param {!{detail: {action: string, device: !chrome.bluetooth.Device}}} e
   * @private
   */
  onDeviceEvent_: function(e) {
    var action = e.detail.action;
    var device = e.detail.device;
    if (action == 'connect')
      this.connectDevice_(device);
    else if (action == 'disconnect')
      this.disconnectDevice_(device);
    else if (action == 'remove')
      this.forgetDevice_(device);
    else
      console.error('Unexected action: ' + action);
  },

  /**
   * Handle a response sent from the pairing dialog and pass it to the
   * bluetoothPrivate API.
   * @param {Event} e
   * @private
   */
  onResponse_: function(e) {
    var options =
        /** @type {!chrome.bluetoothPrivate.SetPairingResponseOptions} */ (
            e.detail);
    this.bluetoothPrivate.setPairingResponse(options, function() {
      if (chrome.runtime.lastError) {
        // TODO(stevenjb): Show error.
        console.error(
            'Error setting pairing response: ' + options.device.name +
            ': Response: ' + options.response + ': Error: ' +
            chrome.runtime.lastError.message);
      }
      this.$$('#deviceDialog').close();
    }.bind(this));
  },

  /**
   * @param {string} address
   * @return {number} The index of the device associated with |address| or -1.
   * @private
   */
  getDeviceIndex_: function(address) {
    var len = this.deviceList_.length;
    for (var i = 0; i < len; ++i) {
      if (this.deviceList_[i].address == address)
        return i;
    }
    return -1;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @return {string} The text to display for |device| in the device list.
   * @private
   */
  getDeviceName_: function(device) {
    return device.name || device.address;
  },

  /**
   * @return {!Array<!chrome.bluetooth.Device>}
   * @private
   */
  getPairedOrConnecting_: function() {
    return this.deviceList_.filter(function(device) {
      return !!device.paired || !!device.connecting;
    });
  },

  /**
   * @return {boolean} True if deviceList contains any paired devices.
   * @private
   */
  haveDevices_: function() {
    return this.deviceList_.findIndex(function(d) {
      return !!d.paired;
    }) != -1;
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  connectDevice_: function(device) {
    // If the device is not paired, show the pairing dialog.
    if (!device.paired) {
      // Set the pairing device and clear any pairing event.
      this.pairingDevice_ = device;
      this.pairingEvent_ = null;

      this.openDialog_('pairDevice');
    }

    this.bluetoothPrivate.connect(device.address, function(result) {
      var error;
      if (chrome.runtime.lastError) {
        error = chrome.runtime.lastError.message;
      } else {
        switch (result) {
          case chrome.bluetoothPrivate.ConnectResultType.ALREADY_CONNECTED:
          case chrome.bluetoothPrivate.ConnectResultType.AUTH_CANCELED:
          case chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS:
          case chrome.bluetoothPrivate.ConnectResultType.SUCCESS:
            break;
          default:
            error = result;
        }
      }

      if (!error) {
        this.$$('#deviceDialog').close();
        return;
      }

      var name = this.getDeviceName_(device);
      var id = 'bluetooth_connect_' + error;
      if (this.i18nExists(id)) {
        this.errorMessage_ = this.i18n(id, name);
      } else {
        this.errorMessage_ = error;
        console.error('Unexpected error connecting to: ' + name + ': ' + error);
      }
      this.openDialog_('connectError');
    }.bind(this));
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  disconnectDevice_: function(device) {
    this.bluetoothPrivate.disconnectAll(device.address, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error disconnecting: ' + device.address +
            chrome.runtime.lastError.message);
      }
    });
  },

  /**
   * @param {!chrome.bluetooth.Device} device
   * @private
   */
  forgetDevice_: function(device) {
    this.bluetoothPrivate.forgetDevice(device.address, function() {
      if (chrome.runtime.lastError) {
        console.error(
            'Error forgetting: ' + device.name + ': ' +
            chrome.runtime.lastError.message);
      }
      this.updateDeviceList_();
    }.bind(this));
  },

  /**
   * @param {string} dialogId
   * @param {string} dialogToShow The name of the dialog.
   * @return {boolean}
   * @private
   */
  dialogIsVisible_(dialogId, dialogToShow) {
    return dialogToShow == dialogId;
  },

  /**
   * @param {string} dialogId
   * @private
   */
  openDialog_: function(dialogId) {
    if (this.dialogId_) {
      // Dialog already opened, just update the contents.
      this.dialogId_ = dialogId;
      return;
    }
    this.dialogId_ = dialogId;
    // Call flush so that the dialog gets sized correctly before it is opened.
    Polymer.dom.flush();
    var dialog = this.$$('#deviceDialog');
    dialog.open();
    this.startDiscovery_();
  },

  /** @private */
  onDialogClosed_: function() {
    this.stopDiscovery_();
    this.dialogId_ = '';
    this.pairingDevice_ = null;
    this.pairingEvent_ = null;
  },

  /**
   * @param {Event} e
   * @private
   */
  stopTap_: function(e) {
    e.stopPropagation();
  },
});
